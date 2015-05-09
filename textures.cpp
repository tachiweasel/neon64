// neon64/textures.cpp

#include "plugin.h"
#include "rdp.h"
#include "textures.h"
#include <stdint.h>
#include <stdio.h>

swizzled_texture load_texture(uint8_t tile_index) {
    swizzled_texture texture;

    tile *tile = &plugin_thread.rdp.tiles[tile_index];
    uint32_t bytes = (plugin_thread.rdp.texture_lower_right_s + 1) << tile->size >> 1;
    uint32_t line = (2047 + plugin_thread.rdp.texture_lower_right_t) /
        plugin_thread.rdp.texture_lower_right_t;
    texture.width = line << 3;
    texture.height = bytes / texture.width;
    texture.width /= 2;
    
    uint16_t *origin = (uint16_t *)&plugin.memory.rdram[plugin_thread.rdp.texture_address];

    texture.pixels = (uint16_t *)malloc(texture.width * texture.height * sizeof(uint16_t));

#if 0
	char buf[256];
	snprintf(buf, sizeof(buf), "/tmp/neon64tex%d.tga", (int)width);
    FILE *f = fopen(buf, "w");
	char header[18] = { 0 };
	header[2] = 2;
	header[12] = width & 0xff;
	header[13] = (width >> 8) & 0xff;
	header[14] = height & 0xff;
	header[15] = (height >> 8) & 0xff;
	header[16] = 24;
	fwrite(header, 1, sizeof(header), f);
#endif

    uint16_t *dest = texture.pixels;
    for (int32_t y = texture.height - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < texture.width; x += 2) {
            uint16_t pixel = origin[y * texture.width + x + 1];
            dest[0] = (((pixel >> 1) & 0x1f) << 11) | (((pixel >> 6) & 0x1f) << 5) |
                ((pixel >> 11) & 0x1f);

            pixel = origin[y * texture.width + x];
            dest[1] = (((pixel >> 1) & 0x1f) << 11) | (((pixel >> 6) & 0x1f) << 5) |
                ((pixel >> 11) & 0x1f);

            dest = &dest[2];
        }
    }

#if 0
    fclose(f);
    printf("wrote %s, tileindex=%d size=%d width=%d height=%d\n",
           buf,
           (int)tile_index,
           tile->size,
           width,
           height);
#endif
    return texture;
}

void destroy_texture(swizzled_texture *texture) {
    free(texture->pixels);
    texture->pixels = NULL;
}

bool texture_is_active(swizzled_texture *texture) {
    return texture->pixels != NULL;
}

