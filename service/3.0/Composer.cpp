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


#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <log/log.h>

#include "Composer.h"
#include "Common.h"

namespace aidl::android::hardware::graphics::composer3::impl {
namespace meson {

// do some init
Composer::Composer() {
    mHal = std::make_unique<HwcHal>();
    mHal->init();
}

Composer::~Composer() {

}

ndk::ScopedAStatus Composer::createClient(
        std::shared_ptr<IComposerClient>* outClient) {
    DEBUG_LOG("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mClientMutex);

    const bool previousClientDestroyed = waitForClientDestroyedLocked(lock);
    if (!previousClientDestroyed) {
        ALOGE("%s: failed as composer client already exists", __FUNCTION__);
        *outClient = nullptr;
        return ToBinderStatus(HWC3::Error::NoResources);
    }

    //TODO
    *outClient = nullptr;

    return ndk::ScopedAStatus::ok();
}

bool Composer::waitForClientDestroyedLocked(
        std::unique_lock<std::mutex>& lock __unused) {

    //TODO
    return true;
}

void Composer::onClientDestroyed() {
    std::lock_guard<std::mutex> lock(mClientMutex);

    mClientDestroyedCondition.notify_all();
}

binder_status_t Composer::dump(int fd, const char** /*args*/,
        uint32_t /*numArgs*/) {
    DEBUG_LOG("%s", __FUNCTION__);
    std::string output = mHal->dump();
    write(fd, output.c_str(), output.size());
    return STATUS_OK;
}

ndk::ScopedAStatus Composer::getCapabilities(std::vector<Capability>* caps) {
    DEBUG_LOG("%s", __FUNCTION__);

    const std::array<Capability, 5> all_caps = {{
        Capability::SIDEBAND_STREAM,
        Capability::SKIP_CLIENT_COLOR_TRANSFORM,
        Capability::PRESENT_FENCE_IS_NOT_RELIABLE,
        Capability::SKIP_VALIDATE,
        Capability::BOOT_DISPLAY_CONFIG,
    }};

    caps->clear();
    for (auto cap : all_caps) {
        if (mHal->hasCapability(static_cast<hwc2_capability_t>(cap))) {
            caps->emplace_back(cap);
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::SpAIBinder Composer::createBinder() {
    DEBUG_LOG("%s", __FUNCTION__);

    auto binder = BnComposer::createBinder();
    AIBinder_setInheritRt(binder.get(), true);
    return binder;
}

}
}  // namespace aidl::android::hardware::graphics::composer3::impl
