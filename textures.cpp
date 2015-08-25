// neon64/textures.cpp

#include "plugin.h"
#include "rdp.h"
#include "textures.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const uint32_t SBOX_TABLE[256] = {
	0x4660c395, 0x3baba6c5, 0x27ec605b, 0xdfc1d81a, 0xaaac4406, 0x3783e9b8, 0xa4e87c68, 0x62dc1b2a,
	0xa8830d34, 0x10a56307, 0x4ba469e3, 0x54836450, 0x1b0223d4, 0x23312e32, 0xc04e13fe, 0x3b3d61fa,
	0xdab2d0ea, 0x297286b1, 0x73dbf93f, 0x6bb1158b, 0x46867fe2, 0xb7fb5313, 0x3146f063, 0x4fd4c7cb,
	0xa59780fa, 0x9fa38c24, 0x38c63986, 0xa0bac49f, 0xd47d3386, 0x49f44707, 0xa28dea30, 0xd0f30e6d,
	0xd5ca7704, 0x934698e3, 0x1a1ddd6d, 0xfa026c39, 0xd72f0fe6, 0x4d52eb70, 0xe99126df, 0xdfdaed86,
	0x4f649da8, 0x427212bb, 0xc728b983, 0x7ca5d563, 0x5e6164e5, 0xe41d3a24, 0x10018a23, 0x5a12e111,
	0x999ebc05, 0xf1383400, 0x50b92a7c, 0xa37f7577, 0x2c126291, 0x9daf79b2, 0xdea086b1, 0x85b1f03d,
	0x598ce687, 0xf3f5f6b9, 0xe55c5c74, 0x791733af, 0x39954ea8, 0xafcff761, 0x5fea64f1, 0x216d43b4,
	0xd039f8c1, 0xa6cf1125, 0xc14b7939, 0xb6ac7001, 0x138a2eff, 0x2f7875d6, 0xfe298e40, 0x4a3fad3b,
	0x066207fd, 0x8d4dd630, 0x96998973, 0xe656ac56, 0xbb2df109, 0x0ee1ec32, 0x03673d6c, 0xd20fb97d,
	0x2c09423c, 0x093eb555, 0xab77c1e2, 0x64607bf2, 0x945204bd, 0xe8819613, 0xb59de0e3, 0x5df7fc9a,
	0x82542258, 0xfb0ee357, 0xda2a4356, 0x5c97ab61, 0x8076e10d, 0x48e4b3cc, 0x7c28ec12, 0xb17986e1,
	0x01735836, 0x1b826322, 0x6602a990, 0x7c1cef68, 0xe102458e, 0xa5564a67, 0x1136b393, 0x98dc0ea1,
	0x3b6f59e5, 0x9efe981d, 0x35fafbe0, 0xc9949ec2, 0x62c765f9, 0x510cab26, 0xbe071300, 0x7ee1d449,
	0xcc71beef, 0xfbb4284e, 0xbfc02ce7, 0xdf734c93, 0x2f8cebcd, 0xfeedc6ab, 0x5476ee54, 0xbd2b5ff9,
	0xf4fd0352, 0x67f9d6ea, 0x7b70db05, 0x5a5f5310, 0x482dd7aa, 0xa0a66735, 0x321ae71f, 0x8e8ad56c,
	0x27a509c3, 0x1690b261, 0x4494b132, 0xc43a42a7, 0x3f60a7a6, 0xd63779ff, 0xe69c1659, 0xd15972c8,
	0x5f6cdb0c, 0xb9415af2, 0x1261ad8d, 0xb70a6135, 0x52ceda5e, 0xd4591dc3, 0x442b793c, 0xe50e2dee,
	0x6f90fc79, 0xd9ecc8f9, 0x063dd233, 0x6cf2e985, 0xe62cfbe9, 0x3466e821, 0x2c8377a2, 0x00b9f14e,
	0x237c4751, 0x40d4a33b, 0x919df7e8, 0xa16991a4, 0xc5295033, 0x5c507944, 0x89510e2b, 0xb5f7d902,
	0xd2d439a6, 0xc23e5216, 0xd52d9de3, 0x534a5e05, 0x762e73d4, 0x3c147760, 0x2d189706, 0x20aa0564,
	0xb07bbc3b, 0x8183e2de, 0xebc28889, 0xf839ed29, 0x532278f7, 0x41f8b31b, 0x762e89c1, 0xa1e71830,
	0xac049bfc, 0x9b7f839c, 0x8fd9208d, 0x2d2402ed, 0xf1f06670, 0x2711d695, 0x5b9e8fe4, 0xdc935762,
	0xa56b794f, 0xd8666b88, 0x6872c274, 0xbc603be2, 0x2196689b, 0x5b2b5f7a, 0x00c77076, 0x16bfa292,
	0xc2f86524, 0xdd92e83e, 0xab60a3d4, 0x92daf8bd, 0x1fe14c62, 0xf0ff82cc, 0xc0ed8d0a, 0x64356e4d,
	0x7e996b28, 0x81aad3e8, 0x05a22d56, 0xc4b25d4f, 0x5e3683e5, 0x811c2881, 0x124b1041, 0xdb1b4f02,
	0x5a72b5cc, 0x07f8d94e, 0xe5740463, 0x498632ad, 0x7357ffb1, 0x0dddd380, 0x3d095486, 0x2569b0a9,
	0xd6e054ae, 0x14a47e22, 0x73ec8dcc, 0x004968cf, 0xe0c3a853, 0xc9b50a03, 0xe1b0eb17, 0x57c6f281,
	0xc9f9377d, 0x43e03612, 0x9a0c4554, 0xbb2d83ff, 0xa818ffee, 0xf407db87, 0x175e3847, 0x5597168f,
	0xd3d547a7, 0x78f3157c, 0xfc750f20, 0x9880a1c6, 0x1af41571, 0x95d01dfc, 0xa3968d62, 0xeae03cf8,
	0x02ee4662, 0x5f1943ff, 0x252d9d1c, 0x6b718887, 0xe052f724, 0x4cefa30b, 0xdcc31a00, 0xe4d0024d,
	0xdbb4534a, 0xce01f5c8, 0x0c072b61, 0x5d59736a, 0x60291da4, 0x1fbe2c71, 0x2f11d09c, 0x9dce266a,
};

const uint32_t SBOX_HASH_SEED = 0x31415927;

inline uint32_t sbox_hash(const uint8_t *key, uint32_t len, uint32_t seed) {
	uint32_t h = len + seed;
	for (; len & ~1; len -= 2, key += 2)
		h = (((h ^ SBOX_TABLE[key[0]]) * 3) ^ SBOX_TABLE[key[1]]) * 3;
	if (len & 1)
		h = (h ^ SBOX_TABLE[key[0]]) * 3;
	h += (h >> 22) ^ (h << 4);
	return h;
}

swizzled_texture load_texture_metadata(uint8_t tile_index) {
    swizzled_texture texture;
    tile *tile = &plugin.rdp.tiles[tile_index];

    uint32_t bytes = (plugin.rdp.texture_lower_right_s + 1) << tile->size >> 1;
    uint32_t line = (2047 + plugin.rdp.texture_lower_right_t) /
        plugin.rdp.texture_lower_right_t;
    texture.width = line << 3;
    texture.height = bytes / texture.width;

    // FIXME(tachiweasel): This seems to work in Mario, but is it right?
    if (tile->size > 0)
        texture.width = texture.width >> (tile->size - 1);

    if (tile->format == TEXTURE_FORMAT_IA && tile->size == BPP_4) {
        printf("IA4 width=%d height=%d bytes=%d\n",
               (int)texture.width,
               (int)texture.height,
               (int)bytes);
    }

    uint16_t *origin = (uint16_t *)&plugin.memory.rdram[plugin.rdp.texture_address];
    texture.hash = sbox_hash((uint8_t *)origin, bytes, SBOX_HASH_SEED);

    texture.clamp_s = tile->clamp_s;
    texture.clamp_t = tile->clamp_t;
    return texture;
}

static void load_texture_pixels_ia4(swizzled_texture *texture) {
    uint8_t *origin = (uint8_t *)&plugin.memory.rdram[plugin.rdp.texture_address];
    uint32_t *dest = texture->pixels;
    printf("loading IA4\n");
    for (int32_t y = 0; y < texture->height; y++) {
        for (uint32_t x = 0; x < texture->width; x += 2) {
            uint8_t byte = origin[y * (texture->width / 2) + x / 2];
            dest[0] = ((uint32_t)((byte & 0xe0) >> 5) << 5) |
                ((uint32_t)((byte & 0xe0) >> 5) << 13) |
                ((uint32_t)((byte & 0xe0) >> 5) << 21) |
                ((uint32_t)((byte & 0x10) >> 4) << 31);
            dest[1] = ((uint32_t)((byte & 0x0e) >> 1) << 5) |
                ((uint32_t)((byte & 0x0e) >> 1) << 13) |
                ((uint32_t)((byte & 0x0e) >> 1) << 21) |
                ((uint32_t)((byte & 0x01) >> 0) << 31);

            dest = &dest[2];
        }
    }
}

static void load_texture_pixels_ia8(swizzled_texture *texture) {
    uint8_t *origin = (uint8_t *)&plugin.memory.rdram[plugin.rdp.texture_address];
    uint32_t *dest = texture->pixels;
    for (int32_t y = 0; y < texture->height; y++) {
        for (uint32_t x = 0; x < texture->width; x++) {
            uint8_t byte = origin[(y * texture->width + x) ^ 3];
            *dest = ((uint32_t)((byte & 0xf0) >> 4) << 4) |
                ((uint32_t)((byte & 0xf0) >> 4) << 12) |
                ((uint32_t)((byte & 0xf0) >> 4) << 20) |
                ((uint32_t)((byte & 0x0f) >> 0) << 28);

            dest = &dest[1];
        }
    }
}

static void load_texture_pixels_rgba16(swizzled_texture *texture) {
    uint16_t *origin = (uint16_t *)&plugin.memory.rdram[plugin.rdp.texture_address];
    uint32_t *dest = texture->pixels;
    for (int32_t y = 0; y < texture->height; y++) {
        for (uint32_t x = 0; x < texture->width; x += 2) {
            uint16_t pixel = origin[y * texture->width + x + 1];
#if 0
            uint8_t r = (((pixel >> 1) & 0x1f) << 3);
            uint8_t g = (((pixel >> 6) & 0x1f) << 3);
            uint8_t b = (((pixel >> 11) & 0x1f) << 3);
            fprintf(stderr, "pixel=%d,%d,%d\n", (int)r, (int)g, (int)b);
#endif
            dest[0] = (((pixel >> 1) & 0x1f) << 19) | (((pixel >> 6) & 0x1f) << 11) |
                (((pixel >> 11) & 0x1f) << 3) | ((pixel & 1) ? 0xff000000 : 0);

            pixel = origin[y * texture->width + x];
            dest[1] = (((pixel >> 1) & 0x1f) << 19) | (((pixel >> 6) & 0x1f) << 11) |
                (((pixel >> 11) & 0x1f) << 3) | ((pixel & 1) ? 0xff000000 : 0);

            dest = &dest[2];
        }
    }
}

static void load_texture_pixels_ia16(swizzled_texture *texture) {
    uint16_t *origin = (uint16_t *)&plugin.memory.rdram[plugin.rdp.texture_address];
    uint32_t *dest = texture->pixels;
    for (int32_t y = 0; y < texture->height; y++) {
        for (uint32_t x = 0; x < texture->width; x += 2) {
            uint16_t pixel = origin[y * texture->width + x + 1];

            dest[0] = ((pixel >> 8) << 0) | ((pixel >> 8) << 8) | ((pixel >> 8) << 16) |
                ((pixel & 0xff) << 24);

            pixel = origin[y * texture->width + x];
            dest[1] = ((pixel >> 8) << 0) | ((pixel >> 8) << 8) | ((pixel >> 8) << 16) |
                ((pixel & 0xff) << 24);

            dest = &dest[2];
        }
    }
}

static void clear_texture_pixels(swizzled_texture *texture) {
    uint32_t *dest = texture->pixels;
    for (int32_t y = 0; y < texture->height; y++) {
        for (uint32_t x = 0; x < texture->width; x++) {
            dest[0] = 0xff0000ff;
            dest = &dest[1];
        }
    }
}

void load_texture_pixels(swizzled_texture *texture, uint8_t tile_index) {
    texture->pixels = (uint32_t *)malloc(texture->width * texture->height * sizeof(uint32_t));

    tile *tile = &plugin.rdp.tiles[tile_index];
    printf("loading texture pixels for texture format %d, bpp %d\n", tile->format, tile->size);

	char buf[256];
	snprintf(buf, sizeof(buf), "/tmp/neon64tex%d.tga", (int)texture->hash);
    FILE *f;
    if (tile->size == BPP_16 && tile->format == TEXTURE_FORMAT_IA) {
        f = fopen(buf, "w");
        char header[18] = { 0 };
        header[2] = 2;
        header[12] = texture->width & 0xff;
        header[13] = (texture->width >> 8) & 0xff;
        header[14] = texture->height & 0xff;
        header[15] = (texture->height >> 8) & 0xff;
        header[16] = 24;
        fwrite(header, 1, sizeof(header), f);
        fprintf(stderr, "--- starting %dx%d---\n", (int)texture->width, (int)texture->height);
    }

    // TODO(tachiweasel): CI, I formats.
    switch (tile->size) {
    case BPP_4:
        switch (tile->format) {
        case TEXTURE_FORMAT_IA:
            load_texture_pixels_ia4(texture);
            break;
        default:
            printf("warning: unimplemented 4bpp format: %d\n", (int)tile->format);
            load_texture_pixels_ia4(texture);
        }
        break;
    case BPP_8:
        switch (tile->format) {
        case TEXTURE_FORMAT_IA:
            printf("*** loading ia8!\n");
            load_texture_pixels_ia8(texture);
            break;
        default:
            load_texture_pixels_ia8(texture);
            printf("warning: unimplemented 8bpp format: %d\n", (int)tile->format);
        }
        break;
    default:
        printf("warning: unimplemented bpp: %d\n", (int)tile->size);
        /* fall through */
    case BPP_16:
        switch (tile->format) {
        case TEXTURE_FORMAT_IA:
            load_texture_pixels_ia16(texture);
            //clear_texture_pixels(texture);
            break;
        case TEXTURE_FORMAT_RGBA:
            load_texture_pixels_rgba16(texture);
            break;
        default:
            printf("warning: unimplemented 16bpp format: %d\n", (int)tile->format);
            load_texture_pixels_rgba16(texture);
            break;
        }
    }

    if (tile->size == BPP_16 && tile->format == TEXTURE_FORMAT_IA) {
        for (int32_t y = texture->height - 1; y >= 0; y--) {
            for (int32_t x = 0; x < texture->width; x++) {
                uint32_t color = texture->pixels[y * texture->width + x];
                fputc((int)((color >> 16) & 0xff), f);
                fputc((int)((color >> 8) & 0xff), f);
                fputc((int)((color >> 0) & 0xff), f);
            }
        }
        fclose(f);
        printf("wrote %s, tileindex=%d width=%d height=%d\n",
               buf,
               (int)tile_index,
               texture->width,
               texture->height);
    }
}

void destroy_texture(swizzled_texture *texture) {
    free(texture->pixels);
    texture->pixels = NULL;
}

bool texture_is_active(swizzled_texture *texture) {
    return texture->pixels != NULL;
}

