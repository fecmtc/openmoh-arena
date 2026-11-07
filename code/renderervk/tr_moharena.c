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
// The draw API from tr_moharena_public.h, as renderergl1/tr_moharena.c has it
// on main. 2D is drawn on the spot after R_IssuePendingRenderCommands, as
// RE_StretchRaw draws the cinematics; vk_draw copies the vertexes into the
// frame's own buffer during the call.
//
// With the render thread (r_smp, tr_smp.c), R_IssuePendingRenderCommands
// hands what is queued to the thread and waits for it; the front end then
// owns the back end's state until the next hand-over. So nothing here touches
// that state before it: Begin2D only queues the 2D window, each draw sets the
// state it needs after the wait, and End2D restores it only after a draw.
// A frame with F7 drawing waits for the thread once per draw; a frame
// without draws never waits.
//
// The textures are image_t of their own, outside tr.images, so neither the
// hunk nor R_DeleteTextures touches them; the client frees them all before
// the renderer shuts down. The backend keeps them right for the frames in
// flight: an upload in the middle of a frame is seen only by the draws
// recorded after it, and a destroyed texture's storage is released once no
// frame can use it.
//
// Colors are scaled by tr.identityLight like the game's own 2D
// (Draw_SetColor), so overbright gamma does not brighten them. Drawn after
// Set2DWindow, which ends the temporal path's scene: the post effects (TAA,
// AO, motion blur) never reach it.

#include "tr_local.h"
#include "../renderercommon/tr_moharena_public.h"

typedef struct {
	image_t			image;		// image.texnum is 0 when the slot is free
	unsigned int	handle;
} moharenaTexture_t;

static moharenaTexture_t	moharenaTextures[MOHARENA_RENDER_MAX_TEXTURES];
static unsigned int			moharenaSerial;
static qboolean				moharenaIn2D;
static qboolean				moharenaDrew;	// a draw since Begin2D: the front end owns the back end

// Room for converted vertexes and repacked texture rows; it grows as needed
// and is freed with the textures.
static byte	*moharenaScratch;
static int	moharenaScratchSize;

static int MoHArena_IsReady( void ) {
	return tr.registered && glConfig.vidWidth > 0 && glConfig.vidHeight > 0;
}

static int MoHArena_MaxTextureSize( void ) {
	return glConfig.maxTextureSize;
}

static byte *MoHArena_Scratch( size_t size ) {
	if ( size > (size_t)INT_MAX ) {
		return NULL;
	}

	if ( (int)size > moharenaScratchSize ) {
		if ( moharenaScratch ) {
			ri.Free( moharenaScratch );
		}
		moharenaScratch = ri.Malloc( (int)size );
		moharenaScratchSize = moharenaScratch ? (int)size : 0;
	}

	return moharenaScratch;
}

static void MoHArena_FreeScratch( void ) {
	if ( moharenaScratch ) {
		ri.Free( moharenaScratch );
	}
	moharenaScratch = NULL;
	moharenaScratchSize = 0;
}

static moharenaTexture_t *MoHArena_FindTexture( unsigned int handle ) {
	unsigned int index;

	if ( !handle ) {
		return NULL;
	}

	index = ( handle & 0xFF ) - 1;
	if ( index >= MOHARENA_RENDER_MAX_TEXTURES ) {
		return NULL;
	}

	if ( !moharenaTextures[index].image.texnum || moharenaTextures[index].handle != handle ) {
		return NULL;
	}

	return &moharenaTextures[index];
}

static void MoHArena_DeleteTexture( moharenaTexture_t *entry ) {
	int unit;

	// the render thread may still bind it (vk_destroy_texture waits too, but
	// only after the cache below)
	R_FrontEndSync();

	// R_Bind's cache must not keep the handle
	for ( unit = 0; unit < 2; unit++ ) {
		if ( glState.currenttextures[unit] == (int)entry->image.texnum ) {
			glState.currenttextures[unit] = 0;
		}
	}

	// without the device (after vk_shutdown) this only resets texnum
	vk_destroy_texture( &entry->image );
	Com_Memset( entry, 0, sizeof( *entry ) );
}

static int MoHArena_CreateTexture( int width, int height, const unsigned char *rgba, unsigned int *outHandle ) {
	moharenaTexture_t	*entry;
	int					index;

	if ( outHandle ) {
		*outHandle = 0;
	}

	if ( !MoHArena_IsReady() || !rgba || !outHandle || width <= 0 || height <= 0 ) {
		return -1;
	}

	if ( width > glConfig.maxTextureSize || height > glConfig.maxTextureSize ) {
		return -1;
	}

	for ( index = 0; index < MOHARENA_RENDER_MAX_TEXTURES; index++ ) {
		if ( !moharenaTextures[index].image.texnum ) {
			break;
		}
	}

	if ( index == MOHARENA_RENDER_MAX_TEXTURES ) {
		return -1;
	}

	entry = &moharenaTextures[index];
	Com_Memset( entry, 0, sizeof( *entry ) );
	Q_strncpyz( entry->image.imgName, "*moharena", sizeof( entry->image.imgName ) );
	entry->image.width = entry->image.uploadWidth = width;
	entry->image.height = entry->image.uploadHeight = height;
	entry->image.internalFormat = TEXFMT_RGBA8;
	entry->image.numMipmaps = 1;
	entry->image.dynamicallyUpdated = qtrue;
	entry->image.wrapClampModeX = WRAP_CLAMP;
	entry->image.wrapClampModeY = WRAP_CLAMP;
	entry->image.bytesUsed = width * height * 4;

	// one level: textures without mipmaps are filtered linearly
	vk_create_texture( &entry->image, width, height, 1, TEXFMT_RGBA8 );
	if ( !entry->image.texnum ) {
		Com_Memset( entry, 0, sizeof( *entry ) );
		return -1;
	}
	vk_upload_texture( &entry->image, 0, 0, 0, width, height, rgba );

	// The low byte is the slot, the rest a serial, so a stale handle never
	// matches a new texture in the same slot soon.
	moharenaSerial = ( moharenaSerial + 1 ) & 0xFFFFFF;
	if ( !moharenaSerial ) {
		moharenaSerial = 1;
	}

	entry->handle = ( moharenaSerial << 8 ) | (unsigned int)( index + 1 );

	*outHandle = entry->handle;
	return 0;
}

static int MoHArena_UpdateTexture(
	unsigned int handle, int x, int y, int width, int height, const unsigned char *rgba, int rowPitchBytes
) {
	moharenaTexture_t	*entry;
	byte				*packed;
	int					row;

	entry = MoHArena_FindTexture( handle );
	if ( !MoHArena_IsReady() || !entry || !rgba || x < 0 || y < 0 || width <= 0 || height <= 0 ) {
		return -1;
	}

	if ( x > entry->image.width || width > entry->image.width - x || y > entry->image.height
		|| height > entry->image.height - y ) {
		return -1;
	}

	if ( rowPitchBytes % 4 || rowPitchBytes / 4 < width ) {
		return -1;
	}

	if ( rowPitchBytes == width * 4 ) {
		vk_upload_texture( &entry->image, 0, x, y, width, height, rgba );
		return 0;
	}

	// The backend takes tightly packed rows: one upload of the rows packed,
	// not one per row.
	packed = MoHArena_Scratch( (size_t)width * height * 4 );
	if ( !packed ) {
		return -1;
	}

	for ( row = 0; row < height; row++ ) {
		Com_Memcpy( packed + (size_t)row * width * 4, rgba + (size_t)row * rowPitchBytes, (size_t)width * 4 );
	}

	vk_upload_texture( &entry->image, 0, x, y, width, height, packed );
	return 0;
}

static int MoHArena_DestroyTexture( unsigned int handle ) {
	moharenaTexture_t *entry;

	entry = MoHArena_FindTexture( handle );
	if ( !entry ) {
		return -1;
	}

	MoHArena_DeleteTexture( entry );
	return 0;
}

static void MoHArena_DestroyAllTextures( void ) {
	int index;

	for ( index = 0; index < MOHARENA_RENDER_MAX_TEXTURES; index++ ) {
		if ( moharenaTextures[index].image.texnum ) {
			MoHArena_DeleteTexture( &moharenaTextures[index] );
		}
	}

	MoHArena_FreeScratch();
}

static void MoHArena_Begin2D( void ) {
	if ( !MoHArena_IsReady() ) {
		return;
	}

	// exports: run at once, or queued for the render thread
	Set2DWindow( 0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1 );
	RE_Scissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	moharenaIn2D = qtrue;
	moharenaDrew = qfalse;
}

static int MoHArena_DrawTriangles(
	const moharenaRenderVertex_t	*vertices,
	int								numVertices,
	const unsigned int				*indices,
	int								numIndices,
	unsigned int					texture,
	int								clipX,
	int								clipY,
	int								clipWidth,
	int								clipHeight
) {
	const moharenaTexture_t			*entry;
	const moharenaRenderVertex_t	*vertex;
	drawGeometry_t					geometry;
	float							*xyz;
	float							*st;
	byte							*colors;
	byte							*scratch;
	int								left, top, right, bottom;
	int								i;

	if ( !moharenaIn2D || !MoHArena_IsReady() || !vertices || !indices || numVertices <= 0 || numIndices <= 0 ) {
		return -1;
	}

	if ( numIndices % 3 || clipWidth < 0 || clipHeight < 0 ) {
		return -1;
	}

	entry = NULL;
	if ( texture ) {
		entry = MoHArena_FindTexture( texture );
		if ( !entry ) {
			return -1;
		}
	}

	for ( i = 0; i < numIndices; i++ ) {
		if ( indices[i] >= (unsigned int)numVertices ) {
			return -1;
		}
	}

	// The clip rectangle, inside the screen; the backend's scissor counts
	// from the bottom.
	left = clipX > 0 ? clipX : 0;
	top = clipY > 0 ? clipY : 0;
	right = clipX > glConfig.vidWidth - clipWidth ? glConfig.vidWidth : clipX + clipWidth;
	bottom = clipY > glConfig.vidHeight - clipHeight ? glConfig.vidHeight : clipY + clipHeight;
	if ( right <= left || bottom <= top ) {
		return 0;
	}

	// The backend reads three floats of position, two of texture
	// coordinates and four color bytes per vertex, each in an array of its
	// own.
	scratch = MoHArena_Scratch( (size_t)numVertices * ( sizeof( float ) * 5 + 4 ) );
	if ( !scratch ) {
		return -1;
	}
	xyz = (float *)scratch;
	st = xyz + (size_t)numVertices * 3;
	colors = (byte *)( st + (size_t)numVertices * 2 );

	for ( i = 0; i < numVertices; i++ ) {
		vertex = &vertices[i];
		xyz[i * 3 + 0] = vertex->x;
		xyz[i * 3 + 1] = vertex->y;
		xyz[i * 3 + 2] = 0;
		st[i * 2 + 0] = vertex->u;
		st[i * 2 + 1] = vertex->v;
		colors[i * 4 + 0] = (byte)( vertex->rgba[0] * tr.identityLightByte / 255 );
		colors[i * 4 + 1] = (byte)( vertex->rgba[1] * tr.identityLightByte / 255 );
		colors[i * 4 + 2] = (byte)( vertex->rgba[2] * tr.identityLightByte / 255 );
		colors[i * 4 + 3] = vertex->rgba[3];
	}

	// draws what is queued (the 2D window of Begin2D among it) and the open
	// 2D batch first; with the render thread, waits for it
	R_IssuePendingRenderCommands();
	R_SelectTexture( 0 );
	R_TexEnv( TEXENV_MODULATE );
	R_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	R_EnableTexture( qtrue );
	moharenaDrew = qtrue;
	RB_SetScissor( left, glConfig.vidHeight - bottom, right - left, bottom - top );

	Com_Memset( &geometry, 0, sizeof( geometry ) );
	geometry.numVertexes = numVertices;
	geometry.xyz = xyz;
	geometry.xyzStride = sizeof( float ) * 3;
	geometry.colors = colors;
	if ( entry ) {
		R_SelectTexture( 0 );
		R_Bind( (image_t *)&entry->image );
		geometry.st[0] = st;
		geometry.stStride[0] = sizeof( float ) * 2;
	}

	vk_draw( PRIM_TRIANGLES, &geometry, numIndices, (const glIndex_t *)indices );
	return 0;
}

static void MoHArena_End2D( void ) {
	if ( !moharenaIn2D ) {
		return;
	}

	moharenaIn2D = qfalse;
	if ( !MoHArena_IsReady() ) {
		return;
	}

	RE_Scissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	if ( !moharenaDrew ) {
		return;
	}
	moharenaDrew = qfalse;

	// no hand-over since the last draw's wait: the front end still owns the
	// back end's state
	R_SelectTexture( 0 );
	R_EnableTexture( qtrue );
	RB_SetColor4ubv( backEnd.color2D );
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

const moharenaRenderApi_t *GetMoHArenaRenderAPI( int version ) {
	if ( version != MOHARENA_RENDER_API_VERSION ) {
		return NULL;
	}

	return &moharenaRenderApi;
}
