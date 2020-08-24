/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>

#include "DrmPlane.h"

DrmPlane::DrmPlane(drmModePlanePtr p)
    : HwDisplayPlane(),
    mId(p->plane_id),
    mCrtcMask(p->possible_crtcs),
    mFormatCnt(p->count_formats) {
    MESON_ASSERT(mFormatCnt, "format should > 0");
    mFormats = new uint32_t[mFormatCnt];
    memcpy(mFormats, p->formats, sizeof(uint32_t) * p->count_formats);
}

DrmPlane::~DrmPlane() {
}

const char * DrmPlane::getName() {
    const char * name;
    switch (mType) {
        case DRM_PLANE_TYPE_PRIMARY:
            name = "osd-primary";
            break;
        case DRM_PLANE_TYPE_OVERLAY:
            name = "osd-overlay";
            break;
        case DRM_PLANE_TYPE_CURSOR:
            name = "cursor";
            break;
        default:
            name = "unknown-drm-plane";
            break;

    };
    return NULL;
}

uint32_t DrmPlane::getId() {
    return mId;
}

uint32_t DrmPlane::getType() {
    switch (mType) {
        case DRM_PLANE_TYPE_PRIMARY:
        case DRM_PLANE_TYPE_OVERLAY:
            return OSD_PLANE;
            break;
        case DRM_PLANE_TYPE_CURSOR:
            return CURSOR_PLANE;
        default:
            return INVALID_PLANE;
    };
}

uint32_t DrmPlane::getCapabilities() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmPlane::getFixedZorder() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

uint32_t DrmPlane::getPossibleCrtcs() {
    return mCrtcMask;
}

bool DrmPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    /*check scanout buffer*/
    switch (fb->mFbType) {
        case DRM_FB_CURSOR:
            if (!am_gralloc_is_coherent_buffer(fb->mBufferHandle))
                return false;
            break;
        case DRM_FB_SCANOUT:
            break;
        default:
            return false;
    }

    /*check format*/
    int format = am_gralloc_get_format(fb->mBufferHandle);
    if (!validateFormat(format))
        return false;

    /*check vpu limit: blend mode*/


    /*check vpu limit: buffer size*/


    return true;
}

int32_t DrmPlane::setPlane(std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder, int blankOp) {
    UNUSED(fb);
    UNUSED(zorder);
    UNUSED(blankOp);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void DrmPlane::setDebugFlag(int dbgFlag) {
    UNUSED(dbgFlag);
    MESON_LOG_EMPTY_FUN();
}

bool DrmPlane::validateFormat(uint32_t format) {
    for (int i = 0; i < mFormatCnt; i ++) {
        if (format == mFormats[i])
            return true;
    }

    return false;
}


void DrmPlane::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
    MESON_LOG_EMPTY_FUN();
}


