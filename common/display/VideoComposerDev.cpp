/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <VideoComposerDev.h>
#include <MesonLog.h>
#include <sys/ioctl.h>
#include <misc.h>

static std::map<int,std::map<int,std::shared_ptr<VideoComposerDev>> > gComposerDevByDisplayId;


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

    MESON_LOGD("VideoComposerDev (%d) set (%d).\n", mDrvFd, bEnable);
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
    ATRACE_CALL();
    std::shared_ptr<DrmFramebuffer> fb;
    video_frame_info_t * vFrameInfo;
    releaseFence = -1;
    memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));

    mVideoFramesInfo.frame_count = 0;
    mVideoFramesInfo.layer_index = mDrvFd;
    mVideoFramesInfo.disp_zorder = z;

    MESON_LOGV("VideoComposerDev(%d) setframes (%zu-%d)",
            mDrvFd, composefbs.size(), mVideoFramesInfo.disp_zorder);

    bool isBlackBuffer = false;
    for (int i = 0; i < composefbs.size(); i++) {
        fb = composefbs[i];
        vFrameInfo = &mVideoFramesInfo.frame_info[mVideoFramesInfo.frame_count];
        buffer_handle_t buf = fb->mBufferHandle;

        vFrameInfo->sideband_type = 0;
        drm_fb_type_t fbType = fb->getFbType();
        if (fbType == DRM_FB_VIDEO_DMABUF ||
            fbType == DRM_FB_VIDEO_UVM_DMA) {
            vFrameInfo->fd = am_gralloc_get_buffer_fd(buf);
            vFrameInfo->type = 1;
        } else if (fbType == DRM_FB_VIDEO_SIDEBAND ||
            fbType == DRM_FB_VIDEO_SIDEBAND_SECOND ||
            fbType == DRM_FB_VIDEO_SIDEBAND_TV) {
            vFrameInfo->type = 2;
            int sideband_type;
            am_gralloc_get_sideband_type(buf, &sideband_type);
            vFrameInfo->sideband_type = sideband_type;
        } else if (fbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
            int fd = fb->getVtBuffer();
            if (fd < 0) {
                vFrameInfo->fd = fb->getSolidColorBuffer(true);
                vFrameInfo->type = 1;
                isBlackBuffer = true;
                vFrameInfo->source_type = HWC_CREAT_ION;
            } else {
                vFrameInfo->fd = fd;
                vFrameInfo->type = 0;
                vFrameInfo->source_type = DTV_FIX_TUNNEL;
            }
        } else {
            MESON_LOGE("unknow fb (%d) type %d !!", fb->mZorder, fbType);
            return -EINVAL;
        }

        if ((vFrameInfo->type != 2) && (vFrameInfo->fd < 0)) {
            MESON_LOGW("vframe get invalid buffer fd");
            continue;
        }
        mVideoFramesInfo.frame_count++;

        bool isSidebandBuffer = fb->isSidebandBuffer();
        drm_rect_t sourceCrop = fb->getSourceCrop();
        vFrameInfo->dst_x = fb->mDisplayFrame.left;
        vFrameInfo->dst_y = fb->mDisplayFrame.top;
        vFrameInfo->dst_w = fb->mDisplayFrame.right - fb->mDisplayFrame.left;
        vFrameInfo->dst_h = fb->mDisplayFrame.bottom - fb->mDisplayFrame.top;
        vFrameInfo->crop_x = sourceCrop.left;
        vFrameInfo->crop_y = sourceCrop.top;
        vFrameInfo->crop_w = sourceCrop.right - sourceCrop.left;
        vFrameInfo->crop_h = sourceCrop.bottom - sourceCrop.top;
        vFrameInfo->zorder = fb->mZorder;
        vFrameInfo->transform = fb->mTransform;
        /*pass aligned buffer width and height to video composer*/
        vFrameInfo->reserved[0] = isSidebandBuffer ? 0 : am_gralloc_get_stride_in_pixel(buf);
        vFrameInfo->reserved[1] = isSidebandBuffer ? 0 : am_gralloc_get_aligned_height(buf);
        if (isSidebandBuffer) {
            vFrameInfo->buffer_w = isBlackBuffer ? VIDEO_BUFFER_W : 0;
            vFrameInfo->buffer_h = isBlackBuffer ? VIDEO_BUFFER_H : 0;
        } else {
            vFrameInfo->buffer_w = am_gralloc_get_width(buf);
            vFrameInfo->buffer_h = am_gralloc_get_height(buf);
        }

        MESON_LOGV("VideoComposerDev(%d) layerId(%" PRIu64 ") setframe zorder(%d) "
                "Fbtype(%s) bufferFd(%d) (%dx%d) aligned wxh (%dx%d))",
                mDrvFd, fb->mId, fb->mZorder, drmFbTypeToString(fbType), vFrameInfo->fd,
                vFrameInfo->buffer_w, vFrameInfo->buffer_h,
                vFrameInfo->reserved[0], vFrameInfo->reserved[1]);
    }

    if (mVideoFramesInfo.frame_count == 0) {
        MESON_LOGV("VideoComposerDEV(%d) has no frame count", mDrvFd);
        return -EINVAL;
    }

    if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_FRAMES, &mVideoFramesInfo) != 0) {
        MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
            strerror(errno), errno, mDrvFd);
        return -1;
    }

    if (mVideoFramesInfo.frame_info[0].composer_fen_fd >= 0)
        releaseFence = mVideoFramesInfo.frame_info[0].composer_fen_fd;

    MESON_LOGV("VideoComposerDev(%d) setframe ReleaseFence(%d)", mDrvFd, releaseFence);

    return 0;
}

int createVideoComposerDev(int fd, int idx) {
    int retValue;
    int capacity;
    if (ioctl(fd , VIDEO_COMPOSER_IOCTL_GET_PANEL_CAPABILITY, &retValue) != 0) {
        MESON_LOGE("video plane get capibility ioctl (%d) return(%d)", retValue, errno);
    }
    capacity =(retValue >> (2 * idx + 18) & 0x3);
    std::shared_ptr<VideoComposerDev> dev = std::make_shared<VideoComposerDev>(fd);
    std::map<int, std::shared_ptr<VideoComposerDev>> gComposerDevTemp;

    if (gComposerDevByDisplayId.count(capacity) == 1 ) {
        gComposerDevTemp = gComposerDevByDisplayId[capacity];
    }
    gComposerDevTemp.emplace(idx, std::move(dev));
    gComposerDevByDisplayId[capacity] = gComposerDevTemp;
    return 0;
}

std::shared_ptr<VideoComposerDev> getVideoComposerDev(int idx) {
    MESON_ASSERT(gComposerDevByDisplayId.size() > 0, "videocomposer no instance.");
    for (auto it = gComposerDevByDisplayId.begin(); it != gComposerDevByDisplayId.end(); it++) {
        std::map<int, std::shared_ptr<VideoComposerDev>> gComposerDevTemp = it->second;
        if (gComposerDevTemp.count(idx)  == 1 ) {
            return gComposerDevTemp[idx];
        }
    }
    MESON_LOGD("getVideoComposerDev not found ");
    return nullptr;
}

std::map<int,std::shared_ptr<VideoComposerDev>>  getVideoComposerDevByDisplayId(int displayId){
     MESON_ASSERT(gComposerDevByDisplayId.size() > 0, "videocomposer no instance.");
     return gComposerDevByDisplayId[displayId];
}
