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
#include <DebugHelper.h>

OsdPlane::OsdPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id),
      mDrmFb(NULL),
      mFirstPresent(true),
      mBlank(true) {
    mPlaneInfo.out_fen_fd = -1;
    mPlaneInfo.op = 0x0;
    snprintf(mName, 64, "OSD-%d", id);

    getProperties();
}

OsdPlane::~OsdPlane() {

}

int32_t OsdPlane::getProperties() {
    mCapability = 0;

    int capacity;
    if (ioctl(mDrvFd, FBIOGET_OSD_CAPBILITY, &capacity) != 0) {
        MESON_LOGE("osd plane get capibility ioctl (%d) return(%d)", mCapability, errno);
        return 0;
    }

    if (capacity & OSD_ZORDER_EN) {
        mCapability |= PLANE_SUPPORT_ZORDER;
    }
    if (capacity & OSD_FREESCALE) {
        mCapability |= PLANE_SUPPORT_FREE_SCALE;
    }
    if (capacity & OSD_UBOOT_LOGO) {
        mCapability |= PLANE_SHOW_LOGO;
    }
    if (capacity & OSD_VIDEO_CONFLICT) {
        mCapability |= PLANE_VIDEO_CONFLICT;
    }
    if (capacity & OSD_PLANE_PRIMARY) {
        mCapability |= PLANE_PRIMARY;
    }

    return 0;
}

const char * OsdPlane::getName() {
    return mName;
}

uint32_t OsdPlane::getPlaneType() {
    if (mIdle) {
        return INVALID_PLANE;
    }

    return OSD_PLANE;
}

uint32_t OsdPlane::getCapabilities() {
    return mCapability;
}

int32_t OsdPlane::getFixedZorder() {
    if (mCapability & PLANE_SUPPORT_ZORDER) {
        return PLANE_VARIABLE_ZORDER;
    }

    return OSD_PLANE_FIXED_ZORDER;
}

int32_t OsdPlane::setPlane(std::shared_ptr<DrmFramebuffer> &fb) {
    if (mDrvFd < 0) {
        MESON_LOGE("osd plane fd is not valiable!");
        return -EBADF;
    }

    // close uboot logo, if bootanim begin to show
    if (mFirstPresent) {
        // TODO: will move this in plane info op, and do this in the driver with
        // one vsync.
        mFirstPresent = false;
        sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
        //sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
    }

    drm_rect_t srcCrop       = fb->mSourceCrop;
    drm_rect_t disFrame      = fb->mDisplayFrame;
    buffer_handle_t buf      = fb->mBufferHandle;

    mPlaneInfo.magic         = OSD_SYNC_REQUEST_RENDER_MAGIC_V2;
    mPlaneInfo.len           = sizeof(osd_plane_info_t);
    mPlaneInfo.type          = DIRECT_COMPOSE_MODE;

    mPlaneInfo.xoffset       = srcCrop.left;
    mPlaneInfo.yoffset       = srcCrop.top;
    mPlaneInfo.width         = srcCrop.right  - srcCrop.left;
    mPlaneInfo.height        = srcCrop.bottom - srcCrop.top;

    mPlaneInfo.dst_x         = disFrame.left;
    mPlaneInfo.dst_y         = disFrame.top;
    mPlaneInfo.dst_w         = disFrame.right  - disFrame.left;
    mPlaneInfo.dst_h         = disFrame.bottom - disFrame.top;

    if (DebugHelper::getInstance().discardInFence()) {
        fb->getAcquireFence()->waitForever("osd-input");
        mPlaneInfo.in_fen_fd = -1;
    } else {
        mPlaneInfo.in_fen_fd     = fb->getAcquireFence()->dup();
    }

    mPlaneInfo.shared_fd     = ::dup(am_gralloc_get_buffer_fd(buf));
    mPlaneInfo.format        = am_gralloc_get_format(buf);
    mPlaneInfo.byte_stride   = am_gralloc_get_stride_in_byte(buf);
    mPlaneInfo.pixel_stride  = am_gralloc_get_stride_in_pixel(buf);
    /* osd request plane zorder > 0 */
    mPlaneInfo.zorder        = fb->mZorder + 1;
    mPlaneInfo.blend_mode    = fb->mBlendMode;
    mPlaneInfo.plane_alpha   = fb->mPlaneAlpha;
    mPlaneInfo.op            &= ~(OSD_BLANK_OP_BIT);
    mPlaneInfo.afbc_inter_format = am_gralloc_get_vpu_afbc_mask(buf);

    if (ioctl(mDrvFd, FBIOPUT_OSD_SYNC_RENDER_ADD, &mPlaneInfo) != 0) {
        MESON_LOGE("osd plane FBIOPUT_OSD_SYNC_RENDER_ADD return(%d)", errno);
        return -EINVAL;
    }

    if (mDrmFb) {
    /* dup a out fence fd for layer's release fence, we can't close this fd
    * now, cause display retire fence will also use this fd. will be closed
    * on SF side*/
        if (DebugHelper::getInstance().discardOutFence()) {
            mDrmFb->setReleaseFence(-1);
        } else {
            mDrmFb->setReleaseFence((mPlaneInfo.out_fen_fd >= 0) ? ::dup(mPlaneInfo.out_fen_fd) : -1);
        }
    }

    // update drm fb.
    mDrmFb = fb;

    mPlaneInfo.in_fen_fd  = -1;
    mPlaneInfo.out_fen_fd = -1;
    return 0;
}

int32_t OsdPlane::blank(int blankOp) {
//    MESON_LOGD("osd%d plane set blank %d", mId-30, blank);
    bool bBlank = (blankOp == UNBLANK) ? false : true;

    if (mBlank != bBlank) {
        uint32_t val = bBlank ? 1 : 0;
        if (ioctl(mDrvFd, FBIOPUT_OSD_SYNC_BLANK, &val) != 0) {
            MESON_LOGE("osd plane blank ioctl (%d) return(%d)", bBlank, errno);
            return -EINVAL;
        }
        mBlank = bBlank;
    }

    return 0;
}

void OsdPlane::dump(String8 & dumpstr) {
    if (!mBlank) {
        dumpstr.appendFormat("osd%2d "
                "     %3d | %1d | %4d, %4d, %4d, %4d |  %4d, %4d, %4d, %4d | %2d | %2d | %4d |"
                " %4d | %5d | %5d | %4x |%8x  |\n",
                 mId,
                 mPlaneInfo.zorder,
                 mPlaneInfo.type,
                 mPlaneInfo.xoffset, mPlaneInfo.yoffset, mPlaneInfo.width, mPlaneInfo.height,
                 mPlaneInfo.dst_x, mPlaneInfo.dst_y, mPlaneInfo.dst_w, mPlaneInfo.dst_h,
                 mPlaneInfo.shared_fd,
                 mPlaneInfo.format,
                 mPlaneInfo.byte_stride,
                 mPlaneInfo.pixel_stride,
                 mPlaneInfo.blend_mode,
                 mPlaneInfo.plane_alpha,
                 mPlaneInfo.op,
                 mPlaneInfo.afbc_inter_format);
    }
}

