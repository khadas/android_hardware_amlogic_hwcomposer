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

    auto client = ndk::SharedRefBase::make<ComposerClient>(mHal.get());
    if (!client) {
        ALOGE("%s: failed to init composer client", __FUNCTION__);
        *outClient = nullptr;
        return ToBinderStatus(HWC3::Error::NoResources);
    }

    auto error = client->init();
    if (error != HWC3::Error::None) {
        *outClient = nullptr;
        return ToBinderStatus(error);
    }

    auto clientDestroyed = [this]() { onClientDestroyed(); };
    client->setOnClientDestroyed(clientDestroyed);

    mClient = client;
    *outClient = client;

    mHal->setAidlClientPid(AIBinder_getCallingPid());

    return ndk::ScopedAStatus::ok();
}

bool Composer::waitForClientDestroyedLocked(
        std::unique_lock<std::mutex>& lock) {
    if (!mClient.expired()) {
      // In surface flinger we delete a composer client on one thread and
      // then create a new client on another thread. Although surface
      // flinger ensures the calls are made in that sequence (destroy and
      // then create), sometimes the calls land in the composer service
      // inverted (create and then destroy). Wait for a brief period to
      // see if the existing client is destroyed.
      constexpr const auto kTimeout = std::chrono::seconds(5);
      mClientDestroyedCondition.wait_for(
          lock, kTimeout, [this]() -> bool { return mClient.expired(); });
      if (!mClient.expired()) {
        ALOGW("%s: previous client was not destroyed", __FUNCTION__);
      }
    }

    return mClient.expired();

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

    const std::array<Capability, 6> all_caps = {{
        Capability::SIDEBAND_STREAM,
        Capability::SKIP_CLIENT_COLOR_TRANSFORM,
        Capability::PRESENT_FENCE_IS_NOT_RELIABLE,
        Capability::SKIP_VALIDATE,
        Capability::BOOT_DISPLAY_CONFIG,
#if (PLATFORM_SDK_VERSION > 33) || (PLATFORM_SDK_VERSION == 33 \
            && (ANDROID_PLATFORM_SDK_EXTENSION_VERSION >= 5))
        Capability::HDR_OUTPUT_CONVERSION_CONFIG,
#endif
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
