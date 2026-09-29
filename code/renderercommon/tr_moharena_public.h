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

// tr_moharena_public.h -- Added in MoH Arena
//
// A small optional draw API for the MoH Arena client bridge (cl_moharena.cpp):
// textures and indexed triangle lists drawn on top of the 2D screen. Only the
// GL1 renderer has it, and only when linked into the client. The renderer's
// own table (refexport_t) and REF_API_VERSION stay unchanged.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MOHARENA_RENDER_API_VERSION 1

// The most textures the API keeps at once.
#define MOHARENA_RENDER_MAX_TEXTURES 64

// A position in screen pixels (origin top-left), a texture coordinate and a
// color with straight alpha that modulates the texel.
typedef struct {
    float         x;
    float         y;
    float         u;
    float         v;
    unsigned char rgba[4];
} moharenaRenderVertex_t;

typedef struct {
    int version;

    // Nonzero while the renderer has a GL context and its images; every other
    // call fails without it.
    int (*IsReady)(void);
    // The largest texture side the driver takes.
    int (*MaxTextureSize)(void);

    // Textures are 32-bit RGBA, tightly packed rows. Handles are nonzero and
    // never reused soon. All return 0 on success and a negative value on
    // failure.
    int (*CreateTexture)(int width, int height, const unsigned char *rgba, unsigned int *outHandle);
    int (*UpdateTexture)(
        unsigned int handle, int x, int y, int width, int height, const unsigned char *rgba, int rowPitchBytes
    );
    int (*DestroyTexture)(unsigned int handle);
    // Frees every texture; the client calls it before the renderer shuts down.
    void (*DestroyAllTextures)(void);

    // Begin2D sets a full-screen 2D window with alpha blending; DrawTriangles
    // is valid only between Begin2D and End2D. The clip rectangle is in screen
    // pixels, origin top-left. texture 0 draws untextured.
    void (*Begin2D)(void);
    int (*DrawTriangles)(
        const moharenaRenderVertex_t *vertices,
        int                           numVertices,
        const unsigned int           *indices,
        int                           numIndices,
        unsigned int                  texture,
        int                           clipX,
        int                           clipY,
        int                           clipWidth,
        int                           clipHeight
    );
    void (*End2D)(void);
} moharenaRenderApi_t;

// NULL when the version is not supported.
const moharenaRenderApi_t *GetMoHArenaRenderAPI(int version);

#ifdef __cplusplus
}
#endif
