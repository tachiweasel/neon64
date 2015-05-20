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

struct rdp {
    matrix4x4f32 projection[16];
    uint32_t projection_index;

    matrix4x4f32 modelview[16];
    uint32_t modelview_index;

    vertex vertices[128];

    tile tiles[8];
    swizzled_texture swizzled_texture;
    uint16_t texture_upper_left_s;
    uint16_t texture_upper_left_t;
    uint16_t texture_lower_right_s;
    uint16_t texture_lower_right_t;
    uint32_t texture_address;
    uint8_t texture_tile;
    bool texture_enabled;

    uint32_t segments[16];
};

void send_dp_interrupt();
void send_sp_interrupt();

#endif

