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

// DESCRIPTION:
// Client-only first-person chase spectate (eye + lean + zoom).
// Changed in MoH Arena: with the followed player's weapon and hands (CG_SpectateFP_ArmsModel).

#include "cg_local.h"
// Added in MoH Arena: a weapon model's init commands (dtikicmd_t), which hold its name and its class
#include "../corepp/tiki.h"

/* Matches bg_local.h PELVIS_TAG — lean roll lives here on entityState. */
#define SPECTATEFP_PELVIS_TAG 3
#define SPECTATEFP_ZOOM_FOV   45.0f
#define SPECTATEFP_LEAN_MAX   45.0f
// Added in MoH Arena: the numbers of the game's own first-person view (CG_OffsetFirstPersonView)
#define SPECTATEFP_LEAN_PIVOT    28.7f
#define SPECTATEFP_HEIGHT_RATE   12.5f
#define SPECTATEFP_HEIGHT_MAXGAP 32.0f
// Added in MoH Arena: the bone controllers of a player (bg_local.h), set for the hands of the followed player
#define SPECTATEFP_HEAD_TAG  0
#define SPECTATEFP_TORSO_TAG 1
#define SPECTATEFP_ARMS_TAG  2
#define SPECTATEFP_MOUTH_TAG 4
// Added in MoH Arena: the hands' action count starts far above the server's own (0 to 3), so the first frame
//  of a followed player always differs from what the own view left behind
#define SPECTATEFP_ANIM_COUNT_BASE 1000
// Added in MoH Arena: how far below the feet the ground is looked for, and the speed above which a move
//  between two snapshots is a teleport and not a movement
#define SPECTATEFP_GROUND_REACH 4.0f
#define SPECTATEFP_MAX_SPEED    1000.0f
// Added in MoH Arena: how long the hands may hold an action after the weapon is back at idle, and how long
//  after an action has run out with no word from the weapon (milliseconds)
#define SPECTATEFP_IDLE_WAIT 2000
#define SPECTATEFP_OVER_WAIT 400

cvar_t *cg_spectate_firstperson;

static int s_fpLastClient = -1;
// Added in MoH Arena: the eased height of the eye, as cg.fCurrentViewHeight is for the own view (0: not set)
static float s_fpViewZ = 0.0f;

// Added in MoH Arena: the server's switch, its serverinfo key "moharena_firstperson"
static qboolean s_fpServerWanted = qfalse;

// Added in MoH Arena: the weapon in the followed player's right hand and what the hands do with it
//  (CG_SpectateFP_UpdateWeapon)
typedef struct {
	qboolean valid;       // a weapon is in the right hand in this frame
	int      client;      // whose record this is (-1: nobody's)
	int      entNum;
	int      modelindex;
	int      weaponClass; // as STAT_EQUIPPED_WEAPON
	char     name[64];    // as the name in CS_WEAPONS
	int      anim;        // VM_ANIM_*
	int      animCount;   // goes up for every new action
	int      lastSlot;
	int      lastIndex;
	qboolean idlePending; // the weapon is back at idle, the hands follow once their action is over
	int      idleTime;
	int      overTime;
} spectateFpWeapon_t;

static spectateFpWeapon_t s_fpWeapon = {
	qfalse, -1, -1, 0, 0, "", VM_ANIM_IDLE, SPECTATEFP_ANIM_COUNT_BASE, -1, -1, qfalse, 0, 0
};
static int      s_fpArmsFrame = -1;         // the frame the hands were last drawn in
static qboolean s_fpArmsDrawn[MAX_CLIENTS]; // who was drawn as hands the last time
// Added in MoH Arena: the followed player's movement, for the bob of the eye and the sway of the hands
static vec3_t   s_fpVelocity;
static qboolean s_fpWalking;
static qboolean s_fpCrouching;
static float    s_fpBobPhase;
static float    s_fpBobAmp;

static void CG_SpectateFP_UpdateWeapon(int followClient, const centity_t *followCent, qboolean wasValid);
static void CG_SpectateFP_Motion(const centity_t *cent, const vec3_t base, const vec3_t mins, const vec3_t maxs);

void CG_SpectateFP_RegisterCvars(void)
{
	/* Added in Omaha */
	cg_spectate_firstperson = cgi.Cvar_Get("cg_spectate_firstperson", "0", CVAR_ARCHIVE);
}

// Added in MoH Arena: the key command that turns the first-person view on and off
void CG_SpectateFP_Toggle_f(void)
{
	if (!cg_spectate_firstperson) {
		return;
	}

	cgi.Cvar_Set("cg_spectate_firstperson", cg_spectate_firstperson->integer ? "0" : "1");
}

// Added in MoH Arena: the server's switch. While its serverinfo key "moharena_firstperson" is exactly "1",
//  everyone who follows a player as a spectator sees through that player's eyes, in both UIs. Any other
//  value, and a serverinfo without the key, leave the choice to the modern UI's cvar.
void CG_SpectateFP_ParseServerinfo(const char *info)
{
	const char *value = Info_ValueForKey(info, "moharena_firstperson");

	s_fpServerWanted = (value[0] == '1' && !value[1]) ? qtrue : qfalse;
}

qboolean CG_SpectateFP_Wanted(void)
{
	// Changed in MoH Arena: the server's switch comes first, the cvar belongs to the modern UI
	if (s_fpServerWanted) {
		return qtrue;
	}

	return (CG_ModernUI() && cg_spectate_firstperson && cg_spectate_firstperson->integer) ? qtrue : qfalse;
}

qboolean CG_SpectateFP_Active(void)
{
	return cg.spectateFp.active;
}

int CG_SpectateFP_FollowClient(void)
{
	return cg.spectateFp.active ? cg.spectateFp.clientNum : -1;
}

qboolean CG_SpectateFP_InZoom(void)
{
	return (cg.spectateFp.active && cg.spectateFp.inZoom) ? qtrue : qfalse;
}

int CG_SpectateFP_ZoomFov(void)
{
	return cg.spectateFp.active ? cg.spectateFp.zoomFov : 0;
}

/*
===============
CG_SpectateFP_ZoomType

Added in MoH Arena: which scope CG_DrawZoomOverlay draws for the player followed. That function reads
the weapon of the spectator's own player state, which holds none, so every followed player got the
Allied scope. A spectator is sent no other player's weapon name; what that player holds is the entity
attached to them, and its model tells the scoped KAR98 (1) and the binoculars (3) from every other
rifle (0). Without such an entity the followed player's team decides: 1 for the Axis.
===============
*/
int CG_SpectateFP_ZoomType(void)
{
	const entityState_t *es;
	const char          *model;
	int                  i;

	if (!cg.spectateFp.active || !cg.snap) {
		return 0;
	}

	for (i = 0; i < cg.snap->numEntities; i++) {
		es = &cg.snap->entities[i];
		if (es->parent != cg.spectateFp.clientNum || es->modelindex <= 0 || es->modelindex >= MAX_MODELS) {
			continue;
		}
		if (es->renderfx & RF_DONTDRAW) {
			continue;
		}

		model = CG_ConfigString(CS_MODELS + es->modelindex);
		if (Q_stristr(model, "kar98sniper")) {
			return 1;
		}
		if (Q_stristr(model, "binocular")) {
			return 3;
		}
		if (Q_stristr(model, "models/weapons/")) {
			return 0;
		}
	}

	return (cg_entities[cg.spectateFp.clientNum].currentState.eFlags & EF_AXIS) ? 1 : 0;
}

float CG_SpectateFP_LeanAngle(void)
{
	return cg.spectateFp.active ? cg.spectateFp.leanAngle : 0.0f;
}

static float CG_SpectateFP_LeanFromBones(const entityState_t *es)
{
	float lean;

	if (!es) {
		return 0.0f;
	}

	/* PmoveAdjustAngleSettings: pelvis roll = fLeanAngle * 0.8 */
	lean = es->bone_angles[SPECTATEFP_PELVIS_TAG][ROLL] / 0.8f;
	if (lean > SPECTATEFP_LEAN_MAX) {
		lean = SPECTATEFP_LEAN_MAX;
	} else if (lean < -SPECTATEFP_LEAN_MAX) {
		lean = -SPECTATEFP_LEAN_MAX;
	}
	return lean;
}

/*
===============
CG_SpectateFP_LeanTarget

Fixed in Omaha: interpolate lean between current/next entity states (same idea as
ps.fLeanAngle lerp).
===============
*/
static float CG_SpectateFP_LeanTarget(centity_t *cent)
{
	float lean0;
	float lean1;
	float f;

	lean0 = CG_SpectateFP_LeanFromBones(&cent->currentState);
	if (!cent->interpolate || !cg.nextSnap) {
		return lean0;
	}

	lean1 = CG_SpectateFP_LeanFromBones(&cent->nextState);
	f     = cg.frameInterpolation;
	if (f < 0.0f) {
		f = 0.0f;
	} else if (f > 1.0f) {
		f = 1.0f;
	}
	return lean0 + (lean1 - lean0) * f;
}

void CG_SpectateFP_Update(void)
{
	int        followClient;
	centity_t *followCent;
	float      fov;
	qboolean   wasValid = s_fpWeapon.valid;

	// Added in MoH Arena: the followed player's weapon is looked for again on every frame
	s_fpWeapon.valid = qfalse;

	cg.spectateFp.active    = qfalse;
	cg.spectateFp.clientNum = -1;
	cg.spectateFp.leanAngle = 0.0f;
	cg.spectateFp.inZoom    = qfalse;
	cg.spectateFp.zoomFov   = 0;

	if (!CG_SpectateFP_Wanted() || !cg.snap) {
		s_fpLastClient = -1;
		s_fpViewZ      = 0.0f;
		return;
	}

	// Added in MoH Arena: only a spectator follows a player. A map's scripted camera sets the camera view
	//  flag for players too, and their STAT_INFOCLIENT names the teammate under the crosshair.
	if (!(cg.snap->ps.pm_flags & PMF_SPECTATING) || !(cg.snap->ps.pm_flags & PMF_CAMERA_VIEW)) {
		s_fpLastClient = -1;
		s_fpViewZ      = 0.0f;
		return;
	}

	followClient = cg.snap->ps.stats[STAT_INFOCLIENT];
	if (followClient < 0 || followClient >= MAX_CLIENTS) {
		s_fpLastClient = -1;
		s_fpViewZ      = 0.0f;
		return;
	}

	followCent = &cg_entities[followClient];
	if (!followCent->currentValid || !followCent->currentState.modelindex) {
		s_fpLastClient = -1;
		s_fpViewZ      = 0.0f;
		return;
	}

	if (followClient != s_fpLastClient) {
		s_fpLastClient    = followClient;
		s_fpViewZ         = 0.0f;
		s_fpWeapon.client = -1;
		s_fpBobPhase      = 0.0f;
		s_fpBobAmp        = 0.0f;
	}

	cg.spectateFp.active    = qtrue;
	cg.spectateFp.clientNum = followClient;

	// Changed in MoH Arena: the lean of the two snapshots, blended, with no easing on top of it. The own
	//  view takes its lean as it is, so an eased one was behind what the followed player saw.
	cg.spectateFp.leanAngle = CG_SpectateFP_LeanTarget(followCent);

	fov = cg.snap->ps.fov;
	if (fov <= 0.0f) {
		fov = cg.camera_fov;
	}
	cg.spectateFp.inZoom  = (fov > 0.0f && fov <= SPECTATEFP_ZOOM_FOV) ? qtrue : qfalse;
	cg.spectateFp.zoomFov = cg.spectateFp.inZoom ? (int)(fov + 0.5f) : 0;
	if (cg.spectateFp.zoomFov < 1) {
		cg.spectateFp.zoomFov = 1;
	}

	// Added in MoH Arena: what the followed player holds and does with it, for the weapon and the hands
	CG_SpectateFP_UpdateWeapon(followClient, followCent, wasValid);
}

/*
===============
CG_SpectateFP_SnapOrigin

Added in MoH Arena: where the server put the followed player, blended between the two snapshots.
The place another player is drawn at (lerpOrigin) runs ahead of that by the player's speed, for up
to cg_smoothClientsTime (cg_smoothClients). For a player who runs into a wall it is behind the wall
until the server's speed is the stopped one, and an eye taken from there looked through the wall.
===============
*/
static void CG_SpectateFP_SnapOrigin(const centity_t *cent, vec3_t out)
{
	float f;
	int   i;

	if (!cent->interpolate || !cg.nextSnap) {
		VectorCopy(cent->currentState.origin, out);
		return;
	}

	f = cg.frameInterpolation;
	if (f < 0.0f) {
		f = 0.0f;
	} else if (f > 1.0f) {
		f = 1.0f;
	}

	for (i = 0; i < 3; i++) {
		out[i] = cent->currentState.origin[i] + f * (cent->nextState.origin[i] - cent->currentState.origin[i]);
	}
}

/*
===============
CG_SpectateFP_CalcEye

Changed in MoH Arena: the eye is worked out as CG_OffsetFirstPersonView does it for the own view, so a
spectator sees what the followed player sees: the eye turns about a point below it with the pitch, a
lean swings it on an arc around the looking direction, the roll is the own view's, the height is eased
and the two box traces keep it out of walls. Before, a lean moved the eye straight sideways by 0.35 units
for each degree (14 at a full lean, where the own view is 18.4 out and 6.7 lower) with a smaller roll.
The bob of a walking player is the own view's too, as strong as the followed player's speed makes it.
Its step is counted here: only the player's own client knows where in a step they are.
===============
*/
qboolean CG_SpectateFP_CalcEye(vec3_t outOrigin, vec3_t outAngles)
{
	centity_t *cent;
	trace_t    trace;
	vec3_t     base;
	vec3_t     pivotAngles;
	vec3_t     forward, left;
	vec3_t     start, end, delta;
	vec3_t     mins, maxs;
	float      viewHeight;
	float      targetZ, gap, step;
	float      lean;
	float      frac, speed, bob;

	if (!CG_SpectateFP_Active()) {
		return qfalse;
	}

	cent = &cg_entities[cg.spectateFp.clientNum];
	VectorCopy(cg.camera_angles, outAngles);

	/*
	 * Fixed in Omaha: stock chase pads pitch with g_spectatefollow_pitch * trace
	 * fraction (default +2). Undo the full bias — best we can do client-side
	 * without the server fraction — so FP looks with the followed view, not
	 * the over-the-shoulder chase tilt.
	 * Changed in Omaha: hard-coded bias (do not create a console cvar).
	 */
#define CG_SPECFP_PITCH_BIAS 2.0f
	// Changed in MoH Arena: the pitch as -180 to 180. It comes off the wire as 0 to 360, and its sign
	//  picks the pivot below.
	outAngles[PITCH] = AngleNormalize180(outAngles[PITCH]) - CG_SPECFP_PITCH_BIAS;

	/*
	 * Fixed in Omaha: use origin + stand/crouch viewheight instead of eyes bone.
	 * A bare ForceUpdatePose on the followed world model often left the bone at
	 * feet (camera in the floor). Match retail chase: origin + viewheight.
	 */
	IntegerToBoundingBox(cent->currentState.solid, mins, maxs);
	if (maxs[2] > 0.0f && maxs[2] < 60.0f) {
		viewHeight = (float)CROUCH_VIEWHEIGHT;
	} else {
		viewHeight = (float)DEFAULT_VIEWHEIGHT;
	}

	// Changed in MoH Arena: the server's place of the player, not the drawn one (CG_SpectateFP_SnapOrigin)
	CG_SpectateFP_SnapOrigin(cent, base);
	// Added in MoH Arena: the player's speed, and whether they stand on the ground and crouch, for the
	//  height and the bob below and for the sway of the hands
	CG_SpectateFP_Motion(cent, base, mins, maxs);

	// Added in MoH Arena: the height of the eye is eased as the own view's is (cg.fCurrentViewHeight), so
	//  crouching, standing up and steps move it as the followed player sees them and not in one jump.
	//  The own view doubles the rate off the ground, which CG_SpectateFP_Motion looks for with a trace.
	targetZ = base[2] + viewHeight;
	gap     = targetZ - s_fpViewZ;
	if (s_fpViewZ == 0.0f || !cent->interpolate || fabs(gap) < 0.1f) {
		s_fpViewZ = targetZ;
	} else {
		if (gap > SPECTATEFP_HEIGHT_MAXGAP) {
			gap       = SPECTATEFP_HEIGHT_MAXGAP;
			s_fpViewZ = targetZ - SPECTATEFP_HEIGHT_MAXGAP;
		} else if (gap < -SPECTATEFP_HEIGHT_MAXGAP) {
			gap       = -SPECTATEFP_HEIGHT_MAXGAP;
			s_fpViewZ = targetZ + SPECTATEFP_HEIGHT_MAXGAP;
		}

		step = (float)cg.frametime / 1000.0f * gap * SPECTATEFP_HEIGHT_RATE;
		if (!s_fpWalking) {
			step += step;
		}
		if (fabs(gap) < fabs(step)) {
			step = gap;
		}
		s_fpViewZ += step;
	}

	outOrigin[0] = base[0];
	outOrigin[1] = base[1];
	outOrigin[2] = s_fpViewZ;

	// Added in MoH Arena: the eye turns with the pitch about a point below it, 0.4 of the eye's height
	//  down for a look downwards and 0.2 for a look upwards, as in the own view
	pivotAngles[0] = outAngles[PITCH];
	pivotAngles[1] = outAngles[YAW];
	pivotAngles[2] = 0.0f;
	AngleVectorsLeft(pivotAngles, forward, left, NULL);

	VectorCopy(outOrigin, start);
	if (outAngles[PITCH] > 0.0f) {
		start[2] -= (s_fpViewZ - base[2]) * 0.4f;
	} else {
		start[2] -= (s_fpViewZ - base[2]) * 0.2f;
	}
	VectorSubtract(outOrigin, start, delta);
	RotatePointAroundVector(end, left, delta, outAngles[PITCH] * 0.4f);
	VectorAdd(start, end, outOrigin);

	// Changed in MoH Arena: a lean swings the eye on an arc of 28.7 units around the looking direction and
	//  rolls the view by 0.3 of the lean (0.4 in Allied Assault), the numbers of the own view
	lean = cg.spectateFp.leanAngle;
	if (lean != 0.0f) {
		VectorCopy(outOrigin, start);
		start[2] -= SPECTATEFP_LEAN_PIVOT;
		VectorSubtract(outOrigin, start, delta);
		RotatePointAroundVector(end, forward, delta, lean);
		VectorAdd(start, end, outOrigin);

		outAngles[ROLL] += lean * (cg_target_game >= TG_MOHTA ? 0.3f : 0.4f);
	}

	// Added in MoH Arena: the bob of the own view while the followed player walks, with its numbers. Its
	//  strength follows the player's speed; the step is this client's own count (see above).
	frac = (float)cg.frametime / 1000.0f;
	if (s_fpWalking) {
		speed         = VectorLength(s_fpVelocity);
		s_fpBobPhase += (frac + frac) * M_PI * (speed * 0.0015f + 0.9f);
		if (s_fpBobPhase > M_PI * 2.0f) {
			s_fpBobPhase -= M_PI * 2.0f;
		}

		s_fpBobAmp = s_fpBobAmp ? speed : speed * 0.5f;
		if (lean != 0.0f) {
			s_fpBobAmp *= 0.75f;
		}
		s_fpBobAmp *= (1.0f - fabs(outAngles[PITCH]) * (1.0f / 90.0f) * 0.5f) * 0.5f;
	} else if (s_fpBobAmp > 0.0f) {
		s_fpBobAmp -= (frac + frac) * s_fpBobAmp;
		if (s_fpBobAmp < 0.1f) {
			s_fpBobAmp = 0.0f;
		}
	}

	if (s_fpBobAmp > 0.0f) {
		bob = sin(s_fpBobPhase) * s_fpBobAmp * 0.03f;
		if (bob > 16.0f) {
			bob = 16.0f;
		} else if (bob < -16.0f) {
			bob = -16.0f;
		}
		VectorMA(outOrigin, bob, left, outOrigin);

		bob = (fabs(sin(s_fpBobPhase - 0.94f)) - 0.5f) * s_fpBobAmp * 0.06f;
		if (bob > 16.0f) {
			bob = 16.0f;
		} else if (bob < -16.0f) {
			bob = -16.0f;
		}
		outOrigin[2] += bob;
	}

	/*
	 * Fixed in Omaha: the lean offset went straight through walls. Sweep the
	 * same 6-unit eye box as CG_OffsetFirstPersonView so the renderer near
	 * plane stays clear of geometry.
	 */
	// Changed in MoH Arena: both of the own view's traces, first up or down from the plain eye and then
	//  sideways at that height, on every frame and not for a lean alone
	VectorSet(mins, -6, -6, -6);
	VectorSet(maxs, 6, 6, 6);

	VectorSet(start, base[0], base[1], targetZ);
	VectorSet(end, base[0], base[1], outOrigin[2]);
	CG_Trace(
		&trace, start, mins, maxs, end, cg.spectateFp.clientNum, MASK_PLAYERSOLID, qfalse, qtrue, "SpectateFP Height"
	);

	VectorCopy(trace.endpos, start);
	VectorSet(end, outOrigin[0], outOrigin[1], trace.endpos[2]);
	CG_Trace(
		&trace, start, mins, maxs, end, cg.spectateFp.clientNum, MASK_PLAYERSOLID, qfalse, qtrue, "SpectateFP Lateral"
	);
	VectorCopy(trace.endpos, outOrigin);

	return qtrue;
}

/*
===============
CG_SpectateFP_Motion

Added in MoH Arena: how the followed player moves, for the bob of the eye and the sway of the hands. The
own view has the predicted player state for it; of another player there are only the two snapshots, so
the speed is the way between them and the ground is looked for with the player's box.
===============
*/
static void CG_SpectateFP_Motion(const centity_t *cent, const vec3_t base, const vec3_t mins, const vec3_t maxs)
{
	trace_t trace;
	vec3_t  end;
	float   dt;

	VectorClear(s_fpVelocity);
	if (cent->interpolate && cg.nextSnap) {
		dt = (float)(cg.nextSnap->serverTime - cg.snap->serverTime) / 1000.0f;
		if (dt > 0.0f) {
			VectorSubtract(cent->nextState.origin, cent->currentState.origin, s_fpVelocity);
			VectorScale(s_fpVelocity, 1.0f / dt, s_fpVelocity);
			if (VectorLength(s_fpVelocity) > SPECTATEFP_MAX_SPEED) {
				VectorClear(s_fpVelocity);
			}
		}
	}

	VectorCopy(base, end);
	end[2] -= SPECTATEFP_GROUND_REACH;
	CG_Trace(
		&trace, base, mins, maxs, end, cg.spectateFp.clientNum, MASK_PLAYERSOLID, qfalse, qtrue, "SpectateFP Ground"
	);

	s_fpWalking = (trace.startsolid || trace.allsolid || (trace.fraction < 1.0f && trace.plane.normal[2] >= 0.7f))
					? qtrue
					: qfalse;
	s_fpCrouching = (maxs[2] > 0.0f && maxs[2] < 60.0f) ? qtrue : qfalse;
}

/*
===============
CG_SpectateFP_ClassForName

Added in MoH Arena: a weapon model's class word as the bits the server puts into STAT_EQUIPPED_WEAPON
(G_WeaponClassNameToNum), which pick the hands' animations
===============
*/
static int CG_SpectateFP_ClassForName(const char *name)
{
	static const struct {
		const char *name;
		int         bits;
	} classes[] = {
		{"pistol",  WEAPON_CLASS_PISTOL },
		{"rifle",   WEAPON_CLASS_RIFLE  },
		{"smg",     WEAPON_CLASS_SMG    },
		{"mg",      WEAPON_CLASS_MG     },
		{"grenade", WEAPON_CLASS_GRENADE},
		{"heavy",   WEAPON_CLASS_HEAVY  },
		{"cannon",  WEAPON_CLASS_CANNON },
		{"item",    WEAPON_CLASS_ITEM   },
		{"item1",   WEAPON_CLASS_ITEM1  },
		{"item2",   WEAPON_CLASS_ITEM2  },
		{"item3",   WEAPON_CLASS_ITEM3  },
		{"item4",   WEAPON_CLASS_ITEM4  },
	};
	int i;

	for (i = 0; i < (int)ARRAY_LEN(classes); i++) {
		if (!Q_stricmp(name, classes[i].name)) {
			return classes[i].bits;
		}
	}

	return 0;
}

/*
===============
CG_SpectateFP_PlayerTiki

Added in MoH Arena: the model the followed player is drawn with, whose tag numbers the server uses for
what the player holds. A skin this client does not have falls back to the team's model, as it does when
the player is drawn (CG_ModelAnim).
===============
*/
static dtiki_t *CG_SpectateFP_PlayerTiki(const entityState_t *s)
{
	dtiki_t  *tiki = NULL;
	qhandle_t hModel;

	if (s->modelindex > 0 && s->modelindex < MAX_MODELS && cgs.model_draw[s->modelindex]) {
		tiki = cgi.R_Model_GetHandle(cgs.model_draw[s->modelindex]);
	}

	if (!tiki) {
		hModel = cgi.R_RegisterModel(va(
			"models/player/%s.tik",
			(s->eFlags & EF_AXIS) ? dm_playergermanmodel->resetString : dm_playermodel->resetString
		));
		if (hModel) {
			tiki = cgi.R_Model_GetHandle(hModel);
		}
	}

	return tiki;
}

/*
===============
CG_SpectateFP_FindWeapon

Added in MoH Arena: the entity in the followed player's right hand, or NULL. The server hangs the weapon
in use on the tag "tag_weapon_right" of the player's model; one that is put away is not drawn.
===============
*/
static const entityState_t *CG_SpectateFP_FindWeapon(int followClient, dtiki_t *playerTiki)
{
	const entityState_t *es;
	const char          *tagName;
	int                  i;

	for (i = 0; i < cg.snap->numEntities; i++) {
		es = &cg.snap->entities[i];
		if (es->parent != followClient || es->modelindex <= 0 || es->modelindex >= MAX_MODELS) {
			continue;
		}
		if (es->renderfx & RF_DONTDRAW) {
			continue;
		}

		tagName = cgi.Tag_NameForNum(playerTiki, es->tag_num & TAG_MASK);
		if (!tagName || Q_stricmp(tagName, "tag_weapon_right")) {
			continue;
		}
		if (!cgs.model_draw[es->modelindex] || !cgi.R_Model_GetHandle(cgs.model_draw[es->modelindex])) {
			continue;
		}

		return es;
	}

	return NULL;
}

/*
===============
CG_SpectateFP_WeaponIdentity

Added in MoH Arena: a weapon's name and class, which the own view has from its player state, read here
from the model's own init commands ("name" and "weapontype"). Every client loads them with the model.
===============
*/
static void CG_SpectateFP_WeaponIdentity(dtiki_t *weaponTiki)
{
	const dtikicmd_t *cmd;
	int               i;

	s_fpWeapon.name[0]     = 0;
	s_fpWeapon.weaponClass = 0;

	if (!weaponTiki || !weaponTiki->a || !weaponTiki->a->server_initcmds) {
		return;
	}

	for (i = 0; i < weaponTiki->a->num_server_initcmds; i++) {
		cmd = &weaponTiki->a->server_initcmds[i];
		if (cmd->num_args < 2 || !cmd->args || !cmd->args[0] || !cmd->args[1]) {
			continue;
		}

		if (!Q_stricmp(cmd->args[0], "name")) {
			Q_strncpyz(s_fpWeapon.name, cmd->args[1], sizeof(s_fpWeapon.name));
		} else if (!Q_stricmp(cmd->args[0], "weapontype")) {
			s_fpWeapon.weaponClass = CG_SpectateFP_ClassForName(cmd->args[1]);
		}
	}
}

/*
===============
CG_SpectateFP_WeaponAction

Added in MoH Arena: the action of the hands (VM_ANIM_*) that goes with the animation a weapon model
plays. The pairs are the ones of the game's state files, where a player's action sets both.
===============
*/
static int CG_SpectateFP_WeaponAction(dtiki_t *weaponTiki, int animIndex)
{
	const char *name;

	if (animIndex < 0) {
		return VM_ANIM_IDLE;
	}

	name = cgi.Anim_NameForNum(weaponTiki, animIndex);
	if (!name) {
		return VM_ANIM_IDLE;
	}

	if (!Q_stricmpn(name, "secondaryfire", 13)) {
		// the second way to throw a grenade is the plain throw for the hands
		return (s_fpWeapon.weaponClass & WEAPON_CLASS_GRENADE) ? VM_ANIM_FIRE : VM_ANIM_FIRE_SECONDARY;
	}
	if (!Q_stricmp(name, "charge") || !Q_stricmp(name, "secondarycharge")) {
		return VM_ANIM_CHARGE;
	}
	if (!Q_stricmpn(name, "fire", 4)) {
		return VM_ANIM_FIRE;
	}
	if (!Q_stricmp(name, "reload_single")) {
		return VM_ANIM_RELOAD_SINGLE;
	}
	if (!Q_stricmp(name, "reload_end")) {
		return VM_ANIM_RELOAD_END;
	}
	if (!Q_stricmpn(name, "reload", 6)) {
		return VM_ANIM_RELOAD;
	}
	if (!Q_stricmp(name, "rechamber")) {
		return VM_ANIM_RECHAMBER;
	}
	if (!Q_stricmp(name, "raise")) {
		return VM_ANIM_PULLOUT;
	}
	if (!Q_stricmp(name, "putaway")) {
		return VM_ANIM_PUTAWAY;
	}
	if (!Q_stricmp(name, "idle0")) {
		return VM_ANIM_IDLE_0;
	}
	if (!Q_stricmp(name, "idle1")) {
		return VM_ANIM_IDLE_1;
	}
	if (!Q_stricmp(name, "idle2")) {
		return VM_ANIM_IDLE_2;
	}

	return VM_ANIM_IDLE;
}

/*
===============
CG_SpectateFP_UpdateWeapon

Added in MoH Arena: what the followed player holds in the right hand and what the hands do with it, once
a frame. A player's own client reads both from its player state (the equipped weapon, iViewModelAnim and
its change count). A spectator is sent none of that for another player, so it is worked out from what
every client is sent: the weapon entity on the player's hand and the animation the server plays on it,
which changes with every shot, reload and bolt action (Weapon::SetWeaponAnim).

Not covered: putting a weapon away, for which the server plays no animation on the stock weapons (the
hands are gone until the next weapon is up), and a ladder, where no hands are drawn.
===============
*/
static void CG_SpectateFP_UpdateWeapon(int followClient, const centity_t *followCent, qboolean wasValid)
{
	const entityState_t *es;
	dtiki_t             *playerTiki;
	dtiki_t             *weaponTiki;
	qboolean             fresh, swapped, over;
	float                weight;
	int                  slot, index, action;
	int                  armsSlot, armsIndex;
	int                  i;

	playerTiki = CG_SpectateFP_PlayerTiki(&followCent->currentState);
	if (!playerTiki) {
		return;
	}

	es = CG_SpectateFP_FindWeapon(followClient, playerTiki);
	if (!es) {
		return;
	}
	weaponTiki = cgi.R_Model_GetHandle(cgs.model_draw[es->modelindex]);

	// the animation the weapon plays: the server keeps one slot with a weight and moves on to the next
	//  slot for every new one
	slot   = -1;
	index  = -1;
	weight = 0.0f;
	for (i = 0; i < MAX_FRAMEINFOS; i++) {
		if (es->frameInfo[i].weight > weight) {
			weight = es->frameInfo[i].weight;
			slot   = i;
			index  = es->frameInfo[i].index;
		}
	}

	fresh   = (s_fpWeapon.client != followClient) ? qtrue : qfalse;
	swapped = (!fresh && (!wasValid || es->number != s_fpWeapon.entNum || es->modelindex != s_fpWeapon.modelindex))
				? qtrue
				: qfalse;

	if (fresh || swapped) {
		// A player just followed starts at idle, whatever the weapon does at that moment. A weapon that
		//  was not in the hand a frame ago is pulled out.
		CG_SpectateFP_WeaponIdentity(weaponTiki);
		s_fpWeapon.anim = swapped ? VM_ANIM_PULLOUT : VM_ANIM_IDLE;
		s_fpWeapon.animCount++;
		s_fpWeapon.lastSlot    = slot;
		s_fpWeapon.lastIndex   = index;
		s_fpWeapon.idlePending = swapped;
		s_fpWeapon.idleTime    = cg.time;
		s_fpWeapon.overTime    = 0;
	} else if (slot != s_fpWeapon.lastSlot || index != s_fpWeapon.lastIndex) {
		s_fpWeapon.lastSlot  = slot;
		s_fpWeapon.lastIndex = index;

		action = CG_SpectateFP_WeaponAction(weaponTiki, index);
		if (action == VM_ANIM_IDLE) {
			// the hands first end what they do (below)
			s_fpWeapon.idlePending = qtrue;
			s_fpWeapon.idleTime    = cg.time;
		} else {
			s_fpWeapon.anim = action;
			s_fpWeapon.animCount++;
			s_fpWeapon.idlePending = qfalse;
			s_fpWeapon.overTime    = 0;
		}
	} else if (s_fpWeapon.anim != VM_ANIM_IDLE) {
		// The hands hold an action until they are told the next one. The own view is told to go back to
		//  idle by the server; here the weapon's own return to idle says so, once the hands' animation has
		//  run out. An action the weapon never left idle for ends a moment after it has run out.
		over = qtrue;
		if (s_fpArmsFrame == cg.clientFrame - 1) {
			over = qfalse;
			if (cgi.anim->g_iLastVMAnimChanged == s_fpWeapon.animCount) {
				armsSlot  = cgi.anim->g_iCurrentVMAnimSlot;
				armsIndex = -1;
				if (armsSlot >= 0 && armsSlot < MAX_FRAMEINFOS) {
					armsIndex = cgi.anim->g_VMFrameInfo[armsSlot].index;
				}

				if (!cg.pPlayerFPSModel || armsIndex < 0
					|| cgi.anim->g_VMFrameInfo[armsSlot].time
						   >= cgi.Anim_Time(cg.pPlayerFPSModel, armsIndex) - 0.01f) {
					over = qtrue;
				}
			}
		}

		if (!over) {
			s_fpWeapon.overTime = 0;
		} else if (!s_fpWeapon.overTime) {
			s_fpWeapon.overTime = cg.time;
		}

		if (s_fpWeapon.idlePending) {
			// a grenade held back has no end of its own
			if (over || s_fpWeapon.anim == VM_ANIM_CHARGE || cg.time - s_fpWeapon.idleTime > SPECTATEFP_IDLE_WAIT) {
				s_fpWeapon.anim = VM_ANIM_IDLE;
				s_fpWeapon.animCount++;
				s_fpWeapon.idlePending = qfalse;
				s_fpWeapon.overTime    = 0;
			}
		} else if (over && s_fpWeapon.anim != VM_ANIM_CHARGE && cg.time - s_fpWeapon.overTime > SPECTATEFP_OVER_WAIT) {
			s_fpWeapon.anim = VM_ANIM_IDLE;
			s_fpWeapon.animCount++;
			s_fpWeapon.overTime = 0;
		}
	}

	s_fpWeapon.valid      = qtrue;
	s_fpWeapon.client     = followClient;
	s_fpWeapon.entNum     = es->number;
	s_fpWeapon.modelindex = es->modelindex;
}

/*
===============
CG_SpectateFP_DrawArms

Added in MoH Arena: whether the followed player is drawn as the weapon and the hands of their own view.
Not while they aim through a scope (the scope's picture is drawn then), not on a ladder and not without
a weapon in the hand: then nothing of them is drawn, as before.
===============
*/
qboolean CG_SpectateFP_DrawArms(void)
{
	const entityState_t *s;

	if (!cg.spectateFp.active || cg.spectateFp.inZoom || !s_fpWeapon.valid || !cg_drawviewmodel->integer) {
		return qfalse;
	}

	s = &cg_entities[cg.spectateFp.clientNum].currentState;
	if (s->eType != ET_PLAYER || (s->eFlags & (EF_DEAD | EF_CLIMBWALL))) {
		return qfalse;
	}

	return qtrue;
}

/*
===============
CG_SpectateFP_ArmsReady

Added in MoH Arena: whether the hands were set up in this frame, so that what the followed player holds
can go onto them (the player is always added before what hangs on them)
===============
*/
qboolean CG_SpectateFP_ArmsReady(void)
{
	if (s_fpArmsFrame != cg.clientFrame || !cg.pPlayerFPSModel) {
		return qfalse;
	}

	return CG_SpectateFP_DrawArms();
}

// Added in MoH Arena: the followed player's weapon and action, for CG_ViewModelAnimation
int CG_SpectateFP_ArmsWeaponClass(void)
{
	return s_fpWeapon.weaponClass;
}

const char *CG_SpectateFP_ArmsWeaponName(void)
{
	return s_fpWeapon.name;
}

int CG_SpectateFP_ArmsAnim(void)
{
	return s_fpWeapon.anim;
}

int CG_SpectateFP_ArmsAnimCount(void)
{
	return s_fpWeapon.animCount;
}

/*
===============
CG_SpectateFP_ArmsChanged

Added in MoH Arena: notes whether a player was drawn as hands, and says when that differs from the last
time. The animations an entity ran before then belong to its other model (CG_ModelAnim).
===============
*/
qboolean CG_SpectateFP_ArmsChanged(int clientNum, qboolean arms)
{
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || s_fpArmsDrawn[clientNum] == arms) {
		return qfalse;
	}

	s_fpArmsDrawn[clientNum] = arms;
	return qtrue;
}

/*
===============
CG_SpectateFP_ArmsPose

Added in MoH Arena: the angles of a first-person model for the followed player, as
PmoveAdjustAngleSettings_Client sets them for the own view: the model turns with the view's yaw alone and
its arms bone takes the pitch and 0.7 of the lean. Written into the entity's drawn angles and never into
its state, whose pelvis roll is where the lean is read from.
===============
*/
void CG_SpectateFP_ArmsPose(centity_t *cent)
{
	vec3_t armsAngles;

	QuatClear(cent->bone_quat[SPECTATEFP_HEAD_TAG]);
	QuatClear(cent->bone_quat[SPECTATEFP_TORSO_TAG]);
	QuatClear(cent->bone_quat[SPECTATEFP_PELVIS_TAG]);
	QuatCopy(cent->currentState.bone_quat[SPECTATEFP_MOUTH_TAG], cent->bone_quat[SPECTATEFP_MOUTH_TAG]);

	VectorSet(cent->lerpAngles, 0, AngleMod(cg.refdefViewAngles[YAW]), 0);

	armsAngles[0] = AngleMod(cg.refdefViewAngles[PITCH]);
	if (armsAngles[0] > 180.0f) {
		armsAngles[0] -= 360.0f;
	}
	armsAngles[0] += 8.0f;
	armsAngles[1]  = 0.0f;
	armsAngles[2]  = cg.spectateFp.leanAngle * 0.7f;
	EulerToQuat(armsAngles, cent->bone_quat[SPECTATEFP_ARMS_TAG]);
}

/*
===============
CG_SpectateFP_ArmsModel

Added in MoH Arena: turns the followed player's model into the weapon hands of their own view, as
CG_ModelAnim does for this client's own player: the first-person model that goes with the player's skin,
its animation from the action worked out in CG_SpectateFP_UpdateWeapon, the hand that goes with the
weapon, and its place at the eye with the sway of a moving player (CG_OffsetFirstPersonView).
===============
*/
void CG_SpectateFP_ArmsModel(centity_t *cent, refEntity_t *model)
{
	dtiki_t      *worldTiki = model->tiki;
	orientation_t oHead;
	vec3_t        vDelta;
	vec3_t        mat[3];
	qhandle_t     hModel;
	char          fpsname[128];
	const char   *hide;
	const char   *show;
	int           iTag;
	int           iSurfaceNum;
	int           i;

	// the first-person model of the player's skin, kept where the own view keeps its own
	if (!cg.pLastPlayerWorldModel || cg.pLastPlayerWorldModel != worldTiki) {
		COM_StripExtension(worldTiki->a->name, fpsname, sizeof(fpsname));
		Q_strcat(fpsname, sizeof(fpsname), "_fps.tik");

		hModel = cgi.R_RegisterModel(fpsname);
		if (!hModel) {
			hModel = cgi.R_RegisterModel(va(
				"models/player/%s_fps.tik",
				(cent->currentState.eFlags & EF_AXIS) ? dm_playergermanmodel->resetString : dm_playermodel->resetString
			));
		}

		if (hModel) {
			cg.hPlayerFPSModelHandle = hModel;
			cg.pPlayerFPSModel       = cgi.R_Model_GetHandle(hModel);
			if (!cg.pPlayerFPSModel) {
				cg.pPlayerFPSModel = worldTiki;
			}
		} else {
			cg.hPlayerFPSModelHandle = model->hModel;
			cg.pPlayerFPSModel       = worldTiki;
		}

		cg.pLastPlayerWorldModel = worldTiki;
	}

	model->tiki   = cg.pPlayerFPSModel;
	model->hModel = cg.hPlayerFPSModelHandle;
	memset(model->surfaces, 0, sizeof(model->surfaces));

	CG_ViewModelAnimation(model);
	model->renderfx |= RF_FRAMELERP;
	cgi.ForceUpdatePose(model);

	if ((cent->currentState.eFlags & EF_UNARMED) || cg_drawviewmodel->integer <= 1) {
		// the weapon alone, with no hands
		for (i = 0; i < MAX_MODEL_SURFACES; i++) {
			model->surfaces[i] |= MDL_SURFACE_NODRAW;
		}
	} else {
		if (!Q_stricmp(s_fpWeapon.name, "M1 Garand") || !Q_stricmp(s_fpWeapon.name, "Springfield '03 Sniper")
			|| !Q_stricmp(s_fpWeapon.name, "Mauser KAR 98K") || !Q_stricmp(s_fpWeapon.name, "KAR98 - Sniper")) {
			hide = "lefthand";
			show = "garandhand";
		} else {
			hide = "garandhand";
			show = "lefthand";
		}

		iSurfaceNum = cgi.Surface_NameToNum(model->tiki, hide);
		if (iSurfaceNum >= 0 && iSurfaceNum < MAX_MODEL_SURFACES) {
			model->surfaces[iSurfaceNum] |= MDL_SURFACE_NODRAW;
		}

		iSurfaceNum = cgi.Surface_NameToNum(model->tiki, show);
		if (iSurfaceNum >= 0 && iSurfaceNum < MAX_MODEL_SURFACES) {
			model->surfaces[iSurfaceNum] &= ~MDL_SURFACE_NODRAW;
		}
	}

	model->renderfx |= RF_DEPTHHACK;

	// The model's "eyes bone" sits at the spectator's eye. The own view finds its eye at that bone and
	//  moves the model along with every later change of the eye, which comes to the same place.
	VectorCopy(cg.refdef.vieworg, model->origin);
	iTag = cgi.Tag_NumForName(worldTiki, "eyes bone");
	if (iTag != -1) {
		oHead = cgi.TIKI_Orientation(model, iTag);
		for (i = 0; i < 3; i++) {
			VectorMA(model->origin, -oHead.origin[i], model->axis[i], model->origin);
		}
	}

	// the sway of the hands of a moving player
	VectorCopy(cg.refdefViewAngles, vDelta);
	vDelta[0] *= 0.5f;
	vDelta[2] *= 0.75f;
	AngleVectorsLeft(vDelta, mat[0], mat[1], mat[2]);

	CG_CalcViewModelMovementFor(
		s_fpBobPhase, s_fpBobAmp, s_fpVelocity, vDelta, s_fpWalking, s_fpCrouching, cg.spectateFp.leanAngle
	);
	for (i = 0; i < 3; i++) {
		VectorMA(model->origin, vDelta[i], mat[i], model->origin);
	}

	model->renderfx &= ~(RF_FIRST_PERSON | RF_THIRD_PERSON);
	model->renderfx |= RF_FIRST_PERSON;

	s_fpArmsFrame = cg.clientFrame;
}
