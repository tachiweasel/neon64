// neon64/rdp.h

#include "rdp.h"

void reset_rdp_for_next_frame(rdp *rdp) {
    rdp->z_base = MAX_Z_BASE;
}

