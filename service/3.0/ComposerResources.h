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

// Thin wrappers around V2_2::hal::ComposerResources related classes that
// return HWC3 error codes and accept HWC3 argument types.

#pragma once

#include "Common.h"
#include <composer-resources/2.2/ComposerResources.h>

#include <memory>
#include <optional>

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

HWC3::Error toHwc3Error(::android::hardware::graphics::composer::V2_1::Error error) {
    switch (error) {
    case ::android::hardware::graphics::composer::V2_1::Error::NONE:
        return HWC3::Error::None;
    case ::android::hardware::graphics::composer::V2_1::Error::BAD_CONFIG:
        return HWC3::Error::BadConfig;
    case ::android::hardware::graphics::composer::V2_1::Error::BAD_DISPLAY:
        return HWC3::Error::BadDisplay;
    case ::android::hardware::graphics::composer::V2_1::Error::BAD_LAYER:
        return HWC3::Error::BadLayer;
    case ::android::hardware::graphics::composer::V2_1::Error::BAD_PARAMETER:
        return HWC3::Error::BadParameter;
    case ::android::hardware::graphics::composer::V2_1::Error::NO_RESOURCES:
        return HWC3::Error::NoResources;
    case ::android::hardware::graphics::composer::V2_1::Error::NOT_VALIDATED:
        return HWC3::Error::NotValidated;
    case ::android::hardware::graphics::composer::V2_1::Error::UNSUPPORTED:
        return HWC3::Error::Unsupported;
    }
}

::android::hardware::graphics::composer::V2_1::Display toHwc2Display(int64_t displayId) {
    return static_cast<::android::hardware::graphics::composer::V2_1::Display>(displayId);
}

::android::hardware::graphics::composer::V2_1::Layer toHwc2Layer(int64_t layerId) {
    return static_cast<::android::hardware::graphics::composer::V2_1::Layer>(layerId);
}

class ComposerResourceReleaser {
public:
    ComposerResourceReleaser(bool isBuffer) : mReplacedHandle(isBuffer) {}
    virtual ~ComposerResourceReleaser() = default;

    ::android::hardware::graphics::composer::V2_2::hal::ComposerResources::
        ReplacedHandle* getReplacedHandle() {
            return &mReplacedHandle;
    }

private:
    ::android::hardware::graphics::composer::V2_2::hal::ComposerResources::
        ReplacedHandle mReplacedHandle;
};

class ComposerResources {
public:
    ComposerResources() = default;

    HWC3::Error init();
    std::unique_ptr<ComposerResourceReleaser> createReleaser(bool isBuffer);

    void clear(::android::hardware::graphics::composer::V2_2::hal::
            ComposerResources::RemoveDisplay removeDisplay);
    bool hasDisplay(int64_t display);
    HWC3::Error addPhysicalDisplay(int64_t display);
    HWC3::Error addVirtualDisplay(int64_t displayId,
            uint32_t outputBufferCacheSize);
    HWC3::Error removeDisplay(int64_t display);
    HWC3::Error setDisplayClientTargetCacheSize(int64_t displayId,
            uint32_t clientTargetCacheSize);
    HWC3::Error getDisplayClientTargetCacheSize(int64_t displayId,
            size_t* outCacheSize);
    HWC3::Error getDisplayOutputBufferCacheSize(int64_t displayId,
            size_t* outCacheSize);

    HWC3::Error addLayer(int64_t displayId, int64_t layerId,
            uint32_t bufferCacheSize);
    HWC3::Error removeLayer(int64_t displayId, int64_t layer);

    void setDisplayMustValidateState(int64_t displayId, bool mustValidate);
    bool mustValidateDisplay(int64_t displayId);
    HWC3::Error getDisplayReadbackBuffer(
            int64_t displayId,
            const aidl::android::hardware::common::NativeHandle& handle,
            buffer_handle_t* outHandle, ComposerResourceReleaser* bufReleaser);
    HWC3::Error getDisplayClientTarget(int64_t displayId, const Buffer& buffer,
            buffer_handle_t* outHandle, ComposerResourceReleaser* bufReleaser);
    HWC3::Error getDisplayOutputBuffer(int64_t displayId, const Buffer& buffer,
            buffer_handle_t* outHandle,
            ComposerResourceReleaser* bufReleaser);
    HWC3::Error getLayerBuffer(int64_t displayId, int64_t layerId,
            const Buffer& buffer,
            buffer_handle_t* outBufferHandle,
            ComposerResourceReleaser* bufReleaser);
    HWC3::Error getLayerSidebandStream(
            int64_t displayId, int64_t layerId,
            const aidl::android::hardware::common::NativeHandle& rawHandle,
            buffer_handle_t* outStreamHandle, ComposerResourceReleaser* bufReleaser);

private:
    std::unique_ptr<::android::hardware::graphics::composer::V2_2::hal::ComposerResources> mImpl;
};

}
}  // namespace aidl::android::hardware::graphics::composer3::impl
