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

#ifdef __cplusplus
extern "C" {
#endif

/* open /dev/videotunnel device and return the opened fd */
int meson_vt_open();

/* close the videotunnel device */
int meson_vt_close(int fd);

/* Allocate an unique videotunnel id. */
int meson_vt_alloc_id(int fd, int *tunnel_id);

/* Free the videotunnel id previous allocated by meson_vt_alloc_id */
int meson_vt_free_id(int fd, int tunnel_id);

/* Connect a consumer or a producer to the sepecific videotunnel. */
int meson_vt_connect(int fd, int tunnel_id, int role);

/* Disconnect a consumer or a producer from the sepecific videotunnel. */
int meson_vt_disconnect(int fd, int tunnel_id, int role);

/* for producer */
/*
 * QueueBuffer indicates the producer has finished filling in the contents
 * of the buffer and transfer ownership of the buffer to consumer.
 */
int meson_vt_queue_buffer(int fd, int tunnel_id, int buffer_fd, int fence_fd);

/*
 * DequeueBuffer requests a buffer from videotunnel to use.
 * Ownership of the buffer is transfered to the producer.
 */
int meson_vt_dequeue_buffer(int fd, int tunnel_id, int *buffer_fd, int *fence_fd);

/* for consumer */
/* Acquire buffer attemps to acquire ownership of the next pending buffer in the videotunnel. */
int meson_vt_acquire_buffer(int fd, int tunnel_id, int *buffer_fd, int *fence_fd);

/*Release buffer releases a buffer from the cosumer back to the videotunnel. */
int meson_vt_release_buffer(int fd, int tunnel_id, int buffer_fd, int fence_fd);

#ifdef __cplusplus
}
#endif

#endif /* _MESON_VIDEO_TUNNEL_H */
