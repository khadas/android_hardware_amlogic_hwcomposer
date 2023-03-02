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

#pragma once

#include <atomic>
#include <cstring>  // for strerror
#include <type_traits>
#include <unordered_set>

#include <log/log.h>
#include "ComposerHal.h"

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

class HwcHal : public ComposerHal {
public:
    virtual ~HwcHal();

    bool init() override;
    bool hasCapability(hwc2_capability_t capability) override;
    std::string dump() override;
    void registerEventCallback(ComposerHal::EventCallback* callback) override;
    void unregisterEventCallback() override;
    uint32_t getMaxVirtualDisplayCount() override;
    HWC3::Error createVirtualDisplay(uint32_t width, uint32_t height, common::PixelFormat* format,
                               int64_t* outDisplay) override;
    HWC3::Error destroyVirtualDisplay(int64_t display) override;

    HWC3::Error createLayer(int64_t display, int64_t* outLayer) override;
    HWC3::Error destroyLayer(int64_t display, int64_t layer) override;
    HWC3::Error getActiveConfig(int64_t display, int32_t* outConfig) override;
    HWC3::Error getClientTargetSupport(int64_t display, uint32_t width, uint32_t height,
                                 common::PixelFormat format, common::Dataspace dataspace) override;
    HWC3::Error getColorModes(int64_t display, std::vector<ColorMode>* outModes) override;

    HWC3::Error getDisplayAttribute(int64_t display, int32_t config, int32_t attribute,
                              int32_t* outValue) override;
    HWC3::Error getDisplayConfigs(int64_t display, std::vector<int32_t>* outConfigs) override;
    HWC3::Error getDisplayName(int64_t display, std::string* outName) override;
    HWC3::Error getDisplayType(int64_t display, HWC2::DisplayType* outType) override;
    HWC3::Error getDozeSupport(int64_t display, bool* outSupport) override;
    HWC3::Error getHdrCapabilities(int64_t display, std::vector<common::Hdr>* outTypes, float* outMaxLuminance,
                             float* outMaxAverageLuminance, float* outMinLuminance) override;
    HWC3::Error setActiveConfig(int64_t display, int32_t config) override;
    HWC3::Error setColorMode(int64_t display, ColorMode mode, RenderIntent intent) override;
    HWC3::Error setPowerMode(int64_t display, PowerMode mode) override;
    HWC3::Error setVsyncEnabled(int64_t display, bool enabled) override;
    HWC3::Error setColorTransform(int64_t display, const float* matrix, int32_t hint) override;

    HWC3::Error setClientTarget(int64_t display, buffer_handle_t target, int32_t acquireFence,
                          int32_t dataspace, const std::vector<common::Rect>& damage) override;
    HWC3::Error setOutputBuffer(int64_t display, buffer_handle_t buffer, int32_t releaseFence) override;
    HWC3::Error validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                          std::vector<Composition>* outCompositionTypes,
                          uint32_t* outDisplayRequestMask, std::vector<int64_t>* outRequestedLayers,
                          std::vector<uint32_t>* outRequestMasks,
                          ClientTargetProperty* outClientTargetProperty) override;

    HWC3::Error acceptDisplayChanges(int64_t display) override;
    HWC3::Error presentDisplay(int64_t display, int32_t* outPresentFence, std::vector<int64_t>* outLayers,
                         std::vector<int32_t>* outReleaseFences) override;
    HWC3::Error setLayerCursorPosition(int64_t display, int64_t layer, int32_t x, int32_t y) override;
    HWC3::Error setLayerBuffer(int64_t display, int64_t layer, buffer_handle_t buffer,
                         int32_t acquireFence) override;
    HWC3::Error setLayerSurfaceDamage(int64_t display, int64_t layer,
                                const std::vector<std::optional<common::Rect>>& damage) override;
    HWC3::Error setLayerBlendMode(int64_t display, int64_t layer, int32_t mode) override;
    HWC3::Error setLayerColor(int64_t display, int64_t layer, Color color) override;
    HWC3::Error setLayerCompositionType(int64_t display, int64_t layer, int32_t type) override;
    HWC3::Error setLayerDataspace(int64_t display, int64_t layer, int32_t dataspace) override;
    HWC3::Error setLayerDisplayFrame(int64_t display, int64_t layer, const common::Rect& frame) override;
    HWC3::Error setLayerPlaneAlpha(int64_t display, int64_t layer, float alpha) override;
    HWC3::Error setLayerSidebandStream(int64_t display, int64_t layer, buffer_handle_t stream) override;
    HWC3::Error setLayerSourceCrop(int64_t display, int64_t layer, const common::FRect& crop) override;
    HWC3::Error setLayerTransform(int64_t display, int64_t layer, int32_t transform) override;
    HWC3::Error setLayerVisibleRegion(int64_t display, int64_t layer,
                                const std::vector<std::optional<common::Rect>>& visibleRegion) override;
    HWC3::Error setLayerZOrder(int64_t display, int64_t layer, uint32_t z) override;

    /* hwc2.2 */
    HWC3::Error getPerFrameMetadataKeys(int64_t display,
            std::vector<PerFrameMetadataKey>* outKeys) override;
    HWC3::Error getRenderIntents(int64_t display, ColorMode mode,
            std::vector<RenderIntent>* outIntents) override;
    HWC3::Error setLayerPerFrameMetadata(int64_t display, int64_t layer,
            const std::vector<std::optional<PerFrameMetadata>>& metadata) override;

    /* hwc2.3 interface */
    HWC3::Error getDisplayIdentificationData(int64_t display, uint8_t* outPort,
            std::vector<uint8_t>* outData) override;

    /* hwc2.4 interface */
    HWC3::Error getDisplayCapabilities(int64_t display, std::vector<DisplayCapability>* outCapabilities) override;
    HWC3::Error getDisplayConnectionType(int64_t display, DisplayConnectionType* outType) override;
    HWC3::Error getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) override;
    HWC3::Error getSupportedContentTypes(int64_t display,
            std::vector<ContentType>* outSupportedContentTypes) override;

    HWC3::Error setActiveConfigWithConstraints(int64_t display, int32_t config,
            const VsyncPeriodChangeConstraints& vsyncPeriodChangeConstraints,
            VsyncPeriodChangeTimeline* timeline) override;
    HWC3::Error setAutoLowLatencyMode(int64_t display, bool on) override;
    HWC3::Error setContentType(int64_t display, ContentType contentType) override;

    /* hwc3 interface */
    HWC3::Error setBootDisplayConfig(int64_t displayId, int32_t config) override;
    HWC3::Error clearBootDisplayConfig(int64_t displayId) override;
    HWC3::Error getPreferredBootDisplayConfig(int64_t displayId, int32_t* config) override;
    HWC3::Error getDisplayPhysicalOrientation(int64_t display, common::Transform* orientation) override;
    HWC3::Error setExpectedPresentTime(int64_t display, const int64_t expectedPresentTime) override;
    HWC3::Error setLayerBrightness(int64_t display, int64_t layer, const LayerBrightness& brightness) override;

    /* hwc3.2 interface */
#if (PLATFORM_SDK_VERSION > 33) || (PLATFORM_SDK_VERSION == 33 \
            && (ANDROID_PLATFORM_SDK_EXTENSION_VERSION >= 5))
    HWC3::Error getHdrConversionCapabilities(
        std::vector<common::HdrConversionCapability>*) override;
    HWC3::Error setHdrConversionStrategy(
        const common::HdrConversionStrategy& conversionStrategy,
        common::Hdr* preferredHdrOutputType) override;
    HWC3::Error getOverlaySupport(OverlayProperties* properties) override;
#endif

    /* extern interface */
    HWC3::Error setAidlClientPid(int32_t pid) override;

protected:
    virtual void initCapabilities();

    template <typename T>
        bool initDispatch(hwc2_function_descriptor_t desc, T* outPfn);
    template <typename T>
        bool initHwc3Dispatch(hwc3_function_descriptor_t desc, T* outPfn);
    virtual bool initDispatch();
    template <typename T>
        bool initOptionalDispatch(hwc2_function_descriptor_t desc, T* outPfn);

    virtual int32_t getChangedCompositionTypes(int64_t display, uint32_t* outTypesCount,
                                               int64_t* outChangedLayers,
                                               HWC2::Composition* outCompositionTypes);
    virtual void onLayerDestroyed(int64_t /* display */, int64_t /* layer */) {}
    virtual void onBeforeValidateDisplay(int64_t /* display */) {}
    static void hotplugHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                            int32_t connected);
    static void refreshHook(hwc2_callback_data_t callbackData, hwc2_display_t display);
    static void vsyncHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                          int64_t timestamp, uint32_t vsyncPeriodNanos);
    static void vsyncPeriodTimingChangedHook(hwc2_callback_data_t callbackData,
                                             hwc2_display_t display,
                                             hwc_vsync_period_change_timeline_t* updated_timeline);
    static void seamlessPossibleHook(hwc2_callback_data_t callbackData, hwc2_display_t display);

    int32_t getChangedCompositionTypesInternal(int64_t display, uint32_t* outTypesCount,
                                               int64_t* outChangedLayers,
                                               HWC2::Composition* outCompositionTypes);

protected:
    hwc2_device_t* mDevice = nullptr;
    std::unordered_set<hwc2_capability_t> mCapabilities;

    struct {
        HWC2_PFN_ACCEPT_DISPLAY_CHANGES acceptDisplayChanges;
        HWC2_PFN_CREATE_LAYER createLayer;
        HWC2_PFN_CREATE_VIRTUAL_DISPLAY createVirtualDisplay;
        HWC2_PFN_DESTROY_LAYER destroyLayer;
        HWC2_PFN_DESTROY_VIRTUAL_DISPLAY destroyVirtualDisplay;
        HWC2_PFN_DUMP dump;
        HWC2_PFN_GET_ACTIVE_CONFIG getActiveConfig;
        HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES getChangedCompositionTypes;
        HWC2_PFN_GET_CLIENT_TARGET_SUPPORT getClientTargetSupport;
        HWC2_PFN_GET_COLOR_MODES getColorModes;
        HWC2_PFN_GET_DISPLAY_ATTRIBUTE getDisplayAttribute;
        HWC2_PFN_GET_DISPLAY_CONFIGS getDisplayConfigs;
        HWC2_PFN_GET_DISPLAY_NAME getDisplayName;
        HWC2_PFN_GET_DISPLAY_REQUESTS getDisplayRequests;
        HWC2_PFN_GET_DISPLAY_TYPE getDisplayType;
        HWC2_PFN_GET_DOZE_SUPPORT getDozeSupport;
        HWC2_PFN_GET_HDR_CAPABILITIES getHdrCapabilities;
        HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT getMaxVirtualDisplayCount;
        HWC2_PFN_GET_RELEASE_FENCES getReleaseFences;
        HWC2_PFN_PRESENT_DISPLAY presentDisplay;
        HWC2_PFN_REGISTER_CALLBACK registerCallback;
        HWC2_PFN_SET_ACTIVE_CONFIG setActiveConfig;
        HWC2_PFN_SET_CLIENT_TARGET setClientTarget;
        HWC2_PFN_SET_COLOR_MODE setColorMode;
        HWC2_PFN_SET_COLOR_TRANSFORM setColorTransform;
        HWC2_PFN_SET_CURSOR_POSITION setCursorPosition;
        HWC2_PFN_SET_LAYER_BLEND_MODE setLayerBlendMode;
        HWC2_PFN_SET_LAYER_BUFFER setLayerBuffer;
        HWC2_PFN_SET_LAYER_COLOR setLayerColor;
        HWC2_PFN_SET_LAYER_COMPOSITION_TYPE setLayerCompositionType;
        HWC2_PFN_SET_LAYER_DATASPACE setLayerDataspace;
        HWC2_PFN_SET_LAYER_DISPLAY_FRAME setLayerDisplayFrame;
        HWC2_PFN_SET_LAYER_PLANE_ALPHA setLayerPlaneAlpha;
        HWC2_PFN_SET_LAYER_SIDEBAND_STREAM setLayerSidebandStream;
        HWC2_PFN_SET_LAYER_SOURCE_CROP setLayerSourceCrop;
        HWC2_PFN_SET_LAYER_SURFACE_DAMAGE setLayerSurfaceDamage;
        HWC2_PFN_SET_LAYER_TRANSFORM setLayerTransform;
        HWC2_PFN_SET_LAYER_VISIBLE_REGION setLayerVisibleRegion;
        HWC2_PFN_SET_LAYER_Z_ORDER setLayerZOrder;
        HWC2_PFN_SET_OUTPUT_BUFFER setOutputBuffer;
        HWC2_PFN_SET_POWER_MODE setPowerMode;
        HWC2_PFN_SET_VSYNC_ENABLED setVsyncEnabled;
        HWC2_PFN_VALIDATE_DISPLAY validateDisplay;
        /* hwc 2.2 */
        HWC2_PFN_GET_PER_FRAME_METADATA_KEYS getPerFrameMetadataKeys;
        HWC2_PFN_GET_RENDER_INTENTS getRenderIntents;
        HWC2_PFN_SET_LAYER_PER_FRAME_METADATA setLayerPerFrameMetadata;

        /* hwc 2.3 */
        HWC2_PFN_GET_DISPLAY_IDENTIFICATION_DATA getDisplayIdentificationData;

        /* hwc 2.4 */
        HWC2_PFN_GET_DISPLAY_CAPABILITIES getDisplayCapabilities;
        HWC2_PFN_GET_DISPLAY_CONNECTION_TYPE getDisplayConnectionType;
        HWC2_PFN_GET_DISPLAY_VSYNC_PERIOD getDisplayVsyncPeriod;
        HWC2_PFN_GET_SUPPORTED_CONTENT_TYPES getSupportedContentTypes;
        HWC2_PFN_SET_ACTIVE_CONFIG_WITH_CONSTRAINTS setActiveConfigWithConstraints;
        HWC2_PFN_SET_AUTO_LOW_LATENCY_MODE setAutoLowLatencyMode;
        HWC2_PFN_SET_CONTENT_TYPE setContentType;
        HWC2_PFN_GET_CLIENT_TARGET_PROPERTY getClientTargetProperty;

        /* hwc 3 */
        HWC3_PFN_SET_BOOT_DISPLAY_CONFIG setBootDisplayConfig;
        HWC3_PFN_CLEAR_BOOT_DISPLAY_CONFIG clearBootDisplayConfig;
        HWC3_PFN_GET_PREFERRED_BOOT_DISPLAY_CONFIG getPreferredBootDisplayConfig;
        HWC3_PFN_GET_DISPLAY_PHYSICAL_ORIENTATION getDisplayPhysicalOrientation;
        HWC3_PFN_SET_EXPECTED_PRESENT_TIME setExpectedPresentTime;
        HWC3_PFN_SET_LAYER_BRIGHTNESS setLayerBrightness;

        /* hwc 3.2*/
        HWC3_PFN_SET_HDR_CONVERSION_STRATEGY setHdrConversionStrategy;
        HWC3_PFN_GET_HDR_CONVERSION_CAPABILITIES getHdrConversionCapabilities;
        HWC3_PFN_GET_OVERLAY_SUPPORT getOverlaySupport;
        /* extern */
        HWC3_PFN_SET_AIDL_CLIENT_PID setAidlClientPid;
    } mDispatch = {};

    ComposerHal::EventCallback* mEventCallback = nullptr;
};

}

}  // namespace aidl::android::hardware::graphics::composer3::impl
