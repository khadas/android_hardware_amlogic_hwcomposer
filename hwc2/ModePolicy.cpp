/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <hardware/hwcomposer2.h>
#include <cutils/properties.h>
#include <MesonLog.h>

#include "Dv.h"
#include "ModePolicy.h"
#include "mode_ubootenv.h"
#include "misc.h"
//#include <systemcontrol.h>
#include "HwcConfig.h"

#define DISPLAY_HDMI_VALID_MODE         "/sys/class/amhdmitx/amhdmitx0/valid_mode"//testing if tv support this displaymode and  deepcolor combination, then if cat result is 1: support, 0: not
#define PROP_DEFAULT_COLOR              "ro.vendor.platform.default_color"
#define LOW_POWER_DEFAULT_COLOR         "ro.vendor.low_power_default_color"  /* Set default deep color is 8bit for Low Power Mode */
#define UBOOTENV_COLORATTRIBUTE         "ubootenv.var.colorattribute"

#define EDID_MAX_SIZE                   2049
#define MODE_8K4K_PREFIX "4320p"

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

/* filter the interlace mode */
static const char* DISPLAY_MODE_LIST[] = {
    MODE_480P,
    MODE_480CVBS,
    MODE_576P,
    MODE_576CVBS,
    MODE_720P,
    MODE_720P50HZ,
    MODE_720P100HZ,
    MODE_720P120HZ,
    MODE_1080P24HZ,
    MODE_1080P25HZ,
    MODE_1080P30HZ,
    MODE_1080P50HZ,
    MODE_1080P,
    MODE_1080P_100HZ,
    MODE_1080P_120HZ,
    MODE_4K2K24HZ,
    MODE_4K2K25HZ,
    MODE_4K2K30HZ,
    MODE_4K2K50HZ,
    MODE_4K2K60HZ,
    MODE_4K2K100HZ,
    MODE_4K2K120HZ,
    MODE_4K2KSMPTE,
    MODE_4K2KSMPTE30HZ,
    MODE_4K2KSMPTE50HZ,
    MODE_4K2KSMPTE60HZ,
    MODE_4K2KSMPTE100HZ,
    MODE_4K2KSMPTE120HZ,
    MODE_768P,
    MODE_PANEL,
    MODE_PAL_M,
    MODE_PAL_N,
    MODE_NTSC_M,
};

static const char* DV_MODE_TYPE[] = {
    "DV_RGB_444_8BIT",
    "DV_YCbCr_422_12BIT",
    "LL_YCbCr_422_12BIT",
    "LL_RGB_444_12BIT",
    "LL_RGB_444_10BIT"
};

static const char* ALLM_MODE_CAP[] = {
    "0",
    "1",
};

static const char* ALLM_MODE[] = {
    "-1",
    "0",
    "1",
};

static const char* CONTENT_TYPE_CAP[] = {
    "graphics",
    "photo",
    "cinema",
    "game",
};

/*****************************************/
/*  deepcolor */

//this is check hdmi mode(4K60/4k50) list
static const char* COLOR_ATTRIBUTE_LIST[] = {
    COLOR_RGB_8BIT,
    COLOR_YCBCR444_8BIT,
    COLOR_YCBCR420_8BIT,
    COLOR_YCBCR422_8BIT,
};

//this is check hdmi mode list
static const char* COLOR_ATTRIBUTE_LIST_8BIT[] = {
    COLOR_RGB_8BIT,
    COLOR_YCBCR444_8BIT,
    COLOR_YCBCR422_8BIT,
};


//this is prior selected list  of 4k2k50hz, 4k2k60hz smpte50hz, smpte60hz
static const char* COLOR_ATTRIBUTE_LIST1[] = {
    COLOR_YCBCR420_10BIT,
    COLOR_YCBCR422_12BIT,
    COLOR_YCBCR420_8BIT,
    COLOR_YCBCR444_8BIT,
    COLOR_RGB_8BIT,
};

//this is prior selected list  of other display mode
static const char* COLOR_ATTRIBUTE_LIST2[] = {
    COLOR_YCBCR444_10BIT,
    COLOR_YCBCR422_12BIT,
    COLOR_RGB_10BIT,
    COLOR_YCBCR444_8BIT,
    COLOR_RGB_8BIT,
};

//this is prior selected list  of Low Power Mode 4k2k50hz, 4k2k60hz smpte50hz, smpte60hz
static const char* COLOR_ATTRIBUTE_LIST3[] = {
    COLOR_YCBCR420_8BIT,
    COLOR_YCBCR420_10BIT,
    COLOR_YCBCR422_8BIT,
    COLOR_YCBCR422_10BIT,
    COLOR_YCBCR444_8BIT,
    COLOR_RGB_8BIT,
    COLOR_YCBCR420_12BIT,
    COLOR_YCBCR422_12BIT,
};

//this is prior selected list of Low Power Mode other display mode
static const char* COLOR_ATTRIBUTE_LIST4[] = {
    COLOR_YCBCR444_8BIT,
    COLOR_YCBCR422_8BIT,
    COLOR_RGB_8BIT,
    COLOR_YCBCR444_10BIT,
    COLOR_YCBCR422_10BIT,
    COLOR_RGB_10BIT,
    COLOR_YCBCR444_12BIT,
    COLOR_YCBCR422_12BIT,
    COLOR_RGB_12BIT,
};


ModePolicy::ModePolicy() {
    mDisplayType = DISPLAY_TYPE_MBOX;
    mPolicy = MESON_POLICY_INVALID;
    mReason = OUTPUT_CHANGE_BY_INIT;
    mDisplayId = 0;
    memset(&mConnectorType, 0, sizeof(mConnectorType));
    memset(&mModeConType, 0, sizeof(mModeConType));
    memset(&mConData, 0, sizeof(mConData));
    memset(&mSceneOutInfo, 0, sizeof(mSceneOutInfo));
    memset(&mState, 0, sizeof(mState));
    memset(&mDvInfo, 0, sizeof(mDvInfo));
    mDisplayWidth = 0;
    mDisplayHeight = 0;
}

ModePolicy::ModePolicy(std::shared_ptr<meson::DisplayAdapter> adapter, const uint32_t displayId) {
    mAdapter = adapter;
    mDisplayType = DISPLAY_TYPE_MBOX;
    mPolicy = MESON_POLICY_INVALID;
    mReason = OUTPUT_CHANGE_BY_INIT;
    mDisplayId = displayId;
    memset(&mConnectorType, 0, sizeof(mConnectorType));
    memset(&mModeConType, 0, sizeof(mModeConType));
    memset(&mConData, 0, sizeof(mConData));
    memset(&mSceneOutInfo, 0, sizeof(mSceneOutInfo));
    memset(&mState, 0, sizeof(mState));
    memset(&mDvInfo, 0, sizeof(mDvInfo));
    mDisplayWidth = 0;
    mDisplayHeight = 0;
}

ModePolicy::~ModePolicy() {

}

bool ModePolicy::getBootEnv(const char *key, char *value) {
    const char* p_value =  meson_mode_get_ubootenv(key);
    MESON_LOGD("get key:%s value:%s", key, p_value);

    if (p_value) {
        strcpy(value, p_value);
        return true;
    }

    return false;
}

int ModePolicy::getBootenvInt(const char* key, int defaultVal) {
    int value = defaultVal;
    const char* p_value =  meson_mode_get_ubootenv(key);
    if (p_value) {
        value = atoi(p_value);
    }
    return value;
}

void ModePolicy::setBootEnv(const char* key, const char* value) {
    MESON_LOGD("set key:%s value:%s", key, value);

    meson_mode_set_ubootenv(key, value);
}


// TODO: need refactor
void ModePolicy::getDvCap(struct meson_hdr_info *data) {
    if (!data) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return;
    }

    std::string dv_cap;
    getDisplayAttribute(DISPLAY_DOLBY_VISION_CAP2, dv_cap);
    strcpy(data->dv_cap, dv_cap.c_str());

    if (strstr(data->dv_cap, "DolbyVision RX support list") != NULL) {
        for (int i = ARRAY_SIZE(DISPLAY_MODE_LIST) - 1; i >= 0; i--) {
            if (strstr(data->dv_cap, DISPLAY_MODE_LIST[i]) != NULL) {
                if ((strlen(data->dv_max_mode) + strlen(DISPLAY_MODE_LIST[i]) + 1) < sizeof(data->dv_max_mode)) {
                    strcat(data->dv_max_mode, DISPLAY_MODE_LIST[i]);
                    strcat(data->dv_max_mode, ",");
                } else {
                    MESON_LOGE("DisplayMode strcat overflow: src=%s, dst=%s\n", DISPLAY_MODE_LIST[i], data->dv_max_mode);
                }
                break;
            }
        }

        for (int i = 0; i < sizeof(DV_MODE_TYPE)/sizeof(DV_MODE_TYPE[0]); i++) {
            if (strstr(data->dv_cap, DV_MODE_TYPE[i])) {
                if ((strlen(data->dv_deepcolor) + strlen(DV_MODE_TYPE[i]) + 1) < sizeof(data->dv_deepcolor)) {
                    strcat(data->dv_deepcolor, DV_MODE_TYPE[i]);
                    strcat(data->dv_deepcolor, ",");
                } else {
                    MESON_LOGE("DisplayMode strcat overflow: src=%s, dst=%s\n", DV_MODE_TYPE[i], data->dv_deepcolor);
                    break;
                }
            }
        }
        MESON_LOGI("TV dv info: mode:%s deepcolor: %s\n", data->dv_max_mode, data->dv_deepcolor);
    } else {
        MESON_LOGE("TV isn't support dv: %s\n", data->dv_cap);
    }
}

void ModePolicy::getHdmiDcCap(char* dc_cap, int32_t len) {
    if (!dc_cap) {
        MESON_LOGE("%s dc_cap is NULL\n", __FUNCTION__);
        return;
    }

    int count = 0;
    while (true) {
        sysfs_get_string_original(DISPLAY_HDMI_DEEP_COLOR, dc_cap, len);
        if (strlen(dc_cap) > 0)
            break;

        if (count >= 5) {
            MESON_LOGE("read dc_cap fail\n");
            break;
        }
        count++;
        usleep(500000);
    }
}

int ModePolicy::getHdmiSinkType() {
    char sinkType[MESON_MODE_LEN] = {0};
    if (sysfs_get_string_ex(DISPLAY_HDMI_SINK_TYPE, sinkType, MESON_MODE_LEN, false) !=0)
        return HDMI_SINK_TYPE_NONE;

    if (NULL != strstr(sinkType, "sink")) {
        return HDMI_SINK_TYPE_SINK;
    } else if (NULL != strstr(sinkType, "repeater")) {
        return HDMI_SINK_TYPE_REPEATER;
    } else {
        return HDMI_SINK_TYPE_NONE;
    }
}

void ModePolicy::getHdmiEdidStatus(char* edidstatus, int32_t len) {
    if (!edidstatus) {
        MESON_LOGE("%s edidstatus is NULL\n", __FUNCTION__);
        return;
    }

    sysfs_get_string(DISPLAY_EDID_STATUS, edidstatus, len);
}

int32_t ModePolicy::setHdrStrategy(int32_t policy, const char *type) {
    SYS_LOGI("%s type:%s policy:%s", __func__,
            meson_hdrPolicyToString(policy), type);

    //1. update env policy
    std::string value = std::to_string(policy);
    setBootEnv(UBOOTENV_HDR_POLICY, value.c_str());

    if (policy == MESON_HDR_POLICY_SOURCE) {
        if (isDolbyVisionEnable()) {
            setBootEnv(UBOOTENV_DOLBYSTATUS, "0");
        }
    } else {
        char dvstatus[MESON_MODE_LEN] = {0};
        if (isDolbyVisionEnable()) {
            sprintf(dvstatus, "%d", mSceneOutInfo.dv_type);
            setBootEnv(UBOOTENV_DOLBYSTATUS, dvstatus);
        }
    }

    int32_t priority = MESON_HDR10_PRIORITY;
    value = std::to_string(MESON_HDR10_PRIORITY);
    if (strstr(type, DV_DISABLE_FORCE_SDR)) {
        priority = MESON_SDR_PRIORITY;
        value = std::to_string(MESON_SDR_PRIORITY);
    }

    setBootEnv(UBOOTENV_HDR_PRIORITY, value.c_str());

    //2. set current hdmi mode
    getHdrInfo(&mConData.hdr_info);
    meson_mode_set_policy_input(mModeConType, &mConData);
    getDisplayMode(mCurrentMode);

    if (!meson_mode_support_mode(mModeConType, priority, mCurrentMode)) {
        setSourceOutputMode(mCurrentMode);

        setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_FORCE);
        setDisplayAttribute(DISPLAY_FORCE_HDR_MODE, type);
    } else {
        MESON_LOGD("%s mode check failed", __func__);
        return -EINVAL;
    }

    return 0;
}

void ModePolicy::getHdrStrategy(char* value) {
    char hdr_policy[MESON_MODE_LEN] = {0};

    memset(hdr_policy, 0, MESON_MODE_LEN);
    getBootEnv(UBOOTENV_HDR_POLICY, hdr_policy);

    if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
        strcpy(value, HDR_POLICY_SOURCE);
    } else if (strstr(hdr_policy, HDR_POLICY_SINK)) {
        strcpy(value, HDR_POLICY_SINK);
    } else if (strstr(hdr_policy, HDR_POLICY_FORCE)) {
        strcpy(value, HDR_POLICY_FORCE);
    }
    MESON_LOGI("getHdrStrategy is [%s]", value);
}

int32_t ModePolicy::setHdrPriority(int32_t type) {
    MESON_LOGI("setHdrPriority is [%s]\n", meson_hdrPriorityToString(type));

    if (type == MESON_DOLBY_VISION_PRIORITY) {
        int32_t dvType = DOLBY_VISION_STD_ENABLE;
        if (mConnector->supportSinkLed()) {
            dvType = DOLBY_VISION_STD_ENABLE;
        } else {
            dvType = DOLBY_VISION_LL_YUV;
        }

        std::string value = std::to_string(dvType);
        strcpy(mConData.hdr_info.ubootenv_dv_type, value.c_str());
    }

    meson_mode_set_policy_input(mModeConType, &mConData);
    getHdrInfo(&mConData.hdr_info);
    getDisplayMode(mCurrentMode);
    if (!meson_mode_support_mode(mModeConType, type, mCurrentMode)) {
        std::string value = std::to_string(type);
        setBootEnv(UBOOTENV_HDR_PRIORITY, value.c_str());
        if (type == MESON_DOLBY_VISION_PRIORITY) {
            setBootEnv(UBOOTENV_DV_TYPE, mConData.hdr_info.ubootenv_dv_type);
        }

        setSourceOutputMode(mCurrentMode, true);

        if (type == MESON_DOLBY_VISION_PRIORITY) {
            setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_FORCE);
            //usleep(100000);//100ms
            setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_DV);
        } else if (type == MESON_HDR10_PRIORITY) {
            setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_FORCE);
            setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_HDR10);
        } else if (type == MESON_SDR_PRIORITY) {
            setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_FORCE);
            setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
            // 8bit or not
            std::string cur_ColorAttribute;
            getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, cur_ColorAttribute);
            if (cur_ColorAttribute.find("8bit", 0) != std::string::npos) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_ENABLE_FORCE_SDR_8BIT);
            } else {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_ENABLE_FORCE_SDR_10BIT);
            }
        }


    } else {
        MESON_LOGD("%s mode check failed", __func__);
        return -EINVAL;
    }

    return 0;
}

int32_t ModePolicy::getHdrPriority() {
    char hdr_priority[MESON_MODE_LEN] = {0};
    meson_hdr_priority_e value = MESON_DOLBY_VISION_PRIORITY;

    memset(hdr_priority, 0, MESON_MODE_LEN);
    getBootEnv(UBOOTENV_HDR_PRIORITY, hdr_priority);

    if (strstr(hdr_priority, "2")) {
        value = MESON_SDR_PRIORITY;
    } else if (strstr(hdr_priority, "1")) {
        value = MESON_HDR10_PRIORITY;
    } else {
        value = MESON_DOLBY_VISION_PRIORITY;
    }

    MESON_LOGI("getHdrPriority is [%s]", meson_hdrPriorityToString(value));
    return (int32_t)value;
}

bool ModePolicy::isTvDolbyVisionEnable() {
    bool ret = false;
    char dv_enable[MESON_MODE_LEN];
    ret = getBootEnv(UBOOTENV_DV_ENABLE, dv_enable);
    if (!ret) {
        if (isMboxSupportDolbyVision()) {
            strcpy(dv_enable, "1");
        } else {
            strcpy(dv_enable, "0");
        }
    }
    MESON_LOGI("dv_enable:%s\n", dv_enable);

    if (!strcmp(dv_enable, "0")) {
        return false;
    } else {
        return true;
    }
}

bool ModePolicy::isDolbyVisionEnable() {
    if (isMboxSupportDolbyVision()) {
        if (!strcmp(mDvInfo.dv_enable, "0") ) {
            return false;
        } else {
            return true;
        }
    } else {
        return false;
    }
}

/* *
 * @Description: Detect Whether TV support dv
 * @return: if TV support return true, or false
 * if true, mode is the Highest resolution Tv dv supported
 * else mode is ""
 */
bool ModePolicy::isTvSupportDolbyVision(char *mode) {
    strcpy(mode, "");
    if (DISPLAY_TYPE_TV == mDisplayType) {
        MESON_LOGI("Current Device is TV, no dv_cap\n");
        return false;
    }

    if (strstr(mConData.hdr_info.dv_cap, "DolbyVision RX support list") == NULL) {
        return false;
    }

    strcat(mode, mConData.hdr_info.dv_max_mode);
    strcat(mode, mConData.hdr_info.dv_deepcolor);

    MESON_LOGD("Current Tv Support DV type [%s]", mode);

    return true;
}

bool ModePolicy::isTvSupportHDR() {
    if (DISPLAY_TYPE_TV == mDisplayType) {
        MESON_LOGI("Current Device is TV, no hdr_cap\n");
        return false;
    }

    drm_hdr_capabilities hdrCaps;
    mConnector->getHdrCapabilities(&hdrCaps);

    if (hdrCaps.HLGSupported ||
            hdrCaps.HDR10Supported ||
            hdrCaps.HDR10PlusSupported) {
        return true;
    }

    return false;
}

bool ModePolicy::isHdrResolutionPriority() {
    return sys_get_bool_prop(PROP_HDR_RESOLUTION_PRIORITY, true);
}

bool ModePolicy::isFrameratePriority() {
    return sys_get_bool_prop(PROP_HDMI_FRAMERATE_PRIORITY, true);
}

bool ModePolicy::isSupport4K() {
    return sys_get_bool_prop(PROP_SUPPORT_4K, true);
}

bool ModePolicy::isSupport4K30Hz() {
    return sys_get_bool_prop(PROP_SUPPORT_OVER_4K30, true);
}

bool ModePolicy::isSupportDeepColor() {
    return sys_get_bool_prop(PROP_DEEPCOLOR, true);
}

bool ModePolicy::isLowPowerMode() {
    return sys_get_bool_prop(LOW_POWER_DEFAULT_COLOR, false);
}


bool ModePolicy::isMboxSupportDolbyVision() {
    return getDvSupportStatus();
}

void ModePolicy::getHdrInfo(meson_hdr_info_t *data) {
    char tvmode[MESON_MODE_LEN] = {0};
    if (!data) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return;
    }

    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);
    data->hdr_policy = (meson_hdr_policy_e)atoi(hdr_policy);

    data->hdr_priority = (meson_hdr_priority_e)getHdrPriority();

    MESON_LOGI("hdr_policy:%d, hdr_priority :%d\n",
            data->hdr_policy,
            data->hdr_priority);

    data->is_enable_dv = isDolbyVisionEnable();
    data->is_tv_supportDv = isTvSupportDolbyVision(tvmode);
    data->is_tv_supportHDR = isTvSupportHDR();
    data->is_hdr_resolution_priority = isHdrResolutionPriority();
    data->is_lowpower_mode = isLowPowerMode();

    char ubootenv_dv_type[MESON_MODE_LEN];
    bool ret = getBootEnv(UBOOTENV_DV_TYPE, ubootenv_dv_type);
    if (ret) {
        strcpy(data->ubootenv_dv_type, ubootenv_dv_type);
    } else if (isMboxSupportDolbyVision()) {
        strcpy(data->ubootenv_dv_type, "1");
    } else {
        strcpy(data->ubootenv_dv_type, "0");
    }
    MESON_LOGI("ubootenv_dv_type:%s\n", data->ubootenv_dv_type);

}

bool ModePolicy::initColorAttribute(char* supportedColorList, int len) {
    int count = 0;
    bool result = false;

    if (supportedColorList != NULL) {
        memset(supportedColorList, 0, len);
    } else {
        MESON_LOGE("supportedColorList is NULL\n");
        return false;
    }

    while (true) {
        sysfs_get_string(DISPLAY_HDMI_DEEP_COLOR, supportedColorList, len);
        if (strlen(supportedColorList) > 0) {
            result = true;
            break;
        }

        if (count++ >= 5) {
            break;
        }
        usleep(500000);
    }

    return result;
}

void ModePolicy::setFilterEdidList(std::map<int, std::string> filterEdidList) {
     MESON_LOGI("FormatColorDepth setFilterEdidList size = %" PRIuFAST16 "", filterEdidList.size());
     mFilterEdid = filterEdidList;
}

bool ModePolicy::isFilterEdid() {
    if (mFilterEdid.empty())
        return false;

    char disPlayEdid[EDID_MAX_SIZE] = {0};
    sysfs_get_string(DISPLAY_EDID_RAW, disPlayEdid, EDID_MAX_SIZE);

    for (int i = 0; i < (int)mFilterEdid.size(); i++) {
        if (!strncmp(mFilterEdid[i].c_str(), disPlayEdid, strlen(mFilterEdid[i].c_str()))) {
            return true;
        }
    }
    return false;
}


bool ModePolicy::isModeSupportDeepColorAttr(const char *mode, const char * color) {
    char outputmode[MESON_MODE_LEN] = {0};

    strcpy(outputmode, mode);
    strcat(outputmode, color);

    if (isFilterEdid() && !strstr(color,"8bit")) {
        MESON_LOGI("this mode has been filtered");
        return false;
    }

    //try support or not
    return sys_set_valid_mode(DISPLAY_HDMI_VALID_MODE, outputmode);
}


bool ModePolicy::isSupportHdmiMode(const char *hdmi_mode, const char *supportedColorList) {
    int length = 0;
    const char **colorList = NULL;

    if (strstr(hdmi_mode, "2160p60hz")  != NULL
        || strstr(hdmi_mode,"2160p50hz") != NULL
        || strstr(hdmi_mode,"smpte50hz") != NULL
        || strstr(hdmi_mode,"smpte60hz") != NULL) {

        colorList = COLOR_ATTRIBUTE_LIST;
        length    = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST);

        for (int i = 0; i < length; i++) {
            if (strstr(supportedColorList, colorList[i]) != NULL) {
                if (isModeSupportDeepColorAttr(hdmi_mode, colorList[i])) {
                    return true;
                }
            }
        }
        return false;
    } else {
        colorList = COLOR_ATTRIBUTE_LIST_8BIT;
        length    = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST_8BIT);

        for (int j = 0; j < length; j++) {
            if (strstr(supportedColorList, colorList[j]) != NULL) {
                if (isModeSupportDeepColorAttr(hdmi_mode, colorList[j])) {
                    return true;
                }
            }
        }
        return false;
    }
}

// color deep
void ModePolicy::getBestHdmiDeepColorAttr(const char *outputmode, char* colorAttribute) {
    char *pos = NULL;
    int length = 0;
    const char **colorList = NULL;
    char supportedColorList[MESON_MAX_STR_LEN];
    if (!initColorAttribute(supportedColorList, MESON_MAX_STR_LEN)) {
        MESON_LOGE("initColorAttribute fail\n");
        return;
    }

    //filter some color value options, aimed at some modes.
    if (!strcmp(outputmode, MODE_4K2K60HZ) || !strcmp(outputmode, MODE_4K2K50HZ)
        || !strcmp(outputmode, MODE_4K2KSMPTE60HZ) || !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
        if (sys_get_bool_prop(LOW_POWER_DEFAULT_COLOR, false)) {
            colorList = COLOR_ATTRIBUTE_LIST3;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST3);
        } else {
            colorList = COLOR_ATTRIBUTE_LIST1;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST1);
        }
    } else {
        if (sys_get_bool_prop(LOW_POWER_DEFAULT_COLOR, false)) {
            colorList = COLOR_ATTRIBUTE_LIST4;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST4);
        } else {
            colorList = COLOR_ATTRIBUTE_LIST2;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST2);
        }
    }

    for (int i = 0; i < length; i++) {
        if ((pos = strstr(supportedColorList, colorList[i])) != NULL) {
            if (isModeSupportDeepColorAttr(outputmode, colorList[i])) {
                MESON_LOGI("support current mode:[%s], deep color:[%s]\n", outputmode, colorList[i]);

                strcpy(colorAttribute, colorList[i]);
                break;
            }
        }
    }
}

void ModePolicy::getHdmiColorAttribute(const char* outputmode, char* colorAttribute, int state) {
    char supportedColorList[MESON_MAX_STR_LEN];

    //if read /sys/class/amhdmitx/amhdmitx0/dc_cap is null. return
    if (!initColorAttribute(supportedColorList, MESON_MAX_STR_LEN)) {

        char defVal[64];
        if (!strcmp(outputmode, MODE_4K2K60HZ) || !strcmp(outputmode, MODE_4K2K50HZ)
            || !strcmp(outputmode, MODE_4K2KSMPTE60HZ) || !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
            strcpy(defVal, COLOR_YCBCR420_8BIT);
        } else {
            strcpy(defVal, COLOR_YCBCR444_8BIT);
        }
        sys_get_string_prop_default(PROP_DEFAULT_COLOR, colorAttribute, defVal);

        MESON_LOGE("Error!!! Do not find sink color list, use default color attribute:%s\n", colorAttribute);
        return;
    }

    if (sys_get_bool_prop(PROP_HDMIONLY, true)) {
        char curMode[MESON_MODE_LEN]    = {0};
        char isBestMode[MESON_MODE_LEN] = {0};
        char colorTemp[MESON_MODE_LEN]  = {0};

        getDisplayMode(curMode);

        // if only change deepcolor in Droid Settings
        // get color format from ubootenv, it was set in Droid Settings
        if ((state == OUTPUT_MODE_STATE_SWITCH) && (!strcmp(curMode, outputmode))
            && getBootEnv(UBOOTENV_ISBESTMODE, isBestMode) && (strcmp(isBestMode, "false") == 0)) {
            //note: "outputmode" should be the second parameter of "strcmp", because it maybe prefix of "curMode".
            MESON_LOGI("Only modify deep color mode, get colorAttr from ubootenv.var.colorattribute\n");
            getBootEnv(UBOOTENV_COLORATTRIBUTE, colorTemp);
            if (isModeSupportDeepColorAttr(curMode,colorTemp)) {
                getBootEnv(UBOOTENV_COLORATTRIBUTE, colorAttribute);
            }
        } else {
            getProperHdmiColorAttribute(outputmode,  colorAttribute);
        }
    }

    MESON_LOGI("get hdmi color attribute : [%s], outputmode is: [%s] , and support color list is: [%s]\n",
        colorAttribute, outputmode, supportedColorList);
}

void ModePolicy::getProperHdmiColorAttribute(const char* outputmode, char* colorAttribute) {
    char ubootvar[MESON_MODE_LEN] = {0};
    char tmpValue[MESON_MODE_LEN] = {0};
    char isBestMode[MESON_MODE_LEN] = {0};

    // if auto switch best mode is off, get priority color value of mode in ubootenv,
    // and judge current mode whether this colorValue is supported in This TV device.
    // if not support or auto switch best mode is on, select color value from Lists in next step.
    if (getBootEnv(UBOOTENV_ISBESTMODE, isBestMode) && (strcmp(isBestMode, "false") == 0)) {
        MESON_LOGI("get color attr from ubootenv.var.%s_deepcolor When is not best mode\n", outputmode);
        sprintf(ubootvar, "ubootenv.var.%s_deepcolor", outputmode);
        if (getBootEnv(ubootvar, tmpValue) && strstr(tmpValue, "bit")
            && isModeSupportDeepColorAttr(outputmode, tmpValue)) {
            strcpy(colorAttribute, tmpValue);
            return;
        }
    }

    getBestHdmiDeepColorAttr(outputmode, colorAttribute);

    //if colorAttr is null above steps, will defines a initial value to it
    if (!strstr(colorAttribute, "bit")) {
        strcpy(colorAttribute, COLOR_YCBCR444_8BIT);
    }
}

void ModePolicy::filterHdmiDispcap(meson_connector_info* data) {
    char supportedColorList[MESON_MAX_STR_LEN];

    if (!(initColorAttribute(supportedColorList, MESON_MAX_STR_LEN))) {
        MESON_LOGE("initColorAttribute fail\n");
        return;
    }

    meson_mode_info_t *modes_ptr = data->modes;
    for (int i = 0; i < data->modes_size; i++) {
        meson_mode_info_t *it = &modes_ptr[i];
        MESON_LOGD("before filtered Hdmi support: %s\n", it->name);
        if (isSupportHdmiMode(it->name, supportedColorList)) {
            MESON_LOGD("after filtered Hdmi support mode : %s\n", it->name);
        }
    }

}

int32_t ModePolicy::getConnectorData(struct meson_policy_in* data, hdmi_dv_info_t *dinfo) {
    if (!data) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return -EINVAL;
    }

    getCommonData(data, dinfo);

    //get hdmi dv_info
    getDvCap(&data->hdr_info);

    // get Hdr info
    getHdrInfo(&data->hdr_info);

    //hdmi info
    getHdmiEdidStatus(data->con_info.edid_parsing, MESON_MODE_LEN);

    //three sink types: sink, repeater, none
    data->con_info.sink_type = (meson_sink_type_e)getHdmiSinkType();
    MESON_LOGI("display sink type:%d [0:none, 1:sink, 2:repeater]\n", data->con_info.sink_type);

    if (HDMI_SINK_TYPE_NONE != data->con_info.sink_type) {
        getSupportedModes();

        //read hdmi dc_cap
        char dc_cap[MESON_MAX_STR_LEN];
        getHdmiDcCap(dc_cap, MESON_MAX_STR_LEN);
        strcpy(data->con_info.dc_cap, dc_cap);

        //getBootEnv(UBOOTENV_HDMIMODE, data->con_info.ubootenv_hdmimode);
        getBootEnv(UBOOTENV_CVBSMODE, data->con_info.ubootenv_cvbsmode);
        MESON_LOGI("ubootenv  cvbsmode:%s\n",
                data->con_info.ubootenv_cvbsmode);

        std::string curColorAttribute;
        //getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, curColorAttribute);
        getBootEnv(UBOOTENV_COLORATTRIBUTE, data->con_info.ubootenv_colorattr);
        MESON_LOGI("ubootenv_colorattribute:%s\n", data->con_info.ubootenv_colorattr);
    }

    getDisplayMode(data->cur_displaymode);

#if 0
    //filter hdmi disp_cap mode for compatibility
    filterHdmiDispcap(&data->con_info);
#endif

    data->con_info.is_support4k = isSupport4K();
    data->con_info.is_support4k30HZ = isSupport4K30Hz();
    data->con_info.is_deepcolor = isSupportDeepColor();

    return 0;
}

bool ModePolicy::setPolicy(int32_t policy) {
    MESON_LOGD("setPolicy to %d", policy);
    switch (policy) {
        case static_cast<int>(MESON_POLICY_BEST):
        case static_cast<int>(MESON_POLICY_RESOLUTION):
        case static_cast<int>(MESON_POLICY_FRAMERATE):
        case static_cast<int>(MESON_POLICY_DOLBY_VISION):
            mPolicy = static_cast<meson_mode_policy>(policy);
            break;
        default:
            MESON_LOGE("Set invalid policy:%d", policy);
            return false;
    }

    return true;
}

//TODO::  refactor to a thread to handle hotplug
void ModePolicy::onHotplug(bool connected) {
    MESON_LOGD("ModePolicy handle hotplug:%d", connected);
    //plugout or suspend,set dummy_l
    if (!connected) {
        std::string displayMode("dummy_l");
        if (isVMXCertification()) {
            displayMode = "576cvbs";
        }

        setDisplayMode(displayMode);
        return;
    }

    //hdmi edid parse error and hpd = 1
    //set default reolsution and color format
    if (isHdmiEdidParseOK() == false) {
        setDefaultMode();
        return;
    }

    //hdmi edid parse ok
    setSourceDisplay(OUTPUT_MODE_STATE_POWER);
}

void ModePolicy::setActiveConfig(std::string mode) {
    mReason = OUTPUT_CHANGE_BY_HWC;
    MESON_LOGI("setDisplayed by hwc %s", mode.c_str());
    setSourceOutputMode(mode.c_str());
    mReason = OUTPUT_CHANGE_BY_INIT;
}

int32_t ModePolicy::getPreferredBootConfig(std::string &config) {
    if (DISPLAY_TYPE_TV == mDisplayType) {
        char curMode[MESON_MODE_LEN] = {0};
        getDisplayMode(curMode);
        config = curMode;
    } else {
#if 0
        //1. get hdmi data
        hdmi_data_t data;

        memset(&data, 0, sizeof(hdmi_data_t));
        getHdmiData(&data);
#endif
        mConData.state = static_cast<meson_mode_state>(OUTPUT_MODE_STATE_INIT);
        mConData.con_info.is_bestcolorspace = true;

        //2. scene logic process
        meson_mode_set_policy(mModeConType, MESON_POLICY_BEST);
        meson_mode_set_policy_input(mModeConType, &mConData);
        meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

        config = mSceneOutInfo.displaymode;
    }

    MESON_LOGI("getPreferredDisplayConfig [%s]", config.c_str());
    return 0;
}

int32_t ModePolicy::setBootConfig(std::string &config) {
    MESON_LOGI("set boot display config to %s\n", config.c_str());
    setBootEnv(UBOOTENV_ISBESTMODE, "false");
    setBootEnv(UBOOTENV_HDMIMODE, config.c_str());
    mPolicy = MESON_POLICY_INVALID;

    return 0;
}

int32_t ModePolicy::clearBootConfig() {
    MESON_LOGI("clear boot display \n");
    setBootEnv(UBOOTENV_ISBESTMODE, "true");
    mPolicy = MESON_POLICY_BEST;
    // after clear boot config, need save the bestMode to uboot env
    mConData.state = static_cast<meson_mode_state>(OUTPUT_MODE_STATE_INIT);
    mConData.con_info.is_bestcolorspace = true;

    //2. scene logic process
    meson_mode_set_policy(mModeConType, MESON_POLICY_BEST);
    meson_mode_set_policy_input(mModeConType, &mConData);
    meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

    //save color space and hdmi resolution to env
    setBootEnv(UBOOTENV_COLORATTRIBUTE, mSceneOutInfo.deepcolor);
    setBootEnv(UBOOTENV_HDMIMODE, mSceneOutInfo.displaymode);
    //need to keep the same value with the defaul value
    setBootEnv(UBOOTENV_FRAC_RATE_POLICY, "1");
    //need enable color space best policy
    setBootEnv(UBOOTENV_BESTCOLORSPACE, "true");

    return 0;
}

void ModePolicy::dump(String8 &dumpstr) {
    dumpstr.append("ModePolicy support modes:\n");
    dumpstr.append("-----------------------------------------------------------"
        "------------------\n");
    dumpstr.append("|  CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   NAME      |\n");
    dumpstr.append("+-----------+------------------+-----------+------------+"
        "----------------+\n");

    auto conPtr = &mConData.con_info;
    for (int i = 0; i < conPtr->modes_size; i++) {
        auto config = conPtr->modes[i];
        dumpstr.appendFormat(" %2d     |      %.3f      |   %5d   |   %5d    |"
            " %14s |\n",
            i,
            config.refresh_rate,
            config.pixel_w,
            config.pixel_h,
            config.name);
    }
    dumpstr.append("-----------------------------------------------------------"
        "---------------\n");
}

int32_t ModePolicy::bindConnector(std::shared_ptr<HwDisplayConnector> & connector) {
    MESON_ASSERT(connector.get(), "ModePolicy bindConnector get null connector!!");

    mConnector = connector;
    switch (mConnector->getType()) {
        case DRM_MODE_CONNECTOR_HDMIA:
            mDisplayType = DISPLAY_TYPE_MBOX;
            mConnectorType = ConnectorType::CONN_TYPE_HDMI;
            mModeConType = MESON_MODE_HDMI;
            break;
        case LEGACY_NON_DRM_CONNECTOR_PANEL:
        case DRM_MODE_CONNECTOR_LVDS:
        case DRM_MODE_CONNECTOR_MESON_LVDS_A:
        case DRM_MODE_CONNECTOR_MESON_LVDS_B:
        case DRM_MODE_CONNECTOR_MESON_LVDS_C:
        case DRM_MODE_CONNECTOR_MESON_VBYONE_A:
        case DRM_MODE_CONNECTOR_MESON_VBYONE_B:
        case DRM_MODE_CONNECTOR_MESON_MIPI_A:
        case DRM_MODE_CONNECTOR_MESON_MIPI_B:
        case DRM_MODE_CONNECTOR_MESON_EDP_A:
        case DRM_MODE_CONNECTOR_MESON_EDP_B:
            mDisplayType = DISPLAY_TYPE_TV;
            mConnectorType = ConnectorType::CONN_TYPE_PANEL;
            mModeConType = MESON_MODE_PANEL;
            break;
        case DRM_MODE_CONNECTOR_TV:
            mDisplayType = DISPLAY_TYPE_MBOX;
            mConnectorType = ConnectorType::CONN_TYPE_CVBS;
            mModeConType = MESON_MODE_HDMI;
            break;
        default:
            MESON_LOGE("bindConnector unknown connector type:%d", mConnector->getType());
            mDisplayType = DISPLAY_TYPE_MBOX;
            mConnectorType = ConnectorType::CONN_TYPE_HDMI;
            mModeConType = MESON_MODE_HDMI;
            break;
    }

    return 0;
}

/* *
 * @Description: init amdolby vision graphics priority when bootup.
 * */
void ModePolicy::initGraphicsPriority() {
    char mode[MESON_MODE_LEN] = {0};
    char defVal[MESON_MODE_LEN] = {"1"};

    sys_get_string_prop_default(PROP_DOLBY_VISION_PRIORITY, mode, defVal);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_GRAPHICS_PRIORITY, mode);
    sys_set_prop(PROP_DOLBY_VISION_PRIORITY, mode);
}


/* *
 * @Description: set hdr mode
 * @params: mode "0":off "1":on "2":auto
 * */
void ModePolicy::setHdrMode(const char* mode) {
    if ((atoi(mode) >= 0) && (atoi(mode) <= 2)) {
        MESON_LOGI("setHdrMode state: %s\n", mode);
        setDisplayAttribute(DISPLAY_HDR_MODE, mode);
        sys_set_prop(PROP_HDR_MODE_STATE, mode);
    }
}

/* *
 * @Description: set sdr mode
 * @params: mode "0":off "2":auto
 * */
void ModePolicy::setSdrMode(const char* mode) {
    if ((atoi(mode) == 0) || atoi(mode) == 2) {
        MESON_LOGI("setSdrMode state: %s\n", mode);
        setDisplayAttribute(DISPLAY_SDR_MODE, mode);
        sys_set_prop(PROP_SDR_MODE_STATE, mode);
        setBootEnv(UBOOTENV_SDR2HDR, (char *)mode);
    }
}


void ModePolicy::initHdrSdrMode() {
    char mode[MESON_MODE_LEN] = {0};
    char defVal[MESON_MODE_LEN] = {HDR_MODE_AUTO};
    sys_get_string_prop_default(PROP_HDR_MODE_STATE, mode, defVal);
    setHdrMode(mode);
    memset(mode, 0, sizeof(mode));
    bool flag = sys_get_bool_prop(PROP_ENABLE_SDR2HDR, false);
    if (flag & isDolbyVisionEnable()) {
        strcpy(mode, SDR_MODE_OFF);
    } else {
        strcpy(defVal, flag ? SDR_MODE_AUTO : SDR_MODE_OFF);
        sys_get_string_prop_default(PROP_SDR_MODE_STATE, mode, defVal);
    }
    setSdrMode(mode);
}

void ModePolicy::initDolbyVision(output_mode_state state) {
    int dv_type = DOLBY_VISION_SET_DISABLE;

    MESON_LOGI("state:%d\n", state);

    dv_type = updateDolbyVisionType();

    if (getCurDolbyVisionState(dv_type,  state)) {
        MESON_LOGI("Current DV type is same as the set value\n");
        return;
    }

    setDolbyVisionEnable(dv_type,  state);
}

int ModePolicy::updateDolbyVisionType(void) {
    char type[MESON_MODE_LEN];

    //1. read DV mode from prop(maybe need to env)
    strcpy(type, mConData.hdr_info.ubootenv_dv_type);
    MESON_LOGI("type %s tv DV mode:%s\n", type, mConData.hdr_info.dv_deepcolor);

    //2. check tv support or not
    if ((strstr(type, "1") != NULL) && strstr(mConData.hdr_info.dv_deepcolor, "DV_RGB_444_8BIT") != NULL) {
        return DOLBY_VISION_SET_ENABLE;
    } else if ((strstr(type, "2") != NULL) && strstr(mConData.hdr_info.dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL) {
        return DOLBY_VISION_SET_ENABLE_LL_YUV;
    } else if ((strstr(type, "3") != NULL)
        && ((strstr(mConData.hdr_info.dv_deepcolor, "LL_RGB_444_12BIT") != NULL) ||
        (strstr(mConData.hdr_info.dv_deepcolor, "LL_RGB_444_10BIT") != NULL))) {
        return DOLBY_VISION_SET_ENABLE_LL_RGB;
    } else if (strstr(type, "0") != NULL) {
        return DOLBY_VISION_SET_DISABLE;
    }

    //3. DV best policy:STD->LL_YUV->LL_RGB for netflix request
    //   DV best policy:LL_YUV->STD->LL_RGB for DV request
    if ((strstr(mConData.hdr_info.dv_deepcolor, "DV_RGB_444_8BIT") != NULL) ||
        (strstr(mConData.hdr_info.dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL)) {
        if (sys_get_bool_prop(PROP_ALWAYS_DOLBY_VISION, false)) {
            if (strstr(mConData.hdr_info.dv_deepcolor, "DV_RGB_444_8BIT") != NULL) {
                return DOLBY_VISION_SET_ENABLE;
            } else if (strstr(mConData.hdr_info.dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL) {
                return DOLBY_VISION_SET_ENABLE_LL_YUV;
            }
        } else {
            if (strstr(mConData.hdr_info.dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL) {
                return DOLBY_VISION_SET_ENABLE_LL_YUV;
            } else if (strstr(mConData.hdr_info.dv_deepcolor, "DV_RGB_444_8BIT") != NULL) {
                return DOLBY_VISION_SET_ENABLE;
            }
        }
    } else if ((strstr(mConData.hdr_info.dv_deepcolor, "LL_RGB_444_12BIT") != NULL) ||
        (strstr(mConData.hdr_info.dv_deepcolor, "LL_RGB_444_10BIT") != NULL)) {
        return DOLBY_VISION_SET_ENABLE_LL_RGB;
    }

    return DOLBY_VISION_SET_DISABLE;
}

bool ModePolicy::getCurDolbyVisionState(int state, output_mode_state mode_state) {
    if ((mode_state != OUTPUT_MODE_STATE_INIT)
            || checkDolbyVisionStatusChanged(state)
            || checkDolbyVisionDeepColorChanged(state)) {
        return false;
    }
    return true;
}

bool ModePolicy::checkDolbyVisionDeepColorChanged(int state) {
    std::string colorAttr;
    char mode[MESON_MAX_STR_LEN] = {0};
    getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, colorAttr);
    if (isTvSupportDolbyVision(mode) && (state == DOLBY_VISION_SET_ENABLE)
            && (strstr(colorAttr.c_str(), "444,8bit") == NULL)) {
        MESON_LOGI("colorAttr %s is not match with DV STD\n", colorAttr.c_str());
        return true;
    } else if ((state == DOLBY_VISION_SET_ENABLE_LL_YUV) && (strstr(colorAttr.c_str(), "422,12bit") == NULL)) {
        MESON_LOGI("colorAttr %s is not match with DV LL YUV\n", colorAttr.c_str());
        return true;
    } else if ((state == DOLBY_VISION_SET_ENABLE_LL_RGB) && (strstr(colorAttr.c_str(), "444,12bit") == NULL)
                && (strstr(colorAttr.c_str(), "444,10bit") == NULL)) {
        MESON_LOGI("colorAttr %s is not match with DV LL RGB\n", colorAttr.c_str());
        return true;
    }
    return false;
}


bool ModePolicy::checkDolbyVisionStatusChanged(int state) {
    std::string curDvEnable = "";
    std::string curDvLLPolicy = "";
    int curDvMode = -1;

    getDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, curDvEnable);
    getDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, curDvLLPolicy);

    if (!strcmp(curDvEnable.c_str(), DV_DISABLE) ||
        !strcmp(curDvEnable.c_str(), "0"))
        curDvMode = DOLBY_VISION_SET_DISABLE;
    else if (!strcmp(curDvLLPolicy.c_str(), "0"))
        curDvMode = DOLBY_VISION_SET_ENABLE;
    else if (!strcmp(curDvLLPolicy.c_str(), "1"))
        curDvMode = DOLBY_VISION_SET_ENABLE_LL_YUV;
    else if (!strcmp(curDvLLPolicy.c_str(), "2"))
        curDvMode = DOLBY_VISION_SET_ENABLE_LL_RGB;

    MESON_LOGI("curDvMode %d, want DvMode %d\n", curDvMode, state);

    if (curDvMode != state) {
        return true;
    } else {
        return false;
    }
}

//get edid crc value to check edid change
bool ModePolicy::isEdidChange() {
    char edid[MESON_MAX_STR_LEN] = {0};
    char crcvalue[MESON_MAX_STR_LEN] = {0};
    unsigned int crcheadlength = strlen(DEFAULT_EDID_CRCHEAD);
    sysfs_get_string(DISPLAY_EDID_VALUE, edid, MESON_MAX_STR_LEN);
    char *p = strstr(edid, DEFAULT_EDID_CRCHEAD);
    if (p != NULL && strlen(p) > crcheadlength) {
        p += crcheadlength;
        if (!getBootEnv(UBOOTENV_EDIDCRCVALUE, crcvalue) || strncmp(p, crcvalue, strlen(p))) {
            MESON_LOGI("update edidcrc: %s->%s\n", crcvalue, p);
            setBootEnv(UBOOTENV_EDIDCRCVALUE, p);
            return true;
        }
    }
    return false;
}

void ModePolicy::saveDeepColorAttr(const char* mode, const char* dcValue) {
    char ubootvar[100] = {0};
    sprintf(ubootvar, "ubootenv.var.%s_deepcolor", mode);
    setBootEnv(ubootvar, (char *)dcValue);
}

void ModePolicy::saveHdmiParamToEnv() {
    char outputMode[MESON_MODE_LEN] = {0};

    getDisplayMode(outputMode);

    // 1. check whether the TV changed or not, if changed save crc
    if (isEdidChange()) {
        MESON_LOGD("tv sink changed\n");
    }

    // 2. save coloattr/hdmimode to bootenv if mode is not null or dummy_l
    if (strstr(outputMode, "cvbs") != NULL) {
        setBootEnv(UBOOTENV_CVBSMODE, (char *)outputMode);
    } else if (strcmp(outputMode, "null") && strcmp(outputMode, "dummy_l")) {
        std::string colorAttr;
        char colorDepth[MESON_MODE_LEN] = {0};
        char colorSpace[MESON_MODE_LEN] = {0};
        char dvstatus[MESON_MODE_LEN]   = {0};
        char dv_type[MESON_MODE_LEN]    = {0};
        char hdr_policy[MESON_MODE_LEN] = {0};
        // 2.1 save color attr
        getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, colorAttr);
        saveDeepColorAttr(outputMode, colorAttr.c_str());
        setBootEnv(UBOOTENV_COLORATTRIBUTE, colorAttr.c_str());
        //colorDepth&&colorSpace is used for uboot hdmi to find
        //best color attributes for the selected hdmi mode when TV changed
        char defVal[MESON_MODE_LEN] = {"8"};
        sys_get_string_prop_default(PROP_DEEPCOLOR_CTL, colorDepth, defVal);
        strcpy(defVal, "auto");
        sys_get_string_prop_default(PROP_PIXFMT, colorSpace, defVal);
        setBootEnv(UBOOTENV_HDMICOLORDEPTH, colorDepth);
        setBootEnv(UBOOTENV_HDMICOLORSPACE, colorSpace);

        // 2.2 save output mode
        if (DISPLAY_TYPE_TABLET != mDisplayType) {
            setBootEnv(UBOOTENV_OUTPUTMODE, (char *)outputMode);
        }

        if (strstr(outputMode, "hz") != NULL) {
            setBootEnv(UBOOTENV_HDMIMODE, (char *)outputMode);
        }

        // 2.3 save amdolby status/dv_type
        // In follow sink mode: 0:disable 1:STD(or enable dv) 2:LL YUV 3: LL RGB
        // In follow source mode: dv is disable  in uboot.
        if (isMboxSupportDolbyVision()) {
            getHdrStrategy(hdr_policy);
            if (!strcmp(hdr_policy, HDR_POLICY_SOURCE)) {
                sprintf(dvstatus, "%d", 0);
            } else {
                sprintf(dvstatus, "%d", mSceneOutInfo.dv_type);
            }
            setBootEnv(UBOOTENV_DOLBYSTATUS, dvstatus);

            sprintf(dv_type, "%d", mSceneOutInfo.dv_type);
            setBootEnv(UBOOTENV_DV_TYPE, dv_type);

            setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);

            MESON_LOGI("dvstatus %s dv_type %s dv_enable %s\n",
                dvstatus, dv_type, mDvInfo.dv_enable);

        } else {
            MESON_LOGI("MBOX is not support dv, dvstatus %s dv_type %d dv_enable %s\n",
                dvstatus, mSceneOutInfo.dv_type, mDvInfo.dv_enable);
        }

        MESON_LOGI("colorattr: %s, outputMode %s, cd %s, cs %s\n",
            colorAttr.c_str(), outputMode, colorDepth, colorSpace);
    }
}


void ModePolicy::enableDolbyVision(int DvMode) {
    char tvmode[MESON_MAX_STR_LEN] = {0};

    if (isMboxSupportDolbyVision() == false) {
        MESON_LOGI("This platform is not support dv or has no dv ko");
        return;
    }
    MESON_LOGI("DvMode %d", DvMode);

    strcpy(mDvInfo.dv_enable, "1");

    //if TV
    if (DISPLAY_TYPE_TV == mDisplayType) {
        setHdrMode(HDR_MODE_OFF);
        setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FOLLOW_SINK);
    }

    //if OTT
    if ((DISPLAY_TYPE_MBOX == mDisplayType) || (DISPLAY_TYPE_REPEATER == mDisplayType)) {
        if (isTvSupportDolbyVision(tvmode) && (mConData.hdr_info.hdr_priority == MESON_DOLBY_VISION_PRIORITY)) {
            MESON_LOGI("Tv is Support dv, tvmode is [%s]", tvmode);

            switch (DvMode) {
                case DOLBY_VISION_SET_ENABLE:
                    MESON_LOGI("Dv set Mode [DV_RGB_444_8BIT]\n");
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "0");
                    break;
                case DOLBY_VISION_SET_ENABLE_LL_YUV:
                    MESON_LOGI("Dv set Mode [LL_YCbCr_422_12BIT]\n");
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "0");
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "1");
                    break;
                case DOLBY_VISION_SET_ENABLE_LL_RGB:
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "0");
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "2");
                    break;
                default:
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, "0");
            }
        }

        char hdr_policy[MESON_MODE_LEN] = {0};
        getHdrStrategy(hdr_policy);
        if (strstr(hdr_policy, HDR_POLICY_SINK)) {
            setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);
            if (isDolbyVisionEnable()) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SINK);
            }
        } else if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
            setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
            if (isDolbyVisionEnable()) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SOURCE);
            }
        }

        //setDisplayAttribute(DISPLAY_DOLBY_VISION_HDR_10_POLICY, DV_HDR_SINK_PROCESS);
    }

    usleep(100000);//100ms
    setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_MODE_IPT_TUNNEL);
    usleep(100000);//100ms

    if (DISPLAY_TYPE_TV == mDisplayType) {
        setHdrMode(HDR_MODE_AUTO);
    }

    initGraphicsPriority();
}

void ModePolicy::disableDolbyVision(int DvMode) {
    //char tvmode[MESON_MODE_LEN]   = {0};
    int  check_status_count = 0;
    [[maybe_unused]] int dv_type = DvMode;

    MESON_LOGI("dv_type %d", dv_type);
    strcpy(mDvInfo.dv_enable, "0");

    //2. update sysfs
    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);

    if (strstr(hdr_policy, HDR_POLICY_SINK)) {
        setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);
    } else if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
        setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
    }

    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FORCE_MODE);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_MODE_BYPASS);
    usleep(100000);//100ms
    std::string dvstatus = "";
    getDisplayAttribute(DISPLAY_DOLBY_VISION_STATUS, dvstatus);

    if (strcmp(dvstatus.c_str(), BYPASS_PROCESS)) {
        while (++check_status_count <30) {
            usleep(20000);//20ms
            getDisplayAttribute(DISPLAY_DOLBY_VISION_STATUS, dvstatus);
            if (!strcmp(dvstatus.c_str(), BYPASS_PROCESS)) {
                break;
            }
        }
    }

    MESON_LOGI("dvstatus %s, check_status_count [%d]", dvstatus.c_str(), check_status_count);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_DISABLE);

    if (DISPLAY_TYPE_TV == mDisplayType) {
        setHdrMode(HDR_MODE_AUTO);
    }

    setSdrMode(SDR_MODE_AUTO);
}

void ModePolicy::getPosition(const char* curMode, int *position) {
    char keyValue[20] = {0};
    char ubootvar[100] = {0};
    int defaultWidth = 0;
    int defaultHeight = 0;
    std::map<uint32_t, drm_mode_info_t> connecterModeList;

    drm_mode_info_t mode = {
           DRM_DISPLAY_MODE_NULL,
           0, 0,
           0, 0,
           60.0,
           0
        };

    if (mConnector->isConnected()) {
        mConnector->getModes(connecterModeList);

        for (auto it = connecterModeList.begin(); it != connecterModeList.end(); it++) {
            if (mConnector->getType() == DRM_MODE_CONNECTOR_TV) {
                if (strstr(curMode, it->second.name)) {
                    strcpy(keyValue, curMode);
                    mode = it->second;
                    break;
                }
            } else {
                if (strstr(curMode, it->second.name)) {
                    if (strstr(it->second.name, MODE_4K2KSMPTE_PREFIX)) {
                        strcpy(keyValue, "4k2ksmpte");
                    } else if (strstr(it->second.name, MODE_PANEL)) {
                        strcpy(keyValue, MODE_PANEL);
                    } else if (strchr(curMode,'p')) {
                        strncpy(keyValue, curMode, strchr(curMode,'p') - curMode + 1);
                    } else if (strchr(curMode,'i')){
                        strncpy(keyValue, curMode, strchr(curMode,'i') - curMode + 1);
                    }
                    mode = it->second;
                    break;
                }
            }
        }
    }

    if (keyValue[0] != '0') {
        defaultWidth = mode.pixelW;
        defaultHeight = mode.pixelH;
    } else {
        strcpy(keyValue, MODE_1080P_PREFIX);
        defaultWidth = FULL_WIDTH_1080;
        defaultHeight = FULL_HEIGHT_1080;
    }

    pthread_mutex_lock(&mEnvLock);
    sprintf(ubootvar, "ubootenv.var.%s_x", keyValue);
    position[0] = getBootenvInt(ubootvar, 0);
    sprintf(ubootvar, "ubootenv.var.%s_y", keyValue);
    position[1] = getBootenvInt(ubootvar, 0);
    sprintf(ubootvar, "ubootenv.var.%s_w", keyValue);
    position[2] = getBootenvInt(ubootvar, defaultWidth);
    sprintf(ubootvar, "ubootenv.var.%s_h", keyValue);
    position[3] = getBootenvInt(ubootvar, defaultHeight);

    MESON_LOGI("%s curMode:%s position[0]:%d position[1]:%d position[2]:%d position[3]:%d\n", __FUNCTION__, curMode, position[0], position[1], position[2], position[3]);

    pthread_mutex_unlock(&mEnvLock);

}

void ModePolicy::setPosition(const char* curMode, int left, int top, int width, int height) {
    char x[512] = {0};
    char y[512] = {0};
    char w[512] = {0};
    char h[512] = {0};
    sprintf(x, "%d", left);
    sprintf(y, "%d", top);
    sprintf(w, "%d", width);
    sprintf(h, "%d", height);

    MESON_LOGI("%s curMode:%s left:%d top:%d width:%d height:%d\n", __FUNCTION__, curMode, left, top, width, height);

    char keyValue[20] = {0};
    char ubootvar[100] = {0};
    std::map<uint32_t, drm_mode_info_t> connecterModeList;

    if (mConnector->isConnected()) {
        mConnector->getModes(connecterModeList);

        for (auto it = connecterModeList.begin(); it != connecterModeList.end(); it++) {
            if (mConnector->getType() == DRM_MODE_CONNECTOR_TV) {
                if (strstr(curMode, it->second.name)) {
                    strcpy(keyValue, curMode);
                    break;
                }
            } else {
                if (strstr(curMode, it->second.name)) {
                     if (strstr(it->second.name, MODE_4K2KSMPTE_PREFIX)) {
                        strcpy(keyValue, "4k2ksmpte");
                    } else if (strstr(it->second.name, MODE_PANEL)) {
                        strcpy(keyValue, MODE_PANEL);
                    } else if (strchr(curMode,'p')) {
                        strncpy(keyValue, curMode, strchr(curMode,'p') - curMode + 1);
                    } else if (strchr(curMode,'i')){
                        strncpy(keyValue, curMode, strchr(curMode,'i') - curMode + 1);
                    }
                    break;
                }
            }
        }
    }

    pthread_mutex_lock(&mEnvLock);
    if (mReason != OUTPUT_CHANGE_BY_HWC) {
        sprintf(ubootvar, "ubootenv.var.%s_x", keyValue);
        setBootEnv(ubootvar, x);
        sprintf(ubootvar, "ubootenv.var.%s_y", keyValue);
        setBootEnv(ubootvar, y);
        sprintf(ubootvar, "ubootenv.var.%s_w", keyValue);
        setBootEnv(ubootvar, w);
        sprintf(ubootvar, "ubootenv.var.%s_h", keyValue);
        setBootEnv(ubootvar, h);
    }
    pthread_mutex_unlock(&mEnvLock);
    mAdapter->setDisplayRect({left, top, width , height}, mConnectorType);

}

void ModePolicy::setDigitalMode(const char* mode) {
    if (mode == NULL) return;

    if (!strcmp("PCM", mode)) {
        sysfs_set_string(AUDIO_DSP_DIGITAL_RAW, "0");
        sysfs_set_string(AV_HDMI_CONFIG, "audio_on");
    } else if (!strcmp("SPDIF passthrough", mode))  {
        sysfs_set_string(AUDIO_DSP_DIGITAL_RAW, "1");
        sysfs_set_string(AV_HDMI_CONFIG, "audio_on");
    } else if (!strcmp("HDMI passthrough", mode)) {
        sysfs_set_string(AUDIO_DSP_DIGITAL_RAW, "2");
        sysfs_set_string(AV_HDMI_CONFIG, "audio_on");
    }
}

bool ModePolicy::getDisplayMode(char* mode) {
    bool ret = false;

    if (mode != NULL) {
        MESON_ASSERT(mAdapter.get(), "modePolicy has no display adapter");
        std::string curMode = "null";
        ret = mAdapter->getDisplayMode(curMode, mConnectorType);
        strcpy(mode, curMode.c_str());
        MESON_LOGI("%s mode:%s\n", __FUNCTION__, mode);
    } else {
        MESON_LOGE("%s mode is NULL\n", __FUNCTION__);
    }

    return ret;
}

//set hdmi output mode
int32_t ModePolicy::setDisplayMode(std::string &mode) {
    MESON_LOGI("%s mode:%s\n", __FUNCTION__, mode.c_str());
    mAdapter->setDisplayMode(mode, mConnectorType);
    return 0;
}

int32_t ModePolicy::setDisplayMode(const char *mode) {
    if (!mode) {
        MESON_LOGE("ModePolicy::setDisplayMode null mode");
        return -EINVAL;
    }
    std::string displayMode(mode);
    return setDisplayMode(displayMode);
}

bool ModePolicy::setDisplayAttribute(const std::string cmd, const std::string attribute) {
    return mAdapter->setDisplayAttribute(cmd, attribute, mConnectorType);
}
bool ModePolicy::getDisplayAttribute(const std::string cmd, std::string& attribute) {
    return mAdapter->getDisplayAttribute(cmd, attribute, mConnectorType);
}

bool ModePolicy::isMatchMode(char* curmode, const char* outputmode) {
    bool ret = false;
    char tmpMode[MESON_MODE_LEN] = {0};

    char *pCmp = curmode;
    //check line feed key
    char *pos = strchr(pCmp, 0x0a);
    if (NULL == pos) {
        //check return key
        char *pos = strchr(pCmp, 0x0d);
        if (NULL == pos) {
            strcpy(tmpMode, pCmp);
        } else {
            strncpy(tmpMode, pCmp, pos - pCmp);
        }
    } else {
        strncpy(tmpMode, pCmp, pos - pCmp);
    }

    MESON_LOGI("curmode:%s, tmpMode:%s, outputmode:%s\n", curmode, tmpMode, outputmode);

    if (!strcmp(tmpMode, outputmode)) {
        ret = true;
    }

    return ret;
}

bool ModePolicy::isTvSupportALLM() {
    char allm_mode_cap[PROP_VALUE_MAX];
    memset(allm_mode_cap, 0, PROP_VALUE_MAX);
    int ret = 0;

    sysfs_get_string(AUTO_LOW_LATENCY_MODE_CAP, allm_mode_cap, PROP_VALUE_MAX);

    for (int i = 0; i < ARRAY_SIZE(ALLM_MODE_CAP); i++) {
        if (!strncmp(allm_mode_cap, ALLM_MODE_CAP[i], strlen(ALLM_MODE_CAP[i]))) {
            ret = i;
        }
    }

    return (ret == 1) ? true : false;
}

bool ModePolicy::getContentTypeSupport(const char* type) {
    char content_type_cap[MESON_MAX_STR_LEN] = {0};
    sysfs_get_string(HDMI_CONTENT_TYPE_CAP, content_type_cap, MESON_MAX_STR_LEN);
    if (strstr(content_type_cap, type)) {
        MESON_LOGI("getContentTypeSupport: %s is true", type);
        return true;
    }

    MESON_LOGI("getContentTypeSupport: %s is false", type);
    return false;
}

bool ModePolicy::getGameContentTypeSupport() {
    return getContentTypeSupport(CONTENT_TYPE_CAP[3]);
}

bool ModePolicy::getSupportALLMContentTypeList(std::vector<std::string> *supportModes) {
    if (isTvSupportALLM()) {
        (*supportModes).push_back(std::string("allm"));
    }

    for (int i = 0; i < ARRAY_SIZE(CONTENT_TYPE_CAP); i++) {
        if (getContentTypeSupport(CONTENT_TYPE_CAP[i])) {
            (*supportModes).push_back(std::string(CONTENT_TYPE_CAP[i]));
        }
    }

    return true;
}


void ModePolicy::setTvDolbyVisionEnable() {
    //if TV
    setHdrMode(HDR_MODE_OFF);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FOLLOW_SINK);

    usleep(100000);//100ms
    setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_MODE_IPT_TUNNEL);
    usleep(100000);//100ms

    setHdrMode(HDR_MODE_AUTO);

    initGraphicsPriority();
}

void ModePolicy::setTvDolbyVisionDisable() {
    int  check_status_count = 0;

    //2. update sysfs
    setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);

    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FORCE_MODE);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_MODE_BYPASS);
    usleep(100000);//100ms
    std::string dvstatus = "";
    getDisplayAttribute(DISPLAY_DOLBY_VISION_STATUS, dvstatus);

    if (strcmp(dvstatus.c_str(), BYPASS_PROCESS)) {
        while (++check_status_count <30) {
            usleep(20000);//20ms
            getDisplayAttribute(DISPLAY_DOLBY_VISION_STATUS, dvstatus);
            if (!strcmp(dvstatus.c_str(), BYPASS_PROCESS)) {
                break;
            }
        }
    }

    MESON_LOGI("dvstatus %s, check_status_count [%d]", dvstatus.c_str(), check_status_count);
    setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_DISABLE);

    setHdrMode(HDR_MODE_AUTO);
    setSdrMode(SDR_MODE_AUTO);
}


void ModePolicy::setDolbyVisionEnable(int state,  output_mode_state mode_state __unused) {
    MESON_LOGI("%s dv mode:%d", __FUNCTION__, state);

    if (DISPLAY_TYPE_TV == mDisplayType) {
        //1. update prop
        char tmp[10];
        sprintf(tmp, "%d", state);
        strcpy(mConData.hdr_info.ubootenv_dv_type, tmp);

        if (state == DOLBY_VISION_SET_DISABLE) {
            strcpy(mDvInfo.dv_enable, "0");
        } else {
            strcpy(mDvInfo.dv_enable, "1");
        }

        if (state == DOLBY_VISION_SET_DISABLE) {
            setTvDolbyVisionDisable();
        } else {
            setTvDolbyVisionEnable();
        }

        //save env
        setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);
    } else {
        //1. update dv env
        char tmp[10];
        char hdr_policy[MESON_MODE_LEN] = {0};
        char dvstatus[MESON_MODE_LEN]   = {0};

        sprintf(tmp, "%d", state);
        strcpy(mConData.hdr_info.ubootenv_dv_type, tmp);

        if (state == DOLBY_VISION_SET_DISABLE) {
            strcpy(mDvInfo.dv_enable, "0");
        } else {
            strcpy(mDvInfo.dv_enable, "1");
        }

        setBootEnv(UBOOTENV_DV_TYPE, mConData.hdr_info.ubootenv_dv_type);
        setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);

        getHdrStrategy(hdr_policy);
        if (!strcmp(hdr_policy, HDR_POLICY_SOURCE)) {
            sprintf(dvstatus, "%d", 0);
        } else {
            sprintf(dvstatus, "%d", state);
        }
        setBootEnv(UBOOTENV_DOLBYSTATUS, dvstatus);

        //2. get final display mode and color format
        setBootEnv(UBOOTENV_ISBESTMODE, "false");

        mConData.state = static_cast<meson_mode_state>(OUTPUT_MODE_STATE_INIT);

        //2. scene logic process
        getConnectorData(&mConData, &mDvInfo);

        meson_mode_set_policy(mModeConType, MESON_POLICY_BEST);
        meson_mode_set_policy_input(mModeConType, &mConData);
        meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

        // 3. save uboot env
        //3.1 save hdmimode
        if (strstr(mSceneOutInfo.displaymode, "cvbs") != NULL) {
            setBootEnv(UBOOTENV_CVBSMODE, mSceneOutInfo.displaymode);
        } else if (strstr(mSceneOutInfo.displaymode, "hz") != NULL) {
            setBootEnv(UBOOTENV_HDMIMODE, mSceneOutInfo.displaymode);
        }
        //3.2 save colorattribute
        saveDeepColorAttr(mSceneOutInfo.displaymode, mSceneOutInfo.deepcolor);
        setBootEnv(UBOOTENV_COLORATTRIBUTE, mSceneOutInfo.deepcolor);
    }
}


/* *
 * @Description: this is a temporary solution, should be revert when android.hardware.graphics.composer@2.4 finished
 *               set the ALLM_Mode
 * @params: "0": ALLM disable (VSIF still contain allm info)
 *          "1": ALLM enable
 *          "-1":really disable ALLM (VSIF don't contain allm info)
 * */
void ModePolicy::setALLMMode(int state) {
    /***************************************************************
     *         Comment for special solution in this func           *
     ***************************************************************
     *                                                             *
     * In HDMI Standard only 0 to disable ALLM and 1 to enable ALLM*
     * but ALLM and amDolby Vision share the same bit in VSIF        *
     * it cause conflict                                           *
     *                                                             *
     * So in amlogic special solution:                             *
     * we add -1 to                                                *
     *     1: disable ALLM                                         *
     *     2: clean ALLM info in VSIF conflict bit                 *
     * when user set 0 to ALLM                                     *
     * we will force change 0 into -1 here                         *
     *                                                             *
     ***************************************************************/

    char buf[PROPERTY_VALUE_MAX] = {0};
    char defVal[PROPERTY_VALUE_MAX] = "";
    int len =sys_get_string_prop_default("persist.vendor.sys.display.allm", buf, defVal);
    int perState = -1;
    char* end;

    if (len > 0) {
        perState = strtol(buf, &end, 0);
        if (end == buf) {
            perState = -1;
        }
    }


    if (perState == state) {
        MESON_LOGI("setALLMMode: the ALLM_Mode is not changed");
        return;
    }

    char dv_mode[MESON_MAX_STR_LEN];
    bool isTVSupportDV = isTvSupportDolbyVision(dv_mode);

    switch (state) {
        case -1:
            [[fallthrough]];
        case 0:
            sysfs_set_string(AUTO_LOW_LATENCY_MODE, ALLM_MODE[0]);
            sys_set_prop("persist.vendor.sys.display.allm", ALLM_MODE[0]);
            MESON_LOGI("setALLMMode: ALLM_Mode: %s", ALLM_MODE[0]);

            if (isTVSupportDV) {
                // reset doblyvision when set -1/0 to ALLM
                setBootEnv(UBOOTENV_BESTDOLBYVISION, "true");
                initDolbyVision(OUTPUT_MODE_STATE_SWITCH);
                setDolbyVisionEnable(DOLBY_VISION_SET_ENABLE,OUTPUT_MODE_STATE_SWITCH);
            }
            break;
        case 1:
            sysfs_set_string(AUTO_LOW_LATENCY_MODE, ALLM_MODE[2]);
            sys_set_prop("persist.vendor.sys.display.allm", ALLM_MODE[2]);
            MESON_LOGI("setALLMMode: ALLM_Mode: %s", ALLM_MODE[2]);

            if (isTVSupportDV && isDolbyVisionEnable()) {
                // disable the doblyvision when ALLM enable
               // setBootEnv(UBOOTENV_BESTDOLBYVISION, "false");
                setDolbyVisionEnable(DOLBY_VISION_SET_DISABLE,OUTPUT_MODE_STATE_SWITCH);
            }
            break;
        default:
            MESON_LOGE("setALLMMode: ALLM_Mode: error state[%d]", state);
            break;
    }
}

/* *
 * @Description: get Current State DolbyVision State
 *
 * @result: if disable amDolby Vision return DOLBY_VISION_SET_DISABLE.
 *          if Current TV support the state saved in Mbox. return that state. like value of saved is 2, and TV support LL_YUV
 *          if Current TV not support the state saved in Mbox. but isDolbyVisionEnable() is enable.
 *             return state in priority queue. like value of saved is 2, But TV only Support LL_RGB, so system will return LL_RGB
 */
int ModePolicy::getDolbyVisionType() {
    int dv_type;
    char dv_mode[MESON_MAX_STR_LEN];

    if (isTvSupportDolbyVision(dv_mode) && (mConData.hdr_info.hdr_priority == MESON_DOLBY_VISION_PRIORITY)) {
        //1. read dv mode from prop(maybe need to env)
        dv_type = mSceneOutInfo.dv_type;
        MESON_LOGI("dv_type %d tv dv mode:%s\n", dv_type, dv_mode);

        //2. check tv support or not
        if ((dv_type == 1) && strstr(dv_mode, "DV_RGB_444_8BIT") != NULL) {
            return DOLBY_VISION_SET_ENABLE;
        } else if ((dv_type == 2) && strstr(dv_mode, "LL_YCbCr_422_12BIT") != NULL) {
            return DOLBY_VISION_SET_ENABLE_LL_YUV;
        } else if ((dv_type == 3)
                && ((strstr(dv_mode, "LL_RGB_444_12BIT") != NULL) || (strstr(dv_mode, "LL_RGB_444_10BIT") != NULL))) {
            return DOLBY_VISION_SET_ENABLE_LL_RGB;
        } else if (dv_type == 0) {
            return DOLBY_VISION_SET_DISABLE;
        }

        //3. amdolby vision best policy:STD->LL_YUV->LL_RGB for netflix request
        //   amdolby vision best policy:LL_YUV->STD->LL_RGB for amdolby vision request
        if ((strstr(dv_mode, "DV_RGB_444_8BIT") != NULL) || (strstr(dv_mode, "LL_YCbCr_422_12BIT") != NULL)) {
            if (sys_get_bool_prop(PROP_ALWAYS_DOLBY_VISION, false)) {
                if (strstr(dv_mode, "DV_RGB_444_8BIT") != NULL) {
                    return DOLBY_VISION_SET_ENABLE;
                } else if (strstr(dv_mode, "LL_YCbCr_422_12BIT") != NULL) {
                    return DOLBY_VISION_SET_ENABLE_LL_YUV;
                }
            } else {
                if (strstr(dv_mode, "LL_YCbCr_422_12BIT") != NULL) {
                    return DOLBY_VISION_SET_ENABLE_LL_YUV;
                } else if (strstr(dv_mode, "DV_RGB_444_8BIT") != NULL) {
                    return DOLBY_VISION_SET_ENABLE;
                }
            }
        } else if ((strstr(dv_mode, "LL_RGB_444_12BIT") != NULL) || (strstr(dv_mode, "LL_RGB_444_10BIT") != NULL)) {
            return DOLBY_VISION_SET_ENABLE_LL_RGB;
        }
    } else {
        // enable amdolby vision core
        if (isDolbyVisionEnable()) {
            return DOLBY_VISION_SET_ENABLE;
        }
    }

    return DOLBY_VISION_SET_DISABLE;
}

int32_t ModePolicy::setAutoLowLatencyMode(bool enabled) {
    if (mConnector->getType() != DRM_MODE_CONNECTOR_HDMIA) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    if (isTvSupportALLM()) {
        setALLMMode(enabled);
        return HWC2_ERROR_NONE;
    } else {
        return HWC2_ERROR_UNSUPPORTED;
    }
}

int32_t ModePolicy::setHdrConversionPolicy(bool passthrough, int32_t forceType) {
    int32_t ret = 0;
    MESON_LOGD("%s passthrough %d forceType %s",
            __func__, passthrough, hdrConversionTypeToString(forceType));

    drm_hdr_capabilities_t hdrCaps;
    mConnector->getHdrCapabilities(&hdrCaps);

    if (passthrough) {
        /*follow source*/
       if (!hdrCaps.DolbyVisionSupported) {
           setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
           setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SINK);
       } else {
           setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SOURCE);
           if (mConnector->supportSinkLed()) {
               setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, DV_SINK_LED);
           } else if (mConnector->supportSourceLed()) {
               setDisplayAttribute(DISPLAY_DOLBY_VISION_LL_POLICY, DV_SOURCE_LED);
           }
       }
        setBootEnv(UBOOTENV_HDR_POLICY, HDR_POLICY_SOURCE);
    } else {
        /*force mode need set attr before switch policy */
        switch (forceType) {
            case DRM_DOLBY_VISION: {
                ret = setHdrPriority(MESON_DOLBY_VISION_PRIORITY);
                break;
            }
            case DRM_HDR10: {
                if (!isMboxSupportDolbyVision()) {
                    ret = setHdrStrategy(MESON_HDR_POLICY_FORCE, FORCE_HDR10);
                } else {
                    ret = setHdrPriority(MESON_HDR10_PRIORITY);
                }

                break;
            }
            case DRM_HLG: {
                ret = setHdrStrategy(MESON_HDR_POLICY_FORCE, FORCE_HLG);
                break;
            }
            case DRM_INVALID: {
                if (!isMboxSupportDolbyVision()) {
                    ret = setHdrStrategy(MESON_HDR_POLICY_FORCE, DV_DISABLE_FORCE_SDR);
                } else {
                    ret = setHdrPriority(MESON_SDR_PRIORITY);
                }
                break;
            }
            default:
                MESON_LOGE("setHdrConversionStrategy: error type[%d]", forceType);
                ret = HWC2_ERROR_UNSUPPORTED;
                break;
        }
    }

    return ret;
}

/*
* apply setting
*/
void ModePolicy::applyDisplaySetting(bool force) {
    //quiescent boot need not output
    bool quiescent = sys_get_bool_prop("ro.boot.quiescent", false);

    MESON_LOGI("quiescent_mode is %d\n", quiescent);
    if (quiescent  && (mState == OUTPUT_MODE_STATE_INIT)) {
        MESON_LOGI("don't need to setting hdmi when quiescent mode\n");
        return;
    }

    //check cvbs mode
    bool cvbsMode = false;

    if (!strcmp(mSceneOutInfo.displaymode, MODE_480CVBS) || !strcmp(mSceneOutInfo.displaymode, MODE_576CVBS)
        || !strcmp(mSceneOutInfo.displaymode, MODE_PAL_M) || !strcmp(mSceneOutInfo.displaymode, MODE_PAL_N)
        || !strcmp(mSceneOutInfo.displaymode, MODE_NTSC_M)
        || !strcmp(mSceneOutInfo.displaymode, "null") || !strcmp(mSceneOutInfo.displaymode, "dummy_l")
        || !strcmp(mSceneOutInfo.displaymode, MODE_PANEL)) {
        cvbsMode = true;
    }

    /* not enable phy in systemcontrol by default
     * as phy will be enabled in driver when set mode
     * only enable phy if phy is disabled but not enabled
     */
    bool phy_enabled_already = true;

    // 1. update hdmi frac_rate_policy
    char frac_rate_policy[MESON_MODE_LEN]     = {0};
    char cur_frac_rate_policy[MESON_MODE_LEN] = {0};
    bool frac_rate_policy_change        = false;

    if (mReason != OUTPUT_CHANGE_BY_HWC) {
        sysfs_get_string(HDMI_TX_FRAMERATE_POLICY, cur_frac_rate_policy, MESON_MODE_LEN);
        getBootEnv(UBOOTENV_FRAC_RATE_POLICY, frac_rate_policy);
        if (strstr(frac_rate_policy, cur_frac_rate_policy) == NULL) {
            sysfs_set_string(HDMI_TX_FRAMERATE_POLICY, frac_rate_policy);
            frac_rate_policy_change = true;
        }  else {
            MESON_LOGI("cur frac_rate_policy is equals\n");
        }
    } else {
         sysfs_get_string(HDMI_TX_FRAMERATE_POLICY, cur_frac_rate_policy, MESON_MODE_LEN);
         char defVal[] = "2";
         sys_get_string_prop_default(HDMI_FRC_POLICY_PROP,frac_rate_policy, defVal);
         if (strstr(frac_rate_policy,"2")) {
             getBootEnv(UBOOTENV_FRAC_RATE_POLICY, frac_rate_policy);
         }
         MESON_LOGI("get frc policy from hwc is %s and current value is %s\n",frac_rate_policy, cur_frac_rate_policy);
          if (strstr(frac_rate_policy, cur_frac_rate_policy) == NULL) {
             sysfs_set_string(HDMI_TX_FRAMERATE_POLICY, frac_rate_policy);

               frac_rate_policy_change = true;
         }
    }

    // 2. set hdmi final color space
    char curColorAttribute[MESON_MODE_LEN] = {0};
    char final_deepcolor[MESON_MODE_LEN]   = {0};
    bool attr_change                 = false;

    std::string cur_ColorAttribute;
    getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, cur_ColorAttribute);
    strcpy(curColorAttribute, cur_ColorAttribute.c_str());
    strcpy(final_deepcolor, mSceneOutInfo.deepcolor);
    MESON_LOGI("curDeepcolor[%s] final_deepcolor[%s]\n", curColorAttribute, final_deepcolor);

    if (strstr(curColorAttribute, final_deepcolor) == NULL) {
        MESON_LOGI("set color space from:%s to %s\n", curColorAttribute, final_deepcolor);
        setDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, final_deepcolor);
        attr_change = true;
    } else {
        MESON_LOGI("cur deepcolor is equals\n");
    }

    // 3. update hdr strategy
    bool hdr_policy_change = false;
    std::string cur_hdr_policy;
    //bool hdr_priority_change = false;
    getDisplayAttribute(DISPLAY_HDR_POLICY, cur_hdr_policy);
    MESON_LOGI("cur hdr policy:%s\n", cur_hdr_policy.c_str());

    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);

    if (strstr(cur_hdr_policy.c_str(), hdr_policy) == NULL) {
        MESON_LOGI("set hdr policy from:%s to %s\n", cur_hdr_policy.c_str(), hdr_policy);
        //hdr_policy_change = true;
    }

    if (!cvbsMode && (isMboxSupportDolbyVision() == false)) {
        if (sys_get_bool_prop(PROP_DOLBY_VISION_FEATURE, false)) {
            if (strstr(hdr_policy, HDR_POLICY_SINK)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);
            } else if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
            } else if (strstr(hdr_policy, HDR_POLICY_FORCE)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
            }
        } else {
            initHdrSdrMode();
        }
    }

    // 4. check amdolby vision
    int  dv_type  = DOLBY_VISION_SET_DISABLE;
    bool dv_change  = false;

    dv_type   = mSceneOutInfo.dv_type;
    dv_change = checkDolbyVisionStatusChanged(dv_type);
    if (isMboxSupportDolbyVision() && dv_change) {
        //4.1 set avmute when signal change at boot
        if ((OUTPUT_MODE_STATE_INIT == mState)
            && (strstr(hdr_policy, HDR_POLICY_SINK))) {
            sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
        }
        //4.2 set dummy_l mode when dv change at UI switch
        if ((OUTPUT_MODE_STATE_SWITCH == mState) && dv_change) {
            setDisplayMode("dummy_l");
        }
        //4.3 enable or disable amdolby vision core
        if (DOLBY_VISION_SET_DISABLE != dv_type) {
            enableDolbyVision(dv_type);
        } else {
            disableDolbyVision(dv_type);
        }

        MESON_LOGI("isDolbyVisionEnable [%d] dv type:%d", isDolbyVisionEnable(), getDolbyVisionType());
    } else {
        MESON_LOGI("cur DvMode is equals\n");
    }

    // 5. check hdmi output mode
    char final_displaymode[MESON_MODE_LEN] = {0};
    char curDisplayMode[MESON_MODE_LEN]    = {0};
    bool modeChange                  = false;

    getDisplayMode(curDisplayMode);
    strcpy(final_displaymode, mSceneOutInfo.displaymode);
    MESON_LOGI("curMode:[%s] ,final_displaymode[%s]\n", curDisplayMode, final_displaymode);

    if (!isMatchMode(curDisplayMode, final_displaymode)) {
        modeChange = true;
    } else {
        MESON_LOGI("cur mode is equals\n");
    }

    //6. check any change
    bool isNeedChange = false;

    if (modeChange || attr_change || frac_rate_policy_change || hdr_policy_change) {
        isNeedChange = true;
    } else if (force) {
        isNeedChange = true;
        MESON_LOGD("force changed");
    } else {
        MESON_LOGI("nothing need to be changed\n");
    }

    // 7. stop hdcp
    if (isNeedChange) {
        sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
        if (OUTPUT_MODE_STATE_POWER != mState) {
            if (usleep(100000) < 0)//100ms
                MESON_LOGE("usleep interrupt!\n");
            sysfs_set_string(DISPLAY_HDMI_HDCP_MODE, "-1");
            //usleep(100000);//100ms
            sysfs_set_string(DISPLAY_HDMI_PHY, "0"); /* Turn off TMDS PHY */
            phy_enabled_already = false;
            if (usleep(50000) < 0)//50ms
                MESON_LOGE("usleep interrupt!\n");
        }
        // stop hdcp tx
        mTxAuth->stop();
    } else if (OUTPUT_MODE_STATE_INIT == mState) {
        // stop hdcp tx
        mTxAuth->stop();
        char fail_case[PROPERTY_VALUE_MAX] = {0};
        char defVal[] = "4";
        sys_get_string_prop_default(HDCP_TX_AUTH_FAIL, fail_case, defVal);
        if (!strcmp(fail_case, "1")) {
            sysfs_set_string(DISPLAY_HDMI_VIDEO_MUTE, "1");
            sysfs_set_string(DISPLAY_HDMI_AUDIO_MUTE, "1");
        } else if (!strcmp(fail_case, "2")) {
            sysfs_set_string(DISPLAY_HDMI_AUDIO_MUTE, "1");
            sysfs_set_string(DISPLAY_MEDIA_VIDEO_MUTE, "1");
        }
    }

    // 8. set hdmi final output mode
    if (isNeedChange) {
        //need drive to do
        if (strstr(final_displaymode, MODE_8K4K_PREFIX)) {
            sysfs_set_string(DISPLAY_HDMI_FRL_RATE, "4");
        } else {
            sysfs_set_string(DISPLAY_HDMI_FRL_RATE, "0");
        }

        //apply hdr policy to driver sysfs
        if (hdr_policy_change) {
            if (strstr(hdr_policy, HDR_POLICY_SINK)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);
                if (isDolbyVisionEnable()) {
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SINK);
                }
            } else if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
                if (isDolbyVisionEnable()) {
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, HDR_POLICY_SOURCE);
                }
            }
        }

#if 0
        //apply hdr priority to driver sysfs
        if (hdr_priority_change) {
            setDisplayAttribute(DISPLAY_HDR_PRIORITY, HDR_PRIORITY_TYPE[hdr_priority]);
        }
#endif

        //set hdmi mode
        setDisplayMode(final_displaymode);
        /* phy already turned on after write display/mode node */
        phy_enabled_already     = true;
    } else {
        MESON_LOGI("curDisplayMode is equal  final_displaymode, Do not need set it\n");
    }

    // graphic
    char final_Mode[MESON_MODE_LEN] = {0};
    getDisplayMode(final_Mode);
    char defVal[] = "0x0";
    if (sys_get_bool_prop(PROP_DISPLAY_SIZE_CHECK, true)) {
        char resolution[MESON_MODE_LEN] = {0};
        char defaultResolution[MESON_MODE_LEN] = {0};
        char finalResolution[MESON_MODE_LEN] = {0};
        int w = 0, h = 0, w1 =0, h1 = 0;
        sysfs_get_string(SYS_DISPLAY_RESOLUTION, resolution, MESON_MODE_LEN);
        sys_get_string_prop_default(PROP_DISPLAY_SIZE, defaultResolution, defVal);
        sscanf(resolution, "%dx%d", &w, &h);
        sscanf(defaultResolution, "%dx%d", &w1, &h1);
        if ((w != w1) || (h != h1)) {
            if (strstr(final_displaymode, "null") && w1 != 0) {
                sprintf(finalResolution, "%dx%d", w1, h1);
            } else {
                sprintf(finalResolution, "%dx%d", w, h);
            }
            sys_set_prop(PROP_DISPLAY_SIZE, finalResolution);
        }
    }
    sys_set_prop(PROP_DISPLAY_ALLM, isTvSupportALLM() ? "1" : "0");
    sys_set_prop(PROP_DISPLAY_GAME, getGameContentTypeSupport() ? "1" : "0");

    char defaultResolution[MESON_MODE_LEN] = {0};
    sys_get_string_prop_default(PROP_DISPLAY_SIZE, defaultResolution, defVal);
    MESON_LOGI("set display-size:%s\n", defaultResolution);

    int position[4] = { 0, 0, 0, 0 };//x,y,w,h
    getPosition(final_displaymode, position);
    setPosition(final_displaymode, position[0], position[1],position[2], position[3]);

    // 9. turn on phy and clear avmute
    if (isNeedChange) {
        /*if (!phy_enabled_already) {
            sysfs_set_string(DISPLAY_HDMI_PHY, "1"); // Turn on TMDS PHY
        }*/
        usleep(20000);
        sysfs_set_string(DISPLAY_HDMI_AUDIO_MUTE, "1");
        sysfs_set_string(DISPLAY_HDMI_AUDIO_MUTE, "0");
        if (isDolbyVisionEnable()) {
            usleep(20000);
        }
    }

    //must clear avmute for new policy(driver maybe set mute)
    sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "-1");

    // 10. start HDMI HDCP authenticate
    if (isNeedChange) {
        if (!cvbsMode) {
            mTxAuth->start();
        }
    } else if (OUTPUT_MODE_STATE_INIT == mState) {
        if (!cvbsMode) {
            mTxAuth->start();
        }
    }

    //audio
    char value[MESON_MAX_STR_LEN] = {0};
    memset(value, 0, sizeof(0));
    getBootEnv(UBOOTENV_DIGITAUDIO, value);
    setDigitalMode(value);

    if ((mState == OUTPUT_MODE_STATE_INIT) ||
         (mState == OUTPUT_MODE_STATE_POWER)) {
        saveHdmiParamToEnv();
    }
}

int32_t ModePolicy::initialize() {
    uint32_t width = 1280;
    uint32_t height = 1080;
    HwcConfig::getFramebufferSize(mDisplayId, width, height);
    mDefaultUI = to_string(height);

    if (DISPLAY_TYPE_MBOX == mDisplayType) {
        mTxAuth = std::make_shared<HDCPTxAuth>();

        setSourceDisplay(OUTPUT_MODE_STATE_INIT);
    } else if (DISPLAY_TYPE_TV == mDisplayType) {
        setSinkDisplay(true);
    }
#if 0
    } else if (DISPLAY_TYPE_TABLET == mDisplayType) {

    } else if (DISPLAY_TYPE_REPEATER == mDisplayType) {
        setSourceDisplay(OUTPUT_MODE_STATE_INIT);
    }
#endif

    return 0;
}

void ModePolicy::setSinkDisplay(bool initState) {
    char current_mode[MESON_MODE_LEN] = {0};
    char outputmode[MESON_MODE_LEN] = {0};

    getDisplayMode(current_mode);
    getBootEnv(UBOOTENV_OUTPUTMODE, outputmode);
    MESON_LOGD("init tv display old outputmode:%s, outputmode:%s\n", current_mode, outputmode);

    if (strlen(outputmode) == 0)
        strncpy(outputmode, mDefaultUI.c_str(), MESON_MODE_LEN-1);

    setSinkOutputMode(outputmode, initState);
}

void ModePolicy::setSinkOutputMode(const char* outputmode, bool initState) {
    [[maybe_unused]] bool sinkInitState = initState;
    MESON_LOGI("set sink output mode:%s, init state:%d\n", outputmode, sinkInitState);

    //set output mode
    char curMode[MESON_MODE_LEN] = {0};
    getDisplayMode(curMode);

    MESON_LOGI("curMode = %s outputmode = %s", curMode, outputmode);
    if (strstr(curMode, outputmode) == NULL) {
        setDisplayMode(outputmode);
    }

    char defVal[PROPERTY_VALUE_MAX] = "0x0";
    if (sys_get_bool_prop(PROP_DISPLAY_SIZE_CHECK, true)) {
        char resolution[MESON_MODE_LEN] = {0};
        char defaultResolution[MESON_MODE_LEN] = {0};
        char finalResolution[MESON_MODE_LEN] = {0};
        int w = 0, h = 0, w1 =0, h1 = 0;
        sysfs_get_string(SYS_DISPLAY_RESOLUTION, resolution, MESON_MODE_LEN);

        sys_get_string_prop_default(PROP_DISPLAY_SIZE, defaultResolution, defVal);
        sscanf(resolution, "%dx%d", &w, &h);
        sscanf(defaultResolution, "%dx%d", &w1, &h1);
        if ((w != w1) || (h != h1)) {
            if (strstr(outputmode, "null") && w1 != 0) {
                sprintf(finalResolution, "%dx%d", w1, h1);
            } else {
                sprintf(finalResolution, "%dx%d", w, h);
            }
            sys_set_prop(PROP_DOLBY_VISION_PRIORITY, finalResolution);
        }
    }

    char defaultResolution[MESON_MODE_LEN] = {0};
    sys_get_string_prop_default(PROP_DISPLAY_SIZE, defaultResolution, defVal);
    MESON_LOGI("set display-size:%s\n", defaultResolution);

    //update hwc windows size
    int position[4] = { 0, 0, 0, 0 };//x,y,w,h
    getPosition(outputmode, position);
    setPosition(outputmode, position[0], position[1],position[2], position[3]);

    //update hdr policy
    if ((isMboxSupportDolbyVision() == false)) {
        if (sys_get_bool_prop(PROP_DOLBY_VISION_FEATURE, false)) {
            char hdr_policy[MESON_MODE_LEN] = {0};
            getHdrStrategy(hdr_policy);
            if (strstr(hdr_policy, HDR_POLICY_SINK)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SINK);
            } else if (strstr(hdr_policy, HDR_POLICY_SOURCE)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_SOURCE);
            } else if (strstr(hdr_policy, HDR_POLICY_FORCE)) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, HDR_POLICY_FORCE);
            }
        } else {
            initHdrSdrMode();
        }
    }

    if (isMboxSupportDolbyVision()) {
        if (isTvDolbyVisionEnable()) {
            setTvDolbyVisionEnable();
        } else {
            setTvDolbyVisionDisable();
        }
    }

    //audio
    char value[MESON_MAX_STR_LEN] = {0};
    memset(value, 0, sizeof(0));
    getBootEnv(UBOOTENV_DIGITAUDIO, value);
    setDigitalMode(value);

    //save output mode
    char finalMode[MESON_MODE_LEN] = {0};
    getDisplayMode(finalMode);
    if (DISPLAY_TYPE_TABLET != mDisplayType) {
        setBootEnv(UBOOTENV_OUTPUTMODE, (char *)finalMode);
    }
    if (strstr(finalMode, "cvbs") != NULL) {
        setBootEnv(UBOOTENV_CVBSMODE, (char *)finalMode);
    } else if (strstr(finalMode, "hz") != NULL) {
        setBootEnv(UBOOTENV_HDMIMODE, (char *)finalMode);
    }

    MESON_LOGI("set output mode:%s done\n", finalMode);
}

bool ModePolicy::isHdmiUsed() {
    bool ret = true;
    char hdmi_state[MESON_MODE_LEN] = {0};
    sysfs_get_string(DISPLAY_HDMI_USED, hdmi_state, MESON_MODE_LEN);

    if (strstr(hdmi_state, "1") == NULL) {
        ret = false;
    }

    return ret;
}

bool ModePolicy::isConnected() {
    return mConnector->isConnected();
}

bool ModePolicy::isVMXCertification() {
    return sys_get_bool_prop(PROP_VMX, false);
}

bool ModePolicy::isHdmiEdidParseOK() {
    bool ret = true;

    char edidParsing[MESON_MODE_LEN] = {0};
    sysfs_get_string(DISPLAY_EDID_STATUS, edidParsing, MESON_MODE_LEN);

    if (strcmp(edidParsing, "ok")) {
        ret = false;
    }

    return ret;
}

bool ModePolicy::isBestPolicy() {
    char isBestMode[MESON_MODE_LEN] = {0};
    if (DISPLAY_TYPE_TV == mDisplayType) {
        return false;
    }

    return !getBootEnv(UBOOTENV_ISBESTMODE, isBestMode) || strcmp(isBestMode, "true") == 0;
}

bool ModePolicy::isBestColorSpace() {
    char isBestColorSpace[MESON_MODE_LEN] = {0};
    if (DISPLAY_TYPE_TV == mDisplayType) {
        return false;
    }
    return !getBootEnv(UBOOTENV_BESTCOLORSPACE, isBestColorSpace) || strcmp(isBestColorSpace, "true") == 0;
}

void ModePolicy::setDefaultMode() {
    MESON_LOGE("EDID parsing error detected\n");

    // check hdmi output mode
    char curDisplayMode[MESON_MODE_LEN]    = {0};
    getDisplayMode(curDisplayMode);

    if (!isMatchMode(curDisplayMode, MESON_DEFAULT_HDMI_MODE)) {
        //set avmute
        sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
        //set default color format
        setDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, MESON_DEFAULT_COLOR_FORMAT);
        //set default resolution
        setDisplayMode(MESON_DEFAULT_HDMI_MODE);

        //update display position
        int position[4] = { 0, 0, 0, 0 };//x,y,w,h
        getPosition(MESON_DEFAULT_HDMI_MODE, position);
        setPosition(MESON_DEFAULT_HDMI_MODE, position[0], position[1],position[2], position[3]);

        //clear avmute
        sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "-1");
    } else {
        MESON_LOGI("cur mode is default mode\n");
    }
}


/*
 * OUTPUT_MODE_STATE_INIT for boot
 * OUTPUT_MODE_STATE_POWER for hdmi plug and suspend/resume
 */
void ModePolicy::setSourceDisplay(output_mode_state state) {
    std::lock_guard<std::mutex> lock(mMutex);

    //1. hdmi used and hpd = 0
    //set dummy_l mode
    if ((isHdmiUsed() == true) && (isConnected() == false)) {
        MESON_LOGD("hdmi usd, set dummy_l");
        if (isVMXCertification()) {
            setDisplayMode("576cvbs");
        } else {
            setDisplayMode("dummy_l");
        }

        MESON_LOGI("hdmi used but plugout when boot\n");
        return;
    }

    //2. hdmi edid parse error and hpd = 1
    //set default reolsution and color format
    if ((isHdmiEdidParseOK() == false) &&
        (isConnected() == true)) {
        setDefaultMode();

        return;
    }

    //3. update hdmi info when boot and hdmi plug/suspend/resume
    if ((state == OUTPUT_MODE_STATE_INIT) ||
        (state == OUTPUT_MODE_STATE_POWER)) {
        memset(&mConData, 0, sizeof(meson_policy_in));
        memset(&mDvInfo, 0, sizeof(hdmi_dv_info_t));
        mState = state;
        mConData.state = static_cast<meson_mode_state>(state);
        getConnectorData(&mConData, &mDvInfo);

        strcpy(mConData.cur_displaymode, mConData.con_info.ubootenv_hdmimode);
     }

    // TOD sceneProcess
    if (isBestPolicy()) {
        mPolicy = MESON_POLICY_BEST;
    }

    meson_mode_set_policy(mModeConType, mPolicy);
    meson_mode_set_policy_input(mModeConType, &mConData);
    meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

    //5. apply settings to driver
    applyDisplaySetting();
}

void ModePolicy::setSourceOutputMode(const char* outputmode, bool force) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (DISPLAY_TYPE_TV == mDisplayType) {
        setSinkOutputMode(outputmode, false);
    } else {
        mState = OUTPUT_MODE_STATE_SWITCH;
        mConData.state = static_cast<meson_mode_state>(mState);

        strcpy(mConData.cur_displaymode, outputmode);
        //meson_mode_set_policy(mModeConType, mPolicy);
        meson_mode_set_policy_input(mModeConType, &mConData);
        meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

        applyDisplaySetting(force);
    }
}

//TODO: do we really need it ?
void ModePolicy::updateDeepColor(bool cvbsMode, output_mode_state state, const char* outputmode) {
    if (!cvbsMode && (mDisplayType != DISPLAY_TYPE_TV)) {
        char colorAttribute[MESON_MODE_LEN] = {0};
        if (sys_get_bool_prop(PROP_DEEPCOLOR, true)) {
            char mode[MESON_MAX_STR_LEN] = {0};
            if (isDolbyVisionEnable() && isTvSupportDolbyVision(mode) &&
                    (mConData.hdr_info.hdr_priority == MESON_DOLBY_VISION_PRIORITY)) {
                 char type[MESON_MODE_LEN] = {0};
                 strcpy(type, mConData.hdr_info.ubootenv_dv_type);
                if (((atoi(type) == 2) && (strstr(mode, DV_MODE_TYPE[2]) == NULL))
                        || ((atoi(type) == 3) && (strstr(mode, DV_MODE_TYPE[3]) == NULL)
                            && (strstr(mode, DV_MODE_TYPE[4]) == NULL))) {
                    strcpy(type, "1");
                }
                switch (atoi(type)) {
                    case DOLBY_VISION_SET_ENABLE:
                        strcpy(colorAttribute, "444,8bit");
                        break;
                    case DOLBY_VISION_SET_ENABLE_LL_YUV:
                        strcpy(colorAttribute, "422,12bit");
                        break;
                    case DOLBY_VISION_SET_ENABLE_LL_RGB:
                        if (strstr(mode, "LL_RGB_444_12BIT") != NULL) {
                            strcpy(colorAttribute, "444,12bit");
                        } else if (strstr(mode, "LL_RGB_444_10BIT") != NULL) {
                            strcpy(colorAttribute, "444,10bit");
                        }
                        break;
                }
            } else {
                getHdmiColorAttribute(outputmode, colorAttribute, (int)state);
            }
        } else {
            strcpy(colorAttribute, "default");
        }
        std::string attr;
        getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, attr);
        if (strstr(attr.c_str(), colorAttribute) == NULL) {
            MESON_LOGI("set DeepcolorAttr value is different from attr sysfs value\n");
            setDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, colorAttribute);
        } else {
            MESON_LOGI("cur deepcolor attr value is equals to colorAttribute, Do not need set it\n");
        }
        MESON_LOGI("setMboxOutputMode colorAttribute = %s\n", colorAttribute);
        //save to ubootenv
        saveDeepColorAttr(outputmode, colorAttribute);
        setBootEnv(UBOOTENV_COLORATTRIBUTE, colorAttribute);
    }
}

void ModePolicy::getCommonData(struct meson_policy_in* data, hdmi_dv_info_t *dinfo) {
    if (!data || !dinfo) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return;
    }

    //hdmi color space best policy flag
    data->con_info.is_bestcolorspace = isBestColorSpace();

    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);
    data->hdr_info.hdr_policy = (meson_hdr_policy_e)atoi(hdr_policy);

    data->hdr_info.hdr_priority = (meson_hdr_priority_e)getHdrPriority();

    MESON_LOGI("isbestColorspace:%d, hdr_policy:%d, hdr_priority :%d\n",
            data->con_info.is_bestcolorspace,
            data->hdr_info.hdr_policy,
            data->hdr_info.hdr_priority);

    getDisplayMode(mCurrentMode);
    getBootEnv(UBOOTENV_HDMIMODE, data->con_info.ubootenv_hdmimode);
    getBootEnv(UBOOTENV_CVBSMODE, data->con_info.ubootenv_cvbsmode);
    MESON_LOGI("hdmi_current_mode:%s, ubootenv hdmimode:%s cvbsmode:%s\n",
            mCurrentMode,
            data->con_info.ubootenv_hdmimode,
            data->con_info.ubootenv_cvbsmode);

    getBootEnv(UBOOTENV_COLORATTRIBUTE, data->con_info.ubootenv_colorattr);
    MESON_LOGI("ubootenv_colorattribute:%s\n",
            data->con_info.ubootenv_colorattr);

    //if no dolby_status env set to std for enable amdolby vision
    //if box support amdolby vision
    bool ret;
    char dv_enable[MESON_MODE_LEN];
    ret = getBootEnv(UBOOTENV_DV_ENABLE, dv_enable);
    if (ret) {
        strcpy(dinfo->dv_enable, dv_enable);
    } else if (isMboxSupportDolbyVision()) {
        strcpy(dinfo->dv_enable, "1");
    } else {
        strcpy(dinfo->dv_enable, "0");
    }
    MESON_LOGI("dv_enable:%s\n", dinfo->dv_enable);

    char ubootenv_dv_type[MESON_MODE_LEN];
    ret = getBootEnv(UBOOTENV_DV_TYPE, ubootenv_dv_type);
    if (ret) {
        strcpy(dinfo->ubootenv_dv_type, ubootenv_dv_type);
    } else if (isMboxSupportDolbyVision()) {
        strcpy(dinfo->ubootenv_dv_type, "1");
    } else {
        strcpy(dinfo->ubootenv_dv_type, "0");
    }
    MESON_LOGI("ubootenv_dv_type:%s\n", dinfo->ubootenv_dv_type);

}

void ModePolicy::drmMode2MesonMode(meson_mode_info_t &mesonMode, drm_mode_info_t &drmMode) {
    strncpy(mesonMode.name, drmMode.name, MESON_MODE_LEN);
    mesonMode.dpi_x = drmMode.dpiX;
    mesonMode.dpi_y = drmMode.dpiY;
    mesonMode.pixel_w = drmMode.pixelW;
    mesonMode.pixel_h = drmMode.pixelH;
    mesonMode.refresh_rate = drmMode.refreshRate;
    mesonMode.group_id = drmMode.groupId;
}

void ModePolicy::getSupportedModes() {
    //pthread_mutex_lock(&mEnvLock);
    std::map<uint32_t, drm_mode_info_t> connecterModeList;

    /* reset ModeList */
    mModes.clear();
    if (mConnector->isConnected()) {
        mConnector->getModes(connecterModeList);

        for (auto it = connecterModeList.begin(); it != connecterModeList.end(); it++) {
            // All modes are supported
            if (isModeSupported(it->second)) {
                mModes.emplace(mModes.size(), it->second);
            }
        }
    }

    /* transfer to meson_mode_info_t */
    auto conPtr = &mConData.con_info;
    conPtr->modes_size = mModes.size();
    if (conPtr->modes_size > conPtr->modes_capacity) {
        // realloc memory
        void * buff = realloc(conPtr->modes, conPtr->modes_size * sizeof(meson_mode_info_t));
        MESON_ASSERT(buff, "modePolicy realloc but has no memory");
        conPtr->modes = (meson_mode_info_t *) buff;
        conPtr->modes_capacity = conPtr->modes_size;
    }

    int i = 0;
    for (auto it = mModes.begin(); it != mModes.end(); it++) {
        drmMode2MesonMode(conPtr->modes[i], it->second);
        i++;
    }
}

bool ModePolicy::isModeSupported(drm_mode_info_t mode) {
    bool ret = false;
    uint32_t i;

    for (i = 0; i < ARRAY_SIZE(DISPLAY_MODE_LIST); i++) {
        if (!strcmp(DISPLAY_MODE_LIST[i], mode.name)) {
            ret = true;
            break;
        }
    }

    return ret;
}

