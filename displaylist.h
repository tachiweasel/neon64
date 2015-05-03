// neon64/displaylist.h

#ifndef DISPLAYLIST_H
#define DISPLAYLIST_H

#include <stdint.h>

struct display_list {
    uint32_t type;
    uint32_t flags;

    uint32_t ucode_boot;
    uint32_t ucode_boot_size;

    uint32_t ucode;
    uint32_t ucode_size;

    uint32_t ucode_data;
    uint32_t ucode_data_size;

    uint32_t dram_stack;
    uint32_t dram_stack_size;

    uint32_t output_buf;
    uint32_t output_buf_size;

    uint32_t data_ptr;
    uint32_t data_size;

    uint32_t yield_data_ptr;
    uint32_t yield_data_size;
};

struct display_item {
    uint8_t op;
    uint8_t arg0;
    uint16_t arg1;
    uint32_t w1;
};

void process_display_list(display_list *list);

#endif

