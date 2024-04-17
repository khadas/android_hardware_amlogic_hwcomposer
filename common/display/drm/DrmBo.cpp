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
#include "DrmDevice.h"

std::map<uint32_t, int> DrmBo::mHndRefs;

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
        case HAL_PIXEL_FORMAT_RGBA_1010102:
            return DRM_FORMAT_ABGR2101010;
        case HAL_PIXEL_FORMAT_RGBA_10101010:
            return DRM_FORMAT_ABGR10101010;
        default:
            //MESON_LOGE("covert format  %u failed.", format);
            return DRM_FORMAT_INVALID;
    }
}

uint64_t convertToDrmModifier(int afbcMask, int afrcMask) {
    if (afbcMask == 0 && afrcMask == 0) {
        return 0;
    }

    if (afrcMask != 0) {
        return DRM_FORMAT_MOD_ARM_AFRC(afrcMask);
    }

    uint64_t features = 0UL;

    if (afbcMask & VPU_AFBC_EN) {
        if (afbcMask & VPU_AFBC_SUPER_BLOCK_ASPECT)
            features |= AFBC_FORMAT_MOD_BLOCK_SIZE_32x8;
        else
            features |= AFBC_FORMAT_MOD_BLOCK_SIZE_16x16;
    }

    if (afbcMask & VPU_AFBC_BLOCK_SPLIT)
        features |= (AFBC_FORMAT_MOD_SPLIT | AFBC_FORMAT_MOD_SPARSE);

    if (afbcMask & VPU_AFBC_TILED_HEADER_EN)
        features |= AFBC_FORMAT_MOD_TILED;

    if (features)
        return DRM_FORMAT_MOD_ARM_AFBC(features | AFBC_FORMAT_MOD_YTR);

    return 0;
}

DrmBo::DrmBo() {
    fbId = 0;
    width = 0;
    height = 0;
    format = 0;
    alpha = 0;
    blend = 0;
    z = 0;
    inFence = -1;
    color = 0;
    secureEnable = 0;
    memset(&srcRect, 0, sizeof(srcRect));
    memset(&crtcRect, 0, sizeof(crtcRect));
}

DrmBo::~DrmBo() {
    release();
}

void DrmBo::refHandle(uint32_t hnd) {
    mHndRefs[hnd] ++;
}

void DrmBo::unrefHandle(uint32_t hnd) {
    if (--mHndRefs[hnd])
      return ;

    mHndRefs.erase(hnd);

    struct drm_gem_close closeArg;
    int drmFd = getDrmDevice()->getDeviceFd();
    memset(&closeArg, 0, sizeof(drm_gem_close));
    closeArg.handle = hnd;
    int ret = drmIoctl(drmFd, DRM_IOCTL_GEM_CLOSE, &closeArg);
    if (ret != 0)
        MESON_LOGE("DRM_IOCTL_GEM_CLOSE failed hnd[%d]ret[%d]", hnd, ret);
}

int32_t DrmBo::import(
    std::shared_ptr<DrmFramebuffer> & fb) {
    int ret = 0;
    native_handle_t * buf = fb->mBufferHandle;
    int drmFd = getDrmDevice()->getDeviceFd();

    if (fbId > 0)
        release();

    int halFormat = am_gralloc_get_format(buf);
    format = covertToDrmFormat(halFormat);
    if (format == DRM_FORMAT_INVALID) {
        MESON_LOGE("import meet unknown format[%x], buf=%p", halFormat, buf);
        return -EINVAL;
    }

    ret = drmPrimeFDToHandle(drmFd, am_gralloc_get_buffer_fd(buf), &handles[0]);
    if (ret != 0) {
        MESON_LOGE("FdToHandle failed fd(%d) ret(%d)\n", am_gralloc_get_buffer_fd(buf), ret);
        return ret;
    }

    width = am_gralloc_get_width(buf);
    height = am_gralloc_get_height(buf);
    int afbc = am_gralloc_get_vpu_afbc_mask(buf);
    int afrc = am_gralloc_get_vpu_afrc_mask(buf);
    pitches[0] = am_gralloc_get_stride_in_byte(buf);
    offsets[0] = 0;
    modifiers[0] = convertToDrmModifier(afbc, afrc);

    /*set other elements to invalid.*/
    for (int i = 1; i < BUF_PLANE_NUM; i++) {
        handles[i] = 0;
        pitches[i] = 0;
        offsets[i] = 0;
        modifiers[i] = 0;
    }

    ret = drmModeAddFB2WithModifiers(drmFd, width, height,
                    format, handles, pitches,
                    offsets, modifiers, &fbId,
                    modifiers[0] ? DRM_MODE_FB_MODIFIERS : 0);
    MESON_ASSERT(!ret, "drmModeAddFB2 failed(%d)", ret);

    srcRect = fb->getSourceCrop();
    crtcRect = fb->getDisplayFrame();
    alpha = fb->mPlaneAlpha;
    blend = fb->mBlendMode;
    z = fb->mZorder;
    inFence = fb->getAcquireFence() ? fb->getAcquireFence()->dup() : -1;
    secureEnable = am_gralloc_is_secure_buffer(buf) ?  1 : 0;

    refHandle(handles[0]);
    return ret;
}

int32_t DrmBo::release() {
    if (fbId == 0)
        return 0;

    int ret = 0;
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

        for (int j = i + 1; j < BUF_PLANE_NUM; j++)
          if (handles[j] == handles[i])
            handles[j] = 0;

        unrefHandle(handles[i]);
        handles[i] = 0;
    }

    return ret;
}

