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


#include <aidl/android/hardware/graphics/composer3/ChangedCompositionLayer.h>
#include <aidl/android/hardware/graphics/composer3/ChangedCompositionTypes.h>
#include <aidl/android/hardware/graphics/composer3/DisplayRequest.h>

#include <optional>

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

struct DisplayChanges {
    std::optional<ChangedCompositionTypes> compositionChanges;
    std::optional<DisplayRequest> displayRequestChanges;
    std::optional<ClientTargetPropertyWithBrightness> clientTargetProperty;

    void addLayerCompositionChange(int64_t displayId, int64_t layerId,
                                   Composition layerComposition) {
        if (!compositionChanges) {
            compositionChanges.emplace();
            compositionChanges->display = displayId;
        }

        ChangedCompositionLayer compositionChange;
        compositionChange.layer = layerId;
        compositionChange.composition = layerComposition;
        compositionChanges->layers.emplace_back(std::move(compositionChange));
    }

    void addDisplayAndLayerRequests(int64_t displayId,
                                    uint32_t displayRequestMask,
                                    std::vector<int64_t> &requestedLayers,
                                    std::vector<uint32_t> &requestedMasks) {
        if (!displayRequestChanges) {
            displayRequestChanges.emplace();
        }

        displayRequestChanges->display = displayId;
        displayRequestChanges->mask = static_cast<int32_t>(displayRequestMask);

        for (size_t i= 0; i < requestedLayers.size(); i++) {
            DisplayRequest::LayerRequest request;
            request.layer = requestedLayers[i];
            request.mask = static_cast<int32_t>(requestedMasks[i]);
            displayRequestChanges->layerRequests.emplace_back(std::move(request));
        }
    }

    void addClientTargetProperty(ClientTargetProperty property) {
        if (!clientTargetProperty) {
            clientTargetProperty.emplace();
        }
        clientTargetProperty->clientTargetProperty = property;
    }

    void clearLayerCompositionChanges() { compositionChanges.reset(); }

    bool hasAnyChanges() const {
        return compositionChanges.has_value() || displayRequestChanges.has_value();
    }

    void reset() {
        compositionChanges.reset();
        displayRequestChanges.reset();
    }
};

}
}  // namespace aidl::android::hardware::graphics::composer3::impl

