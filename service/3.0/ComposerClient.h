/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <utils/Mutex.h>

#include <memory>
#include <android-base/unique_fd.h>

#include "ComposerHal.h"
#include "ComposerResources.h"
//#include "int64_t.h"

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

class ComposerClient : public BnComposerClient {
public:
    ComposerClient();
    ComposerClient(ComposerHal *hal);
    virtual ~ComposerClient();

    HWC3::Error init();

    void setOnClientDestroyed(std::function<void()> onClientDestroyed) {
      mOnClientDestroyed = onClientDestroyed;
    }

    // HWC3 interface:
    ndk::ScopedAStatus createLayer(int64_t displayId, int32_t bufferSlotCount,
                                   int64_t* layer) override;
    ndk::ScopedAStatus createVirtualDisplay(int32_t width, int32_t height,
                                            common::PixelFormat formatHint,
                                            int32_t outputBufferSlotCount,
                                            VirtualDisplay* display) override;
    ndk::ScopedAStatus destroyLayer(int64_t displayId, int64_t layer) override;
    ndk::ScopedAStatus destroyVirtualDisplay(int64_t displayId) override;

    ndk::ScopedAStatus executeCommands(
        const std::vector<DisplayCommand>& commands,
        std::vector<CommandResultPayload>* results) override;
    ndk::ScopedAStatus getActiveConfig(int64_t displayId,
                                       int32_t* config) override;
    ndk::ScopedAStatus getColorModes(int64_t displayId,
                                     std::vector<ColorMode>* colorModes) override;
    ndk::ScopedAStatus getDataspaceSaturationMatrix(
        common::Dataspace dataspace, std::vector<float>* matrix) override;
    ndk::ScopedAStatus getDisplayAttribute(int64_t displayId, int32_t config,
                                           DisplayAttribute attribute,
                                           int32_t* value) override;
    ndk::ScopedAStatus getDisplayConfigs(int64_t displayId,
                                         std::vector<int32_t>* configs) override;
    ndk::ScopedAStatus getDisplayCapabilities(
        int64_t displayId, std::vector<DisplayCapability>* caps) override;
    ndk::ScopedAStatus getDisplayConnectionType(
        int64_t displayId, DisplayConnectionType* type) override;
    ndk::ScopedAStatus getDisplayIdentificationData(
        int64_t displayId, DisplayIdentification* id) override;
    ndk::ScopedAStatus getDisplayName(int64_t displayId,
                                      std::string* name) override;
    ndk::ScopedAStatus getDisplayVsyncPeriod(int64_t displayId,
                                             int32_t* vsyncPeriod) override;
    ndk::ScopedAStatus getDisplayedContentSample(
        int64_t displayId, int64_t maxFrames, int64_t timestamp,
        DisplayContentSample* samples) override;
    ndk::ScopedAStatus getDisplayedContentSamplingAttributes(
        int64_t displayId, DisplayContentSamplingAttributes* attrs) override;
    ndk::ScopedAStatus getDisplayPhysicalOrientation(
        int64_t displayId, common::Transform* orientation) override;
    ndk::ScopedAStatus getHdrCapabilities(int64_t displayId,
                                          HdrCapabilities* caps) override;
    ndk::ScopedAStatus getMaxVirtualDisplayCount(int32_t* count) override;

    ndk::ScopedAStatus getPerFrameMetadataKeys(
        int64_t displayId, std::vector<PerFrameMetadataKey>* keys) override;
    ndk::ScopedAStatus getReadbackBufferAttributes(
        int64_t displayId, ReadbackBufferAttributes* attrs) override;
    ndk::ScopedAStatus getReadbackBufferFence(
        int64_t displayId, ndk::ScopedFileDescriptor* acquireFence) override;
    ndk::ScopedAStatus getRenderIntents(
        int64_t displayId, ColorMode mode,
        std::vector<RenderIntent>* intents) override;
    ndk::ScopedAStatus getSupportedContentTypes(
        int64_t displayId, std::vector<ContentType>* types) override;
    ndk::ScopedAStatus getDisplayDecorationSupport(int64_t displayId,
            std::optional<common::DisplayDecorationSupport>* support) override;
    ndk::ScopedAStatus registerCallback(
        const std::shared_ptr<IComposerCallback>& callback) override;
    ndk::ScopedAStatus setActiveConfig(int64_t displayId,
                                       int32_t config) override;
    ndk::ScopedAStatus setActiveConfigWithConstraints(
        int64_t displayId, int32_t config,
        const VsyncPeriodChangeConstraints& constraints,
        VsyncPeriodChangeTimeline* timeline) override;
    ndk::ScopedAStatus setBootDisplayConfig(int64_t displayId,
                                            int32_t config) override;
    ndk::ScopedAStatus clearBootDisplayConfig(int64_t displayId) override;
    ndk::ScopedAStatus getPreferredBootDisplayConfig(int64_t displayId,
                                                     int32_t* config) override;
    ndk::ScopedAStatus setAutoLowLatencyMode(int64_t displayId, bool on) override;

    ndk::ScopedAStatus setClientTargetSlotCount(int64_t displayId,
                                                int32_t count) override;
    ndk::ScopedAStatus setColorMode(int64_t displayId, ColorMode mode,
                                    RenderIntent intent) override;
    ndk::ScopedAStatus setContentType(int64_t displayId,
                                      ContentType type) override;
    ndk::ScopedAStatus setDisplayedContentSamplingEnabled(
        int64_t displayId, bool enable, FormatColorComponent componentMask,
        int64_t maxFrames) override;
    ndk::ScopedAStatus setReadbackBuffer(
        int64_t displayId,
        const aidl::android::hardware::common::NativeHandle& buffer,
        const ndk::ScopedFileDescriptor& releaseFence) override;
    ndk::ScopedAStatus setPowerMode(int64_t displayId, PowerMode mode) override;
    ndk::ScopedAStatus setVsyncEnabled(int64_t displayId, bool enabled) override;
    ndk::ScopedAStatus setIdleTimerEnabled(int64_t displayId,
                                           int32_t timeoutMs) override;

    /*HWC3-V2 interface*/
#if (PLATFORM_SDK_VERSION >= 34)
    ndk::ScopedAStatus getHdrConversionCapabilities(
            std::vector<common::HdrConversionCapability>*) override;
    ndk::ScopedAStatus setHdrConversionStrategy(const common::HdrConversionStrategy& conversionStrategy,
            aidl::android::hardware::graphics::common::Hdr* hdrType) override;
    ndk::ScopedAStatus getOverlaySupport(OverlayProperties* properties) override;
    ndk::ScopedAStatus setRefreshRateChangedCallbackDebugEnabled(int64_t displayId,
            bool enabled) override;
#endif

protected:
    ndk::SpAIBinder createBinder() override;

private:
    class CommandResultWriter;

    void executeDisplayCommand(const DisplayCommand& displayCommand);
    void executeLayerCommand(int64_t displayId, const LayerCommand& layerCommand);
    void executeDisplayCommandSetColorTransform(int64_t displayId,
                                                const std::vector<float>& matrix);
    void executeDisplayCommandSetBrightness(int64_t displayId,
                                            const DisplayBrightness& brightness);
    void executeDisplayCommandSetClientTarget(int64_t displayId,
                                              const ClientTarget& command);
    void executeDisplayCommandSetOutputBuffer(int64_t displayId,
                                              const Buffer& buffer);
    void executeDisplayCommandValidateDisplay(
        int64_t displayId,
        const std::optional<ClockMonotonicTimestamp> expectedPresentTime);
    void executeDisplayCommandAcceptDisplayChanges(int64_t displayId);
    void executeDisplayCommandPresentOrValidateDisplay(
        int64_t displayId,
        const std::optional<ClockMonotonicTimestamp> expectedPresentTime);
    void executeDisplayCommandPresentDisplay(int64_t displayId);

    void executeLayerCommandSetLayerCursorPosition(
        int64_t displayId, int64_t layerId, const common::Point& cursorPosition);
    void executeLayerCommandSetLayerBuffer(int64_t displayId, int64_t layerId,
                                           const Buffer& buffer);
#if (PLATFORM_SDK_VERSION >= 34)
    void executeLayerCommandSetLayerBufferSlotsToClear(int64_t displayId, int64_t layerId,
                                           const std::vector<int32_t>& slotsToClear);
#endif
    void executeLayerCommandSetLayerSurfaceDamage(
        int64_t displayId, int64_t layerId,
        const std::vector<std::optional<common::Rect>>& damage);
    void executeLayerCommandSetLayerBlendMode(
        int64_t displayId, int64_t layerId, const ParcelableBlendMode& blendMode);
    void executeLayerCommandSetLayerColor(int64_t displayId, int64_t layerId,
                                          const Color& color);
    void executeLayerCommandSetLayerComposition(
        int64_t displayId, int64_t layerId, const ParcelableComposition& composition);
    void executeLayerCommandSetLayerDataspace(
        int64_t displayId, int64_t layerId, const ParcelableDataspace& dataspace);
    void executeLayerCommandSetLayerDisplayFrame(int64_t displayId, int64_t layerId,
                                                 const common::Rect& rect);
    void executeLayerCommandSetLayerPlaneAlpha(int64_t displayId, int64_t layerId,
                                               const PlaneAlpha& planeAlpha);
    void executeLayerCommandSetLayerSidebandStream(
        int64_t displayId, int64_t layerId,
        const aidl::android::hardware::common::NativeHandle& sidebandStream);
    void executeLayerCommandSetLayerSourceCrop(int64_t displayId, int64_t layerId,
                                               const common::FRect& sourceCrop);
    void executeLayerCommandSetLayerTransform(
        int64_t displayId, int64_t layerId, const ParcelableTransform& transform);
    void executeLayerCommandSetLayerVisibleRegion(
        int64_t displayId, int64_t layerId,
        const std::vector<std::optional<common::Rect>>& visibleRegion);
    void executeLayerCommandSetLayerZOrder(int64_t displayId, int64_t layerId,
                                           const ZOrder& zOrder);
    void executeLayerCommandSetLayerPerFrameMetadata(
        int64_t displayId, int64_t layerId,
        const std::vector<std::optional<PerFrameMetadata>>& perFrameMetadata);
    void executeLayerCommandSetLayerColorTransform(
        int64_t displayId, int64_t layerId, const std::vector<float>& colorTransform);
    void executeLayerCommandSetLayerPerFrameMetadataBlobs(
        int64_t displayId, int64_t layerId,
        const std::vector<std::optional<PerFrameMetadataBlob>>&
            perFrameMetadataBlob);
    void executeLayerCommandSetLayerBrightness(
        int64_t display, int64_t layerId,
        const LayerBrightness& brightness);

    ::android::base::unique_fd getUniqueFd(const ndk::ScopedFileDescriptor& in);

    // release resources
    void destroyResources();

private:
    std::mutex mStateMutex;
    // The onHotplug(), onVsync(), etc callbacks registered by SurfaceFlinger.
    std::shared_ptr<IComposerCallback> mCallbacks;
    std::function<void()> mOnClientDestroyed;

    // For the duration of a executeCommands(), the helper used to collect
    // individual command results.
    std::unique_ptr<CommandResultWriter> mCommandResults;
    // Manages importing and caching gralloc buffers for displays and layers.
    std::unique_ptr<ComposerResources> mResources;

    ComposerHal *mHal = nullptr;
    std::unique_ptr<ComposerHal::EventCallback> mHalEventCallback;
};

}
}  // namespace aidl::android::hardware::graphics::composer3::impl

