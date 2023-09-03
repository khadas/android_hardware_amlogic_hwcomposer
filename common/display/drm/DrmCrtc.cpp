/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <MesonLog.h>
#include <string.h>
#include <inttypes.h>
#include <DebugHelper.h>

#include <xf86drm.h>
#include "DrmDevice.h"
#include "DrmCrtc.h"
#include "DrmConnector.h"
#include "misc.h"
#include "DrmPlane.h"

/* emulation fb0 of drm free fb memory sysfs node*/
#define DISPLAY_FB0_FREE_FB_MEM_DRM    "/sys/class/graphics/fb0/force_free_mem"

DrmCrtc::DrmCrtc(int drmFd, drmModeCrtcPtr p, uint32_t pipe)
    : HwDisplayCrtc(),
    mDrmFd(drmFd),
    mId(p->crtc_id),
    mPipe(pipe),
    mReq(NULL),
    mLogoClosed(false) {
    loadProperties();
    MESON_ASSERT(p->mode_valid == mActive->getValue(), "valid mode info mismatch.");
    initConversionCaps();

    if (p->mode_valid) {
        memcpy(&mDrmMode, &p->mode, sizeof(drmModeModeInfo));
    } else {
        memset(&mDrmMode, 0, sizeof(drmModeModeInfo));
    }

    mConnectorId = 0;
    memset(&mMesonMode, 0, sizeof(mMesonMode));

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
        {DRM_CRTC_PROP_VRR_ENABLED, &mVrrEnabled},
        {DRM_CRTC_PROP_VIDEO_PIXEL_FORMAT, &mVideoPixelFormat},
        {DRM_CRTC_PROP_OSD_PIXEL_FORMAT, &mOsdPixelFormat},
        {DRM_CRTC_PROP_HDR_CONVERSION_CAP, &mHdrConversionCaps},
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
    /*mode set from drmcrtc, we don't need update state.*/
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

    MESON_LOGV("Crtc [%d] getmode %" PRIu64 ":[%dx%d-%f].",
        mId, mModeBlobId->getValue(), mode.pixelW, mode.pixelH, mode.refreshRate);
    return 0;
}

/* bit0: SDR->HDR10 */
/* bit1: SDR->HLG */
/* bit2: HDR10->SDR */
/* bit3: HDR10->HLG */
/* bit4: HLG->SDR */
/* bit5: HLG->HDR10 */
/* bit6: HDR10_PLUS->HDR10 */
/* bit7: HDR10_PLUS->SDR */
/* bit8: HDR10_PLUS->HLG */
/* bit9: CUVA_HDR->SDR */
/* bit10: CUVA_HDR->HDR10 */
/* bit11: CUVA_HDR->HLG */
/* bit12: CUVA_HLG->SDR */
/* bit13: CUVA_HLG->HDR10 */
/* bit14: CUVA_HLG->HLG */
/* bit15: DOLBY_VISION->SDR */
/* bit16: SDR->DOLBY_VISION */
/* bit17: DOLBY_VISION->HDR10*/
/* bit18: HDR10->DOLBY_VISION */
/* bit19: HLG->DOLBY_VISION */
int32_t DrmCrtc::initConversionCaps() {
    mDrmHdrConversionCaps.clear();
    if (mHdrConversionCaps) {
        uint32_t value = mHdrConversionCaps->getValue();
        MESON_LOGD("%s mHdrCoversionCaps:0x%x",__func__, value);
        if (value & (1 << 0))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_HDR10, 0});
        if (value & (1 << 1))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_HLG, 0});
        if (value & (1 << 2))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_INVALID, 0});
        if (value & (1 << 3))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_HLG, 0});
        if (value & (1 << 4))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_INVALID, 0});
        if (value & (1 << 5))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_HDR10, 0});
        if (value & (1 << 6))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_HDR10, 0});
        if (value & (1 << 7))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_INVALID, 0});
        if (value & (1 << 8))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_HLG, 0});
        if (value & (1 << 15))
            mDrmHdrConversionCaps.push_back({DRM_DOLBY_VISION, DRM_INVALID, 0});
        if (value & (1 << 16))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_DOLBY_VISION, 0});
        if (value & (1 << 17))
            mDrmHdrConversionCaps.push_back({DRM_DOLBY_VISION, DRM_HDR10, 0});
        if (value & (1 << 18))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_DOLBY_VISION, 0});
        if (value & (1 << 19))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_DOLBY_VISION, 0});
    }
    return 0;
}

int32_t DrmCrtc::getConversionCaps(std::vector<drm_hdr_conversion_capability>& hdrconversionCaps) {
    hdrconversionCaps.assign(mDrmHdrConversionCaps.begin(), mDrmHdrConversionCaps.end());
    return 0;
}

int32_t DrmCrtc::getPixelFormats(std::vector<uint32_t>& pixelFormats) {
    if (mVideoPixelFormat && mOsdPixelFormat) {
        uint32_t value = -1;
        value = mOsdPixelFormat->getValue();
        if (value & (1 << 0))
            pixelFormats.emplace_back(DRM_RGBA_8888);
        if (value & (1 << 1))
            pixelFormats.emplace_back(DRM_RGBX_8888);
        if (value & (1 << 2))
            pixelFormats.emplace_back(DRM_RGB_888);
        if (value & (1 << 3))
            pixelFormats.emplace_back(DRM_RGB_565);
        if (value & (1 << 4))
            pixelFormats.emplace_back(DRM_BGRA_8888);

        value = mVideoPixelFormat->getValue();
        if (value & (1 << 0))
            pixelFormats.emplace_back(DRM_YCRCB_420_SP);
        if (value & (1 << 1))
            pixelFormats.emplace_back(DRM_YCBCR_422_I);
        return 0;
    }
    return -EINVAL;
}

int32_t DrmCrtc::setMode(drm_mode_info_t & mode, bool seamless) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    return setModeLocked(mode, seamless);
}

int32_t DrmCrtc::setModeLocked(drm_mode_info_t & mode, bool seamless __unused) {
    ATRACE_CALL();
    int ret;
    std::shared_ptr<DrmProperty> crtcid;
    std::shared_ptr<DrmProperty> modeid;
    std::shared_ptr<DrmProperty> updateprop;
    uint32_t modeBlob;

    auto connectorIt = getDrmDevice()->getConnectorById(mConnectorId);
    DrmConnector * connector = (DrmConnector *)connectorIt.get();

    // get the lock of DrmConnector as it may update when crtc set mode
    std::lock_guard<std::mutex> connectorLock(connector->mMutex);

    connector->getCrtcProp(crtcid);

    /* If in hotplug process, set mode to PendingModes */
    if (getHotplugStatus() == HotplugStatus::InHotplugProcess) {
        MESON_LOGD("connector (%s) setMode %s to pendingMode", connector->getName(), mode.name);
        mPendingModes.push_back(mode);
        return 0;
    }

    modeBlob = connector->getModeBlobId(mode);
    connector->getUpdateProp(updateprop);

    if (modeBlob == 0) {
        MESON_LOGE("Mode invalid for current pipe [%s]", mode.name);
        return -EINVAL;
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

    connector->getDrmModeByBlobId(mDrmMode, modeBlob);
    if (!mLogoClosed) {
        DrmPlane *plane = (DrmPlane *) getDrmDevice()->getPrimaryPlane(mPipe);
        if (plane)
            plane->setCrtcProps(req, mDrmMode);
    }

    /*set prop value*/
    MESON_ASSERT(crtcid->getValue() == mId, "crtc/connector NOT bind?!");
    mActive->setValue(1);
    mModeBlobId->setValue(modeBlob);
    /*already update when apply*/
    crtcid->apply(req);
    mActive->apply(req);
    mModeBlobId->apply(req);

    int enableVrr = 0;
    // Currently always enable vrr, let drm has chance to
    // select BRR mode if connector support it.
    // TODO: support disable it when have UI switch
    // TODO: remove single display limit when dual display support boot conifg
    if (connector->supportVrr() && HWC_DISPLAY_NUM == 1)
        enableVrr = 1;

    mVrrEnabled->setValue(enableVrr);
    mVrrEnabled->apply(req);

    ret = drmModeAtomicCommit(
        mDrmFd,
        req,
        DRM_MODE_ATOMIC_ALLOW_MODESET,
        NULL);
    if (ret) {
        MESON_LOGE("set Mode failed  ret (%d)", ret);
    }

    drmModeAtomicFree(req);

    connector->DrmMode2Mode(mDrmMode, mMesonMode);
    MESON_LOGD("setmode:crtc[%d], name [%s] -modeblob[%d]"
            " [%dx%d-%.2f] seamless:%d enableVrr:%d",
            mId, mode.name, modeBlob,
            mMesonMode.pixelW, mMesonMode.pixelH,
            mMesonMode.refreshRate,
            seamless, enableVrr);

    return ret;
}

int32_t DrmCrtc::waitVBlank(nsecs_t & timestamp) {
    uint32_t reqType = DRM_VBLANK_RELATIVE | (mPipe == 0 ? 0 : (1 << mPipe));

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
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReq == NULL)
        mReq = drmModeAtomicAlloc();

    return mReq;
}

int32_t DrmCrtc::prePageFlip() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReq) {
        MESON_LOGE("still have a req? previous display didnot finish?");
        drmModeAtomicFree(mReq);
        mReq = NULL;
    }

    return 0;
}

int32_t DrmCrtc::atomicClearMode() {
    std::shared_ptr<DrmProperty> crtcid;
    drmModeAtomicReqPtr req = drmModeAtomicAlloc();
    auto connectorIt = getDrmDevice()->getConnectorById(mConnectorId);
    DrmConnector * connector = (DrmConnector *)connectorIt.get();
    connector->getCrtcProp(crtcid);

    mActive->setValue(0);
    crtcid->setValue(0);
    mModeBlobId->setValue(0);
    crtcid->apply(req);
    mActive->apply(req);
    mModeBlobId->apply(req);

    uint32_t flag = DRM_MODE_ATOMIC_ALLOW_MODESET;
    int32_t ret = drmModeAtomicCommit(
        mDrmFd,
        req,
        flag,
        NULL);
    if (ret) {
        MESON_LOGE("atomicClearMode-%d:atomic commit ret (%d)", getId(), ret);
    }
    drmModeAtomicFree(req);
    req = NULL;

    return ret;

}

int32_t DrmCrtc::updatePropertyValue() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);

    //MESON_ASSERT(mReq!= NULL, "updatePropertyValue  with NULL request.");
    if (!mReq) {
        ALOGD("updatePropertyValue with NULL request");
        return 0;
    }

    uint32_t flag = DRM_MODE_ATOMIC_NONBLOCK;
    int32_t ret = drmModeAtomicCommit(
        mDrmFd,
        mReq,
        flag,
        NULL);
    if (ret) {
        MESON_LOGE("updatePropertyValue-%d:atomic commit ret (%d)", getId(), ret);
    }

    drmModeAtomicFree(mReq);
    mReq = NULL;

    return ret;
}

int32_t DrmCrtc::pageFlip(int32_t & out_fence) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    if (mActive->getValue() == 0) {
        out_fence = -1;
        return 0;
    }

    if (!mReq) {
        drm_meson_present_fence_t presentFence;
        presentFence.crtc_idx = mPipe;
        auto ret = ioctl(mDrmFd, DRM_IOCTL_MESON_CREAT_PRESENT_FENCE, &presentFence);
        if (ret) {
            MESON_LOGE("Crtc ioctl get presentFence failed ret=%d", ret);
            out_fence = -1;
        } else {
            out_fence = presentFence.fd;
        }
        return 0;
    }

    MESON_ASSERT(mReq!= NULL, "pageFlip  with NULL request.");
    out_fence = -1;
    drmModeAtomicAddProperty(mReq, mId, mOutFencePtr->getId(), (uint64_t)&out_fence);

    uint32_t flag = DRM_MODE_ATOMIC_NONBLOCK;
    if (DebugHelper::getInstance().enableDrmBlockMode())
        flag = 0;

    ATRACE_BEGIN("AtomicCommit");
    int32_t ret = drmModeAtomicCommit(
        mDrmFd,
        mReq,
        flag,
        NULL);
    if (ret) {
        MESON_LOGE("pageFlip-%d:atomic commit ret (%d)", getId(), ret);
    }
    ATRACE_END();

    drmModeAtomicFree(mReq);
    mReq = NULL;

    /* discard out fence */
    if (DebugHelper::getInstance().discardOutFence()) {
        std::shared_ptr<DrmFence> outfence =
            std::make_shared<DrmFence>(out_fence);
        outfence->waitForever("crtc-output");
        out_fence = -1;
    }

    return ret;
}

int DrmCrtc::setConnectorId(uint32_t connectorId) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConnectorId = connectorId;
    return 0;
}

int32_t DrmCrtc::setPendingMode() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mPendingModes.empty()) {
        MESON_LOGD("[%s] pending modes vector empty", __func__);
        return 0;
    }

    drm_mode_info_t mode = mPendingModes.back();
    MESON_LOGD("[%s] mode %s", __func__, mode.name);
    setModeLocked(mode);

    mPendingModes.clear();
    return 0;
}


void DrmCrtc::closeLogoDisplay() {
    mLogoClosed = true;
    /* free fb0 memory if it used */
    sysfs_set_string(DISPLAY_FB0_FREE_FB_MEM_DRM, "1");
}

void DrmCrtc::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Crtc mPipeId(%d) - mId(%d):\n", mPipe, mId);
    dumpstr.appendFormat("\t Active (%" PRIu64 "), ModeId (%" PRIu64 ") mMode (%s)\n",
                        mActive->getValue(), mModeBlobId->getValue(),
                        mDrmMode.name);

    auto connector = getDrmDevice()->getConnectorById(mConnectorId);
    const char *name = connector ? connector->getName() : "invalid";
    dumpstr.appendFormat("\t mConnectorId (%d)-(%s) \n",
                        mConnectorId,
                        mConnectorId == 0 ? "noConnector" : name);
}

