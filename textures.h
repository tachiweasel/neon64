// neon64/textures.h

#ifndef TEXTURES_H
#define TEXTURES_H

#include <stdint.h>

#define TEXTURE_FORMAT_RGBA 0
#define TEXTURE_FORMAT_YUV  1
#define TEXTURE_FORMAT_CI   2
#define TEXTURE_FORMAT_IA   3
#define TEXTURE_FORMAT_I    4

#define BPP_4   0
#define BPP_8   1
#define BPP_16  2
#define BPP_32  3

struct swizzled_texture {
    uint32_t *pixels;
    uint16_t width;
    uint16_t height;
    uint32_t hash;
    bool clamp_s;
    bool clamp_t;
};

swizzled_texture load_texture_metadata(uint8_t tile_index);
void load_texture_pixels(swizzled_texture *texture, uint8_t tile_index);
void destroy_texture(swizzled_texture *texture);
bool texture_is_active(swizzled_texture *texture);

#endif

