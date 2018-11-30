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

Hwc2Display::Hwc2Display(hw_display_id dspId,
    std::shared_ptr<Hwc2DisplayObserver> observer) {
    mHwId = dspId;
    mObserver = observer;
    mForceClientComposer = false;
    mPowerMode  = std::make_shared<HwcPowerMode>();
    mSignalHpd = false;
    memset(&mHdrCaps, 0, sizeof(mHdrCaps));
    memset(mColorMatrix, 0, sizeof(float) * 16);
}

Hwc2Display::~Hwc2Display() {
    HwDisplayManager::getInstance().unregisterObserver(mHwId);

    mLayers.clear();
    mPlanes.clear();
    mComposers.clear();

    mCrtc.reset();
    mConnector.reset();
    mObserver.reset();
    mCompositionStrategy.reset();

    mModeMgr.reset();
}

int32_t Hwc2Display::initialize() {
    MESON_LOG_FUN_ENTER();
    std::lock_guard<std::mutex> lock(mMutex);
    if (MESON_DUMMY_DISPLAY_ID != mHwId) {
        HwDisplayManager::getInstance().registerObserver(mHwId, this);
     } else {
        MESON_LOGE("Init Hwc2Display with dummy display.");
     }

    HwcConfig::getFramebufferSize (0, mFbWidth, mFbHeight);

    /*get hw components.*/
    mModeMgr = createModeMgr(HwcConfig::getModePolicy());
    mModeMgr->setFramebufferSize(mFbWidth, mFbHeight);

    /*add valid composers*/
    std::shared_ptr<IComposer> composer;
    ComposerFactory::create(MESON_CLIENT_COMPOSER, composer);
    mComposers.emplace(MESON_CLIENT_COMPOSER, std::move(composer));
    ComposerFactory::create(MESON_DUMMY_COMPOSER, composer);
    mComposers.emplace(MESON_DUMMY_COMPOSER, std::move(composer));
#if defined(HWC_ENABLE_GE2D_COMPOSITION)
    //TODO: havenot finish ge2d composer.
    ComposerFactory::create(MESON_GE2D_COMPOSER, composer);
    mComposers.emplace(MESON_GE2D_COMPOSER, std::move(composer));
#endif

    initLayerIdGenerator();

    /*manual do display init, for we may missed some displayevents.*/
    loadDisplayResources();
    mCrtc->update();
    mModeMgr->update();
    mPowerMode->setConnectorStatus(mConnector->isConnected());
    mCrtc->getMode(mDisplayMode);

    MESON_LOG_FUN_LEAVE();
    return 0;
}

void Hwc2Display::loadDisplayResources() {
    MESON_LOG_FUN_ENTER();
    if (MESON_DUMMY_DISPLAY_ID == mHwId)
        return;

    HwDisplayManager::getInstance().getCrtc(mHwId, mCrtc);
    HwDisplayManager::getInstance().getPlanes(mHwId, mPlanes);
    HwDisplayManager::getInstance().getConnector(mHwId, mConnector);
    mCrtc->loadProperities();
    mModeMgr->setDisplayResources(mCrtc, mConnector);
    mConnector->getHdrCapabilities(&mHdrCaps);
#ifdef HWC_HDR_METADATA_SUPPORT
    mCrtc->getHdrMetadataKeys(mHdrKeys);
#endif

    /*update composition strategy.*/
    uint32_t strategyFlags = 0;
    int osdPlanes = 0;
    for (auto it = mPlanes.begin(); it != mPlanes.end(); ++ it) {
        if ((*it)->getPlaneType() == OSD_PLANE) {
            osdPlanes ++;
            if (osdPlanes > 1) {
                strategyFlags |= MUTLI_OSD_PLANES;
                break;
            }
        }
    }
    mCompositionStrategy =
        CompositionStrategyFactory::create(SIMPLE_STRATEGY, strategyFlags);
    MESON_ASSERT(mCompositionStrategy, "Hwc2Display load composition strategy failed.");
    MESON_LOG_FUN_LEAVE();
}

const char * Hwc2Display::getName() {
    return mConnector->getName();
}

const drm_hdr_capabilities_t * Hwc2Display::getHdrCapabilities() {
    if (HwcConfig::defaultHdrCapEnabled()) {
        constexpr int sDefaultMinLumiance = 0;
        constexpr int sDefaultMaxLumiance = 500;
        mHdrCaps.HLGSupported = true;
        mHdrCaps.HDR10Supported = true;
        mHdrCaps.maxLuminance = sDefaultMaxLumiance;
        mHdrCaps.avgLuminance = sDefaultMaxLumiance;
        mHdrCaps.minLuminance = sDefaultMinLumiance;
    }
    return &mHdrCaps;
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
    HwDisplayManager::getInstance().enableVBlank(state);
    return HWC2_ERROR_NONE;
}

void Hwc2Display::onVsync(int64_t timestamp) {
    if (mObserver != NULL) {
        mObserver->onVsync(timestamp);
    }
}

// HWC uses SystemControl for HDMI query / control purpose. Bacuase both parties
// respond to the same hot plug uevent additional means of synchronization
// are required before former can talk to the latter. To accomplish that HWC
// shall wait for SystemControl before it can update its state and notify FWK
// accordingly.
void Hwc2Display::onHotplug(bool connected) {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("On hot plug: [%s]", connected == true ? "Plug in" : "Plug out");

    if (connected) {
        mSignalHpd = true;
        return;
    }
    mPowerMode->setConnectorStatus(false);

    if (mObserver != NULL && mModeMgr->getPolicyType() != FIXED_SIZE_POLICY) {
        mObserver->onHotplug(false);
    }
}

void Hwc2Display::onModeChanged(int stage) {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("On mode change state: [%s]", stage == 1 ? "Complete" : "Begin to change");
    if (stage == 1) {
        if (mObserver != NULL) {
            if (mSignalHpd) {
                loadDisplayResources();
                mCrtc->update();
                if (mCrtc->getMode(mDisplayMode) == 0) {
                    mModeMgr->update();
                    mObserver->onHotplug(true);
                    mSignalHpd = false;
                }
            } else {
                mCrtc->update();
                mModeMgr->update();

                /*Workaround: needed for NTS test.*/
                if (HwcConfig::primaryHotplugEnabled()
                    && mCrtc->getMode(mDisplayMode) == 0
                    && mModeMgr->getPolicyType() == FIXED_SIZE_POLICY) {
                    mObserver->onHotplug(true);
                }
            }

            if (mCrtc->getMode(mDisplayMode) == 0)
                mPowerMode->setConnectorStatus(true);


            /*last call refresh*/
            mObserver->refresh();
        } else {
            MESON_LOGE("No display oberserve register to display (%s)", getName());
        }
    }
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
    MESON_LOGD("createLayer (%d-%p)", idx, layer.get());

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::destroyLayer(hwc2_layer_t  inLayer) {
    std::lock_guard<std::mutex> lock(mMutex);

    MESON_LOGD("destoryLayer (%" PRId64 ")", inLayer);

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

    MESON_LOGV("msz color transform %d(%d)", hint, HAL_COLOR_TRANSFORM_IDENTITY);
    if (hint == HAL_COLOR_TRANSFORM_IDENTITY) {
        mForceClientComposer = false;
        memset(mColorMatrix, 0, sizeof(float) * 16);
    } else {
        mForceClientComposer = true;
        memcpy(mColorMatrix, matrix, sizeof(float) * 16);
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setPowerMode(hwc2_power_mode_t mode __unused) {
    MESON_LOG_EMPTY_FUN();
    return HWC2_ERROR_NONE;
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

    for (auto it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        std::shared_ptr<DrmFramebuffer> buffer = layer;
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
    for (auto  it = mPresentPlanes.begin(); it != mPresentPlanes.end(); it++) {
        std::shared_ptr<HwDisplayPlane> plane = *it;
        if (isPlaneHideForDebug(plane->getPlaneId())) {
            plane->setIdle(true);
        } else {
            plane->setIdle(false);
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
    mCalibrateInfo.crtc_display_x = 0;
    mCalibrateInfo.crtc_display_y = 0;
    mCalibrateInfo.crtc_display_w = mDisplayMode.pixelW;
    mCalibrateInfo.crtc_display_h = mDisplayMode.pixelH;

    if (!HwcConfig::preDisplayCalibrateEnabled()) {
        /*get post calibrate info.*/
        /*for interlaced, we do thing, osd driver will take care of it.*/
        int calibrateCoordinates[4];
        std::string dispModeStr(mDisplayMode.name);
        if (0 == sc_get_osd_position(dispModeStr, calibrateCoordinates)) {
            mCalibrateInfo.crtc_display_x = calibrateCoordinates[0];
            mCalibrateInfo.crtc_display_y = calibrateCoordinates[1];
            mCalibrateInfo.crtc_display_w = calibrateCoordinates[2];
            mCalibrateInfo.crtc_display_h = calibrateCoordinates[3];
        } else {
            MESON_LOGE("(%s): sc_get_osd_position failed", __func__);
        }
    }
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
            layer->mDisplayFrame.left = (uint32_t)ceilf(layer->mBackupDisplayFrame.left *
                mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
                mCalibrateInfo.crtc_display_x;
            layer->mDisplayFrame.top = (uint32_t)ceilf(layer->mBackupDisplayFrame.top *
                mCalibrateInfo.crtc_display_h / mCalibrateInfo.framebuffer_h) +
                mCalibrateInfo.crtc_display_y;
            layer->mDisplayFrame.right = (uint32_t)ceilf(layer->mBackupDisplayFrame.right *
                mCalibrateInfo.crtc_display_w / mCalibrateInfo.framebuffer_w) +
                mCalibrateInfo.crtc_display_x;
            layer->mDisplayFrame.bottom = (uint32_t)ceilf(layer->mBackupDisplayFrame.bottom *
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

    /*collect composition flag*/
    uint32_t compositionFlags = 0;
    if (mForceClientComposer ||
        DebugHelper::getInstance().disableUiHwc() ||
        HwcConfig::forceClientEnabled()) {
        compositionFlags |= COMPOSE_FORCE_CLIENT;
    }
#ifdef HWC_ENABLE_SECURE_LAYER
    if (!mConnector->isSecure()) {
        compositionFlags |= COMPOSE_HIDE_SECURE_FB;
    }
#endif

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
        mCompositionStrategy->setup(mPresentLayers,
            mPresentComposers, mPresentPlanes, mCrtc, compositionFlags);
        if (mCompositionStrategy->decideComposition() < 0) {
            return HWC2_ERROR_NO_RESOURCES;
        }

        /*collect changed dispplay, layer, compostiion.*/
        ret = collectCompositionRequest(outNumTypes, outNumRequests);
    }

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
    MESON_LOGV("layer requests: %d, type changed: %d",
        *outNumRequests, *outNumTypes);

    return ((*outNumTypes) > 0) ? HWC2_ERROR_HAS_CHANGES : HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getDisplayRequests(
    int32_t* outDisplayRequests, uint32_t* outNumElements,
    hwc2_layer_t* outLayers,int32_t* outLayerRequests) {
    *outDisplayRequests = HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET;

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
    for (auto it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        Hwc2Layer * layer = (Hwc2Layer*)(it->get());
        layer->commitCompType(mesonComp2Hwc2Comp(layer));
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::presentDisplay(int32_t* outPresentFence) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mSkipComposition) {
        *outPresentFence = -1;
    } else {
        int32_t outFence = -1;
        /*Start to compose, set up plane info.*/
        if (mCompositionStrategy->commit() != 0) {
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
        if (mCrtc->pageFlip(outFence) < 0) {
            return HWC2_ERROR_UNSUPPORTED;
        }
        *outPresentFence = outFence;
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
        mCompositionStrategy->dump(compDump);
        MESON_LOGE("%s", compDump.string());
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getReleaseFences(uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outFences) {
    uint32_t num = 0;
    bool needInfo = false;
    if (outLayers && outFences)
        needInfo = true;

    /*
    * Return release fence for all layers, not only DEVICE composition Layers,
    * for we donot know if it is DEVICE compositin in last composition.
    */
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
        num++;
        if (needInfo) {
            int32_t releaseFence = layer->getReleaseFence();
            *outLayers = layer->getUniqueId();
            *outFences = releaseFence;
            outLayers++;
            outFences++;
            layer->clearReleaseFence();
        }
    }

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
        return mModeMgr->getDisplayConfigs(outNumConfigs, outConfigs);
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
        return mModeMgr->getDisplayAttribute(config, attribute, outValue, CALL_FROM_SF);
    } else {
        MESON_LOGE("Hwc2Display (%s) getDisplayAttribute miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::getActiveConfig(
    hwc2_config_t* outConfig) {
    if (mModeMgr != NULL) {
        return mModeMgr->getActiveConfig(outConfig, CALL_FROM_SF);
    } else {
        MESON_LOGE("Hwc2Display (%s) getActiveConfig miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::setActiveConfig(
    hwc2_config_t config) {
    if (mModeMgr != NULL) {
        return mModeMgr->setActiveConfig(config);
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

bool Hwc2Display::isPlaneHideForDebug(int id) {
    std::vector<int> hidePlanes;
    DebugHelper::getInstance().getHidePlanes(hidePlanes);
    if (hidePlanes.empty())
        return false;

    for (auto it = hidePlanes.begin(); it < hidePlanes.end(); it++) {
        if (*it == id) {
            return true;
        }
    }

    return false;
}

void Hwc2Display::dumpPresentLayers(String8 & dumpstr) {
    dumpstr.append("----------------------------------------------------------"
        "-------------------------------\n");
    dumpstr.append("|  id  |  z  |  type  |blend| alpha  |t|"
        "  AFBC  |    Source Crop    |    Display Frame  |\n");
    for (auto it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
        dumpstr.append("+------+-----+--------+-----+--------+-+--------+"
            "-------------------+-------------------+\n");
        dumpstr.appendFormat("|%6llu|%5d|%8s|%5d|%8f|%1d|%8x|%4d %4d %4d %4d"
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
        "-------------------------------\n");
}

void Hwc2Display::dump(String8 & dumpstr) {
    /*update for debug*/
    if (DebugHelper::getInstance().debugHideLayers() ||
        DebugHelper::getInstance().debugHidePlanes()) {
        //ask for auto refresh for debug layers update.
        if (mObserver != NULL) {
            mObserver->refresh();
        }
    }

    /*dump*/
    dumpstr.append("---------------------------------------------------------"
        "-----------------------------\n");
    dumpstr.appendFormat("Display %d (%s, %s) \n",
        mHwId, getName(), mForceClientComposer ? "Client-Comp" : "HW-Comp");
    dumpstr.appendFormat("Power: (%d-%d) \n",
        mPowerMode->getMode(), mPowerMode->getScreenStatus());
    /*calibration info*/
    dumpstr.appendFormat("Calibration: (%dx%d)->(%dx%d,%dx%d)\n",
        mCalibrateInfo.framebuffer_w, mCalibrateInfo.framebuffer_h,
        mCalibrateInfo.crtc_display_x, mCalibrateInfo.crtc_display_y,
        mCalibrateInfo.crtc_display_w, mCalibrateInfo.crtc_display_h);

    /* HDR info */
    dumpstr.append("HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%d\n",
        mHdrCaps.DolbyVisionSupported ?  1 : 0);
    dumpstr.appendFormat("    HLG=%d\n",
        mHdrCaps.HLGSupported ?  1 : 0);
    dumpstr.appendFormat("    HDR10=%d, maxLuminance=%d,"
        "avgLuminance=%d, minLuminance=%d\n",
        mHdrCaps.HDR10Supported ? 1 : 0,
        mHdrCaps.maxLuminance,
        mHdrCaps.avgLuminance,
        mHdrCaps.minLuminance);
    dumpstr.append("\n");

    /* dump display configs*/
     mModeMgr->dump(dumpstr);
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
        mCompositionStrategy->dump(dumpstr);
        dumpstr.append("\n");
    }

    dumpstr.append("\n");
}

