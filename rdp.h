// neon64/rdp.h

#ifndef RDP_H
#define RDP_H

#include "rasterize.h"
#include "simd.h"
#include <stdint.h>

#define MI_INTR_SP 0x01
#define MI_INTR_DP 0x20

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
    int16x4_t position;

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
    uint32_t width;
    uint32_t height;
};

struct rdp {
    matrix4x4f32 projection[16];
    uint32_t projection_index;

    matrix4x4f32 modelview[16];
    uint32_t modelview_index;

    vertex vertices[128];

    tile tiles[8];

    uint32_t segments[16];
};

void send_dp_interrupt();
void send_sp_interrupt();

#endif

