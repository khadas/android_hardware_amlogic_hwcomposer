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

#include <misc.h>
#include <sys/ioctl.h>
#include <Amvideoutils.h>
#include <tvp/OmxUtil.h>
#include <MesonLog.h>
#include <gralloc_priv.h>

//#define AMVIDEO_DEBUG

/*Used for zoom position*/
#define OFFSET_STEP          2
#define PERCENT_FULL_SCREEN  100

LegacyVideoPlane::LegacyVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id),
    mZoomPercent(PERCENT_FULL_SCREEN),
    mNeedUpdateAxis(false) {
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

uint32_t LegacyVideoPlane::getCapabilities() {
    return 0;
}

int32_t LegacyVideoPlane::getFixedZorder() {
    /*Legacy video plane not support PLANE_SUPPORT_ZORDER, always at bottom*/
    return LEGACY_VIDEO_PLANE_FIXED_ZORDER;
}

bool LegacyVideoPlane::shouldUpdateAxis(
    std::shared_ptr<DrmFramebuffer> &fb) {
    bool bUpdate = false;

    // TODO: we need to update video axis while mode or freescale state is changed.
    if (mNeedUpdateAxis) {
        mNeedUpdateAxis = false;
        bUpdate = true;
    }

    drm_rect_t *displayFrame = &(fb->mDisplayFrame);

    if (memcmp(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t))) {
        memcpy(&mBackupDisplayFrame, displayFrame, sizeof(drm_rect_t));
        bUpdate = true;
    }

    return bUpdate;
}

int32_t LegacyVideoPlane::setPlane(
    std::shared_ptr<DrmFramebuffer> & fb,
    uint32_t zorder __unused) {
    buffer_handle_t buf = fb->mBufferHandle;

    // TODO: DONOT set mute for now, because we need to implement secure display.
    if (shouldUpdateAxis(fb)) {
        drm_rect_t *displayFrame = &(fb->mDisplayFrame);
        char axis[MAX_STR_LEN] = {0};

        sprintf(axis, "%d %d %d %d", displayFrame->left, displayFrame->top,
                    displayFrame->right, displayFrame->bottom);
        //MESON_LOGV("Set video axis: %s", axis);
        if (mZoomPercent != 0) {
            setScale(*displayFrame, axis);
        }

        sysfs_set_string(SYSFS_VIDEO_AXIS, axis);
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

int32_t LegacyVideoPlane::setScale(drm_rect_t disPosition, char * axis) {
    int zoom_w, zoom_h;
    int disp_w = disPosition.right  - disPosition.left;
    int disp_h = disPosition.bottom - disPosition.top;
    drm_rect_t curPosition = disPosition;

    zoom_w = (100 - mZoomPercent)*(disPosition.right)/(100*2*OFFSET_STEP);
    zoom_h = (100 - mZoomPercent)*(disPosition.bottom)/(100*2*OFFSET_STEP);
    curPosition.left      += zoom_w;
    curPosition.top       += zoom_h;
    curPosition.right     -= zoom_w;
    curPosition.bottom    -= zoom_h;

    float tmp_x,tmp_y,tmp_r,tmp_b;
    tmp_x = (float)((float)((curPosition.left)    * mWindowW) / (float)disp_w);
    tmp_y = (float)((float)((curPosition.top)     * mWindowH) / (float)disp_h);
    tmp_r = (float)((float)((curPosition.right)   * mWindowW) / (float)disp_w);
    tmp_b = (float)((float)((curPosition.bottom)  * mWindowH) / (float)disp_h);
    curPosition.left   = (int)(tmp_x+0.5);
    curPosition.top    = (int)(tmp_y+0.5);
    curPosition.right  = (int)(tmp_r+0.5);
    curPosition.bottom = (int)(tmp_b+0.5);

    sprintf(axis, "%d %d %d %d", curPosition.left, curPosition.top,
                curPosition.right, curPosition.bottom);
    MESON_LOGD("After scale video axis: %s", axis);

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

int32_t LegacyVideoPlane::updateZoomInfo(display_zoom_info_t zoomInfo) {
    mZoomPercent = zoomInfo.percent;
    mWindowW = zoomInfo.width  - 1;
    mWindowH = zoomInfo.height - 1;
    mNeedUpdateAxis = true;
    return 0;
}

void LegacyVideoPlane::dump(String8 & dumpstr __unused) {
    MESON_LOG_EMPTY_FUN();
}

