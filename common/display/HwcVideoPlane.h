/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef HWC_VIDEO_PLANE_H
#define HWC_VIDEO_PLANE_H

#include <sys/types.h>
#include <HwDisplayPlane.h>

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
    u32 reserved[4];
} video_frame_info_t;

typedef struct video_frames_info {
    u32 frame_count;
    video_frame_info_t frame_info[MAX_LAYER_COUNT];
    u32 layer_index;
    u32 disp_zorder;
    u32 reserved[4];
} video_frames_info_t;

struct DiComposerPair {
    uint32_t zorder;
    uint32_t num_composefbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> composefbs;
    std::shared_ptr<HwDisplayPlane> plane;
};


class HwcVideoPlane : public HwDisplayPlane {
public:
    HwcVideoPlane(int32_t drvFd, uint32_t id);
    ~HwcVideoPlane();

    const char * getName();
    uint32_t getPlaneType();
    uint32_t getCapabilities();
    int32_t getFixedZorder();
    uint32_t getPossibleCrtcs();
    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb, uint32_t zorder, int blankOp);
    int32_t setComposePlane(DiComposerPair *difbs, int blankOp);

    void dump(String8 & dumpstr);

protected:
    char mName[64];
    u32 mFramesCount;
    video_frames_info_t mVideoFramesInfo;
    bool mStatus;

    std::vector<std::shared_ptr<DrmFramebuffer>> mLastComposeFbs;
    std::shared_ptr<DrmFence> mLastFence;
};

 #endif/*HWC_VIDEO_PLANE_H*/
