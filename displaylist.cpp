// neon64/displaylist.cpp

#include "displaylist.h"
#include "plugin.h"
#include "rdp.h"
#include "simd.h"
#include "textures.h"
#include <stdio.h>

#define MOVE_WORD_MATRIX    0
#define MOVE_WORD_CLIP      4
#define MOVE_WORD_SEGMENT   6

#define MOVE_MEM_LIGHT_0    0x86
#define MOVE_MEM_LIGHT_1    0x88
#define MOVE_MEM_LIGHT_2    0x8a
#define MOVE_MEM_MATRIX_1   0x9e

#define DLIST_CALL  0
#define DLIST_JUMP  1

#define RDRAM_SIZE  (4 * 1024 * 1024)

typedef int32_t (*display_op_t)(display_item *item);

struct vec4u8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct vec4i16 {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t w;
};

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

#define LOAD_MATRIX_ELEMENT(result, i, j) \
    do { \
        int high = *((int16_t *)(&plugin.memory.rdram[(address + (i * 8) + (j * 2)) ^ 0x2])); \
        int low = \
            *((uint16_t *)(&plugin.memory.rdram[(address + (i * 8) + (j * 2) + 32) ^ 0x2])); \
        result.m[i] = vsetq_lane_f32((float)((high << 16) | low) * (1.0 / 65536.0), \
                                     result.m[i], \
                                     j); \
    } while(0)

// NB: The address is a flat RDRAM address, not a segmented address.
// TODO(tachiweasel): SIMD-ify this?
matrix4x4f32 load_matrix(uint32_t address) {
    matrix4x4f32 result;
    LOAD_MATRIX_ELEMENT(result, 0, 0);
    LOAD_MATRIX_ELEMENT(result, 0, 1);
    LOAD_MATRIX_ELEMENT(result, 0, 2);
    LOAD_MATRIX_ELEMENT(result, 0, 3);
    LOAD_MATRIX_ELEMENT(result, 1, 0);
    LOAD_MATRIX_ELEMENT(result, 1, 1);
    LOAD_MATRIX_ELEMENT(result, 1, 2);
    LOAD_MATRIX_ELEMENT(result, 1, 3);
    LOAD_MATRIX_ELEMENT(result, 2, 0);
    LOAD_MATRIX_ELEMENT(result, 2, 1);
    LOAD_MATRIX_ELEMENT(result, 2, 2);
    LOAD_MATRIX_ELEMENT(result, 2, 3);
    LOAD_MATRIX_ELEMENT(result, 3, 0);
    LOAD_MATRIX_ELEMENT(result, 3, 1);
    LOAD_MATRIX_ELEMENT(result, 3, 2);
    LOAD_MATRIX_ELEMENT(result, 3, 3);
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

vfloat32x4_t multiply_matrix4x4f32_float32x4(matrix4x4f32 *a, vfloat32x4_t x) {
    vfloat32x4_t ax = { 0.0, 0.0, 0.0, 0.0 };
    ax.x = vgetq_lane_f32(a->m[0], 0) * x.x +
           vgetq_lane_f32(a->m[1], 0) * x.y +
           vgetq_lane_f32(a->m[2], 0) * x.z +
           vgetq_lane_f32(a->m[3], 0) * x.w;
    ax.y = vgetq_lane_f32(a->m[0], 1) * x.x +
           vgetq_lane_f32(a->m[1], 1) * x.y +
           vgetq_lane_f32(a->m[2], 1) * x.z +
           vgetq_lane_f32(a->m[3], 1) * x.w;
    ax.z = vgetq_lane_f32(a->m[0], 2) * x.x +
           vgetq_lane_f32(a->m[1], 2) * x.y +
           vgetq_lane_f32(a->m[2], 2) * x.z +
           vgetq_lane_f32(a->m[3], 2) * x.w;
    ax.w = vgetq_lane_f32(a->m[0], 3) * x.x +
           vgetq_lane_f32(a->m[1], 3) * x.y +
           vgetq_lane_f32(a->m[2], 3) * x.z +
           vgetq_lane_f32(a->m[3], 3) * x.w;
    return ax;
}

void transform_and_light_vertex(vertex *vertex) {
    matrix4x4f32 *projection = &plugin.rdp.projection[plugin.rdp.projection_index];
    matrix4x4f32 *modelview = &plugin.rdp.modelview[plugin.rdp.modelview_index];
    //matrix4x4f32 tmp = multiply_matrix4x4f32(*modelview, *projection);
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

    vfloat32x4_t position = vertex->position;
#if 0
    printf("vertex shading: %f,%f,%f,%f -> ",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    position = multiply_matrix4x4f32_float32x4(modelview, position);
    position = multiply_matrix4x4f32_float32x4(projection, position);

#if 0
    printf("%f,%f,%f,%f -> ",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    // Perform perspective division.
    float w = (float)position.w;
    position.x = position.x / w;
    position.y = position.y / w;
    position.z = position.z / w;
    position.w = 1.0;

#if 0
    printf("vertex: %f,%f,%f,%f\n",
           vgetq_lane_f32(position, 0),
           vgetq_lane_f32(position, 1),
           vgetq_lane_f32(position, 2),
           vgetq_lane_f32(position, 3));
#endif

    vertex->position = position;

    vertex->s >>= 5;
    vertex->t >>= 5;
}

vfloat32x4_t transformed_vertex_position(vertex *vertex) {
    return vertex->position;
}

uint32_t transformed_vertex_color(vertex *vertex) {
    return (((vertex->rgba >> 0) & 0xff) << 24) |
           (((vertex->rgba >> 8) & 0xff) << 16) |
           (((vertex->rgba >> 16) & 0xff) << 8) |
           (((vertex->rgba >> 24) & 0xff) << 0);
}

int32_t op_noop(display_item *item) {
    return 0;
}

int32_t op_set_matrix(display_item *item) {
    bool projection = item->arg8 & 1;
    bool set = (item->arg8 >> 1) & 1;
    bool push = (item->arg8 >> 2) & 1;
    uint32_t addr = segment_address(item->arg32);
#if 0
    printf("set matrix(%08x, %s, %s, %s)\n",
           addr,
           projection ? "PROJECTION" : "MODELVIEW",
           set ? "SET" : "MULTIPLY",
           push ? "PUSH" : "OVERWRITE");
#endif

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
            new_matrix = multiply_matrix4x4f32(
                    new_matrix,
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
            new_matrix = multiply_matrix4x4f32(
                    new_matrix,
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

int32_t op_move_mem(display_item *item) {
    switch (item->arg8) {
    case MOVE_MEM_LIGHT_0:
    case MOVE_MEM_LIGHT_1:
    case MOVE_MEM_LIGHT_2:
        break;
    case MOVE_MEM_MATRIX_1:
        printf("*** move mem matrix 1\n");
        break;
    default:
        break;
    }
    return 0;
}

int32_t op_vertex(display_item *item) {
    uint8_t count = (item->arg8 >> 4) + 1;
    uint8_t start_index = item->arg8 & 0xf;
    uint32_t addr = segment_address(item->arg32);
    //printf("vertex(%d, %d, %08x)\n", (int)start_index, (int)count, addr);
    struct rdp_vertex *base = (struct rdp_vertex *)(&plugin.memory.rdram[addr]);
    for (uint8_t i = 0; i < count; i++) {
        struct rdp_vertex *rdp_vertex = &base[i];
        vertex *vertex = &plugin.rdp.vertices[i];

        vfloat32x4_t position;
        position.x = rdp_vertex->x;
        position.y = rdp_vertex->y;
        position.z = rdp_vertex->z;
        position.w = 1.0;
        vertex->position = position;

        vertex->t = rdp_vertex->t;
        vertex->s = rdp_vertex->s;
        vertex->rgba = rdp_vertex->rgba;

        transform_and_light_vertex(&plugin.rdp.vertices[i]);
    }
    return 0;
}

int32_t op_call_display_list(display_item *item) {
    //printf("call display list\n");
    uint32_t addr = segment_address(item->arg32);
    if (item->arg8 == DLIST_CALL) {
        interpret_display_list(addr);
        return 0;
    }

    int32_t new_pc = addr;
    int32_t old_pc = (uint32_t)((uintptr_t)&item[1] - (uintptr_t)&plugin.memory.rdram[0]);
    //printf("jump display list new_pc=%08x old_pc=%08x\n", new_pc, old_pc);
    return new_pc - old_pc;
}

int32_t op_draw_triangle(display_item *item) {
    uint8_t indices[3] = {
        (uint8_t)((item->arg32 >> 16) / 10),
        (uint8_t)((item->arg32 >> 8) / 10),
        (uint8_t)((item->arg32 >> 0) / 10)
    };
#if 0
    printf("draw triangle(%d,%d,%d) texture=%d\n",
           (int)indices[0],
           (int)indices[1],
           (int)indices[2],
           (int)plugin.rdp.texture_enabled);
#endif
    vertex transformed_vertices[3];
    for (int i = 0; i < 3; i++)
        transformed_vertices[i] = plugin.rdp.vertices[indices[i]];

    triangle triangle;
    triangle.v0.position = transformed_vertex_position(&transformed_vertices[0]);
    triangle.v0.shade = transformed_vertex_color(&transformed_vertices[0]);
    triangle.v1.position = transformed_vertex_position(&transformed_vertices[1]);
    triangle.v1.shade = transformed_vertex_color(&transformed_vertices[1]);
    triangle.v2.position = transformed_vertex_position(&transformed_vertices[2]);
    triangle.v2.shade = transformed_vertex_color(&transformed_vertices[2]);

#if 0
    if (plugin.rdp.texture_enabled)
        plugin.render_state.texture = &plugin.rdp.swizzled_texture;
    else
        plugin.render_state.texture = NULL;
#endif

#if 0
    printf("drawing triangle vertices "
           "%d,%d,%d (s %d, t %d) %d,%d,%d (s %d, t %d) %d,%d,%d (s %d, t %d)\n",
           t.v0.x, t.v0.y, t.v0.z, t.t0.x, t.t0.y,
           t.v1.x, t.v1.y, t.v1.z, t.t1.x, t.t1.y,
           t.v2.x, t.v2.y, t.v2.z, t.t2.x, t.t2.y);
#endif
    add_triangle(&plugin.gl_state, &triangle);
    return 0;
}

int32_t op_draw_texture_rectangle(display_item *item) {
    return 16;
}

int32_t op_draw_flipped_texture_rectangle(display_item *item) {
    return 16;
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
#if 0
    printf("pop matrix(%s) = %d\n",
           projection ? "PROJECTION" : "MODELVIEW",
           projection ? plugin.rdp.projection_index : plugin.rdp.modelview_index);
#endif
    return 0;
}

int32_t op_texture(display_item *item) {
    plugin.rdp.texture_enabled = item->arg16 & 1;
    plugin.rdp.texture_tile = (item->arg16 >> 8) & 0x7;
    uint8_t level = (item->arg16 >> 11) & 0x7;
#if 0
    printf("texture: tile=%d level=%d enabled=%d\n",
           (int)plugin.rdp.texture_tile,
           (int)level,
           (int)plugin.rdp.texture_enabled);
#endif
    return 0;
}

int32_t op_move_word(display_item *item) {
    uint8_t type = item->arg16 & 0xff;
#if 0
    printf("move word(%d) %08x %08x\n", (int)type, ((uint32_t *)item)[0], ((uint32_t *)item)[1]);
#endif
    switch (type) {
    case MOVE_WORD_MATRIX:
        {
            printf("moving matrix word\n");
            break;
        }
    case MOVE_WORD_CLIP:
        {
            printf("moving clip word\n");
            break;
        }
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

int32_t op_load_block(display_item *item) {
#if 0
    // FIXME(tachiweasel): perf test hack
    if (texture_is_active(&plugin.rdp.swizzled_texture))
        return 0;
#endif

    uint8_t tile_index = (item->arg32 >> 24) & 0x7;
    plugin.rdp.texture_upper_left_t = item->arg16 & 0xfff;
    plugin.rdp.texture_upper_left_s =
        (uint16_t)(item->arg16 >> 12) | (uint16_t)(item->arg8 << 4);
    plugin.rdp.texture_lower_right_t = (item->arg32 >> 0) & 0xfff;
    plugin.rdp.texture_lower_right_s = (item->arg32 >> 12) & 0xfff;
#if 0
    printf("load block %d: uls=%d ult=%d lrs=%d lrt=%d\n",
           (int)tile_index,
           plugin.rdp.texture_upper_left_s,
           plugin.rdp.texture_upper_left_t,
           plugin.rdp.texture_lower_right_s,
           plugin.rdp.texture_lower_right_t);
#endif

    if (texture_is_active(&plugin.rdp.swizzled_texture))
        destroy_texture(&plugin.rdp.swizzled_texture);
    plugin.rdp.swizzled_texture = load_texture(tile_index);
    return 0;
}

int32_t op_load_tile(display_item *item) {
    printf("load tile\n");
    return 0;
}

int32_t op_set_tile(display_item *item) {
    int tile_index = (item->arg32 >> 24) & 0x7;
    tile *tile = &plugin.rdp.tiles[tile_index];
    tile->format = (item->arg8 >> 5) & 0x7;
    tile->size = (item->arg8 >> 3) & 0x3;
    tile->addr = item->arg16 & 0x1ff;
    tile->mask_t = (item->arg32 >> 14) & 0xf;
    tile->shift_t = (item->arg32 >> 10) & 0xf;
    tile->mask_s = (item->arg32 >> 4) & 0xf;
    tile->shift_s = (item->arg32 >> 0) & 0xf;
#if 0
    printf("set tile %d: format=%d size=%d addr=%x arg32=%08x mask_t=%d shift_t=%d mask_s=%d shift_s=%d\n",
           tile_index,
           tile->format,
           tile->size,
           tile->addr,
           item->arg32,
           tile->mask_t,
           tile->shift_t,
           tile->mask_s,
           tile->shift_s);
#endif
    return 0;
}

int32_t op_set_fill_color(display_item *item) {
    //printf("set fill color\n");
    return 0;
}

int32_t op_set_blend_color(display_item *item) {
    //printf("set blend color\n");
    return 0;
}

int32_t op_set_prim_color(display_item *item) {
    //printf("set prim color\n");
    return 0;
}

int32_t op_set_env_color(display_item *item) {
    //printf("set env color\n");
    return 0;
}

int32_t op_set_combine(display_item *item) {
    uint32_t mux0 = ((uint32_t)item->arg8 << 16) | (uint32_t)item->arg16;
    uint32_t mux1 = item->arg32;
    printf("set combine(mux0=%08x, mux1=%08x)\n", mux0, mux1);
    return 0;
}

int32_t op_set_texture_image(display_item *item) {
    plugin.rdp.texture_address = segment_address(item->arg32);
    //plugin.rdp.texture_size = (item->arg8 >> 3) & 0x3;
#if 0
    printf("set texture image: addr=%08x\n", segment_address(item->arg32));
#endif
    return 0;
}

int32_t op_end_display_list(display_item *item) {
    return -1;
}

#define SIXTEEN_NO_OPS \
    op_noop, op_noop, op_noop, op_noop, \
    op_noop, op_noop, op_noop, op_noop, \
    op_noop, op_noop, op_noop, op_noop, \
    op_noop, op_noop, op_noop, op_noop  \

display_op_t OPS[256] = {
    op_noop,                            // 00
    op_set_matrix,                      // 01
    op_noop,                            // 02
    op_move_mem,                        // 03
    op_vertex,                          // 04
    op_noop,                            // 05
    op_call_display_list,               // 06
    op_noop,                            // 07
    op_noop,                            // 08
    op_noop,                            // 09
    op_noop,                            // 0a
    op_noop,                            // 0b
    op_noop,                            // 0c
    op_noop,                            // 0d
    op_noop,                            // 0e
    op_noop,                            // 0f
    SIXTEEN_NO_OPS,                     // 10
    SIXTEEN_NO_OPS,                     // 20
    SIXTEEN_NO_OPS,                     // 30
    SIXTEEN_NO_OPS,                     // 40
    SIXTEEN_NO_OPS,                     // 50
    SIXTEEN_NO_OPS,                     // 60
    SIXTEEN_NO_OPS,                     // 70
    SIXTEEN_NO_OPS,                     // 80
    SIXTEEN_NO_OPS,                     // 90
    SIXTEEN_NO_OPS,                     // a0
    op_noop,                            // b0
    op_noop,                            // b1
    op_noop,                            // b2
    op_noop,                            // b3
    op_noop,                            // b4
    op_noop,                            // b5
    op_noop,                            // b6
    op_noop,                            // b7
    op_end_display_list,                // b8
    op_noop,                            // b9
    op_noop,                            // ba
    op_texture,                         // bb
    op_move_word,                       // bc
    op_pop_matrix,                      // bd
    op_noop,                            // be
    op_draw_triangle,                   // bf
    SIXTEEN_NO_OPS,                     // c0
    SIXTEEN_NO_OPS,                     // d0
    op_noop,                            // e0
    op_noop,                            // e1
    op_noop,                            // e2
    op_noop,                            // e3
    op_draw_texture_rectangle,          // e4
    op_draw_flipped_texture_rectangle,  // e5
    op_noop,                            // e6
    op_noop,                            // e7
    op_noop,                            // e8
    op_noop,                            // e9
    op_noop,                            // ea
    op_noop,                            // eb
    op_noop,                            // ec
    op_noop,                            // ed
    op_noop,                            // ee
    op_noop,                            // ef
    op_noop,                            // f0
    op_noop,                            // f1
    op_noop,                            // f2
    op_load_block,                      // f3
    op_load_tile,                       // f4
    op_set_tile,                        // f5
    op_noop,                            // f6
    op_set_fill_color,                  // f7
    op_noop,                            // f8
    op_set_blend_color,                 // f9
    op_set_prim_color,                  // fa
    op_set_env_color,                   // fb
    op_set_combine,                     // fc
    op_set_texture_image,               // fd
    op_noop,                            // fe
    op_noop,                            // ff
};

void interpret_display_list(uint32_t pc) {
    //printf("first pc=%08x op=%02x word=%08x\n", list->data_ptr, pc->op, *(uint32_t *)pc);
    while (true) {
        if (pc > RDRAM_SIZE)
            break;

        display_item *current = (display_item *)&plugin.memory.rdram[pc];
        display_op_t op = OPS[current->op];
        pc += 8;
        if (op != NULL) {
            int32_t displacement = op(current);
            if (displacement == -1)
                break;
            pc += displacement;
            //printf("new pc=%08x\n", pc);
        }
    }
}

void process_display_list(display_list *list) {
    //printf("start master DL processing\n");
    interpret_display_list(list->data_ptr);
    //printf("end master DL processing\n");
}

void foo(void *render_state,
         void *framebuffer_pixels,
         void *z_pixels,
         void *triangle,
         int16x8_t z,
         int16x8_t r,
         int16x8_t g,
         int16x8_t b,
         int16x8_t s,
         int16x8_t t,
         uint16x8_t mask) {}

