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

#include <string>
#include <vector>

// TODO remove hwcomposer2.h dependency
#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include <Common.h>
namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

class ComposerHal {
public:
    virtual ~ComposerHal() = default;

    virtual bool init() = 0;
    virtual bool hasCapability(hwc2_capability_t capability) = 0;
    // dump the debug information
    virtual std::string dump() = 0;

    class EventCallback {
    public:
        virtual ~EventCallback() = default;
        virtual void onHotplug(int64_t display, HWC2::Connection connected) = 0;
        virtual void onRefresh(int64_t display) = 0;
        virtual void onVsync(int64_t display, int64_t timestamp,
                             uint32_t vsyncPeriodNanos) = 0;
        virtual void onVsyncPeriodTimingChanged(int64_t display,
                                                const VsyncPeriodChangeTimeline& timeline) = 0;
        virtual void onSeamlessPossible(int64_t display) = 0;
    };

    // Register the event callback object.  The event callback object is valid
    // until it is unregistered.  A hotplug event must be generated for each
    // connected physical display, before or after this function returns.
    virtual void registerEventCallback(EventCallback* callback) = 0;

    // Unregister the event callback object.  It must block if there is any
    // callback in-flight.
    virtual void unregisterEventCallback() = 0;

    virtual uint32_t getMaxVirtualDisplayCount() = 0;
    virtual HWC3::Error createVirtualDisplay(uint32_t width, uint32_t height,
                                    common::PixelFormat* format, int64_t* outDisplay) = 0;
    virtual HWC3::Error destroyVirtualDisplay(int64_t display) = 0;
    virtual HWC3::Error createLayer(int64_t display, int64_t* outLayer) = 0;
    virtual HWC3::Error destroyLayer(int64_t display, int64_t layer) = 0;

    virtual HWC3::Error getActiveConfig(int64_t display, int32_t* outConfig) = 0;
    virtual HWC3::Error getClientTargetSupport(int64_t display, uint32_t width, uint32_t height,
                                    common::PixelFormat format, common::Dataspace dataspace) = 0;
    virtual HWC3::Error getColorModes(int64_t display, std::vector<ColorMode>* outModes) = 0;
    virtual HWC3::Error getDisplayAttribute(int64_t display, int32_t config,
                                    int32_t attribute, int32_t* outValue) = 0;
    virtual HWC3::Error getDisplayConfigs(int64_t display, std::vector<int32_t>* outConfigs) = 0;
    virtual HWC3::Error getDisplayName(int64_t display, std::string* outName) = 0;
    virtual HWC3::Error getDisplayType(int64_t display, HWC2::DisplayType* outType) = 0;
    virtual HWC3::Error getDozeSupport(int64_t display, bool* outSupport) = 0;
    virtual HWC3::Error getHdrCapabilities(int64_t display, std::vector<common::Hdr>* outTypes,
                                    float* outMaxLuminance, float* outMaxAverageLuminance,
                                    float* outMinLuminance) = 0;

    virtual HWC3::Error setActiveConfig(int64_t display, int32_t config) = 0;
    virtual HWC3::Error setColorMode(int64_t display, ColorMode mode, RenderIntent intent) = 0;
    virtual HWC3::Error setPowerMode(int64_t display, PowerMode mode) = 0;
    virtual HWC3::Error setVsyncEnabled(int64_t display, bool enabled) = 0;

    virtual HWC3::Error setColorTransform(int64_t display, const float* matrix, int32_t hint) = 0;
    virtual HWC3::Error setClientTarget(int64_t display, buffer_handle_t target, int32_t acquireFence,
                                    int32_t dataspace, const std::vector<common::Rect>& damage) = 0;
    virtual HWC3::Error setOutputBuffer(int64_t display, buffer_handle_t buffer,
                                    int32_t releaseFence) = 0;
    /* 2.4 */
    virtual HWC3::Error validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                                    std::vector<Composition>* outCompositionTypes,
                                    uint32_t* outDisplayRequestMask,
                                    std::vector<int64_t>* outRequestedLayers,
                                    std::vector<uint32_t>* outRequestMasks,
                                    ClientTargetProperty* outClientTargetProperty) = 0;
    virtual HWC3::Error acceptDisplayChanges(int64_t display) = 0;
    virtual HWC3::Error presentDisplay(int64_t display, int32_t* outPresentFence,
                                    std::vector<int64_t>* outLayers,
                                    std::vector<int32_t>* outReleaseFences) = 0;

    virtual HWC3::Error setLayerCursorPosition(int64_t display, int64_t layer, int32_t x, int32_t y) = 0;
    virtual HWC3::Error setLayerBuffer(int64_t display, int64_t layer, buffer_handle_t buffer,
                                    int32_t acquireFence) = 0;
    virtual HWC3::Error setLayerSurfaceDamage(int64_t display, int64_t layer,
                                    const std::vector<std::optional<common::Rect>>& damage) = 0;
    virtual HWC3::Error setLayerBlendMode(int64_t display, int64_t layer, int32_t mode) = 0;
    virtual HWC3::Error setLayerColor(int64_t display, int64_t layer, Color color) = 0;
    virtual HWC3::Error setLayerCompositionType(int64_t display, int64_t layer, int32_t type) = 0;
    virtual HWC3::Error setLayerDataspace(int64_t display, int64_t layer, int32_t dataspace) = 0;
    virtual HWC3::Error setLayerDisplayFrame(int64_t display, int64_t layer, const common::Rect& frame) = 0;
    virtual HWC3::Error setLayerPlaneAlpha(int64_t display, int64_t layer, float alpha) = 0;
    virtual HWC3::Error setLayerSidebandStream(int64_t display, int64_t layer, buffer_handle_t stream) = 0;
    virtual HWC3::Error setLayerSourceCrop(int64_t display, int64_t layer, const common::FRect& crop) = 0;
    virtual HWC3::Error setLayerTransform(int64_t display, int64_t layer, int32_t transform) = 0;
    virtual HWC3::Error setLayerVisibleRegion(int64_t display, int64_t layer,
                                    const std::vector<std::optional<common::Rect>>& visibleRegion) = 0;
    virtual HWC3::Error setLayerZOrder(int64_t display, int64_t layer, uint32_t z) = 0;

    /* hwc hal 2.2 interface */
    virtual HWC3::Error getPerFrameMetadataKeys(int64_t display,
            std::vector<PerFrameMetadataKey>* outKeys) = 0;
    virtual HWC3::Error getRenderIntents(int64_t display, ColorMode mode,
            std::vector<RenderIntent>* outIntents) = 0;

    virtual HWC3::Error setLayerPerFrameMetadata(int64_t display, int64_t layer,
            const std::vector<std::optional<PerFrameMetadata>>& metadata) = 0;

    /* hwc hal 2.3 interface */
    virtual HWC3::Error getDisplayIdentificationData(int64_t display, uint8_t* outPort,
            std::vector<uint8_t>* outData) = 0;

    /* hwc hal 2.4 interface */
    virtual HWC3::Error getDisplayCapabilities(int64_t display, std::vector<DisplayCapability>* outCapabilities) = 0;
    virtual HWC3::Error getDisplayConnectionType(int64_t display, DisplayConnectionType* outType) = 0;
    virtual HWC3::Error getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) = 0;
    virtual HWC3::Error getSupportedContentTypes(int64_t display,
            std::vector<ContentType>* outSupportedContentTypes) = 0;

    virtual HWC3::Error setActiveConfigWithConstraints(int64_t display, int32_t config,
            const VsyncPeriodChangeConstraints& vsyncPeriodChangeConstraints,
            VsyncPeriodChangeTimeline* timeline) = 0;
    virtual HWC3::Error setAutoLowLatencyMode(int64_t display, bool on) = 0;
    virtual HWC3::Error setContentType(int64_t display, ContentType contentType) = 0;
};

}
} // namespace aidl::android::hardware::graphics::composer3::impl
