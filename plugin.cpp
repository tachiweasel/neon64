// neon64/plugin.cpp

#include "m64p_plugin.h"
#include "m64p_vidext.h"
#include "rasterize.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

extern "C" int dummy_gl_function() {
    return 0;
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
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("neon64",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          FRAMEBUFFER_WIDTH * 2,
                                          FRAMEBUFFER_HEIGHT * 2,
                                          0);
    SDL_GL_CreateContext(window);

    return 1;
}

extern "C" void MoveScreen(int xpos, int ypos) {}

extern "C" void ProcessDList() {}

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

