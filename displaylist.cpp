// neon64/displaylist.cpp

#include "displaylist.h"
#include "rdp.h"
#include "simd.h"
#include <stdio.h>

#define MOVE_WORD_SEGMENT   6

#define DLIST_CALL  0
#define DLIST_JUMP  1

typedef int32_t (*display_op_t)(display_item *item);

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
    if (projection) {
        if (plugin.rdp.projection_index < 15 && push) {
            plugin.rdp.projection[plugin.rdp.projection_index + 1] =
                plugin.rdp.projection[plugin.rdp.projection_index];
            plugin.rdp.projection_index++;
        }
        plugin.rdp.projection[plugin.rdp.projection_index] = new_matrix;
    } else {
        if (plugin.rdp.modelview_index < 15 && push) {
            plugin.rdp.modelview[plugin.rdp.modelview_index + 1] =
                plugin.rdp.modelview[plugin.rdp.modelview_index];
            plugin.rdp.modelview_index++;
        }
        plugin.rdp.modelview[plugin.rdp.modelview_index] = new_matrix;
    }

    return 0;
}

int32_t op_vertex(display_item *item) {
    printf("vertex\n");
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

