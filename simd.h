// neon64/simd.h

#ifndef SIMD_H
#define SIMD_H

#include <stdint.h>

#ifndef __arm__
typedef float float32x4_t __attribute__((vector_size(16)));
typedef int16_t int16x4_t __attribute__((vector_size(8)));
typedef int16_t int16x8_t __attribute__((vector_size(16)));
typedef int32_t int32x4_t __attribute__((vector_size(16)));
typedef uint16_t uint16x4_t __attribute__((vector_size(8)));
typedef uint16_t uint16x8_t __attribute__((vector_size(16)));
typedef uint32_t uint32x4_t __attribute__((vector_size(16)));
#endif

#ifdef __arm__
#include <arm_neon.h>
#else
#include <xmmintrin.h>
#endif

#ifndef __arm__

inline bool is_zero(int16x8_t vector) {
    return (__int128_t)vector == 0;
}

inline int32x4_t vdupq_n_s32(int32_t value) {
    int32x4_t result = {
        value,
        value,
        value,
        value,
    };
    return result;
}

inline int16x4_t vdup_n_s16(int16_t value) {
    int16x4_t result = {
        value,
        value,
        value,
        value,
    };
    return result;
}

inline int16x8_t vdupq_n_s16(int16_t value) {
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

inline float32x4_t vaddq_f32(float32x4_t a, float32x4_t b) {
    return a + b;
}

inline int32x4_t vaddq_s32(int32x4_t a, int32x4_t b) {
    return a + b;
}

inline float32x4_t vmulq_f32(float32x4_t a, float32x4_t b) {
    return a * b;
}

inline int16x8_t vceqq_s16(int16x8_t a, int16x8_t b) {
    return a == b;
}

inline int16x8_t vcltq_s16(int16x8_t a, int16x8_t b) {
    return a < b;
}

inline int16x8_t vcgeq_s16(int16x8_t a, int16x8_t b) {
    return a >= b;
}

inline int32x4_t vcgeq_s32(int32x4_t a, int32x4_t b) {
    return a >= b;
}

inline int16x8_t vandq_s16(int16x8_t a, int16x8_t b) {
    return a & b;
}

inline int16x8_t vandq_n_s16(int16x8_t vector, int16_t value) {
    for (int i = 0; i < 8; i++) {
        if (vector[i] < 0)
            vector[i] = 0;
        else if (vector[i] > 255)
            vector[i] = 255;
    }
    return vector;
}

inline int16x8_t vaddq_s16(int16x8_t a, int16x8_t b) {
    return a + b;
}

inline int16x8_t vmvnq_s16(int16x8_t vector) {
    return ~vector;
}

inline uint16x8_t vmvnq_u16(uint16x8_t vector) {
    return ~vector;
}

inline int16x8_t vorrq_s16(int16x8_t a, int16x8_t b) {
    return a | b;
}

inline int32x4_t vorrq_s32(int32x4_t a, int32x4_t b) {
    return a | b;
}

inline float vgetq_lane_f32(float32x4_t vector, uint8_t index) {
    return vector[index];
}

inline int16_t vget_lane_s16(int16x4_t vector, uint8_t index) {
    return vector[index];
}

inline int16_t vgetq_lane_s16(int16x8_t vector, uint8_t index) {
    return vector[index];
}

inline int32_t vgetq_lane_s32(int32x4_t vector, uint8_t index) {
    return vector[index];
}

inline uint16_t vgetq_lane_u16(uint16x8_t vector, uint8_t index) {
    return vector[index];
}

inline float32x4_t vsetq_lane_f32(float value, float32x4_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

inline int16x4_t vset_lane_s16(int16_t value, int16x4_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

inline int16x8_t vsetq_lane_s16(int16_t value, int16x8_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

inline uint16x8_t vsetq_lane_u16(uint16_t value, uint16x8_t vector, int8_t index) {
    vector[index] = value;
    return vector;
}

inline uint16x8_t vqsubq_u16(uint16x8_t a, uint16x8_t b) {
    return _mm_subs_epu16(a, b);
}

inline float32x4_t vdupq_n_f32(float value) {
    float32x4_t result = {
        value,
        value,
        value,
        value,
    };
    return result;
}

inline uint16x8_t vdupq_n_u16(uint16_t value) {
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

inline float32x4_t vmulq_n_f32(float32x4_t vector, float value) {
    return vector * vdupq_n_f32(value);
}

inline int32x4_t vmulq_n_s32(int32x4_t vector, int32_t value) {
    return vector * vdupq_n_s32(value);
}

inline int32x4_t vmulq_s32(int32x4_t a, int32x4_t b) {
    return a * b;
}

inline float32x4_t vrecpeq_f32(float32x4_t vector) {
    return vdupq_n_f32(1.0) / vector;
}

inline int16x4_t vget_low_s16(int16x8_t vector) {
    int16x4_t result = {
        vector[0],
        vector[1],
        vector[2],
        vector[3],
    };
    return result;
}

inline int16x4_t vget_high_s16(int16x8_t vector) {
    int16x4_t result = {
        vector[4],
        vector[5],
        vector[6],
        vector[7],
    };
    return result;
}

inline int32x4_t vmovl_s16(int16x4_t vector) {
    int32x4_t result = {
        (int32_t)vector[0],
        (int32_t)vector[1],
        (int32_t)vector[2],
        (int32_t)vector[3],
    };
    return result;
}

inline int16x8_t vcombine_s16(int16x4_t low, int16x4_t high) {
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

inline uint16x8_t vcombine_u16(uint16x4_t low, uint16x4_t high) {
    uint16x8_t result = {
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

inline int32x4_t vcvtq_s32_f32(float32x4_t vector) {
    int32x4_t result = {
        (int32_t)vector[0],
        (int32_t)vector[1],
        (int32_t)vector[2],
        (int32_t)vector[3],
    };
    return result;
}

inline float32x4_t vcvtq_f32_s32(int32x4_t vector) {
    float32x4_t result = {
        (float)vector[0],
        (float)vector[1],
        (float)vector[2],
        (float)vector[3],
    };
    return result;
}

inline int16x4_t vmovn_s32(int32x4_t vector) {
    int16x4_t result = {
        (int16_t)vector[0],
        (int16_t)vector[1],
        (int16_t)vector[2],
        (int16_t)vector[3],
    };
    return result;
}

inline uint16x4_t vmovn_u32(uint32x4_t vector) {
    uint16x4_t result = {
        (uint16_t)vector[0],
        (uint16_t)vector[1],
        (uint16_t)vector[2],
        (uint16_t)vector[3],
    };
    return result;
}

inline int16x8_t vshlq_n_s16(int16x8_t vector, uint8_t bits) {
    return vector << vdupq_n_s16(bits);
}

inline int32x4_t vshlq_n_s32(int32x4_t vector, uint8_t bits) {
    return vector << vdupq_n_s32(bits);
}

inline int16x8_t vshrq_n_s16(int16x8_t vector, uint8_t bits) {
    return vector >> vdupq_n_s16(bits);
}

inline int32x4_t vshrq_n_s32(int32x4_t vector, uint8_t bits) {
    return vector >> vdupq_n_s32(bits);
}

inline int32x4_t vrecpeq_u32(int32x4_t vector) {
    // FIXME(tachiweasel): Stub!
    return vector;
}

#endif

#endif

