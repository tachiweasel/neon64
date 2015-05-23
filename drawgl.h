// neon64/drawgl.h

#ifndef DRAWGL_H
#define DRAWGL_H

#include <stdint.h>

#ifdef __arm__
#include <GLES2/gl2.h>
#else
#include <OpenGL/gl.h>
#endif

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT 240

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
};

struct triangle {
    triangle_vertex v0;
    triangle_vertex v1;
    triangle_vertex v2;
};

void check_shader(GLint shader);
void init_gl_state(gl_state *gl_state);
void add_triangle(gl_state *gl_state, triangle *triangle);
void reset_gl_state(gl_state *gl_state);
void init_scene(gl_state *gl_state);
void draw_scene(gl_state *gl_state);

#endif


