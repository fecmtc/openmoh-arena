#ifndef MOHARENA_OPM_ENGINE_V1_H
#define MOHARENA_OPM_ENGINE_V1_H

/* The two plain-C tables between OpenMoH Arena's client bridge and the MoH
 * Arena module (docs/openmohaa-bridge.md). The bridge, built into the game,
 * fills MohArenaOpmEngineV1 with engine services; the module fills
 * MohArenaOpmModuleV1 with its callbacks.
 *
 * Both tables are versioned and append-only: a field is read only when
 * struct_size covers it, and a null function is a missing service. The
 * bridge calls every module callback on the game's main thread, and the
 * module calls engine services only from inside those callbacks. Strings are
 * NUL-terminated unless a size comes with them. Integer results are 0 for
 * success and negative for failure. */

#include <stddef.h>
#include <stdint.h>

#include "moharena_host_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOHARENA_OPM_ENGINE_ABI_V1 UINT32_C(0x00010000)

/* The game's com_target_game values. */
enum {
    MOHARENA_OPM_GAME_AA = 0,
    MOHARENA_OPM_GAME_SH = 1,
    MOHARENA_OPM_GAME_BT = 2
};

/* OpenMoHAA's teamtype_t values, as STAT_TEAM and the players' configstrings
 * carry them. The bridge checks them against the game at compile time. */
enum {
    MOHARENA_OPM_TEAM_NONE = 0,
    MOHARENA_OPM_TEAM_SPECTATOR = 1,
    MOHARENA_OPM_TEAM_FREEFORALL = 2,
    MOHARENA_OPM_TEAM_ALLIES = 3,
    MOHARENA_OPM_TEAM_AXIS = 4
};

/* The client's connection state, reduced. */
enum {
    MOHARENA_OPM_CONNECTION_NONE = 0,    /* Disconnected or in the main menu. */
    MOHARENA_OPM_CONNECTION_LOADING = 1, /* Connecting or loading a map. */
    MOHARENA_OPM_CONNECTION_ACTIVE = 2   /* In the game. */
};

/* The one cvar flag the module may ask for when it registers a cvar. */
enum {
    MOHARENA_OPM_CVAR_USERINFO = 0x2
};

/* What the bridge does beyond the services in its table: the bits of the
 * engine table's `features`. A bridge from before a bit existed leaves it 0. */
enum {
    /* The bridge asks the module's filter_server_command about every "print"
     * server command, and drops the ones it refuses. */
    MOHARENA_OPM_FEATURE_PRINT_FILTER = 0x1,
    /* In AA the bridge asks the module's kill_line_color about every death
     * message, and shows the ones it names in green. */
    MOHARENA_OPM_FEATURE_KILL_COLORS = 0x2,
    /* The bridge's cgame reads the serverinfo key "moharena_firstperson":
     * while it is "1", a spectator sees through the eyes of the player they
     * follow. */
    MOHARENA_OPM_FEATURE_SPECTATOR_FIRST_PERSON = 0x4
};

typedef struct MohArenaOpmClientStateV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t connection;       /* MOHARENA_OPM_CONNECTION_* */
    uint32_t demo_playing;
    uint32_t cgame_loaded;
    uint32_t snapshot_valid;
    /* Changes with every new gamestate from the server. */
    uint32_t gamestate_serial;
    /* The current snapshot's server time; 0 without one. */
    int32_t server_time;
    /* From the current snapshot's player state. */
    int32_t local_slot;
    int32_t team;              /* MOHARENA_OPM_TEAM_* from STAT_TEAM */
    uint32_t zoomed;           /* STAT_INZOOM is nonzero */
    uint32_t crosshair;        /* STAT_CROSSHAIR is nonzero */
    uint32_t spectating;       /* PMF_SPECTATING */
    uint32_t camera_view;      /* PMF_CAMERA_VIEW */
    uint32_t intermission;     /* PMF_INTERMISSION */
    uint32_t no_hud;           /* PMF_NO_HUD */
    float fov;
    /* The window's drawable size in pixels. */
    uint32_t viewport_width;
    uint32_t viewport_height;
    /* 0 while the game window is not focused (com_unfocused). */
    uint32_t focused;
} MohArenaOpmClientStateV1;

/* One player's entity in the current snapshot. */
typedef struct MohArenaOpmEntityV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t present;
    /* MOHARENA_OPM_TEAM_ALLIES or _AXIS from the entity's own team flags,
     * otherwise MOHARENA_OPM_TEAM_NONE. */
    int32_t team;
    float origin[3];
} MohArenaOpmEntityV1;

/* The last 3D view the game rendered with its world. */
typedef struct MohArenaOpmViewV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t valid;
    /* Counts rendered world views; changes with every new one. */
    uint32_t serial;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    float fov_x;
    float fov_y;
    float origin[3];
    float axis[3][3]; /* forward, left, up */
} MohArenaOpmViewV1;

/* Stores the value and the cvar's flags. Fails when the cvar does not exist
 * or the value does not fit. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmCvarGetFn)(
    void *engine_context, const char *name, char *output, uint32_t capacity, uint32_t *output_size,
    uint32_t *flags);
/* Sets an existing cvar the way the console does, honoring its flags. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmCvarSetFn)(
    void *engine_context, const char *name, const char *value);
/* Creates a cvar, or returns the existing one unchanged. flags is 0 or
 * MOHARENA_OPM_CVAR_USERINFO. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmCvarRegisterFn)(
    void *engine_context, const char *name, const char *default_value, uint32_t flags);
/* Appends one line to the game's command buffer. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmCommandFn)(void *engine_context, const char *text);
typedef int32_t (MOHARENA_CALL *MohArenaOpmClientStateFn)(
    void *engine_context, MohArenaOpmClientStateV1 *output);
/* Copies one configstring of the current gamestate. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmConfigStringFn)(
    void *engine_context, int32_t index, char *output, uint32_t capacity, uint32_t *output_size);
typedef int32_t (MOHARENA_CALL *MohArenaOpmEntityFn)(
    void *engine_context, int32_t slot, MohArenaOpmEntityV1 *output);
typedef int32_t (MOHARENA_CALL *MohArenaOpmLastViewFn)(void *engine_context, MohArenaOpmViewV1 *output);
/* Copies the last print the server sent, and a serial that changes with
 * every new one. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmServerMessageFn)(
    void *engine_context, char *output, uint32_t capacity, uint32_t *output_size, uint32_t *serial);

/* Rendering, valid only inside the module's on_draw, where the bridge has
 * set up the renderer's 2D state. Texture handles are the renderer's own and
 * nonzero; every one is gone once the bridge sends MOHARENA_OPM_EVENT_RENDER_DOWN. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmRenderSizeFn)(
    void *engine_context, uint32_t *width, uint32_t *height, uint32_t *max_texture_size);
typedef int32_t (MOHARENA_CALL *MohArenaOpmCreateTextureFn)(
    void *engine_context, uint32_t width, uint32_t height, const uint8_t *rgba, uint32_t *out_texture);
typedef int32_t (MOHARENA_CALL *MohArenaOpmUpdateTextureFn)(
    void *engine_context, uint32_t texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    const uint8_t *rgba, uint32_t row_pitch_bytes);
typedef int32_t (MOHARENA_CALL *MohArenaOpmDestroyTextureFn)(void *engine_context, uint32_t texture);
/* The batch follows MohArenaDrawBatchV1's rules; the module has checked it. */
typedef int32_t (MOHARENA_CALL *MohArenaOpmDrawTrianglesFn)(
    void *engine_context, const MohArenaDrawBatchV1 *batch);

/* Sets the players whose taunt sounds the loaded cgame must not play: bit n
 * of slot_mask is the player in slot n. Returns 1 when the loaded cgame
 * applies the mask, and 0 when no cgame is loaded or the loaded one can't.
 * The mask ends with that cgame: the next one starts with none. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmSetVoiceMuteFn)(void *engine_context, uint64_t slot_mask);

/* Sets the sniper scopes the loaded cgame draws (moharena_scope_v1.h). The
 * cgame keeps a copy. Returns 1 when the loaded cgame took the scope, and 0
 * when no cgame is loaded, the loaded one has no such call, or the struct is
 * one it can't read: it then draws the game's own scopes. The scope ends with
 * that cgame: the next one starts with the game's own. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmSetScopeFn)(void *engine_context, const MohArenaScopeV1 *scope);

typedef struct MohArenaOpmEngineV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    void *engine_context;
    /* The game's version text, for the log only. */
    const char *engine_version;
    uint32_t target_game;          /* MOHARENA_OPM_GAME_* */
    uint32_t max_clients;
    int32_t cs_serverinfo;
    int32_t cs_players;
    int32_t cs_level_start_time;
    /* MOHARENA_OPM_FEATURE_* bits. It was a reserved field, always 0. */
    uint32_t features;
    MohArenaOpmCvarGetFn cvar_get;
    MohArenaOpmCvarSetFn cvar_set;
    MohArenaOpmCvarRegisterFn cvar_register;
    MohArenaOpmCommandFn command;
    MohArenaOpmClientStateFn client_state;
    MohArenaOpmConfigStringFn config_string;
    MohArenaOpmEntityFn entity;
    MohArenaOpmLastViewFn last_view;
    MohArenaOpmServerMessageFn server_message;
    MohArenaOpmRenderSizeFn render_size;
    MohArenaOpmCreateTextureFn create_texture;
    MohArenaOpmUpdateTextureFn update_texture;
    MohArenaOpmDestroyTextureFn destroy_texture;
    MohArenaOpmDrawTrianglesFn draw_triangles;
    /* Taken from the reserved fields, which a bridge always left null: the
     * table's size stays. */
    MohArenaOpmSetVoiceMuteFn set_voice_mute;
    MohArenaOpmSetScopeFn set_scope;
    void *reserved[6];
} MohArenaOpmEngineV1;

/* Events the bridge sends through on_event. The module starts with the
 * renderer down: the bridge sends RENDER_UP once the renderer runs, right
 * after the start when it already does. */
enum {
    /* Before the renderer shuts down: every texture is about to go. */
    MOHARENA_OPM_EVENT_RENDER_DOWN = 1,
    /* After the renderer started again. */
    MOHARENA_OPM_EVENT_RENDER_UP = 2,
    MOHARENA_OPM_EVENT_CGAME_LOADED = 3,
    MOHARENA_OPM_EVENT_CGAME_UNLOADED = 4,
    MOHARENA_OPM_EVENT_DISCONNECTED = 5,
    MOHARENA_OPM_EVENT_MAP_LOADING = 6,
    /* The game window lost focus. */
    MOHARENA_OPM_EVENT_FOCUS_LOST = 7
};

/* Once per client frame, after the frame's snapshot and before its screen. */
typedef void (MOHARENA_CALL *MohArenaOpmFrameFn)(void *module_context);
/* Once per drawn screen, after the game's own 2D and before the buffer swap. */
typedef void (MOHARENA_CALL *MohArenaOpmDrawFn)(void *module_context);
/* One key event. kind is MOHARENA_INPUT_KEY_DOWN or _KEY_UP with a Windows
 * virtual-key code, _BUTTON_DOWN or _BUTTON_UP with button 0 left, 1 right,
 * 2 middle, _WHEEL with notches in amount (positive away from the user), or
 * _CHARACTER with one Unicode code point. modifiers holds MOHARENA_MODIFIER_*
 * bits. Returns 1 when the module used the event and the game must drop it;
 * the game still gets every key-up, so held keys release. The bridge sends
 * F7 always and every other event only while input_captured is 1. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmInputFn)(
    void *module_context, uint32_t kind, uint32_t code, int32_t amount, uint32_t modifiers);
/* Mouse motion: a relative move in x and y, or with absolute set the
 * cursor's window position. Returns 1 when the game must drop it. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmMouseFn)(
    void *module_context, int32_t x, int32_t y, uint32_t absolute);
/* 1 while the module's menu owns keyboard and mouse. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmInputCapturedFn)(void *module_context);
typedef void (MOHARENA_CALL *MohArenaOpmEventFn)(void *module_context, uint32_t event);
/* One server command as the cgame reads it, split into its arguments. */
typedef void (MOHARENA_CALL *MohArenaOpmServerCommandFn)(
    void *module_context, uint32_t argc, const char *const *argv);
/* Asked for each new user command: 1 when its movement, jumping, attacks,
 * use and lean must be cleared. The view angles always stay. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmHoldMovementFn)(void *module_context, int32_t server_time);
/* Ends the module's work. Safe to call more than once; the bridge calls no
 * module function afterwards, frees the textures the module left and never
 * unloads the module. */
typedef void (MOHARENA_CALL *MohArenaOpmStopFn)(void *module_context);
/* Asked about a server command before the cgame reads it, for the commands
 * the engine's `features` name: nonzero drops the command, so the cgame never
 * sees it, and on_server_command is not called for it. The cgame may ask for
 * a command more than once: the answer must not change within a frame. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmFilterServerCommandFn)(
    void *module_context, uint32_t argc, const char *const *argv);

/* Asked about an AA death message before the cgame prints it: the text of a
 * "print" server command that starts with the game's red marker, the byte
 * 0x04. `line` is that text, marker included. Nonzero shows the line in
 * green; 0 leaves it red. The bridge asks after on_server_command, and never
 * about a command the filter dropped. The cgame may ask for a command more
 * than once: the last answer decides. */
typedef uint32_t (MOHARENA_CALL *MohArenaOpmKillLineColorFn)(
    void *module_context, const char *line);

typedef struct MohArenaOpmModuleV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    void *module_context;
    MohArenaOpmFrameFn on_frame;
    MohArenaOpmDrawFn on_draw;
    MohArenaOpmInputFn on_input;
    MohArenaOpmMouseFn on_mouse;
    MohArenaOpmInputCapturedFn input_captured;
    MohArenaOpmEventFn on_event;
    MohArenaOpmServerCommandFn on_server_command;
    MohArenaOpmHoldMovementFn hold_movement;
    MohArenaOpmStopFn stop;
    /* Taken from the reserved fields, which a module always left null: the
     * table's size stays. */
    MohArenaOpmFilterServerCommandFn filter_server_command;
    MohArenaOpmKillLineColorFn kill_line_color;
    void *reserved[6];
} MohArenaOpmModuleV1;

#ifdef __cplusplus
}
#endif

#endif
