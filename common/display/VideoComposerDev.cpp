/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include <VideoComposerDev.h>
#include <MesonLog.h>
#include <sys/ioctl.h>

static std::map<int, std::shared_ptr<VideoComposerDev>> gComposerDev;

VideoComposerDev::VideoComposerDev(int drvFd) {
    mDrvFd = drvFd;
    mEnable = false;
}

VideoComposerDev::~VideoComposerDev() {
    close(mDrvFd);
}

int32_t VideoComposerDev::enable(bool bEnable) {
    if (mEnable == bEnable)
        return 0;

    MESON_LOGV("VideoComposerDev (%d) set (%d).\n", mDrvFd, bEnable);
    int val = bEnable ? 1 : 0;
    if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_ENABLE, &val) != 0) {
        MESON_LOGE("VideoComposerDev: ioctl error, %s(%d), mDrvFd = %d",
            strerror(errno), errno, mDrvFd);
        return -errno;
    }

    mEnable = bEnable;
    return 0;
}

int32_t VideoComposerDev::setFrame(
    std::shared_ptr<DrmFramebuffer> & fb,
    int & releaseFence,
    uint32_t z) {
    std::vector<std::shared_ptr<DrmFramebuffer>> composefbs;
    composefbs.push_back(fb);
    return setFrames(composefbs, releaseFence, z);
}

int32_t VideoComposerDev::setFrames(
    std::vector<std::shared_ptr<DrmFramebuffer>> & composefbs,
    int & releaseFence,
    uint32_t z) {
    std::shared_ptr<DrmFramebuffer> fb;
    video_frame_info_t * vFrameInfo;
    releaseFence = -1;
    memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));

    mVideoFramesInfo.frame_count = composefbs.size();
    mVideoFramesInfo.layer_index = mDrvFd;
    mVideoFramesInfo.disp_zorder = z;

    MESON_LOGV("VideoComposerDev(%d) setframes (%d-%d)",
            mDrvFd, mVideoFramesInfo.frame_count, mVideoFramesInfo.disp_zorder);

    for (int i = 0; i < composefbs.size(); i++) {
        fb = composefbs[i];
        vFrameInfo = &mVideoFramesInfo.frame_info[i];
        buffer_handle_t buf = fb->mBufferHandle;

        vFrameInfo->sideband_type = 0;
        if (fb->mFbType == DRM_FB_VIDEO_OMX_V4L ||
            fb->mFbType == DRM_FB_VIDEO_UVM_DMA) {
            vFrameInfo->fd = am_gralloc_get_omx_v4l_file(buf);
            vFrameInfo->type = 0;
        } else if (fb->mFbType == DRM_FB_VIDEO_DMABUF) {
            vFrameInfo->fd = am_gralloc_get_buffer_fd(buf);
            vFrameInfo->type = 1;
        } else if (fb->mFbType == DRM_FB_VIDEO_SIDEBAND ||
            fb->mFbType == DRM_FB_VIDEO_SIDEBAND_SECOND ||
            fb->mFbType == DRM_FB_VIDEO_SIDEBAND_TV) {
            vFrameInfo->type = 2;
            int sideband_type;
            am_gralloc_get_sideband_type(buf, &sideband_type);
            vFrameInfo->sideband_type = sideband_type;
        } else if (fb->mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
            vFrameInfo->type = 0;
            vFrameInfo->fd = fb->getVtBuffer();
        } else {
            MESON_LOGE("unknow fb (%d) type %d !!", fb->mZorder, fb->mFbType);
            break;
        }

        vFrameInfo->dst_x = fb->mDisplayFrame.left;
        vFrameInfo->dst_y = fb->mDisplayFrame.top;
        vFrameInfo->dst_w = fb->mDisplayFrame.right - fb->mDisplayFrame.left;
        vFrameInfo->dst_h = fb->mDisplayFrame.bottom - fb->mDisplayFrame.top;
        vFrameInfo->crop_x = fb->mSourceCrop.left;
        vFrameInfo->crop_y = fb->mSourceCrop.top;
        vFrameInfo->crop_w = fb->mSourceCrop.right - fb->mSourceCrop.left;
        vFrameInfo->crop_h = fb->mSourceCrop.bottom - fb->mSourceCrop.top;
        vFrameInfo->buffer_w = am_gralloc_get_width(buf);
        vFrameInfo->buffer_h = am_gralloc_get_height(buf);
        vFrameInfo->zorder = fb->mZorder;
        vFrameInfo->transform = fb->mTransform;

        MESON_LOGV("VideoComposerDev(%d) setframe (%d) (%d-%dx%d)",
                mDrvFd, fb->mZorder, fb->mFbType,
                vFrameInfo->buffer_w, vFrameInfo->buffer_h);
    }

    if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_FRAMES, &mVideoFramesInfo) != 0) {
        MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
            strerror(errno), errno, mDrvFd);
        return -1;
    }

    if (mVideoFramesInfo.frame_info[0].composer_fen_fd >= 0)
        releaseFence = vFrameInfo->composer_fen_fd;

    return 0;
}

int createVideoComposerDev(int fd, int idx) {
    std::shared_ptr<VideoComposerDev> dev = std::make_shared<VideoComposerDev>(fd);
    gComposerDev.emplace(idx, std::move(dev));
    return 0;
}

std::shared_ptr<VideoComposerDev> getVideoComposerDev(int idx) {
    MESON_ASSERT(gComposerDev.size() > 0, "videocomposer no instance.");
    return gComposerDev[idx];
}
