// neon64/rasterize.h

#ifndef RASTERIZE_H
#define RASTERIZE_H

#include "simd.h"
#include "textures.h"
#include <stdint.h>

#define FRAMEBUFFER_WIDTH           320
#define FRAMEBUFFER_HEIGHT          240
#define WORKER_THREAD_COUNT         4
#define WORKER_THREAD_COUNT_STRING  "4.0"

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
    uint16_t *pixels;
};

struct render_state {
    framebuffer framebuffer;
    int16_t *depth;
    swizzled_texture *texture;
    uint32_t worker_id;
    uint32_t pixels_drawn;
    uint32_t z_buffered_pixels_drawn;
    uint32_t triangles_drawn;
};

void init_render_state(render_state *render_state, framebuffer *framebuffer, uint32_t worker_id);
void draw_triangle(render_state *render_state, const triangle *t);
void foo(void *render_state,
         void *framebuffer_pixels,
         void *z_pixels,
         void *triangle,
         int16x8_t z,
         int16x8_t r,
         int16x8_t g,
         int16x8_t b,
         int16x8_t s,
         int16x8_t t,
         uint16x8_t mask);

#endif

