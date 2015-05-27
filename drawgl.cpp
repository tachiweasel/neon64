// neon64/drawgl.cpp

#include "drawgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define DATA_TEXTURE_WIDTH  16
#define DATA_TEXTURE_HEIGHT 2048
#define TEXTURE_WIDTH  1024
#define TEXTURE_HEIGHT 1024
#define MAX_TRIANGLES 2048

#define COLUMN_RGB_MODE         0
#define COLUMN_A_MODE           1
#define COLUMN_SA               2
#define COLUMN_SB               3
#define COLUMN_M                4
#define COLUMN_A                5
#define COLUMN_SHADE_COLOR_0    6
#define COLUMN_SHADE_COLOR_1    7
#define COLUMN_SHADE_COLOR_2    8
#define COLUMN_TEXTURE_COORD_0  9
#define COLUMN_TEXTURE_COORD_1  10
#define COLUMN_TEXTURE_COORD_2  11
#define COLUMN_TEXTURE_BOUNDS   12

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
    "   gl_Position = vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);\n"
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
    "uniform sampler2D uTexture;\n"
    "void main(void) {\n"
    "   vec4 rgbMode = texture2D(uData, vec2(0.0 / 16.0, vDataT));\n"
    "   vec4 aMode = texture2D(uData, vec2(1.0 / 16.0, vDataT));\n"
    "   vec4 sa = texture2D(uData, vec2(2.0 / 16.0, vDataT));\n"
    "   vec4 sb = texture2D(uData, vec2(3.0 / 16.0, vDataT));\n"
    "   vec4 m = texture2D(uData, vec2(4.0 / 16.0, vDataT));\n"
    "   vec4 a = texture2D(uData, vec2(5.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor0 = texture2D(uData, vec2(6.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor1 = texture2D(uData, vec2(7.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor2 = texture2D(uData, vec2(8.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor = vLambda[0] * shadeColor0 + vLambda[1] * shadeColor1 + vLambda[2] * \n"
    "       shadeColor2;\n"
    "   vec2 textureCoord0 = texture2D(uData, vec2(9.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord1 = texture2D(uData, vec2(10.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord2 = texture2D(uData, vec2(11.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord = vLambda[0] * textureCoord0 + vLambda[1] * textureCoord1 + \n"
    "       vLambda[2] * textureCoord2;\n"
    "   vec4 textureBounds = texture2D(uData, vec2(12.0 / 16.0, vDataT));\n"
    "   textureCoord = mod(textureCoord, 1.0);\n"
    "   textureCoord = (textureCoord * textureBounds.zw) + textureBounds.xy;\n"
    "   vec4 textureColor = texture2D(uTexture, textureCoord);\n"
    "   if (rgbMode[0] > 0.0 && rgbMode[0] < 1.0)\n"
    "       sa.rgb = shadeColor.rgb;\n"
    "   else if (rgbMode[0] == 1.0)\n"
    "       sa.rgb = textureColor.rgb;\n"
    "   if (rgbMode[1] > 0.0 && rgbMode[1] < 1.0)\n"
    "       sb.rgb = shadeColor.rgb;\n"
    "   else if (rgbMode[1] == 1.0)\n"
    "       sb.rgb = textureColor.rgb;\n"
    "   if (rgbMode[2] > 0.0 && rgbMode[2] < 1.0)\n"
    "       m.rgb = shadeColor.rgb;\n"
    "   else if (rgbMode[2] == 1.0)\n"
    "       m.rgb = textureColor.rgb;\n"
    "   if (rgbMode[3] > 0.0 && rgbMode[3] < 1.0)\n"
    "       a.rgb = shadeColor.rgb;\n"
    "   else if (rgbMode[3] == 1.0)\n"
    "       a.rgb = textureColor.rgb;\n"
    "   if (aMode[0] > 0.0 && aMode[0] < 1.0)\n"
    "       sa.a = shadeColor.a;\n"
    "   else if (aMode[0] == 1.0)\n"
    "       sa.a = textureColor.a;\n"
    "   if (aMode[1] > 0.0 && aMode[1] < 1.0)\n"
    "       sb.a = shadeColor.a;\n"
    "   else if (aMode[1] == 1.0)\n"
    "       sb.a = textureColor.a;\n"
    "   if (aMode[2] > 0.0 && aMode[2] < 1.0)\n"
    "       m.a = shadeColor.a;\n"
    "   else if (aMode[2] == 1.0)\n"
    "       m.a = textureColor.a;\n"
    "   if (aMode[3] > 0.0 && aMode[3] < 1.0)\n"
    "       a.a = shadeColor.a;\n"
    "   else if (aMode[3] == 1.0)\n"
    "       a.a = textureColor.a;\n"
    "   gl_FragColor = (sa - sb) * m + a;\n"
    "}\n";

bool is_shallow(vfloat32x4_t *position) {
    return position->z <= 0.0;
}

void add_triangle(gl_state *gl_state, triangle *triangle) {
    if (gl_state->triangle_count == MAX_TRIANGLES) {
        fprintf(stderr, "warning: triangle overflow!\n");
        return;
    }

    if (is_shallow(&triangle->v0.position) && is_shallow(&triangle->v1.position) &&
            is_shallow(&triangle->v2.position)) {
        return;
    }

    gl_state->triangles[gl_state->triangle_count] = *triangle;
    gl_state->triangle_count++;
}

void init_buffers(gl_state *gl_state) {
    DO_GL(glGenBuffers(1, &gl_state->position_buffer));
    DO_GL(glGenBuffers(1, &gl_state->vertex_index_buffer));
    DO_GL(glGenBuffers(1, &gl_state->data_t_buffer));
}

void init_textures(gl_state *gl_state) {
    DO_GL(glEnable(GL_TEXTURE_2D));

    gl_state->texture_buffer =
        (uint32_t *)malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(uint32_t));
    for (int y = 0; y < TEXTURE_HEIGHT; y++) {
        for (int x = 0; x < TEXTURE_WIDTH; x++) {
            gl_state->texture_buffer[y * DATA_TEXTURE_WIDTH + x] = rand();
        }
    }

    DO_GL(glGenTextures(1, &gl_state->data_texture));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->data_texture));
    DO_GL(glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       DATA_TEXTURE_WIDTH,
                       DATA_TEXTURE_HEIGHT,
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       NULL));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    DO_GL(glGenTextures(1, &gl_state->texture));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->texture));
    DO_GL(glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       DATA_TEXTURE_WIDTH,
                       DATA_TEXTURE_HEIGHT,
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       gl_state->texture_buffer));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    DO_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
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
    gl_state->texture_uniform = GL(glGetUniformLocation(gl_state->program, "uTexture"));

    DO_GL(glEnableVertexAttribArray(gl_state->position_attribute));
    DO_GL(glEnableVertexAttribArray(gl_state->vertex_index_attribute));
    DO_GL(glEnableVertexAttribArray(gl_state->data_t_attribute));
}

void init_gl_state(gl_state *gl_state) {
    gl_state->triangles = (triangle *)malloc(sizeof(triangle) * MAX_TRIANGLES);
    gl_state->triangle_count = 0;
    init_buffers(gl_state);
    init_textures(gl_state);
    init_shaders(gl_state);
}

void reset_gl_state(gl_state *gl_state) {
    gl_state->triangle_count = 0;
}

void set_column(gl_state *gl_state, int y, int column, uint32_t value) {
    gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + column] = value;
}

void init_scene(gl_state *gl_state) {
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));
    float *triangle_vertices = (float *)malloc(sizeof(float) * 3 * 3 * gl_state->triangle_count);
    for (unsigned i = 0; i < gl_state->triangle_count; i++) {
        triangle_vertices[i * 9 + 0] = gl_state->triangles[i].v0.position.x;
        triangle_vertices[i * 9 + 1] = gl_state->triangles[i].v0.position.y;
        triangle_vertices[i * 9 + 2] = gl_state->triangles[i].v0.position.z;
        triangle_vertices[i * 9 + 3] = gl_state->triangles[i].v1.position.x;
        triangle_vertices[i * 9 + 4] = gl_state->triangles[i].v1.position.y;
        triangle_vertices[i * 9 + 5] = gl_state->triangles[i].v1.position.z;
        triangle_vertices[i * 9 + 6] = gl_state->triangles[i].v2.position.x;
        triangle_vertices[i * 9 + 7] = gl_state->triangles[i].v2.position.y;
        triangle_vertices[i * 9 + 8] = gl_state->triangles[i].v2.position.z;
#if 0
        printf("%f,%f,%f\n",
               gl_state->triangles[i].v0.position.x,
               gl_state->triangles[i].v0.position.y,
               gl_state->triangles[i].v0.position.z);
#endif
    }
    DO_GL(glBufferData(GL_ARRAY_BUFFER,
                       gl_state->triangle_count * 3 * 3 * sizeof(float),
                       triangle_vertices,
                       GL_STATIC_DRAW));
    free(triangle_vertices);

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->vertex_index_buffer));
    float *vertex_index_values = (float *)malloc(sizeof(float) * 3 * gl_state->triangle_count);
    for (unsigned int i = 0; i < gl_state->triangle_count; i++) {
        vertex_index_values[i * 3 + 0] = 0.0;
        vertex_index_values[i * 3 + 1] = 1.0;
        vertex_index_values[i * 3 + 2] = 2.0;
    }
    DO_GL(glBufferData(GL_ARRAY_BUFFER,
                       gl_state->triangle_count * 3 * sizeof(float),
                       vertex_index_values,
                       GL_STATIC_DRAW));
    free(vertex_index_values);

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->data_t_buffer));
    float *data_t_values = (float *)malloc(sizeof(float) * 3 * gl_state->triangle_count);
    for (unsigned int i = 0; i < gl_state->triangle_count; i++) {
        data_t_values[i * 3 + 0] = (float)i / (float)DATA_TEXTURE_HEIGHT;
        data_t_values[i * 3 + 1] = (float)i / (float)DATA_TEXTURE_HEIGHT;
        data_t_values[i * 3 + 2] = (float)i / (float)DATA_TEXTURE_HEIGHT;
    }
    DO_GL(glBufferData(GL_ARRAY_BUFFER,
                       gl_state->triangle_count * 3 * sizeof(float),
                       data_t_values,
                       GL_STATIC_DRAW));
    free(data_t_values);

    gl_state->data_texture_buffer =
        (uint32_t *)malloc(DATA_TEXTURE_WIDTH * DATA_TEXTURE_HEIGHT * sizeof(uint32_t));
    triangle *triangles = gl_state->triangles;
    for (int y = 0; y < MAX_TRIANGLES; y++) {
        set_column(gl_state, y, COLUMN_RGB_MODE, 0x7f000000);
        set_column(gl_state, y, COLUMN_A_MODE, 0x7f000000);
        set_column(gl_state, y, COLUMN_SA, 0);
        set_column(gl_state, y, COLUMN_SB, 0);
        set_column(gl_state, y, COLUMN_M, 0);
        set_column(gl_state, y, COLUMN_A, 0);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_0, triangles[y].v0.shade);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_1, triangles[y].v1.shade);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_2, triangles[y].v2.shade);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_0, 0);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_1, 0);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_2, 0);
    }

    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->data_texture));
    DO_GL(glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       DATA_TEXTURE_WIDTH,
                       DATA_TEXTURE_HEIGHT,
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       gl_state->data_texture_buffer));
    free(gl_state->data_texture_buffer);

    printf("triangle count=%d\n", (int)gl_state->triangle_count);
}

void draw_scene(gl_state *gl_state) {
    DO_GL(glUseProgram(gl_state->program));
    DO_GL(glViewport(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT));
    DO_GL(glEnable(GL_TEXTURE_2D));
    DO_GL(glEnable(GL_DEPTH_TEST));
    DO_GL(glDepthFunc(GL_LESS));
    DO_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));
    DO_GL(glVertexAttribPointer(gl_state->position_attribute, 3, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->vertex_index_buffer));
    DO_GL(glVertexAttribPointer(gl_state->vertex_index_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->data_t_buffer));
    DO_GL(glVertexAttribPointer(gl_state->data_t_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glActiveTexture(GL_TEXTURE0 + 0));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->data_texture));
    DO_GL(glUniform1i(gl_state->data_uniform, 0));
    DO_GL(glActiveTexture(GL_TEXTURE0 + 1));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->texture));
    DO_GL(glUniform1i(gl_state->texture_uniform, 0));

    DO_GL(glDrawArrays(GL_TRIANGLES, 0, gl_state->triangle_count * 3));
}

#ifdef DRAWGL_STANDALONE
int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: drawgl vertices.txt\n");
        return 0;
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("neon64",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          FRAMEBUFFER_WIDTH,
                                          FRAMEBUFFER_HEIGHT,
                                          0);
    SDL_GL_CreateContext(window);

    gl_state gl_state;
    init_gl_state(&gl_state);

    FILE *f = fopen(argv[1], "r");
    triangle *triangles = (triangle *)malloc(sizeof(triangle) * MAX_TRIANGLES);
    triangle triangle;
    int triangle_count = 0;
    int vertex_count = 0;
    while (!feof(f)) {
        triangle_vertex vertex;
        unsigned r, g, b, a;
        if (fscanf(f,
                   "%f,%f,%f,%u,%u,%u,%u\n",
                   &vertex.position.x,
                   &vertex.position.y,
                   &vertex.position.z,
                   &r,
                   &g,
                   &b,
                   &a) < 7) {
            break;
        }
        //vertex.shade = r | (g << 8) | (b << 16) | (a << 24);
        vertex.shade = r | (g << 8) | (b << 16) | (a << 24);
        vertex.position.z = 1.0;
        switch (vertex_count) {
        case 0:
            triangle.v0 = vertex;
            break;
        case 1:
            triangle.v1 = vertex;
            break;
        case 2:
            triangle.v2 = vertex;
            break;
        }
        vertex_count++;
        if (vertex_count == 3) {
            if (triangle_count == MAX_TRIANGLES)
                break;
            triangles[triangle_count] = triangle;
            triangle_count++;
            vertex_count = 0;
        }
    }
    fclose(f);
    fprintf(stderr, "read %d triangles\n", (int)triangle_count);

    draw_scene(&gl_state);
    glFinish();

    SDL_GL_SwapWindow(window);

    uint32_t start = SDL_GetTicks();
    draw_scene(&gl_state);
    glFinish();
    uint32_t end = SDL_GetTicks();
    printf("drawing took %dms\n", (int)(end - start));
    SDL_GL_SwapWindow(window);

    SDL_Event event;
    while (SDL_WaitEvent(&event) != 0) {
        if (event.type == SDL_QUIT)
            break;
    }

    return 0;
}
#endif

