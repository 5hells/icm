#ifndef ICM_LAYER_MANAGEMENT_H
#define ICM_LAYER_MANAGEMENT_H

#include <wlr/types/wlr_scene.h>
#include <wayland-server-core.h>

/* Layer organizational structure for proper compositing */
#define ICM_NUM_LAYERS 6

enum icm_layer_type {
    ICM_LAYER_BACKGROUND = 0,  /* Desktop background */
    ICM_LAYER_BOTTOM = 1,       /* Layer-shell bottom */
    ICM_LAYER_NORMAL = 2,       /* Normal windows (most content) */
    ICM_LAYER_TOP = 3,          /* Layer-shell top */
    ICM_LAYER_OVERLAY = 4,      /* Layer-shell overlay (notifications, etc)    */
    ICM_LAYER_CURSOR = 5        /* Cursor (topmost) */
};

struct ICMLayerGroup {
    struct wlr_scene_tree *tree;
    enum icm_layer_type layer_type;
    struct wl_list link;
};

struct ICMCompositorLayers {
    struct wlr_scene_tree *root;
    struct ICMLayerGroup layers[ICM_NUM_LAYERS];
};

/* Initialize layer management for a scene */
struct ICMCompositorLayers *icm_layers_create(struct wlr_scene *scene);

/* Destroy layer management */
void icm_layers_destroy(struct ICMCompositorLayers *layers);

/* Get layer tree for a specific layer type */
struct wlr_scene_tree *icm_layers_get_tree(struct ICMCompositorLayers *layers, 
                                          enum icm_layer_type type);

/* Add a node to a specific layer */
void icm_layers_add_node(struct ICMCompositorLayers *layers,
                         enum icm_layer_type type,
                         struct wlr_scene_node *node);

#endif
