/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include "DisplayClient.h"
#include "DisplayAdapter.h"
#include "MesonLog.h"

namespace meson{
using namespace std;
using namespace ::android::hardware::graphics;

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;
using ::vendor::amlogic::display::meson_display_ipc::V1_0::IMesonDisplayIPC;
using ::vendor::amlogic::display::meson_display_ipc::V1_0::Error;

static const int MAX_GET_SERVICE_COUNT = 5;

DisplayClient::DisplayClient(std::string str)
    : name(str) {
        meson_ipc_client = NULL;
        is_ready = false;
        mMapper_ready = false;
}

bool DisplayClient::tryGetService() {
    if (is_ready == true)
        return true;

    int count = 0;
    while (meson_ipc_client == NULL && count <= MAX_GET_SERVICE_COUNT) {
        MESON_LOGD("Try get meson_ipc service(%d)", count);
        meson_ipc_client  = IMesonDisplayIPC::tryGetService();
        count++;
    }

    if (meson_ipc_client)
        is_ready = true;
    else
        is_ready = false;

    return is_ready;
}

std::unique_ptr<DisplayClient> DisplayClient::create(std::string name) {
    return std::unique_ptr<DisplayClient>(new DisplayClient(name));
}

int32_t DisplayClient::send_request_wait_reply(Json::Value& data, Json::Value& out) {
    bool ret = false;
    const hidl_string str = JsonValue2String(data);
    if (is_ready) {
        MESON_LOGV("Client SendSync :%s", str.c_str());
        meson_ipc_client->send_msg_wait_reply(str, [&out, &ret](const hidl_string& in) {
                ret = String2JsonValue(in.c_str(), out);
                });
    }
    if (ret == false) {
        MESON_LOGE("Server reply format error !");
    }
    return 0;
}

int32_t DisplayClient::send_request(Json::Value& data) {
    const hidl_string str = JsonValue2String(data);
    if (is_ready) {
        MESON_LOGV("Client Send :%s", str.c_str());
        meson_ipc_client->send_msg(str);
    }
    return 0;
}

bool DisplayClient::captureDisplayScreen(const native_handle_t** outBufferHandle) {
    MESON_LOGD("Client captureDisplayScreen");
    if (!is_ready) {
        MESON_LOGE("captureDisplayScreen Server not ready");
        return false;
    }

    Error error;
    hidl_vec<hidl_handle> dataHandles;

    meson_ipc_client->captureDisplayScreen(0 /*displayId reserved*/, 0 /*layerId reserved*/,
            [&] (const auto& tmpError, const auto& tmpOutHandles){
                error = tmpError;
                dataHandles.setToExternal(const_cast<hidl_handle*>(tmpOutHandles.data()),
                    tmpOutHandles.size());
            });
    if (error != Error::NONE) {
        MESON_LOGE("captureDislayScreen failed");
        return false;
    }

    // only import the first buffer handle.
    // We assumed there is only one native handle currently
    if (dataHandles.size() > 0) {
        importBuffer(dataHandles[0], outBufferHandle);
    }

    return true;
}

bool DisplayClient::initMap() {
    if (mMapper_ready == true)
        return true;

    mMapper4 = mapper::V4_0::IMapper::getService();
    if (mMapper4) {
        return true;
    }

    mMapper3 = mapper::V3_0::IMapper::getService();
    if (mMapper3) {
        return true;
    }

    return false;
}

bool DisplayClient::importBuffer(hidl_handle &rawHandle, const native_handle_t** outBufferHandle) {
    if (initMap() == false) {
        *outBufferHandle = nullptr;
        return false;
    }

    const native_handle_t *bufferHandle = nullptr;

    if (mMapper3) {
        mMapper3->importBuffer(rawHandle,
            [&](const auto& tmpError, const auto& tmpBufferHandle) {
                if (tmpError != mapper::V3_0::Error::NONE) {
                    MESON_LOGE("captureDisplayScreen mapper import buffer error:%d", tmpError);
                    return;
                }
                bufferHandle = static_cast<const native_handle_t*>(tmpBufferHandle);
        });
    }
    if (mMapper4) {
        mMapper4->importBuffer(rawHandle,
            [&](const auto& tmpError, const auto& tmpBufferHandle) {
                if (tmpError != mapper::V4_0::Error::NONE) {
                    MESON_LOGE("captureDisplayScreen mapper import buffer error:%d", tmpError);
                    return;
                }
                bufferHandle = static_cast<const native_handle_t*>(tmpBufferHandle);
        });
    }

    *outBufferHandle = bufferHandle;
    return true;
}

} // meson
