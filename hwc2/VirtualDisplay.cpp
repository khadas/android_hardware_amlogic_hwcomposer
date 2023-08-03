/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include <MesonLog.h>
#include <BasicTypes.h>
#include "VirtualDisplay.h"

VirtualDisplay::VirtualDisplay(uint32_t width, uint32_t height, hwc2_display_t id)
    : Hwc2Display(NULL, id) {
    mWidth = width;
    mHeight = height;
    mDisplayId = id;
    mOutputFence.reset();
    mClientFence.reset();
}

VirtualDisplay::~VirtualDisplay() {
}

#define MAX_HWC_LAYERS (256)
#define LAYER_SLOT_BITS (8)

void VirtualDisplay::initLayerIdGenerator() {
    mLayersBitmap = std::make_shared<BitsMap>(MAX_HWC_LAYERS);
    mLayerSeq = 0;
}

hwc2_layer_t VirtualDisplay::createLayerId() {
    hwc2_layer_t layerId = 0;
    int idx = mLayersBitmap->getZeroBit();
    MESON_ASSERT(idx >= 0, "Bitmap getZeroBit failed");
    mLayersBitmap->setBit(idx);

    mLayerSeq++;
    mLayerSeq %= MAX_HWC_LAYERS;

    layerId = ((idx & (MAX_HWC_LAYERS - 1)) << LAYER_SLOT_BITS) |mLayerSeq;
    return layerId;
}

void VirtualDisplay::destroyLayerId(hwc2_layer_t id) {
    int slotIdx = id >> LAYER_SLOT_BITS;
    mLayersBitmap->clearBit(slotIdx);
}

hwc2_error_t VirtualDisplay::createLayer(hwc2_layer_t * outLayer __unused) {

    std::shared_ptr<Hwc2Layer> layer = std::make_shared<Hwc2Layer>(mDisplayId);
    uint32_t idx = createLayerId();
    *outLayer = idx;
    layer->setUniqueId(*outLayer);
    mLayers.emplace(*outLayer, layer);
    MESON_LOGV("%s displayId:%d, layerId:%u", __func__, mDisplayId, idx);

    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::destroyLayer(hwc2_layer_t  inLayer) {
    //std::lock_guard<std::mutex> lock(mMutex);
    auto layerit = mLayers.find(inLayer);
    if (layerit == mLayers.end())
        return HWC2_ERROR_BAD_LAYER;

    std::shared_ptr<Hwc2Layer> layer = layerit->second;
    DebugHelper::getInstance().removeDebugLayer((int)inLayer);
    mLayers.erase(inLayer);

    destroyLayerId(inLayer);
    return HWC2_ERROR_NONE;
}

std::shared_ptr<Hwc2Layer> VirtualDisplay::getLayerById(hwc2_layer_t id) {
    //std::lock_guard<std::mutex> lock(mMutex);
    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>>::iterator it =
    mLayers.find(id);

    if (it != mLayers.end())
        return it->second;

    return NULL;
}

int32_t VirtualDisplay::initialize() {
    initLayerIdGenerator();
    return 0;
}

const char * VirtualDisplay::getName() {
    return "Virtual";
}

const drm_hdr_capabilities_t * VirtualDisplay::getHdrCapabilities() {
    return NULL;
}

hwc2_error_t VirtualDisplay::setVsyncEnable(hwc2_vsync_t enabled) {
    UNUSED(enabled);
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setCursorPosition(hwc2_layer_t layer,
    int32_t x, int32_t y) {
    UNUSED(layer);
    UNUSED(x);
    UNUSED(y);
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setColorTransform(const float* matrix,
    android_color_transform_t hint) {
    UNUSED(matrix);
    UNUSED(hint);
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setPowerMode(int32_t mode) {
    UNUSED(mode);
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setFormat(int32_t* format) {
    MESON_LOGD("VirtualDisplay setformat (%d)", *format);
    UNUSED(format);
    return HWC2_ERROR_NONE;
}

int32_t VirtualDisplay::setPostProcessor(
    std::shared_ptr<HwcPostProcessor> processor) {
    ATRACE_NAME("VirtualDisplay::setPostProcessor");
    mPostProcessor = processor;
    mPostProcessor->start();
    mPostProcessor->createScreenRecordProcessor();
    mPostProcessor->setScreenSize(mWidth,mHeight);
    mPostProcessor->present(PRESENT_CAPSCREEN,-1);
    return 0;
}

void VirtualDisplay::stopVdin() {
    mPostProcessor->stop();
    mPostProcessor->destroyScreenRecordProcessor();
}

hwc2_error_t VirtualDisplay::setOutputBuffer(
    buffer_handle_t hnd, int32_t releaseFence) {
    ATRACE_NAME("VirtualDisplay::setOutputBuffer");
    std::shared_ptr<DrmFramebuffer> capBuffer;
    if (mPostProcessor && hnd) {
        capBuffer = std::make_shared<DrmFramebuffer>(hnd, -1);
        mPostProcessor->getLatestcapFb(capBuffer);
    }
    mOutputFence.reset(new DrmFence(releaseFence));
    capBuffer.reset();
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::validateDisplay(uint32_t* outNumTypes,
    uint32_t* outNumRequests) {
    ATRACE_NAME("VirtualDisplay::validateDisplay");
    mChangedLayers.clear();

    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>>::iterator it;
    for (it = mLayers.begin(); it != mLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (layer->mHwcCompositionType != HWC2_COMPOSITION_CLIENT) {
            layer->mCompositionType = MESON_COMPOSITION_CLIENT;
            mChangedLayers.push_back(layer->getUniqueId());
        }
    }

    *outNumRequests = 0;
    *outNumTypes = mChangedLayers.size();
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::presentDisplay(int32_t* outPresentFence __unused) {
    ATRACE_NAME("VirtualDisplay::presentDisplay");

    if (mPostProcessor != NULL) {
        mPostProcessor->present(PRESENT_CAPSCREEN,-1);
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::acceptDisplayChanges() {
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::getChangedCompositionTypes(
    uint32_t* outNumElements, hwc2_layer_t* outLayers,
    int32_t*  outTypes) {
    *outNumElements = mChangedLayers.size();
    if (outLayers && outTypes) {
        for (uint32_t i = 0; i < mChangedLayers.size(); i++) {
            outLayers[i] = mChangedLayers[i];
            outTypes[i] = MESON_COMPOSITION_CLIENT;
        }
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::getDisplayRequests(
    int32_t* outDisplayRequests, uint32_t* outNumElements,
    hwc2_layer_t* outLayers,int32_t* outLayerRequests) {
    /*No hw layers, no display requests.*/
    UNUSED(outLayers);
    UNUSED(outLayerRequests);

    *outDisplayRequests = HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET |
        HWC2_DISPLAY_REQUEST_WRITE_CLIENT_TARGET_TO_OUTPUT;
    *outNumElements = 0;
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setClientTarget(buffer_handle_t target,
    int32_t acquireFence, int32_t dataspace, hwc_region_t damage) {
    /*No need to handle client target, just return.*/
    UNUSED(target);
    UNUSED(dataspace);
    UNUSED(damage);

    mClientFence.reset(new DrmFence(acquireFence));
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::getReleaseFences(uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outFences) {
    /*No layers handled by hw, no release fences needed.*/
    UNUSED(outLayers);
    UNUSED(outFences);

    *outNumElements = 0;
    return HWC2_ERROR_NONE;
}

hwc2_error_t  VirtualDisplay::getDisplayConfigs(
    uint32_t* outNumConfigs, hwc2_config_t* outConfigs) {
    if (NULL != outConfigs) outConfigs[0] = 0;
    *outNumConfigs = 1;
    return HWC2_ERROR_NONE;
}

hwc2_error_t  VirtualDisplay::getDisplayAttribute(
    hwc2_config_t config, int32_t attribute, int32_t* outValue) {
    if (config > 0) {
        MESON_LOGE("VirtualDisplay set invalid config (%d)", config);
        return HWC2_ERROR_BAD_CONFIG;
    }

    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            *outValue = 1e9 / 60;
        break;
        case HWC2_ATTRIBUTE_WIDTH:
            *outValue = mWidth;
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
            *outValue = mHeight;
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = 160;
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = 160;
        break;
        default:
            MESON_LOGE("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::getActiveConfig(hwc2_config_t* outConfig) {
    *outConfig = 0;
    return HWC2_ERROR_NONE;
}

hwc2_error_t VirtualDisplay::setActiveConfig(hwc2_config_t config) {
    if (config > 0) {
        MESON_LOGE("VirtualDisplay set invalid config (%d)", config);
        return HWC2_ERROR_BAD_CONFIG;
    }

    return HWC2_ERROR_NONE;
}

void VirtualDisplay::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Display (%s) connected.", getName());
}

