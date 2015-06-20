// neon64/drawgl.cpp

#include "drawgl.h"
#include "rdp.h"
#include "textures.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define DATA_TEXTURE_WIDTH  16
#define DATA_TEXTURE_HEIGHT 4096
#define TEXTURE_WIDTH       1024
#define TEXTURE_HEIGHT      1024
#define MAX_TRIANGLES       4096
#define MAX_TEXTURE_INFO    1024
#define MIN_TEXTURE_SIZE    16

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
    "attribute vec4 aPosition;\n"
    "attribute float aVertexIndex;\n"
    "attribute float aDataT;\n"
    "varying vec3 vLambda;\n"
    "varying float vDataT;\n"
    "void main(void) {\n"
    "   gl_Position = aPosition;\n"
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
    "   float dataT = fract(vDataT) > 0.5 ? ceil(vDataT) : floor(vDataT);\n"
    "   dataT /= 4096.0;\n"
    "   vec4 rgbMode = texture2D(uData, vec2(0.0 / 16.0, dataT));\n"
    "   vec4 aMode = texture2D(uData, vec2(1.0 / 16.0, dataT));\n"
    "   vec4 sa = texture2D(uData, vec2(2.0 / 16.0, dataT));\n"
    "   vec4 sb = texture2D(uData, vec2(3.0 / 16.0, dataT));\n"
    "   vec4 m = texture2D(uData, vec2(4.0 / 16.0, dataT));\n"
    "   vec4 a = texture2D(uData, vec2(5.0 / 16.0, dataT));\n"
    "   vec4 shadeColor0 = texture2D(uData, vec2(6.0 / 16.0, dataT));\n"
    "   vec4 shadeColor1 = texture2D(uData, vec2(7.0 / 16.0, dataT));\n"
    "   vec4 shadeColor2 = texture2D(uData, vec2(8.0 / 16.0, dataT));\n"
    "   vec4 shadeColor = vLambda[0] * shadeColor0 + vLambda[1] * shadeColor1 + vLambda[2] * \n"
    "       shadeColor2;\n"
    "   vec4 rawTextureCoord0 = texture2D(uData, vec2(9.0 / 16.0, dataT));\n"
    "   vec2 textureCoord0 = vec2(rawTextureCoord0[0] * 65536.0 + rawTextureCoord0[1] * 256.0,\n"
    "                             rawTextureCoord0[2] * 65536.0 + rawTextureCoord0[3] * 256.0);\n"
    "   vec4 rawTextureCoord1 = texture2D(uData, vec2(10.0 / 16.0, dataT));\n"
    "   vec2 textureCoord1 = vec2(rawTextureCoord1[0] * 65536.0 + rawTextureCoord1[1] * 256.0,\n"
    "                             rawTextureCoord1[2] * 65536.0 + rawTextureCoord1[3] * 256.0);\n"
    "   vec4 rawTextureCoord2 = texture2D(uData, vec2(11.0 / 16.0, dataT));\n"
    "   vec2 textureCoord2 = vec2(rawTextureCoord2[0] * 65536.0 + rawTextureCoord2[1] * 256.0,\n"
    "                             rawTextureCoord2[2] * 65536.0 + rawTextureCoord2[3] * 256.0);\n"
    "   vec2 textureCoord = vLambda[0] * textureCoord0 + vLambda[1] * textureCoord1 + \n"
    "       vLambda[2] * textureCoord2;\n"
    "   vec4 texturePixelBounds = texture2D(uData, vec2(12.0 / 16.0, dataT)) * 4096.0;\n"
    "   /* Convert texture coords to [0.0, 1.0). */\n"
    "   if (texturePixelBounds.z == 0.0 || texturePixelBounds.w == 0.0)\n"
    "       texturePixelBounds.zw = vec2(1.0, 1.0);\n"
    "   textureCoord = textureCoord / texturePixelBounds.zw;\n"
    "   textureCoord = mod(textureCoord, 1.0);\n"
    "   /* Translate texture coords onto the atlas. */\n"
    "   vec4 textureBounds = texturePixelBounds / 1024.0;\n"
    "   textureCoord = (textureCoord * textureBounds.zw) + textureBounds.xy;\n"
    "   vec4 textureColor = texture2D(uTexture, textureCoord);\n"
    //"   vec4 textureColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "   if (rgbMode[0] > 0.0 && rgbMode[0] < 1.0)\n"
    "       sa.rgb = shadeColor.rgb;\n"
    "   else if (rgbMode[0] == 1.0)\n"
    "       sa.rgb = textureColor.rgb;\n"
    //"   sa.rgb = vec3(1.0, 1.0, 1.0);\n"
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
#if 0
    "   if (fract(vDataT) != 0.0)\n"
    "       gl_FragColor = vec4(fract(vDataT), 0.0, 0.0, 1.0);\n"
    "   else\n"
    "       gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
#endif
    "}\n";

uint16_t minu16(uint16_t a, uint16_t b) {
    return (a < b) ? a : b;
}

uint16_t maxu16(uint16_t a, uint16_t b) {
    return (a > b) ? a : b;
}

bool is_shallow(vfloat32x4_t *position) {
    //return position->z < 0.1 && position->z > -0.2;
    abort();
}

void add_triangle(gl_state *gl_state, triangle *triangle) {
    if (gl_state->triangle_count == MAX_TRIANGLES) {
        fprintf(stderr, "warning: triangle overflow!\n");
        return;
    }

#if 0
    // FIXME(tachiweasel): Remove.
    if (is_shallow(&triangle->v0.position) && is_shallow(&triangle->v1.position) &&
            is_shallow(&triangle->v2.position)) {
        return;
    }
#endif

    gl_state->triangles[gl_state->triangle_count] = *triangle;
    gl_state->triangle_count++;
}

bool texture_space_occupied(gl_state *gl_state,
                            uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height) {
    uint16_t left = x, top = y, right = x + width, bottom = y + height;
    for (unsigned i = 0; i < gl_state->texture_info_count; i++) {
        gl_texture_info *texture_info = &gl_state->texture_info[i];
        uint16_t this_left = texture_info->x, this_top = texture_info->y;
        uint16_t this_right = texture_info->x + texture_info->width;
        uint16_t this_bottom = texture_info->y + texture_info->height;
        uint16_t intersection_left = maxu16(left, this_left);
        uint16_t intersection_top = maxu16(top, this_top);
        uint16_t intersection_right = minu16(right, this_right);
        uint16_t intersection_bottom = minu16(bottom, this_bottom);
        if (intersection_left < intersection_right && intersection_top < intersection_bottom)
            return true;
    }
    return false;
}

uint32_t id_of_texture_with_hash(gl_state *gl_state, uint32_t hash) {
    for (uint16_t i = 0; i < gl_state->texture_info_count; i++) {
        if (gl_state->texture_info[i].hash == hash)
            return gl_state->texture_info[i].id;
    }
    return 0;
}

uint32_t add_texture(gl_state *gl_state, swizzled_texture *texture) {
    gl_texture_info texture_info;
    texture_info.id = gl_state->next_texture_id;
    gl_state->next_texture_id++;
    texture_info.hash = texture->hash;

    // Find a spot for the texture.
    bool found = false;
    for (uint16_t y = 0; y < TEXTURE_HEIGHT; y += 16) {
        for (uint16_t x = 0; x < TEXTURE_WIDTH; x += 16) {
            if (texture_space_occupied(gl_state, x, y, texture->width, texture->height))
                continue;
            printf("found location at %d,%d\n", (int)x, (int)y);
            texture_info.x = x;
            texture_info.y = y;
            texture_info.width = texture->width;
            texture_info.height = texture->height;
            found = true;
            break;
        }
        if (found)
            break;
    }

    if (!found) {
        fprintf(stderr, "Texture atlas full!\n");
        abort();
    }

    // Copy pixels in.
    for (uint16_t y = 0; y < texture_info.height; y++) {
        for (uint16_t x = 0; x < texture_info.width; x++) {
            gl_state->texture_buffer[(texture_info.y + y) * TEXTURE_WIDTH + (texture_info.x + x)]
                = texture->pixels[y * texture_info.width + x];
        }
    }

    uint32_t index = gl_state->texture_info_count;
    gl_state->texture_info[index] = texture_info;
    gl_state->texture_info_count++;
    gl_state->texture_buffer_is_dirty = true;

    printf("added texture: %u @ (%d,%d) for (%d,%d)\n",
           gl_state->texture_info_count,
           gl_state->texture_info[index].x,
           gl_state->texture_info[index].y,
           gl_state->texture_info[index].width,
           gl_state->texture_info[index].height);

    return gl_state->texture_info[index].id;
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
    gl_state->texture_info = (gl_texture_info *)malloc(sizeof(gl_texture_info) * MAX_TEXTURE_INFO);
    gl_state->texture_info_count = 0;
    gl_state->next_texture_id = 1;
    gl_state->texture_buffer_is_dirty = false;

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

uint32_t bounds_of_texture_with_id(gl_state *gl_state, uint32_t id) {
    for (uint32_t i = 0; i < gl_state->texture_info_count; i++) {
        gl_texture_info *texture_info = &gl_state->texture_info[i];
        if (texture_info->id != id)
            continue;
        return ((texture_info->x / MIN_TEXTURE_SIZE) << 0) |
            ((texture_info->y / MIN_TEXTURE_SIZE) << 8) |
            ((texture_info->width / MIN_TEXTURE_SIZE) << 16) |
            ((texture_info->height / MIN_TEXTURE_SIZE) << 24);
    }
    return 0;
}

float munge_z_coordinate(triangle *triangle, uint8_t vertex_index) {
    float z_base = triangle->z_base;
    float original_z, w;
    switch (vertex_index) {
    case 0:
        original_z = triangle->v0.position.z;
        w = triangle->v0.position.w;
        break;
    case 1:
        original_z = triangle->v1.position.z;
        w = triangle->v1.position.w;
        break;
    case 2:
        original_z = triangle->v2.position.z;
        w = triangle->v2.position.w;
        break;
    }
    float z_offset = triangle->z_buffer_enabled ? original_z : 0.0;
    float z = (z_base + z_offset / w * Z_BUCKET_SIZE) * w;
    return z;
}

void init_scene(gl_state *gl_state) {
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));

    printf("triangle count = %d, first 1/32 are %d\n",
           gl_state->triangle_count,
           gl_state->triangle_count / 32);
#if 0
    memcpy(&gl_state->triangles[gl_state->triangle_count / 64],
           &gl_state->triangles[gl_state->triangle_count * 2 / 64],
           sizeof(gl_state->triangles[0]) * gl_state->triangle_count * 62 / 64);
    gl_state->triangle_count = gl_state->triangle_count * 63 / 64;
#endif

    float *triangle_vertices = (float *)malloc(sizeof(float) * 3 * 4 * gl_state->triangle_count);
    for (unsigned i = 0; i < gl_state->triangle_count; i++) {
        triangle_vertices[i * 12 + 0] = gl_state->triangles[i].v0.position.x;
        triangle_vertices[i * 12 + 1] = gl_state->triangles[i].v0.position.y;
        triangle_vertices[i * 12 + 2] = munge_z_coordinate(&gl_state->triangles[i], 0);
        triangle_vertices[i * 12 + 3] = gl_state->triangles[i].v0.position.w;
        triangle_vertices[i * 12 + 4] = gl_state->triangles[i].v1.position.x;
        triangle_vertices[i * 12 + 5] = gl_state->triangles[i].v1.position.y;
        triangle_vertices[i * 12 + 6] = munge_z_coordinate(&gl_state->triangles[i], 1);
        triangle_vertices[i * 12 + 7] = gl_state->triangles[i].v1.position.w;
        triangle_vertices[i * 12 + 8] = gl_state->triangles[i].v2.position.x;
        triangle_vertices[i * 12 + 9] = gl_state->triangles[i].v2.position.y;
        triangle_vertices[i * 12 + 10] = munge_z_coordinate(&gl_state->triangles[i], 2);
        triangle_vertices[i * 12 + 11] = gl_state->triangles[i].v2.position.w;

#if 0
        if (i < gl_state->triangle_count * 62 / 64) {
            printf("trivert %f,%f,%f %f,%f,%f %f,%f,%f\n",
                   gl_state->triangles[i].v0.position.x,
                   gl_state->triangles[i].v0.position.y,
                   gl_state->triangles[i].v0.position.z,
                   gl_state->triangles[i].v1.position.x,
                   gl_state->triangles[i].v1.position.y,
                   gl_state->triangles[i].v1.position.z,
                   gl_state->triangles[i].v2.position.x,
                   gl_state->triangles[i].v2.position.y,
                   gl_state->triangles[i].v2.position.z);
        }
#endif
    }
    DO_GL(glBufferData(GL_ARRAY_BUFFER,
                       gl_state->triangle_count * 3 * 4 * sizeof(float),
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
        data_t_values[i * 3 + 0] = (float)i;
        data_t_values[i * 3 + 1] = (float)i;
        data_t_values[i * 3 + 2] = (float)i;
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
        set_column(gl_state, y, COLUMN_RGB_MODE, triangles[y].rgb_mode);
        set_column(gl_state, y, COLUMN_A_MODE, triangles[y].a_mode);
        set_column(gl_state, y, COLUMN_SA, triangles[y].sa_color);
        set_column(gl_state, y, COLUMN_SB, triangles[y].sb_color);
        set_column(gl_state, y, COLUMN_M, triangles[y].m_color);
        set_column(gl_state, y, COLUMN_A, triangles[y].a_color);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_0, triangles[y].v0.shade);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_1, triangles[y].v1.shade);
        set_column(gl_state, y, COLUMN_SHADE_COLOR_2, triangles[y].v2.shade);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_0, triangles[y].v0.texture_coord);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_1, triangles[y].v1.texture_coord);
        set_column(gl_state, y, COLUMN_TEXTURE_COORD_2, triangles[y].v2.texture_coord);
        set_column(gl_state, y, COLUMN_TEXTURE_BOUNDS, triangles[y].texture_bounds);

#if 0
        if (triangles[y].texture_bounds != 0) {
            fprintf(stderr,
                    "texture coords are %08x,%08x,%08x, bounds are %08x\n",
                    triangles[y].v0.texture_coord,
                    triangles[y].v1.texture_coord,
                    triangles[y].v2.texture_coord,
                    triangles[y].texture_bounds);
        }
#endif
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

    if (gl_state->texture_buffer_is_dirty) {
#if 0
        // Dump to TGA...
        FILE *f = fopen("/tmp/neon64texture.tga", "w");
        char header[ 18 ] = { 0 }; // char = byte
        header[ 2 ] = 2; // truecolor
        header[ 12 ] = TEXTURE_WIDTH & 0xFF;
        header[ 13 ] = (TEXTURE_WIDTH >> 8) & 0xFF;
        header[ 14 ] = TEXTURE_HEIGHT & 0xFF;
        header[ 15 ] = (TEXTURE_HEIGHT >> 8) & 0xFF;
        header[ 16 ] = 24; // bits per pixel
        fwrite(&header[0], 1, sizeof(header), f);
        for (int32_t y = TEXTURE_HEIGHT - 1; y >= 0; y--) {
            for (int32_t x = 0; x < TEXTURE_WIDTH; x++) {
                uint32_t color = gl_state->texture_buffer[y * TEXTURE_WIDTH + x];
                fputc((int)((color >> 16) & 0xff), f);
                fputc((int)((color >> 8) & 0xff), f);
                fputc((int)((color >> 0) & 0xff), f);
            }
        }
        fclose(f);
        fprintf(stderr, "wrote /tmp/neon64texture.tga\n");
#endif

        DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->texture));
        DO_GL(glTexImage2D(GL_TEXTURE_2D,
                           0,
                           GL_RGBA,
                           TEXTURE_WIDTH,
                           TEXTURE_HEIGHT,
                           0,
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           gl_state->texture_buffer));
        gl_state->texture_buffer_is_dirty = false;
    }

    printf("triangle count=%d\n", (int)gl_state->triangle_count);
}

void draw_scene(gl_state *gl_state) {
    DO_GL(glUseProgram(gl_state->program));
    DO_GL(glViewport(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT));
    DO_GL(glEnable(GL_TEXTURE_2D));
    DO_GL(glEnable(GL_DEPTH_TEST));
    DO_GL(glEnable(GL_CULL_FACE));
    DO_GL(glCullFace(GL_BACK));
    DO_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->position_buffer));
    DO_GL(glVertexAttribPointer(gl_state->position_attribute, 4, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->vertex_index_buffer));
    DO_GL(glVertexAttribPointer(gl_state->vertex_index_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glBindBuffer(GL_ARRAY_BUFFER, gl_state->data_t_buffer));
    DO_GL(glVertexAttribPointer(gl_state->data_t_attribute, 1, GL_FLOAT, GL_FALSE, 0, 0));
    DO_GL(glActiveTexture(GL_TEXTURE0 + 0));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->data_texture));
    DO_GL(glUniform1i(gl_state->data_uniform, 0));
    DO_GL(glActiveTexture(GL_TEXTURE0 + 1));
    DO_GL(glBindTexture(GL_TEXTURE_2D, gl_state->texture));
    DO_GL(glUniform1i(gl_state->texture_uniform, 1));

    DO_GL(glDrawArrays(GL_TRIANGLES, 0, gl_state->triangle_count * 3));

    DO_GL(glDisable(GL_CULL_FACE));
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

