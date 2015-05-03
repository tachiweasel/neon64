// neon64/displaylist.cpp

#include "displaylist.h"
#include "rasterize.h"
#include "rdp.h"
#include "simd.h"
#include <stdio.h>

#define MOVE_WORD_SEGMENT   6

#define DLIST_CALL  0
#define DLIST_JUMP  1

typedef int32_t (*display_op_t)(display_item *item);

struct rdp_vertex {
    int16_t y;
    int16_t x;
    int16_t flags;
    int16_t z;

    int16_t t;
    int16_t s;

    uint32_t rgba;
};

// Converts a segmented address to a flat RDRAM address.
uint32_t segment_address(uint32_t address) {
    return plugin.rdp.segments[(address >> 24) & 0xf] + (address & 0x00ffffff);
}

// NB: The address is a flat RDRAM address, not a segmented address.
// TODO(tachiweasel): SIMD-ify this?
matrix4x4f32 load_matrix(uint32_t address) {
    matrix4x4f32 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int high = *((int16_t *)(&plugin.memory.rdram[(address + (i * 8) + (j * 2)) ^ 0x2]));
            int low =
                *((uint16_t *)(&plugin.memory.rdram[(address + (i * 8) + (j * 2) + 32) ^ 0x2]));
            result.m[i] = vsetq_lane_f32((float)((high << 16) | low) * (1.0 / 65536.0),
                                         result.m[i],
                                         j);
        }
    }
    return result;
}

// TODO(tachiweasel): SIMD-ify.
matrix4x4f32 multiply_matrix4x4f32(matrix4x4f32 a, matrix4x4f32 b) {
    matrix4x4f32 result;

    result.m[0] = vsetq_lane_f32(vgetq_lane_f32(a.m[0], 0) * vgetq_lane_f32(b.m[0], 0) +
                                 vgetq_lane_f32(a.m[0], 1) * vgetq_lane_f32(b.m[1], 0) +
                                 vgetq_lane_f32(a.m[0], 2) * vgetq_lane_f32(b.m[2], 0) +
                                 vgetq_lane_f32(a.m[0], 3) * vgetq_lane_f32(b.m[3], 0),
                                 result.m[0],
                                 0);
    result.m[0] = vsetq_lane_f32(vgetq_lane_f32(a.m[0], 0) * vgetq_lane_f32(b.m[0], 1) +
                                 vgetq_lane_f32(a.m[0], 1) * vgetq_lane_f32(b.m[1], 1) +
                                 vgetq_lane_f32(a.m[0], 2) * vgetq_lane_f32(b.m[2], 1) +
                                 vgetq_lane_f32(a.m[0], 3) * vgetq_lane_f32(b.m[3], 1),
                                 result.m[0],
                                 1);
    result.m[0] = vsetq_lane_f32(vgetq_lane_f32(a.m[0], 0) * vgetq_lane_f32(b.m[0], 2) +
                                 vgetq_lane_f32(a.m[0], 1) * vgetq_lane_f32(b.m[1], 2) +
                                 vgetq_lane_f32(a.m[0], 2) * vgetq_lane_f32(b.m[2], 2) +
                                 vgetq_lane_f32(a.m[0], 3) * vgetq_lane_f32(b.m[3], 2),
                                 result.m[0],
                                 2);
    result.m[0] = vsetq_lane_f32(vgetq_lane_f32(a.m[0], 0) * vgetq_lane_f32(b.m[0], 3) +
                                 vgetq_lane_f32(a.m[0], 1) * vgetq_lane_f32(b.m[1], 3) +
                                 vgetq_lane_f32(a.m[0], 2) * vgetq_lane_f32(b.m[2], 3) +
                                 vgetq_lane_f32(a.m[0], 3) * vgetq_lane_f32(b.m[3], 3),
                                 result.m[0],
                                 3);

    result.m[1] = vsetq_lane_f32(vgetq_lane_f32(a.m[1], 0) * vgetq_lane_f32(b.m[0], 0) +
                                 vgetq_lane_f32(a.m[1], 1) * vgetq_lane_f32(b.m[1], 0) +
                                 vgetq_lane_f32(a.m[1], 2) * vgetq_lane_f32(b.m[2], 0) +
                                 vgetq_lane_f32(a.m[1], 3) * vgetq_lane_f32(b.m[3], 0),
                                 result.m[1],
                                 0);
    result.m[1] = vsetq_lane_f32(vgetq_lane_f32(a.m[1], 0) * vgetq_lane_f32(b.m[0], 1) +
                                 vgetq_lane_f32(a.m[1], 1) * vgetq_lane_f32(b.m[1], 1) +
                                 vgetq_lane_f32(a.m[1], 2) * vgetq_lane_f32(b.m[2], 1) +
                                 vgetq_lane_f32(a.m[1], 3) * vgetq_lane_f32(b.m[3], 1),
                                 result.m[1],
                                 1);
    result.m[1] = vsetq_lane_f32(vgetq_lane_f32(a.m[1], 0) * vgetq_lane_f32(b.m[0], 2) +
                                 vgetq_lane_f32(a.m[1], 1) * vgetq_lane_f32(b.m[1], 2) +
                                 vgetq_lane_f32(a.m[1], 2) * vgetq_lane_f32(b.m[2], 2) +
                                 vgetq_lane_f32(a.m[1], 3) * vgetq_lane_f32(b.m[3], 2),
                                 result.m[1],
                                 2);
    result.m[1] = vsetq_lane_f32(vgetq_lane_f32(a.m[1], 0) * vgetq_lane_f32(b.m[0], 3) +
                                 vgetq_lane_f32(a.m[1], 1) * vgetq_lane_f32(b.m[1], 3) +
                                 vgetq_lane_f32(a.m[1], 2) * vgetq_lane_f32(b.m[2], 3) +
                                 vgetq_lane_f32(a.m[1], 3) * vgetq_lane_f32(b.m[3], 3),
                                 result.m[1],
                                 3);

    result.m[2] = vsetq_lane_f32(vgetq_lane_f32(a.m[2], 0) * vgetq_lane_f32(b.m[0], 0) +
                                 vgetq_lane_f32(a.m[2], 1) * vgetq_lane_f32(b.m[1], 0) +
                                 vgetq_lane_f32(a.m[2], 2) * vgetq_lane_f32(b.m[2], 0) +
                                 vgetq_lane_f32(a.m[2], 3) * vgetq_lane_f32(b.m[3], 0),
                                 result.m[2],
                                 0);
    result.m[2] = vsetq_lane_f32(vgetq_lane_f32(a.m[2], 0) * vgetq_lane_f32(b.m[0], 1) +
                                 vgetq_lane_f32(a.m[2], 1) * vgetq_lane_f32(b.m[1], 1) +
                                 vgetq_lane_f32(a.m[2], 2) * vgetq_lane_f32(b.m[2], 1) +
                                 vgetq_lane_f32(a.m[2], 3) * vgetq_lane_f32(b.m[3], 1),
                                 result.m[2],
                                 1);
    result.m[2] = vsetq_lane_f32(vgetq_lane_f32(a.m[2], 0) * vgetq_lane_f32(b.m[0], 2) +
                                 vgetq_lane_f32(a.m[2], 1) * vgetq_lane_f32(b.m[1], 2) +
                                 vgetq_lane_f32(a.m[2], 2) * vgetq_lane_f32(b.m[2], 2) +
                                 vgetq_lane_f32(a.m[2], 3) * vgetq_lane_f32(b.m[3], 2),
                                 result.m[2],
                                 2);
    result.m[2] = vsetq_lane_f32(vgetq_lane_f32(a.m[2], 0) * vgetq_lane_f32(b.m[0], 3) +
                                 vgetq_lane_f32(a.m[2], 1) * vgetq_lane_f32(b.m[1], 3) +
                                 vgetq_lane_f32(a.m[2], 2) * vgetq_lane_f32(b.m[2], 3) +
                                 vgetq_lane_f32(a.m[2], 3) * vgetq_lane_f32(b.m[3], 3),
                                 result.m[2],
                                 3);

    result.m[3] = vsetq_lane_f32(vgetq_lane_f32(a.m[3], 0) * vgetq_lane_f32(b.m[0], 0) +
                                 vgetq_lane_f32(a.m[3], 1) * vgetq_lane_f32(b.m[1], 0) +
                                 vgetq_lane_f32(a.m[3], 2) * vgetq_lane_f32(b.m[2], 0) +
                                 vgetq_lane_f32(a.m[3], 3) * vgetq_lane_f32(b.m[3], 0),
                                 result.m[3],
                                 0);
    result.m[3] = vsetq_lane_f32(vgetq_lane_f32(a.m[3], 0) * vgetq_lane_f32(b.m[0], 1) +
                                 vgetq_lane_f32(a.m[3], 1) * vgetq_lane_f32(b.m[1], 1) +
                                 vgetq_lane_f32(a.m[3], 2) * vgetq_lane_f32(b.m[2], 1) +
                                 vgetq_lane_f32(a.m[3], 3) * vgetq_lane_f32(b.m[3], 1),
                                 result.m[3],
                                 1);
    result.m[3] = vsetq_lane_f32(vgetq_lane_f32(a.m[3], 0) * vgetq_lane_f32(b.m[0], 2) +
                                 vgetq_lane_f32(a.m[3], 1) * vgetq_lane_f32(b.m[1], 2) +
                                 vgetq_lane_f32(a.m[3], 2) * vgetq_lane_f32(b.m[2], 2) +
                                 vgetq_lane_f32(a.m[3], 3) * vgetq_lane_f32(b.m[3], 2),
                                 result.m[3],
                                 2);
    result.m[3] = vsetq_lane_f32(vgetq_lane_f32(a.m[3], 0) * vgetq_lane_f32(b.m[0], 3) +
                                 vgetq_lane_f32(a.m[3], 1) * vgetq_lane_f32(b.m[1], 3) +
                                 vgetq_lane_f32(a.m[3], 2) * vgetq_lane_f32(b.m[2], 3) +
                                 vgetq_lane_f32(a.m[3], 3) * vgetq_lane_f32(b.m[3], 3),
                                 result.m[3],
                                 3);

    return result;
}

float32x4_t multiply_matrix4x4f32_float32x4(matrix4x4f32 *a, float32x4_t x) {
    float32x4_t ax = vdupq_n_f32(0.0);
    ax = vsetq_lane_f32((vgetq_lane_f32(a->m[0], 0) * vgetq_lane_f32(x, 0) +
                         vgetq_lane_f32(a->m[1], 0) * vgetq_lane_f32(x, 1) +
                         vgetq_lane_f32(a->m[2], 0) * vgetq_lane_f32(x, 2) +
                         vgetq_lane_f32(a->m[3], 0) * vgetq_lane_f32(x, 3)),
                        ax,
                        0);
    ax = vsetq_lane_f32((vgetq_lane_f32(a->m[0], 1) * vgetq_lane_f32(x, 0) +
                         vgetq_lane_f32(a->m[1], 1) * vgetq_lane_f32(x, 1) +
                         vgetq_lane_f32(a->m[2], 1) * vgetq_lane_f32(x, 2) +
                         vgetq_lane_f32(a->m[3], 1) * vgetq_lane_f32(x, 3)),
                        ax,
                        1);
    ax = vsetq_lane_f32((vgetq_lane_f32(a->m[0], 2) * vgetq_lane_f32(x, 0) +
                         vgetq_lane_f32(a->m[1], 2) * vgetq_lane_f32(x, 1) +
                         vgetq_lane_f32(a->m[2], 2) * vgetq_lane_f32(x, 2) +
                         vgetq_lane_f32(a->m[3], 2) * vgetq_lane_f32(x, 3)),
                        ax,
                        2);
    ax = vsetq_lane_f32((vgetq_lane_f32(a->m[0], 3) * vgetq_lane_f32(x, 0) +
                         vgetq_lane_f32(a->m[1], 3) * vgetq_lane_f32(x, 1) +
                         vgetq_lane_f32(a->m[2], 3) * vgetq_lane_f32(x, 2) +
                         vgetq_lane_f32(a->m[3], 3) * vgetq_lane_f32(x, 3)),
                        ax,
                        3);
    return ax;
}

void transform_and_light_vertex(vertex *vertex) {
    matrix4x4f32 *projection = &plugin.rdp.projection[plugin.rdp.projection_index];
    matrix4x4f32 *modelview = &plugin.rdp.modelview[plugin.rdp.modelview_index];
    matrix4x4f32 tmp = multiply_matrix4x4f32(*modelview, *projection);
#if 0
    printf("matrix:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
           vgetq_lane_f32(tmp.m[0], 0),
           vgetq_lane_f32(tmp.m[0], 1),
           vgetq_lane_f32(tmp.m[0], 2),
           vgetq_lane_f32(tmp.m[0], 3),
           vgetq_lane_f32(tmp.m[1], 0),
           vgetq_lane_f32(tmp.m[1], 1),
           vgetq_lane_f32(tmp.m[1], 2),
           vgetq_lane_f32(tmp.m[1], 3),
           vgetq_lane_f32(tmp.m[2], 0),
           vgetq_lane_f32(tmp.m[2], 1),
           vgetq_lane_f32(tmp.m[2], 2),
           vgetq_lane_f32(tmp.m[2], 3),
           vgetq_lane_f32(tmp.m[3], 0),
           vgetq_lane_f32(tmp.m[3], 1),
           vgetq_lane_f32(tmp.m[3], 2),
           vgetq_lane_f32(tmp.m[3], 3));
#endif

    float32x4_t position = vcvtq_f32_s32(vmovl_s16(vertex->position));
#if 0
    printf("vertex shading: %f,%f,%f,%f -> ",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    position = multiply_matrix4x4f32_float32x4(&tmp, position);
    //position = multiply_matrix4x4f32_float32x4(projection, position);

#if 0
    printf("%f,%f,%f,%f -> ",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    // Perform perspective division.
    float w = (float)vgetq_lane_f32(position, 3);
    position = vsetq_lane_f32(vgetq_lane_f32(position, 0) / w, position, 0);
    position = vsetq_lane_f32(vgetq_lane_f32(position, 1) / w, position, 1);
    position = vsetq_lane_f32(vgetq_lane_f32(position, 2) / w, position, 2);
    position = vsetq_lane_f32(1.0 / w, position, 3);

    position = vsetq_lane_f32(vgetq_lane_f32(position, 0) * (FRAMEBUFFER_WIDTH / 2), position, 0);
    position = vsetq_lane_f32(vgetq_lane_f32(position, 1) * (FRAMEBUFFER_HEIGHT / 2), position, 1);

#if 0
    printf("%f,%f,%f,%f\n",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    vertex->position = vmovn_s32(vcvtq_s32_f32(position));
}

int32_t op_noop(display_item *item) {
    return 0;
}

int32_t op_set_matrix(display_item *item) {
    bool projection = item->arg8 & 1;
    bool set = (item->arg8 >> 1) & 1;
    bool push = (item->arg8 >> 2) & 1;
    uint32_t addr = segment_address(item->arg32);
    printf("set matrix(%08x, %s, %s, %s)\n",
           addr,
           projection ? "PROJECTION" : "MODELVIEW",
           set ? "SET" : "MULTIPLY",
           push ? "PUSH" : "OVERWRITE");

    matrix4x4f32 new_matrix = load_matrix(addr);

#if 0
    printf("set %s loading:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
           projection ? "projection" : "modelview",
           vgetq_lane_f32(new_matrix.m[0], 0),
           vgetq_lane_f32(new_matrix.m[0], 1),
           vgetq_lane_f32(new_matrix.m[0], 2),
           vgetq_lane_f32(new_matrix.m[0], 3),
           vgetq_lane_f32(new_matrix.m[1], 0),
           vgetq_lane_f32(new_matrix.m[1], 1),
           vgetq_lane_f32(new_matrix.m[1], 2),
           vgetq_lane_f32(new_matrix.m[1], 3),
           vgetq_lane_f32(new_matrix.m[2], 0),
           vgetq_lane_f32(new_matrix.m[2], 1),
           vgetq_lane_f32(new_matrix.m[2], 2),
           vgetq_lane_f32(new_matrix.m[2], 3),
           vgetq_lane_f32(new_matrix.m[3], 0),
           vgetq_lane_f32(new_matrix.m[3], 1),
           vgetq_lane_f32(new_matrix.m[3], 2),
           vgetq_lane_f32(new_matrix.m[3], 3));
#endif

    if (projection) {
        if (plugin.rdp.projection_index < 15 && push) {
            plugin.rdp.projection[plugin.rdp.projection_index + 1] =
                plugin.rdp.projection[plugin.rdp.projection_index];
            plugin.rdp.projection_index++;
        }
        if (!set) {
            new_matrix = multiply_matrix4x4f32(new_matrix,
                                               plugin.rdp.projection[plugin.rdp.projection_index]);
#if 0
            printf("set projection post-mult:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
                   vgetq_lane_f32(new_matrix.m[0], 0),
                   vgetq_lane_f32(new_matrix.m[0], 1),
                   vgetq_lane_f32(new_matrix.m[0], 2),
                   vgetq_lane_f32(new_matrix.m[0], 3),
                   vgetq_lane_f32(new_matrix.m[1], 0),
                   vgetq_lane_f32(new_matrix.m[1], 1),
                   vgetq_lane_f32(new_matrix.m[1], 2),
                   vgetq_lane_f32(new_matrix.m[1], 3),
                   vgetq_lane_f32(new_matrix.m[2], 0),
                   vgetq_lane_f32(new_matrix.m[2], 1),
                   vgetq_lane_f32(new_matrix.m[2], 2),
                   vgetq_lane_f32(new_matrix.m[2], 3),
                   vgetq_lane_f32(new_matrix.m[3], 0),
                   vgetq_lane_f32(new_matrix.m[3], 1),
                   vgetq_lane_f32(new_matrix.m[3], 2),
                   vgetq_lane_f32(new_matrix.m[3], 3));
#endif
        }
        plugin.rdp.projection[plugin.rdp.projection_index] = new_matrix;
    } else {
        if (plugin.rdp.modelview_index < 15 && push) {
            plugin.rdp.modelview[plugin.rdp.modelview_index + 1] =
                plugin.rdp.modelview[plugin.rdp.modelview_index];
            plugin.rdp.modelview_index++;
        }
        if (!set) {
            new_matrix = multiply_matrix4x4f32(new_matrix,
                                               plugin.rdp.modelview[plugin.rdp.modelview_index]);
#if 0
            printf("set modelview post-mult:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
                   vgetq_lane_f32(new_matrix.m[0], 0),
                   vgetq_lane_f32(new_matrix.m[0], 1),
                   vgetq_lane_f32(new_matrix.m[0], 2),
                   vgetq_lane_f32(new_matrix.m[0], 3),
                   vgetq_lane_f32(new_matrix.m[1], 0),
                   vgetq_lane_f32(new_matrix.m[1], 1),
                   vgetq_lane_f32(new_matrix.m[1], 2),
                   vgetq_lane_f32(new_matrix.m[1], 3),
                   vgetq_lane_f32(new_matrix.m[2], 0),
                   vgetq_lane_f32(new_matrix.m[2], 1),
                   vgetq_lane_f32(new_matrix.m[2], 2),
                   vgetq_lane_f32(new_matrix.m[2], 3),
                   vgetq_lane_f32(new_matrix.m[3], 0),
                   vgetq_lane_f32(new_matrix.m[3], 1),
                   vgetq_lane_f32(new_matrix.m[3], 2),
                   vgetq_lane_f32(new_matrix.m[3], 3));
#endif
        }
        plugin.rdp.modelview[plugin.rdp.modelview_index] = new_matrix;
    }

    return 0;
}

int32_t op_vertex(display_item *item) {
    uint8_t count = item->arg8 >> 4;
    uint8_t start_index = item->arg8 & 0xf;
    uint32_t addr = segment_address(item->arg32);
    printf("vertex(%d, %d, %08x)\n", (int)start_index, (int)count, addr);
    for (uint8_t i = 0; i < count; i++) {
        struct rdp_vertex *rdp_vertex = (struct rdp_vertex *)(&plugin.memory.rdram[addr]);
        vertex *vertex = &plugin.rdp.vertices[i];

        int16x4_t position = vdup_n_s16(0);
        position = vset_lane_s16(rdp_vertex->x, position, 0);
        position = vset_lane_s16(rdp_vertex->y, position, 1);
        position = vset_lane_s16(rdp_vertex->z, position, 2);
        position = vset_lane_s16(1, position, 3);
        vertex->position = position;

        vertex->t = rdp_vertex->t;
        vertex->s = rdp_vertex->s;
        vertex->rgba = rdp_vertex->rgba;

        transform_and_light_vertex(&plugin.rdp.vertices[i]);
    }
    return 0;
}

int32_t op_call_display_list(display_item *item) {
    printf("call display list\n");
    uint32_t addr = segment_address(item->arg32);
    if (item->arg8 == DLIST_CALL) {
        interpret_display_list(addr);
        return 0;
    }

    printf("jump display list\n");
    int32_t new_pc = addr;
    int32_t old_pc = (uint32_t)((uintptr_t)&item[1] - (uintptr_t)&plugin.memory.rdram);
    return new_pc - old_pc;
}

int32_t op_draw_triangle(display_item *item) {
    printf("draw triangle\n");
    return 0;
}

int32_t op_pop_matrix(display_item *item) {
    bool projection = item->arg32 & 1;
    if (projection) {
        if (plugin.rdp.projection_index > 0)
            plugin.rdp.projection_index--;
    } else {
        if (plugin.rdp.modelview_index > 0)
            plugin.rdp.modelview_index--;
    }
    printf("pop matrix(%s) = %d\n",
           projection ? "PROJECTION" : "MODELVIEW",
           projection ? plugin.rdp.projection_index : plugin.rdp.modelview_index);
    return 0;
}

int32_t op_move_word(display_item *item) {
    uint8_t type = item->arg16 & 0xff;
    printf("move word(%d) %08x %08x\n", (int)type, ((uint32_t *)item)[0], ((uint32_t *)item)[1]);
    switch (type) {
    case MOVE_WORD_SEGMENT:
        {
            uint32_t segment = (item->arg16 >> 10) & 0xf;
            uint32_t base = item->arg32 & 0x00ffffff;
            plugin.rdp.segments[segment] = base;
            // printf("segment[%d] = %08x\n", segment, base);
            break;
        }
    }
    return 0;
}

int32_t op_end_display_list(display_item *item) {
    return -1;
}

display_op_t OPS[256] = {
    [0x00] = op_noop,
    [0x01] = op_set_matrix,
    [0x04] = op_vertex,
    [0x06] = op_call_display_list,
    [0xbf] = op_draw_triangle,
    [0xbd] = op_pop_matrix,
    [0xbc] = op_move_word,
    [0xb8] = op_end_display_list,
};

void interpret_display_list(uint32_t pc) {
    //printf("first pc=%08x op=%02x word=%08x\n", list->data_ptr, pc->op, *(uint32_t *)pc);
    while (true) {
        display_item *current = (display_item *)&plugin.memory.rdram[pc];
        display_op_t op = OPS[current->op];
        pc += 8;
        if (op != NULL) {
            int32_t displacement = op(current);
            if (displacement == -1)
                break;
            pc += displacement;
        }
    }
}

void process_display_list(display_list *list) {
    printf("start master DL processing\n");
    interpret_display_list(list->data_ptr);
    printf("end master DL processing\n");
}

