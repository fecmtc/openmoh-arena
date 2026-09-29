#ifndef MOHARENA_OPM_BOOTSTRAP_V1_H
#define MOHARENA_OPM_BOOTSTRAP_V1_H

/* How OpenMoH Arena's client bridge starts the MoH Arena module
 * (docs/openmohaa-bridge.md). The launcher starts openmohaa.exe with three
 * environment variables. The bridge reads them once and clears them, checks
 * the module file's SHA-256, loads it by full path from the game's folder,
 * then calls MohArenaOpmStartV1 once from the game's main thread. Without the
 * variables, or on any mismatch, the game runs without the module. */

#include <stddef.h>
#include <stdint.h>

#include "moharena_host_v1.h"
#include "moharena_opm_engine_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOHARENA_OPM_BOOTSTRAP_ABI_V1 UINT32_C(0x00010000)
#define MOHARENA_OPM_NONCE_MAX 64u
#define MOHARENA_OPM_PATH_MAX 1024u

/* The launcher's nonce: 1 to 63 ASCII letters, digits or '-'. */
#define MOHARENA_OPM_ENV_NONCE "MOHARENA_NATIVE_NONCE"
/* The folder for the module's log and settings, UTF-8. */
#define MOHARENA_OPM_ENV_LOG_DIR "MOHARENA_NATIVE_LOG_DIR"
/* The SHA-256 the module file must have: 64 lowercase hex digits. */
#define MOHARENA_OPM_ENV_MODULE_SHA256 "MOHARENA_NATIVE_MODULE_SHA256"

#define MOHARENA_OPM_MODULE_X86 "moharena-opm-x86.dll"
#define MOHARENA_OPM_MODULE_X64 "moharena-opm-x64.dll"
#define MOHARENA_OPM_START_SYMBOL "MohArenaOpmStartV1"

/* The module's states, as the retail adapter reports them. */
enum {
    MOHARENA_OPM_STATE_DISABLED = 0,
    MOHARENA_OPM_STATE_READY = 1,
    MOHARENA_OPM_STATE_UNSUPPORTED = 2,
    MOHARENA_OPM_STATE_FAILED = 3,
    MOHARENA_OPM_STATE_STOPPED = 4
};

typedef struct MohArenaOpmBootstrapV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t flags;           /* 0 */
    uint32_t reserved0;
    char log_directory[MOHARENA_OPM_PATH_MAX]; /* UTF-8, NUL-terminated */
    char nonce[MOHARENA_OPM_NONCE_MAX];        /* NUL-terminated */
} MohArenaOpmBootstrapV1;

typedef struct MohArenaOpmStartResultV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    MohArenaResult result;
    uint32_t native_state;    /* MOHARENA_OPM_STATE_* */
    /* A short code such as "invalid_nonce" when the start failed. */
    char failure_code[96];
    /* The pipe the launcher talks to, UTF-8, when ready. */
    char pipe_name[160];
} MohArenaOpmStartResultV1;

/* The bridge sets abi_version and struct_size of out_module and out_result
 * and zeroes the rest. With MOHARENA_OK, out_module holds the module's
 * callbacks and the bridge keeps engine valid until it calls stop. Any other
 * result leaves out_module zeroed; the bridge then never calls the module
 * again. Only the first call starts anything: a later one reports the
 * current state in out_result, leaves out_module zeroed and returns
 * MOHARENA_E_VALIDATION_FAILED. */
typedef MohArenaResult (MOHARENA_CALL *MohArenaOpmStartFn)(
    const MohArenaOpmBootstrapV1 *bootstrap, const MohArenaOpmEngineV1 *engine,
    MohArenaOpmModuleV1 *out_module, MohArenaOpmStartResultV1 *out_result);

#ifdef __cplusplus
}
#endif

#endif
