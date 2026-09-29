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

// cl_moharena.h -- Added in MoH Arena
//
// The client's calls into the MoH Arena bridge (cl_moharena.cpp). Include it
// after client.h. Every call does nothing unless the launcher started the
// module, so the game runs as before without it.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Once, after the client and its renderer have started.
void MoHArena_Init(void);
// From CL_Shutdown; the module stops only when the game quits.
void MoHArena_Shutdown(qboolean quit);

// Once per client frame, before the screen is drawn.
void MoHArena_Frame(void);
// Once per drawn screen, after the game's own 2D and before the buffer swap.
void MoHArena_Draw(void);
// Before the renderer shuts down.
void MoHArena_RenderShutdown(void);

void MoHArena_MapLoading(void);
void MoHArena_Disconnected(void);
void MoHArena_CGameLoaded(void);
void MoHArena_CGameUnloaded(void);

// Input while the module's menu is open. Each returns qtrue when the game
// must drop the event.
qboolean MoHArena_KeyEvent(int key, qboolean down, unsigned time);
qboolean MoHArena_CharEvent(int ch);
qboolean MoHArena_MouseEvent(int dx, int dy);

// A server command the cgame is about to read, already split into Cmd_Argv.
void MoHArena_ServerCommand(void);
// clc.serverMessage has just changed.
void MoHArena_ServerMessage(void);

// Clears the movement of a new user command while the module holds it.
void MoHArena_HoldMovement(usercmd_t *cmd);
// The cgame's R_RenderScene: remembers the last world view, then renders.
void MoHArena_RenderScene(const refdef_t *fd);

#ifdef __cplusplus
}
#endif
