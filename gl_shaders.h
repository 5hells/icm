#ifndef GL_SHADERS_H
#define GL_SHADERS_H

#include <stdint.h>
#include <wlr/render/wlr_renderer.h>

/**
 * GL Shader System for ICM Compositor
 * 
 * Provides rendering shaders for:
 * - Background effects (blur, color filters)
 * - Foreground compositing (windows, decorations)
 * - Screen transformations (rotation, scale, perspective)
 * 
 * Shaders are compiled once at startup and cached in memory.
 */

/* Shader types */
typedef enum {
    SHADER_BACKGROUND_BLUR,        /* Background blur for translucent surfaces */
    SHADER_BACKGROUND_COLOR_FILTER,/* Color/brightness effects on background */
    SHADER_FOREGROUND_SOLID,       /* Standard window rendering */
    SHADER_FOREGROUND_DECORATION,  /* Window decoration rendering */
    SHADER_TRANSFORM_MATRIX,       /* Apply transformation matrices */
    SHADER_COMPOSITE,              /* Composite multiple layers */
    SHADER_COUNT
} GLShaderType;

/* Shader program handle */
typedef struct {
    uint32_t program_id;
    uint32_t vertex_shader;
    uint32_t fragment_shader;
} GLShaderProgram;

/* Shader manager state */
typedef struct {
    GLShaderProgram shaders[SHADER_COUNT];
    uint8_t initialized;
    struct wlr_renderer *wlr_renderer;
} GLShaderManager;

/**
 * Initialize the GL shader system
 * Must be called once during compositor startup with a valid wlr_renderer
 * 
 * @param renderer The wlroots renderer instance
 * @return 0 on success, -1 on failure
 */
int gl_shader_init(struct wlr_renderer *renderer);

/**
 * Clean up and destroy all shaders
 * Must be called during compositor shutdown
 */
void gl_shader_fini(void);

/**
 * Get a compiled shader program by type
 * 
 * @param type The shader type to retrieve
 * @return Pointer to GLShaderProgram, or NULL if not initialized
 */
const GLShaderProgram *gl_shader_get(GLShaderType type);

/**
 * Apply blur effect to background using shader
 * 
 * @param blur_radius Amount of blur (0.0 = none, 1.0 = maximum)
 * @param texture_id The background texture to blur
 */
void gl_shader_apply_blur(float blur_radius, uint32_t texture_id);

/**
 * Apply color filter effect using shader
 * 
 * @param brightness Brightness multiplier (1.0 = normal, <1.0 = dark)
 * @param saturation Saturation multiplier (1.0 = normal, 0.0 = grayscale)
 * @param hue_shift Hue rotation in degrees
 * @param texture_id The source texture
 */
void gl_shader_apply_color_filter(float brightness, float saturation, 
                                   float hue_shift, uint32_t texture_id);

/**
 * Apply transformation matrix to rendered content
 * 
 * @param matrix 4x4 transformation matrix (column-major, 16 floats)
 */
void gl_shader_apply_transform_matrix(const float *matrix);

/**
 * Check if shaders are available and initialized
 * 
 * @return 1 if ready, 0 otherwise
 */
uint8_t gl_shader_is_ready(void);

#endif /* GL_SHADERS_H */
