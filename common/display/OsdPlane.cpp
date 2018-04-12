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
    getProperties();
    mPlaneInfo.out_fen_fd = -1;
    mPlaneInfo.op = 0x0;

    snprintf(mName, 64, "OSD-%d", id);
}

OsdPlane::~OsdPlane() {

}

int32_t OsdPlane::getProperties() {
    // TODO: set OSD1 to cursor plane with hard code for implement on p212
    // refrence board.
    int32_t ret = 0;

    mCapability = 0x0;
    if (mDrvFd < 0) {
        MESON_LOGE("osd plane fd is not valiable!");
        return -EBADF;
    }

    if (ioctl(mDrvFd, FBIOGET_OSD_CAPBILITY, &mCapability) != 0) {
        MESON_LOGE("osd plane get capibility ioctl (%d) return(%d)", mCapability, errno);
        return -EINVAL;
    }

    if (mCapability & OSD_LAYER_ENABLE) {
        mPlaneType = (mCapability & OSD_HW_CURSOR)
            ? CURSOR_PLANE : OSD_PLANE;
    }
    MESON_LOGD("osd%d plane type is %d", mId-30, mPlaneType);

    return ret;
}

const char * OsdPlane::getName() {
    return mName;
}

uint32_t OsdPlane::getPlaneType() {
    int32_t debugOsdPlanes = -1;
    char val[PROP_VALUE_LEN_MAX];

    memset(val, 0, sizeof(val));
    if (sys_get_string_prop("sys.hwc.debug.osdplanes", val))
        debugOsdPlanes = atoi(val);

    MESON_LOGV("debugOsdPlanes: %d", debugOsdPlanes);
    if (debugOsdPlanes == -1)
        return mPlaneType;
    else
        return (mId < 30 + debugOsdPlanes) ? OSD_PLANE : 0;
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
        sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
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
    mPlaneInfo.format        = am_gralloc_get_format(buf);
    mPlaneInfo.shared_fd     = am_gralloc_get_buffer_fd(buf);
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

    // this plane will be shown.
    blank(false);

    // update drm fb.
    mDrmFb = fb;

    mPlaneInfo.in_fen_fd  = -1;
    mPlaneInfo.out_fen_fd = -1;
    return 0;
}

int32_t OsdPlane::blank(bool blank) {
//    MESON_LOGD("osd%d plane set blank %d", mId-30, blank);
    if (mDrvFd < 0) {
        MESON_LOGE("osd plane fd is not valiable!");
        return -EBADF;
    }

    if (mBlank != blank) {
        uint32_t val = blank ? 1 : 0;
        if (ioctl(mDrvFd, FBIOPUT_OSD_SYNC_BLANK, &val) != 0) {
            MESON_LOGE("osd plane blank ioctl (%d) return(%d)", blank, errno);
            return -EINVAL;
        }
        mBlank = blank;
    }

    return 0;
}

String8 OsdPlane::compositionTypeToString() {
    String8 compType("NONE");

    if (mDrmFb) {
        switch (mDrmFb->mCompositionType) {
            case MESON_COMPOSITION_DUMMY:
                compType = "DUMMY";
                break;
            case MESON_COMPOSITION_GE2D:
                compType = "GE2D";
                break;
            case MESON_COMPOSITION_PLANE_VIDEO:
                compType = "VIDEO";
                break;
            case MESON_COMPOSITION_PLANE_VIDEO_SIDEBAND:
                compType = "SIDEBAND";
                break;
            case MESON_COMPOSITION_PLANE_OSD:
                compType = "OSD";
                break;
            case MESON_COMPOSITION_PLANE_OSD_COLOR:
                compType = "COLOR";
                break;
            case MESON_COMPOSITION_PLANE_CURSOR:
                compType = "CURSOR";
                break;
            default:
                compType = "NONE";
                break;
        }
    }

    return compType;
}

void OsdPlane::dump(String8 & dumpstr) {
    if (!mBlank) {
        dumpstr.appendFormat("osd%2d "
                "     %3d | %8s | %1d | %4d, %4d, %4d, %4d |  %4d, %4d, %4d, %4d | %2d | %2d | %4d |"
                " %4d | %5d | %5d | %4x |%8x  |\n",
                 mId,
                 mPlaneInfo.zorder,
                 compositionTypeToString().string(),
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

