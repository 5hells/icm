#include <wayland-util.h>
#include <stdint.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr-layer-shell-unstable-v1-protocol.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_input_device.h>
#include <wayland-server-protocol.h>
#include <stdlib.h>
#include <wayland-server.h>

/* Forward declarations */
struct Server;

struct wlr_renderer;

enum CursorMode {
    CURSOR_PASSTHROUGH,
    CURSOR_MOVE,
    CURSOR_RESIZE
};


struct LayerSurface;

struct BufferEntry
{
    struct wl_list link;
    uint32_t buffer_id;
    int32_t x, y;
    int32_t width;
    int32_t height;
    uint32_t format;
    void *data;
    size_t size;
    struct wlr_buffer *wlr_buffer;
    struct wlr_scene_buffer *scene_buffer;
    int dmabuf_fd;
    uint8_t visible;
    uint8_t dirty;  // Flag to indicate buffer content has changed
    float opacity;
    float blur_radius;
    uint8_t blur_enabled;
    uint8_t effect_enabled;
    uint8_t effect_dirty;
    uint8_t use_effect_buffer;
    char effect_equation[256];
    uint8_t *effect_data;
    size_t effect_data_size;
    float transform_matrix[16];
    uint8_t has_transform_matrix;
    float scale_x, scale_y;
    float rotation;
    uint8_t minimized;
    uint8_t maximized;
    uint8_t fullscreen;
    uint8_t decorated;
    uint8_t focused;
    int32_t layer;
    uint32_t parent_id;
    
    /* Animation state */
    uint8_t animating;
    uint32_t animation_start_time;
    uint32_t animation_duration;
    float start_opacity, target_opacity;
    float start_scale_x, start_scale_y, target_scale_x, target_scale_y;
    float start_x, start_y, target_x, target_y;
    float start_translate_x, start_translate_y, start_translate_z;
    float target_translate_x, target_translate_y, target_translate_z;
    float start_rotate_x, start_rotate_y, start_rotate_z;
    float target_rotate_x, target_rotate_y, target_rotate_z;
    float start_scale_z, target_scale_z;
    /* Current interpolated values during animation */
    float current_translate_x, current_translate_y, current_translate_z;
    float current_rotate_x, current_rotate_y, current_rotate_z;
    float current_scale_z;
    
    struct
    {
        int fd;
        uint32_t offset;
        uint32_t stride;
        uint64_t modifier;
    } planes[4];
    uint32_t num_planes;
};

/* Custom buffer implementation for IPC pixel data */
struct IPCPixelBuffer {
    struct wlr_buffer base;
    void *data;
    size_t size;
    int width, height;
    uint32_t format;
};

static void ipc_pixel_buffer_destroy(struct wlr_buffer *wlr_buffer) {
    struct IPCPixelBuffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    wlr_buffer_finish(wlr_buffer);
    // Don't free data here - it's managed by BufferEntry
    free(buffer);
}

static bool ipc_pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
        uint32_t flags, void **data, uint32_t *format, size_t *stride) {
    struct IPCPixelBuffer *buffer = wl_container_of(wlr_buffer, buffer, base);

    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
        return false; // Read-only for now
    }

    *format = buffer->format;
    *data = buffer->data;
    *stride = buffer->width * 4; // Assume RGBA
    return true;
}

static void ipc_pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
    // Nothing to do
}

static const struct wlr_buffer_impl ipc_pixel_buffer_impl = {
    .destroy = ipc_pixel_buffer_destroy,
    .begin_data_ptr_access = ipc_pixel_buffer_begin_data_ptr_access,
    .end_data_ptr_access = ipc_pixel_buffer_end_data_ptr_access
};

static struct wlr_buffer *ipc_buffer_create_wlr_buffer(void *data, int width, int height, uint32_t format) {
    struct IPCPixelBuffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) return NULL;

    wlr_buffer_init(&buffer->base, &ipc_pixel_buffer_impl, width, height);

    buffer->data = data;
    buffer->size = width * height * 4; // Assume RGBA
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;

    return &buffer->base;
}

struct ImageEntry {
    struct wl_list link;
    uint32_t image_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint8_t *data;
    size_t data_size;
};

struct KeybindEntry {
    struct wl_list link;
    uint32_t keybind_id;
    uint32_t modifiers;
    uint32_t keycode;
    struct IPCClient *client;
};

struct ClickRegion {
    struct wl_list link;
    uint32_t region_id;
    uint32_t window_id;
    int32_t x, y;
    uint32_t width, height;
    struct IPCClient *client;
};

struct ScreenCopyRequest {
    struct wl_list link;
    uint32_t request_id;
    uint32_t x, y, width, height;
    struct IPCClient *client;
};

struct IPCClient
{
    struct wl_list link;
    int socket_fd;
    struct wl_event_source *event_source;
    struct Server *server;
    uint8_t read_buffer[65536];
    size_t read_pos;

    uint32_t batch_id;
    int batching;

    /* Event registration */
    int registered_pointer;
    int registered_keyboard;
    uint32_t event_window_id;

    /* Global event registration */
    int registered_global_pointer;
    int registered_global_keyboard;
    int registered_global_capture_mouse;
    int registered_global_capture_keyboard;

    /* Window events subscription */
    uint32_t window_event_mask;  /* bitfield: 1=created, 2=destroyed, 4=title, 8=state, 16=focus */
};

struct IPCServer
{
    struct Server *server;
    int socket_fd;
    struct wl_event_source *event_source;
    struct wl_list clients;
    struct wl_list buffers;
    struct wl_list surfaces;
    struct wl_list images;
    struct wl_list keybinds;
    struct wl_list click_regions;
    struct wl_list screen_copy_requests;
    uint32_t next_buffer_id;
    uint32_t next_surface_id;
    uint32_t next_image_id;
    uint32_t next_keybind_id;
    uint32_t next_region_id;
    uint32_t next_window_id;
    char screen_effect_equation[256];
    uint8_t screen_effect_enabled;
    /* Background effect buffer for screen-wide effects */
    struct BufferEntry *screen_effect_buffer;
    uint8_t screen_effect_dirty;
    /* Decoration configuration */
    uint32_t decoration_border_width;   /* Width of decoration borders in pixels */
    uint32_t decoration_title_height;   /* Height of title bar in pixels */
    uint32_t decoration_color_focus;    /* RGBA color of focused window decorations */
    uint32_t decoration_color_unfocus;  /* RGBA color of unfocused window decorations */
    uint8_t  decoration_enabled;        /* Whether decorations are rendered by compositor */
};

struct Server {
    struct wl_event_loop *event_loop;
    struct wl_display *wl_display;
    struct wlr_session *wlr_session;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *compositor;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_output_layout;
    struct wlr_output_layout *output_layout;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_layer_shell_v1 *layer_shell;
    struct wlr_xwayland *xwayland;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_list views;
    struct wl_list layer_surfaces;
    struct wl_list outputs;
    struct wl_list keyboards;
    enum CursorMode cursor_mode;
    struct View *grabbed_view;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;
    struct wl_listener new_output;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_layer_surface;
    struct wl_listener new_xwayland_surface;
    struct wl_listener new_input;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct IPCServer ipc_server;
    int cursor_theme_loaded;
    uint32_t focused_window_id;  /* ID of currently focused window (0 = none) */
};

int ipc_server_init(struct IPCServer *ipc_server, struct Server *server, const char *socket_path);

void ipc_server_destroy(struct IPCServer *ipc_server);

int ipc_server_handle_client(int fd, uint32_t mask, void *data);

struct BufferEntry *ipc_buffer_create(struct IPCServer *ipc_server, uint32_t buffer_id,
                                      int32_t width, int32_t height, uint32_t format);
void ipc_buffer_destroy(struct IPCServer *ipc_server, uint32_t buffer_id);
struct BufferEntry *ipc_buffer_get(struct IPCServer *ipc_server, uint32_t buffer_id);

struct ImageEntry *ipc_image_create(struct IPCServer *ipc_server, uint32_t image_id,
                                    uint32_t width, uint32_t height, uint32_t format,
                                    const uint8_t *data, size_t data_size);
void ipc_image_destroy(struct IPCServer *ipc_server, uint32_t image_id);
struct ImageEntry *ipc_image_get(struct IPCServer *ipc_server, uint32_t image_id);

int send_event_to_client(struct IPCClient *client, uint16_t type, const void *payload, size_t payload_size);
void ipc_client_disconnect(struct IPCClient *client);

void ipc_server_broadcast_shutdown(struct IPCServer *ipc_server);

void ipc_check_keybind(struct IPCServer *ipc_server, uint32_t modifiers, uint32_t keycode);
void ipc_check_click_region(struct IPCServer *ipc_server, uint32_t window_id, int32_t x, int32_t y, uint32_t button, uint32_t state);
void ipc_window_unmap(struct IPCServer *ipc_server, uint32_t window_id);

void update_animations(struct IPCServer *ipc_server);

void apply_pixel_effect(uint8_t *pixels, size_t width, size_t height,
    const char *equation, double time_seconds);

struct LayerSurface
{
    struct wl_list link;
    struct Server *server;
    struct wlr_layer_surface_v1 *layer_surface;
    struct wlr_scene_layer_surface_v1 *scene_layer;
    struct wl_listener destroy;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener surface_commit;
    struct wl_listener output_destroy;
    struct wl_listener new_popup;
    uint32_t window_id;
};