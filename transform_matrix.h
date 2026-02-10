#ifndef TRANSFORM_MATRIX_H
#define TRANSFORM_MATRIX_H

#include <wlr/types/wlr_scene.h>
#include <stdint.h>
#include <math.h>

/**
 * Custom matrix transformation support for wlr_scene_buffers.
 * 
 * wlroots doesn't provide native matrix transformation support, so this system
 * provides a way to track and manage 4x4 transformation matrices for scene buffers.
 * 
 * For rendering, these matrices should be applied via:
 * - Direct OpenGL rendering with wlr_renderer hijacking
 * - Custom compositor matrix composition
 * - Per-buffer shader transformation
 */

struct SceneBufferMatrixTransform {
    struct wlr_scene_buffer *scene_buffer;
    float matrix[16];  // 4x4 matrix in column-major order
    uint8_t has_matrix;
};

/**
 * Initialize the matrix transformation system.
 * Should be called during compositor initialization.
 */
void matrix_transform_init(void);

/**
 * Cleanup the matrix transformation system.
 * Should be called during compositor shutdown.
 */
void matrix_transform_fini(void);

/**
 * Apply a 4x4 transformation matrix to a scene buffer.
 * 
 * The matrix should be in column-major order (standard for graphics).
 * 
 * @param scene_buffer The scene buffer to transform
 * @param matrix Pointer to 16 floats representing a 4x4 matrix
 */
void wlr_scene_buffer_set_transform_matrix(struct wlr_scene_buffer *scene_buffer, 
                                           const float *matrix);

/**
 * Clear the transformation matrix from a scene buffer.
 * Reset to identity (no transformation).
 * 
 * @param scene_buffer The scene buffer to clear transformation from
 */
void wlr_scene_buffer_clear_transform_matrix(struct wlr_scene_buffer *scene_buffer);

/**
 * Get the transformation matrix for a scene buffer.
 * Returns NULL if no matrix is set.
 * 
 * @param scene_buffer The scene buffer
 * @return Pointer to matrix data, or NULL if no matrix is set
 */
const float *wlr_scene_buffer_get_transform_matrix(struct wlr_scene_buffer *scene_buffer);

/**
 * Check if a scene buffer has a transformation matrix set.
 * 
 * @param scene_buffer The scene buffer
 * @return 1 if matrix is set, 0 otherwise
 */
uint8_t wlr_scene_buffer_has_transform_matrix(struct wlr_scene_buffer *scene_buffer);

#endif // TRANSFORM_MATRIX_H
