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

// Added in MoH Arena: the modern UI's entry points for a build without it
// (MOHARENA_MODERN_UI off, cmake/client.cmake). ui_legacy is always 1 then
// (MoHArena_InitUIMode), so the original UI does everything: these say "legacy"
// where asked, and otherwise do nothing.

#include "client.h"
#include "cl_uirender.h"
#include "cl_uiperf.h"
#include "cl_uivars.h"
#include "cl_uimenu_dispatcher.h"
#include "cl_hud_host.h"
#include "cl_killfeed.h"
#include "cl_messages_host.h"
#include "cl_objectives_host.h"
#include "cl_scoreboard_host.h"
#include "../uidesign/uid_profile.h"

#ifndef MOHARENA_NO_MODERN_UI
#    error "cl_modern_ui_off.cpp is only built without the modern UI"
#endif

extern "C" {

// cl_uirender.cpp

void CL_UIR_RegisterCvars(void) {}
void CL_UIR_Init(void) {}
void CL_UIR_Shutdown(void) {}
void CL_UIR_OnRendererRegistration(void) {}
void CL_UIR_OnResolutionChanged(void) {}

qboolean CL_UIR_UseLegacyMain(void)
{
    return qtrue;
}

void CL_UIR_GetUiVidSize(int *w, int *h)
{
    if (w) {
        *w = cls.glconfig.vidWidth;
    }
    if (h) {
        *h = cls.glconfig.vidHeight;
    }
}

void CL_UIR_MapMouseToUiVid(float *x, float *y) {}

const char *CL_UIR_ScoreboardMenuId(void)
{
    return "scoreboard";
}

qboolean CL_UIR_UseLegacyHud(void)
{
    return qtrue;
}

qboolean CL_UIR_UseModernHudPack(void)
{
    return qfalse;
}

void CL_UIR_OpenHudChat(int iMode) {}
void CL_UIR_CloseHudChat(void) {}
void CL_UIR_ToggleHudChat(int iMode) {}

qboolean CL_UIR_IsModernMainActive(void)
{
    return qfalse;
}

void CL_UIR_ActivateModernMain(void) {}
void CL_UIR_DeactivateModernMain(void) {}

qboolean CL_UIR_IsEligibleForModernMain(void)
{
    return qfalse;
}

qboolean CL_UIR_ModernMainNeedsStart(void)
{
    return qfalse;
}

void CL_UIR_SyncEligibility(void) {}
void CL_UIR_NotifyMapLoadBegin(void) {}
void CL_UIR_NotifyMapLoadFinished(void) {}

qboolean CL_UIR_ShouldRenderModernDisconnected(void)
{
    return qfalse;
}

qboolean CL_UIR_IsConnectedOverlayOpen(void)
{
    return qfalse;
}

void CL_UIR_OpenConnectedOverlay(void) {}
void CL_UIR_CloseConnectedOverlay(void) {}

qboolean CL_UIR_ShouldRenderConnectedOverlay(void)
{
    return qfalse;
}

void CL_UIR_OpenDmPause(const char *panel) {}
void CL_UIR_CloseDmPause(void) {}

qboolean CL_UIR_IsDmPauseOpen(void)
{
    return qfalse;
}

void CL_UIR_EnterModernInputModeKeepKeys(void) {}

qboolean CL_UIR_ShouldOwnInput(void)
{
    return qfalse;
}

void CL_UIR_RenderDisconnectedMain(void) {}
void CL_UIR_RenderModernOverlay(void) {}
void CL_UIR_DrawHudLayer(void) {}
void CL_UIR_ProfileBeginSample(const char *label) {}
void CL_UIR_ProfileEndSample(const char *kind) {}
void CL_UIR_ProfileDumpLoad(void) {}

qboolean CL_UIR_KeyEvent(int key, qboolean down, unsigned time)
{
    return qfalse;
}

qboolean CL_UIR_CharEvent(int ch)
{
    return qfalse;
}

void CL_UIR_SyncPointerMenus(void) {}

qboolean CL_UIR_LegacyModalOwnsInput(void)
{
    return qtrue;
}

qboolean CL_UIR_IsCapturingKeybind(void)
{
    return qfalse;
}

// cl_uiperf.cpp

cvar_t *ui_perf_gpu = NULL;

void CL_UIPerf_FrameBegin(void) {}
void CL_UIPerf_FrameEnd(void) {}

const UiPerfWindow *CL_UIPerf_Window(void)
{
    return NULL;
}

int CL_UIPerf_HudInteger(void)
{
    return 0;
}

// cl_uivars.cpp

qboolean CL_UIVar_IsStoreName(const char *name)
{
    return qfalse;
}

void CL_UIVar_Set(const char *name, const char *value) {}

// cl_uimenu_dispatcher.cpp

qboolean CL_UIMenu_OpenHold(const char *menuId)
{
    return qfalse;
}

qboolean CL_UIMenu_CloseHold(const char *menuId)
{
    return qfalse;
}

qboolean CL_UIMenu_IsOpen(const char *menuId)
{
    return qfalse;
}

qboolean CL_UIMenu_HasPointerMenuOpen(void)
{
    return qfalse;
}

// cl_hud_host.cpp, cl_killfeed.cpp

void UIR_Hud_NotifyPickedUpWeapon(const char *message) {}
void CL_KillFeed_HandlePrintDeathMsg(void) {}
void CL_KillFeed_HandleDeathPrint(const char *text, int friendly) {}

// cl_messages_host.cpp

void UIR_HudMessages_Clear(void) {}
void UIR_HudMessages_SetAlphaScale(float scale) {}
void UIR_HudMessages_AddRow(const uir_hud_message_input_t *row) {}
void UIR_HudMessages_NotifyChanged(void) {}

int UIR_HudMessages_GetRowCount(void)
{
    return 0;
}

void UIR_HudGameMessages_Clear(void) {}
void UIR_HudGameMessages_SetAlphaScale(float scale) {}
void UIR_HudGameMessages_AddRow(const uir_hud_message_input_t *row) {}
void UIR_HudGameMessages_NotifyChanged(void) {}

int UIR_HudGameMessages_GetRowCount(void)
{
    return 0;
}

void UIR_HudChat_Clear(void) {}
void UIR_HudChat_AddRow(const uir_hud_message_input_t *row) {}

// cl_objectives_host.cpp

void UIR_Objectives_Clear(void) {}
void UIR_Objectives_SetAlpha(float alpha) {}
void UIR_Objectives_AddRow(const uir_objective_row_t *row) {}
void UIR_Objectives_NotifyChanged(void) {}

// cl_scoreboard_host.cpp

void UIR_Scoreboard_Clear(void) {}
void UIR_Scoreboard_SetMeta(const uir_scoreboard_meta_t *meta) {}
void UIR_Scoreboard_AddRow(const uir_scoreboard_row_t *row) {}
void UIR_Scoreboard_SetRowCount(int count) {}
void UIR_Scoreboard_NotifyChanged(void) {}
void UIR_Scoreboard_MarkAllAlive(void) {}

// uidesign/uid_profile.cpp

void UID_ProfileResetLoad(void) {}
void UID_ProfileBegin(uid_prof_phase_t phase) {}
void UID_ProfileEnd(uid_prof_phase_t phase) {}
void UID_ProfileSetLoadLabel(const char *label) {}

} // extern "C"

UiPerfScope::UiPerfScope(int markId)
    : id(markId)
{
}

UiPerfScope::~UiPerfScope() {}
