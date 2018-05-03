/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "LegacyVideoPlane.h"
#include "AmFramebuffer.h"
#include <sys/ioctl.h>
#include <Amvideoutils.h>
#include <tvp/OmxUtil.h>
#include <MesonLog.h>
#include <gralloc_priv.h>

//#define AMVIDEO_DEBUG

LegacyVideoPlane::LegacyVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id) {
    snprintf(mName, 64, "AmVideo-%d", id);

    if (getMute(mPlaneMute) != 0) {
        MESON_LOGE("get video mute failed.");
        mPlaneMute = false;
    }

    mOmxKeepLastFrame = 0;
    getOmxKeepLastFrame(mOmxKeepLastFrame);
}

LegacyVideoPlane::~LegacyVideoPlane() {

}

uint32_t LegacyVideoPlane::getPlaneType() {
    return LEGACY_VIDEO_PLANE;
}

const char * LegacyVideoPlane::getName() {
    return mName;
}

int32_t LegacyVideoPlane::getCapabilities() {
    int32_t ret = 0;
    return ret;
}

bool LegacyVideoPlane::shouldUpdateAxis(
    std::shared_ptr<DrmFramebuffer> &fb) {
    bool bUpdate = false;

    // TODO: we need to update video axis while mode or freescale state is changed.
    drm_rect_t *displayFrame = &(fb->mDisplayFrame);

    if (memcmp(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t))) {
        memcpy(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t));
        bUpdate = true;
    }

    if (mBackupTransform != fb->mTransform) {
        mBackupTransform = fb->mTransform;
        bUpdate = true;
    }

    return bUpdate;
}

int32_t LegacyVideoPlane::setPlane(std::shared_ptr<DrmFramebuffer> & fb) {
    buffer_handle_t buf = fb->mBufferHandle;

    // TODO: DONOT set mute for now, because we need to implement secure display.
    if (shouldUpdateAxis(fb)) {
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

    }

    if (am_gralloc_is_omx_metadata_buffer(buf)) {
        private_handle_t const* buffer = private_handle_t::dynamicCast(buf);

        char *base = (char*)mmap(
            NULL, buffer->size, PROT_READ|PROT_WRITE,
            MAP_SHARED, buffer->share_fd, 0);

        if (base != MAP_FAILED) {
            set_omx_pts(base, &mDrvFd);
            munmap(base, buffer->size);
            MESON_LOGV("set omx pts ok.");
        } else {
            MESON_LOGE("set omx pts failed.");
        }
    }

    return 0;
}

int32_t LegacyVideoPlane::blank(int blankOp) {
    MESON_LOGD("LegacyVideoPlane  blank (%d)", blankOp);

    if (blankOp == BLANK_FOR_SECURE_CONTENT
        || blankOp == BLANK_FOR_NO_CONENT) {
        setMute(true);
        return 0;
    }

    if (blankOp == UNBLANK) {
        setMute(false);
    }

    if (!mOmxKeepLastFrame)
        return 0;

    int blankStatus = 0;
    getVideodisableStatus(blankStatus);

    if (blankOp == BLANK_FOR_NO_CONENT && blankStatus == 0) {
        setVideodisableStatus(1);
    }

    if (blankOp == UNBLANK && blankStatus > 0) {
        setVideodisableStatus(0);
    }
    return 0;
}

int32_t LegacyVideoPlane::getMute(bool & staus) {
    uint32_t val = 1;
    if (ioctl(mDrvFd, AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT, &val) != 0) {
        MESON_LOGE("AMSTREAM_GET_VIDEO_OUTPUT ioctl fail(%d)", errno);
        return -EINVAL;
    }
    staus = (val == 0) ? true : false;

    return 0;
}

int32_t LegacyVideoPlane::setMute(bool status) {
    if (mPlaneMute != status) {
        MESON_LOGD("muteVideo to %d", status);
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
        if (mPlaneMute != val) {
            MESON_LOGE("status video (%d) vs (%d)", mPlaneMute, val);
        }
#endif
        MESON_LOGD("Already set video to (%d)", status);
    }

    return 0;
}

int32_t LegacyVideoPlane::getVideodisableStatus(int& status) {
    int ret = ioctl(mDrvFd, AMSTREAM_IOC_GET_VIDEO_DISABLE_MODE, &status);
    if (ret < 0) {
        MESON_LOGE("getvideodisable error, ret=%d", ret);
        return ret;
    }
    return 0;
}

int32_t LegacyVideoPlane::setVideodisableStatus(int status) {
    int ret = ioctl(mDrvFd, AMSTREAM_IOC_SET_VIDEO_DISABLE_MODE, &status);
    if (ret < 0) {
        MESON_LOGE("setvideodisable error, ret=%d", ret);
        return ret;
    }
    return 0;
}

int32_t LegacyVideoPlane::getOmxKeepLastFrame(unsigned int & keep) {
    int omx_info = 0;
    int ret = ioctl(mDrvFd, AMSTREAM_IOC_GET_OMX_INFO, (unsigned long)&omx_info);
    if (ret < 0) {
        MESON_LOGE("get omx info error, ret =%d", ret);
        keep = 0;
    } else {
        keep = omx_info & 0x1; //omx_info bit0: keep last frmame
        ret = 0;
    }
    return ret;
}

void LegacyVideoPlane::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
    MESON_LOG_EMPTY_FUN();
}

