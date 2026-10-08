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

// cl_moharena_uipolicy.h -- Added in MoH Arena
//
// What the modern UI may change: the settings its menus may write and their
// values, the commands its key rows may bind and its buttons may run, and the
// files it may load. The lists are fixed in cl_moharena_uipolicy.cpp, so a
// menu file can only place the controls. Include it after client.h.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Writes a listed setting. qfalse when the setting is not listed or the menus
// may not write this value, and the setting then stays as it is.
qboolean MoHArena_UISettingSet(const char *name, const char *value);
// Puts a listed setting back to its default.
qboolean MoHArena_UISettingReset(const char *name);
// The settings the DEFAULTS button puts back, one per index; NULL after the
// last.
const char *MoHArena_UISettingDefault(int index);

// The command to bind for a key row: the listed spelling, "" to clear the key,
// or NULL when the command is not listed.
const char *MoHArena_UIBind(const char *binding);
// One command a menu button may run, rebuilt in out from the listed words.
qboolean    MoHArena_UICommand(const char *command, char *out, size_t outSize);
// A server address the browser may connect to.
qboolean    MoHArena_UIAddressAllowed(const char *address);
// A menu file the modern UI may load.
qboolean    MoHArena_UIFileAllowed(const char *path);
// Whether the file at path may carry this menu id or HUD id. The ids of the
// modern UI's own pack belong to its own files; any other id is free.
qboolean    MoHArena_UIMenuFile(const char *menuId, const char *path);
qboolean    MoHArena_UIHudFile(const char *hudId, const char *path);
// Copies text typed into the HUD chat without its control characters.
void        MoHArena_UIPlainText(const char *text, char *out, size_t outSize);

// Once per client frame in the modern UI mode: keeps cg_fov inside its plain
// limits while no game is being drawn.
void MoHArena_UIKeepFov(void);

#ifdef __cplusplus
}
#endif
