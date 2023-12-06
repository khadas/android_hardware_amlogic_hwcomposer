/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once

#include <stdlib.h>
#include <string>
#include "DrmTypes.h"

#define DISPLAY_HPD_STATE               "/sys/class/amhdmitx/amhdmitx0/hpd_state"
//#define DISPLAY_HDMI_DISP_CAP           "/sys/class/amhdmitx/amhdmitx0/disp_cap"//RX support display mode
//#define DISPLAY_HDMI_DISP_CAP_3D        "/sys/class/amhdmitx/amhdmitx0/disp_cap_3d"//RX support display 3d mode
#define DISPLAY_HDMI_DEEP_COLOR         "/sys/class/amhdmitx/amhdmitx0/dc_cap"//RX support deep color

//#define DISPLAY_HDMI_AUDIO              "/sys/class/amhdmitx/amhdmitx0/aud_cap"
#define DISPLAY_HDMI_AUDIO_MUTE         "/sys/class/amhdmitx/amhdmitx0/aud_mute"
#define DISPLAY_HDMI_VIDEO_MUTE         "/sys/class/amhdmitx/amhdmitx0/vid_mute"
//#define DISPLAY_HDMI_MODE_PREF          "/sys/class/amhdmitx/amhdmitx0/preferred_mode"
#define DISPLAY_HDMI_SINK_TYPE          "/sys/class/amhdmitx/amhdmitx0/sink_type"
//#define DISPLAY_HDMI_VIC                "/sys/class/amhdmitx/amhdmitx0/vic"//if switch between 8bit and 10bit, clear mic first
#define DISPLAY_HDMI_USED               "/sys/class/amhdmitx/amhdmitx0/hdmi_used"


#define DISPLAY_HDMI_AVMUTE_SYSFS       "/sys/devices/virtual/amhdmitx/amhdmitx0/avmute"
#define DISPLAY_EDID_VALUE              "/sys/class/amhdmitx/amhdmitx0/edid"
#define DISPLAY_EDID_STATUS             "/sys/class/amhdmitx/amhdmitx0/edid_parsing"
#define DISPLAY_EDID_RAW                "/sys/class/amhdmitx/amhdmitx0/rawedid"
#define DISPLAY_HDMI_PHY                "/sys/class/amhdmitx/amhdmitx0/phy"

#define AUDIO_DSP_DIGITAL_RAW           "/sys/class/audiodsp/digital_raw"
#define AV_HDMI_CONFIG                  "/sys/class/amhdmitx/amhdmitx0/config"
//#define AV_HDMI_3D_SUPPORT              "/sys/class/amhdmitx/amhdmitx0/support_3d"

//auto low latency mode
#define AUTO_LOW_LATENCY_MODE_CAP       "/sys/class/amhdmitx/amhdmitx0/allm_cap"
#define AUTO_LOW_LATENCY_MODE           "/sys/class/amhdmitx/amhdmitx0/allm_mode"
#define HDMI_CONTENT_TYPE_CAP           "/sys/class/amhdmitx/amhdmitx0/contenttype_cap"
#define HDMI_CONTENT_TYPE               "/sys/class/amhdmitx/amhdmitx0/contenttype_mode"
#define HDMI_TX_FRAMERATE_POLICY        "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"
#define DISPLAY_HDMI_FRL_RATE           "/sys/class/amhdmitx/amhdmitx0/frl_rate"

class IModePolicy {
public:
    IModePolicy() {}
    virtual ~IModePolicy() { }

    virtual int32_t bindConnector(std::shared_ptr<HwDisplayConnector> & connector);
    virtual bool setPolicy(int32_t policy) = 0;
    virtual int32_t initialize() = 0;
    virtual void onHotplug(bool connected) = 0;

    //user change display settings by UI
    virtual int32_t clearUserDisplayConfig() = 0;
    virtual int32_t getCurrentSupportDeepColor(std::string &color) = 0;
    virtual int32_t setColorSpace(std::string &colorspace) = 0;
    virtual int32_t setDvMode(std::string &dv_mode) = 0;

    //TODO: refactor it
    virtual void setActiveConfig(std::string mode) = 0;

    // default boot config
    virtual int32_t getPreferredBootConfig(std::string &config) = 0;
    virtual int32_t setBootConfig(drm_mode_info_t & config) = 0;
    virtual int32_t clearBootConfig() = 0;

    //hdr strategy
    virtual void setAllowedHdrTypes(uint32_t allowedHdrTypes, bool isAuto, bool passThrough);
    virtual int32_t getPreferredHdrConversionType();
    virtual int32_t setHdrConversionPolicy(bool passthrough, int32_t forceType) = 0;
    // ALLM supported
    virtual int32_t setAutoLowLatencyMode(bool enabled) = 0;

    virtual void dump(String8 &dumpstr) = 0;
};
