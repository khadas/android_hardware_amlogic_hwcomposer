/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once

#include <json/json.h>
#include <stdbool.h>
#include <string>

#include <utils/StrongPointer.h>
#include <cutils/native_handle.h>

#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <vendor/amlogic/display/meson_display_ipc/1.0/IMesonDisplayIPC.h>

#include "utile.h"

namespace meson {
using namespace std;
using ::android::sp;
using ::vendor::amlogic::display::meson_display_ipc::V1_0::IMesonDisplayIPC;
using ::android::hardware::hidl_handle;
using IMapper_3_0 = ::android::hardware::graphics::mapper::V3_0::IMapper;
using IMapper_4_0 = ::android::hardware::graphics::mapper::V4_0::IMapper;

class DisplayClient {
public:
    ~DisplayClient() = default;
    bool reconnect();
    bool tryGetService();
    int32_t send_request_wait_reply(Json::Value& data, Json::Value& out);
    int32_t send_request(Json::Value& data);
    bool captureDisplayScreen(const native_handle_t** outBufferHandle);
    static DisplayClient &getInstance();
    bool getWhiteBoardHanle(const native_handle_t** outBufferHandle);

protected:
    std::string name;
    sp<IMesonDisplayIPC> meson_ipc_client;
    sp<IMapper_3_0> mMapper3;
    sp<IMapper_4_0> mMapper4;

    bool is_ready;
private:
    DisplayClient(std::string name);
    bool initMap();
    bool importBuffer(hidl_handle &rawHandle, const native_handle_t** outBufferHandle);

    bool mMapper_ready;

    struct DisplayServerDeathRecipient : public android::hardware::hidl_death_recipient {
        DisplayServerDeathRecipient(DisplayClient *client): mClient(client) {};

        // hidl_death_recipient interface
        virtual void serviceDied(uint64_t cookie,
            const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override;
        private:
            DisplayClient *mClient;
    };

    sp<DisplayServerDeathRecipient> mDisplayServerDeathRecipient = NULL;

    static std::mutex sLock;
    static DisplayClient *sInstance;
};

} //namespace meson
