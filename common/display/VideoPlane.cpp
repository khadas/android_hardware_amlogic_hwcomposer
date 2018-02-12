/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VideoPlane.h"
#include <sys/ioctl.h>
#include <Amvideoutils.h>
#include <MesonLog.h>

//#define AMVIDEO_DEBUG
#define AMSTREAM_IOC_MAGIC  'S'
#define AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT  _IOR(AMSTREAM_IOC_MAGIC, 0x21, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT  _IOW(AMSTREAM_IOC_MAGIC, 0x22, int)

VideoPlane::VideoPlane(int32_t drvFd, uint32_t id) :
    HwDisplayPlane(drvFd, id) {
    mPlaneType = VIDEO_PLANE;

    if (getMute(mPlaneMute) != 0) {
        MESON_LOGE("get video mute failed.");
        mPlaneMute = false;
    }
}

VideoPlane::~VideoPlane() {

}

bool VideoPlane::shouldUpdate(std::shared_ptr<DrmFramebuffer> &fb) {
    // TODO: we need to update video axis while mode or freescale state is changed.
    // if (mPresentOverlay) return true;

    drm_rect_t *displayFrame = &(fb->mDisplayFrame);

    if (memcmp(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t))) {
        memcpy(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t));
        return true;
    }

    if (mBackupTransform != fb->mTransform) {
        mBackupTransform = fb->mTransform;
        return true;
    }

    // TODO: we need to update video axis while acitve config is changed.

    // if (mLastLayerScaleX != (int)(mLayerScaleX * FLOAT_SCALE_UP_FACTOR)) {
    //     mLastLayerScaleX = (int)(mLayerScaleX * FLOAT_SCALE_UP_FACTOR);
    //     return true;
    // }

    // if (mLastLayerScaleY != (int)(mLayerScaleY * FLOAT_SCALE_UP_FACTOR)) {
    //     mLastLayerScaleY = (int)(mLayerScaleY * FLOAT_SCALE_UP_FACTOR);
    //     return true;
    // }

    return false;
}

int VideoPlane::setPlane(std::shared_ptr<DrmFramebuffer> &fb) {
    buffer_handle_t buf = fb->mBufferHandle;
    MESON_LOGD("videoPlane [%p]", (void*)buf);

    // TODO: DONOT set mute for now, because we need to implement secure display.
    // setMute(PrivHandle::isSecure(buf););

    if (shouldUpdate(fb)) {
        int32_t angle = 0;
        drm_rect_t *displayFrame = &(fb->mDisplayFrame);

        switch (fb->mTransform) {
            case 0:
                angle = 0;
            break;
            case HAL_TRANSFORM_ROT_90:
                angle = 90;
            break;
            case HAL_TRANSFORM_ROT_180:
                angle = 180;
            break;
            case HAL_TRANSFORM_ROT_270:
                angle = 270;
            break;
            default:
            return 0;
        }

        MESON_LOGV("displayFrame: [%d, %d, %d, %d]",
                displayFrame->left,
                displayFrame->top,
                displayFrame->right,
                displayFrame->bottom);

        amvideo_utils_set_virtual_position(
                displayFrame->left,
                displayFrame->top,
                displayFrame->right - displayFrame->left,
                displayFrame->bottom - displayFrame->top,
                angle);

        // dumpPlaneInfo();
    }

    return 0;
}

int32_t VideoPlane::getMute(bool& output) {
    if (mDrvFd < 0) return -EBADF;

    uint32_t val = 1;
    if (ioctl(mDrvFd, AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT, &val) != 0) {
        MESON_LOGE("AMSTREAM_GET_VIDEO_OUTPUT ioctl fail(%d)", errno);
        return -EINVAL;
    }
    output = (val == 0) ? true : false;

    return 0;
}

int32_t VideoPlane::setMute(bool status) {
    if (mDrvFd < 0) return -EBADF;

    if (mPlaneMute != status) {
        ALOGD("muteVideo to %d", status);
        uint32_t val = status ? 0 : 1;
        if (ioctl(mDrvFd, AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT, val) != 0) {
            MESON_LOGE("AMSTREAM_SET_VIDEO_OUTPUT ioctl (%d) return(%d)", status, errno);
            return -EINVAL;
        }
        mPlaneMute = status;
    } else {
#ifdef AMVIDEO_DEBUG
        bool val = true;
        getMute(val);
        if (mPlaneMute != val)
            MESON_LOGE("status video (%d) vs (%d)", mPlaneMute, val);
#endif
        MESON_LOGV("Already set video to (%d)", status);
    }

    return 0;
}

int32_t VideoPlane::blank() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void VideoPlane::dump(String8 & dumpstr) {
    MESON_LOG_EMPTY_FUN();
}

