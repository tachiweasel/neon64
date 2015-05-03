// neon64/displaylist.cpp

#include "displaylist.h"
#include "rdp.h"
#include <stdio.h>

typedef bool (*display_op_t)(display_item *item);

bool op_noop(display_item *item) {
    return true;
}

bool op_set_matrix(display_item *item) {
    printf("set matrix\n");
    return true;
}

bool op_vertex(display_item *item) {
    printf("vertex\n");
    return true;
}

bool op_call_display_list(display_item *item) {
    //item->arg1
    printf("call display list\n");
    return true;
}

bool op_draw_triangle(display_item *item) {
    printf("draw triangle\n");
    return true;
}

bool op_pop_matrix(display_item *item) {
    printf("pop matrix\n");
    return true;
}

bool op_end_display_list(display_item *item) {
    printf("end display list\n");
    return false;
}

display_op_t OPS[256] = {
    [0x00] = op_noop,
    [0x01] = op_set_matrix,
    [0x04] = op_vertex,
    [0x06] = op_call_display_list,
    [0xbf] = op_draw_triangle,
    [0xbd] = op_pop_matrix,
    [0xb8] = op_end_display_list,
};

void process_display_list(display_list *list) {
    display_item *pc = (display_item *)&plugin.memory.rdram[list->data_ptr];
    while (true) {
        display_op_t op = OPS[pc->op];
        if (op != NULL) {
            if (!op(pc))
                break;
        }
        pc++;
    }
}

