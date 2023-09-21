/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */
#pragma once
#include <stdlib.h>
#include <string.h>

#include <json/value.h>
#include <json/json.h>

#include <utils/String8.h>

#include "mode_policy.h"
#include "mode_ubootenv.h"

#define DOLBY_VISION_LL_RGB             3
#define DOLBY_VISION_LL_YUV             2
#define DOLBY_VISION_STD_ENABLE         1
#define DOLBY_VISION_DISABLE            0

#define COLOR_YCBCR444_12BIT             "444,12bit"
#define COLOR_YCBCR444_10BIT             "444,10bit"
#define COLOR_YCBCR444_8BIT              "444,8bit"
#define COLOR_YCBCR422_12BIT             "422,12bit"
#define COLOR_YCBCR422_10BIT             "422,10bit"
#define COLOR_YCBCR422_8BIT              "422,8bit"
#define COLOR_YCBCR420_12BIT             "420,12bit"
#define COLOR_YCBCR420_10BIT             "420,10bit"
#define COLOR_YCBCR420_8BIT              "420,8bit"
#define COLOR_RGB_12BIT                  "rgb,12bit"
#define COLOR_RGB_10BIT                  "rgb,10bit"
#define COLOR_RGB_8BIT                   "rgb,8bit"


enum {
    DISPLAY_TYPE_NONE                   = 0,
    DISPLAY_TYPE_TABLET                 = 1,
    DISPLAY_TYPE_MBOX                   = 2,
    DISPLAY_TYPE_TV                     = 3,
    DISPLAY_TYPE_REPEATER               = 4
};

class VtsModePolicy {
public:
    VtsModePolicy();
    ~VtsModePolicy();

    int setUp();
    int tearDown();

    void getPolicyIn(meson_policy_in &in) {in = mIn;};
    void getModePolicy(meson_mode_policy &policy) {policy = mPolicy;};
    void setModePolicy(meson_mode_policy policy) {mPolicy = policy;};
    void getModePolicyOut(meson_policy_out &out) {out = mOut;};
    void resetPolicy() {mPolicy = MESON_POLICY_INVALID;};

    void setModeState(meson_mode_state_e state) {mIn.state = state;};

    void setHdrPolicy(meson_hdr_policy_e policy) {mIn.hdr_info.hdr_policy = policy;};
    void setHdrPriority(meson_hdr_priority_e priority) {mIn.hdr_info.hdr_priority = priority;};
    void setDvEnable(bool enable) {mIn.hdr_info.is_tv_supportDv = enable;};
    void setEdidParsing(const std::string parsing) {strncpy(mIn.con_info.edid_parsing, parsing.c_str(), MESON_MODE_LEN);};
    void setSinkType(const meson_sink_type type) {mIn.con_info.sink_type = type;};
    void setDeepColorMode(bool enable) {mIn.con_info.is_deepcolor = enable;};
    void setBestColorSpace(bool enable) {mIn.con_info.is_bestcolorspace = enable;};
    void setSupport4k(bool support) {mIn.con_info.is_support4k = support;};
    void setCurMode(const std::string mode) {strncpy(mIn.cur_displaymode, mode.c_str(), MESON_MODE_LEN);};
    void setDvColor(const std::string color) {strncpy(mIn.hdr_info.dv_deepcolor, color.c_str(), MESON_MODE_LEN);};
    void setUenvDvType(const std::string type) {strncpy(mIn.hdr_info.ubootenv_dv_type, type.c_str(), MESON_MODE_LEN);};
    void setDvMaxMode(const std::string type) {strncpy(mIn.hdr_info.dv_max_mode, type.c_str(), MESON_MODE_LEN);};

    const char *getCurMode() {return mIn.cur_displaymode;};
    const char *getCvbsMode() {return mIn.con_info.ubootenv_cvbsmode;};
    const char *getUenvColor() {return mIn.con_info.ubootenv_colorattr;};

    int setEdidInfoPath(const char * path);
    int sceneProcess();
    void dump();

private:
    int parseConnectorInfo(const Json::Value &root);
    int parseHdrInfo(const Json::Value &root);
    void parseDcCap(const Json::Value &root);
    void jsonMode2MesonMode(meson_mode_info_t &mesonMode, const Json::Value & jsonMode);
    meson_connector_type_e parseConnectorType(const std::string &value);
    meson_sink_type_e parseSinkType(const std::string & value);
    meson_hdr_priority_e parseHdrPriority(const std::string &value);
    meson_hdr_policy_e parseHdrPolicy(const std::string &value);

    struct meson_policy_in mIn;
    struct meson_policy_out mOut;
    enum meson_mode_policy mPolicy;

    uint32_t mDisplayId;
    int32_t mDisplayType;
    //output_change_reason mReason;
    meson_connector_type_e mModeConType;
   // output_mode_state mState;
};
