/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log/log.h>
#include <HwcHal.h>

#define GET_DISPLAY_ID(display) \
    hwc2_display_t displayId = static_cast<hwc2_display_t>(display)

#define GET_LAYER_ID(layer) \
    hwc2_layer_t layerId = static_cast<hwc2_layer_t>(layer)

#define GET_CONFIG_ID(config) \
    hwc2_config_t configId = static_cast<hwc2_config_t>(config)

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

// help functions to do type change
hwc_rect_t toHwcRect(common::Rect rect) {
    return hwc_rect_t{rect.left, rect.top, rect.right, rect.bottom};
}

hwc_frect_t toHwcFRect(common::FRect rect) {
    return hwc_frect_t{rect.left, rect.top, rect.right, rect.bottom};
}

HwcHal::~HwcHal() {
    if (mDevice) {
        hwc2_close(mDevice);
    }
}

bool HwcHal::init() {
    const hw_module_t* module;
    int error = hw_get_module(HWC_HARDWARE_MODULE_ID, &module);
    if (error) {
        ALOGE("failed to get hwcomposer module");
        return false;
    }

    // we own the device from this point on
    int result = hwc2_open(module, &mDevice);
    if (result) {
        ALOGE("failed to open hwcomposer device: %s", strerror(-result));
        return false;
    }

    initCapabilities();
    bool requireReliablePresentFence = false;
    if (requireReliablePresentFence &&
        hasCapability(HWC2_CAPABILITY_PRESENT_FENCE_IS_NOT_RELIABLE)) {
        ALOGE("present fence must be reliable");
        mDevice->common.close(&mDevice->common);
        mDevice = nullptr;
        return false;
    }

    if (!initDispatch()) {
        mDevice->common.close(&mDevice->common);
        mDevice = nullptr;
        return false;
    }

    return true;
}

bool HwcHal::hasCapability(hwc2_capability_t capability) {
    return (mCapabilities.count(capability) > 0);
}

std::string HwcHal::dump() {
    uint32_t len = 0;
    mDispatch.dump(mDevice, &len, nullptr);

    std::vector<char> buf(len + 1);
    mDispatch.dump(mDevice, &len, buf.data());
    buf.resize(len + 1);
    buf[len] = '\0';

    return buf.data();
}

void HwcHal::registerEventCallback(ComposerHal::EventCallback* callback) {
    mEventCallback = callback;

    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_HOTPLUG, this,
                               reinterpret_cast<hwc2_function_pointer_t>(hotplugHook));
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_REFRESH, this,
                               reinterpret_cast<hwc2_function_pointer_t>(refreshHook));
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_VSYNC, this,
                               reinterpret_cast<hwc2_function_pointer_t>(vsyncHook));
}

void HwcHal::unregisterEventCallback() {
    // we assume the callback functions
    //
    //  - can be unregistered
    //  - can be in-flight
    //  - will never be called afterward
    //
    // which is likely incorrect
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_HOTPLUG, this, nullptr);
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_REFRESH, this, nullptr);
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_VSYNC, this, nullptr);

    mEventCallback = nullptr;
}

uint32_t HwcHal::getMaxVirtualDisplayCount() {
    return mDispatch.getMaxVirtualDisplayCount(mDevice);
}

HWC3::Error HwcHal::createVirtualDisplay(uint32_t width, uint32_t height,
                                         common::PixelFormat* format,
                                         int64_t* outDisplay) {
    int32_t hwc_format = static_cast<int32_t>(*format);
    hwc2_display_t outDisplayId;
    int32_t err = mDispatch.createVirtualDisplay(mDevice, width, height,
                                                 &hwc_format, &outDisplayId);
    *format = static_cast<common::PixelFormat>(hwc_format);
    *outDisplay = static_cast<int64_t>(outDisplayId);

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::destroyVirtualDisplay(int64_t display) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.destroyVirtualDisplay(mDevice, displayId);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::createLayer(int64_t display, int64_t* outLayer) {
    GET_DISPLAY_ID(display);
    hwc2_layer_t outLayerId;
    int32_t err = mDispatch.createLayer(mDevice, displayId, &outLayerId);
    *outLayer = static_cast<int64_t> (outLayerId);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::destroyLayer(int64_t display, int64_t layer) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.destroyLayer(mDevice, displayId, layerId);
    onLayerDestroyed(display, layer);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getActiveConfig(int64_t display, int32_t* outConfig) {
    GET_DISPLAY_ID(display);
    hwc2_config_t outConfigId;
    int32_t err = mDispatch.getActiveConfig(mDevice, displayId, &outConfigId);
    *outConfig = static_cast<int32_t>(outConfigId);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getClientTargetSupport(int64_t display, uint32_t width,
                                           uint32_t height,
                                           common::PixelFormat format,
                                           common::Dataspace dataspace) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.getClientTargetSupport(mDevice, displayId, width, height,
                                                   static_cast<int32_t>(format),
                                                   static_cast<int32_t>(dataspace));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getColorModes(int64_t display, std::vector<ColorMode>* outModes) {
    GET_DISPLAY_ID(display);
    uint32_t count = 0;
    int32_t err = mDispatch.getColorModes(mDevice, displayId, &count, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    outModes->resize(count);
    err = mDispatch.getColorModes(
        mDevice, displayId, &count,
        reinterpret_cast<std::underlying_type<ColorMode>::type*>(outModes->data()));
    if (err != HWC2_ERROR_NONE) {
        *outModes = std::vector<ColorMode>();
        return static_cast<HWC3::Error>(err);
    }

    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayAttribute(int64_t display, int32_t config, int32_t attribute,
                                        int32_t* outValue) {
    GET_DISPLAY_ID(display);
    GET_CONFIG_ID(config);

    int32_t err = mDispatch.getDisplayAttribute(mDevice, displayId, configId,
                                                static_cast<int32_t>(attribute), outValue);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getDisplayConfigs(int64_t display, std::vector<int32_t>* outConfigs) {
    GET_DISPLAY_ID(display);
    uint32_t count = 0;
    int32_t err = mDispatch.getDisplayConfigs(mDevice, displayId, &count, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    std::vector<hwc2_config_t> configs;
    configs.resize(count);
    err = mDispatch.getDisplayConfigs(mDevice, displayId, &count, configs.data());
    if (err != HWC2_ERROR_NONE) {
        *outConfigs = std::vector<int32_t>();
        return static_cast<HWC3::Error>(err);
    }

    outConfigs->resize(count);
    outConfigs->assign(configs.begin(), configs.end());

    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayName(int64_t display, std::string* outName) {
    GET_DISPLAY_ID(display);
    uint32_t count = 0;
    int32_t err = mDispatch.getDisplayName(mDevice, displayId, &count, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    std::vector<char> buf(count + 1);
    err = mDispatch.getDisplayName(mDevice, displayId, &count, buf.data());
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }
    buf.resize(count + 1);
    buf[count] = '\0';

    *outName = buf.data();

    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayType(int64_t display, HWC2::DisplayType* outType) {
    GET_DISPLAY_ID(display);
    int32_t hwc_type = HWC2_DISPLAY_TYPE_INVALID;
    int32_t err = mDispatch.getDisplayType(mDevice, displayId, &hwc_type);
    *outType = static_cast<HWC2::DisplayType>(hwc_type);

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getDozeSupport(int64_t display, bool* outSupport) {
    GET_DISPLAY_ID(display);
    int32_t hwc_support = 0;
    int32_t err = mDispatch.getDozeSupport(mDevice, displayId, &hwc_support);
    *outSupport = hwc_support;

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::getHdrCapabilities(int64_t display,
                                       std::vector<common::Hdr>* outTypes,
                                       float* outMaxLuminance,
                                       float* outMaxAverageLuminance,
                                       float* outMinLuminance) {
    GET_DISPLAY_ID(display);
    uint32_t count = 0;
    int32_t err =
        mDispatch.getHdrCapabilities(mDevice, displayId, &count,
                                     nullptr, outMaxLuminance,
                                     outMaxAverageLuminance, outMinLuminance);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    outTypes->resize(count);
    err = mDispatch.getHdrCapabilities(
        mDevice, displayId, &count,
        reinterpret_cast<std::underlying_type<common::Hdr>::type*>(outTypes->data()),
        outMaxLuminance,
        outMaxAverageLuminance, outMinLuminance);
    if (err != HWC2_ERROR_NONE) {
        *outTypes = std::vector<common::Hdr>();
        return static_cast<HWC3::Error>(err);
    }

    return HWC3::Error::None;
}

HWC3::Error HwcHal::setActiveConfig(int64_t display, int32_t config) {
    GET_DISPLAY_ID(display);
    GET_CONFIG_ID(config);
    int32_t err = mDispatch.setActiveConfig(mDevice, displayId, configId);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setColorMode(int64_t display, ColorMode mode, RenderIntent intent) {
    if (static_cast<int>(intent) < 0) {
        return HWC3::Error::BadParameter;
    }

    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.setColorMode(mDevice, displayId, static_cast<int32_t>(mode));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setPowerMode(int64_t display, PowerMode mode) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.setPowerMode(mDevice, displayId, static_cast<int32_t>(mode));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setVsyncEnabled(int64_t display, bool enabled) {
    GET_DISPLAY_ID(display);
    hwc2_vsync_t vsyncEnabled = enabled ? HWC2_VSYNC_ENABLE : HWC2_VSYNC_DISABLE;
    int32_t err = mDispatch.setVsyncEnabled(mDevice, displayId,
                                            static_cast<int32_t>(vsyncEnabled));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setColorTransform(int64_t display, const float* matrix, int32_t hint) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.setColorTransform(mDevice, displayId, matrix, hint);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setClientTarget(int64_t display, buffer_handle_t target,
                                    int32_t acquireFence,
                                    int32_t dataspace,
                                    const std::vector<common::Rect>& damage) {
    GET_DISPLAY_ID(display);

    std::vector<hwc_rect_t> halDamage;
    for (auto item : damage) {
        halDamage.push_back(toHwcRect(item));
    }

    hwc_region region = {halDamage.size(), halDamage.data()};
    int32_t err = mDispatch.setClientTarget(mDevice, displayId,
                                            target, acquireFence,
                                            dataspace, region);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setOutputBuffer(int64_t display, buffer_handle_t buffer,
                                    int32_t releaseFence) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.setOutputBuffer(mDevice, displayId,
                                            buffer, releaseFence);
    // unlike in setClientTarget, releaseFence is owned by us
    if (err == HWC2_ERROR_NONE && releaseFence >= 0) {
        close(releaseFence);
    }

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::validateDisplay(int64_t display,
                                    std::vector<int64_t>* outChangedLayers,
                                    std::vector<Composition>* outCompositionTypes,
                                    uint32_t* outDisplayRequestMask,
                                    std::vector<int64_t>* outRequestedLayers,
                                    std::vector<uint32_t>* outRequestMasks,
                                    ClientTargetProperty* outClientTargetProperty) {
    GET_DISPLAY_ID(display);
    uint32_t typesCount = 0;
    uint32_t reqsCount = 0;
    onBeforeValidateDisplay(display);
    int32_t err = mDispatch.validateDisplay(mDevice, displayId, &typesCount, &reqsCount);

    if (err != HWC2_ERROR_NONE && err != HWC2_ERROR_HAS_CHANGES) {
        return static_cast<HWC3::Error>(err);
    }

    err = getChangedCompositionTypes(display, &typesCount, nullptr, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    std::vector<int64_t> changedLayers(typesCount);
    std::vector<HWC2::Composition> compositionTypes(typesCount);
    err = getChangedCompositionTypes(display, &typesCount, changedLayers.data(),
                                     compositionTypes.data());
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    int32_t displayReqs = 0;
    err = mDispatch.getDisplayRequests(mDevice, displayId, &displayReqs, &reqsCount, nullptr,
                                       nullptr);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    std::vector<hwc2_layer_t> requestedLayers(reqsCount);
    std::vector<uint32_t> requestMasks(reqsCount);
    err = mDispatch.getDisplayRequests(mDevice, displayId, &displayReqs, &reqsCount,
                                       requestedLayers.data(),
                                       reinterpret_cast<int32_t*>(requestMasks.data()));
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    *outChangedLayers = std::move(changedLayers);
    *outDisplayRequestMask = static_cast<uint32_t> (displayReqs);
    outRequestedLayers->assign(requestedLayers.begin(), requestedLayers.end());
    outCompositionTypes->clear();
    for (auto type : compositionTypes) {
        outCompositionTypes->push_back(static_cast<Composition>(type));
    }

    *outRequestMasks = std::move(requestMasks);

    if (mDispatch.getClientTargetProperty) {
        hwc_client_target_property_t clientTargetProperty;
        err = mDispatch.getClientTargetProperty(mDevice, displayId, &clientTargetProperty);
        outClientTargetProperty->pixelFormat =
            static_cast<common::PixelFormat>(clientTargetProperty.pixelFormat);
        outClientTargetProperty->dataspace =
            static_cast<common::Dataspace>(clientTargetProperty.dataspace);
    }

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::acceptDisplayChanges(int64_t display) {
    GET_DISPLAY_ID(display);
    int32_t err = mDispatch.acceptDisplayChanges(mDevice, displayId);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::presentDisplay(int64_t display,
                                   int32_t* outPresentFence,
                                   std::vector<int64_t>* outLayers,
                                   std::vector<int32_t>* outReleaseFences) {
    GET_DISPLAY_ID(display);
    *outPresentFence = -1;
    int32_t err = mDispatch.presentDisplay(mDevice, displayId, outPresentFence);
    if (err != HWC2_ERROR_NONE) {
        return static_cast<HWC3::Error>(err);
    }

    uint32_t count = 0;
    err = mDispatch.getReleaseFences(mDevice, displayId, &count, nullptr, nullptr);
    if (err != HWC2_ERROR_NONE) {
        ALOGW("failed to get release fences");
        return HWC3::Error::None;
    }

    std::vector<hwc2_layer_t> layers(count);
    outReleaseFences->resize(count);
    err = mDispatch.getReleaseFences(mDevice, displayId, &count, layers.data(),
                                     outReleaseFences->data());
    if (err != HWC2_ERROR_NONE) {
        ALOGW("failed to get release fences");
        outLayers->clear();
        outReleaseFences->clear();
        return HWC3::Error::None;
    }
    outLayers->resize(count);
    outLayers->assign(layers.begin(), layers.end());

    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerCursorPosition(int64_t display, int64_t layer,
                                           int32_t x, int32_t y) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setCursorPosition(mDevice, displayId, layerId, x, y);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerBuffer(int64_t display,
                                   int64_t layer,
                                   buffer_handle_t buffer,
                                   int32_t acquireFence) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerBuffer(mDevice, displayId, layerId,
                                           buffer, acquireFence);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerSurfaceDamage(int64_t display, int64_t layer,
        const std::vector<std::optional<common::Rect>>& damage) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);

    std::vector<hwc_rect_t> halDamage;
    for (auto item : damage) {
        if (item.has_value())
            halDamage.push_back(toHwcRect(item.value()));
    }

    hwc_region region = {halDamage.size(), halDamage.data()};
    int32_t err = mDispatch.setLayerSurfaceDamage(mDevice, displayId, layerId, region);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerBlendMode(int64_t display, int64_t layer, int32_t mode) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerBlendMode(mDevice, displayId, layerId, mode);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerColor(int64_t display, int64_t layer, Color color) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    hwc_color_t hwc_color{
        static_cast<uint8_t>(color.r),
        static_cast<uint8_t>(color.g),
        static_cast<uint8_t>(color.b),
        static_cast<uint8_t>(color.a)
    };
    int32_t err = mDispatch.setLayerColor(mDevice, displayId, layerId, hwc_color);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerCompositionType(int64_t display, int64_t layer, int32_t type) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerCompositionType(mDevice, displayId, layerId, type);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerDataspace(int64_t display, int64_t layer, int32_t dataspace) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerDataspace(mDevice, displayId, layerId, dataspace);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerDisplayFrame(int64_t display, int64_t layer, const common::Rect& frame) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerDisplayFrame(mDevice, displayId, layerId, toHwcRect(frame));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerPlaneAlpha(int64_t display, int64_t layer, float alpha) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerPlaneAlpha(mDevice, displayId, layerId, alpha);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerSidebandStream(int64_t display,
                                           int64_t layer, buffer_handle_t stream) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerSidebandStream(mDevice, displayId, layerId, stream);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerSourceCrop(int64_t display, int64_t layer, const common::FRect& crop) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerSourceCrop(mDevice, displayId, layerId, toHwcFRect(crop));
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerTransform(int64_t display, int64_t layer, int32_t transform) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerTransform(mDevice, displayId, layerId, transform);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerVisibleRegion(int64_t display, int64_t layer,
                            const std::vector<std::optional<common::Rect>>& visibleRegion) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);

    std::vector<hwc_rect_t> visible;
    for (auto item : visibleRegion) {
        if (item.has_value())
            visible.push_back(toHwcRect(item.value()));
    }

    hwc_region_t region = {visible.size(), visible.data()};
    int32_t err = mDispatch.setLayerVisibleRegion(mDevice, displayId, layerId, region);
    return static_cast<HWC3::Error>(err);
}

HWC3::Error HwcHal::setLayerZOrder(int64_t display, int64_t layer, uint32_t z) {
    GET_DISPLAY_ID(display);
    GET_LAYER_ID(layer);
    int32_t err = mDispatch.setLayerZOrder(mDevice, displayId, layerId, z);
    return static_cast<HWC3::Error>(err);
}

// TODO: implement 2.2 ~ 2.4 mandatory interfaces
HWC3::Error HwcHal::getDisplayIdentificationData(int64_t /* display */, uint8_t* /* outPort */,
            std::vector<uint8_t>* /* outData */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::getPerFrameMetadataKeys(int64_t /* display */,
            std::vector<PerFrameMetadataKey>* /* outKeys */) {

    return HWC3::Error::None;
}

HWC3::Error HwcHal::getRenderIntents(int64_t /* display */, ColorMode /* mode */,
            std::vector<RenderIntent>* /* outIntents */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::setLayerPerFrameMetadata(int64_t /* display */, int64_t /* layer */,
        const std::vector<std::optional<PerFrameMetadata>>& /* metadata */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayCapabilities(int64_t /* display */,
        std::vector<DisplayCapability>* /* outCapabilities */) {

    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayConnectionType(int64_t /* display */,
        DisplayConnectionType* /* outType */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::getDisplayVsyncPeriod(int64_t /* display */,
        int32_t* /* outVsyncPeriod */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::getSupportedContentTypes(int64_t /* display */,
            std::vector<ContentType>* /* outSupportedContentTypes */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::setActiveConfigWithConstraints(int64_t /* display */, int32_t /* config */,
        const VsyncPeriodChangeConstraints& /* vsyncPeriodChangeConstraints */,
        VsyncPeriodChangeTimeline* /* timeline */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::setAutoLowLatencyMode(int64_t /* display */, bool /* on */) {
    return HWC3::Error::None;
}

HWC3::Error HwcHal::setContentType(int64_t /* display */,
        ContentType /* contentType */) {
    return HWC3::Error::None;
}

void HwcHal::initCapabilities() {
    uint32_t count = 0;
    mDevice->getCapabilities(mDevice, &count, nullptr);

    std::vector<int32_t> caps(count);
    mDevice->getCapabilities(mDevice, &count, caps.data());
    caps.resize(count);

    mCapabilities.reserve(count);
    for (auto cap : caps) {
        mCapabilities.insert(static_cast<hwc2_capability_t>(cap));
    }
}

template <typename T>
bool HwcHal::initDispatch(hwc2_function_descriptor_t desc, T* outPfn) {
    auto pfn = mDevice->getFunction(mDevice, desc);
    if (pfn) {
        *outPfn = reinterpret_cast<T>(pfn);
        return true;
    } else {
        ALOGE("failed to get hwcomposer2 function %d", desc);
        return false;
    }
}

template <typename T>
bool HwcHal::initOptionalDispatch(hwc2_function_descriptor_t desc, T* outPfn) {
    auto pfn = mDevice->getFunction(mDevice, desc);
    if (pfn) {
        *outPfn = reinterpret_cast<T>(pfn);
        return true;
    } else {
        return false;
    }
}

bool HwcHal::initDispatch() {
    if (!initDispatch(HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES, &mDispatch.acceptDisplayChanges) ||
        !initDispatch(HWC2_FUNCTION_CREATE_LAYER, &mDispatch.createLayer) ||
        !initDispatch(HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY, &mDispatch.createVirtualDisplay) ||
        !initDispatch(HWC2_FUNCTION_DESTROY_LAYER, &mDispatch.destroyLayer) ||
        !initDispatch(HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY,
                      &mDispatch.destroyVirtualDisplay) ||
        !initDispatch(HWC2_FUNCTION_DUMP, &mDispatch.dump) ||
        !initDispatch(HWC2_FUNCTION_GET_ACTIVE_CONFIG, &mDispatch.getActiveConfig) ||
        !initDispatch(HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES,
                      &mDispatch.getChangedCompositionTypes) ||
        !initDispatch(HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT,
                      &mDispatch.getClientTargetSupport) ||
        !initDispatch(HWC2_FUNCTION_GET_COLOR_MODES, &mDispatch.getColorModes) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE, &mDispatch.getDisplayAttribute) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_CONFIGS, &mDispatch.getDisplayConfigs) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_NAME, &mDispatch.getDisplayName) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_REQUESTS, &mDispatch.getDisplayRequests) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_TYPE, &mDispatch.getDisplayType) ||
        !initDispatch(HWC2_FUNCTION_GET_DOZE_SUPPORT, &mDispatch.getDozeSupport) ||
        !initDispatch(HWC2_FUNCTION_GET_HDR_CAPABILITIES, &mDispatch.getHdrCapabilities) ||
        !initDispatch(HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT,
                      &mDispatch.getMaxVirtualDisplayCount) ||
        !initDispatch(HWC2_FUNCTION_GET_RELEASE_FENCES, &mDispatch.getReleaseFences) ||
        !initDispatch(HWC2_FUNCTION_PRESENT_DISPLAY, &mDispatch.presentDisplay) ||
        !initDispatch(HWC2_FUNCTION_REGISTER_CALLBACK, &mDispatch.registerCallback) ||
        !initDispatch(HWC2_FUNCTION_SET_ACTIVE_CONFIG, &mDispatch.setActiveConfig) ||
        !initDispatch(HWC2_FUNCTION_SET_CLIENT_TARGET, &mDispatch.setClientTarget) ||
        !initDispatch(HWC2_FUNCTION_SET_COLOR_MODE, &mDispatch.setColorMode) ||
        !initDispatch(HWC2_FUNCTION_SET_COLOR_TRANSFORM, &mDispatch.setColorTransform) ||
        !initDispatch(HWC2_FUNCTION_SET_CURSOR_POSITION, &mDispatch.setCursorPosition) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_BLEND_MODE, &mDispatch.setLayerBlendMode) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_BUFFER, &mDispatch.setLayerBuffer) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_COLOR, &mDispatch.setLayerColor) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE,
                      &mDispatch.setLayerCompositionType) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_DATASPACE, &mDispatch.setLayerDataspace) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME, &mDispatch.setLayerDisplayFrame) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA, &mDispatch.setLayerPlaneAlpha)) {
        return false;
    }

    if (hasCapability(HWC2_CAPABILITY_SIDEBAND_STREAM)) {
        if (!initDispatch(HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM,
                          &mDispatch.setLayerSidebandStream)) {
            return false;
        }
    }

    if (!initDispatch(HWC2_FUNCTION_SET_LAYER_SOURCE_CROP, &mDispatch.setLayerSourceCrop) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE,
                      &mDispatch.setLayerSurfaceDamage) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_TRANSFORM, &mDispatch.setLayerTransform) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION,
                      &mDispatch.setLayerVisibleRegion) ||
        !initDispatch(HWC2_FUNCTION_SET_LAYER_Z_ORDER, &mDispatch.setLayerZOrder) ||
        !initDispatch(HWC2_FUNCTION_SET_OUTPUT_BUFFER, &mDispatch.setOutputBuffer) ||
        !initDispatch(HWC2_FUNCTION_SET_POWER_MODE, &mDispatch.setPowerMode) ||
        !initDispatch(HWC2_FUNCTION_SET_VSYNC_ENABLED, &mDispatch.setVsyncEnabled) ||
        !initDispatch(HWC2_FUNCTION_VALIDATE_DISPLAY, &mDispatch.validateDisplay)) {
        return false;
    }

    /* 2.2 interface */
    initOptionalDispatch(HWC2_FUNCTION_GET_PER_FRAME_METADATA_KEYS,
            &mDispatch.getPerFrameMetadataKeys);
    initOptionalDispatch(HWC2_FUNCTION_GET_RENDER_INTENTS, &mDispatch.getRenderIntents);
    initOptionalDispatch(HWC2_FUNCTION_SET_LAYER_PER_FRAME_METADATA,
            &mDispatch.setLayerPerFrameMetadata);

    /* 2.3 interface */
    initOptionalDispatch(HWC2_FUNCTION_GET_DISPLAY_IDENTIFICATION_DATA,
            &mDispatch.getDisplayIdentificationData);

    /* 2.4 interface */
    if (!initDispatch(HWC2_FUNCTION_GET_DISPLAY_CAPABILITIES,
                &mDispatch.getDisplayCapabilities) ||
        !initDispatch(HWC2_FUNCTION_GET_DISPLAY_VSYNC_PERIOD,
            &mDispatch.getDisplayVsyncPeriod) ||
        !initDispatch(HWC2_FUNCTION_SET_ACTIVE_CONFIG_WITH_CONSTRAINTS,
                &mDispatch.setActiveConfigWithConstraints)) {
        return false;
    }

    initOptionalDispatch(HWC2_FUNCTION_GET_DISPLAY_CONNECTION_TYPE,
            &mDispatch.getDisplayConnectionType);
    initOptionalDispatch(HWC2_FUNCTION_GET_SUPPORTED_CONTENT_TYPES,
            &mDispatch.getSupportedContentTypes);
    initOptionalDispatch(HWC2_FUNCTION_SET_AUTO_LOW_LATENCY_MODE,
            &mDispatch.setAutoLowLatencyMode);
    initOptionalDispatch(HWC2_FUNCTION_SET_CONTENT_TYPE, &mDispatch.setContentType);
    initOptionalDispatch(HWC2_FUNCTION_GET_CLIENT_TARGET_PROPERTY,
            &mDispatch.getClientTargetProperty);

    return true;
}

int32_t HwcHal::getChangedCompositionTypes(int64_t display, uint32_t* outTypesCount,
                                           int64_t* outChangedLayers,
                                           HWC2::Composition* outCompositionTypes) {
    return getChangedCompositionTypesInternal(display, outTypesCount, outChangedLayers,
                                              outCompositionTypes);
}

void HwcHal::hotplugHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                        int32_t connected) {
    auto hal = static_cast<HwcHal*>(callbackData);
    hal->mEventCallback->onHotplug(static_cast<int64_t>(display),
                                   static_cast<HWC2::Connection>(connected));
}

void HwcHal::refreshHook(hwc2_callback_data_t callbackData, hwc2_display_t display) {
    auto hal = static_cast<HwcHal*>(callbackData);
    hal->mEventCallback->onRefresh(static_cast<int64_t>(display));
}

void HwcHal::vsyncHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                      int64_t timestamp) {
    auto hal = static_cast<HwcHal*>(callbackData);
    hal->mEventCallback->onVsync(static_cast<int64_t>(display), timestamp);
}

int32_t HwcHal::getChangedCompositionTypesInternal(int64_t display, uint32_t* outTypesCount,
                                           int64_t* outChangedLayers,
                                           HWC2::Composition* outCompositionTypes) {
    GET_DISPLAY_ID(display);
    return mDispatch.getChangedCompositionTypes(
            mDevice, displayId, outTypesCount,
            reinterpret_cast<hwc2_layer_t*>(outChangedLayers),
            reinterpret_cast<std::underlying_type<HWC2::Composition>::type*>(
                    outCompositionTypes));
}

}
}  // namespace aidl::android::hardware::graphics::composer3::impl
