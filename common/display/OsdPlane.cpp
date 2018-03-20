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

OsdPlane::OsdPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id),
      mDrmFb(NULL),
      mFirstPresent(true),
      mBlank(true) {
    getProperties();
    mPlaneInfo.out_fen_fd = -1;
    mPlaneInfo.op = 0x0;
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

    mPlaneInfo.in_fen_fd     = fb->getAcquireFence()->dup();
    mPlaneInfo.format        = PrivHandle::getFormat(buf);
    mPlaneInfo.shared_fd     = PrivHandle::getFd(buf);
    mPlaneInfo.byte_stride   = PrivHandle::getBStride(buf);
    mPlaneInfo.pixel_stride  = PrivHandle::getPStride(buf);
    /* osd request plane zorder > 0 */
    mPlaneInfo.zorder        = fb->mZorder + 1;
    mPlaneInfo.blend_mode    = fb->mBlendMode;
    mPlaneInfo.plane_alpha   = fb->mPlaneAlpha;
    mPlaneInfo.op            &= ~(OSD_BLANK_OP_BIT);
    mPlaneInfo.afbc_inter_format
        = translateInternalFormat(PrivHandle::getInternalFormat(buf));

    if (ioctl(mDrvFd, FBIOPUT_OSD_SYNC_RENDER_ADD, &mPlaneInfo) != 0) {
        MESON_LOGE("osd plane FBIOPUT_OSD_SYNC_RENDER_ADD return(%d)", errno);
        return -EINVAL;
    }

    if (mDrmFb) {
        /* dup a out fence fd for layer's release fence, we can't close this fd
         * now, cause display retire fence will also use this fd. will be closed
         * on SF side*/
        mDrmFb->setReleaseFence((mPlaneInfo.out_fen_fd >= 0) ? ::dup(mPlaneInfo.out_fen_fd) : -1);
    }

    // this plane will be shown.
    blank(false);

    // update drm fb.
    mDrmFb = fb;

    mPlaneInfo.in_fen_fd  = -1;
    mPlaneInfo.out_fen_fd = -1;
    return 0;
}

int OsdPlane::translateInternalFormat(uint64_t internalFormat) {
    int afbcFormat = 0;

    if (internalFormat & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK) {
        afbcFormat |= (OSD_AFBC_EN | OSD_YUV_TRANSFORM | OSD_BLOCK_SPLIT);
        if (internalFormat & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK) {
            afbcFormat |= OSD_SUPER_BLOCK_ASPECT;
        }

        if (internalFormat & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK) {
            afbcFormat |= OSD_BLOCK_SPLIT;
        }

        /*if (internalFormat & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK_YUV_DISABLE) {
            afbcFormat &= ~OSD_YUV_TRANSFORM;
        }*/

        if (internalFormat & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS) {
            afbcFormat |= OSD_TILED_HEADER_EN;
        }
    }

    MESON_LOGV("internal format: 0x%llx translated afbc format: 0x%x",
            internalFormat, afbcFormat);

    return afbcFormat;
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
            case MESON_COMPOSITION_CLIENT_TARGET:
                compType = "CLIENT";
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
        dumpstr.appendFormat("  osd%1d \n"
                "     %3d | %10s | %1d | %4d, %4d, %4d, %4d |  %4d, %4d, %4d, %4d | %2d |   %2d   | %4d |"
                " %4d | %5d | %5d | %4x |  %8x  |\n",
                 mId - 30,
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

