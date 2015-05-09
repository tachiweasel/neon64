// neon64/textures.h

#ifndef TEXTURES_H
#define TEXTURES_H

#include <stdint.h>

struct swizzled_texture {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
};

swizzled_texture load_texture(uint8_t tile_index);
void destroy_texture(swizzled_texture *texture);
bool texture_is_active(swizzled_texture *texture);

#endif

