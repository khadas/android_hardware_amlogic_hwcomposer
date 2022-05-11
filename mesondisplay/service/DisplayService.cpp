/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 * Meson display server side implement
 */

#define LOG_NDEBUG 1

#include <android-base/file.h>

#include "DisplayService.h"
#include "MesonLog.h"
#include "misc.h"

namespace meson{
using namespace std;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::vendor::amlogic::display::meson_display_ipc::V1_0::Error;
using ConnectorType = DisplayAdapter::ConnectorType;

MesonIpcServer::MesonIpcServer() {
}

Return<void> MesonIpcServer::debug(const hidl_handle &handle __unused, const hidl_vec<hidl_string> & __unused) {
    return Void();
}

bool MesonIpcServer::check_recursion_record_and_push(const hidl_string& str) {
    if (recursion_record.size() >= RECURSION_LIMIT) {
        MESON_LOGD("Service reach recursion limited(%d), show stack below", RECURSION_LIMIT);
        //show the first str on top
        MESON_LOGD("%s", str.c_str());
        while (!recursion_record.empty()) {
            std::string stack;
            recursion_record.top(stack);
            MESON_LOGD("%s", stack.c_str());
            recursion_record.pop();
        }
        return false;
    }
    recursion_record.push(str);
    return true;
};

Return<void> MesonIpcServer::send_msg_wait_reply(const hidl_string& msg_in, send_msg_wait_reply_cb _hidl_cb) {
    MESON_LOGV("Server << %s", msg_in.c_str());

    if (!check_recursion_record_and_push(msg_in)) {
        _hidl_cb("");
        return Void();
    }

    Json::Value value;
    bool ret = String2JsonValue(msg_in.c_str(), value);

    if (!ret) {
        MESON_LOGE("Server message decode error!");
        _hidl_cb("");
    } else {
        Json::Value reply;
        message_handle(value, reply);
        hidl_string out_str = JsonValue2String(reply);
        MESON_LOGV("Server >> %s", out_str.c_str());
        _hidl_cb(out_str);
    }
    recursion_record.pop();
    return Void();
}

Return<void> MesonIpcServer::send_msg(const hidl_string& msg_in) {
    MESON_LOGV("Server:<<:%s:", msg_in.c_str());

    if (!check_recursion_record_and_push(msg_in)) {
        return Void();
    }

    Json::Value value;
    bool ret = String2JsonValue(msg_in.c_str(), value);

    if (!ret) {
        MESON_LOGE("Server message decode error!");
    } else {
        Json::Value reply;
        message_handle(value, reply);
    }
    recursion_record.pop();
    return Void();
}

void MesonIpcServer::message_handle(Json::Value& in, Json::Value& out) {
    UNUSED(in);
    UNUSED(out);
    NOTIMPLEMENTED;
}

Return<void> MesonIpcServer::captureDisplayScreen(const int32_t displayId, const int32_t layerId,
        captureDisplayScreen_cb hidl_cb) {
    UNUSED(displayId);
    UNUSED(layerId);
    UNUSED(hidl_cb);
    NOTIMPLEMENTED;
    return Void();
}

DisplayServer::DisplayServer(std::unique_ptr<DisplayAdapter>& adapter) {
    if (!adapter) {
        MESON_LOGE("Server create with null adapter!");
    }
    this->adapter = std::move(adapter);
#if 1
    if (registerAsService() != android::OK) {
        MESON_LOGE("Server RegisterAsServer failed(%d)!", registerAsService());
    } else {
        MESON_LOGD("register success");
    }
#endif
}

void DisplayServer::message_handle(Json::Value& in, Json::Value& out) {
    std::string cmd,tmp1;
    Json::Value ret;
    if (!adapter || !in.isMember("cmd")) {
        MESON_LOGE("Server: Display Adapter not ready or cmd formate issue!");
        return;
    }
    cmd = in["cmd"].asString();
    if (cmd == "displayType") {
        ret = adapter->displayType();
    } else if (cmd == "getSupportDisplayModes") {
        vector<DisplayModeInfo> displayModeList;
        Json::Value list;
        if (!in.isMember("p_displayType"))
            goto OUT;
        adapter->getSupportDisplayModes(displayModeList, (ConnectorType)in["p_displayType"].asUInt());
        int index = 0;
        for (auto i : displayModeList) {
            Json::Value mode;
            if (true == DisplayMode2Json(i, mode)) {
                list[index++] = mode;
            }
        }
        ret["displayModeList"] = list;
    } else if (cmd == "getDisplayMode") {
        std::string mode;
        if (!in.isMember("p_displayType"))
            goto OUT;
        adapter->getDisplayMode(mode, (ConnectorType)in["p_displayType"].asUInt());
        ret["mode"] = mode;
    } else if (cmd == "setDisplayMode") {
        if (!in.isMember("p_displayType") || !in.isMember("p_mode"))
            goto OUT;
        adapter->setDisplayMode(in["p_mode"].asString(), (ConnectorType)in["p_displayType"].asUInt());
    } else if (cmd == "setDisplayViewPort") {
        if (!in.isMember("p_displayType") || !in.isMember("rect"))
            goto OUT;
        adapter->setDisplayRect(in["rect"].asString().c_str(), (ConnectorType)in["p_displayType"].asUInt());
    } else if (cmd == "getDisplayViewPort") {
        Rect rect;
        if (!in.isMember("p_displayType"))
            goto OUT;
        adapter->getDisplayRect(rect, (ConnectorType)in["p_displayType"].asUInt());
        ret["rect"] = rect.toString();
    } else if (cmd == "setDisplayAttribute") {
        if (!in.isMember("name") || !in.isMember("value") || !in.isMember("p_displayType"))
            goto OUT;
        adapter->setDisplayAttribute(in["name"].asString(), in["value"].asString(),
                (ConnectorType) in["p_displayType"].asUInt());
    } else if (cmd == "getDisplayAttribute") {
        if (!in.isMember("name") || !in.isMember("p_displayType"))
            goto OUT;
        string value;
        adapter->getDisplayAttribute(in["name"].asString(), value,
                (ConnectorType) in["p_displayType"].asUInt());
        ret["value"] = value;
    } else if (cmd == "getDisplayVsyncAndPeriod") {
        int64_t vsyncTimestamp;
        int32_t vsyncPeriod;
        adapter->getDisplayVsyncAndPeriod(vsyncTimestamp, vsyncPeriod);
        std::string value = std::to_string(vsyncTimestamp) + "," + std::to_string(vsyncPeriod);
        ret["value"] = value;
    } else if (cmd == "dumpDisplayAttribute") {
        if (!in.isMember("p_displayType"))
            goto OUT;
        adapter->dumpDisplayAttribute(ret, (ConnectorType)in["p_displayType"].asUInt());
    } else {
        MESON_LOGE("CMD not implement!");
    }
OUT:
    out["ret"] = ret;
}

Return<void> DisplayServer::captureDisplayScreen(const int32_t displayId, const int32_t layerId,
        captureDisplayScreen_cb hidl_cb) {
    UNUSED(displayId);
    UNUSED(layerId);

    MESON_LOGD("DisplayServer captureDisplayScreen");
    hidl_vec<hidl_handle> outHandles;

    if (!adapter) {
        MESON_LOGD("DisplayServer display Adatprer not ready");
        outHandles.setToExternal(nullptr, 0);
        hidl_cb(Error::NO_RESOURCES, outHandles);
        return Void();
    }

    const native_handle_t* bufferHandle = nullptr;
    bool ret = adapter->captureDisplayScreen(&bufferHandle);
    if (!ret) {
        outHandles.setToExternal(nullptr, 0);
        hidl_cb(Error::NO_RESOURCES, outHandles);
        return Void();
    }

    std::vector<hidl_handle> handles;
    handles.push_back(bufferHandle);

    outHandles.setToExternal(const_cast<hidl_handle*>(handles.data()), handles.size());
    hidl_cb(Error::NONE, outHandles);

    // release native handle
    if (bufferHandle) {
        gralloc_free_dma_buf(const_cast<native_handle_t*> (bufferHandle));
    }
    return Void();
}

Return<void> DisplayServer::debug(const hidl_handle &handle, const hidl_vec<hidl_string> &) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];
        std::ostringstream dump_buf;

        dump_buf << "MesonDisplay initialized properly." << std::endl;

        std::string buf = dump_buf.str();
        if (!android::base::WriteStringToFd(buf, fd)) {
            MESON_LOGE("Failed to dump state to fd");
        }

        fsync(fd);
    }

    return Void();
}


} //namespace android
