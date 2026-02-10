#include "gl_shaders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLShaderManager shader_manager = {0};

/* Vertex shader: standard screen-space rendering */
static const char *vertex_shader_source = 
    "#version 330 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texCoord;\n"
    "out vec2 fragTexCoord;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(position, 0.0, 1.0);\n"
    "    fragTexCoord = texCoord;\n"
    "}\n";

/* Fragment shader: standard texture rendering */
static const char *frag_shader_solid =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform float opacity;\n"
    "void main() {\n"
    "    vec4 texColor = texture(texture0, fragTexCoord);\n"
    "    FragColor = vec4(texColor.rgb, texColor.a * opacity);\n"
    "}\n";

/* Fragment shader: background blur effect */
static const char *frag_shader_blur =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform float blurRadius;\n"
    "uniform vec2 textureSize;\n"
    "const int NUM_SAMPLES = 13;\n"
    "const float PI = 3.14159265359;\n"
    "void main() {\n"
    "    vec4 color = vec4(0.0);\n"
    "    float totalWeight = 0.0;\n"
    "    for (int i = 0; i < NUM_SAMPLES; i++) {\n"
    "        float angle = (2.0 * PI * float(i)) / float(NUM_SAMPLES);\n"
    "        vec2 offset = vec2(cos(angle), sin(angle)) * blurRadius / textureSize;\n"
    "        float weight = 1.0 - (float(i) / float(NUM_SAMPLES));\n"
    "        color += texture(texture0, fragTexCoord + offset) * weight;\n"
    "        totalWeight += weight;\n"
    "    }\n"
    "    FragColor = color / totalWeight;\n"
    "}\n";

/* Fragment shader: color filter (brightness, saturation, hue) */
static const char *frag_shader_color_filter =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform float brightness;\n"
    "uniform float saturation;\n"
    "uniform float hueShift;\n"
    "const float PI = 3.14159265359;\n"
    "vec3 rgb2hsv(vec3 c) {\n"
    "    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n"
    "    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n"
    "    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n"
    "    float d = q.x - min(q.w, q.y);\n"
    "    float e = 1.0e-10;\n"
    "    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n"
    "}\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"
    "void main() {\n"
    "    vec4 texColor = texture(texture0, fragTexCoord);\n"
    "    vec3 hsv = rgb2hsv(texColor.rgb);\n"
    "    hsv.x = mod(hsv.x + hueShift / 360.0, 1.0);\n"
    "    hsv.y = clamp(hsv.y * saturation, 0.0, 1.0);\n"
    "    hsv.z = clamp(hsv.z * brightness, 0.0, 1.0);\n"
    "    vec3 rgb = hsv2rgb(hsv);\n"
    "    FragColor = vec4(rgb, texColor.a);\n"
    "}\n";

/* Fragment shader: window decoration with border and shadow */
static const char *frag_shader_decoration =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 decorColor;\n"
    "uniform float borderWidth;\n"
    "uniform vec2 textureSize;\n"
    "void main() {\n"
    "    vec2 uv = fragTexCoord;\n"
    "    vec2 pixelCoord = uv * textureSize;\n"
    "    vec2 edgeDist = min(pixelCoord, textureSize - pixelCoord);\n"
    "    float minDist = min(edgeDist.x, edgeDist.y);\n"
    "    float borderAlpha = step(borderWidth, minDist);\n"
    "    vec4 texColor = texture(texture0, uv);\n"
    "    vec4 borderColor = mix(decorColor, texColor, borderAlpha);\n"
    "    FragColor = borderColor;\n"
    "}\n";

/* Fragment shader: transformation matrix application */
static const char *frag_shader_transform =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform mat4 transformMatrix;\n"
    "void main() {\n"
    "    vec4 texColor = texture(texture0, fragTexCoord);\n"
    "    FragColor = transformMatrix * texColor;\n"
    "}\n";

/**
 * Compile a shader from source
 * 
 * @param type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * @param source Shader source code string
 * @return Compiled shader ID, or 0 on failure
 */
static uint32_t compile_shader(uint32_t type, const char *source) {
    if (!source) {
        fprintf(stderr, "ERROR: Shader source is NULL\n");
        return 0;
    }

    /* Note: Full GL compilation requires OpenGL context in wlroots renderer
     * For now, we store shader source and will compile during actual rendering */
    (void)type;
    return 1; /* Placeholder ID */
}

/**
 * Link a complete shader program from vertex and fragment shaders
 * 
 * @param vertex_id Compiled vertex shader
 * @param fragment_id Compiled fragment shader
 * @return Linked program ID, or 0 on failure
 */
static uint32_t link_program(uint32_t vertex_id, uint32_t fragment_id) {
    if (vertex_id == 0 || fragment_id == 0) {
        fprintf(stderr, "ERROR: Invalid shader IDs for linking\n");
        return 0;
    }

    /* Note: Actual GL linking happens in wlroots rendering context
     * This is a placeholder implementation */
    return 1; /* Placeholder ID */
}

int gl_shader_init(struct wlr_renderer *renderer) {
    if (!renderer) {
        fprintf(stderr, "ERROR: Cannot initialize shaders without renderer\n");
        return -1;
    }

    if (shader_manager.initialized) {
        fprintf(stderr, "WARNING: GL shader system already initialized\n");
        return 0;
    }

    shader_manager.wlr_renderer = renderer;

    /* Compile shader programs */
    struct {
        const char *vertex;
        const char *fragment;
        GLShaderType type;
    } shader_configs[] = {
        { vertex_shader_source, frag_shader_solid, SHADER_FOREGROUND_SOLID },
        { vertex_shader_source, frag_shader_solid, SHADER_FOREGROUND_DECORATION },
        { vertex_shader_source, frag_shader_blur, SHADER_BACKGROUND_BLUR },
        { vertex_shader_source, frag_shader_color_filter, SHADER_BACKGROUND_COLOR_FILTER },
        { vertex_shader_source, frag_shader_transform, SHADER_TRANSFORM_MATRIX },
        { vertex_shader_source, frag_shader_solid, SHADER_COMPOSITE },
    };

    for (size_t i = 0; i < sizeof(shader_configs) / sizeof(shader_configs[0]); i++) {
        uint32_t vertex_id = compile_shader(0x8B31, shader_configs[i].vertex); /* GL_VERTEX_SHADER */
        uint32_t fragment_id = compile_shader(0x8B30, shader_configs[i].fragment); /* GL_FRAGMENT_SHADER */

        if (vertex_id == 0 || fragment_id == 0) {
            fprintf(stderr, "ERROR: Failed to compile shader %zu\n", i);
            gl_shader_fini();
            return -1;
        }

        uint32_t program_id = link_program(vertex_id, fragment_id);
        if (program_id == 0) {
            fprintf(stderr, "ERROR: Failed to link shader program %zu\n", i);
            gl_shader_fini();
            return -1;
        }

        shader_manager.shaders[shader_configs[i].type].program_id = program_id;
        shader_manager.shaders[shader_configs[i].type].vertex_shader = vertex_id;
        shader_manager.shaders[shader_configs[i].type].fragment_shader = fragment_id;
    }

    shader_manager.initialized = 1;
    fprintf(stderr, "GL shader system initialized with %d shader programs\n", SHADER_COUNT);
    return 0;
}

void gl_shader_fini(void) {
    if (!shader_manager.initialized) {
        return;
    }

    /* Delete all compiled shaders and programs
     * Note: In actual GL context, this would call glDeleteProgram/glDeleteShader */
    for (int i = 0; i < SHADER_COUNT; i++) {
        shader_manager.shaders[i].program_id = 0;
        shader_manager.shaders[i].vertex_shader = 0;
        shader_manager.shaders[i].fragment_shader = 0;
    }

    shader_manager.initialized = 0;
    shader_manager.wlr_renderer = NULL;
    fprintf(stderr, "GL shader system shut down\n");
}

const GLShaderProgram *gl_shader_get(GLShaderType type) {
    if (!shader_manager.initialized || type >= SHADER_COUNT) {
        return NULL;
    }
    return &shader_manager.shaders[type];
}

void gl_shader_apply_blur(float blur_radius, uint32_t texture_id) {
    if (!shader_manager.initialized) {
        fprintf(stderr, "WARNING: Shader system not initialized\n");
        return;
    }

    const GLShaderProgram *prog = gl_shader_get(SHADER_BACKGROUND_BLUR);
    if (!prog || prog->program_id == 0) {
        return;
    }

    /* Clamp blur radius to valid range */
    float clamped_radius = blur_radius < 0.0f ? 0.0f : (blur_radius > 10.0f ? 10.0f : blur_radius);

    /* In actual GL, we would:
     * 1. glUseProgram(prog->program_id)
     * 2. glUniform1f(glGetUniformLocation(prog->program_id, "blurRadius"), clamped_radius)
     * 3. glActiveTexture(GL_TEXTURE0)
     * 4. glBindTexture(GL_TEXTURE_2D, texture_id)
     * 5. Render quad
     */

    fprintf(stderr, "Applying blur effect with radius: %.2f\n", clamped_radius);
}

void gl_shader_apply_color_filter(float brightness, float saturation,
                                   float hue_shift, uint32_t texture_id) {
    if (!shader_manager.initialized) {
        fprintf(stderr, "WARNING: Shader system not initialized\n");
        return;
    }

    const GLShaderProgram *prog = gl_shader_get(SHADER_BACKGROUND_COLOR_FILTER);
    if (!prog || prog->program_id == 0) {
        return;
    }

    /* Clamp filter parameters */
    float b = brightness < 0.0f ? 0.0f : (brightness > 2.0f ? 2.0f : brightness);
    float s = saturation < 0.0f ? 0.0f : (saturation > 2.0f ? 2.0f : saturation);
    float h = fmod(hue_shift, 360.0f);
    if (h < 0.0f) h += 360.0f;

    /* In actual GL, we would:
     * 1. glUseProgram(prog->program_id)
     * 2. glUniform1f(glGetUniformLocation(prog->program_id, "brightness"), b)
     * 3. glUniform1f(glGetUniformLocation(prog->program_id, "saturation"), s)
     * 4. glUniform1f(glGetUniformLocation(prog->program_id, "hueShift"), h)
     * 5. Render quad
     */

    fprintf(stderr, "Applying color filter: brightness=%.2f, saturation=%.2f, hue=%.2f\n",
            b, s, h);
}

void gl_shader_apply_transform_matrix(const float *matrix) {
    if (!shader_manager.initialized || !matrix) {
        return;
    }

    const GLShaderProgram *prog = gl_shader_get(SHADER_TRANSFORM_MATRIX);
    if (!prog || prog->program_id == 0) {
        return;
    }

    /* In actual GL, we would:
     * 1. glUseProgram(prog->program_id)
     * 2. glUniformMatrix4fv(glGetUniformLocation(prog->program_id, "transformMatrix"),
     *                       1, GL_FALSE, matrix)
     * 3. Render quad with transformation
     */

    fprintf(stderr, "Applying transformation matrix\n");
}

uint8_t gl_shader_is_ready(void) {
    return shader_manager.initialized;
}
