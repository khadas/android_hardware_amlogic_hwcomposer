/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <MesonLog.h>
#include <string.h>
#include <inttypes.h>

#include <xf86drm.h>
#include "DrmDevice.h"
#include "DrmCrtc.h"
#include "DrmConnector.h"

DrmCrtc::DrmCrtc(int drmFd, drmModeCrtcPtr p, uint32_t pipe)
    : HwDisplayCrtc(),
    mDrmFd(drmFd),
    mId(p->crtc_id),
    mPipe(pipe),
    mReq(NULL) {
    loadProperties();
    MESON_ASSERT(p->mode_valid == mActive->getValue(), "valid mode info mismatch.");

    if (p->mode_valid) {
        memcpy(&mDrmMode, &p->mode, sizeof(drmModeModeInfo));
    } else {
        memset(&mDrmMode, 0, sizeof(drmModeModeInfo));
    }

    mConnectorId = 0;
    memset(&mMesonMode, 0, sizeof(mMesonMode));

    mNeedPageFlip = false;

    MESON_LOGD("DrmCrtc init pipe(%d)-id(%d), mode (%s),active(%" PRId64 ")",
        mPipe, mId, mDrmMode.name, mActive->getValue());
}

DrmCrtc::~DrmCrtc() {
}

int32_t DrmCrtc::loadProperties() {
    ATRACE_CALL();
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
    /*mode set from drmcrtc, we dont need update state.*/
    MESON_LOG_EMPTY_FUN ();
    return 0;
}

int32_t DrmCrtc::readCurDisplayMode(std::string & dispmode) {
    drm_mode_info_t curmode;
    int32_t ret = getMode(curmode);
    if (ret == 0)
        dispmode = curmode.name;
    else
        dispmode = "INVALID";

    return ret;
}

int32_t DrmCrtc::getMode(drm_mode_info_t & mode) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mActive->getValue() == 0 || mConnectorId == 0) {
        MESON_LOGE("Crtc [%d] getmode for inactive or not bind. connectorId %d",
                mId, mConnectorId);
        return -EFAULT;
    }

    if (mDrmMode.name[0] == 0) {
        MESON_LOGE("Invalid drmmode , return .");
        return -EINVAL;
    }

    if (mMesonMode.name[0] == 0) {
        auto connectorIt = getDrmDevice()->getConnectorById(mConnectorId);
        DrmConnector * connector = (DrmConnector *)connectorIt.get();
        connector->DrmMode2Mode(mDrmMode, mMesonMode);
    }

    mode = mMesonMode;

    /*
     * If we are in hotplug process, let systemcontrol think the display mode
     * is that it just seted. PendingModes will be set when hotplug process finished
     */
    if (getHotplugStatus() == HotplugStatus::InHotplugProcess) {
        if (!mPendingModes.empty())
            mode = mPendingModes.back();
    }

    MESON_LOGD("Crtc [%d] getmode %llu:[%dx%d-%f].",
        mId, mModeBlobId->getValue(), mode.pixelW, mode.pixelH, mode.refreshRate);
    return 0;
}

int32_t DrmCrtc::setMode(drm_mode_info_t & mode) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    int ret;
    std::shared_ptr<DrmProperty> crtcid;
    std::shared_ptr<DrmProperty> modeid;
    std::shared_ptr<DrmProperty> updateprop;
    uint32_t modeBlob;

    auto connectorIt = getDrmDevice()->getConnectorById(mConnectorId);
    DrmConnector * connector = (DrmConnector *)connectorIt.get();
    connector->getCrtcProp(crtcid);

    modeBlob = connector->getModeBlobId(mode);
    connector->getUpdateProp(updateprop);

    if (modeBlob == 0) {
        MESON_LOGE("Mode invalid for current pipe [%s]", mode.name);
        return -EINVAL;
    }

    /* If in hotplug process, set mode to PendingModes */
    if (getHotplugStatus() == HotplugStatus::InHotplugProcess) {
        MESON_LOGD("connector (%s) setMode %s to pendingMode", connector->getName(), mode.name);
        mPendingModes.push_back(mode);
        return 0;
    }

    drmModeAtomicReqPtr req = drmModeAtomicAlloc();

    /*TODO: update mModeBlobId        and compare id.*/
    if (strncmp(mDrmMode.name, mode.name, DRM_DISPLAY_MODE_LEN) == 0) {
        if (updateprop) {
            MESON_LOGD("Set update flag");
            updateprop->setValue(1);
            updateprop->apply(req);
        }
    }

    /*set prop value*/
    MESON_ASSERT(crtcid->getValue() == mId, "crtc/connector NOT bind?!");
    mActive->setValue(1);
    mModeBlobId->setValue(modeBlob);
    /*already update when apply*/
    crtcid->apply(req);
    mActive->apply(req);
    mModeBlobId->apply(req);

    ret = drmModeAtomicCommit(
        mDrmFd,
        req,
        DRM_MODE_ATOMIC_ALLOW_MODESET,
        NULL);
    if (ret) {
        MESON_LOGE("set Mode failed  ret (%d)", ret);
    }

    drmModeAtomicFree(req);

    connector->getDrmModeByBlobId(mDrmMode, modeBlob);
    connector->DrmMode2Mode(mDrmMode, mMesonMode);
    MESON_LOGD("setmode:crtc[%d], name [%s] -modeblob[%d]",
        mId, mode.name, modeBlob);
    return ret;
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

drmModeAtomicReqPtr DrmCrtc::getAtomicReq() {
    if (mReq == NULL) {
        MESON_LOGD("No req, create a new one");
        mReq = drmModeAtomicAlloc();
    }

    return mReq;
}

int32_t DrmCrtc::prePageFlip() {
    ATRACE_CALL();
    if (mReq) {
        MESON_LOGE("still have a req? previous display didnot finish?");
        drmModeAtomicFree(mReq);
    }

    mReq = drmModeAtomicAlloc();
    return 0;
}

int32_t DrmCrtc::pageFlip(int32_t & out_fence) {
    ATRACE_CALL();
    if (mActive->getValue() == 0) {
        out_fence = -1;
        return 0;
    }

    if (!mNeedPageFlip) {
        drmModeAtomicFree(mReq);
        mReq = NULL;
        out_fence = -1;
        return 0;
    }

    MESON_ASSERT(mReq!= NULL, "pageFlip  with NULL request.");
    out_fence = -1;
    drmModeAtomicAddProperty(mReq, mId, mOutFencePtr->getId(), (uint64_t)&out_fence);

    int32_t ret = drmModeAtomicCommit(
        mDrmFd,
        mReq,
        DRM_MODE_ATOMIC_NONBLOCK,
        NULL);
    if (ret) {
        MESON_LOGE("pageFlip:atomic commit ret (%d)", ret);
    }

    drmModeAtomicFree(mReq);
    mReq = NULL;
    mNeedPageFlip = false;
    return ret;
}

int DrmCrtc::setConnectorId(uint32_t connectorId) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConnectorId = connectorId;
    return 0;
}

int32_t DrmCrtc::setPendingMode() {
    if (mPendingModes.empty()) {
        MESON_LOGD("[%s] pending modes vector empty", __func__);
        return 0;
    }

    drm_mode_info_t mode = mPendingModes.back();
    MESON_LOGD("[%s] mode %s", __func__, mode.name);
    setMode(mode);

    mPendingModes.clear();
    return 0;
}

void DrmCrtc::setCrtcPageUpdateStatus(bool status) {
    mNeedPageFlip = status;
}

void DrmCrtc::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Crtc mPipeId(%d) - mId(%d):\n", mPipe, mId);
    dumpstr.appendFormat("\t Active (%llu), ModeId (%llu) mMode (%s)\n",
                        mActive->getValue(), mModeBlobId->getValue(),
                        mDrmMode.name);

    auto connector = getDrmDevice()->getConnectorById(mConnectorId);
    const char *name = connector ? connector->getName() : "invalid";
    dumpstr.appendFormat("\t mConnectorId (%d)-(%s) \n",
                        mConnectorId,
                        mConnectorId == 0 ? "noConnector" : name);
}

