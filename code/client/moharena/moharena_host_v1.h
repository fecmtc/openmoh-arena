#ifndef MOHARENA_HOST_V1_H
#define MOHARENA_HOST_V1_H

/* A subset of moharena-native's include/moharena/moharena_host_v1.h: only the
 * parts the OpenMoH Arena bridge ABI (moharena_opm_engine_v1.h and
 * moharena_opm_bootstrap_v1.h) uses. Every value and layout here matches the
 * full header; the rest of it is the module's own business. */

#include <stddef.h>
#include <stdint.h>

#include "moharena_scope_v1.h"

#if defined(_WIN32)
#define MOHARENA_CALL __cdecl
#else
#define MOHARENA_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MOHARENA_HOST_ABI_V1 UINT32_C(0x00010000)

typedef int32_t MohArenaResult;
enum {
    MOHARENA_OK = 0,
    MOHARENA_E_INVALID_ARGUMENT = -1,
    MOHARENA_E_ABI_MISMATCH = -2,
    MOHARENA_E_STRUCT_TOO_SMALL = -3,
    MOHARENA_E_OUT_OF_MEMORY = -4,
    MOHARENA_E_UNSUPPORTED_BUILD = -5,
    MOHARENA_E_VALIDATION_FAILED = -6,
    MOHARENA_E_INTERNAL = -7
};

/* KEY_DOWN and KEY_UP carry a Windows virtual-key code. CHARACTER carries one
 * Unicode code point (never a surrogate half). BUTTON_DOWN and BUTTON_UP
 * carry the button: 0 left, 1 right, 2 middle. WHEEL carries notches,
 * positive away from the user. */
typedef enum MohArenaInputKindV1 {
    MOHARENA_INPUT_KEY_DOWN = 1,
    MOHARENA_INPUT_KEY_UP = 2,
    MOHARENA_INPUT_CHARACTER = 3,
    MOHARENA_INPUT_POINTER = 4,
    MOHARENA_INPUT_BUTTON_DOWN = 5,
    MOHARENA_INPUT_FOCUS_LOST = 6,
    MOHARENA_INPUT_BUTTON_UP = 7,
    MOHARENA_INPUT_WHEEL = 8
} MohArenaInputKindV1;

enum {
    MOHARENA_MODIFIER_CTRL = 1,
    MOHARENA_MODIFIER_SHIFT = 2,
    MOHARENA_MODIFIER_ALT = 4
};

/* A position in viewport pixels (origin top-left), a texture coordinate and a
 * color with straight, not premultiplied, alpha that modulates the texel. */
typedef struct MohArenaVertexV1 {
    float x;
    float y;
    float u;
    float v;
    uint8_t rgba[4];
} MohArenaVertexV1;

/* One indexed triangle list. index_count is a multiple of 3 and every index
 * is below vertex_count. texture 0 draws untextured. The clip rectangle is in
 * viewport pixels; x1/y1 are inclusive and x2/y2 exclusive. */
typedef struct MohArenaDrawBatchV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    const MohArenaVertexV1 *vertices;
    uint32_t vertex_count;
    const uint32_t *indices;
    uint32_t index_count;
    uint32_t texture;
    float clip_x1;
    float clip_y1;
    float clip_x2;
    float clip_y2;
} MohArenaDrawBatchV1;

#ifdef __cplusplus
}
#endif

#endif
