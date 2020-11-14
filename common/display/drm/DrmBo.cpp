/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <DrmBo.h>
#include <MesonLog.h>

#include <xf86drm.h>
#include <drm_fourcc.h>

#include "DrmDevice.h"

uint32_t covertToDrmFormat(uint32_t format) {
    switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
            return DRM_FORMAT_ARGB8888;
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return DRM_FORMAT_ABGR8888;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            return DRM_FORMAT_XBGR8888;
        case HAL_PIXEL_FORMAT_RGB_888:
            return DRM_FORMAT_BGR888;
        case HAL_PIXEL_FORMAT_RGB_565:
            return DRM_FORMAT_BGR565;
        default:
            //MESON_LOGE("covert format  %u failed.", format);
            return DRM_FORMAT_INVALID;
    }
}

uint64_t convertToDrmModifier(int afbcMask) {
    UNUSED(afbcMask);
 //   MESON_LOG_EMPTY_FUN ();
    return 0;

#if 0
    uint64_t features = 0UL;

    if (afbcMask & MALI_GRALLOC_INTFMT_AFBC_BASIC) {
    if (afbcMask & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
        features |= AFBC_FORMAT_MOD_BLOCK_SIZE_32x8;
    else
        features |= AFBC_FORMAT_MOD_BLOCK_SIZE_16x16;
    }

    if (afbcMask & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK)
        features |= (AFBC_FORMAT_MOD_SPLIT | AFBC_FORMAT_MOD_SPARSE);

    if (afbcMask & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
        features |= AFBC_FORMAT_MOD_TILED;

    if (features)
        return DRM_FORMAT_MOD_ARM_AFBC(features | AFBC_FORMAT_MOD_YTR);

    return 0;
#endif
}

DrmBo::DrmBo() {
    fbId = 0;
}

DrmBo::~DrmBo() {
    release();
}

int32_t DrmBo::import(
    std::shared_ptr<DrmFramebuffer> & fb) {
    if (fbId > 0)
        release();

    int ret = 0;
    native_handle_t * buf = fb->mBufferHandle;
    int drmFd = getDrmDevice()->getDeviceFd();

    ret = drmPrimeFDToHandle(drmFd, am_gralloc_get_buffer_fd(buf), &handles[0]);
    MESON_ASSERT(!ret, "FdToHandle failed(%d)\n", ret);

    pitches[0] = am_gralloc_get_stride_in_byte(buf);
    offsets[0] = 0;
    modifiers[0] = 0;
    /*set other elemnts to invalid.*/
    for (int i = 1; i < BUF_PLANE_NUM; i++) {
        handles[i] = 0;
        pitches[i] = 0;
        offsets[i] = 0;
        modifiers[i] = 0;
    }

    width = am_gralloc_get_width(buf);
    height = am_gralloc_get_height(buf);
    format = covertToDrmFormat(am_gralloc_get_format(buf));
    if (format == DRM_FORMAT_INVALID) {
        return -EINVAL;
    }


    ret = drmModeAddFB2WithModifiers(drmFd, width, height,
                    format, handles, pitches,
                    offsets, modifiers, &fbId,
                    modifiers[0] ? DRM_MODE_FB_MODIFIERS : 0);
    MESON_ASSERT(!ret, "drmModeAddFB2 failed(%d)", ret);

    srcRect = fb->mSourceCrop;
    crtcRect = fb->mDisplayFrame;
    alpha = fb->mPlaneAlpha;
    blend = fb->mBlendMode;
    z = fb->mZorder;
    inFence = fb->getAcquireFence()->dup();

    return ret;
}

int32_t DrmBo::release() {
    if (fbId == 0)
        return 0;

    int ret = 0;
    struct drm_gem_close closeArg;
    int drmFd = getDrmDevice()->getDeviceFd();

    if (inFence >=0)
        close(inFence);
    inFence = -1;

    ret = drmModeRmFB(drmFd, fbId);
    if (ret !=0) {
        MESON_LOGE("drmModeRmFB failed fb[%d]ret[%d]", fbId, ret);
        return ret;
    }

    for (int i = 0; i < BUF_PLANE_NUM; i++) {
        if (!handles[i])
            continue;

        memset(&closeArg, 0, sizeof(drm_gem_close));
        closeArg.handle = handles[i];
        int ret = drmIoctl(drmFd, DRM_IOCTL_GEM_CLOSE, &closeArg);
        if (ret !=0) {
             MESON_LOGE("DRM_IOCTL_GEM_CLOSE failed fb[%d]ret[%d]", fbId, ret);
             break;
         }
        handles[i] = 0;
    }

    return ret;
}

