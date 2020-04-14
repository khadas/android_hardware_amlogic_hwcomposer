/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "HwcVideoPlane.h"
#include "AmFramebuffer.h"
#include <DebugHelper.h>
#include <OmxUtil.h>
#include <misc.h>


inline bool isSidebandVideo(drm_fb_type_t fbtype) {
    if (fbtype == DRM_FB_VIDEO_SIDEBAND ||
        fbtype == DRM_FB_VIDEO_SIDEBAND_TV ||
        fbtype == DRM_FB_VIDEO_SIDEBAND_SECOND) {
        return true;
    }

    return false;
}

HwcVideoPlane::HwcVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id) {
    snprintf(mName, 64, "HwcVideo-%d", id);
    mDisplayedVideoType = DRM_FB_UNDEFINED;
    memset(mAmVideosPath, 0, sizeof(mAmVideosPath));
    mBlank = true;
}

HwcVideoPlane::~HwcVideoPlane() {
}

const char * HwcVideoPlane::getName() {
    return mName;
}

uint32_t HwcVideoPlane::getPlaneType() {
    return HWC_VIDEO_PLANE;
}

uint32_t HwcVideoPlane::getCapabilities() {
    /*HWCVideoplane always support zorder.*/
    return PLANE_SUPPORT_ZORDER;
}

int32_t HwcVideoPlane::getFixedZorder() {
    return INVALID_ZORDER;
}

uint32_t HwcVideoPlane::getPossibleCrtcs() {
    return CRTC_VOUT1;
}

bool HwcVideoPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->mFbType == DRM_FB_VIDEO_OMX_V4L ||
        fb->mFbType == DRM_FB_VIDEO_OMX2_V4L2)
        return true;

    return false;
}

void HwcVideoPlane::setAmVideoPath(int id) {
    if (id == 0) {
        strncpy(mAmVideosPath, "/sys/class/video/disable_video", sizeof(mAmVideosPath));
    } else if (id == 1) {
        strncpy(mAmVideosPath, "/sys/class/video/disable_videopip", sizeof(mAmVideosPath));
    } else {
        // nothing
        MESON_LOGE("unknown video path (%d)", id);
    }

    mVideoComposer = getVideoComposerDev(id);
}

int32_t HwcVideoPlane::getVideodisableStatus(int& status) {
    /*fun is only called in updateVideodisableStatus(), mVideoDisableFd >= 0*/
    char buf[32] = {0};
    int ret;
    int fd = -1;

    if (strlen(mAmVideosPath) == 0)
        return -1;

    if ((fd = open(mAmVideosPath,  O_RDWR, 0)) < 0) {
        MESON_LOGE("open %s failed", mAmVideosPath);
        return -1;
    }

    if ((ret = read(fd, buf, sizeof(buf))) < 0) {
        MESON_LOGE("get video disable failed, ret=%d error=%s", ret, strerror(errno));
    } else {
        sscanf(buf, "%d", &status);
        ret = 0;
    }

    close(fd);
    return ret;
}

int32_t HwcVideoPlane::setVideodisableStatus(int status) {
    /*fun is only called in updateVideodisableStatus(), mVideoDisableFd >= 0*/
    char buf[32] = {0};
    int ret;
    int fd = -1;

    if (strlen(mAmVideosPath) == 0)
        return -1;

    if ((fd = open(mAmVideosPath,  O_RDWR, 0)) < 0) {
        MESON_LOGE("open %s failed", mAmVideosPath);
        return -1;
    }

    snprintf(buf, 32, "%d", status);
    if ((ret = write(fd, buf, strnlen(buf, sizeof(buf)))) < 0) {
        MESON_LOGE("set video disable failed, ret=%d", ret);
    } else {
        ret = 0;
    }

    close(fd);
    return ret;
}

int32_t HwcVideoPlane::setPlane(
    std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder, int blankOp) {
    MESON_ASSERT(mDrvFd >= 0, "osd plane fd is not valiable!");

    bool bBlank = blankOp == UNBLANK ? false : true;
    if (!bBlank) {
        MESON_ASSERT(fb.get() != NULL, "fb shoud not NULL");

        /*diable video if sideband vide.*/
        drm_fb_type_t type = fb->mFbType;
        if (mDisplayedVideoType != DRM_FB_UNDEFINED) {
            if (type != mDisplayedVideoType) {
                if (isSidebandVideo(type) && isSidebandVideo(mDisplayedVideoType)) {
                    mVideoComposer->enable(false);
                }
                mDisplayedVideoType = DRM_FB_UNDEFINED;
            }
        }

        /* update video plane disable status */
        /* the value of blankOp is UNBLANK */
        if (fb->mFbType == DRM_FB_VIDEO_OMX_V4L ||
            fb->mFbType == DRM_FB_VIDEO_OMX2_V4L2 ||
            fb->mFbType == DRM_FB_VIDEO_DMABUF) {
            int blankStatus = 0;
            getVideodisableStatus(blankStatus);
            if (blankStatus == 1) {
                setVideodisableStatus(2);
            }
        }

        if (fb->mFbType == DRM_FB_DI_COMPOSE_OUTPUT) {
            /*Nothing to do now, di composer will post directlly,
        * fence will set by composer also.
        .*/
            MESON_LOGD("Nothing to do for COMPOSE OUTPUT.");
        } else {
            int composefd = -1;
            /*Video post to display directlly.*/
            if (mDisplayedVideoType == DRM_FB_UNDEFINED) {
                mVideoComposer->enable(true);
            }
            mVideoComposer->setFrame(fb, composefd, zorder);

            /*update last frame release fence*/
            if (DebugHelper::getInstance().discardOutFence()) {
                fb->setReleaseFence(-1);
            } else {
                fb->setReleaseFence(composefd);
            }
        }

        mDisplayedVideoType = fb->mFbType;
        mVideoFb = fb;
    }else if (mBlank != bBlank) {
        mVideoComposer->enable(false);
        mDisplayedVideoType = DRM_FB_UNDEFINED;
        mVideoFb.reset();
    }

    mBlank = bBlank;
    return 0;
}

void HwcVideoPlane::dump(String8 & dumpstr) {
    dumpstr.appendFormat("HwcVideo %d %d %d",
            mId,
            mVideoFb->mZorder,
            mVideoFb->mFbType);
}

