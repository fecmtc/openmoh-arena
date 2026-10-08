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

// cl_moharena_uipolicy.cpp -- Added in MoH Arena
//
// The fixed lists behind the modern UI: the settings its menus may write and
// the values each one takes, the commands its key rows may bind and its
// buttons may run, and the files it may load. A menu file names a setting or
// a command, and this file says whether it is a listed one. It reads no menu
// file and runs no command itself: the modern UI calls it before it writes,
// binds, runs or loads anything.

#include "client.h"
#include "cl_moharena_uipolicy.h"
#include "../qcommon/moharena_limits.h"

#include <stddef.h>
#include <string.h>

// A setting with one of these flags is never written from a menu.
#define MOHARENA_UI_LOCKED_FLAGS (CVAR_ROM | CVAR_INIT | CVAR_CHEAT | CVAR_PROTECTED)
// The most words in one button command.
#define MOHARENA_UI_WORDS_MAX 4
// The longest word of a button command, with its terminator.
#define MOHARENA_UI_WORD_MAX 32
// The most digits of a number in a button command.
#define MOHARENA_UI_INDEX_DIGITS 5
// The longest server address, with its terminator.
#define MOHARENA_UI_ADDRESS_MAX 65
// The longest menu file path, with its terminator.
#define MOHARENA_UI_PATH_MAX 128

// How the value of a setting is checked. Sliders are brought inside their
// limits; every other kind is taken as it is or refused.
typedef enum {
    // "0" or "1".
    MOHARENA_UI_SWITCH,
    // A whole number inside the limits.
    MOHARENA_UI_WHOLE,
    // A slider: a number, brought inside the limits.
    MOHARENA_UI_NUMBER,
    // A slider of either sign: its size is brought inside the limits.
    MOHARENA_UI_MAGNITUDE,
    // One of a list of words.
    MOHARENA_UI_CHOICE,
    // A name of letters, digits, _ and -.
    MOHARENA_UI_NAME,
    // Typed text, cleaned.
    MOHARENA_UI_TEXT,
    // com_maxfps: a whole number without the three values MoH Arena's rules
    // refuse.
    MOHARENA_UI_MAXFPS
} moharenaUIKind_t;

typedef struct {
    const char        *name;
    moharenaUIKind_t   kind;
    // The limits of a number, or the longest text.
    double             minimum;
    double             maximum;
    // The words of a choice, NULL after the last.
    const char *const *choices;
    // The DEFAULTS button puts it back.
    qboolean           defaults;
} moharenaUISetting_t;

/*
==============================================================================

The lists

==============================================================================
*/

static const char *const moharenaUISensitivityModes[] = {"sensitivity", "cm360", NULL};
static const char *const moharenaUIZoomModes[]        = {"off", "legacy", "screen", NULL};
static const char *const moharenaUITextureBits[]      = {"0", "16", "32", NULL};
static const char *const moharenaUISoundRates[]       = {"11", "22", "44", NULL};

static const char *const moharenaUITextureModes[] = {
    "GL_NEAREST",
    "GL_LINEAR",
    "GL_NEAREST_MIPMAP_NEAREST",
    "GL_LINEAR_MIPMAP_NEAREST",
    "GL_NEAREST_MIPMAP_LINEAR",
    "GL_LINEAR_MIPMAP_LINEAR",
    NULL,
};

static const char *const moharenaUISoundDrivers[] = {
    "auto",
    "DirectSound3D Hardware Support",
    "Creative Labs EAX 2 (TM)",
    "Creative Labs EAX (TM)",
    "Aureal A3D 2.0 (TM)",
    "Dolby Surround",
    "Miles Fast 2D Positional Audio",
    NULL,
};

static const char *const moharenaUIWeaponSigns[] = {
    "rifle_sign",
    "sniperrifle_sign",
    "submachinegun_sign",
    "machinegun_sign",
    "rocketlauncher_sign",
    "shotgun_sign",
    NULL,
};

// Every setting the menus may write. A setting that is not here is refused,
// whatever a menu file says.
static const moharenaUISetting_t moharenaUISettings[] = {
    // Profile
    {"ui_name",                            MOHARENA_UI_TEXT,      0,      32,                   NULL,                       qfalse},
    {"ui_dm_playermodel_set",              MOHARENA_UI_NAME,      0,      63,                   NULL,                       qfalse},
    {"ui_dm_playergermanmodel_set",        MOHARENA_UI_NAME,      0,      63,                   NULL,                       qfalse},

    // Mouse
    {"ui_modernsettings_sensitivity_mode", MOHARENA_UI_CHOICE,    0,      0,                    moharenaUISensitivityModes, qtrue },
    {"sensitivity",                        MOHARENA_UI_NUMBER,    0.0001, 10000,                NULL,                       qtrue },
    {"ui_modernsettings_dpi",              MOHARENA_UI_WHOLE,     100,    32000,                NULL,                       qtrue },
    {"cg_zoomSensitivity",                 MOHARENA_UI_CHOICE,    0,      0,                    moharenaUIZoomModes,        qtrue },
    {"m_pitch",                            MOHARENA_UI_MAGNITUDE, 0.001,  1,                    NULL,                       qtrue },
    {"m_yaw",                              MOHARENA_UI_NUMBER,    0.001,  1,                    NULL,                       qtrue },
    {"m_filter",                           MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cl_mouseAccel",                      MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },

    // Video
    {"r_fullscreen",                       MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"r_noborder",                         MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"r_mode",                             MOHARENA_UI_WHOLE,     -2,     11,                   NULL,                       qtrue },
    {"r_displayRefresh",                   MOHARENA_UI_WHOLE,     0,      1000,                 NULL,                       qtrue },
    {"r_swapInterval",                     MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"com_maxfps",                         MOHARENA_UI_MAXFPS,    0,      1000,                 NULL,                       qtrue },
    {"fps",                                MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"r_texturebits",                      MOHARENA_UI_CHOICE,    0,      0,                    moharenaUITextureBits,      qtrue },
    {"ui_scale",                           MOHARENA_UI_NUMBER,    0.25,   2,                    NULL,                       qtrue },
    {"r_gamma",                            MOHARENA_UI_NUMBER,    0.5,    3,                    NULL,                       qtrue },
    {"ui_om_menu_map_view",                MOHARENA_UI_NAME,      0,      63,                   NULL,                       qtrue },

    // Graphics
    {"r_picmip",                           MOHARENA_UI_WHOLE,     0,      MOHARENA_PICMIP_MAX,  NULL,                       qtrue },
    {"r_textureMode",                      MOHARENA_UI_CHOICE,    0,      0,                    moharenaUITextureModes,     qtrue },
    {"r_ext_compressed_textures",          MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"r_lodscale",                         MOHARENA_UI_NUMBER,    0.25,   2,                    NULL,                       qtrue },
    {"cg_shadows",                         MOHARENA_UI_WHOLE,     0,      MOHARENA_SHADOWS_MAX, NULL,                       qtrue },
    {"r_fastdlights",                      MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"vss_draw",                           MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cg_effectdetail",                    MOHARENA_UI_NUMBER,    0.2,    1,                    NULL,                       qtrue },
    {"cg_rain",                            MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cg_marks_add",                       MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"r_drawstaticdecals",                 MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"com_blood",                          MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },

    // Audio
    {"s_volume",                           MOHARENA_UI_NUMBER,    0,      1,                    NULL,                       qtrue },
    {"s_musicvolume",                      MOHARENA_UI_NUMBER,    0,      1,                    NULL,                       qtrue },
    {"s_ambientvolume",                    MOHARENA_UI_NUMBER,    0,      1,                    NULL,                       qtrue },
    {"s_speaker_type",                     MOHARENA_UI_WHOLE,     0,      3,                    NULL,                       qtrue },
    {"s_khz",                              MOHARENA_UI_CHOICE,    0,      0,                    moharenaUISoundRates,       qtrue },
    {"s_milesdriver",                      MOHARENA_UI_CHOICE,    0,      0,                    moharenaUISoundDrivers,     qtrue },
    {"s_reverb",                           MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },

    // HUD and gameplay
    {"ui_om_hud",                          MOHARENA_UI_NAME,      0,      63,                   NULL,                       qtrue },
    // The sizes of the parts of the "arena" HUD style; the three feeds in px of text
    {"ui_om_hud_compass_scale",            MOHARENA_UI_NUMBER,    0.5,    2,                    NULL,                       qtrue },
    {"ui_om_hud_topbar_scale",             MOHARENA_UI_NUMBER,    0.5,    2,                    NULL,                       qtrue },
    {"ui_om_hud_killfeed_size",            MOHARENA_UI_WHOLE,     10,     32,                   NULL,                       qtrue },
    {"ui_om_hud_messages_size",            MOHARENA_UI_WHOLE,     10,     32,                   NULL,                       qtrue },
    {"ui_om_hud_chat_size",                MOHARENA_UI_WHOLE,     10,     32,                   NULL,                       qtrue },
    {"cg_drawviewmodel",                   MOHARENA_UI_WHOLE,     0,      2,                    NULL,                       qtrue },
    {"ui_weaponsbar",                      MOHARENA_UI_WHOLE,     0,      2,                    NULL,                       qtrue },
    {"ui_om_scoreboard_disable_cursor",    MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cl_run",                             MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cg_autoswitch",                      MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cg_hitmarker",                       MOHARENA_UI_SWITCH,    0,      1,                    NULL,                       qtrue },
    {"cg_hitmarker_sound",                 MOHARENA_UI_NAME,      0,      63,                   NULL,                       qtrue },

    // Written by the weapon and vote menus; not settings
    {"ui_weaponsign",                      MOHARENA_UI_CHOICE,    0,      0,                    moharenaUIWeaponSigns,      qfalse},
    {"ui_votestringentry",                 MOHARENA_UI_TEXT,      0,      64,                   NULL,                       qfalse},
};

// Every command a key row may bind.
static const char *const moharenaUIBinds[] = {
    "+forward",
    "+back",
    "+moveleft",
    "+moveright",
    "+moveup",
    "+movedown",
    "+speed",
    "+leanleft",
    "+leanright",
    "+attackprimary",
    "+attacksecondary",
    "reload",
    "weapnext",
    "weapprev",
    "+use",
    "useweaponclass pistol",
    "useweaponclass rifle",
    "useweaponclass smg",
    "useweaponclass mg",
    "useweaponclass grenade",
    "useweaponclass heavy",
    "toggleitem",
    "messagemode",
    "messagemode_team",
    "instamsg_main",
    "useprimary",
    "toggle_spectate_firstperson",
    "toggle_scoreboard_cursor",
    "cycle_scoreboard_sort",
};

// Every command a menu button may run. # stands for a number of one to five
// digits.
static const char *const moharenaUICommands[] = {
    "ui_close menu dm_pause",
    "join_team allies",
    "join_team axis",
    "auto_join_team",
    "spectator",
    "primarydmweapon rifle",
    "primarydmweapon sniper",
    "primarydmweapon smg",
    "primarydmweapon mg",
    "primarydmweapon heavy",
    "primarydmweapon shotgun",
    "vote 1",
    "vote 0",
    "callvote #",
    "callvote # #",
    "callentryvote",
    "pushcallvote",
    "pushcallvotesublist #",
    "pushcallvotesubtext #",
    "pushcallvotesubinteger #",
    "pushcallvotesubfloat #",
    "pushcallvotesubclient #",
    "set ui_votetype #",
    "pushmenu main",
    "disconnect",
};

// A menu or HUD of the modern UI's own pack, and the one file that may carry
// its id.
typedef struct {
    const char *id;
    const char *path;
} moharenaUIOwnFile_t;

static const moharenaUIOwnFile_t moharenaUIMenuFiles[] = {
    {"main",            "ui/modern/main.xml"                 },
    {"scoreboard",      "ui/modern/menus/scoreboard.xml"     },
    {"dm_pause",        "ui/modern/menus/dm_pause.xml"       },
    {"dm_pause_modern", "ui/modern/menus/dm_pause_modern.xml"},
};

static const moharenaUIOwnFile_t moharenaUIHudFiles[] = {
    {"classic",     "ui/modern/huds/classic.xml"    },
    {"modern",      "ui/modern/huds/modern.xml"     },
    {"competitive", "ui/modern/huds/competitive.xml"},
    {"arena",       "ui/modern/huds/arena.xml"      },
};

/*
==============================================================================

Settings

==============================================================================
*/

static qboolean MoHArena_UILetterOrDigit(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? qtrue : qfalse;
}

static const moharenaUISetting_t *MoHArena_UISettingFind(const char *name)
{
    size_t i;

    if (!name || !name[0]) {
        return NULL;
    }

    for (i = 0; i < ARRAY_LEN(moharenaUISettings); i++) {
        if (!Q_stricmp(name, moharenaUISettings[i].name)) {
            return &moharenaUISettings[i];
        }
    }

    return NULL;
}

// Copies a value that passed its check, exactly as it was given.
static qboolean MoHArena_UICopy(const char *value, char *out, size_t outSize)
{
    const size_t length = strlen(value);

    if (length >= outSize) {
        return qfalse;
    }

    memcpy(out, value, length + 1);
    return qtrue;
}

// A whole number written without decimals.
static qboolean MoHArena_UIReadWhole(const char *value, int *whole)
{
    moharenaNumber_t number;

    if (!MoHArena_ReadNumber(value, &number) || strchr(value, '.')) {
        return qfalse;
    }

    *whole = number.negative ? -number.whole : number.whole;
    return qtrue;
}

// Checks a value against its setting and puts what may be stored in out. A
// value that is allowed comes out exactly as it went in, so a menu writing an
// untouched control back changes nothing.
static qboolean
MoHArena_UISettingValue(const moharenaUISetting_t *setting, const char *value, char *out, size_t outSize)
{
    moharenaNumber_t number;
    double           size;
    size_t           length;
    size_t           i;
    int              whole;

    if (!outSize) {
        return qfalse;
    }

    out[0] = 0;

    switch (setting->kind) {
    case MOHARENA_UI_SWITCH:
        if (strcmp(value, "0") && strcmp(value, "1")) {
            return qfalse;
        }

        return MoHArena_UICopy(value, out, outSize);

    case MOHARENA_UI_WHOLE:
    case MOHARENA_UI_MAXFPS:
        if (!MoHArena_UIReadWhole(value, &whole) || whole < setting->minimum || whole > setting->maximum) {
            return qfalse;
        }

        if (setting->kind == MOHARENA_UI_MAXFPS && whole >= 332 && whole <= 334) {
            return qfalse;
        }

        return MoHArena_UICopy(value, out, outSize);

    case MOHARENA_UI_NUMBER:
        if (!MoHArena_ReadNumber(value, &number)) {
            return qfalse;
        }

        size = atof(value);
        if (size < setting->minimum) {
            Com_sprintf(out, outSize, "%g", setting->minimum);
            return qtrue;
        }

        if (size > setting->maximum) {
            Com_sprintf(out, outSize, "%g", setting->maximum);
            return qtrue;
        }

        return MoHArena_UICopy(value, out, outSize);

    case MOHARENA_UI_MAGNITUDE:
        if (!MoHArena_ReadNumber(value, &number)) {
            return qfalse;
        }

        size = fabs(atof(value));
        if (size < setting->minimum) {
            Com_sprintf(out, outSize, "%s%g", number.negative ? "-" : "", setting->minimum);
            return qtrue;
        }

        if (size > setting->maximum) {
            Com_sprintf(out, outSize, "%s%g", number.negative ? "-" : "", setting->maximum);
            return qtrue;
        }

        return MoHArena_UICopy(value, out, outSize);

    case MOHARENA_UI_CHOICE:
        for (i = 0; setting->choices[i]; i++) {
            if (!Q_stricmp(value, setting->choices[i])) {
                return MoHArena_UICopy(value, out, outSize);
            }
        }

        return qfalse;

    case MOHARENA_UI_NAME:
        length = strlen(value);
        if (!length || length > (size_t)setting->maximum) {
            return qfalse;
        }

        for (i = 0; i < length; i++) {
            if (!MoHArena_UILetterOrDigit(value[i]) && value[i] != '_' && value[i] != '-') {
                return qfalse;
            }
        }

        return MoHArena_UICopy(value, out, outSize);

    case MOHARENA_UI_TEXT:
        // Never refused. What could end a quoted argument, start another
        // command or split a userinfo string is left out.
        length = 0;
        for (i = 0; value[i]; i++) {
            const unsigned char c = (unsigned char)value[i];

            if (c < 32 || c == 127 || c == '"' || c == ';' || c == '\\') {
                continue;
            }

            if (length >= (size_t)setting->maximum || length + 1 >= outSize) {
                break;
            }

            out[length++] = (char)c;
        }

        out[length] = 0;
        return qtrue;
    }

    return qfalse;
}

qboolean MoHArena_UISettingSet(const char *name, const char *value)
{
    const moharenaUISetting_t *setting;
    const cvar_t              *var;
    char                       text[MAX_CVAR_VALUE_STRING];

    setting = MoHArena_UISettingFind(name);
    if (!setting || !value) {
        return qfalse;
    }

    if (!MoHArena_UISettingValue(setting, value, text, sizeof(text))) {
        return qfalse;
    }

    var = Cvar_FindVar(setting->name);
    if (!var) {
        // Not registered yet: created as a "set" typed in the console creates
        // it, so the game's own registration still gives it its default.
        Cvar_Set2(setting->name, text, qfalse);
        return qtrue;
    }

    // The engine's own write protection stays in force.
    if (var->flags & MOHARENA_UI_LOCKED_FLAGS) {
        return qfalse;
    }

    // Set at once, latched settings too: the menus show a setting from its
    // current value, and their APPLY button compares that value to decide on
    // a restart.
    Cvar_Set(setting->name, text);
    return qtrue;
}

qboolean MoHArena_UISettingReset(const char *name)
{
    const moharenaUISetting_t *setting;
    const cvar_t              *var;
    char                       text[MAX_CVAR_VALUE_STRING];

    setting = MoHArena_UISettingFind(name);
    if (!setting) {
        return qfalse;
    }

    var = Cvar_FindVar(setting->name);
    if (!var || !var->resetString || (var->flags & MOHARENA_UI_LOCKED_FLAGS)) {
        return qfalse;
    }

    // The default goes through the same check as any other value.
    if (!MoHArena_UISettingValue(setting, var->resetString, text, sizeof(text))) {
        return qfalse;
    }

    // As the console's reset does it: a latched setting waits for its restart.
    Cvar_Set2(setting->name, text, qfalse);
    return qtrue;
}

const char *MoHArena_UISettingDefault(int index)
{
    size_t i;

    if (index < 0) {
        return NULL;
    }

    for (i = 0; i < ARRAY_LEN(moharenaUISettings); i++) {
        if (!moharenaUISettings[i].defaults) {
            continue;
        }

        if (!index) {
            return moharenaUISettings[i].name;
        }

        index--;
    }

    return NULL;
}

void MoHArena_UIKeepFov(void)
{
    const cvar_t *var;
    char          text[MOHARENA_FOV_TEXT_MAX];

    var = Cvar_FindVar("cg_fov");
    if (!var || !var->string) {
        return;
    }

    // The plain limits only. The cgame applies the limit of the drawn view
    // itself, and a second limit here from another size could undo a value
    // that was set for that view.
    if (MoHArena_ClampFov(var->string, 0, 0, text, sizeof(text))) {
        Cvar_Set("cg_fov", text);
    }
}

/*
==============================================================================

Binds and commands

==============================================================================
*/

const char *MoHArena_UIBind(const char *binding)
{
    size_t i;

    if (!binding) {
        return NULL;
    }

    if (!binding[0]) {
        return "";
    }

    for (i = 0; i < ARRAY_LEN(moharenaUIBinds); i++) {
        if (!Q_stricmp(binding, moharenaUIBinds[i])) {
            return moharenaUIBinds[i];
        }
    }

    return NULL;
}

// Splits one command into its words. -1 when it holds anything but plain
// words: no quotes, no separators, no variables.
static int MoHArena_UIWords(const char *text, char words[MOHARENA_UI_WORDS_MAX][MOHARENA_UI_WORD_MAX])
{
    size_t length;
    size_t i;
    int    count;

    count = 0;
    i     = 0;
    for (;;) {
        while (text[i] == ' ' || text[i] == '\t') {
            i++;
        }

        if (!text[i]) {
            return count;
        }

        if (count >= MOHARENA_UI_WORDS_MAX) {
            return -1;
        }

        length = 0;
        while (text[i] && text[i] != ' ' && text[i] != '\t') {
            const char c = text[i];

            if (!MoHArena_UILetterOrDigit(c) && c != '_' && c != '-' && c != '+') {
                return -1;
            }

            if (length + 1 >= MOHARENA_UI_WORD_MAX) {
                return -1;
            }

            words[count][length++] = c;
            i++;
        }

        words[count][length] = 0;
        count++;
    }
}

// A number of one to five digits.
static qboolean MoHArena_UIReadIndex(const char *word, int *index)
{
    int value;
    int i;

    value = 0;
    for (i = 0; word[i]; i++) {
        if (i >= MOHARENA_UI_INDEX_DIGITS || word[i] < '0' || word[i] > '9') {
            return qfalse;
        }

        value = value * 10 + (word[i] - '0');
    }

    if (!i) {
        return qfalse;
    }

    *index = value;
    return qtrue;
}

// Compares the words with one listed command and rebuilds it in out: the
// listed spelling of each word, and each number printed again.
static qboolean MoHArena_UICommandMatch(
    const char *listed, char words[MOHARENA_UI_WORDS_MAX][MOHARENA_UI_WORD_MAX], int count, char *out, size_t outSize
)
{
    const char *text;
    const char *end;
    char        number[16];
    size_t      size;
    size_t      textLength;
    size_t      length;
    int         index;
    int         word;

    length = 0;
    word   = 0;
    out[0] = 0;

    while (*listed) {
        end  = strchr(listed, ' ');
        size = end ? (size_t)(end - listed) : strlen(listed);

        if (word >= count) {
            return qfalse;
        }

        if (size == 1 && listed[0] == '#') {
            if (!MoHArena_UIReadIndex(words[word], &index)) {
                return qfalse;
            }

            Com_sprintf(number, sizeof(number), "%d", index);
            text       = number;
            textLength = strlen(number);
        } else {
            if (strlen(words[word]) != size || Q_stricmpn(words[word], listed, size)) {
                return qfalse;
            }

            text       = listed;
            textLength = size;
        }

        if (length + textLength + 2 > outSize) {
            return qfalse;
        }

        if (word) {
            out[length++] = ' ';
        }

        memcpy(out + length, text, textLength);
        length      += textLength;
        out[length]  = 0;

        word++;
        listed += size;
        if (*listed == ' ') {
            listed++;
        }
    }

    return word == count ? qtrue : qfalse;
}

qboolean MoHArena_UICommand(const char *command, char *out, size_t outSize)
{
    char   words[MOHARENA_UI_WORDS_MAX][MOHARENA_UI_WORD_MAX];
    size_t i;
    int    count;

    if (!command || !out || !outSize) {
        return qfalse;
    }

    out[0] = 0;
    count  = MoHArena_UIWords(command, words);
    if (count <= 0) {
        return qfalse;
    }

    for (i = 0; i < ARRAY_LEN(moharenaUICommands); i++) {
        if (MoHArena_UICommandMatch(moharenaUICommands[i], words, count, out, outSize)) {
            return qtrue;
        }
    }

    out[0] = 0;
    return qfalse;
}

/*
==============================================================================

Addresses, files and typed text

==============================================================================
*/

qboolean MoHArena_UIAddressAllowed(const char *address)
{
    size_t i;

    // A host name, an IPv4 or a bracketed IPv6 address, with or without a
    // port. Nothing that starts like an option of the connect command.
    if (!address || (!MoHArena_UILetterOrDigit(address[0]) && address[0] != '[')) {
        return qfalse;
    }

    for (i = 0; address[i]; i++) {
        const char c = address[i];

        if (i + 1 >= MOHARENA_UI_ADDRESS_MAX) {
            return qfalse;
        }

        if (!MoHArena_UILetterOrDigit(c) && c != '.' && c != ':' && c != '-' && c != '_' && c != '[' && c != ']') {
            return qfalse;
        }
    }

    return qtrue;
}

qboolean MoHArena_UIFileAllowed(const char *path)
{
    static const char root[]   = "ui/modern/";
    static const char ending[] = ".xml";
    const size_t      rootLength   = sizeof(root) - 1;
    const size_t      endingLength = sizeof(ending) - 1;
    size_t            length;
    size_t            i;

    if (!path) {
        return qfalse;
    }

    length = strlen(path);
    if (length >= MOHARENA_UI_PATH_MAX || length <= rootLength + endingLength) {
        return qfalse;
    }

    if (Q_stricmpn(path, root, rootLength) || Q_stricmp(path + length - endingLength, ending)) {
        return qfalse;
    }

    for (i = 0; i < length; i++) {
        const char c = path[i];

        if (!MoHArena_UILetterOrDigit(c) && c != '_' && c != '-' && c != '.' && c != '/') {
            return qfalse;
        }

        // No way out of the folder: a dot or a slash is never followed by
        // another one, which leaves out "..", "//", "/." and "./".
        if ((c == '.' || c == '/') && (path[i + 1] == '.' || path[i + 1] == '/')) {
            return qfalse;
        }
    }

    return qtrue;
}

static qboolean MoHArena_UIOwnFile(const moharenaUIOwnFile_t *files, size_t count, const char *id, const char *path)
{
    size_t i;

    if (!id || !path) {
        return qfalse;
    }

    for (i = 0; i < count; i++) {
        if (!Q_stricmp(id, files[i].id)) {
            return Q_stricmp(path, files[i].path) ? qfalse : qtrue;
        }
    }

    // Not an id of the modern UI's own pack: any menu file may carry it.
    return qtrue;
}

qboolean MoHArena_UIMenuFile(const char *menuId, const char *path)
{
    return MoHArena_UIOwnFile(moharenaUIMenuFiles, ARRAY_LEN(moharenaUIMenuFiles), menuId, path);
}

qboolean MoHArena_UIHudFile(const char *hudId, const char *path)
{
    return MoHArena_UIOwnFile(moharenaUIHudFiles, ARRAY_LEN(moharenaUIHudFiles), hudId, path);
}

void MoHArena_UIPlainText(const char *text, char *out, size_t outSize)
{
    size_t length;
    size_t i;

    if (!out || !outSize) {
        return;
    }

    length = 0;
    if (text) {
        for (i = 0; text[i]; i++) {
            const unsigned char c = (unsigned char)text[i];

            if (c < 32 || c == 127) {
                continue;
            }

            if (length + 1 >= outSize) {
                break;
            }

            out[length++] = (char)c;
        }
    }

    out[length] = 0;
}
