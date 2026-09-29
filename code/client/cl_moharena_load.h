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

// cl_moharena_load.h -- Added in MoH Arena
//
// The Windows loader of the MoH Arena module (cl_moharena_win32.cpp). It uses
// no engine header, so windows.h and the engine's types never meet.

#pragma once

#include "moharena/moharena_opm_bootstrap_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads the launcher's three environment variables once and clears them,
// checks the module file next to the exe against the SHA-256 the launcher
// gave, and loads it. Returns the module's start function with bootstrap
// filled in, or NULL with a short failure code in reason. reason is empty
// when the launcher did not ask for the module at all.
MohArenaOpmStartFn MoHArena_LoadModule(MohArenaOpmBootstrapV1 *bootstrap, char *reason, size_t reasonSize);

#ifdef __cplusplus
}
#endif
