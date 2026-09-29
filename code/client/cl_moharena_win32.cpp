/*
===========================================================================
Copyright (C) 2026 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// cl_moharena_win32.cpp -- Added in MoH Arena
//
// Loads the MoH Arena module on Windows. The launcher starts the game with
// three environment variables: its nonce, the module's log folder and the
// SHA-256 the module file must have. They are read once and cleared at once,
// so no later child process sees them. The module must be a plain file next
// to the exe, under the one name for this architecture. It is hashed and
// loaded while it stays open with read sharing only, so it cannot change in
// between. Any mismatch leaves the game without the module; nothing retries.

#if defined(MOHARENA_NATIVE_BRIDGE) && defined(_WIN32)

#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    include <bcrypt.h>

#    include <string.h>
#    include <string>
#    include <vector>

#    include "cl_moharena_load.h"

#    define MOHARENA_WIDEN2(text) L##text
#    define MOHARENA_WIDEN(text)  MOHARENA_WIDEN2(text)

// The largest module file the loader hashes.
#    define MOHARENA_MODULE_SIZE_MAX (64 * 1024 * 1024)

// Room for each variable, in UTF-16 units with the terminator.
#    define MOHARENA_ENV_SHORT_MAX 128
#    define MOHARENA_ENV_PATH_MAX  2048

static void SetReason(char *reason, size_t reasonSize, const char *code)
{
    size_t length;

    if (!reason || !reasonSize) {
        return;
    }

    length = strlen(code);
    if (length >= reasonSize) {
        length = reasonSize - 1;
    }

    memcpy(reason, code, length);
    reason[length] = 0;
}

// Reads one variable: -1 when it is missing, -2 when it does not fit,
// otherwise its length.
static int ReadEnvironment(const wchar_t *name, wchar_t *output, DWORD capacity)
{
    DWORD length;

    output[0] = 0;
    SetLastError(ERROR_SUCCESS);
    length = GetEnvironmentVariableW(name, output, capacity);
    if (!length) {
        output[0] = 0;
        return GetLastError() == ERROR_ENVVAR_NOT_FOUND ? -1 : 0;
    }

    if (length >= capacity) {
        output[0] = 0;
        return -2;
    }

    return (int)length;
}

// 1 to 63 ASCII letters, digits or '-'.
static bool NonceValid(const wchar_t *nonce, int length)
{
    int i;

    if (length < 1 || length >= (int)MOHARENA_OPM_NONCE_MAX) {
        return false;
    }

    for (i = 0; i < length; i++) {
        const wchar_t c = nonce[i];

        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-')) {
            return false;
        }
    }

    return true;
}

// Exactly 64 lowercase hex digits.
static bool DigestValid(const wchar_t *digest, int length)
{
    int i;

    if (length != 64) {
        return false;
    }

    for (i = 0; i < length; i++) {
        const wchar_t c = digest[i];

        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) {
            return false;
        }
    }

    return true;
}

// "X:\", "X:/" or a UNC path.
static bool PathAbsolute(const wchar_t *path, int length)
{
    if (length >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z'))
        && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        return true;
    }

    return length >= 3 && path[0] == L'\\' && path[1] == L'\\';
}

// The folder of the running exe, with a trailing backslash.
static bool ExeFolder(std::wstring &folder)
{
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD                length;
    size_t               slash;

    for (;;) {
        length = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
        if (!length) {
            return false;
        }

        if (length < buffer.size()) {
            break;
        }

        // Truncated: try again with more room, up to the longest path.
        if (buffer.size() >= 32768) {
            return false;
        }

        buffer.resize(buffer.size() * 2);
    }

    folder.assign(buffer.data(), length);
    slash = folder.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return false;
    }

    folder.resize(slash + 1);
    return true;
}

// Hashes the whole file from its start; expectedSize guards against a file
// that changed size after it was opened.
static bool HashFile(HANDLE file, ULONGLONG expectedSize, char *hex, size_t hexSize)
{
    static const char  digits[] = "0123456789abcdef";
    BCRYPT_ALG_HANDLE  algorithm = NULL;
    BCRYPT_HASH_HANDLE hash      = NULL;
    unsigned char      digest[32];
    ULONGLONG          total = 0;
    DWORD              count;
    bool               ok = true;
    size_t             i;

    if (hexSize < sizeof(digest) * 2 + 1) {
        return false;
    }

    std::vector<unsigned char> buffer(64 * 1024);

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0))) {
        return false;
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    for (;;) {
        if (!ReadFile(file, buffer.data(), (DWORD)buffer.size(), &count, NULL)) {
            ok = false;
            break;
        }

        if (!count) {
            break;
        }

        total += count;
        if (total > expectedSize || !BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), count, 0))) {
            ok = false;
            break;
        }
    }

    if (ok && total != expectedSize) {
        ok = false;
    }

    if (ok && !BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) {
        ok = false;
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!ok) {
        return false;
    }

    for (i = 0; i < sizeof(digest); i++) {
        hex[i * 2]     = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }

    hex[sizeof(digest) * 2] = 0;
    return true;
}

MohArenaOpmStartFn MoHArena_LoadModule(MohArenaOpmBootstrapV1 *bootstrap, char *reason, size_t reasonSize)
{
    static const wchar_t *const names[3] = {
        MOHARENA_WIDEN(MOHARENA_OPM_ENV_NONCE),
        MOHARENA_WIDEN(MOHARENA_OPM_ENV_LOG_DIR),
        MOHARENA_WIDEN(MOHARENA_OPM_ENV_MODULE_SHA256),
    };
    wchar_t                    nonce[MOHARENA_ENV_SHORT_MAX];
    wchar_t                    logDirectory[MOHARENA_ENV_PATH_MAX];
    wchar_t                    expected[MOHARENA_ENV_SHORT_MAX];
    char                       expectedHex[65];
    char                       actualHex[65];
    char                       logUtf8[MOHARENA_OPM_PATH_MAX];
    int                        nonceLength, logLength, expectedLength;
    int                        i;
    std::wstring               path;
    HANDLE                     file;
    BY_HANDLE_FILE_INFORMATION info;
    ULONGLONG                  fileSize;
    HMODULE                    module;
    FARPROC                    entry;

    SetReason(reason, reasonSize, "");
    if (!bootstrap) {
        SetReason(reason, reasonSize, "internal");
        return NULL;
    }

    memset(bootstrap, 0, sizeof(*bootstrap));

    nonceLength    = ReadEnvironment(names[0], nonce, MOHARENA_ENV_SHORT_MAX);
    logLength      = ReadEnvironment(names[1], logDirectory, MOHARENA_ENV_PATH_MAX);
    expectedLength = ReadEnvironment(names[2], expected, MOHARENA_ENV_SHORT_MAX);

    // Cleared whatever they held, before anything else can start a process.
    for (i = 0; i < 3; i++) {
        SetEnvironmentVariableW(names[i], NULL);
    }

    if (nonceLength == -1 && logLength == -1 && expectedLength == -1) {
        // Not started by the launcher for the native client.
        return NULL;
    }

    if (nonceLength < 0 || logLength < 0 || expectedLength < 0) {
        SetReason(reason, reasonSize, "environment_incomplete");
        return NULL;
    }

    if (!NonceValid(nonce, nonceLength)) {
        SetReason(reason, reasonSize, "invalid_nonce");
        return NULL;
    }

    if (!DigestValid(expected, expectedLength)) {
        SetReason(reason, reasonSize, "invalid_module_sha256");
        return NULL;
    }

    if (!PathAbsolute(logDirectory, logLength)
        || !WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, logDirectory, -1, logUtf8, (int)sizeof(logUtf8), NULL, NULL
        )) {
        SetReason(reason, reasonSize, "invalid_log_directory");
        return NULL;
    }

    // Plain ASCII, checked above.
    for (i = 0; i < 64; i++) {
        expectedHex[i] = (char)expected[i];
    }
    expectedHex[64] = 0;

    if (!ExeFolder(path)) {
        SetReason(reason, reasonSize, "exe_path");
        return NULL;
    }

    path += sizeof(void *) == 8 ? MOHARENA_WIDEN(MOHARENA_OPM_MODULE_X64) : MOHARENA_WIDEN(MOHARENA_OPM_MODULE_X86);

    // Read sharing only: nobody writes, renames or deletes the file while it
    // is open. A link is opened as itself and refused below.
    file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        SetReason(reason, reasonSize, GetLastError() == ERROR_FILE_NOT_FOUND ? "module_missing" : "module_open");
        return NULL;
    }

    if (!GetFileInformationByHandle(file, &info)) {
        CloseHandle(file);
        SetReason(reason, reasonSize, "module_open");
        return NULL;
    }

    fileSize = ((ULONGLONG)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    if ((info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) || !fileSize
        || fileSize > MOHARENA_MODULE_SIZE_MAX) {
        CloseHandle(file);
        SetReason(reason, reasonSize, "module_not_plain_file");
        return NULL;
    }

    if (!HashFile(file, fileSize, actualHex, sizeof(actualHex))) {
        CloseHandle(file);
        SetReason(reason, reasonSize, "module_read");
        return NULL;
    }

    if (memcmp(actualHex, expectedHex, sizeof(expectedHex)) != 0) {
        CloseHandle(file);
        SetReason(reason, reasonSize, "module_sha256_mismatch");
        return NULL;
    }

    // Loaded while the checked file is still open; its own imports are
    // looked up from its folder first. The module is never unloaded.
    module = LoadLibraryExW(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    CloseHandle(file);
    if (!module) {
        SetReason(reason, reasonSize, "module_load");
        return NULL;
    }

    entry = GetProcAddress(module, MOHARENA_OPM_START_SYMBOL);
    if (!entry) {
        SetReason(reason, reasonSize, "module_entry");
        return NULL;
    }

    bootstrap->abi_version = MOHARENA_OPM_BOOTSTRAP_ABI_V1;
    bootstrap->struct_size = sizeof(*bootstrap);
    bootstrap->flags       = 0;
    memcpy(bootstrap->log_directory, logUtf8, strlen(logUtf8) + 1);
    for (i = 0; i < nonceLength; i++) {
        bootstrap->nonce[i] = (char)nonce[i];
    }
    bootstrap->nonce[nonceLength] = 0;

    return reinterpret_cast<MohArenaOpmStartFn>(reinterpret_cast<void *>(entry));
}

#endif
