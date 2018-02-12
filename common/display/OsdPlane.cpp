/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "OsdPlane.h"
#include <MesonLog.h>
#include <misc.h>

#define FBIOPUT_OSD_SYNC_RENDER_ADD  0x4519

OsdPlane::OsdPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane (drvFd, id),
      mPriorFrameRetireFd(-1),
      mFirstPresentDisplay(false),
      mRetireFence(DrmFence::NO_FENCE) {
    getProperties();
    mPlaneInfo.out_fen_fd = -1;
    mPlaneInfo.op = 0x0;
}

OsdPlane::~OsdPlane() {

}

int32_t OsdPlane::getProperties() {
    // TODO: set OSD1 to cursor plane with hard code for implement on p212
    // refrence board.

    if (mId == 30) {
        mPlaneType = OSD_PLANE;
    } else /*if (mId == 31) */{
        mPlaneType = CURSOR_PLANE;
    }
    return 0;
}

int OsdPlane::setPlane(std::shared_ptr<DrmFramebuffer> &fb) {
    // close uboot logo, if bootanim begin to show
    if (mFirstPresentDisplay) {
        // TODO: will move this in plane info op, and do this in the driver with
        // one vsync.
        mFirstPresentDisplay = false;
        sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
        sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
    }

    mPriorFrameRetireFd      = mRetireFence->dup();

    drm_rect_t srcCrop       = fb->mSourceCrop;
    drm_rect_t disFrame      = fb->mDisplayFrame;
    buffer_handle_t buf      = fb->mBufferHandle;

    mPlaneInfo.type          = DIRECT_COMPOSE_MODE;

    mPlaneInfo.xoffset       = srcCrop.left;
    mPlaneInfo.yoffset       = srcCrop.top;
    mPlaneInfo.width         = srcCrop.right  - srcCrop.left;
    mPlaneInfo.height        = srcCrop.bottom - srcCrop.top;

    mPlaneInfo.dst_x         = disFrame.left;
    mPlaneInfo.dst_y         = disFrame.top;
    mPlaneInfo.dst_w         = disFrame.right  - disFrame.left;
    mPlaneInfo.dst_h         = disFrame.bottom - disFrame.top;

    mPlaneInfo.in_fen_fd     = fb->getAcquireFence()->dup();
    mPlaneInfo.format        = PrivHandle::getFormat(buf);
    mPlaneInfo.shared_fd     = PrivHandle::getFd(buf);
    mPlaneInfo.byte_stride   = PrivHandle::getBStride(buf);
    mPlaneInfo.stride        = PrivHandle::getPStride(buf);
    mPlaneInfo.zorder        = fb->mZorder;
    mPlaneInfo.blend_mode    = fb->mBlendMode;
    mPlaneInfo.plane_alpha   = fb->mPlaneAlpha;
    mPlaneInfo.op           &= ~(OSD_BLANK_OP_BIT);
    MESON_LOGD("osdPlane [%p]", (void*)buf);

    fb->setReleaseFence(mRetireFence->dup());
    ioctl(mDrvFd, FBIOPUT_OSD_SYNC_RENDER_ADD, &mPlaneInfo);
    mRetireFence.reset(new DrmFence(mPlaneInfo.out_fen_fd));
    dumpPlaneInfo();

    mPlaneInfo.in_fen_fd     = -1;
    return 0;
}

int32_t OsdPlane::blank() {
    MESON_LOG_EMPTY_FUN();
    mPlaneInfo.op |= OSD_BLANK_OP_BIT;
    // ioctl(mDrvFd, FBIOPUT_OSD_SYNC_RENDER_ADD, &mPlaneInfo);

    return 0;
}

int32_t OsdPlane::pageFlip(int32_t &outFence) {
    outFence = mPriorFrameRetireFd;
    mPriorFrameRetireFd = -1;
    return 0;
}

void OsdPlane::dumpPlaneInfo() {
    MESON_LOGD("*****PLANE INFO*****");
    MESON_LOGD("type: %d", mPlaneInfo.type);
    MESON_LOGD("src: [%d, %d, %d, %d]", mPlaneInfo.xoffset, mPlaneInfo.yoffset, mPlaneInfo.width, mPlaneInfo.height);
    MESON_LOGD("dst: [%d, %d, %d, %d]", mPlaneInfo.dst_x, mPlaneInfo.dst_y, mPlaneInfo.dst_w, mPlaneInfo.dst_h);
    MESON_LOGD("infd: %d", mPlaneInfo.in_fen_fd);
    MESON_LOGD("oufd: %d", mPlaneInfo.out_fen_fd);
    MESON_LOGD("form: %d", mPlaneInfo.format);
    MESON_LOGD("shfd: %d", mPlaneInfo.shared_fd);
    MESON_LOGD("bstr: %d", mPlaneInfo.byte_stride);
    MESON_LOGD("pstr: %d", mPlaneInfo.stride);
    MESON_LOGD("zord: %d", mPlaneInfo.zorder);
    MESON_LOGD("blen: %d", mPlaneInfo.blend_mode);
    MESON_LOGD("alph: %d", mPlaneInfo.plane_alpha);
    MESON_LOGD("op:   %d", mPlaneInfo.op);
    MESON_LOGD("********************");
}

void OsdPlane::dump(String8 & dumpstr) {
}

