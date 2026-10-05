#ifndef MOHARENA_SCOPE_V1_H
#define MOHARENA_SCOPE_V1_H

/* The sniper scopes a host draws where the game draws its own
 * (docs/openmohaa-bridge.md, D11). Plain C, and nothing but <stdint.h>: the
 * core, both hosts and OpenMoH Arena's cgame read the same structs and the
 * same three rules below.
 *
 * A scope is drawn in a square at the middle of the view: four lens quads of
 * half the square's side each, in the order upper left, upper right, lower
 * left, lower right, then the game's two black side bars, then the black
 * quads of the reticle. The module only says which lens, and which black
 * quads; the pictures are the game's own. */

#include <stdint.h>

enum {
    MOHARENA_SCOPE_LENS_GAME = 0,   /* the game draws its own */
    MOHARENA_SCOPE_LENS_ALLIED = 1, /* the Allied lens picture */
    MOHARENA_SCOPE_LENS_AXIS = 2,   /* the two KAR pictures */
    MOHARENA_SCOPE_LENS_CLEAN = 3   /* the Allied lens, no cross */
};
#define MOHARENA_SCOPE_MAX_QUADS 8

/* The game's pictures a lens quad is drawn with. */
enum {
    MOHARENA_SCOPE_PICTURE_ALLIED = 0,    /* textures/hud/zoomoverlay */
    MOHARENA_SCOPE_PICTURE_KAR_TOP = 1,   /* textures/hud/kartop.tga */
    MOHARENA_SCOPE_PICTURE_KAR_BOTTOM = 2 /* textures/hud/karbottom.tga */
};

/* The clean lens ends at this texture coordinate, in place of 1: the Allied
 * picture holds its cross in the last of its 256 rows and columns only, and
 * a quad that ends here samples nothing of them. */
#define MOHARENA_SCOPE_CLEAN_EDGE (254.0f / 256.0f)

typedef struct MohArenaScopeQuadV1 {
    int32_t x, y, w, h; /* pixels from the square's top left */
    uint32_t rgb;       /* 0 (black); else: the stock scope */
} MohArenaScopeQuadV1;

typedef struct MohArenaScopeRifleV1 {
    uint32_t lens;      /* MOHARENA_SCOPE_LENS_* */
    uint32_t lens_size; /* the square's side the quads fit */
    uint32_t quad_count;
    MohArenaScopeQuadV1 quads[MOHARENA_SCOPE_MAX_QUADS];
} MohArenaScopeRifleV1;

typedef struct MohArenaScopeV1 {
    uint32_t struct_size;
    MohArenaScopeRifleV1 allies; /* every scoped rifle but: */
    MohArenaScopeRifleV1 axis;   /* "KAR98 - Sniper" */
} MohArenaScopeV1;

/* One lens quad as a host draws it. */
typedef struct MohArenaScopeLensQuadV1 {
    uint32_t picture;     /* MOHARENA_SCOPE_PICTURE_* */
    int32_t x, y, w, h;   /* pixels from the square's top left */
    float s1, t1, s2, t2; /* texture coordinates, as the game's draw call takes them */
} MohArenaScopeLensQuadV1;

/* 1 for a struct a host can read: struct_size covers it, each lens is a
 * known one and neither rifle counts more quads than it holds. In place of
 * any other struct a host keeps the game's own scopes. */
static inline int moharena_scope_readable(const MohArenaScopeV1 *scope) {
    if (!scope || scope->struct_size < (uint32_t)sizeof(MohArenaScopeV1)) return 0;
    if (scope->allies.lens > (uint32_t)MOHARENA_SCOPE_LENS_CLEAN ||
        scope->axis.lens > (uint32_t)MOHARENA_SCOPE_LENS_CLEAN) return 0;
    return scope->allies.quad_count <= (uint32_t)MOHARENA_SCOPE_MAX_QUADS &&
           scope->axis.quad_count <= (uint32_t)MOHARENA_SCOPE_MAX_QUADS;
}

/* 1 when a host may draw the rifle's scope in a square of side lens_size:
 * the lens is a known one, the quads were built for that very square, and
 * each of them has an area, lies inside the square and is black. A rifle
 * that fails is drawn as the game draws it. */
static inline int moharena_scope_rifle_ok(const MohArenaScopeRifleV1 *rifle, uint32_t lens_size) {
    uint32_t i;
    if (!rifle || rifle->lens > (uint32_t)MOHARENA_SCOPE_LENS_CLEAN) return 0;
    if (rifle->quad_count > (uint32_t)MOHARENA_SCOPE_MAX_QUADS || rifle->lens_size != lens_size) return 0;
    for (i = 0; i < rifle->quad_count; ++i) {
        const MohArenaScopeQuadV1 *quad = &rifle->quads[i];
        if (quad->rgb != 0u || quad->w <= 0 || quad->h <= 0 || quad->x < 0 || quad->y < 0) return 0;
        if ((uint32_t)quad->w > lens_size || (uint32_t)quad->x > lens_size - (uint32_t)quad->w) return 0;
        if ((uint32_t)quad->h > lens_size || (uint32_t)quad->y > lens_size - (uint32_t)quad->h) return 0;
    }
    return 1;
}

/* Lens quad `index` (0 to 3, in the game's order) of a lens other than the
 * game's own, in a square of side 2 * half: the picture, the place and the
 * texture coordinates the game's own code draws it with. The clean lens is
 * the Allied one up to MOHARENA_SCOPE_CLEAN_EDGE. Returns 0, and leaves
 * `out` alone, for anything else. */
static inline int moharena_scope_lens_quad(uint32_t lens, uint32_t index, int32_t half,
                                           MohArenaScopeLensQuadV1 *out) {
    const float edge = lens == (uint32_t)MOHARENA_SCOPE_LENS_CLEAN ? MOHARENA_SCOPE_CLEAN_EDGE : 1.0f;
    const int right = (index & 1u) != 0u;
    const int lower = (index & 2u) != 0u;
    if (!out || index > 3u || half <= 0) return 0;
    if (lens != (uint32_t)MOHARENA_SCOPE_LENS_ALLIED && lens != (uint32_t)MOHARENA_SCOPE_LENS_AXIS &&
        lens != (uint32_t)MOHARENA_SCOPE_LENS_CLEAN) return 0;
    out->x = right ? half : 0;
    out->y = lower ? half : 0;
    out->w = half;
    out->h = half;
    if (lens == (uint32_t)MOHARENA_SCOPE_LENS_AXIS) {
        /* The KAR pictures are the scope's right half: plain on the right, mirrored on the left. */
        out->picture = lower ? (uint32_t)MOHARENA_SCOPE_PICTURE_KAR_BOTTOM : (uint32_t)MOHARENA_SCOPE_PICTURE_KAR_TOP;
        out->s1 = right ? 0.0f : 1.0f;
        out->t1 = 0.0f;
        out->s2 = right ? 1.0f : 0.0f;
        out->t2 = 1.0f;
    } else {
        /* The Allied picture is the scope's upper left quarter, mirrored into the other three. */
        out->picture = (uint32_t)MOHARENA_SCOPE_PICTURE_ALLIED;
        out->s1 = right ? edge : 0.0f;
        out->t1 = lower ? edge : 0.0f;
        out->s2 = right ? 0.0f : edge;
        out->t2 = lower ? 0.0f : edge;
    }
    return 1;
}

#endif
