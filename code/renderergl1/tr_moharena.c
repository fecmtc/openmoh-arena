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

// tr_moharena.c -- Added in MoH Arena
//
// The draw API from tr_moharena_public.h. 2D here is immediate mode on the
// main thread after R_IssuePendingRenderCommands, as tr_draw.c does. The
// textures are plain GL textures kept outside tr.images, so neither the hunk
// nor the image cleanup touches them; the client frees them all before the
// renderer shuts down. Colors are scaled by tr.identityLight like the game's
// own 2D (Draw_SetColor), so overbright gamma does not brighten them.

#include "tr_local.h"
#include "../renderercommon/tr_moharena_public.h"

typedef struct {
    GLuint       texnum; // 0 when the slot is free
    unsigned int handle;
    int          width;
    int          height;
} moharenaTexture_t;

static moharenaTexture_t moharenaTextures[MOHARENA_RENDER_MAX_TEXTURES];
static unsigned int      moharenaSerial;
static qboolean          moharenaIn2D;

static int MoHArena_IsReady(void)
{
    return tr.registered && glConfig.vidWidth > 0 && glConfig.vidHeight > 0;
}

static int MoHArena_MaxTextureSize(void)
{
    return glConfig.maxTextureSize;
}

static moharenaTexture_t *MoHArena_FindTexture(unsigned int handle)
{
    unsigned int index;

    if (!handle) {
        return NULL;
    }

    index = (handle & 0xFF) - 1;
    if (index >= MOHARENA_RENDER_MAX_TEXTURES) {
        return NULL;
    }

    if (!moharenaTextures[index].texnum || moharenaTextures[index].handle != handle) {
        return NULL;
    }

    return &moharenaTextures[index];
}

// Binds on unit 0 through the renderer's cache, so GL_Bind stays right.
static void MoHArena_BindTexture(GLuint texnum)
{
    GL_SelectTexture(0);
    if (glState.currenttextures[0] != (int)texnum) {
        glState.currenttextures[0] = texnum;
        qglBindTexture(GL_TEXTURE_2D, texnum);
    }
}

static void MoHArena_DeleteTexture(moharenaTexture_t *entry)
{
    int unit;

    if (qglDeleteTextures && glConfig.vidWidth > 0) {
        qglDeleteTextures(1, &entry->texnum);
        // A deleted texture unbinds itself.
        for (unit = 0; unit < 2; unit++) {
            if (glState.currenttextures[unit] == (int)entry->texnum) {
                glState.currenttextures[unit] = 0;
            }
        }
    }

    Com_Memset(entry, 0, sizeof(*entry));
}

static int MoHArena_CreateTexture(int width, int height, const unsigned char *rgba, unsigned int *outHandle)
{
    moharenaTexture_t *entry;
    GLuint             texnum;
    int                index;

    if (outHandle) {
        *outHandle = 0;
    }

    if (!MoHArena_IsReady() || !rgba || !outHandle || width <= 0 || height <= 0) {
        return -1;
    }

    if (width > glConfig.maxTextureSize || height > glConfig.maxTextureSize) {
        return -1;
    }

    for (index = 0; index < MOHARENA_RENDER_MAX_TEXTURES; index++) {
        if (!moharenaTextures[index].texnum) {
            break;
        }
    }

    if (index == MOHARENA_RENDER_MAX_TEXTURES) {
        return -1;
    }

    texnum = 0;
    qglGenTextures(1, &texnum);
    if (!texnum) {
        return -1;
    }

    MoHArena_BindTexture(texnum);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, haveClampToEdge ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, haveClampToEdge ? GL_CLAMP_TO_EDGE : GL_CLAMP);

    // The low byte is the slot, the rest a serial, so a stale handle never
    // matches a new texture in the same slot soon.
    moharenaSerial = (moharenaSerial + 1) & 0xFFFFFF;
    if (!moharenaSerial) {
        moharenaSerial = 1;
    }

    entry         = &moharenaTextures[index];
    entry->texnum = texnum;
    entry->handle = (moharenaSerial << 8) | (unsigned int)(index + 1);
    entry->width  = width;
    entry->height = height;

    *outHandle = entry->handle;
    return 0;
}

static int MoHArena_UpdateTexture(
    unsigned int handle, int x, int y, int width, int height, const unsigned char *rgba, int rowPitchBytes
)
{
    const moharenaTexture_t *entry;
    int                      row;

    entry = MoHArena_FindTexture(handle);
    if (!MoHArena_IsReady() || !entry || !rgba || x < 0 || y < 0 || width <= 0 || height <= 0) {
        return -1;
    }

    if (x > entry->width || width > entry->width - x || y > entry->height || height > entry->height - y) {
        return -1;
    }

    if (rowPitchBytes % 4 || rowPitchBytes / 4 < width) {
        return -1;
    }

    MoHArena_BindTexture(entry->texnum);
    if (rowPitchBytes == width * 4) {
        qglTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return 0;
    }

    // No unpack row length here: one row at a time.
    for (row = 0; row < height; row++) {
        qglTexSubImage2D(
            GL_TEXTURE_2D, 0, x, y + row, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba + (size_t)row * rowPitchBytes
        );
    }

    return 0;
}

static int MoHArena_DestroyTexture(unsigned int handle)
{
    moharenaTexture_t *entry;

    entry = MoHArena_FindTexture(handle);
    if (!entry) {
        return -1;
    }

    MoHArena_DeleteTexture(entry);
    return 0;
}

static void MoHArena_DestroyAllTextures(void)
{
    int index;

    for (index = 0; index < MOHARENA_RENDER_MAX_TEXTURES; index++) {
        if (moharenaTextures[index].texnum) {
            MoHArena_DeleteTexture(&moharenaTextures[index]);
        }
    }
}

static void MoHArena_Begin2D(void)
{
    if (!MoHArena_IsReady()) {
        return;
    }

    // With the modern UI a repeated 2D window is skipped, and the queue would
    // then be drawn too late: draw what is queued and forget the last window.
    // With the original UI Set2DWindow never skips and draws the queue itself.
    if (R_ModernUI()) {
        R_IssuePendingRenderCommands();
        RE_InvalidateSet2DWindow();
    }
    Set2DWindow(
        0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1
    );
    RE_Scissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
    GL_SelectTexture(0);
    GL_TexEnv(GL_MODULATE);
    GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
    qglEnable(GL_TEXTURE_2D);
    moharenaIn2D = qtrue;
}

static int MoHArena_DrawTriangles(
    const moharenaRenderVertex_t *vertices,
    int                           numVertices,
    const unsigned int           *indices,
    int                           numIndices,
    unsigned int                  texture,
    int                           clipX,
    int                           clipY,
    int                           clipWidth,
    int                           clipHeight
)
{
    const moharenaTexture_t      *entry;
    const moharenaRenderVertex_t *vertex;
    byte                          color[4];
    int                           left, top, right, bottom;
    int                           i;

    if (!moharenaIn2D || !MoHArena_IsReady() || !vertices || !indices || numVertices <= 0 || numIndices <= 0) {
        return -1;
    }

    if (numIndices % 3 || clipWidth < 0 || clipHeight < 0) {
        return -1;
    }

    entry = NULL;
    if (texture) {
        entry = MoHArena_FindTexture(texture);
        if (!entry) {
            return -1;
        }
    }

    for (i = 0; i < numIndices; i++) {
        if (indices[i] >= (unsigned int)numVertices) {
            return -1;
        }
    }

    // The clip rectangle, inside the screen; GL's scissor counts from the
    // bottom.
    left   = clipX > 0 ? clipX : 0;
    top    = clipY > 0 ? clipY : 0;
    right  = clipX > glConfig.vidWidth - clipWidth ? glConfig.vidWidth : clipX + clipWidth;
    bottom = clipY > glConfig.vidHeight - clipHeight ? glConfig.vidHeight : clipY + clipHeight;
    if (right <= left || bottom <= top) {
        return 0;
    }

    // GL_Scissor, not qglScissor: the modern UI keeps track of the rectangle.
    GL_Scissor(left, glConfig.vidHeight - bottom, right - left, bottom - top);

    if (entry) {
        MoHArena_BindTexture(entry->texnum);
    } else {
        qglDisable(GL_TEXTURE_2D);
    }

    qglBegin(GL_TRIANGLES);
    for (i = 0; i < numIndices; i++) {
        vertex   = &vertices[indices[i]];
        color[0] = (byte)(vertex->rgba[0] * tr.identityLightByte / 255);
        color[1] = (byte)(vertex->rgba[1] * tr.identityLightByte / 255);
        color[2] = (byte)(vertex->rgba[2] * tr.identityLightByte / 255);
        color[3] = vertex->rgba[3];
        qglColor4ubv(color);
        qglTexCoord2f(vertex->u, vertex->v);
        qglVertex2f(vertex->x, vertex->y);
    }
    qglEnd();

    if (!entry) {
        qglEnable(GL_TEXTURE_2D);
    }

    return 0;
}

static void MoHArena_End2D(void)
{
    if (!moharenaIn2D) {
        return;
    }

    moharenaIn2D = qfalse;
    if (!MoHArena_IsReady()) {
        return;
    }

    RE_Scissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
    qglEnable(GL_TEXTURE_2D);
    qglColor4ubv(backEnd.color2D);
}

static const moharenaRenderApi_t moharenaRenderApi = {
    MOHARENA_RENDER_API_VERSION,
    MoHArena_IsReady,
    MoHArena_MaxTextureSize,
    MoHArena_CreateTexture,
    MoHArena_UpdateTexture,
    MoHArena_DestroyTexture,
    MoHArena_DestroyAllTextures,
    MoHArena_Begin2D,
    MoHArena_DrawTriangles,
    MoHArena_End2D,
};

const moharenaRenderApi_t *GetMoHArenaRenderAPI(int version)
{
    if (version != MOHARENA_RENDER_API_VERSION) {
        return NULL;
    }

    return &moharenaRenderApi;
}
