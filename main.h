#ifndef ICM_MAIN_H
#define ICM_MAIN_H

#include <wayland-util.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/xwayland.h>

/* Scene layer ordering */
enum SceneLayer {
    LyrBg,       /* Background layer (wallpaper) */
    LyrBottom,   /* Bottom layer */
    LyrNormal,   /* Normal windows (tiled/floating) */
    LyrTop,      /* Top layer */
    LyrOverlay,  /* Overlay layer (taskbar, notifications) */
    NUM_LAYERS
};

extern struct wlr_scene_tree *layers[NUM_LAYERS];

struct View {
    struct wl_list link;
    struct Server *server;
    bool is_xwayland;
    union {
        struct wlr_xdg_surface *xdg_surface;
        struct wlr_xwayland_surface *xwayland_surface;
    };
    struct wlr_scene_tree *scene_tree;
    double x, y;
    bool mapped;
    bool position_set_by_ipc;  /* Track if position was set via IPC command */
    uint32_t window_id;
    float opacity;
    float blur_radius;
    uint8_t blur_enabled;
    float scale_x, scale_y;
    float rotation;
    float transform_matrix[16];
    uint8_t has_transform_matrix;

    // Mesh transformation support
    struct {
        struct icm_msg_mesh_vertex *vertices;
        uint32_t mesh_width;
        uint32_t mesh_height;
        uint8_t enabled;
    } mesh_transform;

    // listeners
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
};

#endif
