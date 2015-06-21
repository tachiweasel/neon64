// neon64/drawgl.h

#ifndef DRAWGL_H
#define DRAWGL_H

#include <stdint.h>
#include "textures.h"

#ifdef __arm__
#include <GLES2/gl2.h>
#else
#include <OpenGL/gl.h>
#endif

#define COMBINE_MODE_COLOR      0x00
#define COMBINE_MODE_SHADE      0x7f
#define COMBINE_MODE_TEXTURE    0xff
#define COMBINE_MODE_SA_SHIFT   24
#define COMBINE_MODE_SB_SHIFT   16
#define COMBINE_MODE_M_SHIFT    8
#define COMBINE_MODE_A_SHIFT    0
#define COMBINE_MODE_MASK       0xff

#if 0
#define GL(cmd) ({ \
    auto _result = (cmd); \
    GLenum _err = glGetError(); \
    if (_err != GL_NO_ERROR) { \
        fprintf(stderr, "GL error at %s:%d: %08x\n", __FILE__, __LINE__, _err); \
    } \
    _result; \
})

#define DO_GL(cmd) \
    do { \
        (cmd); \
        GLenum _err = glGetError(); \
        if (_err != GL_NO_ERROR) { \
            fprintf(stderr, "GL error at %s:%d: %08x\n", __FILE__, __LINE__, _err); \
        } \
    } while(0)
#else
#define GL(cmd) (cmd)
#define DO_GL(cmd) (cmd)
#endif

struct triangle;

struct gl_texture_info {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t id;
    uint32_t hash;
};

struct gl_state {
    triangle *triangles;
    uint32_t triangle_count;
    GLint program;
    GLint vertex_shader;
    GLint fragment_shader;
    GLint position_attribute;
    GLint vertex_index_attribute;
    GLint data_t_attribute;
    GLint data_uniform;
    GLint texture_uniform;
    GLuint position_buffer;
    GLuint vertex_index_buffer;
    GLuint data_t_buffer;
    GLuint data_texture;
    GLuint texture;
    uint32_t *data_texture_buffer;
    uint32_t *texture_buffer;
    gl_texture_info *texture_info;
    uint32_t texture_info_count;
    uint32_t next_texture_id;
    bool texture_buffer_is_dirty;
};

struct vfloat32x2_t {
    float x;
    float y;
};

struct vfloat32x4_t {
    float x;
    float y;
    float z;
    float w;
};

struct triangle_vertex {
    vfloat32x4_t position;
    uint32_t shade;
    uint32_t texture_coord;
};

struct triangle {
    triangle_vertex v0;
    triangle_vertex v1;
    triangle_vertex v2;
    uint32_t sa_color;
    uint32_t sb_color;
    uint32_t m_color;
    uint32_t a_color;
    uint32_t rgb_mode;
    uint32_t a_mode;
    uint32_t texture_bounds;
    float z_base;
    bool z_buffer_enabled;
};

void check_shader(GLint shader);
void init_gl_state(gl_state *gl_state);
void add_triangle(gl_state *gl_state, triangle *triangle);
uint32_t id_of_texture_with_hash(gl_state *gl_state, uint32_t hash);
uint32_t bounds_of_texture_with_id(gl_state *gl_state, uint32_t id);
uint32_t add_texture(gl_state *gl_state, swizzled_texture *texture);
void reset_gl_state(gl_state *gl_state);
void init_scene(gl_state *gl_state);
void draw_scene(gl_state *gl_state);

#endif


