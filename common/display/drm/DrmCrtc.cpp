/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <string.h>
#include <inttypes.h>

#include <xf86drm.h>
#include "DrmDevice.h"
#include "DrmCrtc.h"

DrmCrtc::DrmCrtc(int drmFd, drmModeCrtcPtr p, uint32_t pipe)
    : HwDisplayCrtc(),
    mDrmFd(drmFd),
    mId(p->crtc_id),
    mPipe(pipe),
    mReq(NULL) {
    loadProperties();
    MESON_ASSERT(p->mode_valid == mActive->getValue(), "valid mode info mismatch.");

    if (p->mode_valid) {
        mMode = p->mode;
    } else {
        memset(&mMode, 0, sizeof(drmModeModeInfo));
    }

    MESON_LOGD("DrmCrtc init pipe(%d)-id(%d), mode (%s),active(%" PRId64 ")",
        mPipe, mId, mMode.name, mActive->getValue());
}

DrmCrtc::~DrmCrtc() {
}

int32_t DrmCrtc::loadProperties() {
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } crtcProps[] = {
        {DRM_CRTC_PROP_ACTIVE, &mActive},
        {DRM_CRTC_PROP_MODEID, &mModeBlobId},
        {DRM_CRTC_PROP_OUTFENCEPTR, &mOutFencePtr},
    };
    const int crtcPropsNum = sizeof(crtcProps)/sizeof(crtcProps[0]);
    int initedProps = 0;

    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(mDrmFd, mId, DRM_MODE_OBJECT_CRTC);
    MESON_ASSERT(props != NULL, "DrmCrtc::loadProperties failed.");

    for (int i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(mDrmFd, props->props[i]);
        for (int j = 0; j < crtcPropsNum; j++) {
            if (strcmp(prop->name, crtcProps[j].propname) == 0) {
                *(crtcProps[j].drmprop) =
                    std::make_shared<DrmProperty>(prop, mId, props->prop_values[i]);
                initedProps ++;
                break;
            }
        }
       drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    MESON_ASSERT(crtcPropsNum == initedProps, "NOT ALL CRTC PROPS INITED.");
    return 0;
}

int32_t DrmCrtc::getId() {
    return mId;
}

uint32_t DrmCrtc::getPipe() {
    return mPipe;
}

int32_t DrmCrtc::update() {
    std::lock_guard<std::mutex> lock(mMutex);

    MESON_LOGE("DrmCrtc::update nothing to do.");
    return 0;
}

int32_t DrmCrtc::getMode(drm_mode_info_t & mode) {
    if (mActive->getValue() == 0) {
        MESON_LOGE("Crtc [%d] getmode for inactive.", mId);
        return -EFAULT;
    }

    strncpy(mode.name, mMode.name, DRM_DISPLAY_MODE_LEN);
    mode.refreshRate = mMode.vrefresh;
    mode.pixelW = mMode.vdisplay;
    mode.pixelH = mMode.hdisplay;
    mode.dpiX = mode.dpiY = 160;
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::setMode(drm_mode_info_t & mode) {
    UNUSED(mode);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::waitVBlank(nsecs_t & timestamp) {
    static uint32_t reqType = DRM_VBLANK_RELATIVE | (mPipe == 0 ? 0 : (1 << mPipe));

    drmVBlank vbl;
    vbl.request.type = (drmVBlankSeqType)reqType;
    vbl.request.sequence = 1;
    int32_t ret = drmWaitVBlank(mDrmFd, &vbl);
    if (!ret) {
        timestamp = vbl.reply.tval_sec * 1000000000LL + vbl.reply.tval_usec *1000LL;
    } else {
        MESON_LOGE("waitVBlank failed crtc[%d]ret[%d]", mId, ret);
    }
    return ret;
}

int32_t DrmCrtc::prePageFlip() {
    if (mReq) {
        MESON_LOGE("still have a req? previous display didnot finish?");
        drmModeAtomicFree(mReq);
    }

    mReq = drmModeAtomicAlloc();
    return 0;
}

int32_t DrmCrtc::pageFlip(int32_t & out_fence) {
    MESON_ASSERT(mReq!= NULL, "pageFlip  with NULL request.");
    out_fence = -1;
    drmModeAtomicAddProperty(mReq, mId, mOutFencePtr->getId(), (uint64_t)&out_fence);

    int32_t ret = drmModeAtomicCommit(
        mDrmFd,
        mReq,
        DRM_MODE_ATOMIC_ALLOW_MODESET,
        NULL);
    if (ret) {
        MESON_LOGE("atomic commit ret (%d)", ret);
    }

    drmModeAtomicFree(mReq);
    mReq = NULL;
    return ret;
}
