/*
===========================================================================
Copyright (C) 2026 Project: Omaha

This file is part of Project: Omaha source code.

Project: Omaha builds upon OpenMoHAA / ioquake3 / F.A.K.K. foundations.
Project: Omaha source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Project: Omaha source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Project: Omaha source code; if not, see COPYING.txt in the
source tree, or write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "cg_hitmarker.h"

#include "cg_local.h"

#include <math.h>

/* Display window after a hit the server reported. */
#define HITMARKER_DURATION_MS  250
#define HITMARKER_SOUND_VOLUME 2.0f /* Same as retail dm_hit_notify. */
#define HITMARKER_OPACITY      0.5f /* Peak alpha; fade still runs to 0. */
#define HITMARKER_INNER_PX     6.0f
#define HITMARKER_OUTER_PX     14.0f
#define HITMARKER_THICKNESS    2.0f

static cvar_t *cg_hitmarker;
static cvar_t *cg_hitmarker_sound;

static int s_hitTime;

void CG_Hitmarker_RegisterCvars(void)
{
	const int flags = CVAR_ARCHIVE;

	// Changed in MoH Arena: hitmarkers are off until the player turns them on
	cg_hitmarker = cgi.Cvar_Get("cg_hitmarker", "0", flags); /* Changed in Omaha */
	cg_hitmarker_sound = cgi.Cvar_Get("cg_hitmarker_sound", "classic", flags); /* Changed in Omaha */
}

qboolean CG_Hitmarker_Enabled(void)
{
	return (cg_hitmarker && cg_hitmarker->integer) ? qtrue : qfalse;
}

qboolean CG_Hitmarker_SuppressRetailNotify(void)
{
	return CG_Hitmarker_Enabled();
}

/* Changed in Omaha: CHAN_LOCAL like retail dm_hit_notify — S_StartLocalSound uses CHAN_MENU and can spatialize to silence. */
static void CG_Hitmarker_PlaySound(void)
{
	char        path[MAX_QPATH];
	char        name[MAX_QPATH];
	sfxHandle_t handle;
	int         entNum;
	size_t      len;

	if (cg_hitmarker_sound && cg_hitmarker_sound->string[0]) {
		Q_strncpyz(name, cg_hitmarker_sound->string, sizeof(name));
	} else {
		Q_strncpyz(name, "hitmarker", sizeof(name));
	}
	/* Basename only — reject path traversal from the archived cvar. */
	if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
		Q_strncpyz(name, "hitmarker", sizeof(name));
	}
	len = strlen(name);
	if (len > 4 && !Q_stricmp(name + len - 4, ".wav")) {
		name[len - 4] = '\0';
	}
	if (!name[0]) {
		Q_strncpyz(name, "hitmarker", sizeof(name));
	}
	/* Changed in Omaha: cyclic "None" disables hitmarker sound. */
	if (!Q_stricmp(name, "none")) {
		return;
	}

	Com_sprintf(path, sizeof(path), "sound/prom/hitmarkers/%s.wav", name);
	handle = cgi.S_RegisterSound(path, qfalse);
	if (!handle) {
		handle = cgi.S_RegisterSound("sound/prom/hitmarkers/hitmarker.wav", qfalse);
	}
	/* clientNum + CHAN_LOCAL: always reclaimable; ENTITYNUM_NONE can fail PickChannel when 2D is busy. */
	entNum = cg.snap ? cg.snap->ps.clientNum : 0;
	cgi.S_StartSound(NULL, entNum, CHAN_LOCAL, handle, HITMARKER_SOUND_VOLUME, -1.0f, 1.0f, -1.0f, qfalse);
}

void CG_Hitmarker_Trigger(void)
{
	if (!CG_Hitmarker_Enabled()) {
		return;
	}

	s_hitTime = cg.time;
	CG_Hitmarker_PlaySound();
}

void CG_Hitmarker_OnNotify(qboolean isKill)
{
	(void)isKill;

	if (!CG_Hitmarker_Enabled()) {
		return;
	}

	CG_Hitmarker_Trigger();
}

static void CG_Hitmarker_DrawSegment(
	float cx, float cy, float ux, float uy, float inner, float outer, float thickness, float alpha
)
{
	float  len;
	int    steps;
	int    i;
	vec4_t color;

	len = outer - inner;
	if (len <= 0.0f || thickness <= 0.0f || alpha <= 0.0f) {
		return;
	}

	steps = (int)(len + 0.5f);
	if (steps < 1) {
		steps = 1;
	}

	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = alpha;

	cgi.R_SetColor(color);
	for (i = 0; i <= steps; i++) {
		float t = inner + (len * (float)i) / (float)steps;
		float x = cx + ux * t - thickness * 0.5f;
		float y = cy + uy * t - thickness * 0.5f;

		cgi.R_DrawBox(x, y, thickness, thickness);
	}
	cgi.R_SetColor(NULL);
}

void CG_DrawHitmarker(void)
{
	float cx, cy;
	float scale;
	float inner, outer, thickness;
	float alpha;
	float invSqrt2;
	int   age;

	if (!CG_Hitmarker_Enabled()) {
		return;
	}

	if (!cg_hud || !cg_hud->integer) {
		return;
	}

	if (!cg.snap) {
		return;
	}

	if ((cg.snap->ps.pm_flags & PMF_NO_HUD) || (cg.snap->ps.pm_flags & PMF_INTERMISSION)) {
		return;
	}

	if (!s_hitTime) {
		return;
	}

	age = cg.time - s_hitTime;
	if (age < 0 || age >= HITMARKER_DURATION_MS) {
		s_hitTime = 0;
		return;
	}

	alpha = HITMARKER_OPACITY * (1.0f - ((float)age / (float)HITMARKER_DURATION_MS));
	if (alpha <= 0.0f) {
		return;
	}

	scale = cgs.uiHiResScale[1];
	if (scale <= 0.0f) {
		scale = 1.0f;
	}

	inner = HITMARKER_INNER_PX * scale;
	outer = HITMARKER_OUTER_PX * scale;
	thickness = HITMARKER_THICKNESS * scale;
	if (thickness < 1.0f) {
		thickness = 1.0f;
	}

	// Changed in MoH Arena: always in the middle, where the crosshair is drawn
	cx = floorf(cgs.glconfig.vidWidth * 0.5f);
	cy = floorf(cgs.glconfig.vidHeight * 0.5f);

	invSqrt2 = 0.70710678f;
	CG_Hitmarker_DrawSegment(cx, cy, invSqrt2, invSqrt2, inner, outer, thickness, alpha);
	CG_Hitmarker_DrawSegment(cx, cy, invSqrt2, -invSqrt2, inner, outer, thickness, alpha);
	CG_Hitmarker_DrawSegment(cx, cy, -invSqrt2, invSqrt2, inner, outer, thickness, alpha);
	CG_Hitmarker_DrawSegment(cx, cy, -invSqrt2, -invSqrt2, inner, outer, thickness, alpha);
}
