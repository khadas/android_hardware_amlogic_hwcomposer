/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DisplayAdapter.h"
#include <inttypes.h>
#include <json/json.h>
namespace meson{

bool Json2DisplayMode(const Json::Value& json, DisplayModeInfo& mode) {
    if (json.isMember("name") && json.isMember("dpiX") &&
        json.isMember("dpiY") && json.isMember("pixelW") &&
        json.isMember("pixelH") && json.isMember("refreshRate")) {
        if (!json["name"].isString()) {
            return false;
        }
        mode.name = json["name"].asString();
        mode.dpiX = json["dpiX"].asUInt();
        mode.dpiY = json["dpiY"].asUInt();
        mode.pixelW = json["pixelW"].asUInt();
        mode.pixelH = json["pixelH"].asUInt();
        mode.refreshRate = json["refreshRate"].asFloat();
        return true;
    }
    return false;
};

bool DisplayMode2Json(const DisplayModeInfo& mode, Json::Value& json) {
    json["name"] = mode.name;
    json["dpiX"] = mode.dpiX;
    json["dpiY"] = mode.dpiY;
    json["pixelW"] = mode.pixelW;
    json["pixelH"] = mode.pixelH;
    json["refreshRate"] = mode.refreshRate;
    return true;
};

std::string JsonValue2String(const Json::Value& json) {
    std::string jsonStr;

    Json::StreamWriterBuilder factory;
    jsonStr = Json::writeString(factory, json);

    return jsonStr;
}

bool String2JsonValue(const std::string& inStr, Json::Value& out) {
    bool ret = false;

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    ret = reader->parse(inStr.c_str(),
                        inStr.c_str() + inStr.size(),
                        &out, /* error_message = */ nullptr);

    return ret;
}

}; //namespace meson
