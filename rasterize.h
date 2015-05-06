// neon64/rasterize.h

#ifndef RASTERIZE_H
#define RASTERIZE_H

#include <stdint.h>

#define FRAMEBUFFER_WIDTH       320
#define FRAMEBUFFER_HEIGHT      240
#define TEXTURE_WIDTH           64
#define TEXTURE_HEIGHT          64
#define WORKER_THREAD_COUNT     4

#define SUBFRAMEBUFFER_HEIGHT   ((FRAMEBUFFER_HEIGHT) / (WORKER_THREAD_COUNT))

struct vec2i16 {
    int16_t x;
    int16_t y;
};

struct vec4u8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct vec4i16 {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t w;
};

struct triangle {
    vec4i16 v0;
    vec4i16 v1;
    vec4i16 v2;

    vec4u8 c0;
    vec4u8 c1;
    vec4u8 c2;

    vec2i16 t0;
    vec2i16 t1;
    vec2i16 t2;
};


struct framebuffer {
    uint16_t pixels[FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1)];
};

struct texture {
    uint16_t pixels[TEXTURE_WIDTH * TEXTURE_HEIGHT];
};

struct render_state {
    framebuffer *framebuffer;
    int16_t *depth;
    texture *texture;
};

void init_render_state(render_state *render_state, framebuffer *framebuffer);
void draw_triangle(render_state *render_state, const triangle *t);

#endif

