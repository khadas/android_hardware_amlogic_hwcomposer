/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DisplayAdapter.h"
#include "DisplayClient.h"
#include "DisplayAdapterRemote.h"
#include "MesonLog.h"

#define IF_SERVER_NOT_READY_RETURN(ret) \
    if (!connectServerIfNeed()) { \
        MESON_LOGD("Can't connect with MesonDisplay server!"); \
        return ret; \
    }

namespace meson{
using namespace std;
using ConnectorType = DisplayAdapter::ConnectorType;
using BackendType = DisplayAdapter::BackendType;

DisplayAdapter::BackendType DisplayAdapterRemote::displayType() {
    Json::Value cmd, ret;
    IF_SERVER_NOT_READY_RETURN(DisplayAdapter::DISPLAY_TYPE_MAX);
    cmd["cmd"] = "displayType";
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret")) {
        MESON_LOGE("Server reply error!");
        return (DisplayAdapter::BackendType)ret["ret"].asUInt();
    } else {
        return DisplayAdapter::DISPLAY_TYPE_MAX;
    }
}

bool DisplayAdapterRemote::getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
    Json::Value cmd, ret;
    Json::FastWriter write;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "getSupportDisplayModes";
    cmd["p_displayType"] = displayType;
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret") && ret["ret"].isMember("displayModeList")) {
        Json::Value& list = ret["ret"]["displayModeList"];
        displayModeList.clear();
        for (unsigned int i = 0; i < list.size(); i++) {
            Json::Value& mode_in = list[i];
            DisplayModeInfo mode;
            if (true == Json2DisplayMode(mode_in, mode)) {
                displayModeList.push_back(mode);
            } else {
                MESON_LOGE("Get Wrong DisplayMode info");
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

bool DisplayAdapterRemote::getDisplayMode(string& mode, ConnectorType displayType) {
    Json::Value cmd, ret;
    Json::FastWriter write;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "getDisplayMode";
    cmd["p_displayType"] = displayType;
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret") && ret["ret"]["mode"].isString()) {
        mode = ret["ret"]["mode"].asString();
        return true;
    } else
        return false;
}

bool DisplayAdapterRemote::setDisplayMode(const string& mode, ConnectorType displayType) {
    Json::Value cmd;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "setDisplayMode";
    cmd["p_mode"] = mode;
    cmd["p_displayType"] = displayType;
    ipc->send_request(cmd);
    return true;
}

bool DisplayAdapterRemote::captureDisplayScreen(const native_handle_t **outBufferHandle) {
    IF_SERVER_NOT_READY_RETURN(false);
    return ipc->captureDisplayScreen(outBufferHandle);
}

bool DisplayAdapterRemote::setDisplayRect(const Rect rect, ConnectorType displayType) {
    Json::Value cmd;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "setDisplayViewPort";
    cmd["rect"] = rect.toString().c_str();
    cmd["p_displayType"] = displayType;
    ipc->send_request(cmd);
    return true;
}

bool DisplayAdapterRemote::getDisplayRect(Rect& rect, ConnectorType displayType) {
    Json::Value cmd, ret;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "getDisplayViewPort";
    cmd["p_displayType"] = displayType;

    ipc->send_request_wait_reply(cmd, ret);

    if (ret.isMember("ret") && ret["ret"]["rect"].isString()) {
        rect = ret["ret"]["rect"].asString().c_str();
        return true;
    } else
        return false;
}

bool DisplayAdapterRemote::setDisplayAttribute(
        const string& name, const string& value,
        ConnectorType displayType) {
    Json::Value cmd;
    if (name.empty())
        return false;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "setDisplayAttribute";
    cmd["name"] = name.c_str();
    cmd["value"] = value.c_str();
    cmd["p_displayType"] = displayType;
    ipc->send_request(cmd);
    return true;
}

bool DisplayAdapterRemote::getDisplayAttribute(
        const string& name, string& value,
        ConnectorType displayType) {
    Json::Value cmd, ret;
    if (name.empty())
        return false;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "getDisplayAttribute";
    cmd["name"] = name.c_str();
    cmd["p_displayType"] = displayType;
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret") && ret["ret"]["value"].isString()) {
        value = ret["ret"]["value"].asString();
        return true;
    } else {
        return false;
    }
}

bool DisplayAdapterRemote::getDisplayVsyncAndPeriod(int64_t& vsyncTimestamp, int32_t& vsyncPeriodNanos) {
    Json::Value cmd, ret;
    IF_SERVER_NOT_READY_RETURN(false);

    cmd["cmd"] = "getDisplayVsyncAndPeriod";
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret") && ret["ret"]["value"].isString()) {
        sscanf(ret["ret"]["value"].asString().c_str(), "%" PRId64 ",%d",
                &vsyncTimestamp, &vsyncPeriodNanos);
        return true;
    } else {
        return false;
    }
}

DisplayAdapterRemote::DisplayAdapterRemote() {
    ipc = DisplayClient::create("DisplayAdapterRemote");
    if (!ipc)
        MESON_LOGE("Error when connect with Server");
};


bool DisplayAdapterRemote::dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) {
    Json::Value cmd, ret;
    IF_SERVER_NOT_READY_RETURN(false);
    cmd["cmd"] = "dumpDisplayAttribute";
    cmd["p_displayType"] = displayType;
    ipc->send_request_wait_reply(cmd, ret);
    if (ret.isMember("ret")) {
        Json::StyledWriter  write;
        DEBUG_INFO("Dump attributes:\n%s", write.write(ret["ret"]).c_str());
        json = ret["ret"];
        return true;
    }
    return false;
};

std::unique_ptr<DisplayAdapter> DisplayAdapterRemote::create() {
    return static_cast<std::unique_ptr<DisplayAdapter>>(std::make_unique<DisplayAdapterRemote>());
}

bool DisplayAdapterRemote::connectServerIfNeed() {
        return ipc->tryGetService();
}

std::unique_ptr<DisplayAdapter> DisplayAdapterCreateRemote() {
    return DisplayAdapterRemote::create();
}

// provide both a creation and a destruction function for users
// who load C++ functions and classes dynamically using the dlopen API
extern "C" {

DisplayAdapter* createDisplayAdatperRemote() {
    return new DisplayAdapterRemote;
}

void destroyDisplayAdapterRemote(DisplayAdapter * adapter) {
    delete adapter;
}

}

} //namespace android
