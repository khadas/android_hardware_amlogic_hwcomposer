/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VIDEO_COMPOSER_DEV_H
#define VIDEO_COMPOSER_DEV_H

#include <BasicTypes.h>
#include <stdlib.h>
#include <DrmFramebuffer.h>

typedef uint32_t u32;

#define VIDEO_COMPOSER_IOC_MAGIC  'V'
#define VIDEO_COMPOSER_IOCTL_SET_FRAMES   _IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x00, video_frames_info_t)
#define VIDEO_COMPOSER_IOCTL_SET_ENABLE   _IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x01, int)
#define VIDEO_COMPOSER_IOCTL_SET_DISABLE  _IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x02, int)
#define MAX_LAYER_COUNT 9

typedef struct video_frame_info {
    u32 fd;
    u32 composer_fen_fd;
    u32 disp_fen_fd;
    u32 buffer_w;
    u32 buffer_h;
    u32 dst_x;
    u32 dst_y;
    u32 dst_w;
    u32 dst_h;
    u32 crop_x;
    u32 crop_y;
    u32 crop_w;
    u32 crop_h;
    u32 zorder;
    u32 transform;
    u32 type;
    u32 sideband_type;
    u32 reserved[3];
} video_frame_info_t;

typedef struct video_frames_info {
    u32 frame_count;
    video_frame_info_t frame_info[MAX_LAYER_COUNT];
    u32 layer_index; /*useless member, always set to 0 now.*/
    u32 disp_zorder;
    u32 reserved[4];
} video_frames_info_t;

class VideoComposerDev {
public:
    VideoComposerDev(int drvFd);
    ~VideoComposerDev();

    int32_t enable(bool bEnable);
    int32_t setFrame(std::shared_ptr<DrmFramebuffer> & fb, int & releaseFence, uint32_t z);
    int32_t setFrames(std::vector<std::shared_ptr<DrmFramebuffer>> & composefbs, int & releaseFence, uint32_t z);

protected:
    int mDrvFd;
    video_frames_info_t mVideoFramesInfo;
    bool mEnable;
};

int createVideoComposerDev(int fd, int idx);
std::shared_ptr<VideoComposerDev> getVideoComposerDev(int idx);

#endif
