// neon64/rasterize.cpp

#include "rasterize.h"
#include "simd.h"

#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef DRAW
#include <SDL2/SDL.h>
#endif

#define PIXEL_STEP_SIZE         8
#define TRIANGLE_COUNT          500000

struct display_list {
    triangle *triangles;
    uint32_t triangles_len;
};

struct worker_thread_info {
    int16_t id;

    pthread_cond_t finished_cond;
    pthread_mutex_t finished_lock;

    render_state *render_state;
    display_list *display_list;

    uint32_t triangle_count;
};

int16_t mini16(int16_t v0, int16_t v1) {
    return v0 < v1 ? v0 : v1;
}

int16_t maxi16(int16_t v0, int16_t v1) {
    return v0 > v1 ? v0 : v1;
}

int32_t minu32(uint32_t v0, uint32_t v1) {
    return v0 < v1 ? v0 : v1;
}

int16_t min3i16(int16_t v0, int16_t v1, int16_t v2) {
    return mini16(v0, mini16(v1, v2));
}

int16_t max3i16(int16_t v0, int16_t v1, int16_t v2) {
    return maxi16(v0, maxi16(v1, v2));
}

int16_t orient2d(const vec4i16 *a, const vec4i16 *b, const vec2i16 *c) {
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}

struct triangle_edge {
    int32x4_t x_step;
    int32x4_t y_step;
};

struct varying {
    int16x8_t row;
    int16x8_t x_step;
    int16x8_t y_step;
};

#ifdef __arm__

bool is_zero(int16x8_t vector) {
    uint32_t cond;
    __asm__("vsli.32 %f0,%e0,#16\n"
            "vcmp.f64 %f0,#0\n"
            "vmrs APSR_nzcv,FPSCR\n"
            "mov %1,#0\n"
            "moveq %1,#1"
            : "=w" (vector), "=r" (cond)
            :
            : "cc");
    return cond;
}

#define COMPARE_GE_INT16X8(mask, a, b, label) \
    int16x8_t tmp; \
    uint32_t cond; \
    __asm__("vclt.s16 %q0,%q3,%q4\n" \
            "vmov.s16 %q1,%q0\n" \
            "vsli.32 %f1,%e1,#16\n" \
            "vcmp.f64 %f1,#0\n" \
            "vmrs APSR_nzcv,FPSCR\n" \
            "mov %2,#0\n" \
            "movgt %2,#1" \
            : "=w" (mask), "=w" (tmp), "=r" (cond) \
            : "w" (a), "w" (b) \
            : "cc"); \
    if (cond) \
        goto label;
#else
#define COMPARE_GE_INT16X8(mask, a, b, label) \
    do { \
        mask = (a) >= (b); \
        if ((__int128_t)(mask) == 0) { \
            goto label; \
        } \
    } while(0)
#endif

#define SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, index) \
    do { \
        if (vgetq_lane_u16(mask, index)) { \
            int16_t this_texture_index = vgetq_lane_s16(texture_index, index); \
            pixels = vsetq_lane_u16(render_state->texture->pixels[this_texture_index], \
                                    pixels, \
                                    index); \
        } \
    } while (0)

inline int16x8_t lerp_int16x8(int16_t x0,
                              int16_t x1,
                              int16_t x2,
                              int32x4_t w1_lowf,
                              int32x4_t w2_lowf,
                              int32x4_t w1_highf,
                              int32x4_t w2_highf) {
    if (x0 == x1 && x0 == x2)
        return vdupq_n_s16(x0);

    int32x4_t x1_x0_lowf = vshrq_n_s32(vmulq_n_s32(w1_lowf, x1 - x0), 8);
    int32x4_t x1_x0_highf = vshrq_n_s32(vmulq_n_s32(w1_highf, x1 - x0), 8);
    int16x8_t x1_x0 = vcombine_s16(vmovn_s32(x1_x0_lowf), vmovn_s32(x1_x0_highf));

    int32x4_t x2_x0_lowf = vshrq_n_s32(vmulq_n_s32(w2_lowf, x2 - x0), 8);
    int32x4_t x2_x0_highf = vshrq_n_s32(vmulq_n_s32(w2_highf, x2 - x0), 8);
    int16x8_t x2_x0 = vcombine_s16(vmovn_s32(x2_x0_lowf), vmovn_s32(x2_x0_highf));

    return vaddq_s16(vaddq_s16(vdupq_n_s16(x0), x1_x0), x2_x0);
}

int oneify(int value) {
    return value == 0 ? 1 : value;
}

void draw_pixels(render_state *render_state,
                 const vec2i16 *origin,
                 const triangle *triangle,
                 int16x8_t z,
                 int16x8_t r,
                 int16x8_t g,
                 int16x8_t b,
                 uint16x8_t mask) {
    uint16x8_t pixels = vdupq_n_u16(0);

    // Compute index into the pixel/depth buffer.
    int row = (-origin->y / WORKER_THREAD_COUNT) + SUBFRAMEBUFFER_HEIGHT / 2;
    int column = origin->x + FRAMEBUFFER_WIDTH / 2;
    int index = row * FRAMEBUFFER_WIDTH + column;

    // Z-buffer.
    int16x8_t *z_values_ptr = (int16x8_t *)&render_state->depth[index];
    int16x8_t z_values = *z_values_ptr;
    mask = vandq_s16(mask, vcltq_s16(z, z_values));
    if (is_zero(mask))
        return;
    *z_values_ptr = vorrq_s16(vandq_s16(z_values, vmvnq_u16(mask)), vandq_s16(z, mask));

#ifdef TEXTURING
    int16x8_t s = lerp_int16x8(triangle->t0.x,
                               triangle->t1.x,
                               triangle->t2.x,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);
    int16x8_t t = lerp_int16x8(triangle->t0.x,
                               triangle->t1.x,
                               triangle->t2.x,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);
    int16x8_t texture_width_mask = vdupq_n_s16(TEXTURE_WIDTH - 1);
    int16x8_t texture_height_mask = vdupq_n_s16(TEXTURE_HEIGHT - 1);
    s = vandq_s16(s, texture_width_mask);
    t = vandq_s16(t, texture_height_mask);
    int16x8_t texture_index = vaddq_s16(vmulq_n_s16(t, TEXTURE_WIDTH), s);

    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 0);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 1);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 2);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 3);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 4);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 5);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 6);
    SAMPLE_TEXTURE(pixels, mask, render_state, texture_index, 7);
#else

    mask = vmvnq_s16(vceqq_s16(mask, vdupq_n_u16(0)));

#ifdef COLORING
    // FIXME(tachiweasel): Maybe remove the `vandq_n_s16` below if we get better accuracy.
    r = vandq_s16(r, vdupq_n_s16(0xff));
    g = vandq_s16(g, vdupq_n_s16(0xff));
    b = vandq_s16(b, vdupq_n_s16(0xff));
#if 0
    printf("r=%d g=%d b=%d\n",
           (int)vgetq_lane_s16(r, 0),
           (int)vgetq_lane_s16(g, 0),
           (int)vgetq_lane_s16(b, 0));
#endif
    r = vshlq_n_s16(vshrq_n_s16(r, 3), 11);
    g = vshlq_n_s16(vshrq_n_s16(g, 2), 5);
    b = vshrq_n_s16(b, 3);
    pixels = vandq_s16(mask, vorrq_s16(vorrq_s16(r, g), b));
#else
    pixels = mask;
#endif

#endif

    uint16x8_t *ptr = (uint16x8_t *)&render_state->framebuffer.pixels[index];
    *ptr = (*ptr & ~mask) | pixels;
}

vec2i16 rand_vec2i16() {
    vec2i16 result;
    result.x = rand();
    result.y = rand();
    return result;
}

inline void setup_triangle_edge(triangle_edge *edge,
                                const vec4i16 *v0,
                                const vec4i16 *v1,
                                const vec2i16 *origin,
                                int32x4_t *row_low,
                                int32x4_t *row_high) {
    int32_t a = v0->y - v1->y;
    int32_t b = v1->x - v0->x;
    int32_t c = (int32_t)v0->x * (int32_t)v1->y - (int32_t)v0->y * (int32_t)v1->x;

#if 0
    if (c > 0x7fff || c < -0x7fff)
        printf("*** warning, overflow!\n");
#endif

    edge->x_step = vdupq_n_s32(a * PIXEL_STEP_SIZE);
    edge->y_step = vdupq_n_s32(b * WORKER_THREAD_COUNT);

    int32x4_t x_low = vdupq_n_s32(origin->x), x_high = vdupq_n_s32(origin->x + 4);
    int32x4_t addend = { 0, 1, 2, 3 };
    x_low += addend;
    x_high += addend;
    int32x4_t y = vdupq_n_s32(origin->y);

    int32x4_t y_factor = vmulq_n_s32(y, b);
    int32x4_t c_addend = vdupq_n_s32(c);
    *row_low = vaddq_s32(vaddq_s32(vmulq_n_s32(x_low, a), y_factor), c_addend);
    *row_high = vaddq_s32(vaddq_s32(vmulq_n_s32(x_high, a), y_factor), c_addend);
}

inline int16x8_t normalize_triangle_edge(triangle_edge *edge,
                                         int32x4_t w_low,
                                         int32x4_t w_high,
                                         int32_t wsum) {
    float32x4_t wrecip_sumf = vrecpeq_f32(vdupq_n_f32(wsum));

    float32x4_t wlowf = vcvtq_f32_s32(w_low);
    float32x4_t whighf = vcvtq_f32_s32(w_high);
    wlowf = vmulq_f32(vmulq_n_f32(wlowf, 32768.0), wrecip_sumf);
    whighf = vmulq_f32(vmulq_n_f32(whighf, 32768.0), wrecip_sumf);
    int16x8_t normalized_w = vcombine_s16(vmovn_s32(vcvtq_s32_f32(wlowf)),
                                          vmovn_s32(vcvtq_s32_f32(whighf)));

    float32x4_t x_step_lowf = vcvtq_f32_s32(vmovl_s16(vget_low_s16(edge->x_step)));
    float32x4_t x_step_highf = vcvtq_f32_s32(vmovl_s16(vget_high_s16(edge->x_step)));
    x_step_lowf = vmulq_f32(vmulq_n_f32(x_step_lowf, 32768.0), wrecip_sumf);
    x_step_highf = vmulq_f32(vmulq_n_f32(x_step_highf, 32768.0), wrecip_sumf);

    float32x4_t y_step_lowf = vcvtq_f32_s32(vmovl_s16(vget_low_s16(edge->y_step)));
    float32x4_t y_step_highf = vcvtq_f32_s32(vmovl_s16(vget_high_s16(edge->y_step)));
    y_step_lowf = vmulq_f32(vmulq_n_f32(y_step_lowf, 32768.0), wrecip_sumf);
    y_step_highf = vmulq_f32(vmulq_n_f32(y_step_highf, 32768.0), wrecip_sumf);

    return normalized_w;
}

inline void setup_varying(varying *varying,
                          int16_t x0,
                          int16_t x1,
                          int16_t x2,
                          int16x8_t w1_row,
                          int16x8_t w2_row,
                          int16x8_t w1_x_step,
                          int16x8_t w2_x_step,
                          int16x8_t w1_y_step,
                          int16x8_t w2_y_step) {
    int32x4_t w1_row_low = vmovl_s16(vget_low_s16(w1_row));
    int32x4_t w1_row_high = vmovl_s16(vget_high_s16(w1_row));
    int32x4_t w2_row_low = vmovl_s16(vget_low_s16(w2_row));
    int32x4_t w2_row_high = vmovl_s16(vget_high_s16(w2_row));
    w1_row_low = vshrq_n_s32(vmulq_n_s32(w1_row_low, x1 - x0), 15);
    w1_row_high = vshrq_n_s32(vmulq_n_s32(w1_row_high, x1 - x0), 15);
    w2_row_low = vshrq_n_s32(vmulq_n_s32(w2_row_low, x2 - x0), 15);
    w2_row_high = vshrq_n_s32(vmulq_n_s32(w2_row_high, x2 - x0), 15);
    int32x4_t row_low = vaddq_s32(vdupq_n_s32(x0), vaddq_s32(w1_row_low, w2_row_low));
    int32x4_t row_high = vaddq_s32(vdupq_n_s32(x0), vaddq_s32(w1_row_high, w2_row_high));
    varying->row = vcombine_s16(vmovn_s32(row_low), vmovn_s32(row_high));

#if 0
    w1_row = vcombine_s16(vmovn_s32(w1_row_low), vmovn_s32(w1_row_high));
    w2_row = vcombine_s16(vmovn_s32(w2_row_low), vmovn_s32(w2_row_high));
    varying->row = vaddq_s16(vdupq_n_s16(x0), vaddq_s16(w1_row, w2_row));
#endif

    int32x4_t w1_x_step_low = vmovl_s16(vget_low_s16(w1_x_step));
    int32x4_t w1_x_step_high = vmovl_s16(vget_high_s16(w1_x_step));
    int32x4_t w2_x_step_low = vmovl_s16(vget_low_s16(w2_x_step));
    int32x4_t w2_x_step_high = vmovl_s16(vget_high_s16(w2_x_step));
    w1_x_step_low = vshrq_n_s32(vmulq_n_s32(w1_x_step_low, x1 - x0), 15);
    w1_x_step_high = vshrq_n_s32(vmulq_n_s32(w1_x_step_high, x1 - x0), 15);
    w2_x_step_low = vshrq_n_s32(vmulq_n_s32(w2_x_step_low, x2 - x0), 15);
    w2_x_step_high = vshrq_n_s32(vmulq_n_s32(w2_x_step_high, x2 - x0), 15);
    int32x4_t x_step_low = vaddq_s32(w1_x_step_low, w2_x_step_low);
    int32x4_t x_step_high = vaddq_s32(w1_x_step_high, w2_x_step_high);
    varying->x_step = vcombine_s16(vmovn_s32(x_step_low), vmovn_s32(x_step_high));

#if 0
    w1_x_step = vcombine_s16(vmovn_s32(w1_x_step_low), vmovn_s32(w1_x_step_high));
    w2_x_step = vcombine_s16(vmovn_s32(w2_x_step_low), vmovn_s32(w2_x_step_high));
    varying->x_step = vaddq_s16(w1_x_step, w2_x_step);
#endif

    int32x4_t w1_y_step_low = vmovl_s16(vget_low_s16(w1_y_step));
    int32x4_t w1_y_step_high = vmovl_s16(vget_high_s16(w1_y_step));
    int32x4_t w2_y_step_low = vmovl_s16(vget_low_s16(w2_y_step));
    int32x4_t w2_y_step_high = vmovl_s16(vget_high_s16(w2_y_step));
    w1_y_step_low = vshrq_n_s32(vmulq_n_s32(w1_y_step_low, x1 - x0), 15);
    w1_y_step_high = vshrq_n_s32(vmulq_n_s32(w1_y_step_high, x1 - x0), 15);
    w2_y_step_low = vshrq_n_s32(vmulq_n_s32(w2_y_step_low, x2 - x0), 15);
    w2_y_step_high = vshrq_n_s32(vmulq_n_s32(w2_y_step_high, x2 - x0), 15);
    int32x4_t y_step_low = vaddq_s32(w1_y_step_low, w2_y_step_low);
    int32x4_t y_step_high = vaddq_s32(w1_y_step_high, w2_y_step_high);
    varying->y_step = vcombine_s16(vmovn_s32(y_step_low), vmovn_s32(y_step_high));

#if 0
    w1_y_step = vcombine_s16(vmovn_s32(w1_y_step_low), vmovn_s32(w1_y_step_high));
    w2_y_step = vcombine_s16(vmovn_s32(w2_y_step_low), vmovn_s32(w2_y_step_high));
    varying->y_step = vaddq_s16(w1_y_step, w2_y_step);
#endif
}

void draw_triangle(render_state *render_state, const triangle *t) {
    const vec4i16 *v0 = &t->v0, *v1 = &t->v1, *v2 = &t->v2;
    int16_t min_x = min3i16(v0->x, v1->x, v2->x), min_y = min3i16(v0->y, v1->y, v2->y);
    int16_t max_x = max3i16(v0->x, v1->x, v2->x), max_y = max3i16(v0->y, v1->y, v2->y);

#if 0
    min_y = (min_y & ~(WORKER_THREAD_COUNT - 1)) + (min_y > 0 ? -1 : 0) - render_state->worker_id;
#endif

    min_x = maxi16(min_x, -FRAMEBUFFER_WIDTH / 2) & ~7;
    min_y = maxi16(min_y, -FRAMEBUFFER_HEIGHT / 2);
    max_x = mini16(max_x, FRAMEBUFFER_WIDTH / 2 - 1);
    max_y = mini16(max_y, FRAMEBUFFER_HEIGHT / 2 - 1);

    // FIXME(tachiweasel): Cheap clipping hack. Do this properly.
    if (v0->z <= -500.0 || v1->z <= -500.0 || v2->z <= -500.0)
        return;
    if (v0->z == 0.0 || v1->z == 0.0 || v2->z == 0.0)
        return;
#if 0
    if (v0->x < -500.0 || v1->x < -500.0 || v2->x < -500.0)
        return;
    if (v0->x > 500.0 || v1->x > 500.0 || v2->x > 500.0)
        return;
    if (v0->y < -500.0 || v1->y < -500.0 || v2->y < -500.0)
        return;
    if (v0->y > 500.0 || v1->y > 500.0 || v2->y > 500.0)
        return;
#endif

    triangle_edge e01, e12, e20;
    vec2i16 origin = { min_x, min_y };
    int32x4_t w0_row_low, w0_row_high, w1_row_low, w1_row_high, w2_row_low, w2_row_high;
    setup_triangle_edge(&e12, v1, v2, &origin, &w0_row_low, &w0_row_high);
    setup_triangle_edge(&e20, v2, v0, &origin, &w1_row_low, &w1_row_high);
    setup_triangle_edge(&e01, v0, v1, &origin, &w2_row_low, &w2_row_high);

    int32_t wsum = vgetq_lane_s32(w0_row_low, 0) + vgetq_lane_s32(w1_row_low, 0) +
        vgetq_lane_s32(w2_row_low, 0);
    if (wsum == 0)
        wsum = 1;

    normalize_triangle_edge(&e12, w0_row_low, w0_row_high, wsum);
    int16x8_t normalized_w1_row = normalize_triangle_edge(&e20, w1_row_low, w1_row_high, wsum);
    int16x8_t normalized_w2_row = normalize_triangle_edge(&e01, w2_row_low, w2_row_high, wsum);

    varying z_varying;
    setup_varying(&z_varying,
                  t->v0.z,
                  t->v1.z,
                  t->v2.z,
                  normalized_w1_row,
                  normalized_w2_row,
                  e20.x_step,
                  e01.x_step,
                  e20.y_step,
                  e01.y_step);
#if 0
    z_varying.x_step = vdupq_n_s16(0);
    z_varying.y_step = vdupq_n_s16(0);
#endif
    printf("z varying z0=%d z1=%d z2=%d step=(%d,%d)\n",
            (int)t->v0.z,
            (int)t->v1.z,
            (int)t->v2.z,
            (int)z_varying.x_step[0],
            (int)z_varying.y_step[0]);

    varying r_varying;
    setup_varying(&r_varying,
                  t->c0.r,
                  t->c1.r,
                  t->c2.r,
                  normalized_w1_row,
                  normalized_w2_row,
                  e20.x_step,
                  e01.x_step,
                  e20.y_step,
                  e01.y_step);
#if 0
    printf("r varying c0.r=%d c1.r=%d c2.r=%d step=(%d,%d)\n",
           (int)t->c0.r,
           (int)t->c1.r,
           (int)t->c2.r,
           (int)vgetq_lane_s16(r_varying.x_step, 0),
           (int)vgetq_lane_s16(r_varying.y_step, 0));
#endif

    varying g_varying;
    setup_varying(&g_varying,
                  t->c0.g,
                  t->c1.g,
                  t->c2.g,
                  normalized_w1_row,
                  normalized_w2_row,
                  e20.x_step,
                  e01.x_step,
                  e20.y_step,
                  e01.y_step);

    varying b_varying;
    setup_varying(&b_varying,
                  t->c0.b,
                  t->c1.b,
                  t->c2.b,
                  normalized_w1_row,
                  normalized_w2_row,
                  e20.x_step,
                  e01.x_step,
                  e20.y_step,
                  e01.y_step);

    r_varying.row = vdupq_n_s16(t->c0.r);
    g_varying.row = vdupq_n_s16(t->c0.g);
    b_varying.row = vdupq_n_s16(t->c0.b);
    r_varying.x_step = vdupq_n_s16(0);
    r_varying.y_step = vdupq_n_s16(0);
    g_varying.x_step = vdupq_n_s16(0);
    g_varying.y_step = vdupq_n_s16(0);
    b_varying.x_step = vdupq_n_s16(0);
    b_varying.y_step = vdupq_n_s16(0);

    int32x4_t zero = vdupq_n_s32(0);

    for (int16_t y = min_y; y <= max_y; y += WORKER_THREAD_COUNT) {
        int32x4_t w0_low = w0_row_low, w0_high = w0_row_high;
        int32x4_t w1_low = w1_row_low, w1_high = w1_row_high;
        int32x4_t w2_low = w2_row_low, w2_high = w2_row_high;
        int16x8_t z = z_varying.row;
        int16x8_t r = r_varying.row, g = g_varying.row, b = b_varying.row;
        for (int16_t x = min_x; x <= max_x; x += PIXEL_STEP_SIZE) {
            vec2i16 p = { x, y };
            // FIXME(tachiweasel): I think this is slow.
            uint32x4_t mask_low = vcgeq_s32(vorrq_s32(w0_low, vorrq_s32(w1_low, w2_low)), zero);
            uint32x4_t mask_high =
                vcgeq_s32(vorrq_s32(w0_high, vorrq_s32(w1_high, w2_high)), zero);
#if 0
            int16x8_t w0 = vcombine_s16(vmovn_s32(w0_low), vmovn_s32(w0_high));
            int16x8_t w1 = vcombine_s16(vmovn_s32(w1_low), vmovn_s32(w1_high));
            int16x8_t w2 = vcombine_s16(vmovn_s32(w2_low), vmovn_s32(w2_high));
            uint16x8_t mask = vcgeq_s16(vorrq_s16(vorrq_s16(w0, w1), w2), zero);
#endif
            uint16x8_t mask = vcombine_u16(vmovn_u32(mask_low), vmovn_s32(mask_high));

            bool draw = false;

#define TEST(i) \
    do { \
        uint16_t hit = vgetq_lane_u16(mask, i); \
        if (hit) { \
            draw = true; \
        } \
    } while(0)

            TEST(0);
            TEST(1);
            TEST(2);
            TEST(3);
            TEST(4);
            TEST(5);
            TEST(6);
            TEST(7);

            //COMPARE_GE_INT16X8(mask, w0 | w1 | w2, zero, dont_draw);

            if (draw)
                draw_pixels(render_state, &p, t, z, r, g, b, mask);

//dont_draw:
            w0_low += e12.x_step;
            w0_high += e12.x_step;
            w1_low += e20.x_step;
            w1_high += e20.x_step;
            w2_low += e01.x_step;
            w2_high += e01.x_step;
            z += z_varying.x_step;
            r += r_varying.x_step;
            g += g_varying.x_step;
            b += b_varying.x_step;
        }

        w0_row_low += e12.y_step;
        w0_row_high += e12.y_step;
        w1_row_low += e20.y_step;
        w1_row_high += e20.y_step;
        w2_row_low += e01.y_step;
        w2_row_high += e01.y_step;
        z_varying.row += z_varying.y_step;
        r_varying.row += r_varying.y_step;
        g_varying.row += g_varying.y_step;
        b_varying.row += b_varying.y_step;
    }
}

void *worker_thread(void *cookie) {
    worker_thread_info *info = (worker_thread_info *)cookie;

    struct timeval target;
    gettimeofday(&target, NULL);
    target.tv_sec += 3;

    for (uint32_t display_item_index = 0;
         display_item_index < info->display_list->triangles_len;
         display_item_index++) {
        draw_triangle(info->render_state, &info->display_list->triangles[display_item_index]);
        info->triangle_count++;
    }

    pthread_mutex_lock(&info->finished_lock);
    pthread_cond_broadcast(&info->finished_cond);
    pthread_mutex_unlock(&info->finished_lock);
    return NULL;
}

void init_render_state(render_state *render_state, framebuffer *framebuffer, uint32_t worker_id) {
    render_state->framebuffer.pixels = framebuffer->pixels;
    memset(render_state->framebuffer.pixels,
           '\0',
           sizeof(uint16_t) * FRAMEBUFFER_WIDTH * SUBFRAMEBUFFER_HEIGHT);

    render_state->depth = new int16_t[FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1)];
    for (int j = 0; j < FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1); j++)
        render_state->depth[j] = 0x7fff;

    render_state->texture = new texture;
    for (int j = 0; j < TEXTURE_WIDTH * TEXTURE_HEIGHT; j++)
        render_state->texture->pixels[j] = rand();

    render_state->worker_id = worker_id;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: rasterize SCENE\n");
        return 0;
    }
    FILE *f = fopen(argv[1], "r");

    display_list display_list;

    display_list.triangles = new triangle[TRIANGLE_COUNT];
    display_list.triangles_len = 0;

    vec4i16 vertices[3];
    vec4u8 colors[3];
    int vertices_len = 0;
    while (!feof(f)) {
        float x, y, z;
        uint8_t r, g, b, a;
        if (fscanf(f, "%f,%f,%f,%hhd,%hhd,%hhd,%hhd\n", &x, &y, &z, &r, &g, &b, &a) != 7)
            break;

        vertices[vertices_len].x = x * (float)(FRAMEBUFFER_WIDTH / 2);
        vertices[vertices_len].y = y * (float)(FRAMEBUFFER_HEIGHT / 2);
        vertices[vertices_len].z = z;
        vertices[vertices_len].w = 1.0;
        colors[vertices_len].r = r;
        colors[vertices_len].g = g;
        colors[vertices_len].b = b;
        colors[vertices_len].a = a;
        vertices_len++;

        if (vertices_len == 3) {
            display_list.triangles[display_list.triangles_len].v0 = vertices[0];
            display_list.triangles[display_list.triangles_len].v1 = vertices[1];
            display_list.triangles[display_list.triangles_len].v2 = vertices[2];

            display_list.triangles[display_list.triangles_len].c0 = colors[0];
            display_list.triangles[display_list.triangles_len].c1 = colors[1];
            display_list.triangles[display_list.triangles_len].c2 = colors[2];

            display_list.triangles[display_list.triangles_len].t0 = rand_vec2i16();
            display_list.triangles[display_list.triangles_len].t1 = rand_vec2i16();
            display_list.triangles[display_list.triangles_len].t2 = rand_vec2i16();

            display_list.triangles_len++;
            vertices_len = 0;
        }
    }

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    worker_thread_info worker_thread_info[WORKER_THREAD_COUNT];
    for (int16_t i = 0; i < WORKER_THREAD_COUNT; i++) {
        render_state *render_state = new struct render_state;
        init_render_state(render_state, new framebuffer, i);

        worker_thread_info[i].id = i;
        pthread_cond_init(&worker_thread_info[i].finished_cond, NULL);
        pthread_mutex_init(&worker_thread_info[i].finished_lock, NULL);
        worker_thread_info[i].render_state = render_state;
        worker_thread_info[i].display_list = &display_list;
        worker_thread_info[i].triangle_count = 0;

        pthread_mutex_lock(&worker_thread_info[i].finished_lock);
        pthread_t thread;
        pthread_create(&thread, NULL, worker_thread, &worker_thread_info[i]);
    }

    for (int16_t i = 0; i < WORKER_THREAD_COUNT; i++) {
        pthread_cond_wait(&worker_thread_info[i].finished_cond,
                          &worker_thread_info[i].finished_lock);
        pthread_mutex_unlock(&worker_thread_info[i].finished_lock);
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    printf("rendered %d triangles in %fms\n",
           display_list.triangles_len,
           (float)(now.tv_sec - start_time.tv_sec) * 1000.0 +
           (float)(now.tv_usec - start_time.tv_usec) / 1000.0);

#ifdef DRAW
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("neon64",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          FRAMEBUFFER_WIDTH * 2,
                                          FRAMEBUFFER_HEIGHT * 2,
                                          0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGB565,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             FRAMEBUFFER_WIDTH,
                                             FRAMEBUFFER_HEIGHT);
    SDL_UpdateTexture(texture,
                      NULL,
                      worker_thread_info[0].render_state->framebuffer->pixels,
                      FRAMEBUFFER_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_QUIT)
            return 0;
    }
#endif

    return 0;
}

