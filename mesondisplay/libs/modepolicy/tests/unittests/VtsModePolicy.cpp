/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>

#include <log/log.h>

#include "VtsModePolicy.h"


VtsModePolicy::VtsModePolicy() {
    mDisplayType = DISPLAY_TYPE_MBOX;
    mPolicy = MESON_POLICY_BEST;
    mDisplayId = 0;
    mModeConType = MESON_MODE_HDMI;
    memset(&mIn, 0, sizeof(mIn));
    memset(&mOut, 0, sizeof(mOut));
}

VtsModePolicy::~VtsModePolicy() {
    if (mIn.con_info.modes)
        free(mIn.con_info.modes);
}

int VtsModePolicy::setUp() {
    meson_mode_set_test_mode(true);
    return 0;
}

int VtsModePolicy::tearDown() {
    meson_mode_set_test_mode(false);
    return 0;
}

/*
 * It's better that we can parse the real EDID
 * But now read it from json file
 *
 * JSON EDID FILE formats:
 *
 * {
 *      "ConnectorType": "HDMI",
 *      "ColorAttribute": [
 *          "444,8bit",
 *          "rgb,8bit"
 *      ],
 *      "HDR": "HLG",
 *      "Modes": [
 *          {"name": "1080p60hz", "pixelW": 1920, "pixelH": 1080, "dpiX": 42406, "dpiY": 42203, "refreshRate": 60},
 *          {"name": "1080p50hz", "pixelW": 1920, "pixelH": 1080, "dpiX": 42406, "dpiY": 42203, "refreshRate": 50}
 *      ]
 *  }
 *
 */

int VtsModePolicy::setEdidInfoPath(const char *path) {
    Json::Reader reader;  //for reading the data
    Json::Value root; //for modifying and storing new values

    // opening file using fstream
    std::ifstream file(path);

    // check if there is any error is getting data from the json file
    if (!reader.parse(file, root)) {
        ALOGD("%s", reader.getFormattedErrorMessages().c_str());
        return -EINVAL;
    }

    ALOGD("%s %s", __func__, path);
    parseConnectorInfo(root);
    parseHdrInfo(root);

    return 0;
}

int VtsModePolicy::parseConnectorInfo(const Json::Value &root) {
    // state INIT
    mIn.state = MESON_SCENE_STATE_INIT;
    strncpy(mIn.cur_displaymode, root["currentMode"].asString().c_str(), MESON_MODE_LEN);

    mModeConType = parseConnectorType(root["ConnectorType"].asString());

    // modes transfer to meson_mode_info_t
    const Json::Value modes = root["Modes"];
    auto conPtr = &mIn.con_info;
    conPtr->modes_size = modes.size();
    if (conPtr->modes_size > conPtr->modes_capacity) {
        // realloc memory
        void * buff = realloc(conPtr->modes, conPtr->modes_size * sizeof(meson_mode_info_t));
        //A(buff, "modePolicy realloc but has no memory");
        conPtr->modes = (meson_mode_info_t *) buff;
        conPtr->modes_capacity = conPtr->modes_size;
    }

    for (int index = 0; index < modes.size(); index++) {
        jsonMode2MesonMode(conPtr->modes[index], modes[index]);
    }

    conPtr->sink_type = parseSinkType(root["SinkType"].asString());
    strncpy(conPtr->edid_parsing, root["EdidParsing"].asString().c_str(), MESON_MODE_LEN);
    strncpy(conPtr->ubootenv_hdmimode, root["uenvHdmiMode"].asString().c_str(), MESON_MODE_LEN);
    strncpy(conPtr->ubootenv_cvbsmode, root["uenvCvbsMode"].asString().c_str(), MESON_MODE_LEN);
    strncpy(conPtr->ubootenv_colorattr, root["uenvColorAttribute"].asString().c_str(), MESON_MODE_LEN);
    conPtr->is_support4k = root["support4k"].asBool();
    conPtr->is_deepcolor = root["deepColor"].asBool();
    conPtr->is_bestcolorspace = true;

    return 0;
}

void VtsModePolicy::parseDcCap(const Json::Value &root) {
    const Json::Value colors = root["ColorAttribute"];
    auto conPtr = &mIn.con_info;
    conPtr->dc_cap[0] = '\0';

    for (int index = 0; index < colors.size(); index++) {
        if (strlen(conPtr->dc_cap) + strlen(colors[index].asString().c_str()) > MESON_MAX_STR_LEN)
            break;

        strcat(conPtr->dc_cap, colors[index].asString().c_str());
        strcat(conPtr->dc_cap, "\n");
    }
}

int VtsModePolicy::parseHdrInfo(const Json::Value &root) {
    auto hdrPtr = &mIn.hdr_info;
    hdrPtr->is_enable_dv = root["enableDV"].asBool();
    hdrPtr->is_tv_supportHDR = root["supportHdr"].asBool();
    hdrPtr->is_tv_supportDv = root["supportDv"].asBool();
    hdrPtr->is_hdr_resolution_priority = root["hdrResolutionPriority"].asBool();
    hdrPtr->is_lowpower_mode = root["lowPowerMode"].asBool();

    hdrPtr->hdr_priority = parseHdrPriority(root["hdrPriority"].asString());
    hdrPtr->hdr_policy = parseHdrPolicy(root["hdrPolicy"].asString());
    strncpy(hdrPtr->dv_max_mode, root["dvMaxMode"].asString().c_str(), MESON_MODE_LEN);
    strncpy(hdrPtr->dv_deepcolor, root["dvColorAttribute"].asString().c_str(), MESON_MODE_LEN);

    parseDcCap(root);

    return 0;
}

meson_connector_type_e VtsModePolicy::parseConnectorType(const std::string &value) {
    if (value.find("HDMI") != std::string::npos) {
        return MESON_MODE_HDMI;
    }

    return MESON_MODE_HDMI;
}

void VtsModePolicy::jsonMode2MesonMode(meson_mode_info_t &mesonMode, const Json::Value & jsonMode) {
    strncpy(mesonMode.name, jsonMode["name"].asString().c_str(), MESON_MODE_LEN);
    mesonMode.dpi_x = jsonMode["dpiX"].asInt();
    mesonMode.dpi_y = jsonMode["dpiY"].asInt();
    mesonMode.pixel_w = jsonMode["pixelW"].asInt();
    mesonMode.pixel_h = jsonMode["pixelH"].asInt();
    mesonMode.refresh_rate = jsonMode["refreshRate"].asFloat();
}

int VtsModePolicy::sceneProcess() {
    ALOGD("%s", __func__);
    meson_mode_set_policy(mModeConType, mPolicy);
    meson_mode_set_policy_input(mModeConType, &mIn);
    meson_mode_get_policy_output(mModeConType, &mOut);

    return 0;
}

void VtsModePolicy::dump() {
    auto hdrPtr = &mIn.hdr_info;
    ALOGD("supportHDR:%d\n", hdrPtr->is_tv_supportHDR);
    ALOGD("HDR priority:%d\n", hdrPtr->hdr_priority);
    ALOGD("HDR policy:%d\n", hdrPtr->hdr_policy);

    auto conPtr = &mIn.con_info;
    ALOGD("EdidParsing:%s\n", conPtr->edid_parsing);
    ALOGD("sinkType:%d\n", conPtr->sink_type);
    ALOGD("dc_cap :%s\n", conPtr->dc_cap);
    ALOGD("\nModePolicyTest support modes(%d):\n", conPtr->modes_size);
    ALOGD("-----------------------------------------------------------"
        "------------------\n");
    ALOGD("|  CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   NAME      |\n");
    ALOGD("+-----------+------------------+-----------+------------+"
        "----------------+\n");

    for (int i = 0; i < conPtr->modes_size; i++) {
        auto config = conPtr->modes[i];
        ALOGD(" %2d     |      %.3f      |   %5d   |   %5d    | %14s |\n",
            i,
            config.refresh_rate,
            config.pixel_w,
            config.pixel_h,
            config.name);
    }
    ALOGD("-----------------------------------------------------------"
        "---------------\n");
    ALOGD("out mode:%s\n", mOut.displaymode);
    ALOGD("out color:%s\n", mOut.deepcolor);
    ALOGD("out dv type:%d\n\n", mOut.dv_type);
}

meson_sink_type_e VtsModePolicy::parseSinkType(const std::string & value) {
    ALOGD("%s value:%s", __func__, value.c_str());
    if (value.find("none") != std::string::npos) {
        return MESON_SINK_TYPE_NONE;
    } else if (value.find("sink") != std::string::npos) {
        return MESON_SINK_TYPE_SINK;
    } else if (value.find("repeater") != std::string::npos) {
        return MESON_SINK_TYPE_REPEATER;
    }

    return MESON_SINK_TYPE_SINK;
}

meson_hdr_priority_e VtsModePolicy::parseHdrPriority(const std::string &value) {
    if (value.find("SDR") != std::string::npos) {
        return MESON_SDR_PRIORITY;
    } else if (value.find("HDR10") != std::string::npos) {
        return MESON_HDR10_PRIORITY;
    } else if (value.find("DV") != std::string::npos) {
       return MESON_DOLBY_VISION_PRIORITY;
    }

    return MESON_SDR_PRIORITY;
}

meson_hdr_policy_e VtsModePolicy::parseHdrPolicy(const std::string &value) {
    if (value.find("followSink") != std::string::npos) {
        return MESON_HDR_POLICY_SINK;
    } else if (value.find("followSource") != std::string::npos) {
        return MESON_HDR_POLICY_SOURCE;
    }

    return MESON_HDR_POLICY_SINK;
}
