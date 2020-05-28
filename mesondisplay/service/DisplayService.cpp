/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 * Meson display server side implement
 */

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

bool MesonIpcServer::check_recursion_record_and_push(const std::string& str) {
    if (recursion_record.size() >= RECURSION_LIMIT) {
        DEBUG_INFO("Service reach recursion limited(%d), show stack below", RECURSION_LIMIT);
        //show the first str on top
        DEBUG_INFO("%s", str.c_str());
        while (!recursion_record.empty()) {
            std::string stack;
            recursion_record.top(stack);
            DEBUG_INFO("%s", stack.c_str());
            recursion_record.pop();
        }
        return false;
    }
    recursion_record.push(str);
    return true;
};

Return<void> MesonIpcServer::send_msg_wait_reply(const hidl_string& msg_in, send_msg_wait_reply_cb _hidl_cb) {
    bool ret;
    Json::Reader reader;
    Json::Value value, reply;
    Json::FastWriter write;
    hidl_string out_str;
    const std::string tmp = msg_in.c_str();
    DEBUG_INFO("Server(s):<<:%s:", msg_in.c_str());

    if (!check_recursion_record_and_push(tmp)) {
        _hidl_cb("");
        return Void();
    }

    ret = reader.parse(tmp, value);
    if (!ret) {
        DEBUG_INFO("Server message decode error!");
        _hidl_cb("");
    } else {
        message_handle(value, reply);
        out_str = write.write(reply);
        DEBUG_INFO("Server:>>:%s:", out_str.c_str());
        _hidl_cb(out_str);
    }
    recursion_record.pop();
    return Void();
}

Return<void> MesonIpcServer::send_msg(const hidl_string& msg_in) {
    bool ret;
    Json::Reader reader;
    Json::Value value, reply;
    const std::string tmp = msg_in.c_str();
    DEBUG_INFO("Server:<<:%s:", msg_in.c_str());

    if (!check_recursion_record_and_push(tmp)) {
        return Void();
    }

    ret = reader.parse(tmp, value);
    if (!ret) {
        DEBUG_INFO("Server message decode error!");
    } else {
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
        DEBUG_INFO("Server create with null adapter!");
    }
    this->adapter = std::move(adapter);
#if 1
    if (registerAsService() != android::OK) {
        DEBUG_INFO("Server RegisterAsServer failed(%d)!", registerAsService());
    } else {
        DEBUG_INFO("register success");
    }
#endif
}

void DisplayServer::message_handle(Json::Value& in, Json::Value& out) {
    std::string cmd,tmp1;
    Json::Value ret;
    if (!adapter || !in.isMember("cmd")) {
        DEBUG_INFO("Server: Display Adapter not ready or cmd formate issue!");
        return;
    }
    cmd = in["cmd"].asString();
    DEBUG_INFO("Server Handle[%s]", cmd.c_str());
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
    } else if (cmd == "setPrefDisplayMode") {
        if (!in.isMember("p_displayType") || !in.isMember("p_mode"))
            goto OUT;
        adapter->setPrefDisplayMode(in["p_mode"].asString(), (ConnectorType)in["p_displayType"].asUInt());
    } else {
        DEBUG_INFO("CMD not implement!");
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
        gralloc_unref_dma_buf(const_cast<native_handle_t*> (bufferHandle));
        gralloc_free_dma_buf(const_cast<native_handle_t*> (bufferHandle));
    }
    return Void();
}

} //namespace android
