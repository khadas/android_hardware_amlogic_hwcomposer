/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DisplayAdapter.h"
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

}; //namespace meson
