// neon64/plugin.cpp

#include "displaylist.h"
#include "m64p_plugin.h"
#include "m64p_vidext.h"
#include "plugin.h"
#include "rasterize.h"
#include "rdp.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdlib.h>

struct plugin plugin;

__thread struct plugin_thread plugin_thread;

#ifdef __arm__
#define WINDOW_WIDTH    1920
#define WINDOW_HEIGHT   1080
#else
#define WINDOW_WIDTH    (FRAMEBUFFER_WIDTH * 2)
#define WINDOW_HEIGHT   (FRAMEBUFFER_HEIGHT * 2)
#endif

#define CHECK_GL(function) \
    do { \
        function; \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) { \
            printf("*** GL error %x on %s\n", err, #function); \
        } \
    } while(0)

const char *VERTEX_SHADER =
    "attribute vec2 aVertexPosition;\n"
    "attribute vec2 aTextureCoord;\n"
    "varying vec2 vTextureCoord;\n"
    "void main(void) {\n"
    "   gl_Position = vec4(aVertexPosition.x, aVertexPosition.y, 0.0, 1.0);\n"
    "   vTextureCoord = aTextureCoord;\n"
    "}\n";

const char *FRAGMENT_SHADER =
#ifdef __arm__
    "precision mediump float;\n"
#endif
    "varying vec2 vTextureCoord;\n"
    "uniform sampler2D uSampler;\n"
    "void main(void) {\n"
    "   float y = floor(vTextureCoord.y * 240.0) / " WORKER_THREAD_COUNT_STRING ";\n"
    "   y = floor(y) / 240.0 + fract(y);\n"
    "   vec4 color = texture2D(uSampler, vec2(vTextureCoord.x, y));\n"
    "   gl_FragColor = color;\n"
    "}\n";

const float VERTICES[8] = {
    -1.0, 1.0,
    1.0,  1.0,
    -1.0, -1.0,
    1.0,  -1.0
};

const float TEXTURE_COORDS[8] = {
    0.0, 0.0,
    1.0, 0.0,
    0.0, 1.0,
    1.0, 1.0
};

extern "C" int dummy_gl_function() {
    return 0;
}

void *worker_thread_main(void *arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    worker_thread_comm *worker = &plugin.workers[id];
    init_render_state(&plugin_thread.render_state, &worker->framebuffer, id);

    pthread_mutex_lock(&worker->rendering_lock);
    while (true) {
        while (!worker->rendering)
            pthread_cond_wait(&worker->rendering_cond, &worker->rendering_lock);
        pthread_mutex_unlock(&worker->rendering_lock);

        srand(1);

        memset(plugin_thread.render_state.framebuffer.pixels,
               '\0',
               sizeof(uint16_t) * FRAMEBUFFER_WIDTH * SUBFRAMEBUFFER_HEIGHT);
        for (int i = 0; i < FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1); i++)
            plugin_thread.render_state.depth[i] = 0x7fff;
        plugin_thread.render_state.pixels_drawn = 0;
        plugin_thread.render_state.z_buffered_pixels_drawn = 0;
        plugin_thread.render_state.triangles_drawn = 0;

        process_display_list((display_list *)&plugin.memory.dmem[0x0fc0]);

#if 0
        printf("drew %d pixels (%d z-buffered), %d triangles, pixels/triangle %d\n",
               plugin_thread.render_state.pixels_drawn,
               plugin_thread.render_state.z_buffered_pixels_drawn,
               plugin_thread.render_state.triangles_drawn,
               (plugin_thread.render_state.triangles_drawn == 0 ? 0 :
                plugin_thread.render_state.pixels_drawn /
                plugin_thread.render_state.triangles_drawn));
#endif

        pthread_mutex_lock(&worker->rendering_lock);
        worker->rendering = false;
        pthread_cond_signal(&worker->rendering_cond);
    }

    return NULL;
}

void send_dp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_DP;
}

void send_sp_interrupt() {
    *plugin.registers.mi_intr |= MI_INTR_SP;
}

extern "C" m64p_error PluginStartup(m64p_dynlib_handle handle,
                                    void *context,
                                    void (*debug_callback)(void *, int, const char *)) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error PluginShutdown() {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error PluginGetVersion(m64p_plugin_type *plugin_type,
                                       int *plugin_version,
                                       int *api_version,
                                       const char **plugin_name_ptr,
                                       int *capabilities) {
    if (plugin_type != NULL)
        *plugin_type = M64PLUGIN_GFX;
    if (plugin_version != NULL)
        *plugin_version = 0x00010000;
    if (api_version != NULL)
        *api_version = 0x20200;
    if (plugin_name_ptr != NULL)
        *plugin_name_ptr = "neon64";
    if (capabilities != NULL)
        *capabilities = 0;
    return M64ERR_SUCCESS;
}

extern "C" int ChangeWindow() {
    return 1;
}

extern "C" int InitiateGFX(GFX_INFO gfx_info) {
    plugin.memory.rdram = gfx_info.RDRAM;
    plugin.memory.dmem = gfx_info.DMEM;
    plugin.registers.mi_intr = gfx_info.MI_INTR_REG;

    plugin.pixels = new uint16_t[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT];

    SDL_Init(SDL_INIT_VIDEO);

    plugin.gl_state.window = SDL_CreateWindow("neon64",
                                              SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              WINDOW_WIDTH,
                                              WINDOW_HEIGHT,
                                              0);
    SDL_GL_CreateContext(plugin.gl_state.window);

    CHECK_GL(glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT));
    CHECK_GL(glEnable(GL_TEXTURE_2D));

    plugin.gl_state.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    CHECK_GL(glShaderSource(plugin.gl_state.vertex_shader, 1, &VERTEX_SHADER, NULL));
    CHECK_GL(glCompileShader(plugin.gl_state.vertex_shader));
    plugin.gl_state.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    CHECK_GL(glShaderSource(plugin.gl_state.fragment_shader, 1, &FRAGMENT_SHADER, NULL));
    CHECK_GL(glCompileShader(plugin.gl_state.fragment_shader));

    plugin.gl_state.program = glCreateProgram();
    CHECK_GL(glAttachShader(plugin.gl_state.program, plugin.gl_state.vertex_shader));
    CHECK_GL(glAttachShader(plugin.gl_state.program, plugin.gl_state.fragment_shader));
    CHECK_GL(glLinkProgram(plugin.gl_state.program));
    CHECK_GL(glUseProgram(plugin.gl_state.program));
    char log[4096];
    glGetProgramInfoLog(plugin.gl_state.program, 4096, NULL, log);
    printf("program log: %s\n", log);

    plugin.gl_state.vertex_position_attribute = glGetAttribLocation(plugin.gl_state.program,
                                                                    "aVertexPosition");
    plugin.gl_state.texture_coord_attribute = glGetAttribLocation(plugin.gl_state.program,
                                                                  "aTextureCoord");

    CHECK_GL(glGenBuffers(1, &plugin.gl_state.vertex_position_buffer));
    CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, plugin.gl_state.vertex_position_buffer));
    CHECK_GL(glEnableVertexAttribArray(plugin.gl_state.vertex_position_attribute));
    CHECK_GL(glVertexAttribPointer(plugin.gl_state.vertex_position_attribute,
                                   2,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   0,
                                   NULL));
    CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW));

    CHECK_GL(glGenBuffers(1, &plugin.gl_state.texture_coord_buffer));
    CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, plugin.gl_state.texture_coord_buffer));
    CHECK_GL(glEnableVertexAttribArray(plugin.gl_state.texture_coord_attribute));
    CHECK_GL(glVertexAttribPointer(plugin.gl_state.texture_coord_attribute,
                                   2,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   0,
                                   NULL));
    CHECK_GL(glBufferData(GL_ARRAY_BUFFER,
                          sizeof(TEXTURE_COORDS),
                          TEXTURE_COORDS,
                          GL_STATIC_DRAW));

    CHECK_GL(glGenTextures(1, &plugin.gl_state.texture));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, plugin.gl_state.texture));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D,
                          0,
                          GL_RGB,
                          FRAMEBUFFER_WIDTH,
                          FRAMEBUFFER_HEIGHT,
                          0,
                          GL_RGB,
                          GL_UNSIGNED_SHORT_5_6_5,
                          NULL));

    for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
        worker_thread_comm *worker = &plugin.workers[i];
        worker->framebuffer.pixels = &plugin.pixels[FRAMEBUFFER_WIDTH * SUBFRAMEBUFFER_HEIGHT * i];
        pthread_mutex_init(&worker->rendering_lock, NULL);
        pthread_cond_init(&worker->rendering_cond, NULL);
        worker->rendering = false;
        pthread_create(&worker->thread, NULL, worker_thread_main, (void *)(uintptr_t)i);
    }

    return 1;
}

extern "C" void MoveScreen(int xpos, int ypos) {}

extern "C" void ProcessDList() {
    uint32_t start_time = SDL_GetTicks();

    for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
        worker_thread_comm *worker = &plugin.workers[i];
        pthread_mutex_lock(&worker->rendering_lock);
        worker->rendering = true;
        pthread_cond_signal(&worker->rendering_cond);
        pthread_mutex_unlock(&worker->rendering_lock);
    }
    for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
        worker_thread_comm *worker = &plugin.workers[i];
        pthread_mutex_lock(&worker->rendering_lock);
        while (worker->rendering)
            pthread_cond_wait(&worker->rendering_cond, &worker->rendering_lock);
        pthread_cond_signal(&worker->rendering_cond);
        pthread_mutex_unlock(&worker->rendering_lock);
    }

    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, plugin.gl_state.texture));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D,
                          0,
                          GL_RGB,
                          FRAMEBUFFER_WIDTH,
                          FRAMEBUFFER_HEIGHT,
                          0,
                          GL_RGB,
                          GL_UNSIGNED_SHORT_5_6_5,
                          plugin.pixels));
    CHECK_GL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    uint32_t end_time = SDL_GetTicks();
    uint32_t elapsed_time = end_time - start_time;
    if (elapsed_time >= 22) {
        printf("rendered frame in %dms -- %dms too slow\n",
               elapsed_time,
               elapsed_time - 22);
    } else {
        printf("rendered frame in %dms\n", elapsed_time);
    }

    SDL_GL_SwapWindow(plugin.gl_state.window);

    send_dp_interrupt();
    send_sp_interrupt();
}

extern "C" void ProcessRDPList() {}

extern "C" void RomClosed() {}

extern "C" int RomOpen() {
    return 1;
}

extern "C" void ShowCFB() {}

extern "C" void UpdateScreen() {}

extern "C" void ViStatusChanged() {}

extern "C" void ViWidthChanged() {}

extern "C" void ReadScreen2(void *dest, int *width, int *height, int front) {}

extern "C" void SetRenderingCallback(void (*callback)(int)) {}

extern "C" void FBRead(unsigned addr) {}

extern "C" void FBWrite(unsigned addr, unsigned size) {}

extern "C" void FBGetFrameBufferInfo(void *ptr) {}

extern "C" void ResizeVideoOutput(int width, int height) {}

extern "C" void VidExt_Init() {}

extern "C" void VidExt_Quit() {}

extern "C" m64p_error VidExt_ListFullscreenModes(m64p_2d_size *size, int *len) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_SetVideoMode(int, int, int, m64p_video_mode, m64p_video_flags) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_ResizeWindow(int, int) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_SetCaption(const char *) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_ToggleFullScreen() {
    return M64ERR_SUCCESS;
}

extern "C" void *VidExt_GL_GetProcAddress(const char *) {
    return (void *)dummy_gl_function;
}

extern "C" m64p_error VidExt_GL_SetAttribute(m64p_GLattr, int) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_GL_GetAttribute(m64p_GLattr, int *) {
    return M64ERR_SUCCESS;
}

extern "C" m64p_error VidExt_GL_SwapBuffers() {
    return M64ERR_SUCCESS;
}

