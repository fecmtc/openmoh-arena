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

// cl_moharena.cpp -- Added in MoH Arena
//
// The client bridge to the MoH Arena module (moharena/*.h). When the launcher
// asks for it, the Windows loader (cl_moharena_win32.cpp) checks and loads the
// module, and the bridge starts it with a table of engine services. The client
// then calls the bridge at fixed points: once per frame, once per drawn
// screen, for input, and on renderer, cgame and connection changes. While the
// module's menu (F7) is open it gets keyboard and mouse; the console keeps its
// own keys, and every key-up still reaches the game so held keys release.
//
// Without the launcher, on any mismatch, or in builds without
// MOHARENA_NATIVE_BRIDGE, every call here does nothing. Drawing also needs the
// GL1 renderer linked into the client (MOHARENA_RENDER_API); without it the
// module gets no render services.

#include "client.h"
#include "cl_ui.h"
#include "cl_moharena.h"

#if defined(MOHARENA_NATIVE_BRIDGE) && defined(_WIN32)

#    include <stddef.h>
#    include <string.h>

#    include "cl_moharena_load.h"
#    ifdef MOHARENA_RENDER_API
#        include "../renderercommon/tr_moharena_public.h"
#    endif

// The most arguments of one server command sent on to the module.
#    define MOHARENA_SERVER_ARGS_MAX 1024
// The longest cvar name the module may use, with its terminator.
#    define MOHARENA_CVAR_NAME_MAX 64
// The largest batch the bridge draws.
#    define MOHARENA_BATCH_MAX (1 << 20)
// The largest texture side or position the bridge hands on.
#    define MOHARENA_TEXTURE_LIMIT 65536u
// Clip rectangles are clamped to this before they become integers.
#    define MOHARENA_CLIP_LIMIT 65536.0f
// The Windows virtual-key code of F7.
#    define MOHARENA_VK_F7 0x76

static_assert((int)TG_MOH == (int)MOHARENA_OPM_GAME_AA, "target game");
static_assert((int)TG_MOHTA == (int)MOHARENA_OPM_GAME_SH, "target game");
static_assert((int)TG_MOHTT == (int)MOHARENA_OPM_GAME_BT, "target game");
static_assert((int)TEAM_NONE == (int)MOHARENA_OPM_TEAM_NONE, "team");
static_assert((int)TEAM_SPECTATOR == (int)MOHARENA_OPM_TEAM_SPECTATOR, "team");
static_assert((int)TEAM_FREEFORALL == (int)MOHARENA_OPM_TEAM_FREEFORALL, "team");
static_assert((int)TEAM_ALLIES == (int)MOHARENA_OPM_TEAM_ALLIES, "team");
static_assert((int)TEAM_AXIS == (int)MOHARENA_OPM_TEAM_AXIS, "team");
static_assert((int)CVAR_USERINFO == (int)MOHARENA_OPM_CVAR_USERINFO, "cvar flag");

#    ifdef MOHARENA_RENDER_API
static_assert(sizeof(moharenaRenderVertex_t) == sizeof(MohArenaVertexV1), "vertex");
static_assert(offsetof(moharenaRenderVertex_t, x) == offsetof(MohArenaVertexV1, x), "vertex");
static_assert(offsetof(moharenaRenderVertex_t, y) == offsetof(MohArenaVertexV1, y), "vertex");
static_assert(offsetof(moharenaRenderVertex_t, u) == offsetof(MohArenaVertexV1, u), "vertex");
static_assert(offsetof(moharenaRenderVertex_t, v) == offsetof(MohArenaVertexV1, v), "vertex");
static_assert(offsetof(moharenaRenderVertex_t, rgba) == offsetof(MohArenaVertexV1, rgba), "vertex");
static_assert(sizeof(unsigned int) == sizeof(uint32_t), "index");
#    endif

static MohArenaOpmEngineV1 moharenaEngine;
static MohArenaOpmModuleV1 moharenaModule;
static MohArenaOpmViewV1   moharenaView;
static qboolean            moharenaInitDone;
// The module started and has not stopped.
static qboolean            moharenaRunning;
// The module's menu has keyboard and mouse.
static qboolean            moharenaCaptured;
// Inside Key_ClearStates: its key-ups go to the game.
static qboolean            moharenaClearing;
static qboolean            moharenaUnfocused;
static qboolean            moharenaCGameLoaded;
static unsigned int        moharenaGamestateSerial;
static unsigned int        moharenaMessageSerial;
// The last absolute cursor position sent.
static qboolean            moharenaCursorKnown;
static int                 moharenaCursorX;
static int                 moharenaCursorY;

#    ifdef MOHARENA_RENDER_API
static const moharenaRenderApi_t *moharenaRender;
// The module has had RENDER_UP and not RENDER_DOWN since.
static qboolean                   moharenaRenderUp;
// Inside the module's on_draw, between Begin2D and End2D.
static qboolean                   moharenaInDraw;
#    endif

/*
==============================================================================

Engine services

==============================================================================
*/

// Cvar names are plain identifiers. None holding a login is ever read or set.
static qboolean MoHArena_CvarNameAllowed(const char *name)
{
    static const char loginWord[] = {'p', 'a', 's', 's', 'w', 'o', 'r', 'd', 0};
    size_t            i;

    if (!name || !name[0]) {
        return qfalse;
    }

    for (i = 0; name[i]; i++) {
        const char c = name[i];

        if (i + 1 >= MOHARENA_CVAR_NAME_MAX) {
            return qfalse;
        }

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return qfalse;
        }
    }

    return Q_stristr(name, loginWord) ? qfalse : qtrue;
}

// Printable text without line breaks, shorter than limit.
static qboolean MoHArena_TextAllowed(const char *text, size_t limit)
{
    size_t i;

    if (!text) {
        return qfalse;
    }

    for (i = 0; text[i]; i++) {
        const unsigned char c = (unsigned char)text[i];

        if (i + 1 >= limit || c < 32 || c == 127) {
            return qfalse;
        }
    }

    return qtrue;
}

static int32_t MOHARENA_CALL MoHArena_CvarGet(
    void *context, const char *name, char *output, uint32_t capacity, uint32_t *outputSize, uint32_t *flags
)
{
    const cvar_t *var;
    size_t        length;

    (void)context;
    if (outputSize) {
        *outputSize = 0;
    }

    if (flags) {
        *flags = 0;
    }

    if (!output || !capacity) {
        return -1;
    }

    output[0] = 0;
    if (!MoHArena_CvarNameAllowed(name)) {
        return -1;
    }

    var = Cvar_FindVar(name);
    if (!var || !var->string) {
        return -1;
    }

    length = strlen(var->string);
    if (length >= capacity) {
        return -1;
    }

    memcpy(output, var->string, length + 1);
    if (outputSize) {
        *outputSize = (uint32_t)length;
    }

    if (flags) {
        *flags = (uint32_t)var->flags;
    }

    return 0;
}

static int32_t MOHARENA_CALL MoHArena_CvarSet(void *context, const char *name, const char *value)
{
    const cvar_t *var;

    (void)context;
    if (!MoHArena_CvarNameAllowed(name) || !MoHArena_TextAllowed(value, MAX_CVAR_VALUE_STRING)) {
        return -1;
    }

    if (!Cvar_FindVar(name)) {
        return -1;
    }

    // As the console sets it: read-only, init, cheat and latched cvars keep
    // their rules.
    Cvar_Set2(name, value, qfalse);

    var = Cvar_FindVar(name);
    if (!var || !var->string) {
        return -1;
    }

    if (!strcmp(var->string, value) || (var->latchedString && !strcmp(var->latchedString, value))) {
        return 0;
    }

    return -1;
}

static int32_t MOHARENA_CALL
MoHArena_CvarRegister(void *context, const char *name, const char *defaultValue, uint32_t flags)
{
    (void)context;
    if (!MoHArena_CvarNameAllowed(name) || !MoHArena_TextAllowed(defaultValue, MAX_CVAR_VALUE_STRING)) {
        return -1;
    }

    if (flags != 0 && flags != MOHARENA_OPM_CVAR_USERINFO) {
        return -1;
    }

    return Cvar_Get(name, defaultValue, flags ? CVAR_USERINFO : 0) ? 0 : -1;
}

static int32_t MOHARENA_CALL MoHArena_Command(void *context, const char *text)
{
    (void)context;
    if (!MoHArena_TextAllowed(text, MAX_STRING_CHARS) || !text[0]) {
        return -1;
    }

    Cbuf_AddText(text);
    Cbuf_AddText("\n");
    return 0;
}

static qboolean MoHArena_SnapshotValid(void)
{
    return cl.snap.valid && clc.state == CA_ACTIVE ? qtrue : qfalse;
}

static int32_t MOHARENA_CALL MoHArena_ClientState(void *context, MohArenaOpmClientStateV1 *output)
{
    const playerState_t *ps;
    int                  team;

    (void)context;
    if (!output) {
        return -1;
    }

    memset(output, 0, sizeof(*output));
    output->abi_version = MOHARENA_OPM_ENGINE_ABI_V1;
    output->struct_size = sizeof(*output);

    switch (clc.state) {
    case CA_ACTIVE:
        output->connection = MOHARENA_OPM_CONNECTION_ACTIVE;
        break;
    case CA_CONNECTING:
    case CA_CHALLENGING:
    case CA_CONNECTED:
    case CA_LOADING:
    case CA_PRIMED:
        output->connection = MOHARENA_OPM_CONNECTION_LOADING;
        break;
    default:
        output->connection = MOHARENA_OPM_CONNECTION_NONE;
        break;
    }

    output->demo_playing     = clc.demoplaying ? 1 : 0;
    output->cgame_loaded     = cge ? 1 : 0;
    output->gamestate_serial = moharenaGamestateSerial;
    output->local_slot       = -1;
    output->team             = MOHARENA_OPM_TEAM_NONE;

    if (MoHArena_SnapshotValid()) {
        ps = &cl.snap.ps;

        output->snapshot_valid = 1;
        output->server_time    = cl.snap.serverTime;
        output->local_slot     = ps->clientNum;

        // No team and free-for-all are no team: nobody is a teammate there.
        team = ps->stats[STAT_TEAM];
        if (team == TEAM_SPECTATOR || team == TEAM_ALLIES || team == TEAM_AXIS) {
            output->team = team;
        }

        output->zoomed       = ps->stats[STAT_INZOOM] ? 1 : 0;
        output->crosshair    = ps->stats[STAT_CROSSHAIR] ? 1 : 0;
        output->spectating   = (ps->pm_flags & PMF_SPECTATING) ? 1 : 0;
        output->camera_view  = (ps->pm_flags & PMF_CAMERA_VIEW) ? 1 : 0;
        output->intermission = (ps->pm_flags & PMF_INTERMISSION) ? 1 : 0;
        output->no_hud       = (ps->pm_flags & PMF_NO_HUD) ? 1 : 0;
        output->fov          = ps->fov;
    }

    output->viewport_width  = cls.glconfig.vidWidth > 0 ? (uint32_t)cls.glconfig.vidWidth : 0;
    output->viewport_height = cls.glconfig.vidHeight > 0 ? (uint32_t)cls.glconfig.vidHeight : 0;
    output->focused         = com_unfocused && com_unfocused->integer ? 0 : 1;
    return 0;
}

static int32_t MOHARENA_CALL
MoHArena_ConfigString(void *context, int32_t index, char *output, uint32_t capacity, uint32_t *outputSize)
{
    const char *text;
    size_t      length;
    int         offset;

    (void)context;
    if (outputSize) {
        *outputSize = 0;
    }

    if (!output || !capacity) {
        return -1;
    }

    output[0] = 0;
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        return -1;
    }

    // Only a gamestate the client has fully received.
    if (clc.state != CA_PRIMED && clc.state != CA_ACTIVE) {
        return -1;
    }

    offset = cl.gameState.stringOffsets[index];
    if (offset < 0 || offset >= MAX_GAMESTATE_CHARS) {
        return -1;
    }

    text   = cl.gameState.stringData + offset;
    length = strnlen(text, MAX_GAMESTATE_CHARS - offset);
    if (length >= capacity || offset + length >= MAX_GAMESTATE_CHARS) {
        return -1;
    }

    memcpy(output, text, length);
    output[length] = 0;
    if (outputSize) {
        *outputSize = (uint32_t)length;
    }

    return 0;
}

static int32_t MOHARENA_CALL MoHArena_Entity(void *context, int32_t slot, MohArenaOpmEntityV1 *output)
{
    const entityState_t *es;
    int                  allies, axis;
    int                  i;

    (void)context;
    if (!output) {
        return -1;
    }

    memset(output, 0, sizeof(*output));
    output->abi_version = MOHARENA_OPM_ENGINE_ABI_V1;
    output->struct_size = sizeof(*output);
    output->team        = MOHARENA_OPM_TEAM_NONE;

    if (slot < 0 || slot >= MAX_CLIENTS) {
        return -1;
    }

    if (!MoHArena_SnapshotValid()) {
        return 0;
    }

    for (i = 0; i < cl.snap.numEntities; i++) {
        es = &cl.parseEntities[(cl.snap.parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
        if (es->number != slot) {
            continue;
        }

        if (es->eType != ET_PLAYER) {
            break;
        }

        // The entity's own team flags; both or neither is no team.
        allies = (es->eFlags & EF_ALLIES) != 0;
        axis   = (es->eFlags & EF_AXIS) != 0;
        if (allies && !axis) {
            output->team = MOHARENA_OPM_TEAM_ALLIES;
        } else if (axis && !allies) {
            output->team = MOHARENA_OPM_TEAM_AXIS;
        }

        output->present   = 1;
        output->origin[0] = es->origin[0];
        output->origin[1] = es->origin[1];
        output->origin[2] = es->origin[2];
        break;
    }

    return 0;
}

static int32_t MOHARENA_CALL MoHArena_LastView(void *context, MohArenaOpmViewV1 *output)
{
    (void)context;
    if (!output) {
        return -1;
    }

    *output             = moharenaView;
    output->abi_version = MOHARENA_OPM_ENGINE_ABI_V1;
    output->struct_size = sizeof(*output);
    return 0;
}

static int32_t MOHARENA_CALL MoHArena_ServerMessageGet(
    void *context, char *output, uint32_t capacity, uint32_t *outputSize, uint32_t *serial
)
{
    size_t length;

    (void)context;
    if (outputSize) {
        *outputSize = 0;
    }

    if (serial) {
        *serial = moharenaMessageSerial;
    }

    if (!output || !capacity) {
        return -1;
    }

    output[0] = 0;
    length    = strnlen(clc.serverMessage, sizeof(clc.serverMessage));
    if (length >= capacity || length >= sizeof(clc.serverMessage)) {
        return -1;
    }

    memcpy(output, clc.serverMessage, length);
    output[length] = 0;
    if (outputSize) {
        *outputSize = (uint32_t)length;
    }

    return 0;
}

// The cgame holds the mask and plays no taunt sound of a player in it (cgame/cg_parsemsg.cpp). The bridge
// keeps nothing: it looks the export up in the cgame loaded now, and the next cgame starts with no mask.
static uint32_t MOHARENA_CALL MoHArena_SetVoiceMute(void *context, uint64_t slotMask)
{
    typedef void (*setVoiceMute_t)(uint64_t slotMask);
    setVoiceMute_t setVoiceMute;

    (void)context;
    if (!moharenaCGameLoaded) {
        return 0;
    }

    setVoiceMute = reinterpret_cast<setVoiceMute_t>(Sys_GetCGameFunction("MoHArena_SetVoiceMuteV1"));
    if (!setVoiceMute) {
        return 0;
    }

    setVoiceMute(slotMask);
    return 1;
}

// The cgame holds the sniper scopes and draws them in place of the game's own (cgame/cg_drawtools.cpp). The
// bridge keeps nothing: it looks the export up in the cgame loaded now and hands that cgame's answer back, and
// the next cgame starts with the game's own scopes.
static uint32_t MOHARENA_CALL MoHArena_SetScope(void *context, const MohArenaScopeV1 *scope)
{
    typedef uint32_t (*setScope_t)(const MohArenaScopeV1 *scope);
    setScope_t setScope;

    (void)context;
    if (!moharenaCGameLoaded) {
        return 0;
    }

    setScope = reinterpret_cast<setScope_t>(Sys_GetCGameFunction("MoHArena_SetScopeV1"));
    if (!setScope) {
        return 0;
    }

    return setScope(scope) ? 1 : 0;
}

#    ifdef MOHARENA_RENDER_API

static qboolean MoHArena_RenderReady(void)
{
    return moharenaRender && moharenaRenderUp && moharenaRender->IsReady() ? qtrue : qfalse;
}

static int32_t MOHARENA_CALL
MoHArena_RenderSize(void *context, uint32_t *width, uint32_t *height, uint32_t *maxTextureSize)
{
    (void)context;
    if (!width || !height || !maxTextureSize) {
        return -1;
    }

    *width          = 0;
    *height         = 0;
    *maxTextureSize = 0;
    if (!MoHArena_RenderReady() || cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0) {
        return -1;
    }

    *width          = (uint32_t)cls.glconfig.vidWidth;
    *height         = (uint32_t)cls.glconfig.vidHeight;
    *maxTextureSize = (uint32_t)moharenaRender->MaxTextureSize();
    return 0;
}

static int32_t MOHARENA_CALL MoHArena_CreateTexture(
    void *context, uint32_t width, uint32_t height, const uint8_t *rgba, uint32_t *outTexture
)
{
    unsigned int handle = 0;

    (void)context;
    if (outTexture) {
        *outTexture = 0;
    }

    if (!moharenaInDraw || !MoHArena_RenderReady() || !rgba || !outTexture) {
        return -1;
    }

    if (!width || !height || width > MOHARENA_TEXTURE_LIMIT || height > MOHARENA_TEXTURE_LIMIT) {
        return -1;
    }

    if (moharenaRender->CreateTexture((int)width, (int)height, rgba, &handle) != 0 || !handle) {
        return -1;
    }

    *outTexture = handle;
    return 0;
}

static int32_t MOHARENA_CALL MoHArena_UpdateTexture(
    void          *context,
    uint32_t       texture,
    uint32_t       x,
    uint32_t       y,
    uint32_t       width,
    uint32_t       height,
    const uint8_t *rgba,
    uint32_t       rowPitchBytes
)
{
    (void)context;
    if (!moharenaInDraw || !MoHArena_RenderReady() || !texture || !rgba) {
        return -1;
    }

    if (x > MOHARENA_TEXTURE_LIMIT || y > MOHARENA_TEXTURE_LIMIT || width > MOHARENA_TEXTURE_LIMIT
        || height > MOHARENA_TEXTURE_LIMIT || rowPitchBytes > MOHARENA_TEXTURE_LIMIT * 4) {
        return -1;
    }

    return moharenaRender->UpdateTexture(texture, (int)x, (int)y, (int)width, (int)height, rgba, (int)rowPitchBytes)
                == 0
             ? 0
             : -1;
}

static int32_t MOHARENA_CALL MoHArena_DestroyTexture(void *context, uint32_t texture)
{
    (void)context;
    if (!moharenaInDraw || !MoHArena_RenderReady() || !texture) {
        return -1;
    }

    return moharenaRender->DestroyTexture(texture) == 0 ? 0 : -1;
}

static int MoHArena_ClipEdge(float value, qboolean roundUp)
{
    value = roundUp ? ceilf(value) : floorf(value);
    if (value < -MOHARENA_CLIP_LIMIT) {
        value = -MOHARENA_CLIP_LIMIT;
    } else if (value > MOHARENA_CLIP_LIMIT) {
        value = MOHARENA_CLIP_LIMIT;
    }

    return (int)value;
}

static int32_t MOHARENA_CALL MoHArena_DrawTriangles(void *context, const MohArenaDrawBatchV1 *batch)
{
    int left, top, right, bottom;

    (void)context;
    if (!moharenaInDraw || !MoHArena_RenderReady() || !batch) {
        return -1;
    }

    if (batch->abi_version != MOHARENA_HOST_ABI_V1 || batch->struct_size < sizeof(MohArenaDrawBatchV1)) {
        return -1;
    }

    if (!batch->vertices || !batch->indices || !batch->vertex_count || !batch->index_count
        || batch->vertex_count > MOHARENA_BATCH_MAX || batch->index_count > MOHARENA_BATCH_MAX) {
        return -1;
    }

    if (!isfinite(batch->clip_x1) || !isfinite(batch->clip_y1) || !isfinite(batch->clip_x2)
        || !isfinite(batch->clip_y2)) {
        return -1;
    }

    // x1 and y1 are inclusive, x2 and y2 exclusive.
    left   = MoHArena_ClipEdge(batch->clip_x1, qfalse);
    top    = MoHArena_ClipEdge(batch->clip_y1, qfalse);
    right  = MoHArena_ClipEdge(batch->clip_x2, qtrue);
    bottom = MoHArena_ClipEdge(batch->clip_y2, qtrue);
    if (right <= left || bottom <= top) {
        return 0;
    }

    return moharenaRender->DrawTriangles(
               reinterpret_cast<const moharenaRenderVertex_t *>(batch->vertices),
               (int)batch->vertex_count,
               batch->indices,
               (int)batch->index_count,
               batch->texture,
               left,
               top,
               right - left,
               bottom - top
           )
                == 0
             ? 0
             : -1;
}

#    endif

static void MoHArena_FillEngine(void)
{
    memset(&moharenaEngine, 0, sizeof(moharenaEngine));
    moharenaEngine.abi_version         = MOHARENA_OPM_ENGINE_ABI_V1;
    moharenaEngine.struct_size         = sizeof(moharenaEngine);
    moharenaEngine.engine_context      = &moharenaEngine;
    moharenaEngine.engine_version      = com_version && com_version->string ? com_version->string : "";
    moharenaEngine.target_game         = UINT32_MAX;
    if (com_target_game && com_target_game->integer >= 0) {
        moharenaEngine.target_game = (uint32_t)com_target_game->integer;
    }
    moharenaEngine.max_clients         = MAX_CLIENTS;
    moharenaEngine.cs_serverinfo       = CS_SERVERINFO;
    moharenaEngine.cs_players          = CS_PLAYERS;
    moharenaEngine.cs_level_start_time = CS_LEVEL_START_TIME;
    // MoHArena_ServerCommand asks the module's filter about every "print", and in AA its kill_line_color about
    // every death message. The cgame reads the server's first-person spectator switch (cg_spectate_fp.c).
    moharenaEngine.features            = MOHARENA_OPM_FEATURE_PRINT_FILTER | MOHARENA_OPM_FEATURE_KILL_COLORS
                                       | MOHARENA_OPM_FEATURE_SPECTATOR_FIRST_PERSON;
    moharenaEngine.cvar_get            = MoHArena_CvarGet;
    moharenaEngine.cvar_set            = MoHArena_CvarSet;
    moharenaEngine.cvar_register       = MoHArena_CvarRegister;
    moharenaEngine.command             = MoHArena_Command;
    moharenaEngine.client_state        = MoHArena_ClientState;
    moharenaEngine.config_string       = MoHArena_ConfigString;
    moharenaEngine.entity              = MoHArena_Entity;
    moharenaEngine.last_view           = MoHArena_LastView;
    moharenaEngine.server_message      = MoHArena_ServerMessageGet;
    moharenaEngine.set_voice_mute      = MoHArena_SetVoiceMute;
    moharenaEngine.set_scope           = MoHArena_SetScope;

#    ifdef MOHARENA_RENDER_API
    moharenaRender = GetMoHArenaRenderAPI(MOHARENA_RENDER_API_VERSION);
    if (moharenaRender) {
        moharenaEngine.render_size     = MoHArena_RenderSize;
        moharenaEngine.create_texture  = MoHArena_CreateTexture;
        moharenaEngine.update_texture  = MoHArena_UpdateTexture;
        moharenaEngine.destroy_texture = MoHArena_DestroyTexture;
        moharenaEngine.draw_triangles  = MoHArena_DrawTriangles;
    }
#    endif
}

/*
==============================================================================

Module calls

==============================================================================
*/

static void MoHArena_SendEvent(uint32_t event)
{
    if (moharenaRunning) {
        moharenaModule.on_event(moharenaModule.module_context, event);
    }
}

// Follows the module's menu. When it opens, every held key is released first,
// so no +command stays on while the menu has the keys.
static void MoHArena_SyncCapture(void)
{
    qboolean captured;

    captured = moharenaRunning && moharenaModule.input_captured(moharenaModule.module_context) ? qtrue : qfalse;
    if (captured == moharenaCaptured) {
        return;
    }

    moharenaCaptured    = captured;
    moharenaCursorKnown = qfalse;
    if (captured && !moharenaClearing) {
        moharenaClearing = qtrue;
        Key_ClearStates();
        moharenaClearing = qfalse;
    }
}

// Tells the module once the renderer runs: after the start, and again after
// every renderer restart.
static void MoHArena_SyncRender(void)
{
#    ifdef MOHARENA_RENDER_API
    if (!moharenaRunning || moharenaRenderUp || !moharenaRender || !moharenaRender->IsReady()) {
        return;
    }

    moharenaRenderUp = qtrue;
    MoHArena_SendEvent(MOHARENA_OPM_EVENT_RENDER_UP);
#    endif
}

static void MoHArena_FreeTextures(void)
{
#    ifdef MOHARENA_RENDER_API
    if (moharenaRender) {
        moharenaRender->DestroyAllTextures();
    }
#    endif
}

static void MoHArena_ForgetView(void)
{
    moharenaView.valid = 0;
}

// When the game's own cursor is out (windowed, with the game's menus open),
// the module's cursor follows it.
static void MoHArena_FollowCursor(void)
{
    int x, y;

    if (!moharenaRunning || !moharenaCaptured || IN_IsCursorActive() || moharenaUnfocused) {
        moharenaCursorKnown = qfalse;
        return;
    }

    x = 0;
    y = 0;
    IN_GetMousePosition(&x, &y);
    if (moharenaCursorKnown && x == moharenaCursorX && y == moharenaCursorY) {
        return;
    }

    moharenaCursorKnown = qtrue;
    moharenaCursorX     = x;
    moharenaCursorY     = y;
    moharenaModule.on_mouse(moharenaModule.module_context, x, y, 1);
}

static uint32_t MoHArena_Modifiers(void)
{
    uint32_t modifiers = 0;

    if (keys[K_CTRL].down) {
        modifiers |= MOHARENA_MODIFIER_CTRL;
    }

    if (keys[K_SHIFT].down) {
        modifiers |= MOHARENA_MODIFIER_SHIFT;
    }

    if (keys[K_ALT].down) {
        modifiers |= MOHARENA_MODIFIER_ALT;
    }

    return modifiers;
}

// The Windows virtual-key code of a game key, or 0 when the menu has no use
// for it.
static uint32_t MoHArena_VirtualKey(int key)
{
    if (key >= 'a' && key <= 'z') {
        return 0x41 + (key - 'a');
    }

    if (key >= '0' && key <= '9') {
        return 0x30 + (key - '0');
    }

    if (key >= K_F1 && key <= K_F15) {
        return 0x70 + (key - K_F1);
    }

    switch (key) {
    case K_TAB:
        return 0x09;
    case K_ENTER:
    case K_KP_ENTER:
        return 0x0D;
    case K_ESCAPE:
        return 0x1B;
    case K_SPACE:
        return 0x20;
    case K_BACKSPACE:
        return 0x08;
    case K_UPARROW:
    case K_KP_UPARROW:
        return 0x26;
    case K_DOWNARROW:
    case K_KP_DOWNARROW:
        return 0x28;
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
        return 0x25;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
        return 0x27;
    case K_ALT:
        return 0x12;
    case K_CTRL:
        return 0x11;
    case K_SHIFT:
        return 0x10;
    case K_INS:
    case K_KP_INS:
        return 0x2D;
    case K_DEL:
    case K_KP_DEL:
        return 0x2E;
    case K_PGDN:
    case K_KP_PGDN:
        return 0x22;
    case K_PGUP:
    case K_KP_PGUP:
        return 0x21;
    case K_HOME:
    case K_KP_HOME:
        return 0x24;
    case K_END:
    case K_KP_END:
        return 0x23;
    case K_KP_5:
        return 0x0C;
    case K_KP_SLASH:
        return 0x6F;
    case K_KP_MINUS:
        return 0x6D;
    case K_KP_PLUS:
        return 0x6B;
    case K_KP_STAR:
        return 0x6A;
    case K_KP_NUMLOCK:
        return 0x90;
    case ';':
        return 0xBA;
    case '=':
        return 0xBB;
    case ',':
        return 0xBC;
    case '-':
        return 0xBD;
    case '.':
        return 0xBE;
    case '/':
        return 0xBF;
    case '`':
        return 0xC0;
    case '[':
        return 0xDB;
    case '\\':
        return 0xDC;
    case ']':
        return 0xDD;
    case '\'':
        return 0xDE;
    case K_PAUSE:
        return 0x13;
    case K_CAPSLOCK:
        return 0x14;
    case K_SUPER:
        return 0x5B;
    case K_MENU:
        return 0x5D;
    case K_PRINT:
        return 0x2C;
    case K_SCROLLOCK:
        return 0x91;
    default:
        return 0;
    }
}

/*
==============================================================================

Client calls

==============================================================================
*/

void MoHArena_Init(void)
{
    MohArenaOpmBootstrapV1   bootstrap;
    MohArenaOpmStartResultV1 result;
    MohArenaOpmStartFn       start;
    MohArenaResult           code;
    char                     reason[64];
    char                     failure[sizeof(result.failure_code)];
    size_t                   i, length;

    if (moharenaInitDone) {
        return;
    }

    moharenaInitDone = qtrue;
    reason[0]        = 0;
    memset(&bootstrap, 0, sizeof(bootstrap));
    start = MoHArena_LoadModule(&bootstrap, reason, sizeof(reason));
    if (!start) {
        if (reason[0]) {
            Com_Printf("MoH Arena: native client off (%s)\n", reason);
        } else {
            Com_DPrintf("MoH Arena: native client not requested\n");
        }
        return;
    }

    MoHArena_FillEngine();

    memset(&moharenaModule, 0, sizeof(moharenaModule));
    moharenaModule.abi_version = MOHARENA_OPM_ENGINE_ABI_V1;
    moharenaModule.struct_size = sizeof(moharenaModule);
    memset(&result, 0, sizeof(result));
    result.abi_version = MOHARENA_OPM_BOOTSTRAP_ABI_V1;
    result.struct_size = sizeof(result);

    code = start(&bootstrap, &moharenaEngine, &moharenaModule, &result);
    memset(&bootstrap, 0, sizeof(bootstrap));

    if (code != MOHARENA_OK) {
        // Only the code's plain characters reach the log.
        length = 0;
        for (i = 0; i < sizeof(result.failure_code) && result.failure_code[i]; i++) {
            const char c = result.failure_code[i];

            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
                failure[length++] = c;
            }
        }
        failure[length < sizeof(failure) ? length : sizeof(failure) - 1] = 0;

        memset(&moharenaModule, 0, sizeof(moharenaModule));
        Com_Printf("MoH Arena: native client off (%s)\n", failure[0] ? failure : "start_failed");
        return;
    }

    if (moharenaModule.abi_version != MOHARENA_OPM_ENGINE_ABI_V1
        || moharenaModule.struct_size < sizeof(MohArenaOpmModuleV1) || !moharenaModule.on_frame
        || !moharenaModule.on_draw || !moharenaModule.on_input || !moharenaModule.on_mouse
        || !moharenaModule.input_captured || !moharenaModule.on_event || !moharenaModule.on_server_command
        || !moharenaModule.hold_movement || !moharenaModule.stop) {
        if (moharenaModule.struct_size >= sizeof(MohArenaOpmModuleV1) && moharenaModule.stop) {
            moharenaModule.stop(moharenaModule.module_context);
        }

        memset(&moharenaModule, 0, sizeof(moharenaModule));
        MoHArena_FreeTextures();
        Com_Printf("MoH Arena: native client off (module_table)\n");
        return;
    }

    moharenaRunning = qtrue;
    Com_Printf("MoH Arena: native client on\n");
    MoHArena_SyncRender();
}

void MoHArena_Shutdown(qboolean quit)
{
    if (!quit || !moharenaRunning) {
        return;
    }

    moharenaModule.stop(moharenaModule.module_context);
    moharenaRunning  = qfalse;
    moharenaCaptured = qfalse;
    memset(&moharenaModule, 0, sizeof(moharenaModule));

#    ifdef MOHARENA_RENDER_API
    moharenaRenderUp = qfalse;
#    endif
    MoHArena_FreeTextures();
}

void MoHArena_Frame(void)
{
    qboolean unfocused;

    if (!moharenaRunning) {
        return;
    }

    MoHArena_SyncRender();

    unfocused = com_unfocused && com_unfocused->integer ? qtrue : qfalse;
    if (unfocused && !moharenaUnfocused) {
        MoHArena_SendEvent(MOHARENA_OPM_EVENT_FOCUS_LOST);
    }
    moharenaUnfocused = unfocused;

    moharenaModule.on_frame(moharenaModule.module_context);
    MoHArena_SyncCapture();
    MoHArena_FollowCursor();
}

void MoHArena_Draw(void)
{
    if (!moharenaRunning) {
        return;
    }

    MoHArena_SyncRender();

#    ifdef MOHARENA_RENDER_API
    // Never in saveshot's clean redraw.
    if (moharenaRenderUp && !cls.no_menus && moharenaRender->IsReady()) {
        moharenaRender->Begin2D();
        moharenaInDraw = qtrue;
        moharenaModule.on_draw(moharenaModule.module_context);
        moharenaInDraw = qfalse;
        moharenaRender->End2D();
    }
#    endif

    MoHArena_SyncCapture();
}

void MoHArena_RenderShutdown(void)
{
#    ifdef MOHARENA_RENDER_API
    if (moharenaRenderUp) {
        moharenaRenderUp = qfalse;
        MoHArena_SendEvent(MOHARENA_OPM_EVENT_RENDER_DOWN);
    }
#    endif

    MoHArena_FreeTextures();
}

void MoHArena_MapLoading(void)
{
    MoHArena_ForgetView();
    MoHArena_SendEvent(MOHARENA_OPM_EVENT_MAP_LOADING);
    MoHArena_SyncCapture();
}

void MoHArena_Disconnected(void)
{
    MoHArena_ForgetView();
    MoHArena_SendEvent(MOHARENA_OPM_EVENT_DISCONNECTED);
    MoHArena_SyncCapture();
}

void MoHArena_CGameLoaded(void)
{
    moharenaGamestateSerial++;
    moharenaCGameLoaded = qtrue;
    MoHArena_ForgetView();
    MoHArena_SendEvent(MOHARENA_OPM_EVENT_CGAME_LOADED);
}

void MoHArena_CGameUnloaded(void)
{
    MoHArena_ForgetView();
    if (!moharenaCGameLoaded) {
        return;
    }

    moharenaCGameLoaded = qfalse;
    MoHArena_SendEvent(MOHARENA_OPM_EVENT_CGAME_UNLOADED);
}

qboolean MoHArena_KeyEvent(int key, qboolean down, unsigned time)
{
    uint32_t used;
    uint32_t button;
    uint32_t virtualKey;

    (void)time;
    if (!moharenaRunning || moharenaClearing) {
        return qfalse;
    }

    // F7 always goes to the module, which toggles its menu once per press.
    if (key == K_F7) {
        if (down && keys[K_F7].repeats > 1) {
            return qtrue;
        }

        used = moharenaModule.on_input(
            moharenaModule.module_context,
            down ? MOHARENA_INPUT_KEY_DOWN : MOHARENA_INPUT_KEY_UP,
            MOHARENA_VK_F7,
            0,
            MoHArena_Modifiers()
        );
        MoHArena_SyncCapture();
        return down && used ? qtrue : qfalse;
    }

    if (!moharenaCaptured) {
        return qfalse;
    }

    // The console keeps its keys, and has all of them while it is open;
    // Alt+Enter still switches the window mode.
    if (key == K_CONSOLE || (key == K_ESCAPE && keys[K_SHIFT].down) || (key == K_ENTER && keys[K_ALT].down)
        || UI_ConsoleIsOpen()) {
        return qfalse;
    }

    // The menu makes its own repeats from held keys.
    if (down && keys[key].repeats > 1) {
        return qtrue;
    }

    switch (key) {
    case K_MWHEELUP:
    case K_MWHEELDOWN:
        if (down) {
            moharenaModule.on_input(
                moharenaModule.module_context,
                MOHARENA_INPUT_WHEEL,
                0,
                key == K_MWHEELUP ? 1 : -1,
                MoHArena_Modifiers()
            );
            MoHArena_SyncCapture();
        }
        return qtrue;
    case K_MOUSE1:
    case K_MOUSE2:
    case K_MOUSE3:
        button = key == K_MOUSE1 ? 0 : key == K_MOUSE2 ? 1 : 2;
        used   = moharenaModule.on_input(
            moharenaModule.module_context,
            down ? MOHARENA_INPUT_BUTTON_DOWN : MOHARENA_INPUT_BUTTON_UP,
            button,
            0,
            MoHArena_Modifiers()
        );
        MoHArena_SyncCapture();
        return down && used ? qtrue : qfalse;
    case K_MOUSE4:
    case K_MOUSE5:
        return down ? qtrue : qfalse;
    default:
        break;
    }

    virtualKey = MoHArena_VirtualKey(key);
    if (!virtualKey) {
        return down ? qtrue : qfalse;
    }

    used = moharenaModule.on_input(
        moharenaModule.module_context,
        down ? MOHARENA_INPUT_KEY_DOWN : MOHARENA_INPUT_KEY_UP,
        virtualKey,
        0,
        MoHArena_Modifiers()
    );
    MoHArena_SyncCapture();
    return down && used ? qtrue : qfalse;
}

qboolean MoHArena_CharEvent(int ch)
{
    if (!moharenaRunning || !moharenaCaptured || UI_ConsoleIsOpen()) {
        return qfalse;
    }

    // Control characters come as keys too; the menu takes those.
    if (ch >= 32 && ch != 127) {
        moharenaModule.on_input(
            moharenaModule.module_context, MOHARENA_INPUT_CHARACTER, (uint32_t)ch, 0, MoHArena_Modifiers()
        );
    }

    return qtrue;
}

qboolean MoHArena_MouseEvent(int dx, int dy)
{
    uint32_t used;

    if (!moharenaRunning || !moharenaCaptured || UI_ConsoleIsOpen()) {
        return qfalse;
    }

    used = moharenaModule.on_mouse(moharenaModule.module_context, dx, dy, 0);
    return used ? qtrue : qfalse;
}

qboolean MoHArena_ServerCommand(void)
{
    static const char *args[MOHARENA_SERVER_ARGS_MAX];
    int                count;
    int                i;

    if (!moharenaRunning) {
        return qfalse;
    }

    count = Cmd_Argc();
    if (count <= 0) {
        return qfalse;
    }

    if (count > MOHARENA_SERVER_ARGS_MAX) {
        count = MOHARENA_SERVER_ARGS_MAX;
    }

    for (i = 0; i < count; i++) {
        args[i] = Cmd_Argv(i);
    }

    // The module's filter sees every "print" first; an older module has none. A command it drops goes no
    // further: on_server_command is not called for it, and the cgame never reads it.
    if (moharenaModule.filter_server_command && !strcmp(args[0], "print")
        && moharenaModule.filter_server_command(moharenaModule.module_context, (uint32_t)count, args)) {
        return qtrue;
    }

    moharenaModule.on_server_command(moharenaModule.module_context, (uint32_t)count, args);

    // An AA server sends every death message as a red "print". The module names the ones to show in green, and
    // the bridge writes the green marker over the red one before the cgame prints the text; an older module has
    // no such call. The cgame reads a command twice in a frame and each read splits it afresh, so the bridge
    // asks and writes on every read.
    if (moharenaModule.kill_line_color && moharenaEngine.target_game == MOHARENA_OPM_GAME_AA && count >= 2
        && !strcmp(args[0], "print") && args[1][0] == MESSAGE_CHAT_RED
        && moharenaModule.kill_line_color(moharenaModule.module_context, args[1])) {
        // count >= 2, so this is the command's own text and never the shared empty string
        Cmd_Argv(1)[0] = MESSAGE_CHAT_GREEN;
    }

    return qfalse;
}

void MoHArena_ServerMessage(void)
{
    moharenaMessageSerial++;
}

void MoHArena_HoldMovement(usercmd_t *cmd)
{
    if (!moharenaRunning || !cmd) {
        return;
    }

    if (!moharenaModule.hold_movement(moharenaModule.module_context, cmd->serverTime)) {
        return;
    }

    // Moves, jump and crouch, attacks, use and lean; the view stays free.
    cmd->forwardmove = 0;
    cmd->rightmove   = 0;
    cmd->upmove      = 0;
    cmd->buttons &= (unsigned short)~(
        BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT | BUTTON_USE | BUTTON_LEAN_LEFT | BUTTON_LEAN_RIGHT
    );
}

void MoHArena_RenderScene(const refdef_t *fd)
{
    int i;

    // The world view only: menus and model previews draw without the world.
    if (fd && !(fd->rdflags & RDF_NOWORLDMODEL)) {
        moharenaView.valid = 1;
        moharenaView.serial++;
        moharenaView.x      = fd->x;
        moharenaView.y      = fd->y;
        moharenaView.width  = fd->width;
        moharenaView.height = fd->height;
        moharenaView.fov_x  = fd->fov_x;
        moharenaView.fov_y  = fd->fov_y;
        for (i = 0; i < 3; i++) {
            moharenaView.origin[i]  = fd->vieworg[i];
            moharenaView.axis[0][i] = fd->viewaxis[0][i];
            moharenaView.axis[1][i] = fd->viewaxis[1][i];
            moharenaView.axis[2][i] = fd->viewaxis[2][i];
        }
    }

    re.RenderScene(fd);
}

#else

void MoHArena_Init(void) {}

void MoHArena_Shutdown(qboolean quit)
{
    (void)quit;
}

void MoHArena_Frame(void) {}

void MoHArena_Draw(void) {}

void MoHArena_RenderShutdown(void) {}

void MoHArena_MapLoading(void) {}

void MoHArena_Disconnected(void) {}

void MoHArena_CGameLoaded(void) {}

void MoHArena_CGameUnloaded(void) {}

qboolean MoHArena_KeyEvent(int key, qboolean down, unsigned time)
{
    (void)key;
    (void)down;
    (void)time;
    return qfalse;
}

qboolean MoHArena_CharEvent(int ch)
{
    (void)ch;
    return qfalse;
}

qboolean MoHArena_MouseEvent(int dx, int dy)
{
    (void)dx;
    (void)dy;
    return qfalse;
}

qboolean MoHArena_ServerCommand(void)
{
    return qfalse;
}

void MoHArena_ServerMessage(void) {}

void MoHArena_HoldMovement(usercmd_t *cmd)
{
    (void)cmd;
}

void MoHArena_RenderScene(const refdef_t *fd)
{
    re.RenderScene(fd);
}

#endif
