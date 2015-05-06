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

extern "C" int dummy_gl_function() {
    return 0;
}

void *worker_thread_main(void *arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    worker_thread_comm *worker = &plugin.workers[id];
    init_render_state(&plugin_thread.render_state, worker->framebuffer);

    pthread_mutex_lock(&worker->rendering_lock);
    while (true) {
        while (!worker->rendering)
            pthread_cond_wait(&worker->rendering_cond, &worker->rendering_lock);
        pthread_mutex_unlock(&worker->rendering_lock);

        srand(1);

        memset(plugin_thread.render_state.framebuffer->pixels,
               '\0',
               sizeof(uint16_t) * FRAMEBUFFER_WIDTH * SUBFRAMEBUFFER_HEIGHT);
        for (int i = 0; i < FRAMEBUFFER_WIDTH * (SUBFRAMEBUFFER_HEIGHT + 1); i++)
            plugin_thread.render_state.depth[i] = 0x7fff;

        process_display_list((display_list *)&plugin.memory.dmem[0x0fc0]);

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

SDL_Renderer *renderer;
SDL_Texture *texture;

extern "C" int InitiateGFX(GFX_INFO gfx_info) {
    plugin.memory.rdram = gfx_info.RDRAM;
    plugin.memory.dmem = gfx_info.DMEM;
    plugin.registers.mi_intr = gfx_info.MI_INTR_REG;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("neon64",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          FRAMEBUFFER_WIDTH * 2,
                                          FRAMEBUFFER_HEIGHT * 2,
                                          0);
    SDL_GL_CreateContext(window);

    renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING,
                                FRAMEBUFFER_WIDTH,
                                SUBFRAMEBUFFER_HEIGHT);

    for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
        worker_thread_comm *worker = &plugin.workers[i];
        worker->framebuffer = new framebuffer;
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

    SDL_UpdateTexture(texture,
                      NULL,
                      plugin.workers[0].framebuffer->pixels,
                      FRAMEBUFFER_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    uint32_t end_time = SDL_GetTicks();
    printf("rendered frame in %dms\n", end_time - start_time);

    SDL_RenderPresent(renderer);

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

