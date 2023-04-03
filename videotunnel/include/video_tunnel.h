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

/*for VT_CMD_SET_SHOW_SOLID_COLOR */
typedef enum {
    SOLID_COLOR_FOR_BLACK = 0,
    SOLID_COLOR_FOR_BLUE,
    SOLID_COLOR_FOR_GREEN,
    SOLID_COLOR_FOR_INVALID,
} vt_video_color_t;

typedef enum vt_video_status {
    /* clear lastFrame and blank plane,
     * will re-display when receive new frame
     */
    VT_VIDEO_STATUS_BLANK = 1,
    /* clear lastframe and donot show this video layer
     * until receive show video CMD
     */
    VT_VIDEO_STATUS_HIDE = 2,
    VT_VIDEO_STATUS_SHOW = 3,

    /*
     * only show one frame of solid color,
     * will recovery when receive new frame
     */
    VT_VIDEO_STATUS_COLOR_ONCE = 4,
    /*
     * Always show the solid color frame
     * until receive disable cmd or surface disconnect
     */
    VT_VIDEO_STATUS_COLOR_ALWAYS = 5,
    /*
     * disable color frame
     */
    VT_VIDEO_STATUS_COLOR_DISABLE = 6,

    /* Always show the last frame
     * until receive VT_VIDEO_STATUS_SHOW command
     */
    VT_VIDEO_STATUS_HOLD_FRAME = 7,
} vt_video_status_t;

typedef enum vt_cmd {
    VT_CMD_SET_VIDEO_STATUS,
    VT_CMD_GET_VIDEO_STATUS,
    VT_CMD_SET_GAME_MODE,
    VT_CMD_SET_SOURCE_CROP,
    VT_CMD_SET_SHOW_SOLID_COLOR,
    VT_CMD_SET_COLOR_BLACK,
    VT_CMD_SET_COLOR_BLUE,
    VT_CMD_SET_COLOR_GREEN,
    VT_CMD_SET_DISPLAY_FRAME,
} vt_cmd_t;

typedef struct vt_rect {
    int left;
    int top;
    int right;
    int bottom;
} vt_rect_t;

typedef struct vt_cmd_data {
    struct vt_rect rect;
    vt_video_status_t data;
    int client;
} vt_cmd_data_t;

typedef enum vt_color_cmd {
    VT_CMD_COLOR_BLACK,
    VT_CMD_COLOR_BLUE,
    VT_CMD_COLOR_GREEN,
} vt_color_cmd_t;

typedef enum vt_color_data {
    /*
     * only show one frame of solid color,
     * will recovery when receive new frame
     */
    VT_CMD_COLOR_DATA_ONCE = 4,
    /*
     * Always show the solid color frame
     * until receive disable cmd or surface disconnect
     */
    VT_CMD_COLOR_DATA_ALWAYS = 5,
    /*
     * disable color frame
     */
    VT_CMD_COLOR_DATA_DISABLE = 6,
} vt_color_data_t;

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
int meson_vt_set_displayFrame(int fd, int tunnel_id, struct vt_rect rect);
int meson_vt_getDisplayVsyncAndPeriod(int fd, int tunnel_id, uint64_t *timestamp, uint32_t *period);

/* for consumer */
int meson_vt_acquire_buffer(int fd, int tunnel_id, int *buffer_fd,
        int *fence_fd, int64_t *expected_present_time);
int meson_vt_release_buffer(int fd, int tunnel_id, int buffer_fd, int fence_fd);
int meson_vt_poll_cmd(int fd, int time_out);
int meson_vt_setDisplayVsyncAndPeriod(int fd, int tunnel_id, uint64_t timestamp, uint32_t period);

/* for video cmd */
int meson_vt_set_mode(int fd, int block_mode);
int meson_vt_send_cmd(int fd, int tunnel_id, enum vt_cmd cmd, int cmd_data);
int meson_vt_recv_cmd(int fd, int tunnel_id, enum vt_cmd *cmd, struct vt_cmd_data *cmd_data);

/* for blue/black color frame cmd */
int meson_vt_set_solid_color(int fd, int tunnel_id, enum vt_color_cmd cmd, enum vt_color_data cmd_data);

//int meson_vt_reply_cmd(int fd, enum vt_cmd cmd, int cmd_data, int client_id);

#ifdef __cplusplus
}
#endif

#endif /* _MESON_VIDEO_TUNNEL_H */
