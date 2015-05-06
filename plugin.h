// neon64/plugin.h

#ifndef PLUGIN_H
#define PLUGIN_H

#include "rasterize.h"
#include "rdp.h"
#include <pthread.h>
#include <stdint.h>

struct worker_thread_comm {
    pthread_t thread;
    framebuffer *framebuffer;
    pthread_mutex_t rendering_lock;
    pthread_cond_t rendering_cond;
    bool rendering;
};

struct plugin {
    memory memory;
    registers registers;
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

