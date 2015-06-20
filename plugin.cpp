// neon64/plugin.cpp

#include "displaylist.h"
#include "m64p_plugin.h"
#include "m64p_vidext.h"
#include "plugin.h"
#include "rdp.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

struct plugin plugin;

#ifdef __arm__
#define WINDOW_WIDTH    1920
#define WINDOW_HEIGHT   1080
#else
#define WINDOW_WIDTH    (FRAMEBUFFER_WIDTH * 2)
#define WINDOW_HEIGHT   (FRAMEBUFFER_HEIGHT * 2)
#endif

const GLchar *SCALING_VERTEX_SHADER =
#ifdef GLES
    "precision mediump float;\n"
#endif
    "attribute vec2 aPosition;\n"
    "varying vec2 vTextureCoord;\n"
    "void main(void) {\n"
    "   gl_Position = vec4(aPosition.x * 2.0 - 1.0, aPosition.y * 2.0 - 1.0, 1.0, 1.0);\n"
    "   vTextureCoord = aPosition;\n"
    "}\n";

const GLchar *SCALING_FRAGMENT_SHADER =
#ifdef GLES
    "precision mediump float;\n"
#endif
    "varying vec2 vTextureCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main(void) {\n"
    "   gl_FragColor = texture2D(uTexture, vTextureCoord);\n"
    //"   gl_FragColor = vec4(vTextureCoord.x, vTextureCoord.y, 1.0, 1.0);\n"
    "}\n";

const float SCALING_VERTICES[] = {
    0.0, 0.0,
    0.0, 1.0,
    1.0, 0.0,
    1.0, 1.0
};

extern "C" int dummy_gl_function() {
    return 0;
}

#if 0
void *worker_thread_main(void *arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    worker_thread_comm *worker = &plugin.workers[id];
    init_render_state(&plugin.render_state, &worker->framebuffer, id);

    pthread_mutex_lock(&worker->rendering_lock);
    while (true) {
        while (!worker->rendering)
            pthread_cond_wait(&worker->rendering_cond, &worker->rendering_lock);
        pthread_mutex_unlock(&worker->rendering_lock);

        srand(1);

        memset(plugin.render_state.framebuffer.pixels,
               '\0',
               sizeof(uint16_t) * FRAMEBUFFER_WIDTH * SUBFRAMEBUFFER_HEIGHT);
        for (int i = 0; i < FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1); i++)
            plugin.render_state.depth[i] = 0x7fff;
        plugin.render_state.pixels_drawn = 0;
        plugin.render_state.z_buffered_pixels_drawn = 0;
        plugin.render_state.triangles_drawn = 0;

        process_display_list((display_list *)&plugin.memory.dmem[0x0fc0]);

#if 0
        printf("drew %d pixels (%d z-buffered), %d triangles, pixels/triangle %d\n",
               plugin.render_state.pixels_drawn,
               plugin.render_state.z_buffered_pixels_drawn,
               plugin.render_state.triangles_drawn,
               (plugin.render_state.triangles_drawn == 0 ? 0 :
                plugin.render_state.pixels_drawn /
                plugin.render_state.triangles_drawn));
#endif

        pthread_mutex_lock(&worker->rendering_lock);
        worker->rendering = false;
        pthread_cond_signal(&worker->rendering_cond);
    }

    return NULL;
}
#endif

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

void init_plugin_gl_state(plugin_gl_state *plugin_gl_state) {
    DO_GL(glEnable(GL_TEXTURE_2D));
    DO_GL(glGenTextures(1, &plugin_gl_state->texture));
    DO_GL(glBindTexture(GL_TEXTURE_2D, plugin_gl_state->texture));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    DO_GL(glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       FRAMEBUFFER_WIDTH,
                       FRAMEBUFFER_HEIGHT,
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       NULL));

    DO_GL(glGenFramebuffers(1, &plugin_gl_state->framebuffer));
    DO_GL(glBindFramebuffer(GL_FRAMEBUFFER, plugin_gl_state->framebuffer));
    DO_GL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                 GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D,
                                 plugin_gl_state->texture,
                                 0));

    DO_GL(glGenRenderbuffers(1, &plugin_gl_state->renderbuffer));
    DO_GL(glBindRenderbuffer(GL_RENDERBUFFER, plugin_gl_state->renderbuffer));
    DO_GL(glRenderbufferStorage(GL_RENDERBUFFER,
                                GL_DEPTH_COMPONENT16,
                                FRAMEBUFFER_WIDTH,
                                FRAMEBUFFER_HEIGHT));
    DO_GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                    GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER,
                                    plugin_gl_state->renderbuffer));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        abort();
    }

    DO_GL(glBindFramebuffer(GL_FRAMEBUFFER, plugin_gl_state->framebuffer));

    plugin_gl_state->program = GL(glCreateProgram());

    plugin_gl_state->vertex_shader = GL(glCreateShader(GL_VERTEX_SHADER));
    DO_GL(glShaderSource(plugin_gl_state->vertex_shader, 1, &SCALING_VERTEX_SHADER, NULL));
    DO_GL(glCompileShader(plugin_gl_state->vertex_shader));
    check_shader(plugin_gl_state->vertex_shader);

    plugin_gl_state->fragment_shader = GL(glCreateShader(GL_FRAGMENT_SHADER));
    DO_GL(glShaderSource(plugin_gl_state->fragment_shader, 1, &SCALING_FRAGMENT_SHADER, NULL));
    DO_GL(glCompileShader(plugin_gl_state->fragment_shader));
    check_shader(plugin_gl_state->fragment_shader);

    DO_GL(glAttachShader(plugin_gl_state->program, plugin_gl_state->vertex_shader));
    DO_GL(glAttachShader(plugin_gl_state->program, plugin_gl_state->fragment_shader));
    DO_GL(glLinkProgram(plugin_gl_state->program));
    DO_GL(glUseProgram(plugin_gl_state->program));

    plugin_gl_state->position_attribute = GL(glGetAttribLocation(plugin_gl_state->program,
                                                                 "aPosition"));
    plugin_gl_state->texture_uniform = GL(glGetUniformLocation(plugin_gl_state->program,
                                                               "uTexture"));

    DO_GL(glEnableVertexAttribArray(plugin_gl_state->position_attribute));

    DO_GL(glGenBuffers(1, &plugin_gl_state->position_buffer));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, plugin_gl_state->position_buffer));
    DO_GL(glBufferData(GL_ARRAY_BUFFER,
                       sizeof(SCALING_VERTICES),
                       SCALING_VERTICES,
                       GL_STATIC_DRAW));
}

extern "C" int InitiateGFX(GFX_INFO gfx_info) {
    plugin.memory.rdram = gfx_info.RDRAM;
    plugin.memory.dmem = gfx_info.DMEM;
    plugin.registers.mi_intr = gfx_info.MI_INTR_REG;

    SDL_Init(SDL_INIT_VIDEO);

    plugin.sdl_state.window = SDL_CreateWindow("neon64",
                                               SDL_WINDOWPOS_UNDEFINED,
                                               SDL_WINDOWPOS_UNDEFINED,
                                               WINDOW_WIDTH,
                                               WINDOW_HEIGHT,
                                               0);
    SDL_GL_CreateContext(plugin.sdl_state.window);

    init_plugin_gl_state(&plugin.plugin_gl_state);
    init_gl_state(&plugin.gl_state);

    return 1;
}

extern "C" void MoveScreen(int xpos, int ypos) {}

void draw_scaled_scene() {
    DO_GL(glUseProgram(plugin.plugin_gl_state.program));

    DO_GL(glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT));
    DO_GL(glEnable(GL_TEXTURE_2D));
    DO_GL(glDisable(GL_DEPTH_TEST));

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, plugin.plugin_gl_state.position_buffer));
    DO_GL(glVertexAttribPointer(plugin.plugin_gl_state.position_attribute,
                                2,
                                GL_FLOAT,
                                GL_FALSE,
                                0,
                                0));

    DO_GL(glActiveTexture(GL_TEXTURE0 + 0));
    DO_GL(glBindTexture(GL_TEXTURE_2D, plugin.plugin_gl_state.texture));
    DO_GL(glUniform1i(plugin.plugin_gl_state.texture_uniform, 0));
    DO_GL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    DO_GL(glFinish());
}

extern "C" void ProcessDList() {
    uint32_t start_time = SDL_GetTicks();

#if 0
    worker_thread_comm *worker = &plugin.worker;
    pthread_mutex_lock(&worker->rendering_lock);
    worker->rendering = true;
    pthread_cond_signal(&worker->rendering_cond);
    pthread_mutex_unlock(&worker->rendering_lock);

    pthread_mutex_lock(&worker->rendering_lock);
    while (worker->rendering)
        pthread_cond_wait(&worker->rendering_cond, &worker->rendering_lock);
    pthread_cond_signal(&worker->rendering_cond);
    pthread_mutex_unlock(&worker->rendering_lock);

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
#endif

    DO_GL(glBindFramebuffer(GL_FRAMEBUFFER, plugin.plugin_gl_state.framebuffer));
    reset_gl_state(&plugin.gl_state);
    reset_rdp_for_next_frame(&plugin.rdp);
    process_display_list((display_list *)&plugin.memory.dmem[0x0fc0]);
    init_scene(&plugin.gl_state);
    draw_scene(&plugin.gl_state);
    DO_GL(glFlush());

    DO_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    draw_scaled_scene();

    uint32_t end_time = SDL_GetTicks();
    uint32_t elapsed_time = end_time - start_time;
    if (elapsed_time >= 22) {
        printf("rendered frame in %dms -- %dms too slow\n",
               elapsed_time,
               elapsed_time - 22);
    } else {
        printf("rendered frame in %dms\n", elapsed_time);
    }

    SDL_GL_SwapWindow(plugin.sdl_state.window);

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

