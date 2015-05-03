// neon64/rdp.h

#ifndef RDP_H
#define RDP_H

#include <stdint.h>

#define MI_INTR_SP 0x01
#define MI_INTR_DP 0x20

struct memory {
    uint8_t *rdram;
    uint8_t *dmem;
};

struct registers {
    uint32_t *mi_intr;
};

struct plugin {
    memory memory;
    registers registers;
};

extern struct plugin plugin;

inline void send_dp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_DP;
}

inline void send_sp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_SP;
}

#endif

