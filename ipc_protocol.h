#ifndef ICM_IPC_PROTOCOL_H
#define ICM_IPC_PROTOCOL_H

#include <stdint.h>

#define ICM_IPC_VERSION 2
#define ICM_MAX_FDS_PER_MSG 4

enum icm_ipc_msg_type {
    /* Basic window management */
    ICM_MSG_CREATE_WINDOW = 1,
    ICM_MSG_DESTROY_WINDOW = 2,
    ICM_MSG_SET_WINDOW = 3,
    ICM_MSG_SET_LAYER = 4,
    ICM_MSG_SET_ATTACHMENTS = 5,
    ICM_MSG_DRAW_RECT = 6,
    ICM_MSG_CLEAR_RECTS = 7,
    
    /* DMABUF support */
    ICM_MSG_IMPORT_DMABUF = 8,
    ICM_MSG_EXPORT_DMABUF = 9,
    
    /* Fast drawing primitives */
    ICM_MSG_DRAW_LINE = 10,
    ICM_MSG_DRAW_CIRCLE = 11,
    ICM_MSG_DRAW_POLYGON = 12,
    ICM_MSG_DRAW_IMAGE = 13,
    ICM_MSG_BLIT_BUFFER = 14,
    
    /* Batch operations */
    ICM_MSG_BATCH_BEGIN = 15,
    ICM_MSG_BATCH_END = 16,
    
    /* Nested compositing */
    ICM_MSG_EXPORT_SURFACE = 17,
    ICM_MSG_IMPORT_SURFACE = 18,
    
    /* Buffer management */
    ICM_MSG_CREATE_BUFFER = 19,
    ICM_MSG_DESTROY_BUFFER = 20,
    ICM_MSG_QUERY_BUFFER_INFO = 21,

    /* Event registration */
    ICM_MSG_REGISTER_POINTER_EVENT = 22,
    ICM_MSG_REGISTER_KEYBOARD_EVENT = 23,
    ICM_MSG_QUERY_CAPTURE_MOUSE = 24,
    ICM_MSG_QUERY_CAPTURE_KEYBOARD = 25,

    /* Event messages from server */
    ICM_MSG_POINTER_EVENT = 26,
    ICM_MSG_KEYBOARD_EVENT = 27,

    /* Non-DMABUF image support */
    ICM_MSG_UPLOAD_IMAGE = 28,
    ICM_MSG_DESTROY_IMAGE = 29,
    ICM_MSG_DRAW_UPLOADED_IMAGE = 30,
    ICM_MSG_DRAW_TEXT = 31,

    /* Window visibility */
    ICM_MSG_SET_WINDOW_VISIBLE = 32,

    /* Keybinds */
    ICM_MSG_REGISTER_KEYBIND = 33,
    ICM_MSG_UNREGISTER_KEYBIND = 34,
    ICM_MSG_KEYBIND_EVENT = 35,

    /* Window events */
    ICM_MSG_WINDOW_CREATED = 36,
    ICM_MSG_WINDOW_DESTROYED = 37,

    /* Clickable regions */
    ICM_MSG_REGISTER_CLICK_REGION = 38,
    ICM_MSG_UNREGISTER_CLICK_REGION = 39,
    ICM_MSG_CLICK_REGION_EVENT = 40,

    /* Screen copy */
    ICM_MSG_REQUEST_SCREEN_COPY = 41,
    ICM_MSG_SCREEN_COPY_DATA = 42,

    /* Global event registration */
    ICM_MSG_REGISTER_GLOBAL_POINTER_EVENT = 43,
    ICM_MSG_REGISTER_GLOBAL_KEYBOARD_EVENT = 44,
    ICM_MSG_REGISTER_GLOBAL_CAPTURE_MOUSE = 45,
    ICM_MSG_REGISTER_GLOBAL_CAPTURE_KEYBOARD = 46,
    ICM_MSG_UNREGISTER_GLOBAL_CAPTURE_KEYBOARD = 58,
    ICM_MSG_UNREGISTER_GLOBAL_CAPTURE_MOUSE = 59,

    /* Window positioning/resizing */
    ICM_MSG_SET_WINDOW_POSITION = 47,
    ICM_MSG_SET_WINDOW_SIZE = 48,

    /* Window transformations */
    ICM_MSG_SET_WINDOW_OPACITY = 49,
    ICM_MSG_SET_WINDOW_TRANSFORM = 50,
    ICM_MSG_SET_WINDOW_BLUR = 78,
    ICM_MSG_SET_SCREEN_EFFECT = 79,
    ICM_MSG_SET_WINDOW_EFFECT = 80,

    /* Window layer management */
    ICM_MSG_SET_WINDOW_LAYER = 60,
    ICM_MSG_RAISE_WINDOW = 61,
    ICM_MSG_LOWER_WINDOW = 62,
    ICM_MSG_SET_WINDOW_PARENT = 63,

    /* Advanced 3D transformations */
    ICM_MSG_SET_WINDOW_TRANSFORM_3D = 64,
    ICM_MSG_SET_WINDOW_MATRIX = 65,

    /* Window state management */
    ICM_MSG_SET_WINDOW_STATE = 66,
    ICM_MSG_FOCUS_WINDOW = 67,
    ICM_MSG_BLUR_WINDOW = 83,

    /* Animation support */
    ICM_MSG_ANIMATE_WINDOW = 81,
    ICM_MSG_STOP_ANIMATION = 82,

    /* Window queries */
    ICM_MSG_QUERY_WINDOW_POSITION = 52,
    ICM_MSG_QUERY_WINDOW_SIZE = 53,
    ICM_MSG_QUERY_WINDOW_ATTRIBUTES = 54,
    ICM_MSG_QUERY_WINDOW_LAYER = 68,
    ICM_MSG_QUERY_WINDOW_STATE = 69,
    ICM_MSG_WINDOW_POSITION_DATA = 55,
    ICM_MSG_WINDOW_SIZE_DATA = 56,
    ICM_MSG_WINDOW_ATTRIBUTES_DATA = 57,
    ICM_MSG_WINDOW_LAYER_DATA = 70,
    ICM_MSG_WINDOW_STATE_DATA = 71,

    /* Screen and monitor queries */
    ICM_MSG_QUERY_SCREEN_DIMENSIONS = 72,
    ICM_MSG_SCREEN_DIMENSIONS_DATA = 73,
    ICM_MSG_QUERY_MONITORS = 74,
    ICM_MSG_MONITORS_DATA = 75,

    /* Compositor lifecycle */
    ICM_MSG_COMPOSITOR_SHUTDOWN = 51,

    ICM_MSG_QUERY_WINDOW_INFO = 76,
    ICM_MSG_WINDOW_INFO_DATA = 77,

    /* Mesh transformations (for wobbly windows, etc.) */
    ICM_MSG_SET_WINDOW_MESH_TRANSFORM = 84,
    ICM_MSG_CLEAR_WINDOW_MESH_TRANSFORM = 85,
    ICM_MSG_UPDATE_WINDOW_MESH_VERTICES = 86,

    /* Taskbar/shell queries */
    ICM_MSG_QUERY_TOPLEVEL_WINDOWS = 87,
    ICM_MSG_TOPLEVEL_WINDOWS_DATA = 88,
    ICM_MSG_SUBSCRIBE_WINDOW_EVENTS = 89,
    ICM_MSG_UNSUBSCRIBE_WINDOW_EVENTS = 90,
    ICM_MSG_WINDOW_TITLE_CHANGED = 91,
    ICM_MSG_WINDOW_STATE_CHANGED = 92,

    /* Window decorations (client-side) */
    ICM_MSG_SET_WINDOW_DECORATIONS = 93,
    ICM_MSG_REQUEST_WINDOW_DECORATIONS = 94,
    ICM_MSG_LAUNCH_APP = 95,
};

struct icm_ipc_header {
    uint32_t length;           /* Total message length including header */
    uint16_t type;
    uint16_t flags;
    uint32_t sequence;         /* For matching replies */
    int32_t num_fds;           /* Number of file descriptors following */
};

struct icm_msg_create_window {
    uint32_t window_id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t layer;
    uint32_t color_rgba;
};

struct icm_msg_destroy_window {
    uint32_t window_id;
};

struct icm_msg_set_window {
    uint32_t window_id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct icm_msg_set_layer {
    uint32_t window_id;
    uint32_t layer;
};

struct icm_msg_draw_rect {
    uint32_t window_id;
    uint32_t rect_id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t color_rgba;
};

struct icm_msg_put_pixels {
    uint32_t window_id;
    uint32_t rect_id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t pixels[];
};

struct icm_msg_clear_rects {
    uint32_t window_id;
};

/* DMABUF structures */
struct icm_msg_import_dmabuf {
    uint32_t buffer_id;
    int32_t width;
    int32_t height;
    uint32_t format;           /* DRM format code */
    uint32_t flags;
    uint32_t num_planes;       /* 1-4 planes */
    struct {
        int32_t fd;            /* File descriptor (sep.) */
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } planes[4];
};

struct icm_msg_export_dmabuf {
    uint32_t buffer_id;
    uint32_t flags;
};

struct icm_msg_export_dmabuf_reply {
    uint32_t buffer_id;
    int32_t width;
    int32_t height;
    uint32_t format;
    uint32_t num_planes;
    struct {
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } planes[4];
};

/* Fast drawing primitives */
struct icm_msg_draw_line {
    uint32_t window_id;
    int32_t x0, y0, x1, y1;
    uint32_t color_rgba;
    uint32_t thickness;
};

struct icm_msg_draw_circle {
    uint32_t window_id;
    int32_t cx, cy;
    uint32_t radius;
    uint32_t color_rgba;
    uint32_t fill;             /* 0 = outline, 1 = filled */
};

struct icm_msg_draw_polygon {
    uint32_t window_id;
    uint32_t num_points;
    uint32_t color_rgba;
    uint32_t fill;
    /* Points follow as array of (int32_t x, int32_t y) pairs */
};

struct icm_msg_draw_image {
    uint32_t window_id;
    uint32_t buffer_id;        /* Imported DMABUF buffer ID */
    int32_t x, y;
    uint32_t width, height;
    uint32_t src_x, src_y;
    uint32_t src_width, src_height;
    uint8_t alpha;             /* 0-255 */
};

struct icm_msg_blit_buffer {
    uint32_t window_id;
    uint32_t src_buffer_id;
    uint32_t dst_buffer_id;
    int32_t src_x, src_y;
    int32_t dst_x, dst_y;
    uint32_t width, height;
};

/* Batch operations */
struct icm_msg_batch_begin {
    uint32_t batch_id;
    uint32_t expected_commands;
};

struct icm_msg_batch_end {
    uint32_t batch_id;
};

/* Nested compositing support */
struct icm_msg_export_surface {
    uint32_t window_id;
    uint32_t surface_id;       /* Unique surface identifier */
    uint32_t flags;
};

struct icm_msg_import_surface {
    uint32_t surface_id;
    uint32_t window_id;        /* Where to attach imported surface */
    int32_t x, y;
    uint32_t width, height;
};

/* Buffer management */
struct icm_msg_create_buffer {
    uint32_t buffer_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;           /* DRM format code */
    uint32_t usage_flags;      /* GPU, CPU memory, etc */
};

struct icm_msg_destroy_buffer {
    uint32_t buffer_id;
};

struct icm_msg_query_buffer_info {
    uint32_t buffer_id;
};

struct icm_msg_query_buffer_info_reply {
    uint32_t buffer_id;
    int32_t width;
    int32_t height;
    uint32_t format;
    uint32_t size;
    uint32_t stride;
    int32_t mmap_fd;           /* For CPU access (sep.) */
};

/* Event registration */
struct icm_msg_register_pointer_event {
    uint32_t window_id;
};

struct icm_msg_register_keyboard_event {
    uint32_t window_id;
};

struct icm_msg_query_capture_mouse {
    uint32_t window_id;
};

struct icm_msg_query_capture_keyboard {
    uint32_t window_id;
};

/* Event messages from server */
struct icm_msg_pointer_event {
    uint32_t window_id;
    uint32_t time;
    uint32_t button;
    uint32_t state;
    int32_t x, y;
};

struct icm_msg_keyboard_event {
    uint32_t window_id;
    uint32_t time;
    uint32_t keycode;
    uint32_t state;
    uint32_t modifiers;  /* Modifier keys (e.g., Shift, Ctrl, Alt) */
};

/* Non-DMABUF image support */
struct icm_msg_upload_image {
    uint32_t image_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;           /* 0 = RGBA */
    uint32_t data_size;
    uint8_t data[];
};

struct icm_msg_destroy_image {
    uint32_t image_id;
};

struct icm_msg_draw_uploaded_image {
    uint32_t window_id;
    uint32_t image_id;
    int32_t x, y;
    uint32_t width, height;
    uint32_t src_x, src_y;
    uint32_t src_width, src_height;
    uint8_t alpha;
};

struct icm_msg_draw_text {
    uint32_t window_id;
    int32_t x, y;
    uint32_t color_rgba;
    uint32_t font_size;
    char text[];
};

/* Window visibility */
struct icm_msg_set_window_visible {
    uint32_t window_id;
    uint8_t visible;
};

/* Keybinds */
struct icm_msg_register_keybind {
    uint32_t keybind_id;
    uint32_t modifiers;
    uint32_t keycode;
};

struct icm_msg_unregister_keybind {
    uint32_t keybind_id;
};

struct icm_msg_keybind_event {
    uint32_t keybind_id;
};

/* Window events */
struct icm_msg_window_created {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint8_t decorated;      /* Whether window should have decorations */
    uint8_t focused;        /* Whether window is currently focused */
};

struct icm_msg_window_destroyed {
    uint32_t window_id;
};

/* Clickable regions */
struct icm_msg_register_click_region {
    uint32_t window_id;
    uint32_t region_id;
    int32_t x, y;
    uint32_t width, height;
};

struct icm_msg_unregister_click_region {
    uint32_t region_id;
};

struct icm_msg_click_region_event {
    uint32_t region_id;
    uint32_t button;
    uint32_t state;
};

/* Screen copy */
struct icm_msg_request_screen_copy {
    uint32_t request_id;
    uint32_t x, y;
    uint32_t width, height;
};

struct icm_msg_screen_copy_data {
    uint32_t request_id;
    uint32_t width, height;
    uint32_t format;
    uint32_t data_size;
    uint8_t data[];
};

/* Global event registration */
struct icm_msg_register_global_pointer_event {
    // No payload
};

struct icm_msg_register_global_keyboard_event {
    // No payload
};

struct icm_msg_register_global_capture_mouse {
    // No payload
};

struct icm_msg_register_global_capture_keyboard {
    // No payload
};

/* Window positioning/resizing */
struct icm_msg_set_window_position {
    uint32_t window_id;
    int32_t x, y;
};

struct icm_msg_set_window_size {
    uint32_t window_id;
    uint32_t width, height;
};

/* Window transformations */
struct icm_msg_set_window_opacity {
    uint32_t window_id;
    float opacity;
};

struct icm_msg_set_window_blur {
    uint32_t window_id;
    float blur_radius; // 0.0 = no blur, higher = more blur
    uint8_t enabled; // 0 = disabled, 1 = enabled
};

struct icm_msg_set_screen_effect {
    char equation[256]; // Mathematical equation for pixel manipulation, e.g. "r = r * 0.8; g = g * 0.8; b = b * 0.8"
    uint8_t enabled; // 0 = disabled, 1 = enabled
};

struct icm_msg_set_window_effect {
    uint32_t window_id;
    char equation[256]; // Mathematical equation for pixel manipulation
    uint8_t enabled; // 0 = disabled, 1 = enabled
};

struct icm_msg_set_window_transform {
    uint32_t window_id;
    float scale_x, scale_y;
    float rotation; // degrees
};

/* Window layer management */
struct icm_msg_set_window_layer {
    uint32_t window_id;
    int32_t layer; // z-order, higher values = more on top
};

struct icm_msg_raise_window {
    uint32_t window_id;
};

struct icm_msg_lower_window {
    uint32_t window_id;
};

struct icm_msg_set_window_parent {
    uint32_t window_id;
    uint32_t parent_id; // 0 for root
};

/* Advanced 3D transformations */
struct icm_msg_set_window_transform_3d {
    uint32_t window_id;
    float translate_x, translate_y, translate_z;
    float rotate_x, rotate_y, rotate_z; // degrees
    float scale_x, scale_y, scale_z;
};

struct icm_msg_set_window_matrix {
    uint32_t window_id;
    float matrix[16]; // 4x4 transformation matrix in column-major order
};

/* Window state management */
struct icm_msg_set_window_state {
    uint32_t window_id;
    uint32_t state; // bitfield: 1=minimized, 2=maximized, 4=fullscreen, 8=decorated
};

struct icm_msg_focus_window {
    uint32_t window_id;
};

struct icm_msg_blur_window {
    uint32_t window_id;
};

/* Animation support */
struct icm_msg_animate_window {
    uint32_t window_id;
    uint32_t duration_ms;
    float target_x, target_y;
    float target_scale_x, target_scale_y;
    float target_opacity;
    float target_translate_x, target_translate_y, target_translate_z;
    float target_rotate_x, target_rotate_y, target_rotate_z;
    float target_scale_z;
    uint32_t flags; // bitfield: 1=animate position, 2=animate scale, 4=animate opacity, 8=animate 3d translate, 16=animate 3d rotate, 32=animate 3d scale
};

struct icm_msg_stop_animation {
    uint32_t window_id;
};

/* Window queries */
struct icm_msg_query_window_position {
    uint32_t window_id;
};

struct icm_msg_query_window_size {
    uint32_t window_id;
};

struct icm_msg_query_window_attributes {
    uint32_t window_id;
};

struct icm_msg_query_window_layer {
    uint32_t window_id;
};

struct icm_msg_query_window_state {
    uint32_t window_id;
};

struct icm_msg_window_position_data {
    uint32_t window_id;
    int32_t x, y;
};

struct icm_msg_window_size_data {
    uint32_t window_id;
    uint32_t width, height;
};

struct icm_msg_window_attributes_data {
    uint32_t window_id;
    uint32_t visible;
    float opacity;
    float scale_x, scale_y;
    float rotation;
};

struct icm_msg_window_layer_data {
    uint32_t window_id;
    int32_t layer;
    uint32_t parent_id;
};

struct icm_msg_window_state_data {
    uint32_t window_id;
    uint32_t state;
    uint32_t focused;
};

/* Screen and monitor queries */
struct icm_msg_query_screen_dimensions {
    // No payload, just request
};

struct icm_msg_screen_dimensions_data {
    uint32_t total_width;
    uint32_t total_height;
    float scale;  // Global scale factor
};

struct icm_msg_query_monitors {
    // No payload, just request
};

/* Monitor info structure, fixed size */
struct icm_msg_monitor_info {
    int32_t x, y;              /* Position on virtual screen */
    uint32_t width, height;    /* Dimensions in pixels */
    uint32_t physical_width;   /* Physical size in mm */
    uint32_t physical_height;
    uint32_t refresh_rate;     /* In mHz (e.g., 60000 for 60Hz) */
    float scale;               /* DPI scale factor */
    uint8_t enabled;           /* Whether this monitor is enabled */
    uint8_t primary;           /* Whether this is the primary monitor */
    char name[32];             /* Monitor name/identifier */
};

struct icm_msg_monitors_data {
    uint32_t num_monitors;
    /* Followed by num_monitors * icm_msg_monitor_info structures */
};


struct icm_msg_query_window_info {
    uint32_t window_id;
};

/*

export interface IcmMsgWindowInfoData {
  windowId: number;
  x: number;
  y: number;
  width: number;
  height: number;
  visible: boolean;
  opacity: number;
  scaleX: number;
  scaleY: number;
  rotation: number;
  layer: number;
  parentId: number;
  state: number;
  focused: boolean;
  pid: number;
  processName: string;
}

*/
struct icm_msg_window_info_data {
    uint32_t window_id;
    int32_t x, y;
    uint32_t width, height;
    uint8_t visible;
    float opacity;
    float scale_x, scale_y;
    float rotation;
    int32_t layer;
    uint32_t parent_id;
    uint32_t state; // bitfield: 1=minimized, 2=maximized, 4=fullscreen, 8=decorated
    uint32_t focused; // boolean
    uint32_t pid; // Process ID of owning application
    char process_name[255]; // Name of owning process
};

/* Mesh transformation structures */
struct icm_msg_mesh_vertex {
    float x, y;        /* Position in normalized coordinates [0,1] */
    float u, v;        /* Texture coordinates [0,1] */
};

struct icm_msg_set_window_mesh_transform {
    uint32_t window_id;
    uint32_t mesh_width;   /* Number of vertices in width (e.g., 10 for 10x10 grid) */
    uint32_t mesh_height;  /* Number of vertices in height */
    /* Followed by mesh_width * mesh_height * icm_msg_mesh_vertex structures */
};

struct icm_msg_clear_window_mesh_transform {
    uint32_t window_id;
};

struct icm_msg_update_window_mesh_vertices {
    uint32_t window_id;
    uint32_t start_index;  /* Starting vertex index to update */
    uint32_t num_vertices; /* Number of vertices to update */
    /* Followed by num_vertices * icm_msg_mesh_vertex structures */
};

/* Shell/taskbar support structures */
struct icm_msg_query_toplevel_windows {
    uint32_t flags; /* 0 = all windows, 1 = visible only */
};

struct icm_msg_toplevel_window_entry {
    uint32_t window_id;
    int32_t x, y;
    uint32_t width, height;
    uint8_t visible;
    uint8_t focused;
    uint32_t state;
    char title[256];
    char app_id[128];
};

struct icm_msg_toplevel_windows_data {
    uint32_t num_windows;
    /* Followed by num_windows * icm_msg_toplevel_window_entry structures */
};

struct icm_msg_subscribe_window_events {
    uint32_t event_mask; /* bitfield: 1=created, 2=destroyed, 4=title, 8=state, 16=focus */
};

struct icm_msg_unsubscribe_window_events {
    uint32_t event_mask;
};

struct icm_msg_window_title_changed {
    uint32_t window_id;
    char title[256];
};

struct icm_msg_window_state_changed {
    uint32_t window_id;
    uint32_t state;
    uint8_t visible;
    uint8_t focused;
};

/* Window decorations */
struct icm_msg_set_window_decorations {
    uint32_t window_id;
    uint8_t server_side; /* 0 = client-side, 1 = server-side */
    uint32_t title_height;
    uint32_t border_width;
    uint32_t color_focused;
    uint32_t color_unfocused;
};

struct icm_msg_request_window_decorations {
    uint32_t window_id;
};

struct icm_msg_launch_app {
    uint32_t command_len;
    char command[];
};

#endif
