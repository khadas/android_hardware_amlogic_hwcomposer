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

#include <aidlcommonsupport/NativeHandle.h>
#include <android/binder_ibinder_platform.h>
#include <math/mat4.h>

#include "Common.h"
#include "DisplayChanges.h"
#include "ComposerClient.h"

#define HWC2DISPLAY(displayId) static_cast<hwc2_display_t>(displayId)
#define HWC2LAYER(layerId) static_cast<hwc2_layer_t>(layerId)

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

HWC3::Error toHwc3Error(HWC2::Error error) {
    switch (error) {
    case HWC2::Error::None:
        return HWC3::Error::None;
    case HWC2::Error::BadConfig:
        return HWC3::Error::BadConfig;
    case HWC2::Error::BadDisplay:
        return HWC3::Error::BadDisplay;
    case HWC2::Error::BadLayer:
        return HWC3::Error::BadLayer;
    case HWC2::Error::BadParameter:
        return HWC3::Error::BadParameter;
    case HWC2::Error::NoResources:
        return HWC3::Error::NoResources;
    case HWC2::Error::NotValidated:
        return HWC3::Error::NotValidated;
    case HWC2::Error::Unsupported:
        return HWC3::Error::Unsupported;
    case HWC2::Error::SeamlessNotAllowed:
        return HWC3::Error::SeamlessNotAllowed;
    case HWC2::Error::SeamlessNotPossible:
        return HWC3::Error::SeamlessNotPossible;
    default:
        return HWC3::Error::BadParameter;
    }
}

using ::aidl::android::hardware::graphics::common::PixelFormat;
class HalEventCallback : public ComposerHal::EventCallback {
public:
    HalEventCallback(ComposerHal* hal, IComposerCallback* callback,
                          ComposerResources* resources,
                          ComposerClient* client)
             : mHal(hal), mCallback(callback),
               mResources(resources), mClient(client) {}

    void onHotplug(int64_t display, HWC2::Connection connected) {
        bool bCon = true;
        ALOGD("halEventcallback onHotplug client3.0");
        if (connected == HWC2::Connection::Connected) {
            if (mResources->hasDisplay(display)) {
                // This is a subsequent hotplug "connected" for a display. This signals a
                // display change and thus the framework may want to reallocate buffers. We
                // need to free all cached handles, since they are holding a strong reference
                // to the underlying buffers.
                size_t CacheSize = 0;
                mResources->getDisplayClientTargetCacheSize(display, &CacheSize);
                cleanDisplayResources(display);
                mResources->removeDisplay(display);
                mResources->addPhysicalDisplay(display);
                mResources->setDisplayClientTargetCacheSize(display, static_cast<uint32_t>(CacheSize));
            } else {
                mResources->addPhysicalDisplay(display);
            }
        } else if (connected == HWC2::Connection::Disconnected) {
            mResources->removeDisplay(display);
            bCon = false;
        }

        auto ret = mCallback->onHotplug(display, bCon);
        ALOGE_IF(!ret.isOk(), "failed to send onHotplug");
    }

    void onRefresh(int64_t display) {
        auto ret = mCallback->onRefresh(display);
        ALOGE_IF(!ret.isOk(), "failed to send onRefresh");
    }

    void onVsync(int64_t display, int64_t timestamp, uint32_t vsyncPeriodNanos) {
        auto ret = mCallback->onVsync(display, timestamp,
                                      static_cast<int32_t>(vsyncPeriodNanos));
        ALOGE_IF(!ret.isOk(), "failed to send onVsync");
    }

    void onVsyncPeriodTimingChanged(int64_t display,
            const VsyncPeriodChangeTimeline& updatedTimeline) override {
        auto ret = mCallback->onVsyncPeriodTimingChanged(display, updatedTimeline);
        ALOGE_IF(!ret.isOk(), "failed to send onVsyncPeriodTimingChanged");
    }

    void onSeamlessPossible(int64_t display) override {
        auto ret = mCallback->onSeamlessPossible(display);
        ALOGE_IF(!ret.isOk(), "failed to send onSeamlessPossible");
    }

protected:
    void cleanDisplayResources(int64_t display __unused) {
        size_t cacheSize;
        auto err = mResources->getDisplayClientTargetCacheSize(display, &cacheSize);
        if (err == HWC3::Error::None) {
            for (uint32_t slot = 0; slot < cacheSize; slot++) {
                ::android::hardware::graphics::composer::V2_2::hal::ComposerResources::
                    ReplacedHandle replacedBuffer(/*isBuffer*/ true);
                //ComposerResources::ReplacedHandle replacedBuffer(/*isBuffer*/ true);
                // Replace the buffer slots with NULLs. Keep the old handle until it is
                // replaced in ComposerHal, otherwise we risk leaving a dangling pointer.
                const native_handle_t* clientTarget = nullptr;
                err = mResources->getDisplayClientTarget(display, slot, /*useCache*/ true,
                        /*rawHandle*/ nullptr, &clientTarget,
                        &replacedBuffer);
                if (err != HWC3::Error::None) {
                    continue;
                }

                const std::vector<common::Rect> damage;
                err = mHal->setClientTarget(display, clientTarget, /*fence*/ -1, 0, damage);
                ALOGE_IF(err != HWC3::Error::None,
                        "Can't clean slot %d of the client target buffer"
                        "cache for display %" PRIu64,
                        slot, display);
            }
        } else {
            ALOGE("Can't clean client target cache for display %" PRIu64, display);
        }

        err = mResources->getDisplayOutputBufferCacheSize(display, &cacheSize);
        if (err == HWC3::Error::None) {
            for (uint32_t slot = 0; slot < cacheSize; slot++) {
                // Replace the buffer slots with NULLs. Keep the old handle until it is
                // replaced in ComposerHal, otherwise we risk leaving a dangling pointer.
                ::android::hardware::graphics::composer::V2_2::hal::ComposerResources::
                    ReplacedHandle replacedBuffer(/*isBuffer*/ true);
                const native_handle_t* outputBuffer = nullptr;
                err = mResources->getDisplayOutputBuffer(display, slot, /*useCache*/ true,
                        /*rawHandle*/ nullptr, &outputBuffer,
                        &replacedBuffer);
                if (err != HWC3::Error::None) {
                    continue;
                }

                err = mHal->setOutputBuffer(display, outputBuffer, /*fence*/ -1);
                ALOGE_IF(err != HWC3::Error::None,
                        "Can't clean slot %d of the output buffer cache"
                        "for display %" PRIu64,
                        slot, display);
            }
        } else {
            ALOGE("Can't clean output buffer cache for display %" PRIu64, display);
        }
    }

    ComposerHal* const mHal;
    IComposerCallback* const mCallback;
    ComposerResources* const mResources;
    ComposerClient* const mClient;
};

class ComposerClient::CommandResultWriter {
 public:
    CommandResultWriter(std::vector<CommandResultPayload>* results)
        : mIndex(0), mResults(results) {}

    void nextCommand() { ++mIndex; }

    void addError(HWC3::Error error) {
        CommandError commandErrorResult;
        commandErrorResult.commandIndex = mIndex;
        commandErrorResult.errorCode = static_cast<int32_t>(error);
        mResults->emplace_back(std::move(commandErrorResult));
    }

    void addPresentFence(int64_t displayId, ::android::base::unique_fd fence) {
      if (fence >= 0) {
          PresentFence presentFenceResult;
          presentFenceResult.display = displayId;
          presentFenceResult.fence = ndk::ScopedFileDescriptor(fence.release());
          mResults->emplace_back(std::move(presentFenceResult));
      }
    }

    void addReleaseFences(
        int64_t displayId,
        std::unordered_map<int64_t, ::android::base::unique_fd> layerFences) {
        ReleaseFences releaseFencesResult;
        releaseFencesResult.display = displayId;
        for (auto& [layer, layerFence] : layerFences) {
            if (layerFence >= 0) {
                ReleaseFences::Layer releaseFencesLayerResult;
                releaseFencesLayerResult.layer = layer;
                releaseFencesLayerResult.fence =
                    ndk::ScopedFileDescriptor(layerFence.release());
                releaseFencesResult.layers.emplace_back(
                        std::move(releaseFencesLayerResult));
            }
        }
        mResults->emplace_back(std::move(releaseFencesResult));
    }

    void addChanges(const DisplayChanges& changes) {
        if (changes.compositionChanges) {
            mResults->emplace_back(*changes.compositionChanges);
        }
        if (changes.displayRequestChanges) {
            mResults->emplace_back(*changes.displayRequestChanges);
        }
        if (changes.clientTargetProperty) {
            mResults->emplace_back(*changes.clientTargetProperty);
        }
    }

    void addPresentOrValidateResult(int64_t displayId,
                                    PresentOrValidate::Result pov) {
        PresentOrValidate result;
        result.display = displayId;
        result.result = pov;
        mResults->emplace_back(std::move(result));
    }

private:
    int32_t mIndex = 0;
    std::vector<CommandResultPayload>* mResults = nullptr;
};

ComposerClient::ComposerClient() {
    DEBUG_LOG("%s", __FUNCTION__);
}

ComposerClient::ComposerClient(ComposerHal *hal) {
    mHal = hal;
}

ComposerClient::~ComposerClient() {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    ALOGD("destroying composer client");
    mHal->unregisterEventCallback();
    destroyResources();

    if (mOnClientDestroyed) {
        mOnClientDestroyed();
    }
    ALOGD("removed composer client");
}

HWC3::Error ComposerClient::init() {
    DEBUG_LOG("%s", __FUNCTION__);

    HWC3::Error error = HWC3::Error::None;
    std::unique_lock<std::mutex> lock(mStateMutex);

    mResources = std::make_unique<ComposerResources>();
    if (!mResources) {
        ALOGE("%s failed to allocate ComposerResources", __FUNCTION__);
        return HWC3::Error::NoResources;
    }

    error = mResources->init();
    if (error != HWC3::Error::None) {
        ALOGE("%s failed to initialize ComposerResources", __FUNCTION__);
        return error;
    }

    DEBUG_LOG("%s initialized!", __FUNCTION__);
    return HWC3::Error::None;
}

ndk::ScopedAStatus ComposerClient::createLayer(int64_t displayId,
                                               int32_t bufferSlotCount,
                                               int64_t* layerId) {
    DEBUG_LOG("%s display:%" PRIu64, __FUNCTION__, displayId);

    std::unique_lock<std::mutex> lock(mStateMutex);

    auto error = mHal->createLayer(displayId, layerId);
    if (error != HWC3::Error::None) {
        ALOGE("%s: display:%" PRIu64 " failed to create layer", __FUNCTION__,
                displayId);
        return ToBinderStatus(error);
    }

    error = mResources->addLayer(displayId, *layerId,
                                static_cast<uint32_t>(bufferSlotCount));
    if (error != HWC3::Error::None) {
        ALOGE("%s: display:%" PRIu64 " resources failed to create layer",
                __FUNCTION__, displayId);
        // The display entry may have already been removed by onHotplug.
        // Note: We do not destroy the layer on this error as the hotplug
        // disconnect invalidates the display id. The implementation should
        // ensure all layers for the display are destroyed.
         *layerId = 0;
         return ToBinderStatus(error);
    }

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::createVirtualDisplay(
      int32_t width, int32_t height, PixelFormat formatHint,
      int32_t outputBufferSlotCount, VirtualDisplay* display) {
    DEBUG_LOG("%s", __FUNCTION__);
    int64_t displayId;
    auto err = mHal->createVirtualDisplay(static_cast<uint32_t>(width),
                                          static_cast<uint32_t>(height),
                                          &formatHint, &displayId);
    if (err == HWC3::Error::None) {
        mResources->addVirtualDisplay(displayId,
                                      static_cast<uint32_t>(outputBufferSlotCount));
    }

    // todo create VirtualDisplay
    //*display = static_cast<VirtualDisplay *>(displayId);
    display = nullptr;

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::destroyLayer(int64_t displayId,
                                                int64_t layerId) {
    DEBUG_LOG("%s display:%" PRIu64, __FUNCTION__, displayId);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto error = mHal->destroyLayer(displayId, layerId);

    if (error != HWC3::Error::None) {
        ALOGE("%s: display:%" PRIu64 " failed to destroy layer:%" PRIu64,
                __FUNCTION__, displayId, layerId);
        return ToBinderStatus(error);
    }

    error = mResources->removeLayer(displayId, layerId);
    if (error != HWC3::Error::None) {
        ALOGE("%s: display:%" PRIu64 " resources failed to destroy layer:%" PRIu64,
                __FUNCTION__, displayId, layerId);
        return ToBinderStatus(error);
    }

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::destroyVirtualDisplay(int64_t displayId) {
    DEBUG_LOG("%s", __FUNCTION__);
    auto err = mHal->destroyVirtualDisplay(displayId);
    if (err == HWC3::Error::None) {
        mResources->removeDisplay(displayId);
    }

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::executeCommands(
      const std::vector<DisplayCommand>& commands,
      std::vector<CommandResultPayload>* commandResultPayloads) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    mCommandResults =
        std::make_unique<CommandResultWriter>(commandResultPayloads);

    for (const DisplayCommand& command : commands) {
        executeDisplayCommand(command);
        mCommandResults->nextCommand();
    }

    mCommandResults.reset();

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::getActiveConfig(int64_t displayId,
                                                   int32_t* config) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    *config = 0;
    auto err = mHal->getActiveConfig(displayId, config);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getColorModes(
      int64_t displayId, std::vector<ColorMode>* colorModes) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->getColorModes(displayId, colorModes);

    return ToBinderStatus(err);
}

// todo
ndk::ScopedAStatus ComposerClient::getDataspaceSaturationMatrix(
      common::Dataspace dataspace, std::vector<float>* matrix) {
    DEBUG_LOG("%s", __FUNCTION__);

    if (dataspace != common::Dataspace::SRGB_LINEAR) {
        return ToBinderStatus(HWC3::Error::BadParameter);
    }

    // clang-format off
    constexpr std::array<float, 16> kUnit {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    // clang-format on
    matrix->clear();
    matrix->insert(matrix->begin(), kUnit.begin(), kUnit.end());

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::getDisplayAttribute(
      int64_t displayId, int32_t config, DisplayAttribute attribute,
      int32_t* value) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    *value = 0;
    auto err = mHal->getDisplayAttribute(displayId, config,
                                         static_cast<int32_t>(attribute),
                                         value);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getDisplayCapabilities(
      int64_t displayId, std::vector<DisplayCapability>* outCaps) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);

    auto error = mHal->getDisplayCapabilities(displayId, outCaps);

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getDisplayConfigs(
      int64_t displayId, std::vector<int32_t>* outConfigs) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->getDisplayConfigs(displayId, outConfigs);

    return ToBinderStatus(err);
}


ndk::ScopedAStatus ComposerClient::getDisplayConnectionType(
      int64_t displayId, DisplayConnectionType* outType) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto error = mHal->getDisplayConnectionType(displayId, outType);

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getDisplayIdentificationData(
      int64_t displayId, DisplayIdentification* outIdentification) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    uint8_t port = 0;
    std::vector<uint8_t> data;
    auto error = mHal->getDisplayIdentificationData(displayId, &port, &data);
    outIdentification->port = static_cast<int8_t>(port);
    outIdentification->data = data;

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getDisplayName(int64_t displayId,
                                                  std::string* outName) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->getDisplayName(displayId, outName);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getDisplayVsyncPeriod(
      int64_t displayId, int32_t* outVsyncPeriod) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto error = mHal->getDisplayVsyncPeriod(displayId, outVsyncPeriod);

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getDisplayedContentSample(
      int64_t displayId __unused, int64_t maxFrames __unused, int64_t timestamp __unused,
      DisplayContentSample* outSamples __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::getDisplayedContentSamplingAttributes(
      int64_t displayId __unused, DisplayContentSamplingAttributes* outAttributes __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::getDisplayPhysicalOrientation(
      int64_t displayId, common::Transform* outOrientation) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto err = mHal->getDisplayPhysicalOrientation(displayId, outOrientation);
    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getHdrCapabilities(
      int64_t displayId, HdrCapabilities* outCapabilities) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    std::vector<common::Hdr> types;
    float max_lumi = 0.0f;
    float max_avg_lumi = 0.0f;
    float min_lumi = 0.0f;
    auto err = mHal->getHdrCapabilities(displayId, &types, &max_lumi, &max_avg_lumi, &min_lumi);

    outCapabilities->maxLuminance = max_lumi;
    outCapabilities->maxAverageLuminance = max_avg_lumi;
    outCapabilities->minLuminance = min_lumi;
    outCapabilities->types = types;

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getMaxVirtualDisplayCount(
      int32_t* outCount) {
    DEBUG_LOG("%s", __FUNCTION__);
    *outCount = (int32_t) mHal->getMaxVirtualDisplayCount();

    return ToBinderStatus(HWC3::Error::None);
}

ndk::ScopedAStatus ComposerClient::getPerFrameMetadataKeys(
      int64_t displayId, std::vector<PerFrameMetadataKey>* outKeys) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto error = mHal->getPerFrameMetadataKeys(displayId, outKeys);

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getReadbackBufferAttributes(
      int64_t displayId __unused, ReadbackBufferAttributes* outAttributes __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::getReadbackBufferFence(
      int64_t displayId __unused, ndk::ScopedFileDescriptor* outAcquireFence __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::getRenderIntents(
      int64_t displayId, ColorMode mode, std::vector<RenderIntent>* outIntents) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto err = mHal->getRenderIntents(displayId, mode, outIntents);
    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getSupportedContentTypes(
      int64_t displayId __unused, std::vector<ContentType>* outTypes __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto error = mHal->getSupportedContentTypes(displayId, outTypes);

    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::getDisplayDecorationSupport(int64_t displayId __unused,
            std::optional<common::DisplayDecorationSupport>* support __unused) {

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::registerCallback(
      const std::shared_ptr<IComposerCallback>& callback) {
    DEBUG_LOG("%s", __FUNCTION__);

    mCallbacks = callback;
    mHalEventCallback = std::make_unique<HalEventCallback>(mHal, callback.get(),
            mResources.get(), this);
    mHal->registerEventCallback(mHalEventCallback.get());

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ComposerClient::setActiveConfig(int64_t displayId,
                                                     int32_t configId) {
    DEBUG_LOG("%s display:%" PRIu64 " config:%" PRIu32, __FUNCTION__, displayId,
              configId);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setActiveConfig(displayId, configId);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setActiveConfigWithConstraints(
      int64_t displayId, int32_t configId,
      const VsyncPeriodChangeConstraints& constraints,
      VsyncPeriodChangeTimeline* outTimeline) {
    auto error = mHal->setActiveConfigWithConstraints(displayId, configId,
            constraints, outTimeline);
    return ToBinderStatus(error);
}

ndk::ScopedAStatus ComposerClient::setBootDisplayConfig(int64_t displayId,
                                                        int32_t configId) {
    DEBUG_LOG("%s display:%" PRIu64 " config:%" PRIu32, __FUNCTION__, displayId,
              configId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto err = mHal->setBootDisplayConfig(displayId, configId);
    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::clearBootDisplayConfig(int64_t displayId) {
    DEBUG_LOG("%s display:%" PRIu64, __FUNCTION__, displayId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto err = mHal->clearBootDisplayConfig(displayId);
    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getPreferredBootDisplayConfig(
      int64_t displayId, int32_t* outConfigId) {
    DEBUG_LOG("%s display:%" PRIu64, __FUNCTION__, displayId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto err = mHal->getPreferredBootDisplayConfig(displayId, outConfigId);
    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setAutoLowLatencyMode(int64_t displayId,
                                                         bool on) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setAutoLowLatencyMode(displayId, on);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setClientTargetSlotCount(int64_t displayId,
                                                            int32_t count) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mResources->setDisplayClientTargetCacheSize(displayId,
            static_cast<uint32_t>(count));

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setColorMode(int64_t displayId,
                                                ColorMode mode,
                                                RenderIntent intent) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setColorMode(displayId, mode, intent);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setContentType(int64_t displayId,
                                                  ContentType type) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setContentType(displayId, type);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setDisplayedContentSamplingEnabled(
      int64_t displayId __unused, bool enable __unused, FormatColorComponent componentMask __unused,
      int64_t maxFrames __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::setPowerMode(int64_t displayId,
                                                  PowerMode mode) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setPowerMode(displayId, mode);

    return ToBinderStatus(err);
}
ndk::ScopedAStatus ComposerClient::setReadbackBuffer(
      int64_t displayId,
      const aidl::android::hardware::common::NativeHandle& buffer,
      const ndk::ScopedFileDescriptor& releaseFence __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);

    // Owned by mResources.
    buffer_handle_t importedBuffer = nullptr;

    auto releaser = mResources->createReleaser(true /* isBuffer */);
    auto error = mResources->getDisplayReadbackBuffer(
        displayId, buffer, &importedBuffer, releaser.get());
    if (error != HWC3::Error::None) {
        ALOGE("%s: failed to get readback buffer from resources.", __FUNCTION__);
        return ToBinderStatus(error);
    }

    return ToBinderStatus(HWC3::Error::Unsupported);
}

ndk::ScopedAStatus ComposerClient::setVsyncEnabled(int64_t displayId,
                                                   bool enabled) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setVsyncEnabled(displayId, enabled);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::setIdleTimerEnabled(int64_t displayId __unused,
                                                       int32_t timeoutMs __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    return ToBinderStatus(HWC3::Error::Unsupported);
}

/*hwc 3.2 */
#if (PLATFORM_SDK_VERSION >= 34)
ndk::ScopedAStatus ComposerClient::getHdrConversionCapabilities(
        std::vector<common::HdrConversionCapability>* outHdrConversionCapability) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->getHdrConversionCapabilities(outHdrConversionCapability);

    return ToBinderStatus(err);
}


ndk::ScopedAStatus ComposerClient::setHdrConversionStrategy(
        const common::HdrConversionStrategy& conversionStrategy,
        common::Hdr* preferredHdrOutputType) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->setHdrConversionStrategy(conversionStrategy, preferredHdrOutputType);

    return ToBinderStatus(err);
}

ndk::ScopedAStatus ComposerClient::getOverlaySupport(
        OverlayProperties* properties ) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);
    auto err = mHal->getOverlaySupport(properties);

    return ToBinderStatus(err);

}

ndk::ScopedAStatus ComposerClient::setRefreshRateChangedCallbackDebugEnabled(
        int64_t displayId __unused, bool) {
    DEBUG_LOG("%s", __FUNCTION__);
    return ToBinderStatus(HWC3::Error::Unsupported);
}
#endif

ndk::SpAIBinder ComposerClient::createBinder() {
    auto binder = BnComposerClient::createBinder();
    AIBinder_setInheritRt(binder.get(), true);
    return binder;
}

namespace {

#define DISPATCH_LAYER_COMMAND(layerCmd, display, layer, field, funcName)         \
    do {                                                                          \
      if (layerCmd.field) {                                                       \
          ComposerClient::executeLayerCommandSetLayer##funcName(display, layer,   \
                                                                *layerCmd.field); \
      }                                                                           \
    } while (0)

#define DISPATCH_DISPLAY_COMMAND(displayCmd, display, field, funcName)   \
    do {                                                                 \
      if (displayCmd.field) {                                            \
          executeDisplayCommand##funcName(display, *displayCmd.field);   \
      }                                                                  \
    } while (0)

#define DISPATCH_DISPLAY_BOOL_COMMAND(displayCmd, display, field, funcName) \
    do {                                                                    \
      if (displayCmd.field) {                                               \
          executeDisplayCommand##funcName(display);                         \
      }                                                                     \
    } while (0)

#define DISPATCH_DISPLAY_BOOL_COMMAND_AND_DATA(displayCmd, display, field, \
                                                 data, funcName)           \
    do {                                                                   \
      if (displayCmd.field) {                                              \
          executeDisplayCommand##funcName(display, displayCmd.data);       \
      }                                                                    \
    } while (0)

#define LOG_DISPLAY_COMMAND_ERROR(display, error)                     \
    do {                                                              \
        const std::string errorString = toString(error);              \
        ALOGE("%s: display:%" PRId64 " failed with:%s", __FUNCTION__, \
                display, errorString.c_str());                        \
    } while (0)

#define LOG_LAYER_COMMAND_ERROR(display, layer, error)                    \
    do {                                                                  \
        const std::string errorString = toString(error);                  \
        ALOGE("%s: display:%" PRId64 " layer:%" PRId64 " failed with:%s", \
                __FUNCTION__, display, layer,                             \
                errorString.c_str());                                     \
    } while (0)

}  // namespace

void ComposerClient::executeDisplayCommand(
      const DisplayCommand& displayCommand) {
    int64_t displayId = displayCommand.display;

    for (const LayerCommand& layerCmd : displayCommand.layers) {
        executeLayerCommand(displayId, layerCmd);
    }

    DISPATCH_DISPLAY_COMMAND(displayCommand, displayId, colorTransformMatrix,
                             SetColorTransform);
    DISPATCH_DISPLAY_COMMAND(displayCommand, displayId, brightness, SetBrightness);
    DISPATCH_DISPLAY_COMMAND(displayCommand, displayId, clientTarget,
                             SetClientTarget);
    DISPATCH_DISPLAY_COMMAND(displayCommand, displayId, virtualDisplayOutputBuffer,
                             SetOutputBuffer);
    DISPATCH_DISPLAY_BOOL_COMMAND_AND_DATA(displayCommand, displayId,
                                           validateDisplay, expectedPresentTime,
                                           ValidateDisplay);
    DISPATCH_DISPLAY_BOOL_COMMAND(displayCommand, displayId, acceptDisplayChanges,
                                  AcceptDisplayChanges);
    DISPATCH_DISPLAY_BOOL_COMMAND(displayCommand, displayId, presentDisplay,
                                  PresentDisplay);
    DISPATCH_DISPLAY_BOOL_COMMAND_AND_DATA(
        displayCommand, displayId, presentOrValidateDisplay, expectedPresentTime,
        PresentOrValidateDisplay);
}

void ComposerClient::executeLayerCommand(int64_t displayId,
                                         const LayerCommand& layerCommand) {
    int64_t layerId = layerCommand.layer;

    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, cursorPosition,
                           CursorPosition);
#if (PLATFORM_SDK_VERSION >= 34)
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, bufferSlotsToClear,
                           BufferSlotsToClear);
#endif
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, buffer, Buffer);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, damage, SurfaceDamage);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, blendMode, BlendMode);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, color, Color);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, composition,
                           Composition);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, dataspace, Dataspace);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, displayFrame,
                           DisplayFrame);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, planeAlpha, PlaneAlpha);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, sidebandStream,
                           SidebandStream);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, sourceCrop, SourceCrop);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, transform, Transform);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, visibleRegion,
                           VisibleRegion);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, z, ZOrder);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, colorTransform,
                           ColorTransform);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, perFrameMetadata,
                           PerFrameMetadata);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, perFrameMetadataBlob,
                           PerFrameMetadataBlobs);
    DISPATCH_LAYER_COMMAND(layerCommand, displayId, layerId, brightness,
                           Brightness);
}

void ComposerClient::executeDisplayCommandSetColorTransform(
      int64_t displayId, const std::vector<float>& matrix) {
    DEBUG_LOG("%s", __FUNCTION__);

    float matrixHal[16];
    std::copy(matrix.begin(), matrix.end(), matrixHal);

    const bool isIdentity = (::android::mat4(matrix.data()) == ::android::mat4());

    auto error = mHal->setColorTransform(displayId, matrixHal,
                                         isIdentity ? HAL_COLOR_TRANSFORM_IDENTITY
                                                    : HAL_COLOR_TRANSFORM_ARBITRARY_MATRIX);

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeDisplayCommandSetBrightness(
      int64_t display __unused, const DisplayBrightness& brightness __unused) {
    DEBUG_LOG("%s", __FUNCTION__);
    mCommandResults->addError(HWC3::Error::Unsupported);
}

void ComposerClient::executeDisplayCommandSetClientTarget(
      int64_t displayId, const ClientTarget& clientTarget) {
    DEBUG_LOG("%s", __FUNCTION__);

    // Owned by mResources.
    buffer_handle_t importedBuffer = nullptr;

    auto releaser = mResources->createReleaser(/*isBuffer=*/true);
    auto error = mResources->getDisplayClientTarget(
        displayId, clientTarget.buffer, &importedBuffer, releaser.get());
    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
        return;
    }

    ::android::base::unique_fd fenceFd = getUniqueFd(clientTarget.buffer.fence);
    error = mHal->setClientTarget(displayId, importedBuffer,
            fenceFd.release(),
            static_cast<int32_t>(clientTarget.dataspace),
            clientTarget.damage);

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
        return;
    }
}

void ComposerClient::executeDisplayCommandSetOutputBuffer(
      int64_t displayId, const Buffer& buffer) {
    DEBUG_LOG("%s", __FUNCTION__);

    // Owned by mResources.
    buffer_handle_t importedBuffer = nullptr;

    auto releaser = mResources->createReleaser(/*isBuffer=*/true);
    auto error = mResources->getDisplayOutputBuffer(
        displayId, buffer, &importedBuffer, releaser.get());
    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
        return;
    }

    error = mHal->setOutputBuffer(displayId,
            importedBuffer, buffer.fence.get());

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
        return;
    }
}

void ComposerClient::executeDisplayCommandValidateDisplay(
      int64_t displayId,
      const std::optional<ClockMonotonicTimestamp> expectedPresentTime __unused) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::vector<int64_t> changedLayers;
    std::vector<Composition> compositionTypes;
    uint32_t displayRequestMask = 0x0;
    std::vector<int64_t> requestedLayers;
    std::vector<uint32_t> requestMasks;
    ClientTargetProperty clientTargetProperty{common::PixelFormat::RGBA_8888,
        common::Dataspace::UNKNOWN};

    if (expectedPresentTime.has_value()) {
        mHal->setExpectedPresentTime(displayId, expectedPresentTime->timestampNanos);
    }

    auto error = mHal->validateDisplay(displayId, &changedLayers, &compositionTypes,
            &displayRequestMask, &requestedLayers, &requestMasks,
            &clientTargetProperty);

    mResources->setDisplayMustValidateState(displayId, false);

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
    } else {
        DisplayChanges changes;
        for (uint32_t i= 0; i < changedLayers.size(); i++) {
            changes.addLayerCompositionChange(displayId, changedLayers[i],
                    compositionTypes[i]);
        }
        changes.addDisplayAndLayerRequests(displayId, displayRequestMask,
                requestedLayers, requestMasks);
        changes.addClientTargetProperty(clientTargetProperty);

        mCommandResults->addChanges(changes);
    }
}

void ComposerClient::executeDisplayCommandAcceptDisplayChanges(
      int64_t displayId) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->acceptDisplayChanges(displayId);
    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeDisplayCommandPresentOrValidateDisplay(
      int64_t displayId,
      const std::optional<ClockMonotonicTimestamp> expectedPresentTime __unused) {
    DEBUG_LOG("%s", __FUNCTION__);
    if (expectedPresentTime.has_value()) {
        mHal->setExpectedPresentTime(displayId, expectedPresentTime->timestampNanos);
    }

    // First try to Present as is.
    int presentFence = -1;
    std::vector<int64_t> layers;
    std::vector<int> fences;
    auto err = mResources->mustValidateDisplay(displayId)
        ? HWC3::Error::NotValidated
        : mHal->presentDisplay(displayId, &presentFence, &layers, &fences);
    if (err == HWC3::Error::None) {
        ::android::base::unique_fd displayFence(presentFence);
        std::unordered_map<int64_t, ::android::base::unique_fd> layerFences;

        for (uint32_t i = 0; i < layers.size(); i++) {
            layerFences[layers[i]] = ::android::base::unique_fd(fences[i]);
        }

        mCommandResults->addPresentOrValidateResult(
                displayId, PresentOrValidate::Result::Presented);
        mCommandResults->addPresentFence(displayId, std::move(displayFence));
        mCommandResults->addReleaseFences(displayId, std::move(layerFences));
        return ;
    }

    // Present has failed. We need to fallback to validate
    std::vector<int64_t> changedLayers;
    std::vector<Composition> compositionTypes;
    uint32_t displayRequestMask = 0x0;
    std::vector<int64_t> requestedLayers;
    std::vector<uint32_t> requestMasks;
    ClientTargetProperty clientTargetProperty{common::PixelFormat::RGBA_8888,
        common::Dataspace::UNKNOWN};

    auto error = mHal->validateDisplay(displayId, &changedLayers, &compositionTypes,
            &displayRequestMask, &requestedLayers, &requestMasks,
            &clientTargetProperty);

    mResources->setDisplayMustValidateState(displayId, false);

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
    } else {
        DisplayChanges changes;
        for (uint32_t i= 0; i < changedLayers.size(); i++) {
            changes.addLayerCompositionChange(displayId, changedLayers[i],
                    compositionTypes[i]);
        }
        changes.addDisplayAndLayerRequests(displayId, displayRequestMask,
                requestedLayers, requestMasks);
        changes.addClientTargetProperty(clientTargetProperty);

        mCommandResults->addChanges(changes);
        mCommandResults->addPresentOrValidateResult(
                displayId, PresentOrValidate::Result::Validated);
    }
}

void ComposerClient::executeDisplayCommandPresentDisplay(int64_t displayId) {
    DEBUG_LOG("%s", __FUNCTION__);

    if (mResources->mustValidateDisplay(displayId)) {
        ALOGE("%s: display:%" PRIu64 " not validated", __FUNCTION__,
                displayId);
        mCommandResults->addError(HWC3::Error::NotValidated);
        return;
    }

    int presentFence = -1;
    std::vector<int64_t> layers;
    std::vector<int> fences;
    auto error = mHal->presentDisplay(displayId, &presentFence, &layers, &fences);

    ::android::base::unique_fd displayFence(presentFence);
    std::unordered_map<int64_t, ::android::base::unique_fd> layerFences;

    for (uint32_t i = 0; i < layers.size(); i++) {
        layerFences[layers[i]] = ::android::base::unique_fd(fences[i]);
    }

    if (error != HWC3::Error::None) {
        LOG_DISPLAY_COMMAND_ERROR(displayId, error);
        mCommandResults->addError(error);
    } else {
        mCommandResults->addPresentFence(displayId, std::move(displayFence));
        mCommandResults->addReleaseFences(displayId, std::move(layerFences));
    }
}

void ComposerClient::executeLayerCommandSetLayerCursorPosition(
      int64_t displayId, int64_t layerId, const common::Point& cursorPosition) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerCursorPosition(displayId, layerId,
                    cursorPosition.x, cursorPosition.y);

    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerBuffer(int64_t displayId,
        int64_t layerId, const Buffer& buffer) {
    DEBUG_LOG("%s", __FUNCTION__);

    // Owned by mResources.
    buffer_handle_t importedBuffer = nullptr;

    auto releaser = mResources->createReleaser(/*isBuffer=*/true);
    auto error =
        mResources->getLayerBuffer(displayId, layerId, buffer,
                                   &importedBuffer, releaser.get());
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
        return;
    }

    ::android::base::unique_fd fenceFd = getUniqueFd(buffer.fence);
    auto err = mHal->setLayerBuffer(displayId, layerId, importedBuffer, fenceFd.release());
    if (err != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

#if (PLATFORM_SDK_VERSION >= 34)
void ComposerClient::executeLayerCommandSetLayerBufferSlotsToClear(int64_t displayId,
        int64_t layerId, const std::vector<int32_t>& bufferSlotsToClear) {
    DEBUG_LOG("%s", __FUNCTION__);

    buffer_handle_t importedBuffer = nullptr;

    auto releaser = mResources->createReleaser(/*isBuffer=*/true);
    for (int32_t slot : bufferSlotsToClear) {
        auto error =
            mResources->setLayerBufferSlotsToClear(displayId, layerId, slot,
                                   &importedBuffer, releaser.get());
        if (error != HWC3::Error::None) {
            LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
            mCommandResults->addError(error);
            return;
        }
    }
}
#endif

void ComposerClient::executeLayerCommandSetLayerSurfaceDamage(
      int64_t displayId, int64_t layerId,
      const std::vector<std::optional<common::Rect>>& damage) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerSurfaceDamage(displayId, layerId, damage);

    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerBlendMode(
      int64_t displayId, int64_t layerId, const ParcelableBlendMode& blendMode) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerBlendMode(displayId, layerId,
            static_cast<int32_t>(blendMode.blendMode));
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerColor(int64_t displayId,
                                                      int64_t layerId,
                                                      const Color& color) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerColor(displayId, layerId, color);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerComposition(
      int64_t displayId, int64_t layerId, const ParcelableComposition& composition) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerCompositionType(displayId, layerId,
            static_cast<int32_t> (composition.composition));
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerDataspace(
      int64_t displayId, int64_t layerId, const ParcelableDataspace& dataspace) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerDataspace(displayId, layerId,
            static_cast<int32_t>(dataspace.dataspace));
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerDisplayFrame(
      int64_t displayId, int64_t layerId, const common::Rect& rect) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerDisplayFrame(displayId, layerId, rect);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerPlaneAlpha(
      int64_t displayId, int64_t layerId, const PlaneAlpha& planeAlpha) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerPlaneAlpha(displayId, layerId, planeAlpha.alpha);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerSidebandStream(
      int64_t displayId, int64_t layerId,
      const aidl::android::hardware::common::NativeHandle& handle) {
    DEBUG_LOG("%s", __FUNCTION__);

    // Owned by mResources.
    buffer_handle_t importedStream = nullptr;

    auto releaser = mResources->createReleaser(/*isBuffer=*/false);
    auto error = mResources->getLayerSidebandStream(
        displayId, layerId, handle, &importedStream,
        releaser.get());
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
        return;
    }

    error = mHal->setLayerSidebandStream(displayId, layerId, importedStream);
    if (error != HWC3::Error::None) {
      LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
      mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerSourceCrop(
      int64_t displayId, int64_t layerId, const common::FRect& sourceCrop) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerSourceCrop(displayId, layerId, sourceCrop);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerTransform(
      int64_t displayId, int64_t layerId, const ParcelableTransform& transform) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerTransform(displayId, layerId,
            static_cast<int32_t>(transform.transform));
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerVisibleRegion(
      int64_t displayId, int64_t layerId,
      const std::vector<std::optional<common::Rect>>& visibleRegion) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerVisibleRegion(displayId, layerId, visibleRegion);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerZOrder(int64_t displayId,
        int64_t layerId, const ZOrder& zOrder) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerZOrder(displayId, layerId, (uint32_t)zOrder.z);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, error);
        mCommandResults->addError(error);
    }
}

void ComposerClient::executeLayerCommandSetLayerPerFrameMetadata(
      int64_t displayId, int64_t layerId,
      const std::vector<std::optional<PerFrameMetadata>>& perFrameMetadata) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto err = mHal->setLayerPerFrameMetadata(displayId, layerId, perFrameMetadata);
    if (err != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(displayId, layerId, err);
        mCommandResults->addError(err);
    }
}

void ComposerClient::executeLayerCommandSetLayerColorTransform(
      int64_t display __unused, int64_t layerId __unused,
      const std::vector<float>& colorTransform __unused) {
    DEBUG_LOG("%s", __FUNCTION__);
    mCommandResults->addError(HWC3::Error::Unsupported);
}

void ComposerClient::executeLayerCommandSetLayerPerFrameMetadataBlobs(
      int64_t display __unused, int64_t layerId __unused,
      const std::vector<std::optional<PerFrameMetadataBlob>>&
          perFrameMetadataBlob __unused) {
    DEBUG_LOG("%s", __FUNCTION__);
    mCommandResults->addError(HWC3::Error::Unsupported);
}

void ComposerClient::executeLayerCommandSetLayerBrightness(
      int64_t display, int64_t layer,
      const LayerBrightness& brightness) {
    DEBUG_LOG("%s", __FUNCTION__);

    auto error = mHal->setLayerBrightness(display, layer, brightness);
    if (error != HWC3::Error::None) {
        LOG_LAYER_COMMAND_ERROR(display, layer, error);
        mCommandResults->addError(error);
    }
}

::android::base::unique_fd ComposerClient::getUniqueFd(
        const ndk::ScopedFileDescriptor& in) {
    auto& sfd = const_cast<ndk::ScopedFileDescriptor&>(in);
    ::android::base::unique_fd ret(sfd.get());
    *sfd.getR() = -1;
    return ret;
}

void ComposerClient::destroyResources() {
    // We want to call hwc2_close here (and move hwc2_open to the
    // constructor), with the assumption that hwc2_close would
    //
    //  - clean up all resources owned by the client
    //  - make sure all displays are blank (since there is no layer)
    //
    // But since SF used to crash at this point, different hwcomposer2
    // implementations behave differently on hwc2_close.  Our only portable
    // choice really is to abort().  But that is not an option anymore
    // because we might also have VTS or VR as clients that can come and go.
    //
    // Below we manually clean all resources (layers and virtual
    // displays), and perform a presentDisplay afterwards.

    mResources->clear([this](uint64_t display, bool isVirtual, const std::vector<uint64_t> layers) {
        ALOGW("destroying client resources for display %" PRIu64, display);
        int64_t displayId = static_cast<int64_t> (display);

        for (auto layer : layers) {
            mHal->destroyLayer(displayId, static_cast<int64_t>(layer));
        }

        if (isVirtual) {
            mHal->destroyVirtualDisplay(displayId);
        } else {
            ALOGW("performing a final presentDisplay");

            std::vector<int64_t> changedLayers;
            std::vector<Composition> compositionTypes;
            uint32_t displayRequestMask = 0;
            std::vector<int64_t> requestedLayers;
            std::vector<uint32_t> requestMasks;
            ClientTargetProperty clientTargetProperty{common::PixelFormat::RGBA_8888,
                common::Dataspace::UNKNOWN};
            mHal->validateDisplay(displayId, &changedLayers, &compositionTypes,
                                  &displayRequestMask, &requestedLayers, &requestMasks,
                                  &clientTargetProperty);

            mHal->acceptDisplayChanges(displayId);

            int32_t presentFence = -1;
            std::vector<int64_t> releasedLayers;
            std::vector<int32_t> releaseFences;
            mHal->presentDisplay(displayId, &presentFence, &releasedLayers, &releaseFences);
            if (presentFence >= 0) {
                close(presentFence);
            }
            for (auto fence : releaseFences) {
                if (fence >= 0) {
                    close(fence);
                }
            }
        }
    });

    mResources.reset();
}

}
}  // namespace aidl::android::hardware::graphics::composer3::impl
