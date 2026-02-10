#ifndef ICM_CLIENT_H
#define ICM_CLIENT_H

#include <stdint.h>
#include "ipc_protocol.h"

struct ICMClient {
    int socket_fd;
    uint32_t next_sequence;
};

/* Connection management */
int icm_client_connect(struct ICMClient *client, const char *socket_path);
void icm_client_close(struct ICMClient *client);

/* Buffer operations */
int icm_create_buffer(struct ICMClient *client, uint32_t buffer_id,
                     int32_t width, int32_t height, uint32_t format);
int icm_destroy_buffer(struct ICMClient *client, uint32_t buffer_id);

/* Drawing operations */
int icm_draw_rect(struct ICMClient *client, uint32_t window_id,
                 int32_t x, int32_t y, uint32_t width, uint32_t height,
                 uint32_t color_rgba);
int icm_draw_line(struct ICMClient *client, uint32_t window_id,
                 int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 uint32_t color_rgba, uint32_t thickness);
int icm_draw_circle(struct ICMClient *client, uint32_t window_id,
                   int32_t cx, int32_t cy, uint32_t radius,
                   uint32_t color_rgba, uint32_t fill);

/* DMABUF operations */
int icm_import_dmabuf(struct ICMClient *client, uint32_t buffer_id,
                     int32_t width, int32_t height, uint32_t format,
                     const struct icm_msg_import_dmabuf *dmabuf,
                     const int *fds);

/* Batch operations */
int icm_batch_begin(struct ICMClient *client, uint32_t batch_id);
int icm_batch_end(struct ICMClient *client, uint32_t batch_id);

#endif
