/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define MAX_FRAME_DELAY 10

#include <utils/Trace.h>
#include <hardware/hwcomposer2.h>
#include <inttypes.h>
#include <time.h>
#include <thread>

#include "hwcomposer3.h"
#include "Hwc2Display.h"
#include "Hwc2Layer.h"
#include "Hwc2Base.h"
#include "VtDisplayThread.h"
#include "WBDisplayThread.h"
#include "mode_ubootenv.h"

/*For round corner*/
#include <png.h>
#include <zlib.h>
#include <misc.h>
#include <sys/mman.h>

#include <DrmTypes.h>
#include <HwcConfig.h>
#include <MesonLog.h>
#include <DebugHelper.h>
#include <Composition.h>
#include <IComposer.h>
#include <ComposerFactory.h>
#include <CompositionStrategyFactory.h>
#include <EventThread.h>
#include <systemcontrol.h>
#include <am_gralloc_ext.h>
#include <HwDisplayManager.h>
#include <misc.h>
#include <UvmDev.h>
#include <AmVecmDev.h>
#include <DrmTypes.h>

Hwc2Display::Hwc2Display(std::shared_ptr<Hwc2DisplayObserver> observer, uint32_t display) {
    mObserver = observer;
    mForceClientComposer = false;
    mPowerMode  = std::make_shared<HwcPowerMode>();
    mSignalHpd = false;
    mValidateDisplay = false;
    mVsyncState = false;
    mScaleValue = 1;
    mPresentFence = -1;
    mVtDisplayThread = nullptr;
    mWBDisplayThread = nullptr;
    mVsyncTimestamp = 0;
    mFirstPresent = true;
    mDisplayId = display;
    mFRPeriodNanos = 0;
    mDisableSideband = false;
    memset(&mHdrCaps, 0, sizeof(mHdrCaps));
    memset(mColorMatrix, 0, sizeof(float) * 16);
    memset(&mCalibrateCoordinates, 0, sizeof(int) * 4);
    // for self-adaptive
    mVideoLayerRegion = 0;
    mHasVideoPresent = false;
    mModeChanged = false;
    mFailedDeviceComp = false;
    mLayerSeq = 0;
    mSkipComposition = false;
    mConfirmSkip = false;
    mProcessorFlags = 0;
    mVtVsyncStatus = false;
    mOutsideChanged = false;
    mBootConfig = -1;
    wbHnd = nullptr;
    mExpectedPresentTime = -1;
    mIsDisablePostProcessor = false;
    memset(&mDisplayMode, 0, sizeof(mDisplayMode));
    memset(&mCalibrateInfo, 0, sizeof(mCalibrateInfo));
}

Hwc2Display::~Hwc2Display() {
    mLayers.clear();
    mPlanes.clear();
    mComposers.clear();

    mCrtc.reset();
    mConnector.reset();
    mObserver.reset();
    mCompositionStrategy.reset();
    mPresentCompositionStg.reset();

    mVsync.reset();
    mModeMgr.reset();

    if (mVtDisplayThread) {
        mVtDisplayThread.reset();
    }
    mVtVsync.reset();

    if (mWBDisplayThread) {
        mWBDisplayThread.reset();
    }
    mWBVsync.reset();

    if (mPostProcessor != NULL)
        mPostProcessor->stop();
    mPostProcessor.reset();
}

void Hwc2Display::handleWBThread() {
    if (mWhiteBoardMode || mEnableCallBack) {
        if (!mWBDisplayThread) {
            mWBDisplayThread = std::make_shared<WBDisplayThread>(this);
        }
    }
}

void Hwc2Display::setKeystoneCorrection(std::string params) {
    mKeystoneConfigs = params;
    mObserver->refresh();
    return;
}

int32_t Hwc2Display::setModeMgr(std::shared_ptr<HwcModeMgr> & mgr) {
    MESON_LOG_FUN_ENTER();
    std::lock_guard<std::mutex> lock(mMutex);
    mModeMgr = mgr;

    if (mModeMgr->getDisplayMode(mDisplayMode) == 0) {
        uint32_t fbW,fbH;
        HwcConfig::getFramebufferSize(0, fbW, fbH);
        mScaleValue = (float)fbW/(float)mDisplayMode.pixelW;
        mPowerMode->setConnectorStatus(true);
    }
    MESON_LOG_FUN_LEAVE();
    return 0;
}

int32_t Hwc2Display::initialize() {
    MESON_LOG_FUN_ENTER();
    std::lock_guard<std::mutex> lock(mMutex);

    /*add valid composers*/
    std::shared_ptr<IComposer> composer;
    ComposerFactory::create(MESON_CLIENT_COMPOSER, composer, mDisplayId);
    mComposers.emplace(MESON_CLIENT_COMPOSER, std::move(composer));
    ComposerFactory::create(MESON_DUMMY_COMPOSER, composer, mDisplayId);
    mComposers.emplace(MESON_DUMMY_COMPOSER, std::move(composer));
    /*add yuv/video composer*/
    ComposerFactory::create(MESON_DI_COMPOSER, composer, mDisplayId);
    mComposers.emplace(MESON_DI_COMPOSER, std::move(composer));

    initLayerIdGenerator();

    MESON_LOG_FUN_LEAVE();
    return 0;
}

void Hwc2Display::enableSyncProtection(bool mode) {
    ATRACE_CALL();
    if (mWhiteBoardMode && mCompositionStrategy) {
        mCompositionStrategy->enableSyncProtection(mode);
    }
    return;
}

int32_t Hwc2Display::setDisplayResource(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    MESON_LOG_FUN_ENTER();
    std::lock_guard<std::mutex> lock(mMutex);
    /* need hold vtmutex as it may update compositionStragegy */
    std::lock_guard<std::mutex> vtLock(mVtMutex);

    mCrtc = crtc;
    mPlanes = planes;
    mConnector = connector;

    /*update composition strategy.*/
    uint32_t strategyFlags = 0;
    int osdPlanes = 0;
    bool videoPlanesSupportHighResolution = false;
    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        if ((*it)->getType() == OSD_PLANE) {
            osdPlanes ++;
        }

        if ((*it)->getType() == HWC_VIDEO_PLANE &&
            ((*it)->getCapabilities() & PLANE_SUPPORT_8K) == PLANE_SUPPORT_8K) {
            videoPlanesSupportHighResolution = true;
        }

        if (osdPlanes > 1 && (it == mPlanes.end() - 1 )) {
            if (videoPlanesSupportHighResolution)
                strategyFlags |= MULTI_PLANES_WITH_HR;
            else
                strategyFlags |= MULTI_PLANES_WITH_DI;
        }

        (*it)->setDisplayMode(mDisplayMode);
    }
    MESON_ASSERT(osdPlanes > 0, "No Osd plane assigned to %d", mDisplayId);

    auto newCompositionStrategy =
        CompositionStrategyFactory::create(SIMPLE_STRATEGY, strategyFlags);
    if (newCompositionStrategy != mCompositionStrategy) {
        MESON_LOGD("Update composition %s -> %s",
            mCompositionStrategy != NULL ? mCompositionStrategy->getName() : "NULL",
            newCompositionStrategy->getName());
        mCompositionStrategy = newCompositionStrategy;
    }

    mConnector->getHdrCapabilities(&mHdrCaps);
    mConnector->getSupportedContentTypes(mSupportedContentTypes);
    mCrtc->getHdrMetadataKeys(mHdrKeys);

    if (mModePolicy.get())
        mModePolicy->bindConnector(mConnector);
    MESON_LOG_FUN_LEAVE();
    return 0;
}

int32_t Hwc2Display::setPostProcessor(
    std::shared_ptr<HwcPostProcessor> processor) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPostProcessor = processor;
    mProcessorFlags = 0;
    return 0;
}

int32_t Hwc2Display::setVsync(std::shared_ptr<HwcVsync> vsync) {
    std::lock_guard<std::mutex> lock(mMutex);
    int vsyncType = vsync->getVsyncType();
    switch (vsyncType) {
        case DISPLAY_DEFAULT:
            if (mVsync != vsync) {
                if (mVsync.get()) {
                    mVsync->setEnabled(false);
                    mVsync->setObserver(NULL);
                } else {
                    mVsync = vsync;
                    mVsync->setObserver(this);
                    mVsync->setEnabled(mVsyncState);
                }
            }
            break;
        case DISPLAY_VIDEOTUNNEL:
            if (mVtVsync != vsync) {
                if (mVtVsync.get()) {
                    mVtVsync->setObserver(NULL);
                } else {
                    mVtVsync = vsync;
                    mVtVsync->setObserver(this);
                }
            }
            break;
        case DISPLAY_WHITEBOARD:
            if (mWBVsync != vsync) {
                if (mWBVsync.get()) {
                    mWBVsync->setObserver(NULL);
                } else {
                    mWBVsync = vsync;
                    mWBVsync->setObserver(this);
                }
            }
            break;
        default:
            MESON_LOGE("Get invalid vsync");
            break;
    }
    return 0;
}

/*
 * BeCareful!!: Need avoid setplane conflict, we should hold mMutex or
 * setConnectorStatus(false) && mSkipComposition = true;
 */
int32_t Hwc2Display::blankDisplay() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    mPowerMode->setConnectorStatus(false);
    mSkipComposition = true;

    blankDisplayLocked();

    return 0;
}

/*
 * Make sure all display planes are blank (since there is no layer)
 *
 * If composer service died, surfaceflinger will restart and frameBufferSurface will
 * be recreated. But one framebuffer will be hold by the osd driver. If cma ion memory
 * only configed to triple size of FrameBuffer, there will be one non continuous FrameBuffer
 * and lead to messed display.
 */
int32_t Hwc2Display::blankDisplayLocked(bool blockMode) {
    ATRACE_CALL();
    MESON_LOGD("displayId:%d, blank all display planes", mDisplayId);

    if (!mCrtc)
        return 0;

    mCrtc->prePageFlip();

    {
        ATRACE_NAME("setPlane-blank");
        for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
            (*it)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
        }
    }

    int32_t fenceFd = -1;
    if (mCrtc->pageFlip(fenceFd) == 0) {
        ATRACE_NAME("pageFlip");
        if (blockMode) {
            std::shared_ptr<DrmFence> outfence =
                std::make_shared<DrmFence>(fenceFd);
            outfence->wait(3000);
        } else
            close(fenceFd);
    }

    return 0;
}

const char * Hwc2Display::getName() {
    return mConnector->getName();
}

const drm_hdr_capabilities_t * Hwc2Display::getHdrCapabilities() {
    if (mConnector->isConnected() == false) {
        MESON_LOGD("Requested HDR Capabilities, returning null");
        return nullptr;
    }

    mConnector->getHdrCapabilities(&mHdrCaps);

    if (HwcConfig::defaultHdrCapEnabled()) {
        constexpr int sDefaultMinLumiance = 0;
        constexpr int sDefaultMaxLumiance = 500;
        mHdrCaps.HLGSupported = true;
        mHdrCaps.HDR10Supported = true;
        mHdrCaps.maxLuminance = sDefaultMaxLumiance;
        mHdrCaps.avgLuminance = sDefaultMaxLumiance;
        mHdrCaps.minLuminance = sDefaultMinLumiance;
    } else {
        mConnector->getHdrCapabilities(&mHdrCaps);
    }

    return &mHdrCaps;
}

void Hwc2Display::getDispMode(drm_mode_info_t & dispMode){
    dispMode = mDisplayMode;
}

hwc2_error_t Hwc2Display::getFrameMetadataKeys(
    uint32_t* outNumKeys, int32_t* outKeys) {
    *outNumKeys = mHdrKeys.size();
    if (NULL != outKeys) {
        for (uint32_t i = 0; i < *outNumKeys; i++)
            outKeys[i] = mHdrKeys[i];
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setVsyncEnable(hwc2_vsync_t enabled) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    bool state;
    switch (enabled) {
        case HWC2_VSYNC_ENABLE:
            state = true;
            break;
        case HWC2_VSYNC_DISABLE:
            state = false;
            break;
        default:
            MESON_LOGE("[%s]: set vsync state invalid %d.", __func__, enabled);
            return HWC2_ERROR_BAD_PARAMETER;
    }

    if (DebugHelper::getInstance().enableVsyncDetail()) {
        MESON_LOGD("setVsyncEnable: %s", state ? "true" : "false");
    }

    mVsyncState = state;
    if (mVsync.get())
        mVsync->setEnabled(mVsyncState);
    return HWC2_ERROR_NONE;
}

// HWC uses SystemControl for HDMI query / control purpose. Because both parties
// respond to the same hot plug uevent additional means of synchronization
// are required before former can talk to the latter. To accomplish that HWC
// shall wait for SystemControl before it can update its state and notify FWK
// accordingly.
void Hwc2Display::onHotplug(bool connected) {
    ATRACE_CALL();

    bool bSendPlugOut = false;
    MESON_LOGD("displayID:%d, On hot plug: [%s]",
            mDisplayId, connected == true ? "Plug in" : "Plug out");

    if (mModePolicy.get())
        mModePolicy->onHotplug(connected);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (connected) {
            if (mConnector && mConnector->getType() != DRM_MODE_CONNECTOR_HDMIA) {
                mOutsideChanged = true;
                mPowerMode->setConnectorStatus(true);
                mObserver->refresh();
            }
            mSignalHpd = true;
            handleVtThread();
            return;
        } else {
            if (!mModePolicy.get()) {
                drm_mode_info_t displayMode;
                if (sc_get_property_boolean("persist.vendor.sys.vmx", false)) {
                    strcpy(displayMode.name, "576cvbs");
                } else {
                    sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
                    usleep(100000); // add 100ms delay after av mute
                    strcpy(displayMode.name, "dummy_l");
                }
                mCrtc->setMode(displayMode);
            }
        }

        mPowerMode->setConnectorStatus(false);
        mSkipComposition = true;
        if (mObserver != NULL ) {
            bSendPlugOut = true;
        }

        blankDisplayLocked(mDisplayId == HWC_DISPLAY_PRIMARY ? true : false);
    }

    /* call hotplug out of lock, SF may call some hwc function to cause deadlock.*/
    /* switch to software vsync when hdmi plug out and no cvbs mode */
    /* when hdmi plugout, send CONNECT message for "hdmi-only" */
    if (mConnector && (mConnector->getType() == DRM_MODE_CONNECTOR_HDMIA ||
                mConnector->getType() == DRM_MODE_CONNECTOR_VIRTUAL)) {
        mVsync->setSoftwareMode();
        mModeMgr->update();
        if (bSendPlugOut) {
            cleanupBeforeDestroy();
            mObserver->onHotplug(connected);
        }
    }

    /* wake up the setActiveConfig, if hdmi plug out */
    std::unique_lock<std::mutex> stateLock(mStateLock);
    mModeChanged = false;
    stateLock.unlock();
    mStateCondition.notify_all();
}

void Hwc2Display::onVsyncPeriodTimingChanged(hwc_vsync_period_change_timeline_t* updatedTimeline) {
    if (mObserver) {
        mObserver->onVsyncPeriodTimingChanged(updatedTimeline);
    } else {
        MESON_LOGE("%s Hwc2Display (%p) observer is NULL", __func__, this);
    }
}

/* clear all layers and blank display when extend display plugout,
 * So the resource used by display drvier can be released.
 * Or framebuffer may allocate fail when do plug in/out quickly.
 */
void Hwc2Display::cleanupBeforeDestroy() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    /*clear framebuffer reference by gpu composer*/
    auto itComposer = mComposers.find(MESON_CLIENT_COMPOSER);
    if (itComposer != mComposers.end()) {
        std::shared_ptr<IComposer> clientComposer = itComposer->second;
        clientComposer->prepare();
        // TODO: workaround to clear CLIENT_COMPOSER's clientTarget
        hwc_region_t damage = {0, 0};
        std::shared_ptr<DrmFramebuffer> fb = nullptr;
        clientComposer->setOutput(fb, damage);
    }

    /* we need release all cache handles */
    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        (*it)->clearPlaneResources();
    }

    {
        ATRACE_NAME("resetLayers");
        std::lock_guard<std::mutex> vtLock(mVtMutex);
        MESON_LOGD("%s displayId:%d, clear layers", __func__, mDisplayId);
        for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
            std::shared_ptr<Hwc2Layer> layer = it->second;
            if (layer && layer->isVtBuffer())
                layer->releaseVtResource();
        }

        mLayers.clear();
        mChangedLayers.clear();
        mPresentLayers.clear();
        // For round corner,mVirtualLayer should not be cleared
#ifdef ENABLE_VIRTUAL_LAYER
        if (mVirtualLayer) {
            mLayers.emplace(mVirtualLayer->getUniqueId(), mVirtualLayer);
        }
#endif
        if (mCompositionStrategy) {
            mCompositionStrategy->updateComposition();
            mCompositionStrategy->setup(mPresentLayers,
                    mPresentComposers, mPresentPlanes, mCrtc, 0, 0, mDisplayMode);
        }

        // reset bitmap
        if (mLayersBitmap)
            mLayersBitmap->reset();
        handleVtThread();
    }
}

void Hwc2Display::onUpdate(bool bHdcp) {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("On update: [%s]", bHdcp == true ? "HDCP verify success" : "HDCP verify fail");

    if (bHdcp) {
        if (mObserver != NULL) {
            mObserver->refresh();
        } else {
            MESON_LOGE("No display observe register to display (%s)", getName());
        }
    }
}

void Hwc2Display::onVsync(int64_t timestamp, uint32_t vsyncPeriodNanos, int vsyncType) {
    ATRACE_CALL();
    switch (vsyncType) {
        case DISPLAY_DEFAULT:
            if (mObserver != NULL) {
                mObserver->onVsync(timestamp, vsyncPeriodNanos);
            } else {
                MESON_LOGE("Hwc2Display (%p) observer is NULL", this);
            }
            break;
        case DISPLAY_VIDEOTUNNEL:
            mVsyncTimestamp = timestamp;
            if (mVtDisplayThread) {
                mVtDisplayThread->onVtVsync(timestamp, vsyncPeriodNanos);
            }
            break;
        case DISPLAY_WHITEBOARD:
            mVsyncTimestamp = timestamp;
            if (mWBDisplayThread) {
                mWBDisplayThread->onWBVsync(timestamp, vsyncPeriodNanos);
            }
            break;
        default:
            MESON_LOGE("onVsync get invalid vsync");
            break;
    }
}

void Hwc2Display::onModeChanged(int stage) {
    bool bSendPlugIn = false;
    bool hdrCapsChanged = false;
    bool bNotifySC = false;
    ATRACE_CALL();

    {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_LOGD("On mode change state: [%s]", stage == 1 ? "Complete" : "Begin to change");
        if (stage == 1) {
            if (mObserver != NULL) {
                /*plug in and set displaymode ok, update information.*/
                if (mSignalHpd) {
                    const drm_hdr_capabilities_t oldCaps = mHdrCaps;
                    mConnector->getHdrCapabilities(&mHdrCaps);
                    /* check whether hdr cap changed */
                    hdrCapsChanged = drmHdrCapsDiffer(oldCaps, mHdrCaps);
                    mConnector->getSupportedContentTypes(mSupportedContentTypes);
                    mCrtc->getHdrMetadataKeys(mHdrKeys);
                }

                /*update mode success.*/
                if (mModeMgr->getDisplayMode(mDisplayMode) == 0) {
                    MESON_LOGD("Hwc2Display::onModeChanged getDisplayMode [%s]", mDisplayMode.name);
                    mPowerMode->setConnectorStatus(true);
                    mSkipComposition = false;
                    mOutsideChanged = true;
                    bSendPlugIn = true;
                    if (mSignalHpd) {
                        mSignalHpd = false;
                        bNotifySC = true;
                    }

                    uint32_t fbW,fbH;
                    HwcConfig::getFramebufferSize(0, fbW, fbH);
                    mScaleValue = (float)fbW/(float)mDisplayMode.pixelW;

                    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it)
                        (*it)->setDisplayMode(mDisplayMode);
                }
            } else {
                MESON_LOGE("No display observe register to display (%s)", getName());
            }
        } else {
            /* begin change mode, need blank once */
            mPowerMode->setConnectorStatus(false);
            mSkipComposition = true;

            return;
        }
    }

    /*call hotplug out of lock, SF may call some hwc function to cause deadlock.*/
    if (bSendPlugIn && (mModeMgr->needCallHotPlug() || hdrCapsChanged)) {
        MESON_LOGD("onModeChanged mObserver->onHotplug(true) hdrCapsChanged:%d", hdrCapsChanged);
        // only clear layers when we can send hotplug event
        // as the framework display will recreate when it receive hotplug event
        if (!mFirstPresent) {
            cleanupBeforeDestroy();
        }
        mObserver->onHotplug(true);
        if (bNotifySC)
            sc_notify_hdmi_plugin();
    } else {
        MESON_LOGD("mModeMgr->resetTags");
        mModeMgr->resetTags();
    }

    /* wake up the setActiveConfig */
    std::unique_lock<std::mutex> stateLock(mStateLock);
    mModeChanged = false;
    stateLock.unlock();
    mStateCondition.notify_all();

    /*last call refresh*/
    mObserver->refresh();
}

/*
 * LayerId is 16bits.
 * Higher 8 Bits: now is 256 layer slot, the higher 8 bits is the slot index.
 * Lower 8 Bits: a sequence no, used to distinguish layers have same slot index (
 * which may happened a layer destroyed and at the same time a new layer created.)
 */
#define MAX_HWC_LAYERS (256)
#define LAYER_SLOT_BITS (8)

void Hwc2Display::initLayerIdGenerator() {
    mLayersBitmap = std::make_shared<BitsMap>(MAX_HWC_LAYERS);
    mLayerSeq = 0;
}

hwc2_layer_t Hwc2Display::createLayerId() {
    hwc2_layer_t layerId = 0;
    int idx = mLayersBitmap->getZeroBit();
    MESON_ASSERT(idx >= 0, "Bitmap getZeroBit failed");
    mLayersBitmap->setBit(idx);

    mLayerSeq++;
    mLayerSeq %= MAX_HWC_LAYERS;

    layerId = ((idx & (MAX_HWC_LAYERS - 1)) << LAYER_SLOT_BITS) |mLayerSeq;
    return layerId;
}

void Hwc2Display::destroyLayerId(hwc2_layer_t id) {
    int slotIdx = id >> LAYER_SLOT_BITS;
    mLayersBitmap->clearBit(slotIdx);
}

hwc2_error_t Hwc2Display::createLayer(hwc2_layer_t * outLayer) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    std::lock_guard<std::mutex> vtLock(mVtMutex);

    std::shared_ptr<Hwc2Layer> layer = std::make_shared<Hwc2Layer>(mDisplayId);
    uint32_t idx = createLayerId();
    *outLayer = idx;
    layer->setUniqueId(*outLayer);
    mLayers.emplace(*outLayer, layer);
    MESON_LOGV("%s displayId:%d, layerId:%u", __func__, mDisplayId, idx);

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::destroyLayer(hwc2_layer_t  inLayer) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    auto layerit = mLayers.find(inLayer);
    if (layerit == mLayers.end())
        return HWC2_ERROR_BAD_LAYER;

    std::shared_ptr<Hwc2Layer> layer = layerit->second;
    DebugHelper::getInstance().removeDebugLayer((int)inLayer);
    MESON_LOGV("%s displayId:%d, layerId:%" PRIu64 "",
            __func__, mDisplayId, inLayer);
    mLayers.erase(inLayer);

    handleVtThread();
    if (layer && layer->isVtBuffer())
        layer->releaseVtResource();
    destroyLayerId(inLayer);
    return HWC2_ERROR_NONE;
}

int32_t Hwc2Display::loadVirtualLayerData(FILE *file, std::shared_ptr<Hwc2Layer> tempVirtualLayer) {

    //use libpng
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));
    png_init_io(png_ptr, file);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);

    int m_width = png_get_image_width(png_ptr, info_ptr);
    int m_height = png_get_image_height(png_ptr, info_ptr);

    hwc_frect_t mCrop = {0, 0, static_cast<float>(m_width), static_cast<float>(m_height)};
    tempVirtualLayer->setSourceCrop(mCrop);

    buffer_handle_t hnd = gralloc_alloc_dma_buf(m_width, m_height,
                                                   HAL_PIXEL_FORMAT_RGBA_8888, false, false);
    if (hnd == NULL ) {
        MESON_LOGE("tempVirtualLayer allocate buf failed ");
        return -1;
    }

    tempVirtualLayer->setBuffer(hnd,-1);
    int bufFd = am_gralloc_get_buffer_fd(hnd);
    char *base = (char *)mmap(NULL, m_width*m_height*4, PROT_WRITE , MAP_SHARED, bufFd, 0);
    if (base == MAP_FAILED) {
        MESON_LOGE("tempVirtualLayer mmap failed ");
        return -1;
    }

    unsigned char ** row_pointers = png_get_rows(png_ptr, info_ptr);
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if (row_pointers) {
        MESON_LOGD("tempVirtualLayer size(%dx%d) row_bytes %d ", m_width, m_height, row_bytes);
        for (int i = 0; i < m_height; i++) {
            memcpy(base + row_bytes*i, row_pointers[i], row_bytes);
        }
    }
    munmap(base,m_width*m_height*4);

    return 0;
}

hwc2_error_t Hwc2Display::createVirtualLayer(hwc2_layer_t * outLayer) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);

    std::shared_ptr<Hwc2Layer> mTempVirtualLayer = std::make_shared<Hwc2Layer>(1);
    const char * rcpngPath  = "/data/vendor/Rhodes_RoundedCorner_Proto_alpha_v2.png";

    FILE *file = fopen(rcpngPath,"rb");
    if (file == NULL) {
        MESON_LOGE("Unable to open PNG %s ", rcpngPath);
        return HWC2_ERROR_NO_RESOURCES;
    }

    // load png data for Rounded Corner
    int ret = loadVirtualLayerData(file, mTempVirtualLayer);
    fclose(file);
    if (ret == -1) {
        MESON_LOGE("Load data failed\n");
        return HWC2_ERROR_NO_RESOURCES;
    }

    //setUp the render thread
    std::shared_ptr<CompositionProcessor> comProcessor = std::make_shared<CompositionProcessor>();
    comProcessor->setup();

    //Create the outputs Buffer
    buffer_handle_t hnd;
    hnd = gralloc_alloc_dma_buf(FB_SIZE_1080P_W, FB_SIZE_1080P_H,
                                   HAL_PIXEL_FORMAT_RGBA_8888, true, true, RENDER_TARGET);
    auto outBuf = std::make_shared<DrmFramebuffer>(hnd, -1);

    std::shared_ptr<DrmFramebuffer> inUI = mTempVirtualLayer;
    comProcessor->composite(inUI, inUI, outBuf);

    int width = am_gralloc_get_width(outBuf->mBufferHandle);
    int height = am_gralloc_get_height(outBuf->mBufferHandle);
    [[maybe_unused]] int stride = am_gralloc_get_stride_in_pixel(outBuf->mBufferHandle);
    [[maybe_unused]] int format = am_gralloc_get_format(outBuf->mBufferHandle);
    MESON_LOGD("outputs format %d, (%d, %d) stride %d\n",
        format, width, height, stride);

    mVirtualLayer = std::make_shared<Hwc2Layer>(mDisplayId);
    mVirtualLayer->setBuffer(hnd,-1);
    hwc_frect_t mCrop = {0, 0, static_cast<float>(width), static_cast<float>(height)};
    mVirtualLayer->setSourceCrop(mCrop);
    mVirtualLayer->setBlendMode(HWC2_BLEND_MODE_PREMULTIPLIED);

    uint32_t idx = createLayerId();
    *outLayer = idx;
    mVirtualLayer->setUniqueId(*outLayer);
    mVirtualLayer->mIsVirtualLayer = true;
    mVirtualLayer->mZorder = 63;
    mVirtualLayer->mCompositionType = MESON_COMPOSITION_UNDETERMINED;
    mLayers.emplace(*outLayer, mVirtualLayer);

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setCursorPosition(hwc2_layer_t layer __unused,
    int32_t x __unused, int32_t y __unused) {
    MESON_LOG_EMPTY_FUN();
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setColorTransform(const float* matrix,
    android_color_transform_t hint) {
    bool enable = false;
    if (hint == HAL_COLOR_TRANSFORM_IDENTITY) {
        memset(mColorMatrix, 0, sizeof(float) * 16);
    } else {
        memcpy(mColorMatrix, matrix, sizeof(float) * 16);
        enable = true;
    }

    String8 matrixDump;
    matrixDump.append("\n-------------------------------------------------\n");
    for (int i = 0; i < 16; i ++ )  {
        matrixDump.appendFormat("%6f ", matrix[i]);
        if ((i+1) % 4 == 0)
            matrixDump.append("\n");
    }
    matrixDump.append("-------------------------------------------------\n");

    MESON_LOGV("%s hint %d color matrix:%s",
            __func__, hint, matrixDump.c_str());

    if (mConnector && (mConnector->getType() == DRM_MODE_CONNECTOR_HDMIA ||
                       mConnector->getType() == DRM_MODE_CONNECTOR_TV)) {
        mForceClientComposer = false;
        AmVecmDev::getInstance().setColorTransform(matrix, enable);
    } else {
        // TODO: remove it when driver is support ColorTransform on TV product.
        mForceClientComposer = enable;
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setPowerMode(int32_t mode) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("%s %d", __func__, mode);
    if (mode == HWC3_POWER_MODE_SUSPEND && mAidlService) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    int32_t ret = mPowerMode->setPowerMode(mode);

    /* need blank display when power off */
    if (mode == HWC2_POWER_MODE_OFF) {
        blankDisplayLocked();
    }
    return (hwc2_error_t) ret;
}

std::shared_ptr<Hwc2Layer> Hwc2Display::getLayerById(hwc2_layer_t id) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>>::iterator it =
        mLayers.find(id);

    if (it != mLayers.end())
        return it->second;

    return NULL;
}

hwc2_error_t Hwc2Display::collectLayersForPresent() {
    /*
    * 1) add reference to Layers to keep it alive during display.
    * 2) for special layer, update its composition type.
    * 3) sort Layers by zorder for composition.
    */
    mPresentLayers.reserve(10);

    /*Check if layer list is changed or not*/
    bool bUpdateLayerList = false;
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (layer->isUpdateZorder() == true) {
            bUpdateLayerList = true;
            break;
        }
    }

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        std::shared_ptr<DrmFramebuffer> buffer = layer;
        if ((bUpdateLayerList == true && layer->isUpdateZorder() == false) &&
            !layer->isVtBuffer() && !layer->isVirtualLayer()) {
            continue;
        }

        mPresentLayers.push_back(buffer);

        if (isLayerHideForDebug(it->first)) {
            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
            continue;
        }

        if (HwcConfig::isHeadlessMode()) {
            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
        } else {
            if (layer->mHwcCompositionType == HWC2_COMPOSITION_CLIENT) {
                layer->mCompositionType = MESON_COMPOSITION_CLIENT;
            } else {
                /*
                * Other layers need further handle:
                * 1) HWC2_COMPOSITION_DEVICE
                * 2) HWC2_COMPOSITION_SOLID_COLOR
                * 3) HWC2_COMPOSITION_CURSOR
                * 4) HWC2_COMPOSITION_SIDEBAND
                */
                /*composition type unknown, set to none first.*/
                layer->mCompositionType = MESON_COMPOSITION_UNDETERMINED;
            }
        }
    }

    if (mPresentLayers.size() > 1) {
        /* Sort mComposeLayers by zorder. */
        struct {
            bool operator() (std::shared_ptr<DrmFramebuffer> a,
                std::shared_ptr<DrmFramebuffer> b) {
                return a->mZorder > b->mZorder;
            }
        } zorderCompare;
        std::sort(mPresentLayers.begin(), mPresentLayers.end(), zorderCompare);
    }

    //Set the video layer to dummy when the whiteboard is open and the miniboard is not open
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        Hwc2Layer * layer;
        if (((*it)->getFbType() == DRM_FB_VIDEO_UVM_DMA ||(*it)->getFbType() == DRM_FB_VIDEO_TUNNEL_SIDEBAND) && mHideVideo) {
            layer = (Hwc2Layer*)(it->get());
            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::collectPlanesForPresent() {
    mPresentPlanes = mPlanes;

    if (DebugHelper::getInstance().debugPlanes()) {
        std::map<int, int> planeFlags;
        DebugHelper::getInstance().getPlaneDebugFlags(planeFlags);

        for (auto  it = mPresentPlanes.begin(); it != mPresentPlanes.end(); it++) {
            std::shared_ptr<HwDisplayPlane> plane = *it;

            auto dbgIt = planeFlags.find(plane->getId());
            if (dbgIt != planeFlags.end()) {
                plane->setDebugFlag(dbgIt->second);
            } else {
                plane->setDebugFlag(0);
            }
        }
    }

    return  HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::collectComposersForPresent() {
    for (auto it = mComposers.begin(); it != mComposers.end(); it++) {
        mPresentComposers.push_back(it->second);
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::collectCompositionStgForPresent() {
    mPresentCompositionStg = mCompositionStrategy;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setCalibrateInfo(int32_t caliX,int32_t caliY,int32_t caliW,int32_t caliH){
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    mCalibrateCoordinates[0] = caliX;
    mCalibrateCoordinates[1] = caliY;
    mCalibrateCoordinates[2] = caliW;
    mCalibrateCoordinates[3] = caliH;

    return HWC2_ERROR_NONE;
}

void Hwc2Display::outsideChanged(){
    /*outside hwc has changes need do validate first*/
    mOutsideChanged= true;
    mObserver->refresh();
}

void Hwc2Display::createVirtualLayer() {
    std::lock_guard<std::mutex> lock(mMutex);
    mVirtualLayer = std::make_shared<Hwc2Layer>(mDisplayId);
    mVLIdx = createLayerId();
    mVirtualLayer->setUniqueId(mVLIdx);
    mVirtualLayer->mIsVirtualLayer = true;

    hwc_frect_t mCrop = {0, 0, static_cast<float>(FB_SIZE_4K_W), static_cast<float>(FB_SIZE_4K_H)};
    mVirtualLayer->setSourceCrop(mCrop);
    hwc_rect_t mDisplayFrame = {0, 0, FB_SIZE_4K_W, FB_SIZE_4K_H};
    mVirtualLayer->setDisplayFrame(mDisplayFrame);
    mVirtualLayer->setBlendMode(HWC2_BLEND_MODE_NONE);
    mVirtualLayer->mZorder = MESON_WHITE_BOARD_ZORDER;
    mVirtualLayer->mCompositionType = MESON_COMPOSITION_UNDETERMINED;

    //Bind the buffer to Virtual Layer
    if (wbHnd == nullptr) {
        wbHnd = gralloc_alloc_dma_buf(FB_SIZE_4K_W, FB_SIZE_4K_H, HAL_PIXEL_FORMAT_RGBA_8888, true, true, RENDER_TEXTURE);
        if (wbHnd == nullptr ) {
            MESON_LOGE("Alloc dma buffer for virtual failed");
            return;
        }
    }

    mVirtualLayer->setBuffer(wbHnd,-1);
    mLayers.emplace(mVLIdx, mVirtualLayer);
    return;
}

void Hwc2Display::destroyVirtualLayer() {
    std::lock_guard<std::mutex> lock(mMutex);
    auto layerit = mLayers.find(mVLIdx);
    if (layerit == mLayers.end()) {
        MESON_LOGE("The virtual layer is invalid mVLIdx = %d",mVLIdx);
        return;
    }
    destroyLayerId(mVLIdx);
    mLayers.erase(mVLIdx);
    mVLIdx = -1;
    return;
}

void Hwc2Display::getWriteBoardMode(bool& mode) {
    mode = mWhiteBoardMode;
    return;
}

void Hwc2Display::hideVideoLayer(bool hide) {
    MESON_LOGD("set hide video layer mode to %s", hide ? "true" : "false");
    mHideVideo = hide;
    return;
}

void Hwc2Display::setWBDisplayFrame(int x, int y) {
    hwc_rect_t displayFrame = {x, y, (int)mDisplayMode.pixelW, (int)mDisplayMode.pixelH};
    mVirtualLayer->setDisplayFrame(displayFrame);
    return;
}

void Hwc2Display::setWriteBoardMode(bool mode) {
    MESON_LOGD("set setWriteBoardMode to %s, mWhiteBoardMode = %d", mode ? "true" : "false", mWhiteBoardMode);

    //First create a Virtual Layer to show White Board content.
    if (mode == true && mode != mWhiteBoardMode) {
        createVirtualLayer();
        mWhiteBoardMode = true;
    } else if (mode == false) {
        destroyVirtualLayer();
        mWhiteBoardMode = false;
    }

    //Create White Board Display Thread
    if (!mInitWBDisplayThread) {
        handleWBThread();
        mInitWBDisplayThread = true;
    }

    //Enable White Board display thread Vsync
    if (mWhiteBoardMode == true) {
        mWBVsync->setEnabled(true);
    } else {
        mWBVsync->setEnabled(false);
    }

    mObserver->refresh();
    return;
}

void Hwc2Display::disableSideband(bool isDisable){
    MESON_LOGD("set SideBand Disable %s", isDisable ? "true" : "false");
    mDisableSideband = isDisable;
}

int32_t Hwc2Display::getDisplayIdentificationData(uint32_t &outPort,
        std::vector<uint8_t> &outData) {
    int32_t ret = mConnector->getIdentificationData(outData);
    if (ret == 0)
        outPort = mConnector->getType();
    ALOGE("getPort %d",mConnector->getType());
    return ret;
}

drm_connector_type_t Hwc2Display::getConnectorType() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mConnector != nullptr) {
        return mConnector->getType();
    } else {
        return DRM_MODE_CONNECTOR_INVALID_TYPE;
    }
}

int32_t Hwc2Display::loadCalibrateInfo() {
    hwc2_config_t config;
    int32_t configWidth;
    int32_t configHeight;
    if (mModeMgr->getActiveConfig(&config) != HWC2_ERROR_NONE) {
        ALOGE("[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }
    if (mModeMgr->getDisplayAttribute(config,
            HWC2_ATTRIBUTE_WIDTH, &configWidth) != HWC2_ERROR_NONE) {
        ALOGE("[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }
    if (mModeMgr->getDisplayAttribute(config,
            HWC2_ATTRIBUTE_HEIGHT, &configHeight) != HWC2_ERROR_NONE) {
        ALOGE("[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }

    if (mDisplayMode.pixelW == 0 || mDisplayMode.pixelH == 0) {
        ALOGV("[%s]: Displaymode is invalid(%s, %dx%d)!",
                __func__, mDisplayMode.name, mDisplayMode.pixelW, mDisplayMode.pixelH);
        return -ENOENT;
    }

    /*default info*/
    mCalibrateInfo.framebuffer_w = configWidth;
    mCalibrateInfo.framebuffer_h = configHeight;
    mCalibrateInfo.crtc_display_x = mCalibrateCoordinates[0];
    mCalibrateInfo.crtc_display_y = mCalibrateCoordinates[1];
    mCalibrateInfo.crtc_display_w = mCalibrateCoordinates[2];
    mCalibrateInfo.crtc_display_h = mCalibrateCoordinates[3];

    return 0;
}
// Scaled display frame to the framebuffer config if necessary
// (i.e. not at the default resolution of 1080p)
int32_t Hwc2Display::adjustDisplayFrame() {
    bool bNeedUpdateLayer = false;

    if (mOutsideChanged) {
        mOutsideChanged = false;
        bNeedUpdateLayer = true;
    }

    Hwc2Layer * layer;
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        layer = (Hwc2Layer*)(it->get());
/*For round corner*/
#ifdef ENABLE_VIRTUAL_LAYER
        if (mVirtualLayer && layer->isVirtualLayer()) {
            hwc_rect_t mFrame = {0, 0, mCalibrateInfo.framebuffer_w, mCalibrateInfo.framebuffer_h};
            mVirtualLayer->setDisplayFrame(mFrame);
        }
#endif
        layer->adjustDisplayFrame(mCalibrateInfo);

        if (bNeedUpdateLayer) {
            layer->setLayerUpdate(true);
            layer->vtRefresh();
        }
    }

    return 0;
}

hwc2_error_t Hwc2Display::validateDisplay(uint32_t* outNumTypes,
    uint32_t* outNumRequests) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    /*clear data used in composition.*/
    mPresentLayers.clear();
    mPresentComposers.clear();
    mPresentPlanes.clear();
    mPresentCompositionStg.reset();
    mChangedLayers.clear();
    mOverlayLayers.clear();
    mFailedDeviceComp = false;
    mSkipComposition = false;
    mConfirmSkip = false;

    hwc2_error_t ret = collectLayersForPresent();
    if (ret != HWC2_ERROR_NONE) {
        return ret;
    }
    ret = collectComposersForPresent();
    if (ret != HWC2_ERROR_NONE) {
        return ret;
    }
    ret = collectPlanesForPresent();
    if (ret != HWC2_ERROR_NONE) {
        return ret;
    }
    ret = collectCompositionStgForPresent();
    if (ret != HWC2_ERROR_NONE) {
        return ret;
    }

    /*collect composition flag*/
    uint32_t compositionFlags = 0;
    if (mForceClientComposer ||
        DebugHelper::getInstance().disableUiHwc()) {
        compositionFlags |= COMPOSE_FORCE_CLIENT;
    }

    if (HwcConfig::secureLayerProcessEnabled()) {
        if (!mConnector->isSecure()) {
            compositionFlags |= COMPOSE_HIDE_SECURE_FB;
        }
    }

    if (mDisableSideband) {
        compositionFlags |= COMPOSE_DISABLE_SIDEBAND;
    }

    if (mIsDisablePostProcessor) {
        compositionFlags |= COMPOSE_DISABLE_POSTPROCESSOR;
    }

    /*check power mode*/
    if (mPowerMode->needBlankScreen(mPresentLayers.size())) {
        /*set all layers to dummy*/
        Hwc2Layer *layer;
        for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
            layer = (Hwc2Layer*)(it->get());
            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
        }
        if (!mPowerMode->getScreenStatus()) {
            MESON_LOGV("Need to blank screen.");
            mConfirmSkip = true;
         //   mPresentLayers.clear();
        } else {
            mSkipComposition = true;
        }
    }
    /*do composition*/
    if (!mSkipComposition) {
        mPowerMode->setScreenStatus((mPresentLayers.size() > 0 ? false : true) || mConfirmSkip);
        /*update calibrate info.*/
        loadCalibrateInfo();
        /*update displayframe before do composition.*/
        if (mPresentLayers.size() > 0)
            adjustDisplayFrame();
        /*setup composition strategy.*/
        mPresentCompositionStg->setup(mPresentLayers,
            mPresentComposers, mPresentPlanes, mCrtc, compositionFlags,
            mScaleValue, mDisplayMode);
        if (mPresentCompositionStg->decideComposition() < 0)
            return HWC2_ERROR_NO_RESOURCES;

    } else {
        /* skip Composition */
        std::shared_ptr<IComposer> clientComposer =
            mComposers.find(MESON_CLIENT_COMPOSER)->second;
        clientComposer->prepare();
    }
    /*collect changed dispplay, layer, composition.*/
    ret = collectCompositionRequest(outNumTypes, outNumRequests);

    if (mPowerMode->getScreenStatus()) {
        mProcessorFlags |= PRESENT_BLANK;
    }

    mValidateDisplay = true;

    adjustVsyncMode();

    refreshVtLayersLocked();

    /*dump at end of validate, for we need check by some composition info.*/
    bool dumpLayers = false;
    if (DebugHelper::getInstance().logCompositionDetail()) {
        MESON_LOGE("***CompositionFlow (%s):\n", __func__);
        dumpLayers = true;
    } else if (mFailedDeviceComp) {
        MESON_LOGE("***MonitorFailedDeviceComposition: \n");
        dumpLayers = true;
    }
    if (dumpLayers) {
        String8 layersDump;
        dumpPresentLayers(layersDump);
        MESON_LOGE("%s", layersDump.c_str());
    }
    return ret;
}

hwc2_error_t Hwc2Display::collectCompositionRequest(
    uint32_t* outNumTypes, uint32_t* outNumRequests) {
    Hwc2Layer *layer;
    // for self-adaptive
    int maxRegion = 0, region = 0;
    ISystemControl::Rect maxRect{0, 0, 0, 0};

    bool hasDecoration = false;
    /*collect display requested, and changed composition type.*/
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        layer = (Hwc2Layer*)(it->get());
        if (layer->isVirtualLayer()) {
            continue;
        }

        /* decoration type not support it now */
        if (layer->mFbType == DRM_FB_DECORATION) {
            hasDecoration = true;
        }
        /*record composition changed layer.*/
        hwc2_composition_t expectedHwcComposition =
            mesonComp2Hwc2Comp(layer);
        if (expectedHwcComposition  != layer->mHwcCompositionType) {
            mChangedLayers.push_back(layer->getUniqueId());
            /*For debug.*/
            if (DebugHelper::getInstance().monitorDeviceComposition() &&
                (mPresentLayers.size() <= DebugHelper::getInstance().deviceCompositionThreshold()) &&
                (expectedHwcComposition == HWC2_COMPOSITION_CLIENT)) {
                mFailedDeviceComp = true;
            }
        }
        if (expectedHwcComposition == HWC2_COMPOSITION_SIDEBAND ||
            layer->mCompositionType == MESON_COMPOSITION_PLANE_AMVIDEO)
            mProcessorFlags |= PRESENT_SIDEBAND;

        // for self-adaptive
        if (isVideoPlaneComposition(layer->mCompositionType)) {
            /* For hdmi self-adaptive in systemcontrol.
             * hdmi frame rate is on
             * */
            drm_rect_t dispFrame = layer->getDisplayFrame();
            region = (dispFrame.right - dispFrame.left) *
                     (dispFrame.bottom - dispFrame.top);
            if (region > maxRegion) {
                maxRegion = region;
                maxRect.left   = dispFrame.left;
                maxRect.right  = dispFrame.right;
                maxRect.top    = dispFrame.top;
                maxRect.bottom = dispFrame.bottom;
            }
        }
    }

    // for self-adaptive
    if (maxRegion != 0 && mVideoLayerRegion != maxRegion) {
        sc_frame_rate_display(true, maxRect);
        mVideoLayerRegion = maxRegion;
    }

    if (maxRegion == 0 && mVideoLayerRegion != 0) {
        sc_frame_rate_display(false, maxRect);
        mVideoLayerRegion = 0;
    }

    /*collect client clear layer.*/
    std::shared_ptr<IComposer> clientComposer =
        mComposers.find(MESON_CLIENT_COMPOSER)->second;
    std::vector<std::shared_ptr<DrmFramebuffer>> overlayLayers;
    if (0 == clientComposer->getOverlyFbs(overlayLayers) ) {
        auto it = overlayLayers.begin();
        for (; it != overlayLayers.end(); ++it) {
            layer = (Hwc2Layer*)(it->get());
            mOverlayLayers.push_back(layer->getUniqueId());
        }
    }

    *outNumRequests = mOverlayLayers.size();
    *outNumTypes    = mChangedLayers.size();

    if (hasDecoration)
        return HWC2_ERROR_UNSUPPORTED;

    return ((*outNumTypes) > 0) ? HWC2_ERROR_HAS_CHANGES : HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getDisplayRequests(
    int32_t* outDisplayRequests, uint32_t* outNumElements,
    hwc2_layer_t* outLayers,int32_t* outLayerRequests) {
    *outDisplayRequests = 0;

    /*check if need HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET*/
    *outNumElements = mOverlayLayers.size();
    if (outLayers && outLayerRequests) {
        for (uint32_t i = 0; i < mOverlayLayers.size(); i++) {
            outLayers[i] = mOverlayLayers[i];
            outLayerRequests[i] = HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET;
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getChangedCompositionTypes(
    uint32_t * outNumElements, hwc2_layer_t * outLayers,
    int32_t *  outTypes) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outNumElements = mChangedLayers.size();
    if (outLayers && outTypes) {
        for (uint32_t i = 0; i < mChangedLayers.size(); i++) {
            std::shared_ptr<Hwc2Layer> layer = mLayers.find(mChangedLayers[i])->second;
            outTypes[i] = mesonComp2Hwc2Comp(layer.get());
            outLayers[i] = mChangedLayers[i];
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::acceptDisplayChanges() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
   /* commit composition type */
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer * layer = (Hwc2Layer*)(it->get());
        layer->commitCompType(mesonComp2Hwc2Comp(layer));
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::presentSkipValidateCheck() {
    if (DebugHelper::getInstance().disableUiHwc()) {
        return HWC2_ERROR_NOT_VALIDATED;
    }

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (layer->mCompositionType == MESON_COMPOSITION_CLIENT) {
            MESON_LOGV("only non client composition type support skip validate");
            return HWC2_ERROR_NOT_VALIDATED;
        }
    }

    if (mOutsideChanged) {
        MESON_LOGV("for every outside hwc's changes, need do validate first");
        return HWC2_ERROR_NOT_VALIDATED;
    }

    if (mPresentLayers.size() == mLayers.size()) {
        for (auto at = mLayers.begin(); at != mLayers.end(); at++) {
            std::shared_ptr<Hwc2Layer> curLayer = at->second;
            if (curLayer->isUpdated()) {
                MESON_LOGV("layer (%" PRIu64 ") info changed",curLayer->getUniqueId());
                return HWC2_ERROR_NOT_VALIDATED;
            }
        }
    } else {
        MESON_LOGV("layer size changed, need validate first");
        return HWC2_ERROR_NOT_VALIDATED;
    }
    return HWC2_ERROR_NONE;
}

bool Hwc2Display::isLayerValidate(std::shared_ptr<DrmFramebuffer> & fb) {
    /* check the layer without bufferhandle */
    if (fb->mBufferHandle == NULL && fb->mFbType != DRM_FB_COLOR ) {
        return false;
    }
    return true;
}

hwc2_error_t Hwc2Display::presentDisplay(int32_t* outPresentFence) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);

    if (mFirstPresent) {
        mFirstPresent = false;
        mCrtc->closeLogoDisplay();
    }

    if (mExpectedPresentTime > 0) {
        nsecs_t now = systemTime();
        hwc2_vsync_period_t period = 0;
        getDisplayVsyncPeriod(&period);
        /* not present until the expected time meet */
        if ((mExpectedPresentTime > now + period) &&
                (mExpectedPresentTime < now + MAX_FRAME_DELAY * period)) {
            usleep(ns2us(mExpectedPresentTime - now - period));
        }
    }

    if (mValidateDisplay == false) {
        hwc2_error_t err = presentSkipValidateCheck();
        if (err != HWC2_ERROR_NONE) {
            MESON_LOGV("presentDisplay without validateDisplay err(%d)",err);
            return err;
        }

        MESON_LOGV("present skip validate");
        mCompositionStrategy->updateComposition();

        /*dump at skip validate display, for we need check by some composition info.*/
        bool dumpLayers = false;
        if (DebugHelper::getInstance().logCompositionDetail()) {
            MESON_LOGE("***CompositionFlow (%s):\n", __func__);
            dumpLayers = true;
        } else if (mFailedDeviceComp) {
            MESON_LOGE("***MonitorFailedDeviceComposition: \n");
            dumpLayers = true;
        }
        if (dumpLayers) {
            String8 layersDump;
            dumpPresentLayers(layersDump);
            MESON_LOGE("%s", layersDump.c_str());
        }
    }
    mValidateDisplay = false;

    if (mSkipComposition) {
        *outPresentFence = -1;
    } else {
        if (mPresentFence >= 0)
            close(mPresentFence);
        mPresentFence = -1;

        /*1.client target is always full screen, just post to crtc display axis.
         *2.client target may not update while display frame need changed.
         * When app not updated, sf won't update client target, but hwc
         * need display frame. so always update it before commit.
         */
        // tmp workaround for smpte
        int32_t offset = strstr(mDisplayMode.name, "smpte") ? 1 : 0;

        if (mClientTarget.get()) {
            mClientTarget->mDisplayFrame.left = mCalibrateInfo.crtc_display_x;
            mClientTarget->mDisplayFrame.top = mCalibrateInfo.crtc_display_y + offset;
            mClientTarget->mDisplayFrame.right = mCalibrateInfo.crtc_display_x +
                mCalibrateInfo.crtc_display_w;
            mClientTarget->mDisplayFrame.bottom = mCalibrateInfo.crtc_display_y +
                mCalibrateInfo.crtc_display_h - offset;
        }

        /*start new pageflip, and prepare.*/
        if (mCrtc->prePageFlip() != 0 ) {
            return HWC2_ERROR_NO_RESOURCES;
        }

        /*Start to compose, set up plane info.*/
        {
            std::lock_guard<std::mutex> vtLock(mVtMutex);
            if (mPresentCompositionStg->commit() != 0) {
                return HWC2_ERROR_NONE;
            }
        }
        /*set hdr metadata info.*/
        for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
            if ((*it)->mHdrMetaData.empty() == false) {
                mCrtc->setHdrMetadata((*it)->mHdrMetaData);
                break;
            }
        }

        /* Page flip */
        if (mCrtc->pageFlip(mPresentFence) < 0) {
            return HWC2_ERROR_UNSUPPORTED;
        }

        if (mWhiteBoardMode) {
            int32_t fence = ::dup(mPresentFence);
            mPresentCompositionStg->setReleaseFence(fence);
        }

        if (mPostProcessor != NULL) {
            int32_t displayFence = ::dup(mPresentFence);
            mPostProcessor->setKeystoneConfigs(mKeystoneConfigs);
            mPostProcessor->present(mProcessorFlags, displayFence);
            mProcessorFlags = 0;
        }

        /*need use in getReleaseFence() later, dup to return to sf.*/
        *outPresentFence = ::dup(mPresentFence);
    }

    /* reset layer flag to false */
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (!layer->isVirtualLayer()) {
            layer->clearUpdateFlag();
        }
    }

    /*dump debug informations.*/
    bool dumpComposition = false;
    if (DebugHelper::getInstance().logCompositionDetail()) {
        MESON_LOGE("***CompositionFlow (%s):\n", __func__);
        dumpComposition = true;
    } else if (mFailedDeviceComp) {
        MESON_LOGE("***MonitorFailedDeviceComposition: \n");
        dumpComposition = true;
    }
    if (dumpComposition) {
        String8 compDump;
        mPresentCompositionStg->dump(compDump);
        MESON_LOGE("%s", compDump.c_str());
        if (mClientTarget.get()) {
            MESON_LOGD("clientTarget dispFrame:(%d,%d,%d,%d)",
                    mClientTarget->mDisplayFrame.left,
                    mClientTarget->mDisplayFrame.top,
                    mClientTarget->mDisplayFrame.right,
                    mClientTarget->mDisplayFrame.bottom);
        }
    }


    return HWC2_ERROR_NONE;
}

/*getReleaseFences is return the fence for previous frame, for DPU based
composition, it is reasonable, current frame's present fence is the release
fence for previous frame.
But for m2m composer, there is no present fence, only release fence for
current frame, need do special process to return it in next present loop.*/
hwc2_error_t Hwc2Display::getReleaseFences(uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outFences) {
    ATRACE_CALL();
    uint32_t num = 0;
    bool needInfo = false;
    if (outLayers && outFences)
        needInfo = true;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
            if (isLayerValidate(*it) == false)
                continue;
            Hwc2Layer *layer = (Hwc2Layer*)(it->get());
            if (layer->isVirtualLayer()) {
                continue;
            }

            num++;
            if (needInfo) {
                int32_t releaseFence = layer->getPrevReleaseFence();
                if (releaseFence == -1)
                    *outFences = ::dup(mPresentFence);
                else
                    *outFences = releaseFence;
                *outLayers = layer->getUniqueId();
                outLayers++;
                outFences++;
            }
        }
    }

    *outNumElements = num;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setClientTarget(buffer_handle_t target,
    int32_t acquireFence, int32_t dataspace, hwc_region_t damage) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    /*create DrmFramebuffer for client target.*/
    mClientTarget =  std::make_shared<DrmFramebuffer>(
        target, acquireFence);
    mClientTarget->mFbType = DRM_FB_SCANOUT;
    mClientTarget->mBlendMode = DRM_BLEND_MODE_PREMULTIPLIED;
    mClientTarget->mPlaneAlpha = 1.0f;
    mClientTarget->mTransform = 0;
    mClientTarget->mDataspace = dataspace;

    /*set framebuffer to client composer.*/
    std::shared_ptr<IComposer> clientComposer =
        mComposers.find(MESON_CLIENT_COMPOSER)->second;
    if (clientComposer.get() &&
        clientComposer->setOutput(mClientTarget, damage) == 0) {
        return HWC2_ERROR_NONE;
    }

    return HWC2_ERROR_BAD_PARAMETER;
}

hwc2_error_t  Hwc2Display::getDisplayConfigs(
    uint32_t* outNumConfigs,
    hwc2_config_t* outConfigs) {
    if (mModeMgr != NULL) {
        return (hwc2_error_t)mModeMgr->getDisplayConfigs(outNumConfigs, outConfigs);
    } else {
        MESON_LOGE("Hwc2Display getDisplayConfigs (%s) miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t  Hwc2Display::getDisplayAttribute(
    hwc2_config_t config,
    int32_t attribute,
    int32_t* outValue) {
    if (mModeMgr != NULL) {
        return (hwc2_error_t)mModeMgr->getDisplayAttribute(
            config, attribute, outValue, CALL_FROM_SF);
    } else {
        MESON_LOGE("Hwc2Display (%s) getDisplayAttribute miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::getActiveConfig(
    hwc2_config_t* outConfig) {
    if (mModeMgr != NULL) {
        return (hwc2_error_t)mModeMgr->getActiveConfig(outConfig, CALL_FROM_SF);
    } else {
        MESON_LOGE("Hwc2Display (%s) getActiveConfig miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::setActiveConfig(hwc2_config_t config) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    if (mModeMgr != NULL) {
        /* set to the same activeConfig */
        hwc2_config_t activeCurr;
        if (mModeMgr->getActiveConfig(&activeCurr) == HWC2_ERROR_NONE) {
            if (config == activeCurr)
                return HWC2_ERROR_NONE;
        }

        mSeamlessSwitch = mModeMgr->isSeamlessSwitch(config);
        std::unique_lock<std::mutex> stateLock(mStateLock);
        mModeChanged = true;
        /*  need first blank display then change mode for nonseamless switch */
        if (!mSeamlessSwitch) {
            blankDisplayLocked();
        }
        int ret = mModeMgr->setActiveConfig(config);

        /*
         * seamless mode swith has no mode change uevent
         * we need update the vsync thread period
         */
        if (mSeamlessSwitch) {
            hwc2_vsync_period_t period = 1e9 / 60;
            getDisplayVsyncPeriod(&period);
            if (mVsync)
                mVsync->setPeriod(period);
            if (mVtVsync)
                mVtVsync->setPeriod(period);
        }

        /* wait when the display start refresh at the new config */
        if (!mSeamlessSwitch && mModeChanged) {
            mStateCondition.wait_for(stateLock, std::chrono::seconds(3));
        }

        return (hwc2_error_t) ret;
    } else {
        MESON_LOGE("Display (%s) setActiveConfig miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::setPerferredMode(std::string mode) {
    if (mModeMgr != NULL) {
        return (hwc2_error_t)mModeMgr->setPerferredMode(mode);
    } else {
        MESON_LOGE("Hwc2Display (%s) setPerferredMode miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::getCurrentSupportDeepColor(std::string& color) {
    if (mModePolicy != NULL) {
        return (hwc2_error_t)mModePolicy->getCurrentSupportDeepColor(color);
    } else {
        MESON_LOGE("Display (%s) get current support deep color, but miss mode policy",
                getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::setColorSpace(std::string colorspace) {
    if (mModePolicy != NULL) {
        return (hwc2_error_t)mModePolicy->setColorSpace(colorspace);
    } else {
        MESON_LOGE("Hwc2Display (%s) setColorSpace miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::clearUserDisplayConfig() {
    if (mModePolicy != NULL) {
        return (hwc2_error_t)mModePolicy->clearUserDisplayConfig();
    } else {
        MESON_LOGE("Hwc2Display (%s) clearUserDisplayConfig miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::setDvMode(std::string dv_mode) {
    if (mModePolicy != NULL) {
        return (hwc2_error_t)mModePolicy->setDvMode(dv_mode);
    } else {
        MESON_LOGE("Hwc2Display (%s) setDvMode miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

bool Hwc2Display::isLayerHideForDebug(hwc2_layer_t id) {
    if (DebugHelper::getInstance().disableRefresh())
        return true;

    std::vector<int> hideLayers;
    DebugHelper::getInstance().getHideLayers(hideLayers);
    if (hideLayers.empty())
        return false;

    for (auto it = hideLayers.begin(); it < hideLayers.end(); it++) {
        if (*it == (int)id)
            return true;
    }

    return false;
}

hwc2_error_t Hwc2Display::getDisplayCapabilities(
            uint32_t* outNumCapabilities, uint32_t* outCapabilities) {
    if (outCapabilities == nullptr) {
        if (mConnector->isConnected() && mConnector->isTvSupportALLM())
            *outNumCapabilities = 2;
        else
            *outNumCapabilities = 1;
    } else {
        if ((mConnector->isConnected() && mConnector->isTvSupportALLM())) {
            outCapabilities[0] = HWC2_DISPLAY_CAPABILITY_SKIP_CLIENT_COLOR_TRANSFORM;
            outCapabilities[1] = HWC2_DISPLAY_CAPABILITY_AUTO_LOW_LATENCY_MODE;
        } else if (!mConnector->isConnected()) {
            outCapabilities[0] = HWC2_DISPLAY_CAPABILITY_INVALID;
        }
        else {
            outCapabilities[0] = HWC2_DISPLAY_CAPABILITY_SKIP_CLIENT_COLOR_TRANSFORM;
        }
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getDisplayVsyncPeriod(hwc2_vsync_period_t* outVsyncPeriod) {
    ATRACE_CALL();
    hwc2_config_t config;
    int32_t configPeriod;

    if (mModeMgr->getActiveConfig(&config) != HWC2_ERROR_NONE) {
        return HWC2_ERROR_BAD_CONFIG;
    }

    if (mModeMgr->getDisplayAttribute(config,
            HWC2_ATTRIBUTE_VSYNC_PERIOD, &configPeriod) != HWC2_ERROR_NONE) {
        return HWC2_ERROR_BAD_CONFIG;
    }

    *outVsyncPeriod = configPeriod;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setActiveConfigWithConstraints(hwc2_config_t config,
        hwc_vsync_period_change_constraints_t* vsyncPeriodChangeConstraints,
        hwc_vsync_period_change_timeline_t* outTimeline) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    MESON_LOGV("%s config:%d", __func__, config);
    bool validConfig = false;
    uint32_t arraySize = 0;

    if (mModeMgr->getDisplayConfigs(&arraySize, nullptr) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;

    std::vector<hwc2_config_t> outConfigs;
    outConfigs.resize(arraySize);
    if (mModeMgr->getDisplayConfigs(&arraySize, outConfigs.data()) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;
    for (auto it = outConfigs.begin(); it != outConfigs.end(); ++it) {
        if (*it == config) {
            validConfig = true;
            break;
        }
    }
    /* not valid config */
    if (!validConfig)
        return HWC2_ERROR_BAD_CONFIG;

    mSeamlessSwitch = mModeMgr->isSeamlessSwitch(config);
    if (vsyncPeriodChangeConstraints->seamlessRequired && !mSeamlessSwitch) {
        return HWC2_ERROR_SEAMLESS_NOT_ALLOWED;
    }

    int64_t desiredTimeNanos = vsyncPeriodChangeConstraints->desiredTimeNanos;

    outTimeline->refreshRequired = false;
    hwc2_config_t activeConfig;
    if (mModeMgr->getActiveConfig(&activeConfig) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;

    if (activeConfig != config) {
        std::unique_lock<std::mutex> stateLock(mStateLock);
        mModeChanged = true;
        /*  need first blank display then change mode for nonseamless switch */
        if (!mSeamlessSwitch) {
            blankDisplayLocked();
        }

        int ret = mModeMgr->setActiveConfig(config);
        /*
         * seamless mode swith has no mode change uevent
         * we need update the vsync thread period
         */
        if (mSeamlessSwitch) {
            hwc2_vsync_period_t period = 1e9 / 60;
            getDisplayVsyncPeriod(&period);
            if (mVsync)
                mVsync->setPeriod(period);
            if (mVtVsync)
                mVtVsync->setPeriod(period);
        }

        /* wait when the display start refresh at the new config */
        if (!mSeamlessSwitch && mModeChanged) {
            mStateCondition.wait_for(stateLock, std::chrono::seconds(3));
        }

        int32_t configPeriod;
        if (mModeMgr->getDisplayAttribute(config, HWC2_ATTRIBUTE_VSYNC_PERIOD, &configPeriod)
                != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;

        nsecs_t vsyncTimestamp;
        nsecs_t vsyncPeriod;
        /* wait 3 vblank to confirm vsync have updated successfully */
        for (int i = 0; i < 3; i++) {
            mVsync->waitVsync(vsyncTimestamp, vsyncPeriod);
            /* vsync in a reasonable value 0.5 ms */
            if (abs(vsyncPeriod - configPeriod) < 500 * 1000)
                break;
        }

        /* not return until the desired time reach */
        nsecs_t now = systemTime(CLOCK_MONOTONIC);
        if (now < desiredTimeNanos)
            usleep(ns2us(desiredTimeNanos - now));

        now = systemTime(CLOCK_MONOTONIC);
        outTimeline->newVsyncAppliedTimeNanos =
            (now - desiredTimeNanos >= seconds_to_nanoseconds(1)) ?  desiredTimeNanos : now;

        /* notify vsync timing period changed*/
        hwc_vsync_period_change_timeline_t vsyncTimeline;
        vsyncTimeline.newVsyncAppliedTimeNanos = vsyncTimestamp;
        vsyncTimeline.refreshRequired = false;
        vsyncTimeline.refreshTimeNanos = vsyncTimestamp + vsyncPeriod;
        onVsyncPeriodTimingChanged(&vsyncTimeline);

        return (hwc2_error_t) ret;
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setAutoLowLatencyMode(bool enabled) {
    if (mConnector->isConnected() == false) {
        return HWC2_ERROR_UNSUPPORTED;
    } else {
        // disable post processor and trigger validate display
        MESON_LOGD("%s = %d", __func__, enabled);
        mIsDisablePostProcessor = enabled;
        mOutsideChanged = true;
        if (mModePolicy) {
            return (hwc2_error_t)mModePolicy->setAutoLowLatencyMode(enabled);
        }
        return (hwc2_error_t)mConnector->setAutoLowLatencyMode(enabled);
    }
}

hwc2_error_t Hwc2Display::getSupportedContentTypes(uint32_t* outNum, uint32_t* outSupportedContentTypes) {
    *outNum = mSupportedContentTypes.size();
    if (outSupportedContentTypes != nullptr) {
        for (uint32_t i = 0; i < mSupportedContentTypes.size(); i++) {
            outSupportedContentTypes[i] = mSupportedContentTypes[i];
        }
    }
    return HWC2_ERROR_NONE;
}

bool Hwc2Display::checkIfContentTypeIsSupported(uint32_t contentType) {
    // ContentType::NONE (=0) is always supported.
    if (contentType == 0) {
        return true;
    }
    for (int i = 0; i < mSupportedContentTypes.size(); i++) {
        if (mSupportedContentTypes[i] == contentType) {
            return true;
        }
    }
    return false;
}

hwc2_error_t Hwc2Display::setContentType(uint32_t contentType) {
    bool supported = checkIfContentTypeIsSupported(contentType);

    if (!supported) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    if (mConnector->setContentType(contentType))
        return HWC2_ERROR_UNSUPPORTED;

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setBootConfig(uint32_t config) {
    if (!mModeMgr.get())
        return HWC2_ERROR_UNSUPPORTED;

    int32_t ret = mModeMgr->setBootConfig(config);
    if (ret == HWC2_ERROR_NONE)
        mBootConfig = config;

    return (hwc2_error_t) ret;
}

hwc2_error_t Hwc2Display::clearBootConfig() {
    if (!mModeMgr.get())
        return HWC2_ERROR_UNSUPPORTED;

    int32_t ret = mModeMgr->clearBootConfig();
    return (hwc2_error_t) ret;
}

hwc2_error_t Hwc2Display::getPreferredBootConfig(int32_t* outConfig) {
    if (!mModeMgr.get())
        return HWC2_ERROR_UNSUPPORTED;

    // If connector was  out, then we only report the previous active mode to framework
    if (mPowerMode->getConnectorMode() == MESON_POWER_CONNECTOR_OUT) {
        hwc2_config_t config = 0;
        auto ret = getActiveConfig(&config);
        *outConfig = (int32_t) config;
        return ret;
    }

    int32_t ret = mModeMgr->getPreferredBootConfig(outConfig);
    return (hwc2_error_t) ret;
}

hwc2_error_t Hwc2Display::getPhysicalOrientation(int32_t* outOrientation __unused) {
    *outOrientation = HWC3_TRANSFORM_NONE;
    return HWC2_ERROR_NONE;
}

//TODO: implement expectedPresent time logic
hwc2_error_t Hwc2Display::setExpectedPresentTime(int64_t expectedPresentTime) {
    std::lock_guard<std::mutex> lock(mMutex);
    mExpectedPresentTime = expectedPresentTime;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setAidlClientPid(int32_t pid) {
    /* we need tall uvm pid of surfaceflinger */
    UvmDev::getInstance().setPid(pid);
    mAidlService = true;
    return HWC2_ERROR_NONE;
}

/* hwc 3.2 */
hwc2_error_t Hwc2Display::getOverlaySupport(uint32_t* numElements,
                uint32_t* pixelFormats) {
    std::vector<uint32_t> tempPixelFormats;
    mCrtc->getPixelFormats(tempPixelFormats);
    *numElements = tempPixelFormats.size();

    if (pixelFormats != nullptr) {
        for (auto i = 0; i < tempPixelFormats.size(); i++) {
            pixelFormats[i] = tempPixelFormats[i];
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getHdrConversionCapabilities(uint32_t* outNumCapability,
                drm_hdr_conversion_capability_t* outConversionCapability) {
    MESON_LOGD(" %s ",__func__);

    mCrtc->getConversionCaps(mHdrConversionCaps);
    *outNumCapability = mHdrConversionCaps.size();

    if (outConversionCapability != NULL) {
        for (auto i = 0; i < mHdrConversionCaps.size(); i++) {
            outConversionCapability[i] = mHdrConversionCaps[i];
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setHdrConversionStrategy(bool passThrough, uint32_t numElements,
             bool isAuto, uint32_t* autoAllowedHdrTypes, uint32_t* preferredHdrOutputType) {
    // Hdr conversion strategy switch need change display mode. During the mode change,
    // we will first blank display then change display mode. As blank display will hold mMutes,
    // so we don't hold mMutex here
    // TODO: during HDR conver should not send hotplug event to framework.

    MESON_LOGD("%s passThrough %d isAuto %d ",__func__, passThrough, isAuto);

    auto outHdrConversionType = -1;
    uint32_t userHdrType = 0;
    /* DV enable support DV and HDR10
     * DV disable support HDR10 and HLG */
    if (!passThrough && preferredHdrOutputType) {
        std::vector<uint32_t> HdrTypes(autoAllowedHdrTypes, autoAllowedHdrTypes+numElements);
        for (std::vector<uint32_t>::const_iterator iter = HdrTypes.begin(); iter != HdrTypes.end(); ++iter) {
            MESON_LOGD("%s HdrTypes %d \n",__func__, *iter);
        }

        bool containDVType =
                 std::find(HdrTypes.begin(), HdrTypes.end(), HAL_HDR_DOLBY_VISION) != HdrTypes.end();
        bool containHDR10Type =
                 std::find(HdrTypes.begin(), HdrTypes.end(), HAL_HDR_HDR10) != HdrTypes.end();
        bool containHLGType =
                 std::find(HdrTypes.begin(), HdrTypes.end(), HAL_HDR_HLG) != HdrTypes.end();
        bool containSDRType =
                 std::find(HdrTypes.begin(), HdrTypes.end(), DRM_INVALID) != HdrTypes.end();

        userHdrType = userHdrType | (containDVType << HAL_HDR_DOLBY_VISION);
        userHdrType = userHdrType | (containHDR10Type << HAL_HDR_HDR10);
        userHdrType = userHdrType | (containHLGType << HAL_HDR_HLG);
        userHdrType = userHdrType | (containSDRType << DRM_INVALID);

        mModePolicy->setAllowedHdrTypes(userHdrType, isAuto, passThrough);
        outHdrConversionType = mModePolicy->getPreferredHdrConversionType();

        if (outHdrConversionType == -1) {
            if (mModePolicy) {
                mModePolicy->setHdrConversionPolicy(passThrough, outHdrConversionType);
            } else {
                MESON_LOGD("ModePolicy is NULL");
            }
            return HWC2_ERROR_UNSUPPORTED;
        }
        *preferredHdrOutputType = outHdrConversionType;
    } else {
        userHdrType = userHdrType | (1 << HAL_HDR_DOLBY_VISION);
        userHdrType = userHdrType | (1 << HAL_HDR_HDR10);
        userHdrType = userHdrType | (1 << HAL_HDR_HLG);
        userHdrType = userHdrType | (1 << DRM_INVALID);
        mModePolicy->setAllowedHdrTypes(userHdrType, isAuto, passThrough);
    }

    int32_t ret = -1;
    if (mModePolicy) {
        std::unique_lock<std::mutex> stateLock(mStateLock);
        mModeMgr->resetTags(false);
        ret = mModePolicy->setHdrConversionPolicy(passThrough, outHdrConversionType);
        if (ret == 0) {
            mStateCondition.wait_for(stateLock, std::chrono::seconds(1));
        }
        mModeMgr->resetTags(true);
    } else {
        MESON_LOGD("ModePolicy is NULL");
    }

    return ret > 0 ? HWC2_ERROR_UNSUPPORTED : HWC2_ERROR_NONE;
}

bool Hwc2Display::hasVideoLayerPresent() {
    int videoLayers = 0;
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        drm_fb_type_t type = (*it)->mFbType;
        if (type == DRM_FB_VIDEO_UVM_DMA || type == DRM_FB_VIDEO_OVERLAY)
            videoLayers++;
    }

    return (videoLayers > 0);
}

// when there is nontunnel video playback, change vsync to MixMode
int32_t Hwc2Display::adjustVsyncMode() {
    /* we have nontunnel video now */
    if (hasVideoLayerPresent()) {
        /* we fist have it */
        if (!mHasVideoPresent) {
            MESON_LOGD("%s to MixMode", __func__);
            mVsync->setMixMode(mCrtc);
        }
        mHasVideoPresent = true;
    } else {
        if (mHasVideoPresent) {
            if (HwcConfig::softwareVsyncEnabled()) {
                mVsync->setSoftwareMode();
                MESON_LOGD("%s to SwMode", __func__);
            } else {
                mVsync->setHwMode(mCrtc);
                MESON_LOGD("%s to HwMode", __func__);
            }
        }
        mHasVideoPresent = false;
    }

    return 0;
}

void Hwc2Display::dumpPresentLayers(String8 & dumpstr) {
    dumpstr.append("-----------------------------------------------------------"
        "-------------------------------------------\n");
    dumpstr.append("|  id  |  z  |    type    |blend| alpha  |t|"
        "  AFBC  |    Source Crop    |    Display Frame  |tunnelId|\n");
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
        drm_rect_t sourceCrop = layer->getSourceCrop();
        drm_rect_t displayFrame = layer->getDisplayFrame();

        dumpstr.append("+------+-----+------------+-----+--------+-+--------+"
            "-------------------+-------------------+--------+\n");
        dumpstr.appendFormat("|%6" PRIu64 "|%5d|%12s|%5d|%8f|%1d|%8x|%4d %4d %4d %4d"
            "|%4d %4d %4d %4d|%8d|\n",
            layer->getUniqueId(),
            layer->mZorder,
            drmFbTypeToString(layer->mFbType),
            layer->mBlendMode,
            layer->mPlaneAlpha,
            layer->mTransform,
            layer->isSidebandBuffer() ? 0 : am_gralloc_get_vpu_afbc_mask(layer->mBufferHandle),
            sourceCrop.left,
            sourceCrop.top,
            sourceCrop.right,
            sourceCrop.bottom,
            displayFrame.left,
            displayFrame.top,
            displayFrame.right,
            displayFrame.bottom,
            layer->getVideoTunnelId()
            );
    }
    dumpstr.append("----------------------------------------------------------"
        "--------------------------------------------\n");
}

void Hwc2Display::dumpHwDisplayPlane(String8 &dumpstr) {
    dumpstr.append("HwDisplayPlane Info:\n");
    dumpstr.append("------------------------------------------------------------"
            "-----------------------------------------------------------------\n");
    dumpstr.append("|  ID   |Zorder| type |     source crop     |      dis Frame"
            "      | fd | fm | b_st | p_st | blend | alpha |  op  | afbc fm  |\n");
    dumpstr.append("+-------+------+------+---------------------+-----------------"
            "----+----+----+------+------+-------+-------+------+----------+\n");

    /* dump osd plane */
    for (auto  it = mPresentPlanes.begin(); it != mPresentPlanes.end(); it++) {
        std::shared_ptr<HwDisplayPlane> plane = *it;
        if (!strncmp("OSD", plane->getName(), 3)) {
            plane->dump(dumpstr);
        }
    }

    if (mPostProcessor != NULL) {
        mPostProcessor-> dumpPlane(dumpstr);
    }

    dumpstr.append("------------------------------------------------------------"
            "-----------------------------------------------------------------\n");
    dumpstr.append("\n");
}

void Hwc2Display::dump(String8 & dumpstr) {
    std::lock_guard<std::mutex> lock(mMutex);
    /*update for debug*/
    if (DebugHelper::getInstance().debugHideLayers() ||
        DebugHelper::getInstance().debugPlanes()) {
        //ask for auto refresh for debug layers update.
        if (mObserver != NULL) {
            mObserver->refresh();
        }
    }
    if (mModePolicy)
        mModePolicy->dump(dumpstr);
    /*dump*/
    dumpstr.append("---------------------------------------------------------"
        "-----------------------------\n");
    dumpstr.appendFormat("Display (%s, %s) \n",
        getName(), mForceClientComposer ? "Client-Comp" : "HW-Comp");
    dumpstr.appendFormat("Power: (%d-%d) \n",
        mPowerMode->getConnectorMode(), mPowerMode->getScreenStatus());
    /*calibration info*/
    dumpstr.appendFormat("Calibration: (%dx%d)->(%dx%d,%dx%d)\n",
        mCalibrateInfo.framebuffer_w, mCalibrateInfo.framebuffer_h,
        mCalibrateInfo.crtc_display_x, mCalibrateInfo.crtc_display_y,
        mCalibrateInfo.crtc_display_w, mCalibrateInfo.crtc_display_h);

    /* HDR info */
    if (mConnector)
        dumpstr.appendFormat("HDR current type: %s\n",
                mConnector->getCurrentHdrType().c_str());
    /* max supported DV mode */
    std::string mode;
    mConnector->getDvCap(mode);
    if (!mode.empty())
        dumpstr.appendFormat("Max supported DV mode: %s\n", mode.c_str());

    dumpstr.append("HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%d\n",
        mHdrCaps.DolbyVisionSupported ?  1 : 0);
#if (PLATFORM_SDK_VERSION >= 34)
    dumpstr.appendFormat("    DOLBY_VISION_4K30=%d\n",
        mHdrCaps.DOLBY_VISION_4K30_Supported ? 1 : 0);
#endif
    dumpstr.appendFormat("    HLG=%d\n",
        mHdrCaps.HLGSupported ?  1 : 0);
    dumpstr.appendFormat("    HDR10=%d, HDR10+=%d, "
        "maxLuminance=%d, avgLuminance=%d, minLuminance=%d\n",
        mHdrCaps.HDR10Supported ? 1 : 0,
        mHdrCaps.HDR10PlusSupported ? 1 : 0,
        mHdrCaps.maxLuminance,
        mHdrCaps.avgLuminance,
        mHdrCaps.minLuminance);

    dumpstr.append("\n");

    /* dump display configs*/
    mModeMgr->dump(dumpstr);
    dumpstr.append("\n");
    mVsync->dump(dumpstr);
    dumpstr.appendFormat("    VsyncTimestamp:%" PRId64 " ns\n", mVsyncTimestamp);
    dumpstr.append("\n");

    /* dump present layers info*/
    dumpstr.append("Present layers:\n");
    dumpPresentLayers(dumpstr);
    dumpstr.append("\n");

    /* dump composition stragegy.*/
    if (mPresentCompositionStg) {
        mPresentCompositionStg->dump(dumpstr);
        dumpstr.append("\n");
    }

    /*dump detail debug info*/
    if (DebugHelper::getInstance().dumpDetailInfo()) {
        /* dump composers*/
        dumpstr.append("Valid composers:\n");
        for (auto it = mPresentComposers.begin(); it != mPresentComposers.end(); it++) {
            dumpstr.appendFormat("%s-Composer (%p)\n", it->get()->getName(), it->get());
        }
        dumpstr.append("\n");

        if (mConnector)
            mConnector->dump(dumpstr);

        if (mCrtc)
            mCrtc->dump(dumpstr);

        dumpHwDisplayPlane(dumpstr);
    }

    dumpstr.append("\n");
    if (mPostProcessor != NULL) {
        mPostProcessor-> dump(dumpstr);
    }
}

int32_t Hwc2Display::captureDisplayScreen(buffer_handle_t hnd) {
    int ret = -1;
    std::shared_ptr<DrmFramebuffer> capBuffer;

    ALOGD("hwc2Display:: captureDisplayScreen");
    if (mPostProcessor && hnd) {
        capBuffer = std::make_shared<DrmFramebuffer>(hnd, -1);
        ret = mPostProcessor->getScreencapFb(capBuffer);
        capBuffer.reset();
    }

    return ret;
}

bool Hwc2Display::getDisplayVsyncAndPeriod(int64_t& timestamp, int32_t& vsyncPeriodNanos) {
    ATRACE_CALL();
    timestamp = mVsyncTimestamp;

    /* default set to 16.667 ms */
    hwc2_vsync_period_t period = 1e9 / 60;
    hwc2_error_t  ret = getDisplayVsyncPeriod(&period);

    if (mFRPeriodNanos == 0) {
        vsyncPeriodNanos = period;
    } else {
        // has frame rate hint, return it
        vsyncPeriodNanos = mFRPeriodNanos;
    }

    return ret == HWC2_ERROR_NONE;
}

bool Hwc2Display::isDisplayConnected() {
    if (mConnector->getType() == DRM_MODE_CONNECTOR_VIRTUAL)
        return false;

    return mConnector ? mConnector->isConnected() : false;
}

std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>> Hwc2Display::getAllLayers() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mLayers;
}

int32_t Hwc2Display::setModePolicy(std::shared_ptr<IModePolicy> policy) {
    mModePolicy = policy;
    mModePolicy->bindConnector(mConnector);

    mModeMgr->setModePolicy(policy);
    return 0;
}

bool Hwc2Display::setFrameRateHint(std::string value) {
    if (!value.compare("0")) {
        mFRPeriodNanos = 0;
    } else {
        mFRPeriodNanos = 1e9 * 100 / std::stoi(value);
    }

    MESON_LOGD("%s value:%s", __func__, value.c_str());
    hwc2_vsync_period_t period = 1e9 / 60;
    getDisplayVsyncPeriod(&period);
    period = mFRPeriodNanos == 0 ? period : mFRPeriodNanos;
    MESON_LOGD("%s setPeriod to %d", __func__, period);
    if (mVtVsync.get())
        mVtVsync->setPeriod(period);
    if (mVsync.get())
        mVsync->setPeriod(period);

    return true;
}

int32_t Hwc2Display::getBootConfig(int32_t & config) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    if (mBootConfig != -1) {
        config = mBootConfig;
        return HWC2_ERROR_NONE;
    }

    if (!mConnector)
        return HWC2_ERROR_BAD_PARAMETER;

    const char *defaultMode = UBOOTENV_OUTPUTMODE;
    std::string modeName;
    if (sc_read_bootenv(defaultMode, modeName) != 0) {
        return HWC2_ERROR_BAD_PARAMETER;
    }

    /* find the mode info from connector */
    std::map<uint32_t, drm_mode_info_t> modes;
    drm_mode_info_t mode;
    bool validMode = false;
    mConnector->getModes(modes);
    for (auto it = modes.begin(); it != modes.end(); ++it) {
        if (modeName.compare(it->second.name) == 0) {
            mode = it->second;
            validMode = true;
            break;
        }
    }

    if (!validMode) {
        return HWC2_ERROR_BAD_PARAMETER;
    }

    /* find the configId of the boot display mode */
    uint32_t arraySize = 0;
    if (getDisplayConfigs(&arraySize, nullptr) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;

    std::vector<hwc2_config_t> outConfigs;
    outConfigs.resize(arraySize);
    if (getDisplayConfigs(&arraySize, outConfigs.data()) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;
    for (auto it = outConfigs.begin(); it != outConfigs.end(); ++it) {
        /* get the display mode info */
        int32_t width = -1;
        int32_t height = -1;
        int32_t vsyncPeriod = -1;
        int32_t groupId = -1;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_WIDTH, &width) != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_HEIGHT, &height) != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_VSYNC_PERIOD, &vsyncPeriod) != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_CONFIG_GROUP, &groupId)
                != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;

        if (mode.pixelW == width && mode.pixelH == height && mode.groupId == groupId) {
            /* compare refresh rate */
            if (vsyncPeriod == static_cast<int32_t> (1e9/mode.refreshRate)) {
                config = *it;
                return HWC2_ERROR_NONE;
            }
        }
    }

    return HWC2_ERROR_BAD_PARAMETER;
}

int32_t Hwc2Display::getFrameRateConfigId(int32_t &config, const float frameRate) {
    if (frameRate < 0 || frameRate > 120)
        return HWC2_ERROR_BAD_PARAMETER;

    // recovery find the default mode
    if (frameRate == 0)
        return getBootConfig(config);

    std::lock_guard<std::mutex> lock(mConfigMutex);
    /* get the current config and group id */
    hwc2_config_t currentConfig;
    if (mModeMgr->getActiveConfig(&currentConfig) != HWC2_ERROR_NONE) {
        return HWC2_ERROR_BAD_CONFIG;
    }

    int32_t configGroupId;
    if (mModeMgr->getDisplayAttribute(currentConfig, HWC2_ATTRIBUTE_CONFIG_GROUP, &configGroupId)
            != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;

    /* get display configs */
    uint32_t arraySize = 0;
    if (getDisplayConfigs(&arraySize, nullptr) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;
    std::vector<hwc2_config_t> outConfigs;
    outConfigs.resize(arraySize);
    if (getDisplayConfigs(&arraySize, outConfigs.data()) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;
    for (auto it = outConfigs.begin(); it != outConfigs.end(); ++it) {
        /* get the display mode info */
        int32_t vsyncPeriod = -1;
        int32_t groupId = -1;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_VSYNC_PERIOD, &vsyncPeriod) != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;
        if (getDisplayAttribute(*it, HWC2_ATTRIBUTE_CONFIG_GROUP, &groupId)
                != HWC2_ERROR_NONE)
            return HWC2_ERROR_BAD_CONFIG;

        /* find the mode */
        if ((std::abs(static_cast<float>(1e9/vsyncPeriod) - frameRate) < 0.001)
            && groupId == configGroupId) {
            config = *it;
            return HWC2_ERROR_NONE;
        }
    }

    return HWC2_ERROR_BAD_CONFIG;
}

// setFrameRate for MesonDisplay
// value: 0 means to recovery to default boot Config
int32_t Hwc2Display::setFrameRate(float value) {
    int32_t config;
    int32_t ret = getFrameRateConfigId(config, value);
    if (ret != HWC2_ERROR_NONE) {
        MESON_LOGD("%s could not find config of frameRate :%f", __func__, value);
        return ret;
    }

    MESON_LOGD("%s value:%f", __func__, value);
    mFRPeriodNanos = value == 0 ? 0 : 1e9 / value;
    setActiveConfig(config);

    return HWC2_ERROR_NONE;
}

bool Hwc2Display::needSwitchConnector() {
    if (!mConnector)
        return false;

    auto type = mConnector->getType();
    if (type == DRM_MODE_CONNECTOR_HDMIA ||
            type == DRM_MODE_CONNECTOR_VIRTUAL ||
            type == DRM_MODE_CONNECTOR_TV) {
        return true;
    }

    return false;
}

/*******************Video Tunnel API below*******************/
void Hwc2Display::updateVtBuffers() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    std::shared_ptr<Hwc2Layer> layer;

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        it->second->updateVtBuffer();
    }
}

hwc2_error_t Hwc2Display::presentVtVideo(int32_t* outPresentFence) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    /* videotunnel thread presentDisplay */
    *outPresentFence = -1;
    if (!mSkipComposition) {
         mCompositionStrategy->commitTunnelVideo();

        /*dump debug informations.*/
        bool dumpComposition = false;
        if (DebugHelper::getInstance().logCompositionDetail()) {
            MESON_LOGE("***CompositionFlow (%s):\n", __func__);
            dumpComposition = true;
        } else if (mFailedDeviceComp) {
            MESON_LOGE("***MonitorFailedDeviceComposition: \n");
            dumpComposition = true;
        }
        if (dumpComposition) {
            String8 compDump;
            mPresentCompositionStg->dump(compDump);
            MESON_LOGE("%s", compDump.c_str());
        }
    }
    return HWC2_ERROR_NONE;
}

void Hwc2Display::onFrameAvailable() {
    ATRACE_CALL();
    if (mVtDisplayThread)
        mVtDisplayThread->onFrameAvailableForGameMode();
}

void Hwc2Display::askSurfaceFlingerRefresh() {
    ATRACE_CALL();
    if (mObserver != NULL)
        mObserver->refresh();
}

void Hwc2Display::handleVtThread() {
    bool haveVtLayer = false;

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (layer->isVtBuffer()) {
            haveVtLayer = true;
            break;
        }
    }

    if (haveVtLayer) {
        if (!mVtDisplayThread) {
            mVtDisplayThread = std::make_shared<VtDisplayThread>(this);
        }
        if (!mVtVsyncStatus) {
            mVtVsync->setEnabled(true);
            mVtVsyncStatus = true;
            MESON_LOGD("%s, displayId:%d set video tunnel thread to Enabled",
                    __func__, mDisplayId);
        }
    } else {
        if (mVtDisplayThread && mVtVsyncStatus) {
            mVtVsync->setEnabled(false);
            mVtVsyncStatus = false;
            MESON_LOGD("%s, displayId:%d, set video tunnel thread to Disabled",
                    __func__, mDisplayId);
        }
    }
}

void Hwc2Display::setVtLayersPresentTime() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);

    hwc2_vsync_period_t period = 0;
    getDisplayVsyncPeriod(&period);
    period = mFRPeriodNanos == 0 ? period : mFRPeriodNanos;
    nsecs_t expectPresentedTime = mVsyncTimestamp + period;

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        // expectPresent time is current vsync timestamp + 1 vsyncPeriod
        it->second->setPresentTime(expectPresentedTime);
    }
}

void Hwc2Display::releaseVtLayers() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    int ret;
    std::shared_ptr<Hwc2Layer> layer;

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        layer = it->second;
        if (layer->isVtBuffer()) {
            ret = layer->releaseVtBuffer();
            if (ret != 0 && ret != -EAGAIN) {
                MESON_LOGE("%s, release layer id=%" PRIu64 " failed, ret=%s",
                        __func__, layer->getUniqueId(), strerror(ret));
            }
        }
    }
}

bool Hwc2Display::handleVtDisplayConnection() {
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    bool displayState = true;
    std::shared_ptr<Hwc2Layer> layer;

    if (mPowerMode->needBlankScreen(mPresentLayers.size())) {
        displayState = false;
    }

    MESON_LOGV("%s: displayId:%d displayState:%d",
            __func__, mDisplayId, displayState);
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        layer = it->second;
        if (layer->isVtBuffer()) {
            layer->handleDisplayDisconnect(displayState);
        }
    }

    return displayState;
}

/*
 * whether the videotunnel layer has new game buffer
 */
bool Hwc2Display::newGameBuffer() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        auto layer = it->second;
        if (layer && layer->newGameBuffer())
            return true;
    }

    return false;
}

/*
 * Refresh video tunnel layers. As mode change may blank the display once.
 */
void Hwc2Display::refreshVtLayers() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> vtLock(mVtMutex);
    refreshVtLayersLocked();
}

void Hwc2Display::refreshVtLayersLocked() {
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        auto layer = it->second;
        if (layer->isVtBuffer())
            layer->vtRefresh();
    }
}

void Hwc2Display::createCallbackThread(bool mode) {
    mEnableCallBack = mode;

    //Create thread for virtualdisplay
    if (!mInitWBDisplayThread) {
        handleWBThread();
        mInitWBDisplayThread = true;
    }

    mWBVsync->setEnabled(mode);
    mObserver->refresh();
    return;
}

void Hwc2Display::setReverseMode(int type) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        if ((*it)->getType() == OSD_PLANE) {
            (*it)->setReverseMode(type);
        }
    }

    meson_mode_set_ubootenv(REVERSE_CTL, "1");
    std::string uboot_osd_reverse;
    std::string uboot_video_reverse;

    //clear the history
    sysfs_set_string(AML_MEDIA_REVERSE, "0");
    switch (type) {
        case DRM_REVERSE_MODE_NONE:
            meson_mode_set_ubootenv(UBOOT_OSD_REVERSE, "0");
            meson_mode_set_ubootenv(UBOOT_VIDEO_REVERSE, "0");
            sysfs_set_string(VIDEO_REVERSE, "0");
            break;
        case DRM_REVERSE_MODE_X:
            meson_mode_set_ubootenv(UBOOT_OSD_REVERSE, "osd0,x_rev");
            meson_mode_set_ubootenv(UBOOT_VIDEO_REVERSE, "2");
            sysfs_set_string(VIDEO_REVERSE, "1");
            break;
        case DRM_REVERSE_MODE_Y:
            meson_mode_set_ubootenv(UBOOT_OSD_REVERSE, "osd0,y_rev");
            meson_mode_set_ubootenv(UBOOT_VIDEO_REVERSE, "3");
            sysfs_set_string(VIDEO_REVERSE, "2");
            break;
        case DRM_REVERSE_MODE_ALL:
            meson_mode_set_ubootenv(UBOOT_OSD_REVERSE, "osd0,true");
            meson_mode_set_ubootenv(UBOOT_VIDEO_REVERSE, "1");
            sysfs_set_string(AML_MEDIA_REVERSE, "1");
            break;
        default:
            MESON_LOGE("Unknown reverse type(%d)", type);
            break;
    }
    mObserver->refresh();
    return;
}

