/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "LegacyExtVideoPlane.h"
#include "AmFramebuffer.h"

#include <misc.h>
#include <sys/ioctl.h>
#include <tvp/OmxUtil.h>
#include <MesonLog.h>
#include <gralloc_priv.h>
#include <math.h>


//#define AMVIDEO_DEBUG

LegacyExtVideoPlane::LegacyExtVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id),
    mNeedUpdateAxis(false) {
    snprintf(mName, 64, "AmVideo-%d", id);

    if (getMute(mPlaneMute) != 0) {
        MESON_LOGE("get pip video mute failed.");
        mPlaneMute = false;
    }

    mOmxKeepLastFrame = 0;
    mLegacyExtVideoFb.reset();
    getOmxKeepLastFrame(mOmxKeepLastFrame);
}

LegacyExtVideoPlane::~LegacyExtVideoPlane() {

}

uint32_t LegacyExtVideoPlane::getPlaneType() {
    return LEGACY_EXT_VIDEO_PLANE;
}

const char * LegacyExtVideoPlane::getName() {
    return mName;
}

uint32_t LegacyExtVideoPlane::getCapabilities() {
    return 0;
}

int32_t LegacyExtVideoPlane::getFixedZorder() {
    return INVALID_ZORDER;
}

uint32_t LegacyExtVideoPlane::getPossibleCrtcs() {
    return CRTC_VOUT1;
}

bool LegacyExtVideoPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->mFbType == DRM_FB_VIDEO_SIDEBAND_SECOND ||
        fb->mFbType == DRM_FB_VIDEO_OMX_PTS_SECOND)
        return true;

    return false;
}

bool LegacyExtVideoPlane::shouldUpdateAxis(
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

int32_t LegacyExtVideoPlane::setPlane(
    std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder, int blankOp) {
    if (fb) {
        /*this is added to slove this situation:
         *when source has the signal, then playing video in MoivePlayer.
         *Then, back to home from MoviePlayer.Garbage appears.
         */
        if ((mLegacyExtVideoFb) && (fb)) {
           if (mLegacyExtVideoFb->mFbType == DRM_FB_VIDEO_OMX_PTS_SECOND && fb->mFbType != DRM_FB_VIDEO_OMX_PTS_SECOND) {
                setVideodisableStatus(2);
           }
        }

        mLegacyExtVideoFb = fb;

        buffer_handle_t buf = fb->mBufferHandle;
        /*set video axis.*/
        if (shouldUpdateAxis(fb)) {
            char videoAxisStr[MAX_STR_LEN] = {0};
            drm_rect_t * videoAxis = &(fb->mDisplayFrame);
            sprintf(videoAxisStr, "%d %d %d %d", videoAxis->left, videoAxis->top,
                videoAxis->right - 1, videoAxis->bottom - 1);
            sysfs_set_string(SYSFS_VIDEO_AXIS_PIP, videoAxisStr);
        }

        /*set omx pts.*/
        if (am_gralloc_is_omx_metadata_buffer(buf)) {
            private_handle_t const* buffer = private_handle_t::dynamicCast(buf);

            char *base = (char*)mmap(
                NULL, buffer->size, PROT_READ|PROT_WRITE,
                MAP_SHARED, buffer->share_fd, 0);

            if (base != MAP_FAILED) {
                set_omx_pts(base, &mDrvFd);
                setZorder(zorder);
                munmap(base, buffer->size);
                MESON_LOGV("PIP set omx pts ok.");
            } else {
                MESON_LOGE("PIP set omx pts failed.");
            }
        }
    }

    /*Update video plane blank status.*/
    if (!mLegacyExtVideoFb)
        return 0;

    if (blankOp == BLANK_FOR_SECURE_CONTENT) {
        setMute(true);
        return 0;
    } else if (blankOp == BLANK_FOR_NO_CONTENT) {
        setMute(true);
    } else if (blankOp == UNBLANK) {
        setMute(false);
    } else {
        MESON_LOGE("PIP not support blank type: %d", blankOp);
    }

    int blankStatus = 0;
    getVideodisableStatus(blankStatus);

    if (mLegacyExtVideoFb->mFbType == DRM_FB_VIDEO_OVERLAY) {
        if (blankOp == BLANK_FOR_NO_CONTENT && (blankStatus == 0 || blankStatus == 2)) {
            setVideodisableStatus(1);
        }

        if (blankOp == UNBLANK && (blankStatus == 1 || blankStatus == 2)) {
            setVideodisableStatus(0);
        }
    } else if (mLegacyExtVideoFb->mFbType == DRM_FB_VIDEO_SIDEBAND_SECOND || mLegacyExtVideoFb->mFbType == DRM_FB_VIDEO_OMX_PTS_SECOND) {
        if (mOmxKeepLastFrame) {
            if (blankOp == BLANK_FOR_NO_CONTENT && (blankStatus == 0 || blankStatus == 2)) {
                setVideodisableStatus(1);
            }

            if (blankOp == UNBLANK && blankStatus == 1) {
                setVideodisableStatus(2);
            }
        }
    } else {
        MESON_LOGI("PIP not support video fb type: %d", mLegacyExtVideoFb->mFbType);
    }

    if (blankOp == BLANK_FOR_NO_CONTENT)
        mLegacyExtVideoFb.reset();

    return 0;
}

int32_t LegacyExtVideoPlane::getMute(bool & staus) {
    uint32_t val = 1;
    if (ioctl(mDrvFd, AMSTREAM_IOC_GLOBAL_GET_VIDEOPIP_OUTPUT, &val) != 0) {
        MESON_LOGE("AMSTREAM_IOC_GLOBAL_GET_VIDEOPIP_OUTPUT ioctl fail(%d)", errno);
        return -EINVAL;
    }
    staus = (val == 0) ? true : false;

    return 0;
}

int32_t LegacyExtVideoPlane::setMute(bool status) {
    if (mPlaneMute != status) {
        MESON_LOGD("muteVideopip to %d", status);
        uint32_t val = status ? 0 : 1;
        if (ioctl(mDrvFd, AMSTREAM_IOC_GLOBAL_SET_VIDEOPIP_OUTPUT, val) != 0) {
            MESON_LOGE("AMSTREAM_IOC_GLOBAL_SET_VIDEOPIP_OUTPUT ioctl (%d) return(%d)", status, errno);
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
    }

    return 0;
}

int32_t LegacyExtVideoPlane::setZorder(uint32_t zorder) {
    if (ioctl(mDrvFd, AMSTREAM_IOC_SET_PIP_ZORDER, &zorder) != 0) {
        MESON_LOGE("AMSTREAM_IOC_SET_PIP_ZORDER ioctl return(%d)", errno);
        return -EINVAL;
    }
    return 0;
}

int32_t LegacyExtVideoPlane::getVideodisableStatus(int& status) {
    int ret = ioctl(mDrvFd, AMSTREAM_IOC_GET_VIDEOPIP_DISABLE, &status);
    if (ret < 0) {
        MESON_LOGE("getvideopipdisable error, ret=%d", ret);
        return ret;
    }
    return 0;
}

int32_t LegacyExtVideoPlane::setVideodisableStatus(int status) {
    int ret = ioctl(mDrvFd, AMSTREAM_IOC_SET_VIDEOPIP_DISABLE, &status);
    if (ret < 0) {
        MESON_LOGE("setvideopipdisable error, ret=%d", ret);
        return ret;
    }
    return 0;
}

int32_t LegacyExtVideoPlane::getOmxKeepLastFrame(unsigned int & keep) {
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

void LegacyExtVideoPlane::dump(String8 & dumpstr __unused) {
    MESON_LOG_EMPTY_FUN();
}

