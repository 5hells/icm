#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/signal.h>

#include <wayland-server.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr-layer-shell-unstable-v1-protocol.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <wlr/util/edges.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/util/box.h>
#include <wlr/render/allocator.h>
#include <wlr/xwayland.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/interfaces/wlr_output.h>

#include "ipc_server.h"
#include "ipc_protocol.h"
#include "transform_matrix.h"
#include "gl_shaders.h"
#include "main.h"
#include "signal.h"
#include <bits/sigaction.h>
#include <wayland-util.h>
#include <linux/time.h>

struct View;

/* Scene layer ordering — enum and extern declaration in main.h */
struct wlr_scene_tree *layers[NUM_LAYERS];

struct Output {
    struct wl_list link;
    struct Server *server;
    struct wlr_output *wlr_output;
    struct wlr_scene_output *scene_output;
    struct wl_listener frame;
    struct wl_listener destroy;
};

struct Keyboard {
    struct wl_list link;
    struct Server *server;
    struct wlr_input_device *device;
    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};

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

/* Check if a surface belongs to a layer shell surface */
static struct LayerSurface *layer_surface_at(struct Server *server, double lx, double ly)
{
    struct LayerSurface *layer_surf;
    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (!layer_surf->scene_layer || !layer_surf->layer_surface->surface->mapped) continue;
        
        double sx, sy;
        struct wlr_scene_node *node = wlr_scene_node_at(
            &layer_surf->scene_layer->tree->node, lx, ly, &sx, &sy);
        if (node && node->type == WLR_SCENE_NODE_BUFFER) {
            return layer_surf;
        }
    }
    return NULL;
}

static struct View *desktop_view_at(struct Server *server, double lx, double ly,
                                    struct wlr_surface **surface, double *sx, double *sy)
{
    struct wlr_scene_node *node = wlr_scene_node_at(
        &server->scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
    {
        return NULL;
    }
    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface)
    {
        return NULL;
    }

    *surface = scene_surface->surface;
    struct wl_list *views = &server->views;
    struct View *view;
    wl_list_for_each(view, views, link)
    {
        if ((view->is_xwayland ? view->xwayland_surface->surface : view->xdg_surface->surface) == *surface)
        {
            return view;
        }
    }
    return NULL;
}

/**
 * Set keyboard focus to a specific window
 * 
 * Handles the complete focus transition:
 * - Raises the target window to the top of the stacking order
 * - Deactivates the previously focused window (if any)
 * - Activates the new target window
 * - Notifies the Wayland seat of the focus change
 * 
 * @param view The View to focus
 * @param surface The wlr_surface to focus within the view
 */
static void focus_view(struct View *view, struct wlr_surface *surface) {
    if (!view || !view->mapped) return;

    struct Server *server = view->server;
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    if (!keyboard) return;

    struct wlr_surface *prev = server->seat->keyboard_state.focused_surface;

    /* Short-circuit if already focused */
    if (prev == surface) return;

    /* Raise the target window to the top of the Z-order */
    wlr_scene_node_raise_to_top(&view->scene_tree->node);
    wl_list_remove(&view->link);
    wl_list_insert(&server->views, &view->link);

    /* Deactivate the previously focused window */
    if (prev) {
        struct View *prev_view = NULL;
        struct View *v;
        wl_list_for_each(v, &server->views, link) {
            if ((v->is_xwayland ? v->xwayland_surface->surface : v->xdg_surface->surface) == prev) {
                prev_view = v;
                break;
            }
        }
        if (prev_view) {
            if (prev_view->is_xwayland) {
                wlr_xwayland_surface_activate(prev_view->xwayland_surface, false);
            } else {
                wlr_xdg_toplevel_set_activated(prev_view->xdg_surface->toplevel, false);
            }
        }
    }

    /* Activate the new window */
    if (view->is_xwayland) {
        wlr_xwayland_surface_activate(view->xwayland_surface, true);
    } else {
        wlr_xdg_toplevel_set_activated(view->xdg_surface->toplevel, true);
    }

    /* Notify the Wayland seat of the keyboard focus change */
    wlr_seat_keyboard_notify_enter(server->seat, surface,
        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

/**
 * Handle keyboard modifier key changes
 * 
 * Processes changes to modifier states (Shift, Ctrl, Alt, Super, etc.)
 * and notifies the Wayland seat. This ensures proper modifier tracking
 * for keyboard shortcuts and text input.
 * 
 * Only processes modifiers when there is an active keyboard focus to prevent
 * crashes with certain wlroots versions when modifiers arrive out-of-context.
 */
static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    struct Keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(keyboard->device);

    /* Only notify modifiers if there is active keyboard focus */
    if (keyboard->server->seat->keyboard_state.focused_surface) {
        wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &wlr_kb->modifiers);
    }
}

/**
 * Handle keyboard key events (press and release)
 * 
 * Processes both Wayland seat notifications and IPC client registrations.
 * Routes keyboard events to:
 * - Wayland seat (for standard input routing)
 * - Window-specific IPC clients (registered for specific windows)
 * - Global IPC clients (registered for all keyboard events)
 * - Keybinding system (for dynamic keybind dispatch)
 * 
 * Also handles F1 as a default window focus key when no focus exists.
 */
static void keyboard_handle_key(struct wl_listener *listener, void *data) {
    struct Keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    struct Server *server = keyboard->server;

    /* Ensure keyboard focus exists - fallback to topmost window if needed */
    if (!server->seat->keyboard_state.focused_surface) {
        if (!wl_list_empty(&server->views)) {
            struct View *view = wl_container_of(server->views.next, view, link);
            focus_view(view, view->is_xwayland ? view->xwayland_surface->surface : view->xdg_surface->surface);
        }
    }

    /* Notify the Wayland seat of the keyboard event */
    wlr_seat_keyboard_notify_key(server->seat, event->time_msec, event->keycode, event->state);

    /* Get current keyboard modifiers (Shift, Ctrl, Alt, etc.) */
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(keyboard->device);
    uint32_t mods = wlr_keyboard_get_modifiers(wlr_kb);

    /* Distribute keyboard event to window-specific IPC clients */
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link) {
        if (client->registered_keyboard) {
            struct icm_msg_keyboard_event kevent = {
                .window_id = client->event_window_id,
                .time = event->time_msec,
                .keycode = event->keycode,
                .state = event->state,
                .modifiers = mods
            };
            if (send_event_to_client(client, ICM_MSG_KEYBOARD_EVENT, &kevent, sizeof(kevent)) < 0) {
                fprintf(stderr, "Failed to send keyboard event, disconnecting client\n");
                ipc_client_disconnect(client);
            }
        }
        /* Also distribute to global keyboard listeners */
        if (client->registered_global_keyboard) {
            struct icm_msg_keyboard_event kevent = {
                .window_id = 0,  /* 0 indicates global scope */
                .time = event->time_msec,
                .keycode = event->keycode,
                .state = event->state,
                .modifiers = mods
            };
            if (send_event_to_client(client, ICM_MSG_KEYBOARD_EVENT, &kevent, sizeof(kevent)) < 0) {
                fprintf(stderr, "Failed to send global keyboard event, disconnecting client\n");
                ipc_client_disconnect(client);
            }
        }
    }

    /* On key press: check for registered keybindings and handle special keys */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        /* Check if this key combination matches any registered keybinds */
        ipc_check_keybind(&server->ipc_server, mods, event->keycode);

        /* Handle special keys (F1 = focus topmost window) */
        if (wlr_kb && wlr_kb->xkb_state) {
            uint32_t keycode = event->keycode + 8;  /* XKB uses keycode + 8 convention */
            xkb_keysym_t sym = xkb_state_key_get_one_sym(wlr_kb->xkb_state, keycode);

            if (sym == XKB_KEY_F1) {
                /* F1: Focus topmost window (fallback if focus isn't on a window) */
                if (!wl_list_empty(&server->views)) {
                    struct View *view = wl_container_of(server->views.next, view, link);
                    focus_view(view, view->is_xwayland ?
                               view->xwayland_surface->surface : view->xdg_surface->surface);
                }
            }
        }
    }
}

static void keyboard_destroy(struct wl_listener *listener, void *data);

static void server_new_keyboard(struct Server *server, struct wlr_input_device *device)
{
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(device);
    struct Keyboard *kb = calloc(1, sizeof(*kb));
    kb->server = server;
    kb->device = device;

    // Important: set a default keymap early to avoid xkb_state NULL crashes
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx)
    {
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (keymap)
        {
            wlr_keyboard_set_keymap(wlr_kb, keymap);
            xkb_keymap_unref(keymap);
        }
        xkb_context_unref(ctx);
    }

    wlr_keyboard_set_repeat_info(wlr_kb, 25, 600);

    kb->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&wlr_kb->events.modifiers, &kb->modifiers);
    kb->key.notify = keyboard_handle_key;
    wl_signal_add(&wlr_kb->events.key, &kb->key);
    kb->destroy.notify = keyboard_destroy;
    wl_signal_add(&device->events.destroy, &kb->destroy);

    wl_list_insert(&server->keyboards, &kb->link);
    wlr_seat_set_keyboard(server->seat, wlr_kb);
}

static void keyboard_destroy(struct wl_listener *listener, void *data)
{
    struct Keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void server_new_pointer(struct Server *server, struct wlr_input_device *device)
{
    wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    switch (device->type)
    {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    default:
        break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards))
    {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, caps);
}

static void on_map(struct wl_listener *listener, void *data)
{
    struct View *view = wl_container_of(listener, view, map);
    view->mapped = true;

    struct Server *server = view->server;
    struct wlr_output_layout *output_layout = server->output_layout;
    struct wlr_box output_box = {0, 0, 1920, 1080};
    if (output_layout && !wl_list_empty(&server->outputs)) {
        struct Output *output = wl_container_of(server->outputs.next, output, link);
        wlr_output_layout_get_box(output_layout, output->wlr_output, &output_box);
    }

    int window_width = view->xdg_surface->geometry.width ?: 400;
    int window_height = view->xdg_surface->geometry.height ?: 300;

    /* Only apply cascade positioning if position was not set by IPC client */
    if (!view->position_set_by_ipc) {
        static int cascade_x = 0;
        static int cascade_y = 0;
        const int step = 30;
        const int max_cascade = 5;

        view->x = output_box.x + cascade_x * step;
        view->y = output_box.y + cascade_y * step;

        if (view->x + window_width > output_box.x + output_box.width) {
            view->x = output_box.x;
        }
        if (view->y + window_height > output_box.y + output_box.height - 48) {
            view->y = output_box.y;
        }

        cascade_x = (cascade_x + 1) % max_cascade;
        if (cascade_x == 0) cascade_y = (cascade_y + 1) % max_cascade;

        wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
    }

    wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, window_width, window_height);
    focus_view(view, view->xdg_surface->surface);
}

/**
 * Handle window unmapping event
 * 
 * Called when a window becomes hidden (minimized, destroyed, etc.)
 * Marks the view as unmapped so it's excluded from focus/rendering.
 * Also cleans up any IPC event registrations for this window.
 */
static void on_unmap(struct wl_listener *listener, void *data)
{
    struct View *view = wl_container_of(listener, view, unmap);
    view->mapped = false;
    
    /* Clean up IPC event registrations for this window */
    if (view->window_id > 0) {
        ipc_window_unmap(&view->server->ipc_server, view->window_id);
    }
}

/**
 * Handle window surface commit event
 * 
 * Called when the window's surface state is committed (buffer attached, damage, etc.)
 * On initial commit, sends a configure event to let the client choose its size.
 * 
 * Note: wlr_scene handles geometry offsets internally, so we don't manually adjust.
 */
static void on_commit(struct wl_listener *listener, void *data)
{
    struct View *view = wl_container_of(listener, view, commit);
    if (view->xdg_surface->initial_commit)
    {
        /* Send initial configure without forcing a size - let client choose */
        wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, 0, 0);
    }
}

/**
 * Handle window destruction
 * 
 * Cleans up the view structure when a window is destroyed:
 * - Removes all event listeners
 * - Cleans up IPC event registrations
 * - Removes view from the window list
 * - Frees memory
 */
static void on_destroy(struct wl_listener *listener, void *data)
{
    struct View *view = wl_container_of(listener, view, destroy);
    
    /* Clean up IPC event registrations for this window */
    if (view->window_id > 0) {
        ipc_window_unmap(&view->server->ipc_server, view->window_id);
    }
    
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_move.link);
    wl_list_remove(&view->request_resize.link);
    wl_list_remove(&view->link);
    free(view);
}

/**
 * Handle window move request
 * 
 * Called when the client requests to start an interactive move (e.g., dragging titlebar).
 * Initiates cursor grabbing mode where cursor movement will update window position.
 * Stores the initial grab offset to maintain smooth dragging.
 */
static void on_request_move(struct wl_listener *listener, void *data)
{
    struct View *view = wl_container_of(listener, view, request_move);
    struct Server *server = view->server;
    server->grabbed_view = view;
    server->cursor_mode = CURSOR_MOVE;
    server->grab_x = server->cursor->x - view->x;
    server->grab_y = server->cursor->y - view->y;
}

/**
 * Handle window resize request
 * 
 * Called when the client requests to start an interactive resize (e.g., dragging edges).
 * Initiates cursor grabbing mode where cursor movement will resize the window.
 * Stores the edges being resized and initial geometry for calculating new dimensions.
 */
static void on_request_resize(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct View *view = wl_container_of(listener, view, request_resize);
    struct Server *server = view->server;
    server->grabbed_view = view;
    server->cursor_mode = CURSOR_RESIZE;
    server->grab_x = server->cursor->x - view->x;
    server->grab_y = server->cursor->y - view->y;
    server->resize_edges = event->edges;
    server->grab_geobox = view->xdg_surface->geometry;
}

static void arrange_layers(struct Server *server)
{
    struct LayerSurface *layer_surf;
    
    struct wlr_box full_area = {0, 0, 1920, 1080};
    if (!wl_list_empty(&server->outputs)) {
        struct Output *output = wl_container_of(server->outputs.next, output, link);
        if (output) {
            wlr_output_layout_get_box(server->output_layout, output->wlr_output, &full_area);
        }
    }
    
    struct wlr_box usable_area = full_area;
    
    /* Use proper protocol constants from wlr-layer-shell-unstable-v1-protocol.h */

    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (!layer_surf->scene_layer || !layer_surf->layer_surface->surface->mapped) continue;
        
        struct wlr_layer_surface_v1 *layer_surface = layer_surf->layer_surface;
        if (!layer_surface->initialized) continue;
        
        if (layer_surface->current.exclusive_zone > 0 &&
            layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
            
            uint32_t anchor = layer_surface->current.anchor;
            
            if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) && 
                !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
                usable_area.height -= layer_surface->current.exclusive_zone;
            }
            else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) &&
                     !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
                usable_area.y += layer_surface->current.exclusive_zone;
                usable_area.height -= layer_surface->current.exclusive_zone;
            }
            else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) &&
                     !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
                usable_area.x += layer_surface->current.exclusive_zone;
                usable_area.width -= layer_surface->current.exclusive_zone;
            }
            else if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) &&
                     !(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
                usable_area.width -= layer_surface->current.exclusive_zone;
            }
        }
    }
    
    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (!layer_surf->scene_layer || !layer_surf->layer_surface->surface->mapped) continue;
        if (!layer_surf->layer_surface->initialized) continue;
        
        wlr_scene_layer_surface_v1_configure(layer_surf->scene_layer, &full_area, &usable_area);
    }
}

static void layer_surface_map(struct wl_listener *listener, void *data)
{
    struct LayerSurface *layer_surf = wl_container_of(listener, layer_surf, map);
    layer_surf->layer_surface->surface->mapped = true;
    arrange_layers(layer_surf->server);
}

static void layer_surface_unmap(struct wl_listener *listener, void *data)
{
    struct LayerSurface *layer_surf = wl_container_of(listener, layer_surf, unmap);
    layer_surf->layer_surface->surface->mapped = false;
    
    /* Clean up IPC event registrations for this layer surface */
    if (layer_surf->window_id > 0) {
        ipc_window_unmap(&layer_surf->server->ipc_server, layer_surf->window_id);
    }
}

static void layer_surface_destroy(struct wl_listener *listener, void *data)
{
    struct LayerSurface *layer_surf = wl_container_of(listener, layer_surf, destroy);
    
    /* Clean up IPC event registrations for this layer surface */
    if (layer_surf->window_id > 0) {
        ipc_window_unmap(&layer_surf->server->ipc_server, layer_surf->window_id);
    }
    
    wl_list_remove(&layer_surf->map.link);
    wl_list_remove(&layer_surf->unmap.link);
    wl_list_remove(&layer_surf->destroy.link);
    wl_list_remove(&layer_surf->surface_commit.link);
    wl_list_remove(&layer_surf->new_popup.link);
    wl_list_remove(&layer_surf->link);
    free(layer_surf);
}

static void layer_surface_commit(struct wl_listener *listener, void *data)
{
    struct LayerSurface *layer_surf = wl_container_of(listener, layer_surf, surface_commit);
    struct wlr_layer_surface_v1 *layer_surface = layer_surf->layer_surface;
    
    if (layer_surface->initial_commit) {
        arrange_layers(layer_surf->server);
        return;
    }
    
    if (layer_surface->current.committed & (WLR_LAYER_SURFACE_V1_STATE_LAYER |
                                            WLR_LAYER_SURFACE_V1_STATE_EXCLUSIVE_ZONE)) {
        arrange_layers(layer_surf->server);
    }
}

static void layer_surface_new_popup(struct wl_listener *listener, void *data)
{
    struct LayerSurface *layer_surf = wl_container_of(listener, layer_surf, new_popup);
    struct wlr_xdg_popup *xdg_popup = data;

    /* Create scene tree for the popup */
    struct wlr_scene_tree *popup_tree = wlr_scene_xdg_surface_create(
        wlr_scene_tree_create(layers[LyrOverlay]), xdg_popup->base);

    if (!popup_tree)
    {
        wlr_log(WLR_ERROR, "Failed to create scene tree for layer surface popup");
        return;
    }

    /* Position the popup relative to its parent */
    wlr_scene_node_set_position(&popup_tree->node, xdg_popup->current.geometry.x, xdg_popup->current.geometry.y);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;
    
    struct View *view = calloc(1, sizeof(*view));
    view->server = server;
    view->xdg_surface = xdg_surface;
    view->window_id = server->ipc_server.next_window_id++;

    view->scene_tree = wlr_scene_xdg_surface_create(layers[LyrNormal], xdg_surface);
    view->x = 0;
    view->y = 0;
    view->opacity = 1.0f;
    view->blur_radius = 0.0f;
    view->blur_enabled = 0;
    view->scale_x = 1.0f;
    view->scale_y = 1.0f;
    view->rotation = 0.0f;
    view->has_transform_matrix = 0;
    wlr_scene_node_set_position(&view->scene_tree->node, 0, 0);

    view->map.notify = on_map;
    wl_signal_add(&xdg_surface->surface->events.map, &view->map);
    view->unmap.notify = on_unmap;
    wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
    view->commit.notify = on_commit;
    wl_signal_add(&xdg_surface->surface->events.commit, &view->commit);
    view->destroy.notify = on_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel)
    {
        struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
        view->request_move.notify = on_request_move;
        wl_signal_add(&toplevel->events.request_move, &view->request_move);
        view->request_resize.notify = on_request_resize;
        wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
    }

    wl_list_insert(&server->views, &view->link);
    
    /* Initialize position tracking state */
    view->position_set_by_ipc = false;

    // Send window created event
    struct icm_msg_window_created event = {
        .window_id = view->window_id,
        .width = 400, // Default size, will be updated when configured
        .height = 300};

    struct IPCClient *c, *tmp;
    wl_list_for_each_safe(c, tmp, &server->ipc_server.clients, link)
    {
        if (send_event_to_client(c, ICM_MSG_WINDOW_CREATED, &event, sizeof(event)) < 0)
        {
            /* Client disconnected, will be cleaned up elsewhere */
        }
    }
}

static void server_new_layer_surface(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *layer_surface = data;

    struct LayerSurface *layer_surf = calloc(1, sizeof(*layer_surf));
    layer_surf->server = server;
    layer_surf->layer_surface = layer_surface;
    layer_surf->window_id = server->ipc_server.next_window_id++;

    /* Place layer surfaces in appropriate scene layer based on wlr_layer_shell layer */
    struct wlr_scene_tree *parent_tree = layers[LyrNormal];
    switch (layer_surface->pending.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        parent_tree = layers[LyrBg];
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        parent_tree = layers[LyrBottom];
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        parent_tree = layers[LyrTop];
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        parent_tree = layers[LyrOverlay];
        break;
    }
    layer_surf->scene_layer = wlr_scene_layer_surface_v1_create(parent_tree, layer_surface);
    if (!layer_surf->scene_layer)
    {
        free(layer_surf);
        return;
    }

    layer_surf->map.notify = layer_surface_map;
    wl_signal_add(&layer_surface->surface->events.map, &layer_surf->map);

    layer_surf->unmap.notify = layer_surface_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &layer_surf->unmap);

    layer_surf->destroy.notify = layer_surface_destroy;
    wl_signal_add(&layer_surface->events.destroy, &layer_surf->destroy);

    layer_surf->surface_commit.notify = layer_surface_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &layer_surf->surface_commit);

    layer_surf->new_popup.notify = layer_surface_new_popup;
    wl_signal_add(&layer_surface->events.new_popup, &layer_surf->new_popup);

    wl_list_insert(&server->layer_surfaces, &layer_surf->link);

    // Send window created event
    struct icm_msg_window_created event = {
        .window_id = layer_surf->window_id,
        .width = layer_surface->current.desired_width,
        .height = layer_surface->current.desired_height};

    struct IPCClient *c, *tmp;
    wl_list_for_each_safe(c, tmp, &server->ipc_server.clients, link)
    {
        if (send_event_to_client(c, ICM_MSG_WINDOW_CREATED, &event, sizeof(event)) < 0)
        {
            /* Client disconnected, will be cleaned up elsewhere */
        }
    }
}

static void server_new_xwayland_surface(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, new_xwayland_surface);
    struct wlr_xwayland_surface *xwayland_surface = data;

    /* Skip override-redirect windows (popups, menus, etc.) */
    if (xwayland_surface->override_redirect) {
        return;
    }

    struct View *view = calloc(1, sizeof(*view));
    view->server = server;
    view->is_xwayland = true;
    view->xwayland_surface = xwayland_surface;
    view->window_id = server->ipc_server.next_window_id++;

    view->scene_tree = wlr_scene_tree_create(layers[LyrNormal]);
    struct wlr_scene_surface *scene_surface = wlr_scene_surface_create(view->scene_tree, xwayland_surface->surface);
    if (!scene_surface) {
        wlr_scene_node_destroy(&view->scene_tree->node);
        free(view);
        return;
    }
    view->x = 0;
    view->y = 0;
    view->opacity = 1.0f;
    view->blur_radius = 0.0f;
    view->blur_enabled = 0;
    view->scale_x = 1.0f;
    view->scale_y = 1.0f;
    view->rotation = 0.0f;
    view->has_transform_matrix = 0;
    wlr_scene_node_set_position(&view->scene_tree->node, 0, 0);

    view->map.notify = on_map;
    wl_signal_add(&xwayland_surface->surface->events.map, &view->map);
    view->unmap.notify = on_unmap;
    wl_signal_add(&xwayland_surface->surface->events.unmap, &view->unmap);
    view->commit.notify = on_commit;
    wl_signal_add(&xwayland_surface->surface->events.commit, &view->commit);
    view->destroy.notify = on_destroy;
    wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);

    wl_list_insert(&server->views, &view->link);
    
    /* Initialize position tracking state */
    view->position_set_by_ipc = false;

    // Send window created event
    struct icm_msg_window_created event = {
        .window_id = view->window_id,
        .width = xwayland_surface->width,
        .height = xwayland_surface->height};

    struct IPCClient *c, *tmp;
    wl_list_for_each_safe(c, tmp, &server->ipc_server.clients, link)
    {
        if (send_event_to_client(c, ICM_MSG_WINDOW_CREATED, &event, sizeof(event)) < 0)
        {
            /* Client disconnected, will be cleaned up elsewhere */
        }
    }
}

void process_screen_copy_requests(struct IPCServer *ipc_server)
{
    struct ScreenCopyRequest *req, *tmp;
    wl_list_for_each_safe(req, tmp, &ipc_server->screen_copy_requests, link)
    {
        // Dummy data: create a red rectangle
        uint32_t width = req->width;
        uint32_t height = req->height;
        size_t data_size = width * height * 4;
        uint8_t *data = malloc(data_size);
        if (!data)
            continue;
        for (size_t i = 0; i < data_size; i += 4)
        {
            data[i] = 255;     // R
            data[i + 1] = 0;   // G
            data[i + 2] = 0;   // B
            data[i + 3] = 255; // A
        }
        
        // Apply screen effect if enabled
        if (ipc_server->screen_effect_enabled && strlen(ipc_server->screen_effect_equation) > 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double time_seconds = now.tv_sec + now.tv_nsec / 1000000000.0;
            apply_pixel_effect(data, width, height, ipc_server->screen_effect_equation,
                time_seconds);
        }
        
        struct icm_msg_screen_copy_data msg = {
            .request_id = req->request_id,
            .width = width,
            .height = height,
            .format = 0, // RGBA
            .data_size = data_size};
        // Send header + data
        uint8_t *buf = malloc(sizeof(msg) + data_size);
        if (!buf)
        {
            free(data);
            continue;
        }
        memcpy(buf, &msg, sizeof(msg));
        memcpy(buf + sizeof(msg), data, data_size);
        send_event_to_client(req->client, ICM_MSG_SCREEN_COPY_DATA, buf, sizeof(msg) + data_size);
        free(data);
        free(buf);
        wl_list_remove(&req->link);
        free(req);
    }
}

static void render_ipc_buffers(struct Output *output)
{
    struct Server *server = output->server;
    struct IPCServer *ipc_server = &server->ipc_server;

    struct BufferEntry *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &ipc_server->buffers, link)
    {
        if (!buffer->visible)
        {
            // Remove from scene if it exists
            if (buffer->scene_buffer)
            {
                wlr_scene_node_destroy(&buffer->scene_buffer->node);
                buffer->scene_buffer = NULL;
            }
            if (buffer->wlr_buffer)
            {
                wlr_buffer_drop(buffer->wlr_buffer);
                buffer->wlr_buffer = NULL;
            }
            continue;
        }

        if (!buffer->data)
            continue;

        bool wants_effect = buffer->effect_enabled && buffer->effect_equation[0] != '\0';
        if (wants_effect) {
            size_t needed = buffer->width * buffer->height * 4;
            if (!buffer->effect_data || buffer->effect_data_size != needed) {
                free(buffer->effect_data);
                buffer->effect_data = malloc(needed);
                buffer->effect_data_size = needed;
                buffer->effect_dirty = 1;
            }
        }

        if (wants_effect && (buffer->dirty || buffer->effect_dirty)) {
            memcpy(buffer->effect_data, buffer->data, buffer->size);
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double time_seconds = now.tv_sec + now.tv_nsec / 1000000000.0;
            apply_pixel_effect(buffer->effect_data, buffer->width, buffer->height,
                buffer->effect_equation, time_seconds);
            buffer->effect_dirty = 0;
        }

        if (buffer->use_effect_buffer != wants_effect) {
            buffer->use_effect_buffer = wants_effect;
            if (buffer->scene_buffer) {
                wlr_scene_node_destroy(&buffer->scene_buffer->node);
                buffer->scene_buffer = NULL;
            }
            if (buffer->wlr_buffer) {
                wlr_buffer_drop(buffer->wlr_buffer);
                buffer->wlr_buffer = NULL;
            }
        }

        // Create wlr_buffer if not exists
        if (!buffer->wlr_buffer)
        {
            uint8_t *render_data = buffer->use_effect_buffer ? buffer->effect_data : buffer->data;
            buffer->wlr_buffer = ipc_buffer_create_wlr_buffer(render_data, buffer->width, buffer->height, 0x34325241); // ARGB
            if (!buffer->wlr_buffer)
            {
                fprintf(stderr, "Failed to create wlr_buffer for buffer %u\n", buffer->buffer_id);
                continue;
            }
            fprintf(stderr, "Created wlr_buffer for buffer %u (%dx%d)\n", buffer->buffer_id, buffer->width, buffer->height);
        }

        // Create scene buffer if not exists
        if (!buffer->scene_buffer)
        {
            /* Place IPC buffers in the normal layer by default;
             * handle_set_window_layer can reparent them later */
            buffer->scene_buffer = wlr_scene_buffer_create(layers[LyrNormal], buffer->wlr_buffer);
            if (!buffer->scene_buffer)
            {
                fprintf(stderr, "Failed to create scene buffer for buffer %u\n", buffer->buffer_id);
                wlr_buffer_drop(buffer->wlr_buffer);
                buffer->wlr_buffer = NULL;
                continue;
            }
            fprintf(stderr, "Created scene_buffer for buffer %u\n", buffer->buffer_id);
        }

        // If buffer was modified, update the scene
        if (buffer->dirty)
        {
            // Re-set the buffer to signal changes
            wlr_scene_buffer_set_buffer(buffer->scene_buffer, buffer->wlr_buffer);
            buffer->dirty = 0;
        }

        // Apply transformations
        wlr_scene_node_set_position(&buffer->scene_buffer->node, buffer->x, buffer->y);

        // For scaling - set destination size
        wlr_scene_buffer_set_dest_size(buffer->scene_buffer,
                                       (int)(buffer->width * buffer->scale_x), (int)(buffer->height * buffer->scale_y));

        // For opacity
        wlr_scene_buffer_set_opacity(buffer->scene_buffer, buffer->opacity);

        if (buffer->has_transform_matrix) {
            wlr_scene_buffer_set_transform_matrix(buffer->scene_buffer, buffer->transform_matrix);
        } else {
            wlr_scene_buffer_clear_transform_matrix(buffer->scene_buffer);
        }
    }
}

static void render_screen_effect(struct Output *output)
{
    struct Server *server = output->server;
    struct IPCServer *ipc_server = &server->ipc_server;

    if (!ipc_server->screen_effect_enabled || ipc_server->screen_effect_equation[0] == '\0') {
        /* Clean up screen effect buffer if effect is disabled */
        if (ipc_server->screen_effect_buffer) {
            ipc_buffer_destroy(ipc_server, ipc_server->screen_effect_buffer->buffer_id);
            ipc_server->screen_effect_buffer = NULL;
        }
        return;
    }

    /* Get output dimensions */
    struct wlr_output *wlr_output = output->wlr_output;
    int width = wlr_output->width;
    int height = wlr_output->height;

    /* Create or recreate buffer if dimensions changed */
    if (!ipc_server->screen_effect_buffer ||
        ipc_server->screen_effect_buffer->width != width ||
        ipc_server->screen_effect_buffer->height != height) {
        
        if (ipc_server->screen_effect_buffer) {
            ipc_buffer_destroy(ipc_server, ipc_server->screen_effect_buffer->buffer_id);
        }
        
        uint32_t effect_buffer_id = ipc_server->next_buffer_id++;
        ipc_server->screen_effect_buffer = ipc_buffer_create(ipc_server, effect_buffer_id, 
                                                              width, height, 0x34325241);
        if (!ipc_server->screen_effect_buffer) {
            fprintf(stderr, "Failed to create screen effect buffer\n");
            return;
        }
        
        /* Initialize buffer with a base color (e.g., black) */
        memset(ipc_server->screen_effect_buffer->data, 0, ipc_server->screen_effect_buffer->size);
        ipc_server->screen_effect_buffer->visible = 1;
        ipc_server->screen_effect_buffer->layer = 0; /* Background layer */
        ipc_server->screen_effect_buffer->opacity = 1.0f;
        ipc_server->screen_effect_dirty = 1;
        
        fprintf(stderr, "Created screen effect buffer %ux%u\n", width, height);
    }

    struct BufferEntry *buffer = ipc_server->screen_effect_buffer;
    
    /* Apply effect if dirty */
    if (ipc_server->screen_effect_dirty) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double time_seconds = now.tv_sec + now.tv_nsec / 1000000000.0;
        
        /* Fill buffer with animated pattern based on effect equation */
        apply_pixel_effect(buffer->data, buffer->width, buffer->height,
                          ipc_server->screen_effect_equation, time_seconds);
        
        buffer->dirty = 1;
        ipc_server->screen_effect_dirty = 0;
    }
    
    /* Create/update wlr_buffer if needed */
    if (!buffer->wlr_buffer || buffer->dirty) {
        if (buffer->wlr_buffer) {
            wlr_buffer_drop(buffer->wlr_buffer);
        }
        buffer->wlr_buffer = ipc_buffer_create_wlr_buffer(buffer->data, buffer->width, 
                                                           buffer->height, 0x34325241);
        if (!buffer->wlr_buffer) {
            fprintf(stderr, "Failed to create wlr_buffer for screen effect\n");
            return;
        }
    }
    
    /* Create/update scene buffer if needed */
    if (!buffer->scene_buffer) {
        buffer->scene_buffer = wlr_scene_buffer_create(layers[LyrBg], buffer->wlr_buffer);
        if (!buffer->scene_buffer) {
            fprintf(stderr, "Failed to create scene buffer for screen effect\n");
            return;
        }
    }
    
    if (buffer->dirty) {
        wlr_scene_buffer_set_buffer(buffer->scene_buffer, buffer->wlr_buffer);
        buffer->dirty = 0;
    }
    
    /* Position at origin (fullscreen) */
    wlr_scene_node_set_position(&buffer->scene_buffer->node, 0, 0);
    wlr_scene_buffer_set_opacity(buffer->scene_buffer, buffer->opacity);
    
    /* Mark dirty for next frame to create animated effects */
    ipc_server->screen_effect_dirty = 1;
}

static void output_frame(struct wl_listener *listener, void *data)
{
    struct Output *output = wl_container_of(listener, output, frame);

    // Update animations
    update_animations(&output->server->ipc_server);

    // Render screen-wide background effect
    render_screen_effect(output);

    // Render IPC buffers to scene
    render_ipc_buffers(output);

    wlr_scene_output_commit(output->scene_output, NULL);

    // Process screen copy requests after rendering
    process_screen_copy_requests(&output->server->ipc_server);

    // Load cursor theme after first frame if not loaded and not nested
    if (!output->server->cursor_theme_loaded && !wlr_backend_is_wl(output->server->backend))
    {
        wlr_xcursor_manager_load(output->server->cursor_mgr, 1.0f);
        output->server->cursor_theme_loaded = 1;
        // Set default cursor image
        wlr_cursor_set_xcursor(output->server->cursor, output->server->cursor_mgr, "default");
    }
}

static void output_destroy(struct wl_listener *listener, void *data)
{
    struct Output *output = wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

static void server_new_output(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    /* Initialize output for rendering - critical for proper output setup */
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    /* Configure output mode and enable it */
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    if (!wl_list_empty(&wlr_output->modes))
    {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        if (mode)
        {
            wlr_output_state_set_mode(&state, mode);
        }
    }

    if (!wlr_output_commit_state(wlr_output, &state))
    {
        wlr_log(WLR_ERROR, "Failed to commit output");
        wlr_output_state_finish(&state);
        return;
    }
    wlr_output_state_finish(&state);

    wlr_log(WLR_INFO, "Output %s initialized: %dx%d @ %dmHz",
            wlr_output->name, wlr_output->width, wlr_output->height, wlr_output->refresh);

    struct Output *output = calloc(1, sizeof(*output));
    output->server = server;
    output->wlr_output = wlr_output;
    output->scene_output = wlr_scene_output_create(server->scene, wlr_output);

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);
    wlr_output_layout_add_auto(server->output_layout, wlr_output);

    struct wlr_output_layout_output *lo = wlr_output_layout_get(server->output_layout, wlr_output);
    if (lo)
    {
        wlr_scene_output_layout_add_output(server->scene_output_layout, lo, output->scene_output);
    }
}

/**
 * Cursor surface information structure
 * 
 * Stores details about the surface under the cursor cursor, including:
 * - The wlr_surface pointer
 * - Surface-relative coordinates (sx, sy)
 * - Window ID for routing IPC events
 * - Pointers to View or LayerSurface (if applicable)
 * 
 * Used by process_cursor_motion to provide information to cursor event handlers
 * and IPC client notification routines.
 */
struct CursorSurfaceInfo {
    struct wlr_surface *surface;              /* Wayland surface under cursor */
    double sx, sy;                             /* Surface-local coordinates */
    uint32_t window_id;                        /* Window ID for IPC routing (0 = no specific window) */
    struct View *view;                         /* Pointer to View (NULL if not an XDG/app window) */
    struct LayerSurface *layer_surf;  /* Pointer to LayerSurface if it's a layer surface */
};

/**
 * Route pointer focus based on cursor position
 * 
 * Searches the scene layers from top to bottom to find the topmost surface
 * under the cursor. Updates the Wayland seat's pointer focus and sends enter/motion
 * events to maintain proper pointer state.
 * 
 * Returns information about the surface under the cursor, including:
 * - Surface pointer
 * - Surface-relative coordinates (sx, sy)
 * - Window ID (for IPC coordinate routing)
 * - View/LayerSurface pointers (for window identification)
 * 
 * @param server The compositor server
 * @param time Event timestamp in milliseconds
 * @return CursorSurfaceInfo structure with surface and coordinate details
 */
static struct CursorSurfaceInfo process_cursor_motion(struct Server *server, uint32_t time)
{
    struct CursorSurfaceInfo info = {0};
    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct wlr_scene_node *node = NULL;

    /* Search layers top-to-bottom for the first surface under the cursor */
    for (int layer = NUM_LAYERS - 1; layer >= 0; layer--) {
        node = wlr_scene_node_at(&layers[layer]->node,
                                  server->cursor->x, server->cursor->y, &sx, &sy);
        if (!node || node->type != WLR_SCENE_NODE_BUFFER)
            continue;
        struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *scene_surface =
            wlr_scene_surface_try_from_buffer(scene_buffer);
        if (scene_surface) {
            surface = scene_surface->surface;
            info.sx = sx;
            info.sy = sy;
            break;  /* Found a surface, use it */
        }
    }

    /* No surface under cursor: set default cursor and clear pointer focus */
    if (!surface) {
        if (server->cursor_mgr && server->cursor_theme_loaded)
            wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
        wlr_seat_pointer_notify_clear_focus(server->seat);
        info.surface = NULL;
        return info;
    }

    info.surface = surface;

    /* Identify which window this surface belongs to (IPC buffer, View, or LayerSurface) */
    struct wlr_surface *root_surface = wlr_surface_get_root_surface(surface);
    
    /* Check if it's an IPC-controlled buffer */
    struct BufferEntry *buffer = NULL;
    struct Server *server_ptr = wl_container_of(&server->ipc_server, server_ptr, ipc_server);
    wl_list_for_each(buffer, &server->ipc_server.buffers, link) {
        if (buffer->scene_buffer) {
            struct wlr_scene_node *buf_node = &buffer->scene_buffer->node;
            struct wlr_scene_node *check_node = node;
            while (check_node) {
                if (check_node == buf_node) {
                    info.window_id = buffer->buffer_id;
                    break;
                }
                struct wl_list c = check_node->parent->children;
                check_node = wl_container_of(c.next, check_node, link);
            }
            if (info.window_id) break;
        }
    }

    /* Check if it's a regular XDG/Wayland application window */
    if (!info.window_id) {
        struct View *v;
        wl_list_for_each(v, &server->views, link) {
            struct wlr_surface *vs = v->is_xwayland ?
                v->xwayland_surface->surface : v->xdg_surface->surface;
            if (vs == root_surface && v->mapped) {
                info.view = v;
                info.window_id = v->window_id;
                break;
            }
        }
    }

    /* Check if it's a layer shell surface (panels, notifications, etc.) */
    if (!info.window_id) {
        struct LayerSurface *layer_surf;
        wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
            if (layer_surf->layer_surface->surface == root_surface) {
                info.layer_surf = layer_surf;
                info.window_id = layer_surf->window_id;
                break;
            }
        }
    }

    /* Update Wayland seat pointer focus: enter first, then motion */
    wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);

    return info;
}

/**
 * Handle relative pointer motion events
 * 
 * Processes relative mouse movements (delta_x, delta_y) and:
 * - Moves the cursor in the correct direction
 * - Updates pointer focus based on new position
 * - Sends motion events to IPC clients (both window-specific and global)
 * - Handles window move operations
 * - Handles window resize operations
 */
static void cursor_motion(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);

    /* Determine what surface is under the cursor */
    struct CursorSurfaceInfo surface_info = process_cursor_motion(server, event->time_msec);

    /* Send pointer motion to window-specific IPC clients */
    if (surface_info.surface && surface_info.window_id > 0) {
        struct IPCClient *client, *tmp;
        wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link) {
            if (client->registered_pointer && client->event_window_id == surface_info.window_id) {
                struct icm_msg_pointer_event pevent = {
                    .window_id = surface_info.window_id,
                    .time = event->time_msec,
                    .button = 0,
                    .state = 0,
                    .x = (int32_t)surface_info.sx,  /* Surface-relative coordinates */
                    .y = (int32_t)surface_info.sy
                };
                if (send_event_to_client(client, ICM_MSG_POINTER_EVENT, &pevent, sizeof(pevent)) < 0) {
                    fprintf(stderr, "Failed to send pointer motion event, disconnecting client\n");
                    ipc_client_disconnect(client);
                }
            }
        }
    }

    /* Send global pointer motion to clients listening for all pointers */
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link) {
        if (client->registered_global_pointer) {
            struct icm_msg_pointer_event pevent = {
                .window_id = surface_info.window_id,
                .time = event->time_msec,
                .button = 0,
                .state = 0,
                .x = (int32_t)server->cursor->x,  /* Global coordinates */
                .y = (int32_t)server->cursor->y
            };
            if (send_event_to_client(client, ICM_MSG_POINTER_EVENT, &pevent, sizeof(pevent)) < 0) {
                fprintf(stderr, "Failed to send global pointer motion event, disconnecting client\n");
                ipc_client_disconnect(client);
            }
        }
    }

    /* Handle window move operation: cursor has grabbed a window to drag it */
    if (server->cursor_mode == CURSOR_MOVE && server->grabbed_view)
    {
        struct View *view = server->grabbed_view;
        view->x = server->cursor->x - server->grab_x;
        view->y = server->cursor->y - server->grab_y;
        wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
        return;
    }
    /* Handle window resize operation: cursor has grabbed an edge to resize */
    else if (server->cursor_mode == CURSOR_RESIZE && server->grabbed_view)
    {
        struct View *view = server->grabbed_view;
        struct wlr_box geo = server->grab_geobox;
        double dx = server->cursor->x - (view->x + geo.width);
        double dy = server->cursor->y - (view->y + geo.height);

        int new_w = geo.width;
        int new_h = geo.height;

        if (server->resize_edges & WLR_EDGE_RIGHT)
            new_w += dx;
        if (server->resize_edges & WLR_EDGE_BOTTOM)
            new_h += dy;
        if (server->resize_edges & WLR_EDGE_LEFT)
        {
            new_w -= dx;
            view->x += dx;
        }
        if (server->resize_edges & WLR_EDGE_TOP)
        {
            new_h -= dy;
            view->y += dy;
        }

        /* Enforce minimum window size */
        if (new_w < 200)
            new_w = 200;
        if (new_h < 200)
            new_h = 200;

        wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, new_w, new_h);
        wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
        return;
    }
}

/**
 * Handle absolute pointer motion events
 * 
 * Processes absolute mouse position updates (from touchpads, tablets, etc.) and:
 * - Warps the cursor to the exact position
 * - Updates pointer focus based on new position
 * - Sends motion events to IPC clients (both window-specific and global)
 * - Handles window move operations
 * - Handles window resize operations
 * 
 * Similar behavior to cursor_motion but using absolute coordinates instead of deltas.
 */
static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);

    /* Determine what surface is under the cursor */
    struct CursorSurfaceInfo surface_info = process_cursor_motion(server, event->time_msec);

    /* Send pointer motion to window-specific IPC clients */
    if (surface_info.surface && surface_info.window_id > 0) {
        struct IPCClient *client, *tmp;
        wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link) {
            if (client->registered_pointer && client->event_window_id == surface_info.window_id) {
                struct icm_msg_pointer_event pevent = {
                    .window_id = surface_info.window_id,
                    .time = event->time_msec,
                    .button = 0,
                    .state = 0,
                    .x = (int32_t)surface_info.sx,  /* Surface-relative coordinates */
                    .y = (int32_t)surface_info.sy
                };
                if (send_event_to_client(client, ICM_MSG_POINTER_EVENT, &pevent, sizeof(pevent)) < 0) {
                    fprintf(stderr, "Failed to send pointer motion event, disconnecting client\n");
                    ipc_client_disconnect(client);
                }
            }
        }
    }

    /* Send global pointer motion to clients listening for all pointers */
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link) {
        if (client->registered_global_pointer) {
            struct icm_msg_pointer_event pevent = {
                .window_id = surface_info.window_id,
                .time = event->time_msec,
                .button = 0,
                .state = 0,
                .x = (int32_t)server->cursor->x,  /* Global coordinates */
                .y = (int32_t)server->cursor->y
            };
            if (send_event_to_client(client, ICM_MSG_POINTER_EVENT, &pevent, sizeof(pevent)) < 0) {
                fprintf(stderr, "Failed to send global pointer motion event, disconnecting client\n");
                ipc_client_disconnect(client);
            }
        }
    }

    /* Handle window move operation: cursor has grabbed a window to drag it */
    if (server->cursor_mode == CURSOR_MOVE && server->grabbed_view)
    {
        struct View *view = server->grabbed_view;
        view->x = server->cursor->x - server->grab_x;
        view->y = server->cursor->y - server->grab_y;
        wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
        return;
    }
    /* Handle window resize operation: cursor has grabbed an edge to resize */
    else if (server->cursor_mode == CURSOR_RESIZE && server->grabbed_view)
    {
        struct View *view = server->grabbed_view;
        struct wlr_box geo = server->grab_geobox;
        double dx = server->cursor->x - (view->x + geo.width);
        double dy = server->cursor->y - (view->y + geo.height);

        int new_w = geo.width;
        int new_h = geo.height;

        if (server->resize_edges & WLR_EDGE_RIGHT)
            new_w += dx;
        if (server->resize_edges & WLR_EDGE_BOTTOM)
            new_h += dy;
        if (server->resize_edges & WLR_EDGE_LEFT)
        {
            new_w -= dx;
            view->x += dx;
        }
        if (server->resize_edges & WLR_EDGE_TOP)
        {
            new_h -= dy;
            view->y += dy;
        }

        /* Enforce minimum window size */
        if (new_w < 200)
            new_w = 200;
        if (new_h < 200)
            new_h = 200;

        wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, new_w, new_h);
        wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
        return;
    }
}

static void cursor_button(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;

    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct View *view = NULL;

    /* On button press: find surface under cursor and focus it */
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        /* Search layers top-to-bottom for the surface under the cursor */
        for (int layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {
            struct wlr_scene_node *node = wlr_scene_node_at(
                &layers[layer]->node, server->cursor->x, server->cursor->y, &sx, &sy);
            if (!node || node->type != WLR_SCENE_NODE_BUFFER)
                continue;
            struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
            struct wlr_scene_surface *scene_surface =
                wlr_scene_surface_try_from_buffer(scene_buffer);
            if (scene_surface) {
                surface = scene_surface->surface;
            }
        }

        if (surface) {
            struct wlr_surface *root_surface = wlr_surface_get_root_surface(surface);
            
            /* Check if this is a regular application window */
            struct View *v;
            wl_list_for_each(v, &server->views, link) {
                struct wlr_surface *vs = v->is_xwayland ?
                    v->xwayland_surface->surface : v->xdg_surface->surface;
                if (vs == root_surface && v->mapped) {
                    view = v;
                    break;
                }
            }

            /* Focus the window if it's a regular application */
            if (view) {
                focus_view(view, surface);
                wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
            } else {
                /* For non-window surfaces (layer shells, etc.), just notify enter */
                wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
            }
        }
    }
    else
    {
        /* On button release: end grab operations */
        if (server->cursor_mode != CURSOR_PASSTHROUGH) {
            server->cursor_mode = CURSOR_PASSTHROUGH;
            server->grabbed_view = NULL;
        }
    }

    /* Notify the Wayland seat of the button state change */
    wlr_seat_pointer_notify_button(server->seat, event->time_msec,
                                   event->button, event->state);

    /* Get current surface info for IPC client notifications */
    struct CursorSurfaceInfo surface_info = process_cursor_motion(server, event->time_msec);
    
    /* Route button events to window-specific IPC clients */
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &server->ipc_server.clients, link)
    {
        if (client->registered_pointer && surface_info.window_id > 0 && 
            client->event_window_id == surface_info.window_id)
        {
            struct icm_msg_pointer_event pevent = {
                .window_id = surface_info.window_id,
                .time = event->time_msec,
                .button = event->button,
                .state = event->state,
                /* Send surface-relative coordinates for window-specific events */
                .x = (int32_t)surface_info.sx,
                .y = (int32_t)surface_info.sy
            };
            if (send_event_to_client(client, ICM_MSG_POINTER_EVENT, &pevent, sizeof(pevent)) < 0)
            {
                fprintf(stderr, "Failed to send pointer event, disconnecting client\n");
                ipc_client_disconnect(client);
            }
            else {
                /* Check click regions for this client's window */
                ipc_check_click_region(&server->ipc_server, surface_info.window_id, (int32_t)surface_info.sx, (int32_t)surface_info.sy, event->button, event->state);
            }
        }
    }
}

/**
 * Handle scroll wheel and touchpad scroll events
 * 
 * Forwards scroll events to the Wayland seat, which distributes them
 * to the focused window for scrolling operations.
 */
static void cursor_axis(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
                                 event->delta, event->delta_discrete, event->source,
                                 event->relative_direction);
}

/**
 * Handle pointer input frame completion
 * 
 * Signals to the Wayland seat that a complete pointer input frame has been
 * processed, allowing proper batching of pointer events.
 */
static void cursor_frame(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

static struct wl_display *g_display = NULL;
static struct Server *g_server = NULL;

static void signal_handler(int sig)
{
    wlr_log(WLR_INFO, "Received signal %d, terminating compositor", sig);
    if (g_display)
    {
        wl_display_terminate(g_display);
    }
}

static void setup_signal_handlers(struct wl_display *display)
{
    g_display = display;
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void seat_request_cursor(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    if (!wlr_backend_is_wl(server->backend))
    {
        wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}

static void seat_request_set_selection(struct wl_listener *listener, void *data)
{
    struct Server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

int main(int argc, char **argv)
{
    wlr_log_init(WLR_DEBUG, NULL);

    // Work around Mesa EGL device query allocation issue
    setenv("MESA_EGL_DISABLE_QUERY_DEVICE_EXT", "1", 1);

    char *startup_cmd = NULL;
    char *backend_type = NULL;
    char *socket_path = NULL;
    int c;
    while ((c = getopt(argc, argv, "s:b:h:S")) != -1)
    {
        switch (c)
        {
        case 's':
            startup_cmd = optarg;
            break;
        case 'S':
            // socket path
            socket_path = optarg;
            break;
        case 'b':
            backend_type = optarg;
            break;
        case 'h':
        default:
            printf("Usage: %s [-s \"command to run\"] [-b backend]\n", argv[0]);
            printf("Backends: auto, wayland, x11\n");
            return 0;
        }
    }

    struct Server server = {0};
    g_server = &server;
    wl_list_init(&server.views);
    wl_list_init(&server.outputs);
    wl_list_init(&server.keyboards);
    wl_list_init(&server.layer_surfaces);

    server.wl_display = wl_display_create();
    server.event_loop = wl_display_get_event_loop(server.wl_display);
    const char *remote_display = getenv("WAYLAND_DISPLAY");
    if (backend_type && strcmp(backend_type, "wayland") == 0 || (!backend_type && remote_display))
    {
        server.backend = wlr_wl_backend_create(server.event_loop, NULL);
    }
    else if (backend_type && strcmp(backend_type, "x11") == 0)
    {
        server.backend = wlr_x11_backend_create(server.event_loop, NULL);
    }
    else
    {
        server.backend = wlr_backend_autocreate(server.event_loop, &server.wlr_session);
    }
    if (!server.backend)
    {
        wlr_log(WLR_ERROR, "Failed to create backend");
        return 1;
    }

    server.renderer = wlr_renderer_autocreate(server.backend);
    if (!server.renderer)
    {
        wlr_log(WLR_ERROR, "Failed to create renderer");
        return 1;
    }

    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (!server.allocator)
    {
        wlr_log(WLR_ERROR, "Failed to create allocator");
        return 1;
    }

    server.compositor = wlr_compositor_create(server.wl_display, 6, server.renderer);
    wlr_data_device_manager_create(server.wl_display);

    server.scene = wlr_scene_create();
    
    /* Initialize matrix transformation system for IPC buffers */
    matrix_transform_init();

    /* Initialize GL shader system for rendering effects */
    if (gl_shader_init(server.renderer) < 0) {
        wlr_log(WLR_ERROR, "Failed to initialize GL shader system");
        /* Continue without shaders - compositor can still function */
    }

    /* Create ordered scene layer trees (like dwl) — children added later render on top */
    for (int i = 0; i < NUM_LAYERS; i++)
        layers[i] = wlr_scene_tree_create(&server.scene->tree);

    server.output_layout = wlr_output_layout_create(server.wl_display);
    server.scene_output_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 6);

    server.layer_shell = wlr_layer_shell_v1_create(server.wl_display, 4);

    /* Initialize XWayland for X11 application support */
    server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, false);
    if (!server.xwayland) {
        wlr_log(WLR_ERROR, "Failed to create XWayland");
        /* Continue without XWayland - compositor can still function with Wayland apps */
    }

    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    // Delay loading cursor theme until after backend start to avoid initialization issues
    // wlr_xcursor_manager_load(server.cursor_mgr, 1.0f);

    server.new_output.notify = server_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    server.new_xdg_surface.notify = server_new_xdg_surface;
    wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

    server.new_layer_surface.notify = server_new_layer_surface;
    wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_surface);

    if (server.xwayland) {
        server.new_xwayland_surface.notify = server_new_xwayland_surface;
        wl_signal_add(&server.xwayland->events.new_surface, &server.new_xwayland_surface);
    }

    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);

    server.seat = wlr_seat_create(server.wl_display, "seat0");
    if (!server.seat)
    {
        wlr_log(WLR_ERROR, "Failed to create seat");
        return 1;
    }

    server.request_cursor.notify = seat_request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);

    server.request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket)
    {
        wlr_log(WLR_ERROR, "Failed to add socket");
        return 1;
    }

    if (!wlr_backend_start(server.backend))
    {
        wlr_log(WLR_ERROR, "Failed to start backend");
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, 1);

    // Load cursor theme after backend is started
    // Moved to output_frame to ensure output is ready

    server.cursor_motion.notify = cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);

    server.cursor_motion_absolute.notify = cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);

    server.cursor_button.notify = cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);

    server.cursor_axis.notify = cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);

    server.cursor_frame.notify = cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    if (startup_cmd)
    {
        if (fork() == 0)
        {
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
            wlr_log(WLR_ERROR, "Failed to exec startup command");
            exit(1);
        }
    }

    /* Initialize IPC server */
    const char *ipc_socket = socket_path ? socket_path : getenv("ICM_SOCKET");
    if (!ipc_socket)
    {
        static char default_socket[256];
        const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (runtime_dir)
        {
            snprintf(default_socket, sizeof(default_socket), "%s/icm.sock", runtime_dir);
        }
        else
        {
            snprintf(default_socket, sizeof(default_socket), "/tmp/icm.sock");
        }
        ipc_socket = default_socket;
    }

    if (ipc_server_init(&server.ipc_server, &server, ipc_socket) != 0)
    {
        wlr_log(WLR_ERROR, "Failed to initialize IPC server");
        return 1;
    }

    /* Now that IPC server is ready, launch startup script if no custom startup command provided */
    if (!startup_cmd)
    {
        /* Auto-launch icm.bash startup script if no custom startup command provided */
        const char *home = getenv("HOME");
        if (home)
        {
            static char icm_bash_path[512];
            snprintf(icm_bash_path, sizeof(icm_bash_path), "%s/.config/icm/icm.bash", home);

            if (access(icm_bash_path, X_OK) == 0)
            {
                wlr_log(WLR_INFO, "Auto-launching startup script: %s", icm_bash_path);
                if (fork() == 0)
                {
                    execl("/bin/sh", "/bin/sh", "-c", icm_bash_path, NULL);
                    wlr_log(WLR_ERROR, "Failed to exec icm.bash startup script");
                    exit(1);
                }
            }
            else
            {
                wlr_log(WLR_INFO, "icm.bash startup script not found or not executable at %s", icm_bash_path);
            }
        }
    }

    wlr_log(WLR_INFO, "Running compositor on WAYLAND_DISPLAY=%s", socket);

    /* Setup signal handlers for graceful shutdown */
    setup_signal_handlers(server.wl_display);

    wl_display_run(server.wl_display);

    /* Notify clients of shutdown */
    ipc_server_broadcast_shutdown(&server.ipc_server);

    /* Cleanup XWayland */
    if (server.xwayland) {
        wl_list_remove(&server.new_xwayland_surface.link);
        wlr_xwayland_destroy(server.xwayland);
    }

    /* Cleanup IPC server */
    ipc_server_destroy(&server.ipc_server);
    
    /* Cleanup matrix transformation system */
    /* Cleanup matrix transformation system */
    matrix_transform_fini();
    
    /* Cleanup GL shader system */
    gl_shader_fini();

    wl_display_destroy_clients(server.wl_display);
    wl_display_destroy(server.wl_display);
    return 0;
}