// neon64/drawgl.cpp

#include <OpenGL/gl.h>
#include <SDL2/SDL.h>
#include <stdio.h>

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

struct gl_state {
    GLint program;
    GLint vertex_shader;
    GLint fragment_shader;
    GLint position_attribute;
    GLint vertex_index_attribute;
    GLint data_t_attribute;
    GLint data_uniform;
    GLuint position_buffer;
    GLuint vertex_index_buffer;
    GLuint data_t_buffer;
};

const GLchar *VERTEX_SHADER =
#ifdef GLES
    "precision mediump float;\n"
#endif
    "attribute vec3 aPosition;\n"
    "attribute float aVertexIndex;\n"
    "attribute float aDataT;\n"
    "varying vec3 vLambda;\n"
    "varying float vDataT;\n"
    "void main(void) {\n"
    "   gl_Position = vec4(aPosition, 1.0);\n"
    "   vLambda = vec3(aVertexIndex == 0.0 ? 1.0 : 0.0,\n"
    "                  aVertexIndex == 1.0 ? 1.0 : 0.0,\n"
    "                  aVertexIndex == 2.0 ? 1.0 : 0.0);\n"
    "   vDataT = aDataT;\n"
    "}";

const GLchar *FRAGMENT_SHADER =
#ifdef GLES
    "precision mediump float;\n"
#endif
    "varying vec3 vLambda;\n"
    "varying float vDataT;\n"
    "uniform sampler2D uData;\n"
    "void main(void) {\n"
#if 0
    "   vec4 color0 = texture2D(uData, vec2(0.0 / 16.0, vDataT));"
    "   vec4 color1 = texture2D(uData, vec2(1.0 / 16.0, vDataT));"
    "   vec4 color2 = texture2D(uData, vec2(2.0 / 16.0, vDataT));"
    "   gl_FragColor = vLambda[0] * color0 + vLambda[1] * color1 + vLambda[2] * color2;"
#endif
    "   gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "}\n";

void init_buffers(gl_state *gl_state) {
    DO_GL(glGenBuffers(1, &gl_state->position_buffer));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));
    float triangle_vertices[9] = {
        0.0, 1.0, 0.0,
        -1.0, -1.0, 0.0,
        1.0, -1.0, 0.0
    };
    DO_GL(glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), triangle_vertices, GL_STATIC_DRAW));

    DO_GL(glGenBuffers(1, &gl_state->vertex_index_buffer));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->vertex_index_buffer));
    float vertex_index_values[3] = { 0.0, 1.0, 2.0 };
    DO_GL(glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), vertex_index_values, GL_STATIC_DRAW));

    DO_GL(glGenBuffers(1, &gl_state->data_t_buffer));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->data_t_buffer));
    float data_t_values[3] = { 0.0, 0.0, 0.0 };
    DO_GL(glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), data_t_values, GL_STATIC_DRAW));
}

void check_shader(GLint shader) {
    GLint result;
    DO_GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
    if (result != 0)
        return;
    char info_log[4096];
    GLsizei info_log_length = 4096;
    DO_GL(glGetShaderInfoLog(shader, sizeof(info_log), &info_log_length, info_log));
    fprintf(stderr, "Shader compilation failed: %s\n", info_log);
}

void init_shaders(gl_state *gl_state) {
    gl_state->program = GL(glCreateProgram());

    gl_state->vertex_shader = GL(glCreateShader(GL_VERTEX_SHADER));
    DO_GL(glShaderSource(gl_state->vertex_shader, 1, &VERTEX_SHADER, NULL));
    DO_GL(glCompileShader(gl_state->vertex_shader));
    check_shader(gl_state->vertex_shader);

    gl_state->fragment_shader = GL(glCreateShader(GL_FRAGMENT_SHADER));
    DO_GL(glShaderSource(gl_state->fragment_shader, 1, &FRAGMENT_SHADER, NULL));
    DO_GL(glCompileShader(gl_state->fragment_shader));
    check_shader(gl_state->fragment_shader);

    DO_GL(glAttachShader(gl_state->program, gl_state->vertex_shader));
    DO_GL(glAttachShader(gl_state->program, gl_state->fragment_shader));
    DO_GL(glLinkProgram(gl_state->program));
    DO_GL(glUseProgram(gl_state->program));

    gl_state->position_attribute = GL(glGetAttribLocation(gl_state->program, "aPosition"));
    gl_state->vertex_index_attribute = GL(glGetAttribLocation(gl_state->program, "aVertexIndex"));
    gl_state->data_t_attribute = GL(glGetAttribLocation(gl_state->program, "aDataT"));
    gl_state->data_uniform = GL(glGetUniformLocation(gl_state->program, "uData"));

    DO_GL(glEnableVertexAttribArray(gl_state->position_attribute));
    DO_GL(glEnableVertexAttribArray(gl_state->vertex_index_attribute));
    DO_GL(glEnableVertexAttribArray(gl_state->data_t_attribute));
}

void draw_scene(gl_state *gl_state) {
    DO_GL(glViewport(0, 0, 320, 240));
    DO_GL(glEnable(GL_TEXTURE_2D));
    DO_GL(glClear(GL_COLOR_BUFFER_BIT));

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));
    DO_GL(glVertexAttribPointer(gl_state->position_attribute, 3, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->vertex_index_buffer));
    DO_GL(glVertexAttribPointer(gl_state->vertex_index_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->data_t_buffer));
    DO_GL(glVertexAttribPointer(gl_state->data_t_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));

    DO_GL(glDrawArrays(GL_TRIANGLES, 0, 3));
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("neon64",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          320,
                                          240,
                                          0);
    SDL_GL_CreateContext(window);

    gl_state gl_state;
    init_buffers(&gl_state);
    init_shaders(&gl_state);
    draw_scene(&gl_state);

    SDL_GL_SwapWindow(window);

    SDL_Event event;
    while (SDL_WaitEvent(&event) != 0) {
        if (event.type == SDL_QUIT)
            break;
    }

    return 0;
}

