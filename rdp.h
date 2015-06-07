// neon64/rdp.h

#ifndef RDP_H
#define RDP_H

#include "drawgl.h"
#include "simd.h"
#include "textures.h"
#include <stdint.h>

#define MI_INTR_SP 0x01
#define MI_INTR_DP 0x20

#define TEXTURE_FORMAT_RGBA     0
#define TEXTURE_FORMAT_YUV      1
#define TEXTURE_FORMAT_CI       2
#define TEXTURE_FORMAT_IA       3
#define TEXTURE_FORMAT_I        4

#define RDP_COMBINE_MODE_COMBINED           0
#define RDP_COMBINE_MODE_TEXEL0             1
#define RDP_COMBINE_MODE_TEXEL1             2
#define RDP_COMBINE_MODE_PRIMITIVE          3
#define RDP_COMBINE_MODE_SHADE              4
#define RDP_COMBINE_MODE_ENVIRONMENT        5
#define RDP_COMBINE_MODE_CENTER             6
#define RDP_COMBINE_MODE_SCALE              7
#define RDP_COMBINE_MODE_COMBINED_ALPHA     8
#define RDP_COMBINE_MODE_TEXEL0_ALPHA       9
#define RDP_COMBINE_MODE_TEXEL1_ALPHA       10
#define RDP_COMBINE_MODE_PRIMITIVE_ALPHA    11
#define RDP_COMBINE_MODE_SHADE_ALPHA        12
#define RDP_COMBINE_MODE_ENV_ALPHA          13
#define RDP_COMBINE_MODE_LOD_FRACTION       14
#define RDP_COMBINE_MODE_PRIM_LOD_FRAC      15
#define RDP_COMBINE_MODE_NOISE              16
#define RDP_COMBINE_MODE_K4                 17
#define RDP_COMBINE_MODE_K5                 18
#define RDP_COMBINE_MODE_ONE                19
#define RDP_COMBINE_MODE_ZERO               20
#define RDP_COMBINE_MODE_UNKNOWN            21

#define RDP_GEOMETRY_MODE_ZBUFFER   0x00001
#define RDP_GEOMETRY_MODE_TEXTURE   0x00002
#define RDP_GEOMETRY_MODE_SHADE     0x00004
#define RDP_GEOMETRY_MODE_LIGHTING  0x20000

struct matrix4x4f32 {
    float32x4_t m[4];
};

struct memory {
    uint8_t *rdram;
    uint8_t *dmem;
};

struct registers {
    uint32_t *mi_intr;
};

struct vertex {
    vfloat32x4_t position;

    int16_t s;
    int16_t t;

    uint32_t rgba;
};

struct tile {
    // Color format.
    uint8_t format;
    // Bits per pixel.
    uint8_t size;
    // The texture memory location.
    uint32_t addr;
    uint8_t mask_s;
    uint8_t mask_t;
    uint8_t shift_s;
    uint8_t shift_t;
};

struct combiner {
    uint8_t sargb0, sbrgb0, mrgb0, argb0;
    uint8_t saa0, sba0, ma0, aa0;
    uint8_t sargb1, sbrgb1, mrgb1, argb1;
    uint8_t saa1, sba1, ma1, aa1;
};

struct rdp {
    matrix4x4f32 projection[16];
    uint32_t projection_index;

    matrix4x4f32 modelview[16];
    uint32_t modelview_index;

    vertex vertices[128];

    tile tiles[8];
    uint16_t texture_upper_left_s;
    uint16_t texture_upper_left_t;
    uint16_t texture_lower_right_s;
    uint16_t texture_lower_right_t;
    uint32_t texture_address;
    uint8_t texture_tile;
    bool texture_enabled;
    // The current texture ID; see `drawgl.h`.
    uint32_t texture_id;

    uint32_t segments[16];

    combiner combiner;
    uint32_t primitive_color;
    uint32_t environment_color;
    uint32_t geometry_mode;

    uint32_t light_count;
    uint32_t ambient_light;
};

void send_dp_interrupt();
void send_sp_interrupt();

#endif

