/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <hardware/hwcomposer2.h>
#include <inttypes.h>

#include "Hwc2Display.h"
#include "Hwc2Layer.h"
#include "Hwc2Base.h"

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

Hwc2Display::Hwc2Display(std::shared_ptr<Hwc2DisplayObserver> observer) {
    mObserver = observer;
    mForceClientComposer = false;
    mPowerMode  = std::make_shared<HwcPowerMode>();
    mSignalHpd = false;
    mValidateDisplay = false;
    mVsyncState = false;
    mScaleValue = 1;
    mPresentFence = -1;
    memset(&mHdrCaps, 0, sizeof(mHdrCaps));
    memset(mColorMatrix, 0, sizeof(float) * 16);
    memset(&mCalibrateCoordinates, 0, sizeof(int) * 4);
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

    if (mPostProcessor != NULL)
        mPostProcessor->stop();
    mPostProcessor.reset();
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
    ComposerFactory::create(MESON_CLIENT_COMPOSER, composer);
    mComposers.emplace(MESON_CLIENT_COMPOSER, std::move(composer));
    ComposerFactory::create(MESON_DUMMY_COMPOSER, composer);
    mComposers.emplace(MESON_DUMMY_COMPOSER, std::move(composer));
    /*add yuv/video composer*/
    ComposerFactory::create(MESON_DI_COMPOSER, composer);
    mComposers.emplace(MESON_DI_COMPOSER, std::move(composer));

    initLayerIdGenerator();

    MESON_LOG_FUN_LEAVE();
    return 0;
}

int32_t Hwc2Display::setDisplayResource(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    MESON_LOG_FUN_ENTER();
    std::lock_guard<std::mutex> lock(mMutex);

    mCrtc = crtc;
    mPlanes = planes;
    mConnector = connector;

    /*update composition strategy.*/
    uint32_t strategyFlags = 0;
    int osdPlanes = 0;
    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        if ((*it)->getType() == OSD_PLANE) {
            osdPlanes ++;
            if (osdPlanes > 1) {
                strategyFlags |= MULTI_PLANES_WITH_DI;
                break;
            }
        }
    }
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
#ifdef HWC_HDR_METADATA_SUPPORT
    mCrtc->getHdrMetadataKeys(mHdrKeys);
#endif

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
int32_t Hwc2Display::blankDisplay() {
    MESON_LOGD("blank all display planes");
    std::lock_guard<std::mutex> lock(mMutex);

    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        (*it)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
    }

    int32_t fence = -1;
    if (mCrtc && mCrtc->pageFlip(fence) == 0) {
        std::shared_ptr<DrmFence> outfence =
            std::make_shared<DrmFence>(fence);
        outfence->wait(3000);
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
    return &mHdrCaps;
}

void Hwc2Display::getDispMode(drm_mode_info_t & dispMode){
    dispMode = mDisplayMode;
}

#ifdef HWC_HDR_METADATA_SUPPORT
hwc2_error_t Hwc2Display::getFrameMetadataKeys(
    uint32_t* outNumKeys, int32_t* outKeys) {
    *outNumKeys = mHdrKeys.size();
    if (NULL != outKeys) {
        for (uint32_t i = 0; i < *outNumKeys; i++)
            outKeys[i] = mHdrKeys[i];
    }

    return HWC2_ERROR_NONE;
}
#endif

hwc2_error_t Hwc2Display::setVsyncEnable(hwc2_vsync_t enabled) {
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

// HWC uses SystemControl for HDMI query / control purpose. Bacuase both parties
// respond to the same hot plug uevent additional means of synchronization
// are required before former can talk to the latter. To accomplish that HWC
// shall wait for SystemControl before it can update its state and notify FWK
// accordingly.
void Hwc2Display::onHotplug(bool connected) {
    bool bSendPlugOut = false;
    MESON_LOGD("On hot plug: [%s]", connected == true ? "Plug in" : "Plug out");

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (connected) {
            mSignalHpd = true;
            return;
        }
        mPowerMode->setConnectorStatus(false);
        if (mObserver != NULL ) {
            bSendPlugOut = true;
        }
    }

    /*call hotplug out of lock, SF may call some hwc function to cause deadlock.*/
    if (bSendPlugOut) {
        /* when hdmi plugout, send CONNECT message for "hdmi-only" */
        if (mConnector && mConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
            mModeMgr->update();
            mObserver->onHotplug(true);
        } else {
            mObserver->onHotplug(false);
        }
    }

    /* switch to software vsync when hdmi plug out and no cvbs mode */
    if (mConnector && mConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
        mVsync->setSoftwareMode();
    }
}

/* clear all layers and blank display when extend display plugout,
 * So the resource used by display drvier can be released.
 * Or framebuffer may allocate fail when do plug in/out quickly.
 */
void Hwc2Display::cleanupBeforeDestroy() {
    {/*clear framebuffer reference by gpu composer*/
        std::lock_guard<std::mutex> lock(mMutex);
        std::shared_ptr<IComposer> clientComposer = mComposers.find(MESON_CLIENT_COMPOSER)->second;
        clientComposer->prepare();
    }

    /*clear framebuffer reference by driver*/
    blankDisplay();
}

void Hwc2Display::onUpdate(bool bHdcp) {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("On update: [%s]", bHdcp == true ? "HDCP verify success" : "HDCP verify fail");

    if (bHdcp) {
        if (mObserver != NULL) {
            mObserver->refresh();
        } else {
            MESON_LOGE("No display oberserve register to display (%s)", getName());
        }
    }
}

void Hwc2Display::onVsync(int64_t timestamp, uint32_t vsyncPeriodNanos) {
    if (mObserver != NULL) {
        mObserver->onVsync(timestamp, vsyncPeriodNanos);
    } else {
        MESON_LOGE("Hwc2Display (%p) observer is NULL", this);
    }
}

void Hwc2Display::onModeChanged(int stage) {
    bool bSendPlugIn = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_LOGD("On mode change state: [%s]", stage == 1 ? "Complete" : "Begin to change");
        if (stage == 1) {
            if (mObserver != NULL) {
                /*plug in and set displaymode ok, update inforamtion.*/
                if (mSignalHpd) {
                    mConnector->getHdrCapabilities(&mHdrCaps);
                    mConnector->getSupportedContentTypes(mSupportedContentTypes);
#ifdef HWC_HDR_METADATA_SUPPORT
                    mCrtc->getHdrMetadataKeys(mHdrKeys);
#endif
                }

                /*update mode success.*/
                if (mModeMgr->getDisplayMode(mDisplayMode) == 0) {
                    MESON_LOGD("Hwc2Display::onModeChanged getDisplayMode [%s]", mDisplayMode.name);
                    mPowerMode->setConnectorStatus(true);
                    if (mSignalHpd) {
                        bSendPlugIn = true;
                        mSignalHpd = false;
                    } else {
                        /*Workaround: needed for NTS test.*/
                        if (HwcConfig::primaryHotplugEnabled()
                            && (mModeMgr->getPolicyType() == FIXED_SIZE_POLICY ||
                                mModeMgr->getPolicyType() == REAL_MODE_POLICY)) {
                            bSendPlugIn = true;
                        } else if (mModeMgr->getPolicyType() == ACTIVE_MODE_POLICY) {
                            bSendPlugIn = true;
                        } else if (mModeMgr->getPolicyType() == REAL_MODE_POLICY) {
                            bSendPlugIn = true;
                        }
                    }

                    uint32_t fbW,fbH;
                    HwcConfig::getFramebufferSize(0, fbW, fbH);
                    mScaleValue = (float)fbW/(float)mDisplayMode.pixelW;
                }
            } else {
                MESON_LOGE("No display oberserve register to display (%s)", getName());
            }
        } else {
            /* begin change mode, need blank once */
            mPowerMode->setConnectorStatus(false);
            return;
        }
    }
    /*call hotplug out of lock, SF may call some hwc function to cause deadlock.*/
    if (bSendPlugIn && mModeMgr->needCallHotPlug()) {
        MESON_LOGD("onModeChanged mObserver->onHotplug(true)");
        mObserver->onHotplug(true);
    } else {
        MESON_LOGD("mModeMgr->resetTags");
        mModeMgr->resetTags();
    }
    /*last call refresh*/
    mObserver->refresh();
}

/*
LayerId is 16bits.
Higher 8 Bits: now is 256 layer slot, the higher 8 bits is the slot index.
Lower 8 Bits: a sequence no, used to distinguish layers have same slot index (
which may happended a layer destoryed and at the same time a new layer created.)
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
    std::lock_guard<std::mutex> lock(mMutex);

    std::shared_ptr<Hwc2Layer> layer = std::make_shared<Hwc2Layer>();
    uint32_t idx = createLayerId();
    *outLayer = idx;
    layer->setUniqueId(*outLayer);
    mLayers.emplace(*outLayer, layer);

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::destroyLayer(hwc2_layer_t  inLayer) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto layerit = mLayers.find(inLayer);
    if (layerit == mLayers.end())
        return HWC2_ERROR_BAD_LAYER;

    DebugHelper::getInstance().removeDebugLayer((int)inLayer);
    mLayers.erase(inLayer);
    destroyLayerId(inLayer);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setCursorPosition(hwc2_layer_t layer __unused,
    int32_t x __unused, int32_t y __unused) {
    MESON_LOG_EMPTY_FUN();
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setColorTransform(const float* matrix,
    android_color_transform_t hint) {
    if (hint == HAL_COLOR_TRANSFORM_IDENTITY) {
        mForceClientComposer = false;
        memset(mColorMatrix, 0, sizeof(float) * 16);
    } else {
        mForceClientComposer = true;
        memcpy(mColorMatrix, matrix, sizeof(float) * 16);
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setPowerMode(hwc2_power_mode_t mode) {
    switch(mode) {
        case HWC2_POWER_MODE_ON:
        case HWC2_POWER_MODE_OFF:
            MESON_LOG_EMPTY_FUN();
            return HWC2_ERROR_NONE;
        case HWC2_POWER_MODE_DOZE:
        case HWC2_POWER_MODE_DOZE_SUSPEND:
            return HWC2_ERROR_UNSUPPORTED;
        default:
            return HWC2_ERROR_BAD_PARAMETER;
    };
}

std::shared_ptr<Hwc2Layer> Hwc2Display::getLayerById(hwc2_layer_t id) {
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
        if (bUpdateLayerList == true && layer->isUpdateZorder() == false) {
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
    std::lock_guard<std::mutex> lock(mMutex);
    mCalibrateCoordinates[0] = caliX;
    mCalibrateCoordinates[1] = caliY;
    mCalibrateCoordinates[2] = caliW;
    mCalibrateCoordinates[3] = caliH;

    return HWC2_ERROR_NONE;
}


int32_t Hwc2Display::getDisplayIdentificationData(uint32_t &outPort,
        std::vector<uint8_t> &outData) {
    int32_t ret = mConnector->getIdentificationData(outData);
    if (0 == ret) {
        outPort = mConnector->getId();
    }
    return ret;
}

int32_t Hwc2Display::loadCalibrateInfo() {
    hwc2_config_t config;
    int32_t configWidth;
    int32_t configHeight;
    if (mModeMgr->getActiveConfig(&config) != HWC2_ERROR_NONE) {
        MESON_ASSERT(0, "[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }
    if (mModeMgr->getDisplayAttribute(config,
            HWC2_ATTRIBUTE_WIDTH, &configWidth) != HWC2_ERROR_NONE) {
        MESON_ASSERT(0, "[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }
    if (mModeMgr->getDisplayAttribute(config,
            HWC2_ATTRIBUTE_HEIGHT, &configHeight) != HWC2_ERROR_NONE) {
        MESON_ASSERT(0, "[%s]: getHwcDisplayHeight failed!", __func__);
        return -ENOENT;
    }

    if (mDisplayMode.pixelW == 0 || mDisplayMode.pixelH == 0) {
        MESON_ASSERT(0, "[%s]: Displaymode is invalid(%s, %dx%d)!",
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
    bool bNoScale = false;
    if (mCalibrateInfo.framebuffer_w == mCalibrateInfo.crtc_display_w &&
        mCalibrateInfo.framebuffer_h == mCalibrateInfo.crtc_display_h) {
        bNoScale = true;
    }

    Hwc2Layer * layer;
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        layer = (Hwc2Layer*)(it->get());
        if (bNoScale) {
            layer->mDisplayFrame = layer->mBackupDisplayFrame;
        } else {
            layer->mDisplayFrame.left = (int32_t)ceilf(layer->mBackupDisplayFrame.left *
                mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
                mCalibrateInfo.crtc_display_x;
            layer->mDisplayFrame.top = (int32_t)ceilf(layer->mBackupDisplayFrame.top *
                mCalibrateInfo.crtc_display_h / mCalibrateInfo.framebuffer_h) +
                mCalibrateInfo.crtc_display_y;
            layer->mDisplayFrame.right = (int32_t)ceilf(layer->mBackupDisplayFrame.right *
                mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
                mCalibrateInfo.crtc_display_x;
            layer->mDisplayFrame.bottom = (int32_t)ceilf(layer->mBackupDisplayFrame.bottom *
                mCalibrateInfo.crtc_display_h / mCalibrateInfo.framebuffer_h) +
                mCalibrateInfo.crtc_display_y;
        }
    }

    return 0;
}

hwc2_error_t Hwc2Display::validateDisplay(uint32_t* outNumTypes,
    uint32_t* outNumRequests) {
    std::lock_guard<std::mutex> lock(mMutex);
    /*clear data used in composition.*/
    mPresentLayers.clear();
    mPresentComposers.clear();
    mPresentPlanes.clear();
    mPresentCompositionStg.reset();
    mChangedLayers.clear();
    mOverlayLayers.clear();
    mFailedDeviceComp = false;
    mSkipComposition = false;

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
        DebugHelper::getInstance().disableUiHwc() ||
        HwcConfig::forceClientEnabled()) {
        compositionFlags |= COMPOSE_FORCE_CLIENT;
    }

    if (HwcConfig::secureLayerProcessEnabled()) {
        if (!mConnector->isSecure()) {
            compositionFlags |= COMPOSE_HIDE_SECURE_FB;
        }
    }

    /*check power mode*/
    if (mPowerMode->needBlankScreen(mPresentLayers.size())) {
        if (!mPowerMode->getScreenStatus()) {
            MESON_LOGD("Need to blank screen.");
            mPresentLayers.clear();
        } else {
            mSkipComposition = true;
        }
    }
    /*do composition*/
    if (!mSkipComposition) {
        mPowerMode->setScreenStatus(mPresentLayers.size() > 0 ? false : true);
        /*update calibrate info.*/
        loadCalibrateInfo();
        /*update displayframe before do composition.*/
        if (mPresentLayers.size() > 0)
            adjustDisplayFrame();
        /*setup composition strategy.*/
        mPresentCompositionStg->setup(mPresentLayers,
            mPresentComposers, mPresentPlanes, mCrtc, compositionFlags, mScaleValue);
        if (mPresentCompositionStg->decideComposition() < 0) {
            return HWC2_ERROR_NO_RESOURCES;
        }

        /*collect changed dispplay, layer, compostiion.*/
        ret = collectCompositionRequest(outNumTypes, outNumRequests);
    } else {
        /* skip Composition */
        std::shared_ptr<IComposer> clientComposer =
            mComposers.find(MESON_CLIENT_COMPOSER)->second;
        clientComposer->prepare();
    }

    if (mPowerMode->getScreenStatus()) {
        mProcessorFlags |= PRESENT_BLANK;
    }

    /* If mValidateDisplay = false, hwc will not handle presentDisplay. */
    mValidateDisplay = true;

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
        MESON_LOGE("%s", layersDump.string());
    }
    return ret;
}

hwc2_error_t Hwc2Display::collectCompositionRequest(
    uint32_t* outNumTypes, uint32_t* outNumRequests) {
    Hwc2Layer *layer;
    /*collect display requested, and changed composition type.*/
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        layer = (Hwc2Layer*)(it->get());
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
        if (expectedHwcComposition == HWC2_COMPOSITION_SIDEBAND || layer->mCompositionType == MESON_COMPOSITION_PLANE_AMVIDEO)
            mProcessorFlags |= PRESENT_SIDEBAND;
    }

    /*collcet client clear layer.*/
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
   /* commit composition type */
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer * layer = (Hwc2Layer*)(it->get());
        layer->commitCompType(mesonComp2Hwc2Comp(layer));
    }

    /* set updateZorder flag to false */
    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        layer->updateZorder(false);
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::presentDisplay(int32_t* outPresentFence) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mSkipComposition) {
        *outPresentFence = -1;
    } else {
        if (mValidateDisplay == false) {
            MESON_LOGD("presentDisplay without validateDisplay");
            return HWC2_ERROR_NONE;
        }
        mValidateDisplay = false;
        if (mPresentFence >= 0)
            close(mPresentFence);
        mPresentFence = -1;

        /*start new pageflip, and preapre.*/
        if (mCrtc->prePageFlip() !=0 ) {
            return HWC2_ERROR_NO_RESOURCES;
        }
        /*Start to compose, set up plane info.*/
        if (mPresentCompositionStg->commit() != 0) {
            return HWC2_ERROR_NOT_VALIDATED;
        }
        #ifdef HWC_HDR_METADATA_SUPPORT
        /*set hdr metadata info.*/
        for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
            if ((*it)->mHdrMetaData.empty() == false) {
                mCrtc->setHdrMetadata((*it)->mHdrMetaData);
                break;
            }
        }
        #endif

        /* Page flip */
        if (mCrtc->pageFlip(mPresentFence) < 0) {
            return HWC2_ERROR_UNSUPPORTED;
        }
        if (mPostProcessor != NULL) {
            int32_t displayFence = ::dup(mPresentFence);
            mPostProcessor->present(mProcessorFlags, displayFence);
            mProcessorFlags = 0;
        }

        /*need use in getReleaseFence() later, dup to return to sf.*/
        *outPresentFence = ::dup(mPresentFence);
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
        MESON_LOGE("%s", compDump.string());
    }
    return HWC2_ERROR_NONE;
}

/*getRelaseFences is return the fence for previous frame, for DPU based
compsition, it is reasonable, current frame's present fence is the release
fence for previous frame.
But for m2m composer, there is no present fence, only release fence for
current frame, need do speical process to return it in next present loop.*/
hwc2_error_t Hwc2Display::getReleaseFences(uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outFences) {
    uint32_t num = 0;
    bool needInfo = false;
    if (outLayers && outFences)
        needInfo = true;

    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
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

    close(mPresentFence);
    mPresentFence = -1;
    *outNumElements = num;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setClientTarget(buffer_handle_t target,
    int32_t acquireFence, int32_t dataspace, hwc_region_t damage) {
    std::lock_guard<std::mutex> lock(mMutex);
    /*create DrmFramebuffer for client target.*/
    std::shared_ptr<DrmFramebuffer> clientFb =  std::make_shared<DrmFramebuffer>(
        target, acquireFence);
    clientFb->mFbType = DRM_FB_SCANOUT;
    clientFb->mBlendMode = DRM_BLEND_MODE_PREMULTIPLIED;
    clientFb->mPlaneAlpha = 1.0f;
    clientFb->mTransform = 0;
    clientFb->mDataspace = dataspace;

    /* real mode set real source crop */
    if (HwcConfig::getModePolicy(0) ==  REAL_MODE_POLICY) {
        drm_mode_info_t mode;
        mModeMgr->getDisplayMode(mode);
        clientFb->mSourceCrop.right = ((int32_t)mode.pixelW < clientFb->mSourceCrop.right)
            ? mode.pixelW : clientFb->mSourceCrop.right;
        clientFb->mSourceCrop.bottom = ((int32_t)mode.pixelH < clientFb->mSourceCrop.bottom)
            ? mode.pixelH : clientFb->mSourceCrop.bottom;
    }

    /*client target is always full screen, just post to crtc display axis.*/
    clientFb->mDisplayFrame.left = mCalibrateInfo.crtc_display_x;
    clientFb->mDisplayFrame.top = mCalibrateInfo.crtc_display_y;
    clientFb->mDisplayFrame.right = mCalibrateInfo.crtc_display_x +
        mCalibrateInfo.crtc_display_w;
    clientFb->mDisplayFrame.bottom = mCalibrateInfo.crtc_display_y +
        mCalibrateInfo.crtc_display_h;

    /*set framebuffer to client composer.*/
    std::shared_ptr<IComposer> clientComposer =
        mComposers.find(MESON_CLIENT_COMPOSER)->second;
    if (clientComposer.get() &&
        clientComposer->setOutput(clientFb, damage) == 0) {
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

hwc2_error_t Hwc2Display::setActiveConfig(
    hwc2_config_t config) {
    if (mModeMgr != NULL) {
        return (hwc2_error_t)mModeMgr->setActiveConfig(config);
    } else {
        MESON_LOGE("Display (%s) setActiveConfig miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

bool Hwc2Display::isLayerHideForDebug(hwc2_layer_t id) {
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
        *outNumCapabilities = 1;
    } else {
        if (mConnector->isConnected() == false) {
            outCapabilities[0] = HWC2_DISPLAY_CAPABILITY_INVALID;
        } else {
            outCapabilities[0] = HWC2_DISPLAY_CAPABILITY_AUTO_LOW_LATENCY_MODE;
        }
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getDisplayVsyncPeriod(hwc2_vsync_period_t* outVsyncPeriod) {
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

    if (vsyncPeriodChangeConstraints->seamlessRequired)
        return HWC2_ERROR_SEAMLESS_NOT_POSSIBLE;

    /* todo remove it when support vrr */
    outTimeline->refreshRequired = false;
    hwc2_config_t activeConfig;
    if (mModeMgr->getActiveConfig(&activeConfig) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_CONFIG;
    if (activeConfig != config)
        return (hwc2_error_t)mModeMgr->setActiveConfig(config);

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setAutoLowLatencyMode(bool enabled) {
    if (mConnector->isConnected() == false) {
        return HWC2_ERROR_UNSUPPORTED;
    } else {
        mConnector->setAutoLowLatencyMode(enabled);
    }

    return HWC2_ERROR_NONE;
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

void Hwc2Display::dumpPresentLayers(String8 & dumpstr) {
    dumpstr.append("----------------------------------------------------------"
        "-----------------------------------\n");
    dumpstr.append("|  id  |  z  |    type    |blend| alpha  |t|"
        "  AFBC  |    Source Crop    |    Display Frame  |\n");
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
        dumpstr.append("+------+-----+------------+-----+--------+-+--------+"
            "-------------------+-------------------+\n");
        dumpstr.appendFormat("|%6llu|%5d|%12s|%5d|%8f|%1d|%8x|%4d %4d %4d %4d"
            "|%4d %4d %4d %4d|\n",
            layer->getUniqueId(),
            layer->mZorder,
            drmFbTypeToString(layer->mFbType),
            layer->mBlendMode,
            layer->mPlaneAlpha,
            layer->mTransform,
            am_gralloc_get_vpu_afbc_mask(layer->mBufferHandle),
            layer->mSourceCrop.left,
            layer->mSourceCrop.top,
            layer->mSourceCrop.right,
            layer->mSourceCrop.bottom,
            layer->mDisplayFrame.left,
            layer->mDisplayFrame.top,
            layer->mDisplayFrame.right,
            layer->mDisplayFrame.bottom
            );
    }
    dumpstr.append("----------------------------------------------------------"
        "-----------------------------------\n");
}

void Hwc2Display::dumpHwDisplayPlane(String8 &dumpstr) {
    if (DebugHelper::getInstance().dumpDetailInfo()) {
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
        dumpstr.append("------------------------------------------------------------"
                "-----------------------------------------------------------------\n");
        dumpstr.append("\n");
    }
}

void Hwc2Display::dump(String8 & dumpstr) {
    /*update for debug*/
    if (DebugHelper::getInstance().debugHideLayers() ||
        DebugHelper::getInstance().debugPlanes()) {
        //ask for auto refresh for debug layers update.
        if (mObserver != NULL) {
            mObserver->refresh();
        }
    }

    /*dump*/
    dumpstr.append("---------------------------------------------------------"
        "-----------------------------\n");
    dumpstr.appendFormat("Display (%s, %s) \n",
        getName(), mForceClientComposer ? "Client-Comp" : "HW-Comp");
    dumpstr.appendFormat("Power: (%d-%d) \n",
        mPowerMode->getMode(), mPowerMode->getScreenStatus());
    /*calibration info*/
    dumpstr.appendFormat("Calibration: (%dx%d)->(%dx%d,%dx%d)\n",
        mCalibrateInfo.framebuffer_w, mCalibrateInfo.framebuffer_h,
        mCalibrateInfo.crtc_display_x, mCalibrateInfo.crtc_display_y,
        mCalibrateInfo.crtc_display_w, mCalibrateInfo.crtc_display_h);

    /* HDR info */
    if (mConnector)
        dumpstr.appendFormat("HDR current type: %s\n",
                mConnector->getCurrentHdrType().c_str());
    dumpstr.append("HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%d\n",
        mHdrCaps.DolbyVisionSupported ?  1 : 0);
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
    dumpstr.append("\n");

    /*dump detail debug info*/
    if (DebugHelper::getInstance().dumpDetailInfo()) {
        /* dump composers*/
        dumpstr.append("Valid composers:\n");
        for (auto it = mPresentComposers.begin(); it != mPresentComposers.end(); it++) {
            dumpstr.appendFormat("%s-Composer (%p)\n", it->get()->getName(), it->get());
        }
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
    }

    dumpstr.append("\n");
    dumpHwDisplayPlane(dumpstr);
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
