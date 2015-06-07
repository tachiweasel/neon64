// neon64/textures.h

#ifndef TEXTURES_H
#define TEXTURES_H

#include <stdint.h>

struct swizzled_texture {
    uint32_t *pixels;
    uint16_t width;
    uint16_t height;
    uint32_t hash;
};

swizzled_texture load_texture_metadata(uint8_t tile_index);
void load_texture_pixels(swizzled_texture *texture, uint8_t tile_index);
void destroy_texture(swizzled_texture *texture);
bool texture_is_active(swizzled_texture *texture);

#endif

