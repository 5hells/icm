#include "layer_management.h"
#include <stdlib.h>
#include <string.h>

struct ICMCompositorLayers *icm_layers_create(struct wlr_scene *scene) {
    struct ICMCompositorLayers *layers = calloc(1, sizeof(*layers));
    if (!layers) return NULL;

    layers->root = &scene->tree;

    /* Create a tree for each layer, nested to maintain ordering */
    for (int i = 0; i < ICM_NUM_LAYERS; i++) {
        layers->layers[i].tree = wlr_scene_tree_create(layers->root);
        if (!layers->layers[i].tree) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                if (layers->layers[j].tree) {
                    wlr_scene_node_destroy(&layers->layers[j].tree->node);
                }
            }
            free(layers);
            return NULL;
        }
        layers->layers[i].layer_type = i;
    }

    return layers;
}

void icm_layers_destroy(struct ICMCompositorLayers *layers) {
    if (!layers) return;

    /* The scene node destruction will cascade */
    for (int i = 0; i < ICM_NUM_LAYERS; i++) {
        if (layers->layers[i].tree) {
            wlr_scene_node_destroy(&layers->layers[i].tree->node);
            layers->layers[i].tree = NULL;
        }
    }

    free(layers);
}

struct wlr_scene_tree *icm_layers_get_tree(struct ICMCompositorLayers *layers,
                                           enum icm_layer_type type) {
    if (!layers || type < 0 || type >= ICM_NUM_LAYERS) {
        return NULL;
    }
    return layers->layers[type].tree;
}

void icm_layers_add_node(struct ICMCompositorLayers *layers,
                        enum icm_layer_type type,
                        struct wlr_scene_node *node) {
    if (!layers || !node || type < 0 || type >= ICM_NUM_LAYERS) {
        return;
    }

    struct wlr_scene_tree *tree = layers->layers[type].tree;
    if (tree) {
        /* Reparent the node to the appropriate layer tree */
        wlr_scene_node_reparent(node, tree);
    }
}
