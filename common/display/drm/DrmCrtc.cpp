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

#include "DrmDevice.h"
#include "DrmCrtc.h"

DrmCrtc::DrmCrtc(drmModeCrtcPtr p, uint32_t pipe)
    : HwDisplayCrtc(),
    mId(p->crtc_id),
    mPipe(pipe),
    mModeValid(p->mode_valid) {

    if (mModeValid) {
        mMode = p->mode;
    } else {
        memset(&mMode, 0, sizeof(drmModeModeInfo));
    }

    loadProperties();

    MESON_LOGD("DrmCrtc init pipe(%d)-id(%d), mode (%s),active(%lld)",
        mPipe, mId, mMode.name, mActive->getValue());
}

DrmCrtc::~DrmCrtc() {
    mConnector.reset();
}

int32_t DrmCrtc::loadProperties() {
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } crtcProps[] = {
        {DRM_CRTC_PROP_ACTIVE, &mActive},
        {DRM_CRTC_PROP_MODEID, &mModeBlobId},
        {DRM_CRTC_PROP_OUTFENCE, &mOutFence},
    };
    const int crtcPropsNum = sizeof(crtcProps)/sizeof(crtcProps[0]);
    int initedProps = 0;

    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(getDrmDevice()->getDeviceFd(), mId, DRM_MODE_OBJECT_CRTC);
    MESON_ASSERT(props != NULL, "DrmCrtc::loadProperties failed.");

    for (int i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(getDrmDevice()->getDeviceFd(), props->props[i]);
        for (int j = 0; j < crtcPropsNum; j++) {
            if (strcmp(prop->name, crtcProps[j].propname) == 0) {
                *(crtcProps[j].drmprop) =
                    std::make_shared<DrmProperty>(prop, props->prop_values[i]);
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
    MESON_ASSERT(mConnector, "Crtc need setuped before load Properities.");


    MESON_LOGE("DrmCrtc::update nothing to do.");
    return 0;
}

int32_t DrmCrtc::getMode(drm_mode_info_t & mode) {
    if (!mModeValid) {
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
    UNUSED(timestamp);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmCrtc::pageFlip(int32_t & out_fence) {
    UNUSED(out_fence);
    MESON_LOG_EMPTY_FUN();
    return 0;
}
