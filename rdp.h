// neon64/rdp.h

#ifndef RDP_H
#define RDP_H

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

struct rdp {
    matrix4x4f32 projection[16];
    uint32_t projection_index;

    matrix4x4f32 modelview[16];
    uint32_t modelview_index;

    vertex vertices[128];

    uint32_t segments[16];
};

struct plugin {
    memory memory;
    registers registers;
    rdp rdp;
};

extern struct plugin plugin;

inline void send_dp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_DP;
}

inline void send_sp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_SP;
}

#endif

