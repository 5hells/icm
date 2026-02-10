#include "transform_matrix.h"
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>

static struct wl_list matrix_transforms = { 0 };  // Head of linked list
static uint8_t matrix_transform_initialized = 0;

typedef struct {
    struct wl_list link;
    struct wlr_scene_buffer *scene_buffer;
    float matrix[16];
    uint8_t has_matrix;
} MatrixTransformEntry;

// Find or create an entry for a scene buffer
static MatrixTransformEntry *get_or_create_entry(struct wlr_scene_buffer *scene_buffer) {
    if (!matrix_transform_initialized) {
        return NULL;
    }

    // Search for existing entry
    MatrixTransformEntry *entry;
    wl_list_for_each(entry, &matrix_transforms, link) {
        if (entry->scene_buffer == scene_buffer) {
            return entry;
        }
    }

    // Create new entry
    entry = calloc(1, sizeof(MatrixTransformEntry));
    if (!entry) {
        return NULL;
    }

    entry->scene_buffer = scene_buffer;
    entry->has_matrix = 0;
    wl_list_insert(&matrix_transforms, &entry->link);
    return entry;
}

// Find an existing entry
static MatrixTransformEntry *find_entry(struct wlr_scene_buffer *scene_buffer) {
    if (!matrix_transform_initialized) {
        return NULL;
    }

    MatrixTransformEntry *entry;
    wl_list_for_each(entry, &matrix_transforms, link) {
        if (entry->scene_buffer == scene_buffer) {
            return entry;
        }
    }
    return NULL;
}

void matrix_transform_init(void) {
    if (!matrix_transform_initialized) {
        wl_list_init(&matrix_transforms);
        matrix_transform_initialized = 1;
    }
}

void matrix_transform_fini(void) {
    if (matrix_transform_initialized) {
        MatrixTransformEntry *entry, *tmp;
        wl_list_for_each_safe(entry, tmp, &matrix_transforms, link) {
            wl_list_remove(&entry->link);
            free(entry);
        }
        matrix_transform_initialized = 0;
    }
}

void wlr_scene_buffer_set_transform_matrix(struct wlr_scene_buffer *scene_buffer, 
                                           const float *matrix) {
    if (!scene_buffer || !matrix) {
        return;
    }

    MatrixTransformEntry *entry = get_or_create_entry(scene_buffer);
    if (!entry) {
        return;
    }

    // Copy matrix (column-major order, 16 floats for 4x4)
    memcpy(entry->matrix, matrix, 16 * sizeof(float));
    entry->has_matrix = 1;

    // This is where we would integrate with the renderer to actually apply the matrix
}

void wlr_scene_buffer_clear_transform_matrix(struct wlr_scene_buffer *scene_buffer) {
    if (!scene_buffer) {
        return;
    }

    MatrixTransformEntry *entry = find_entry(scene_buffer);
    if (!entry) {
        return;
    }

    entry->has_matrix = 0;
    memset(entry->matrix, 0, sizeof(entry->matrix));

}

const float *wlr_scene_buffer_get_transform_matrix(struct wlr_scene_buffer *scene_buffer) {
    if (!scene_buffer) {
        return NULL;
    }

    MatrixTransformEntry *entry = find_entry(scene_buffer);
    if (!entry || !entry->has_matrix) {
        return NULL;
    }

    return entry->matrix;
}

uint8_t wlr_scene_buffer_has_transform_matrix(struct wlr_scene_buffer *scene_buffer) {
    if (!scene_buffer) {
        return 0;
    }

    MatrixTransformEntry *entry = find_entry(scene_buffer);
    if (!entry) {
        return 0;
    }

    return entry->has_matrix;
}
