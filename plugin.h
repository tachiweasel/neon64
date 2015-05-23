// neon64/plugin.h

#ifndef PLUGIN_H
#define PLUGIN_H

#include "drawgl.h"
#include "rdp.h"
#include <pthread.h>
#include <stdint.h>

#ifdef __arm__
#include <GLES2/gl2.h>
#else
#include <OpenGL/gl.h>
#endif

struct SDL_Window;

struct sdl_state {
    SDL_Window *window;
};

struct plugin_gl_state {
    GLuint texture;
    GLuint framebuffer;
    GLuint renderbuffer;
    GLuint position_buffer;
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint position_attribute;
    GLuint texture_uniform;
};

struct plugin {
    memory memory;
    registers registers;
    sdl_state sdl_state;
    gl_state gl_state;
    plugin_gl_state plugin_gl_state;
    rdp rdp;
};

extern struct plugin plugin;

#endif

