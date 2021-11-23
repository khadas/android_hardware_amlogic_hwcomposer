/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#ifndef _MESON_VIDEO_TUNNEL_H
#define _MESON_VIDEO_TUNNEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vt_cmd {
    VT_CMD_SET_VIDEO_STATUS,
    VT_CMD_GET_VIDEO_STATUS,
    VT_CMD_SET_GAME_MODE,
    VT_CMD_SET_SOURCE_CROP,
    VT_CMD_SET_SOLID_COLOR_BUF,
    VT_CMD_SET_VIDEO_TYPE,
} vt_cmd_t;

typedef struct vt_rect {
    int left;
    int top;
    int right;
    int bottom;
} vt_rect_t;

typedef struct vt_cmd_data {
    struct vt_rect crop;
    int data;
    int client;
} vt_cmd_data_t;

int meson_vt_open();
int meson_vt_close(int fd);
int meson_vt_alloc_id(int fd, int *tunnel_id);
int meson_vt_free_id(int fd, int tunnel_id);
int meson_vt_connect(int fd, int tunnel_id, int role);
int meson_vt_disconnect(int fd, int tunnel_id, int role);

/* for producer */
int meson_vt_queue_buffer(int fd, int tunnel_id, int buffer_fd,
        int fence_fd, int64_t expected_present_time);
int meson_vt_dequeue_buffer(int fd, int tunnel_id, int *buffer_fd, int *fence_fd);
int meson_vt_cancel_buffer(int fd, int tunnel_id);
int meson_vt_set_sourceCrop(int fd, int tunnel_id, struct vt_rect rect);
int meson_vt_getDisplayVsyncAndPeroid(int fd, int tunnel_id, uint64_t *timestamp, uint32_t *period);

/* for consumer */
int meson_vt_acquire_buffer(int fd, int tunnel_id, int *buffer_fd,
        int *fence_fd, int64_t *expected_present_time);
int meson_vt_release_buffer(int fd, int tunnel_id, int buffer_fd, int fence_fd);
int meson_vt_poll_cmd(int fd, int time_out);
int meson_vt_setDisplayVsyncAndPeroid(int fd, int tunnel_id, uint64_t timestamp, uint32_t period);

/* for video cmd */
int meson_vt_set_mode(int fd, int block_mode);
int meson_vt_send_cmd(int fd, int tunnel_id, enum vt_cmd cmd, int cmd_data);
int meson_vt_recv_cmd(int fd, int tunnel_id, enum vt_cmd *cmd, struct vt_cmd_data *cmd_data);

//int meson_vt_reply_cmd(int fd, enum vt_cmd cmd, int cmd_data, int client_id);

#ifdef __cplusplus
}
#endif

#endif /* _MESON_VIDEO_TUNNEL_H */
