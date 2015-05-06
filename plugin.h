// neon64/plugin.h

#ifndef PLUGIN_H
#define PLUGIN_H

#include "rasterize.h"
#include "rdp.h"
#include <pthread.h>
#include <stdint.h>

#ifdef __arm__
#include <GLES2/gl2.h>
#else
#include <OpenGL/gl.h>
#endif

struct SDL_Window;

struct worker_thread_comm {
    pthread_t thread;
    framebuffer *framebuffer;
    pthread_mutex_t rendering_lock;
    pthread_cond_t rendering_cond;
    bool rendering;
};

struct gl_state {
    SDL_Window *window;
    GLint program;
    GLint vertex_shader;
    GLint fragment_shader;
    GLint vertex_position_attribute;
    GLuint vertex_position_buffer;
    GLint texture_coord_attribute;
    GLuint texture_coord_buffer;
    GLuint texture;
};

struct plugin {
    memory memory;
    registers registers;
    gl_state gl_state;
    worker_thread_comm workers[WORKER_THREAD_COUNT];
};

struct plugin_thread {
    uint32_t id;
    rdp rdp;
    render_state render_state;
};

extern struct plugin plugin;

extern __thread struct plugin_thread plugin_thread;

#endif

