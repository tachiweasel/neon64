// neon64/drawgl.cpp

#include "drawgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define DATA_TEXTURE_WIDTH  16
#define DATA_TEXTURE_HEIGHT 2048
#define TEXTURE_WIDTH  1024
#define TEXTURE_HEIGHT 1024
#define MAX_TRIANGLES 2048

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

#define CC_MODULATEI            0
#define CC_MODULATEIA           1
#define CC_MODULATEIDECALA      2
#define CC_MODULATEI_PRIM       3
#define CC_MODULATEIA_PRIM      4
#define CC_MODULATEIDECALA_PRIM 5
#define CC_DECALRGB             6
#define CC_DECALRGBA            7
#define CC_BLENDI               8
#define CC_BLENDIA              9
#define CC_BLENDIDECALA         10
#define CC_BLENDRGBA            11
#define CC_BLENDRGBDECALA       12
#define CC_REFLECTRGB           13
#define CC_REFLECTRGBDECALA     14
#define CC_HILITERGB            15
#define CC_HILITERGBA           16
#define CC_HILITERGBDECALA      17
#define CC_PRIMITIVE            18
#define CC_SHADE                19
#define CC_ADDRGB               20
#define CC_ADDRGBDECALA         21
#define CC_SHADEDECALA          22
#define CC_BLENDPE              23
#define CC_BLENDPEDECALA        24

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
    "   vec4 mode = texture2D(uData, vec2(0.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor0 = texture2D(uData, vec2(1.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor1 = texture2D(uData, vec2(2.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor2 = texture2D(uData, vec2(3.0 / 16.0, vDataT));\n"
    "   vec4 shadeColor = vLambda[0] * shadeColor0 + vLambda[1] * shadeColor1 + vLambda[2] * \n"
    "       shadeColor2;\n"
    "   vec2 textureCoord0 = texture2D(uData, vec2(4.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord1 = texture2D(uData, vec2(5.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord2 = texture2D(uData, vec2(6.0 / 16.0, vDataT)).xy;\n"
    "   vec2 textureCoord = vLambda[0] * textureCoord0 + vLambda[1] * textureCoord1 + \n"
    "       vLambda[2] * textureCoord2;\n"
    "   vec4 textureBounds = texture2D(uData, vec2(7.0 / 16.0, vDataT));\n"
    "   textureCoord = mod(textureCoord, 1.0);\n"
    "   textureCoord = (textureCoord * textureBounds.zw) + textureBounds.xy;\n"
    "   vec4 textureColor = texture2D(uTexture, textureCoord);\n"
    "   vec4 primitiveColor = texture2D(uData, vec2(8.0 / 16.0, vDataT));\n"
    "   vec4 environmentColor = texture2D(uData, vec2(9.0 / 16.0, vDataT));\n"
    "   float colorCombiner = mode[0] * 255.0;\n"
    "   vec4 resultColor;\n"
    "   if (colorCombiner < 3.0) {\n"
    "       resultColor.rgb = textureColor.rgb * shadeColor.rgb;\n"
    "   } else if (colorCombiner < 6.0) {\n"
    "       resultColor.rgb = textureColor.rgb * primitiveColor.rgb;\n"
    "   } else if (colorCombiner < 8.0) {\n"
    "       resultColor.rgb = textureColor.rgb;\n"
    "   } else if (colorCombiner < 11.0) {\n"
    "       resultColor.rgb = (environmentColor.rgb - shadeColor.rgb) * textureColor.rgb + \n"
    "           shadeColor.rgb;\n"
    "   } else if (colorCombiner < 13.0) {\n"
    "       resultColor.rgb = (textureColor.rgb - shadeColor.rgb) * textureColor.a + \n"
    "           shadeColor.rgb;\n"
    "   } else if (colorCombiner < 15.0) {\n"
    "       resultColor.rgb = environmentColor.rgb * textureColor.rgb + shadeColor.rgb;\n"
    "   } else if (colorCombiner < 18.0) {\n"
    "       resultColor.rgb = (primitiveColor.rgb - shadeColor.rgb) * textureColor.rgb + \n"
    "           shadeColor.rgb;\n"
    "   } else if (colorCombiner == 18.0) {\n"
    "       resultColor.rgb = primitiveColor.rgb;\n"
    "   } else if (colorCombiner == 19.0) {\n"
    "       resultColor.rgb = shadeColor.rgb;\n"
    "   } else if (colorCombiner < 22.0) {\n"
    "       resultColor.rgb = textureColor.rgb + shadeColor.rgb;\n"
    "   } else if (colorCombiner < 23.0) {\n"
    "       resultColor.rgb = shadeColor.rgb;\n"
    "   } else {\n"
    "       resultColor.rgb = (primitiveColor.rgb - environmentColor.rgb) * textureColor.rgb + \n"
    "           environmentColor.rgb;\n"
    "   }\n"
    "   if (colorCombiner == 0.0 || colorCombiner == 6.0 || colorCombiner == 8.0 ||\n"
    "           colorCombiner == 11.0 || colorCombiner == 13.0 || colorCombiner == 15.0 ||\n"
    "           colorCombiner == 19.0 || colorCombiner == 20.0) {\n"
    "       resultColor.a = shadeColor.a;\n"
    "   } else if (colorCombiner == 1.0 || colorCombiner == 9.0 || colorCombiner == 23.0) {\n"
    "       resultColor.a = textureColor.a * shadeColor.a;\n"
    "   } else if (colorCombiner == 2.0 || colorCombiner == 5.0 || colorCombiner == 7.0 ||\n"
    "           colorCombiner == 10.0 || colorCombiner == 12.0 || colorCombiner == 14.0 ||\n"
    "           colorCombiner == 17.0 || colorCombiner == 21.0 || colorCombiner == 22.0 ||\n"
    "           colorCombiner == 24.0) {\n"
    "       resultColor.a = textureColor.a;\n"
    "   } else if (colorCombiner == 3.0 || colorCombiner == 18.0) {\n"
    "       resultColor.a = primitiveColor.a;\n"
    "   } else if (colorCombiner == 4.0) {\n"
    "       resultColor.a = textureColor.a * primitiveColor.a;\n"
    "   } else if (colorCombiner == 16.0) {\n"
    "       resultColor.a = (primitiveColor.a - shadeColor.a) * textureColor.a + shadeColor.a;\n"
    "   }\n"
    "   gl_FragColor = resultColor;\n"
    //"   gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
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
        gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + 0] = CC_SHADE;
        gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + 1] = triangles[y].v0.shade;
        gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + 2] = triangles[y].v1.shade;
        gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + 3] = triangles[y].v2.shade;
        for (int x = 4; x < 16; x++) {
            gl_state->data_texture_buffer[y * DATA_TEXTURE_WIDTH + x] = rand();
        }
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

    DO_GL(glFinish());
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

