#include "icm_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

/* Socket I/O helpers */
static ssize_t send_msg(int socket_fd, const void *data, size_t size,
                        const int *fds, int num_fds) {
    if (num_fds == 0) {
        return send(socket_fd, data, size, MSG_NOSIGNAL);
    }

    struct cmsghdr *cmsg;
    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = (void *)data,
        .iov_len = size,
    };

    char cmsgbuf[CMSG_SPACE(num_fds * sizeof(int))];
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(num_fds * sizeof(int));
    memcpy(CMSG_DATA(cmsg), fds, num_fds * sizeof(int));

    msg.msg_controllen = cmsg->cmsg_len;

    return sendmsg(socket_fd, &msg, MSG_NOSIGNAL);
}

/* Connection management */
int icm_client_connect(struct ICMClient *client, const char *socket_path) {
    client->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(client->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(client->socket_fd);
        return -1;
    }

    client->next_sequence = 1;
    return 0;
}

void icm_client_close(struct ICMClient *client) {
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
}

/* Helper to send message */
static int send_ipc_message(struct ICMClient *client, enum icm_ipc_msg_type type,
                            const void *payload, size_t payload_size,
                            const int *fds, int num_fds) {
    struct icm_ipc_header header = {
        .length = sizeof(header) + payload_size,
        .type = type,
        .flags = 0,
        .sequence = client->next_sequence++,
        .num_fds = num_fds,
    };

    /* Send header and payload together */
    uint8_t buffer[sizeof(header) + payload_size];
    memcpy(buffer, &header, sizeof(header));
    if (payload_size > 0) {
        memcpy(buffer + sizeof(header), payload, payload_size);
    }

    ssize_t ret = send_msg(client->socket_fd, buffer, sizeof(buffer), fds, num_fds);
    if (ret < 0) {
        perror("send_msg");
        return -1;
    }

    return 0;
}

/* Buffer operations */
int icm_create_buffer(struct ICMClient *client, uint32_t buffer_id,
                     int32_t width, int32_t height, uint32_t format) {
    struct icm_msg_create_buffer msg = {
        .buffer_id = buffer_id,
        .width = width,
        .height = height,
        .format = format,
        .usage_flags = 0,
    };

    return send_ipc_message(client, ICM_MSG_CREATE_BUFFER, &msg, sizeof(msg), NULL, 0);
}

int icm_destroy_buffer(struct ICMClient *client, uint32_t buffer_id) {
    struct icm_msg_destroy_buffer msg = {
        .buffer_id = buffer_id,
    };

    return send_ipc_message(client, ICM_MSG_DESTROY_BUFFER, &msg, sizeof(msg), NULL, 0);
}

/* Drawing operations */
int icm_draw_rect(struct ICMClient *client, uint32_t window_id,
                 int32_t x, int32_t y, uint32_t width, uint32_t height,
                 uint32_t color_rgba) {
    struct icm_msg_draw_rect msg = {
        .window_id = window_id,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .color_rgba = color_rgba,
    };

    return send_ipc_message(client, ICM_MSG_DRAW_RECT, &msg, sizeof(msg), NULL, 0);
}

int icm_draw_line(struct ICMClient *client, uint32_t window_id,
                 int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 uint32_t color_rgba, uint32_t thickness) {
    struct icm_msg_draw_line msg = {
        .window_id = window_id,
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
        .color_rgba = color_rgba,
        .thickness = thickness,
    };

    return send_ipc_message(client, ICM_MSG_DRAW_LINE, &msg, sizeof(msg), NULL, 0);
}

int icm_draw_circle(struct ICMClient *client, uint32_t window_id,
                   int32_t cx, int32_t cy, uint32_t radius,
                   uint32_t color_rgba, uint32_t fill) {
    struct icm_msg_draw_circle msg = {
        .window_id = window_id,
        .cx = cx,
        .cy = cy,
        .radius = radius,
        .color_rgba = color_rgba,
        .fill = fill,
    };

    return send_ipc_message(client, ICM_MSG_DRAW_CIRCLE, &msg, sizeof(msg), NULL, 0);
}

/* DMABUF operations */
int icm_import_dmabuf(struct ICMClient *client, uint32_t buffer_id,
                     int32_t width, int32_t height, uint32_t format,
                     const struct icm_msg_import_dmabuf *dmabuf,
                     const int *fds) {
    struct icm_msg_import_dmabuf msg = *dmabuf;
    msg.buffer_id = buffer_id;
    msg.width = width;
    msg.height = height;
    msg.format = format;

    return send_ipc_message(client, ICM_MSG_IMPORT_DMABUF, &msg, sizeof(msg),
                           fds, msg.num_planes);
}

/* Batch operations */
int icm_batch_begin(struct ICMClient *client, uint32_t batch_id) {
    struct icm_msg_batch_begin msg = {
        .batch_id = batch_id,
        .expected_commands = 0,
    };

    return send_ipc_message(client, ICM_MSG_BATCH_BEGIN, &msg, sizeof(msg), NULL, 0);
}

int icm_batch_end(struct ICMClient *client, uint32_t batch_id) {
    struct icm_msg_batch_end msg = {
        .batch_id = batch_id,
    };

    return send_ipc_message(client, ICM_MSG_BATCH_END, &msg, sizeof(msg), NULL, 0);
}
