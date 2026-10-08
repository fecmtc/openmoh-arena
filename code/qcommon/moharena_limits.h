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

// moharena_limits.h -- Added in MoH Arena
//
// The limits MoH Arena puts on a few settings in its modern UI mode, shared by
// the client, the cgame and the renderer so that all three use the same
// numbers. Include it after q_shared.h.

#pragma once

// The lowest and highest cg_fov, in thousandths of a degree.
#define MOHARENA_FOV_MIN_MILLI 65000
#define MOHARENA_FOV_MAX_MILLI 80580
// The widest horizontal view, in degrees.
#define MOHARENA_HFOV_MAX 98.0
// The screen shape cg_fov is measured for.
#define MOHARENA_FOV_ASPECT (4.0 / 3.0)
// Its own pi, so the result does not depend on the platform's M_PI.
#define MOHARENA_PI 3.14159265358979323846
// The highest r_picmip.
#define MOHARENA_PICMIP_MAX 2
// The highest cg_shadows.
#define MOHARENA_SHADOWS_MAX 2
// The longest number MoHArena_ReadNumber takes, with its terminator.
#define MOHARENA_NUMBER_MAX 32
// Room for the cg_fov text MoHArena_ClampFov writes.
#define MOHARENA_FOV_TEXT_MAX 16

// A plain decimal number, as written.
typedef struct {
    qboolean negative;
    // Stops growing once it is past every limit used here.
    int      whole;
    // The first three decimals, in thousandths.
    int      milli;
    // A later decimal is not zero.
    qboolean more;
} moharenaNumber_t;

// Reads a plain decimal number such as "80", "-0.022" or ".5". Spaces,
// exponents, hex and names such as "nan" are not plain: qfalse.
static ID_INLINE qboolean MoHArena_ReadNumber(const char *text, moharenaNumber_t *out)
{
    int i;
    int digits;
    int decimals;

    out->negative = qfalse;
    out->whole    = 0;
    out->milli    = 0;
    out->more     = qfalse;

    if (!text) {
        return qfalse;
    }

    i = 0;
    if (text[i] == '-' || text[i] == '+') {
        out->negative = text[i] == '-' ? qtrue : qfalse;
        i++;
    }

    digits = 0;
    for (; text[i] >= '0' && text[i] <= '9'; i++) {
        if (i >= MOHARENA_NUMBER_MAX - 1) {
            return qfalse;
        }

        if (out->whole < 100000000) {
            out->whole = out->whole * 10 + (text[i] - '0');
        }

        digits++;
    }

    if (text[i] == '.') {
        i++;
        decimals = 0;
        for (; text[i] >= '0' && text[i] <= '9'; i++) {
            if (i >= MOHARENA_NUMBER_MAX - 1) {
                return qfalse;
            }

            if (decimals < 3) {
                out->milli = out->milli * 10 + (text[i] - '0');
            } else if (text[i] != '0') {
                out->more = qtrue;
            }

            decimals++;
            digits++;
        }

        for (; decimals < 3; decimals++) {
            out->milli *= 10;
        }
    }

    return (digits > 0 && !text[i]) ? qtrue : qfalse;
}

// The highest cg_fov for a view of this size, in thousandths of a degree: the
// plain limit, lowered on wide views to keep the horizontal view inside its
// own limit. A view of unknown size gets the plain limit.
static ID_INLINE int MoHArena_MaxFovMilli(int width, int height)
{
    float  aspect;
    double ratio;
    double limit;
    int    milli;

    if (width <= 0 || height <= 0) {
        return MOHARENA_FOV_MAX_MILLI;
    }

    // Single precision for the shape, as the module computes it.
    aspect = (float)width / (float)height;
    ratio  = (double)aspect / MOHARENA_FOV_ASPECT;
    limit  = atan(tan(MOHARENA_HFOV_MAX * MOHARENA_PI / 360.0) / ratio) * 360.0 / MOHARENA_PI;

    if (!(limit < MOHARENA_FOV_MAX_MILLI / 1000.0)) {
        return MOHARENA_FOV_MAX_MILLI;
    }

    // One thousandth above the rounded-down limit, so a value the module
    // worked out with its own math library is never lowered.
    milli = (int)floor(limit * 1000.0) + 1;
    if (milli < MOHARENA_FOV_MIN_MILLI) {
        milli = MOHARENA_FOV_MIN_MILLI;
    }

    if (milli > MOHARENA_FOV_MAX_MILLI) {
        milli = MOHARENA_FOV_MAX_MILLI;
    }

    return milli;
}

// Keeps cg_fov inside its limits for a view of this size. qtrue when the value
// has to change, with the new text in out. A plain number inside the limits
// stays exactly as written; anything else is rewritten as a plain number, so
// the saved config never holds a value that reads differently elsewhere.
static ID_INLINE qboolean MoHArena_ClampFov(const char *text, int width, int height, char *out, size_t outSize)
{
    moharenaNumber_t number;
    double           value;
    int              highest;
    int              milli;

    highest = MoHArena_MaxFovMilli(width, height);

    if (MoHArena_ReadNumber(text, &number)) {
        // Compared as written, in whole thousandths, with no rounding.
        if (number.negative || number.whole < MOHARENA_FOV_MIN_MILLI / 1000) {
            milli = MOHARENA_FOV_MIN_MILLI;
        } else if (number.whole > highest / 1000) {
            milli = highest;
        } else {
            milli = number.whole * 1000 + number.milli;
            if (milli < highest || (milli == highest && !number.more)) {
                return qfalse;
            }

            milli = highest;
        }
    } else {
        value = text ? atof(text) : 0.0;
        if (!(value >= MOHARENA_FOV_MIN_MILLI / 1000.0)) {
            // Also what is not a number at all.
            milli = MOHARENA_FOV_MIN_MILLI;
        } else if (value >= highest / 1000.0) {
            milli = highest;
        } else {
            milli = (int)floor(value * 1000.0);
            if (milli < MOHARENA_FOV_MIN_MILLI) {
                milli = MOHARENA_FOV_MIN_MILLI;
            }

            if (milli > highest) {
                milli = highest;
            }
        }
    }

    Com_sprintf(out, outSize, "%d.%03d", milli / 1000, milli % 1000);
    return qtrue;
}
