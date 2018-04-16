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

#include <MesonLog.h>
#include <DebugHelper.h>
#include <Composition.h>
#include <IComposeDevice.h>
#include <ComposerFactory.h>
#include <CompositionStrategyFactory.h>

Hwc2Display::Hwc2Display(hw_display_id dspId,
    std::shared_ptr<Hwc2DisplayObserver> observer) {
    mHwId = dspId;
    mObserver = observer;
    mForceClientComposer = false;
    memset(&mHdrCaps, 0, sizeof(mHdrCaps));
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
    /*get hw components.*/
    mModeMgr = createModeMgr(HwcModeMgr::FIXED_SIZE_POLICY);

    if (MESON_DUMMY_DISPLAY_ID == mHwId) {
        MESON_LOGE("Init Hwc2Display with dummy display.");
     } else {
        HwDisplayManager::getInstance().registerObserver(mHwId, this);
        HwDisplayManager::getInstance().getCrtc(mHwId, mCrtc);
     }

    /*add valid composers*/
    std::shared_ptr<IComposeDevice> composer;
    ComposerFactory::create(MESON_CLIENT_COMPOSER, composer);
    mComposers.emplace(MESON_CLIENT_COMPOSER, std::move(composer));
    ComposerFactory::create(MESON_DUMMY_COMPOSER, composer);
    mComposers.emplace(MESON_DUMMY_COMPOSER, std::move(composer));
#ifdef HWC_ENABLE_GE2D_COMPOSITION
    ComposerFactory::create(MESON_GE2D_COMPOSER, composer);
    mComposers.emplace(MESON_GE2D_COMPOSER, std::move(composer));
#endif

    /* load present stragetic.*/
    mCompositionStrategy = CompositionStrategyFactory::create(SIMPLE_STRATEGY, 0);

    return 0;
}

void Hwc2Display::loadDisplayResources() {
    HwDisplayManager::getInstance().getPlanes(mHwId, mPlanes);
    HwDisplayManager::getInstance().getConnector(mHwId, mConnector);

    mConnector->loadProperities();
    mConnector->getHdrCapabilities(&mHdrCaps);
}

const char * Hwc2Display::getName() {
    switch (mConnector->getType()) {
        case DRM_MODE_CONNECTOR_HDMI:
            return DRM_CONNECTOR_HDMI_NAME;
        case DRM_MODE_CONNECTOR_CVBS:
            return DRM_CONNECTOR_CVBS_NAME;
        case DRM_MODE_CONNECTOR_PANEL:
            return DRM_CONNECTOR_PANEL_NAME;
        default:
            MESON_LOGE("Unknown connector (%d)", mConnector->getType());
            break;
    }

    return NULL;
}

const drm_hdr_capabilities_t * Hwc2Display::getHdrCapabilities() {
    return &mHdrCaps;
}

hwc2_error_t Hwc2Display::setVsyncEnable(hwc2_vsync_t enabled) {
    HwDisplayManager::getInstance().enableVBlank(enabled);
    return HWC2_ERROR_NONE;
}

void Hwc2Display::onVsync(int64_t timestamp) {
    if (mObserver != NULL) {
        mObserver->onVsync(timestamp);
    }
}

void Hwc2Display::onHotplug(bool connected) {
    MESON_LOGD("onHotplug: %d", connected);
    loadDisplayResources();
    mModeMgr->setDisplayResources(mCrtc, mConnector);

    if (mObserver != NULL) {
        mObserver->setLoadInfoStatus(true);
        mObserver->onHotplug(connected);
    }
}

void Hwc2Display::onModeChanged(int stage) {
    MESON_LOGD("onModeChanged: %d", stage);
    if (stage == 0) {
        mModeMgr->updateDisplayResources();
        if (mObserver != NULL) {
            mObserver->refresh();

            /*Update info to surfaceflinger by hotplug.*/
            if (mModeMgr->getPolicyType() == HwcModeMgr::FIXED_SIZE_POLICY) {
                mObserver->onHotplug(true);
            }
        } else {
            MESON_LOGE("No display oberserve register to display (%s)", getName());
        }
    }
}

void Hwc2Display::setLoadInfoStatus(bool compate) {

}

hwc2_error_t Hwc2Display::createLayer(hwc2_layer_t * outLayer) {
    std::shared_ptr<Hwc2Layer> layer = std::make_shared<Hwc2Layer>();
    hwc2_layer_t layerId = reinterpret_cast<hwc2_layer_t>(layer.get());
    layer->setUniqueId(layerId);
    mLayers.emplace(layerId, layer);
    *outLayer = layerId;

    MESON_LOGD("createLayer (%p)", layer.get());
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::destroyLayer(hwc2_layer_t  inLayer) {
    MESON_LOGD("destoryLayer (%" PRId64 ")", inLayer);
    mLayers.erase(inLayer);
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setCursorPosition(hwc2_layer_t layer,
    int32_t x, int32_t y) {
    UNUSED(layer);
    UNUSED(x);
    UNUSED(y);
    MESON_LOG_EMPTY_FUN();
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setColorTransform(const float* matrix,
    android_color_transform_t hint) {

    if (hint == HAL_COLOR_TRANSFORM_IDENTITY) {
        mForceClientComposer = false;
    } else {
        mForceClientComposer = true;
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::setPowerMode(hwc2_power_mode_t mode) {
    UNUSED(mode);
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
    mPresentLayers.clear();
    mPresentLayers.reserve(10);

    bool disableUiComposer = DebugHelper::getInstance().disableUiHwc();
    disableUiComposer |= mForceClientComposer;

    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>>::iterator it;
    for (it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        std::shared_ptr<DrmFramebuffer> buffer = layer;
        mPresentLayers.push_back(buffer);

         /*update expected internal composition type.*/
        #ifdef HWC_ENABLE_SECURE_LAYER
        if (layer->isSecure() && !mConnector->isSecure()) {
            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
            continue;
        }
        #endif

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
            if (disableUiComposer && layer->isRenderable()) {
                layer->mCompositionType = MESON_COMPOSITION_CLIENT;
            } else {
                layer->mCompositionType = MESON_COMPOSITION_NONE;
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
    mPresentPlanes.clear();
    mPresentPlanes = mPlanes;
    return  HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::collectComposersForPresent() {
    mPresentComposers.clear();

    std::map<meson_composer_t, std::shared_ptr<IComposeDevice>>::iterator it =
        mComposers.begin();
    for ( ; it != mComposers.end(); it++) {
        mPresentComposers.push_back(it->second);
    }

    return HWC2_ERROR_NONE;
}

void Hwc2Display::dumpPresentComponents() {
    /*Dump layers for DEBUG.*/
    MESON_LOGE("+++++++ Present layers +++++++\n");
    for (std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it = mPresentLayers.begin();
            it != mPresentLayers.end(); it++) {
        MESON_LOGE("Layer(%p) zorder (%d)", it->get(), it->get()->mZorder);
    }

    /*Dump composers for DEBUG.*/
    MESON_LOGE("+++++++ Present composers +++++++\n");
    for (std::vector<std::shared_ptr<IComposeDevice>>::iterator it = mPresentComposers.begin();
            it != mPresentComposers.end(); it++) {
        MESON_LOGE("Composer(%p, %s) \n", it->get(), it->get()->getName());
    }

    /*Dump planes for DEBUG.*/
    MESON_LOGE("+++++++ Present planes +++++++\n");
    for (std::vector<std::shared_ptr<HwDisplayPlane>>::iterator it = mPresentPlanes.begin();
            it != mPresentPlanes.end(); it++) {
        MESON_LOGE("Plane (%p, %d)", it->get(), it->get()->getPlaneType());
    }
}

hwc2_error_t Hwc2Display::validateDisplay(uint32_t* outNumTypes,
    uint32_t* outNumRequests) {
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

    // dumpPresentComponents();

    mCompositionStrategy->setUp(mPresentLayers,
        mPresentComposers, mPresentPlanes);
    if (mCompositionStrategy->decideComposition() < 0) {
        return HWC2_ERROR_NO_RESOURCES;
    }

    /*collect changed dispplay, layer, compostiion.*/
    return collectCompositionRequest(outNumTypes, outNumRequests);
}

hwc2_error_t Hwc2Display::collectCompositionRequest(
    uint32_t* outNumTypes, uint32_t* outNumRequests) {
    mChangedLayers.clear();
    mOverlayLayers.clear();

    Hwc2Layer *layer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator firstClientLayer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator lastClientLayer;
    firstClientLayer = lastClientLayer = mPresentLayers.end();

    /*collect display requested, and changed composition type.*/
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        layer = (Hwc2Layer*)(it->get());

        /*record composition changed layer.*/
        hwc2_composition_t expectedHwcComposition =
            translateCompositionType(layer->mCompositionType);
        if (expectedHwcComposition  != layer->mHwcCompositionType) {
            mChangedLayers.push_back(layer->getUniqueId());
        }

        /*find first/last client layers.*/
        if (layer->mCompositionType == MESON_COMPOSITION_CLIENT) {
            if (firstClientLayer == mPresentLayers.end()) {
                firstClientLayer = it;
            }
            lastClientLayer = it;
        }
    }

    /*collect overlay layers between client layers.*/
    if (firstClientLayer != mPresentLayers.end()) {
        for (it = firstClientLayer; it != lastClientLayer; it++) {
            layer = (Hwc2Layer*)(it->get());
            if (isOverlayComposition(layer->mCompositionType)) {
                mOverlayLayers.push_back(layer->getUniqueId());
            }
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
            outTypes[i] = translateCompositionType(layer->mCompositionType);
            outLayers[i] = mChangedLayers[i];
        }
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::acceptDisplayChanges() {
   /* commit composition type */
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mPresentLayers.begin() ; it != mPresentLayers.end(); it++) {
        Hwc2Layer * layer = (Hwc2Layer*)(it->get());
        layer->commitCompositionType();
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::presentDisplay(int32_t* outPresentFence) {
    int32_t outFence = -1;

    /*Start to compose, set up plane info.*/
    if (mCompositionStrategy->commit() != 0) {
        return HWC2_ERROR_NOT_VALIDATED;
    }

    /* Page flip */
    if (mCrtc->pageFlip(outFence) < 0) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    *outPresentFence = outFence;
    return HWC2_ERROR_NONE;
}

hwc2_error_t Hwc2Display::getReleaseFences(uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outFences) {
    uint32_t num = 0;
    bool needInfo = false;
    if (outLayers && outFences)
        needInfo = true;

    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mPresentLayers.begin(); it != mPresentLayers.end(); it++) {
        Hwc2Layer *layer = (Hwc2Layer*)(it->get());
        if (HWC2_COMPOSITION_DEVICE == layer->mHwcCompositionType) {
            num++;
            if (needInfo) {
                int32_t releaseFence = layer->getReleaseFence();
                *outLayers = layer->getUniqueId();
                *outFences = releaseFence;
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
    std::shared_ptr<DrmFramebuffer> clientFb =  std::make_shared<DrmFramebuffer>(
        target, acquireFence);
    clientFb->mDataspace = dataspace;

    std::shared_ptr<IComposeDevice> clientComposer =
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
        return mModeMgr->getDisplayAttribute(config, attribute, outValue);
    } else {
        MESON_LOGE("Hwc2Display (%s) getDisplayAttribute miss valid DisplayConfigure.",
            getName());
        return HWC2_ERROR_BAD_DISPLAY;
    }
}

hwc2_error_t Hwc2Display::getActiveConfig(
    hwc2_config_t* outConfig) {
    if (mModeMgr != NULL) {
        return mModeMgr->getActiveConfig(outConfig);
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

void Hwc2Display::dump(String8 & dumpstr) {
    dumpstr.append("---------------------------------------------------------"
        "-----------------------\n");
    dumpstr.appendFormat("Display (%s, %d, %s) state:\n",
        getName(), mHwId, mModeMgr->getName());

    mModeMgr->dump(dumpstr);

    /* HDR info */
    dumpstr.append("  HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%d\n",
        mHdrCaps.DolbyVisionSupported ?  1 : 0);
    dumpstr.appendFormat("    HDR10=%d, maxLuminance=%d,"
        "avgLuminance=%d, minLuminance=%d\n",
        mHdrCaps.HDR10Supported ? 1 : 0,
        mHdrCaps.maxLuminance,
        mHdrCaps.avgLuminance,
        mHdrCaps.minLuminance);

    dumpstr.appendFormat("HwComposition  (%s)\n", mForceClientComposer ? "disabled" : "enabled");

     if (DebugHelper::getInstance().dumpDetailInfo()) {
        std::vector<std::shared_ptr<HwDisplayPlane>>::iterator plane;
        dumpstr.append(" Plane name \n"
                "       z |  Comp Type | t |        Src Crop        |         DstFrame        | fd "
                "| format |  bs  |  ps  | blend | alpha |  op  |    afbc    |\n");
        dumpstr.append("--------+------------+---+------------------------+-------------------------+----"
                "+--------+------+------+-------+-------+------+------------+\n");
        for (plane = mPlanes.begin(); plane != mPlanes.end(); plane++) {
            (*plane)->dump(dumpstr);
        }
        dumpstr.append("--------+------------+---+------------------------+-------------------------+----"
                "+--------+------+------+-------+-------+------+------------+\n");

        std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>>::iterator layer;
        for (layer = mLayers.begin(); layer != mLayers.end(); layer++) {
            layer->second->dump(dumpstr);
        }

        if (mCompositionStrategy != NULL)
            mCompositionStrategy->dump(dumpstr);
    }
}

