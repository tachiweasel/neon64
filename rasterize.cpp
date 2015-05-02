// neon64/rasterize.cpp

#include "rasterize.h"

#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __arm__
#include <arm_neon.h>
#else
#include <xmmintrin.h>
#endif

#ifdef DRAW
#include <SDL2/SDL.h>
#endif

#define WORKER_THREAD_COUNT     4
#define PIXEL_STEP_SIZE         8
#define TEXTURE_WIDTH           64
#define TEXTURE_HEIGHT          64
#define TRIANGLE_COUNT          500000

#define SUBFRAMEBUFFER_HEIGHT   ((FRAMEBUFFER_HEIGHT) / (WORKER_THREAD_COUNT))

#ifndef __arm__
typedef float float32x4_t __attribute__((vector_size(16)));
typedef int16_t int16x4_t __attribute__((vector_size(8)));
typedef int16_t int16x8_t __attribute__((vector_size(16)));
typedef int32_t int32x4_t __attribute__((vector_size(16)));
typedef uint16_t uint16x8_t __attribute__((vector_size(16)));
#endif

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

struct display_list {
    triangle *triangles;
    uint32_t triangles_len;
};

struct texture {
    uint16_t pixels[TEXTURE_WIDTH * TEXTURE_HEIGHT];
};

struct render_state {
    framebuffer *framebuffer;
    int16_t *depth;
    texture *texture;
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
    __asm__("vclt.s16 %q0,%q3,%q4\nvmov.s16 %q1,%q0\nvsli.32 %f1,%e1,#16\nvcmp.f64 %f1,#0\nvmrs APSR_nzcv,FPSCR\nmov %2,#0\nmovgt %2,#1" \
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

#ifndef __arm__

bool is_zero(int16x8_t vector) {
    return (__int128_t)vector == 0;
}

float32x4_t vaddq_f32(float32x4_t a, float32x4_t b) {
    return a + b;
}

int32x4_t vaddq_s32(int32x4_t a, int32x4_t b) {
    return a + b;
}

float32x4_t vmulq_f32(float32x4_t a, float32x4_t b) {
    return a * b;
}

int16x8_t vcltq_s16(int16x8_t a, int16x8_t b) {
    return a < b;
}

int16x8_t vandq_s16(int16x8_t a, int16x8_t b) {
    return a & b;
}

int16x8_t vaddq_s16(int16x8_t a, int16x8_t b) {
    return a + b;
}

uint16x8_t vmvnq_u16(uint16x8_t vector) {
    return ~vector;
}

int16x8_t vorrq_s16(int16x8_t a, int16x8_t b) {
    return a | b;
}

int16_t vgetq_lane_s16(int16x8_t vector, uint8_t index) {
    return vector[index];
}

uint16_t vgetq_lane_u16(uint16x8_t vector, uint8_t index) {
    return vector[index];
}

int16x8_t vsetq_lane_s16(int16_t value, int16x8_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

uint16x8_t vsetq_lane_u16(uint16_t value, uint16x8_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

uint16x8_t vqsubq_u16(uint16x8_t a, uint16x8_t b) {
    return _mm_subs_epu16(a, b);
}

float32x4_t vdupq_n_f32(float value) {
    float32x4_t result = {
        value,
        value,
        value,
        value,
    };
    return result;
}

int32x4_t vdupq_n_s32(int32_t value) {
    int32x4_t result = {
        value,
        value,
        value,
        value,
    };
    return result;
}

int16x8_t vdupq_n_s16(int16_t value) {
    int16x8_t result = {
        value,
        value,
        value,
        value,
        value,
        value,
        value,
        value,
    };
    return result;
}

uint16x8_t vdupq_n_u16(uint16_t value) {
    uint16x8_t result = {
        value,
        value,
        value,
        value,
        value,
        value,
        value,
        value,
    };
    return result;
}

float32x4_t vmulq_n_f32(float32x4_t vector, float value) {
    return vector * vdupq_n_f32(value);
}

int32x4_t vmulq_n_s32(int32x4_t vector, int32_t value) {
    return vector * vdupq_n_s32(value);
}

int32x4_t vmulq_s32(int32x4_t a, int32x4_t b) {
    return a * b;
}

float32x4_t vrecpeq_f32(float32x4_t vector) {
    return vdupq_n_f32(1.0) / vector;
}

int16x4_t vget_low_s16(int16x8_t vector) {
    int16x4_t result = {
        vector[0],
        vector[1],
        vector[2],
        vector[3],
    };
    return result;
}

int16x4_t vget_high_s16(int16x8_t vector) {
    int16x4_t result = {
        vector[4],
        vector[5],
        vector[6],
        vector[7],
    };
    return result;
}

int32x4_t vmovl_s16(int16x4_t vector) {
    int32x4_t result = {
        (int32_t)vector[0],
        (int32_t)vector[1],
        (int32_t)vector[2],
        (int32_t)vector[3],
    };
    return result;
}

int16x8_t vcombine_s16(int16x4_t low, int16x4_t high) {
    int16x8_t result = {
        low[0],
        low[1],
        low[2],
        low[3],
        high[0],
        high[1],
        high[2],
        high[3],
    };
    return result;
}

int32x4_t vcvtq_s32_f32(float32x4_t vector) {
    int32x4_t result = {
        (float)vector[0],
        (float)vector[1],
        (float)vector[2],
        (float)vector[3],
    };
    return result;
}

float32x4_t vcvtq_f32_s32(int32x4_t vector) {
    float32x4_t result = {
        (int32_t)vector[0],
        (int32_t)vector[1],
        (int32_t)vector[2],
        (int32_t)vector[3],
    };
    return result;
}

int16x4_t vmovn_s32(int32x4_t vector) {
    int16x4_t result = {
        (int16_t)vector[0],
        (int16_t)vector[1],
        (int16_t)vector[2],
        (int16_t)vector[3],
    };
    return result;
}

int16x8_t vshlq_n_s16(int16x8_t vector, uint8_t bits) {
    return vector << vdupq_n_s16(bits);
}

int32x4_t vshlq_n_s32(int32x4_t vector, uint8_t bits) {
    return vector << vdupq_n_s32(bits);
}

int16x8_t vshrq_n_s16(int16x8_t vector, uint8_t bits) {
    return vector >> vdupq_n_s16(bits);
}

int32x4_t vshrq_n_s32(int32x4_t vector, uint8_t bits) {
    return vector >> vdupq_n_s32(bits);
}

int32x4_t vrecpeq_u32(int32x4_t vector) {
    // FIXME(pcwalton): Stub!
    return vector;
}

#endif

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

    int32x4_t x2_x0_lowf = vshrq_n_s32(vmulq_n_s32(w1_lowf, x2 - x0), 8);
    int32x4_t x2_x0_highf = vshrq_n_s32(vmulq_n_s32(w1_highf, x2 - x0), 8);
    int16x8_t x2_x0 = vcombine_s16(vmovn_s32(x2_x0_lowf), vmovn_s32(x2_x0_highf));

    return vaddq_s16(vaddq_s16(vdupq_n_s16(x0), x1_x0), x2_x0);
}

int oneify(int value) {
    return value == 0 ? 1 : value;
}

void draw_pixels(render_state *render_state,
                 const vec2i16 *origin,
                 const triangle *triangle,
                 int16x8_t w0,
                 int16x8_t w1,
                 int16x8_t w2,
                 uint16x8_t mask) {
    uint16x8_t pixels = vdupq_n_u16(0);

    int32x4_t w0_lowf = vmovl_s16(vget_low_s16(w0));
    int32x4_t w1_lowf = vmovl_s16(vget_low_s16(w1));
    int32x4_t w2_lowf = vmovl_s16(vget_low_s16(w2));
    int32x4_t wsum_lowf = vaddq_s32(w0_lowf, vaddq_s32(w1_lowf, w2_lowf));
    int32x4_t w0_highf = vmovl_s16(vget_high_s16(w0));
    int32x4_t w1_highf = vmovl_s16(vget_high_s16(w1));
    int32x4_t w2_highf = vmovl_s16(vget_high_s16(w2));
    int32x4_t wsum_highf = vaddq_s32(w0_highf, vaddq_s32(w1_highf, w2_highf));

    wsum_lowf = vshrq_n_s32(vrecpeq_u32(wsum_lowf), 23);
    wsum_highf = vshrq_n_s32(vrecpeq_u32(wsum_highf), 23);

    w0_lowf = vmulq_s32(w0_lowf, wsum_lowf);
    w1_lowf = vmulq_s32(w1_lowf, wsum_lowf);
    w2_lowf = vmulq_s32(w2_lowf, wsum_lowf);
    w0_highf = vmulq_s32(w0_highf, wsum_highf);
    w1_highf = vmulq_s32(w1_highf, wsum_highf);
    w2_highf = vmulq_s32(w2_highf, wsum_highf);

    // Compute index into the pixel/depth buffer.
    int row = (-origin->y / WORKER_THREAD_COUNT) + SUBFRAMEBUFFER_HEIGHT / 2;
    int column = origin->x + FRAMEBUFFER_WIDTH / 2;
    int index = row * FRAMEBUFFER_WIDTH + column;

    // Z-buffer.
    int16x8_t *z_values_ptr = (int16x8_t *)&render_state->depth[index];
    int16x8_t z = lerp_int16x8(triangle->v0.z,
                               triangle->v1.z,
                               triangle->v2.z,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);
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

#ifdef __arm__
    uint16x8_t zero = { 0 };
    __asm__("vceq.s16 %q0,%q1,%q2" : "=w" (pixels) : "w" (mask), "w" (zero));
    mask = ~pixels;
#else
    mask = ~(mask == vdupq_n_u16(0));
#endif

#ifdef COLORING
    int16x8_t r = lerp_int16x8(triangle->c0.r,
                               triangle->c1.r,
                               triangle->c2.r,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);
    int16x8_t g = lerp_int16x8(triangle->c0.g,
                               triangle->c1.g,
                               triangle->c2.g,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);
    int16x8_t b = lerp_int16x8(triangle->c0.b,
                               triangle->c1.b,
                               triangle->c2.b,
                               w1_lowf,
                               w2_lowf,
                               w1_highf,
                               w2_highf);

    r = vshlq_n_s16(vshrq_n_s16(r, 3), 11);
    g = vshlq_n_s16(vshrq_n_s16(g, 2), 5);
    b = vshrq_n_s16(b, 3);
    pixels = vandq_s16(mask, vorrq_s16(vorrq_s16(r, g), b));
#endif

#endif

    uint16x8_t *ptr = (uint16x8_t *)&render_state->framebuffer->pixels[index];
    *ptr = (*ptr & ~mask) | pixels;
}

vec2i16 rand_vec2i16() {
    vec2i16 result;
    result.x = rand();
    result.y = rand();
    return result;
}

inline int16x8_t setup_triangle_edge(triangle_edge *edge,
                                     const vec4i16 *v0,
                                     const vec4i16 *v1,
                                     const vec2i16 *origin) {
    int16_t a = v0->y - v1->y;
    int16_t b = v1->x - v0->x;
    int16_t c = v0->x * v1->y - v0->y * v1->x;

    edge->x_step = vdupq_n_s16(a * PIXEL_STEP_SIZE);
    edge->y_step = vdupq_n_s16(b);

    int16x8_t x = vdupq_n_s16(origin->x);
    int16x8_t addend = { 0, 1, 2, 3, 4, 5, 6, 7 };
    x += addend;
    int16x8_t y = vdupq_n_s16(origin->y);
    return vdupq_n_s16(a) * x + vdupq_n_s16(b) * y + vdupq_n_s16(c);
}

void draw_triangle(render_state *render_state, const triangle *t) {
    const vec4i16 *v0 = &t->v0, *v1 = &t->v1, *v2 = &t->v2;
    int16_t min_x = min3i16(v0->x, v1->x, v2->x), min_y = min3i16(v0->y, v1->y, v2->y);
    int16_t max_x = max3i16(v0->x, v1->x, v2->x), max_y = max3i16(v0->y, v1->y, v2->y);
    min_x = maxi16(min_x, -FRAMEBUFFER_WIDTH / 2) & ~7;
    min_y = maxi16(min_y, -FRAMEBUFFER_HEIGHT / 2);
    max_x = mini16(max_x, FRAMEBUFFER_WIDTH / 2 - 1);
    max_y = mini16(max_y, FRAMEBUFFER_HEIGHT / 2 - 1);

    triangle_edge e01, e12, e20;
    vec2i16 origin = { min_x, min_y };
    int16x8_t w0_row = setup_triangle_edge(&e12, v1, v2, &origin);
    int16x8_t w1_row = setup_triangle_edge(&e20, v2, v0, &origin);
    int16x8_t w2_row = setup_triangle_edge(&e01, v0, v1, &origin);

    int16x8_t zero = vdupq_n_s16(0);

    for (int16_t y = min_y; y <= max_y; y += WORKER_THREAD_COUNT) {
        int16x8_t w0 = w0_row, w1 = w1_row, w2 = w2_row;
        for (int16_t x = min_x; x <= max_x; x += PIXEL_STEP_SIZE) {
            vec2i16 p = { x, y };
            uint16x8_t mask;
            COMPARE_GE_INT16X8(mask, w0 | w1 | w2, zero, dont_draw);
            draw_pixels(render_state, &p, t, w0, w1, w2, mask);

dont_draw:
            w0 += e12.x_step;
            w1 += e20.x_step;
            w2 += e01.x_step;
        }

        w0_row += e12.y_step;
        w1_row += e20.y_step;
        w2_row += e01.y_step;
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
        render_state->framebuffer = new framebuffer;
        memset(render_state->framebuffer->pixels,
               '\0',
               FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1));

        render_state->depth = new int16_t[FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1)];
        for (int j = 0; j < FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1); j++)
            render_state->depth[j] = 0x7fff;

        render_state->texture = new texture;
        for (int j = 0; j < TEXTURE_WIDTH * TEXTURE_HEIGHT; j++)
            render_state->texture->pixels[j] = rand();

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

