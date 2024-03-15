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
#include <DebugHelper.h>

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
    MODE_640x480P,
    MODE_576P,
    MODE_720P,
    MODE_720P50HZ,
    MODE_720P100HZ,
    MODE_720P120HZ,
    MODE_1080P24HZ,
    MODE_1080P25HZ,
    MODE_1080P30HZ,
    MODE_1080I50HZ,
    MODE_1080P50HZ,
    MODE_1080I,
    MODE_1080P,
    MODE_1080P100HZ,
    MODE_1080P120HZ,
    MODE_1440P50HZ,
    MODE_1440P60HZ,
    MODE_1440P100HZ,
    MODE_1440P120HZ,
    MODE_4K2K24HZ,
    MODE_4K2K25HZ,
    MODE_4K2K30HZ,
    MODE_4K2K50HZ,
    MODE_4K2K60HZ,
    MODE_4K2K100HZ,
    MODE_4K2K120HZ,
    MODE_4K2KSMPTE24HZ,
    MODE_4K2KSMPTE30HZ,
    MODE_4K2KSMPTE50HZ,
    MODE_4K2KSMPTE60HZ,
    MODE_4K2KSMPTE100HZ,
    MODE_4K2KSMPTE120HZ,
    MODE_8K4K24HZ,
    MODE_8K4K25HZ,
    MODE_8K4K30HZ,
    MODE_8K4K48HZ,
    MODE_8K4K50HZ,
    MODE_8K4K60HZ,
    MODE_768P,
    MODE_PANEL,
    MODE_480CVBS,
    MODE_576CVBS,
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

ModePolicy::ModePolicy() {
    mDisplayType = DISPLAY_TYPE_MBOX;
    mPolicy = MESON_POLICY_BEST;
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
    mThread = 0;
    mInitialized = false;
}

ModePolicy::ModePolicy(std::shared_ptr<meson::DisplayAdapter> adapter, const uint32_t displayId) {
    mAdapter = adapter;
    mDisplayType = DISPLAY_TYPE_MBOX;
    mPolicy = MESON_POLICY_BEST;
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
    mInitialized = false;
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
        memset(data->dv_max_mode, 0, sizeof(data->dv_max_mode));
        memset(data->dv_deepcolor, 0, sizeof(data->dv_deepcolor));
        for (int i = ARRAY_SIZE(DISPLAY_MODE_LIST) - 1; i >= 0; i--) {
            if (strstr(data->dv_cap, DISPLAY_MODE_LIST[i]) != NULL) {
                if ((strlen(data->dv_max_mode) + strlen(DISPLAY_MODE_LIST[i]) + 1) < sizeof(data->dv_max_mode)) {
                    strcpy(data->dv_max_mode, DISPLAY_MODE_LIST[i]);
                } else {
                    MESON_LOGE("DisplayMode strcat overflow: src=%s, dst=%s\n", DISPLAY_MODE_LIST[i], data->dv_max_mode);
                }
                break;
            }
        }

        for (int i = 0; i < sizeof(DV_MODE_TYPE)/sizeof(DV_MODE_TYPE[0]); i++) {
            if (strstr(data->dv_cap, DV_MODE_TYPE[i])) {
                if ((strlen(data->dv_deepcolor) + strlen(DV_MODE_TYPE[i]) + 2) < sizeof(data->dv_deepcolor)) {
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

    int32_t priority = MESON_HDR10_PRIORITY;
    value = std::to_string(MESON_HDR10_PRIORITY);
    if (strstr(type, DV_DISABLE_FORCE_SDR)) {
        priority = MESON_SDR_PRIORITY;
        value = std::to_string(MESON_SDR_PRIORITY);
    }

    //2. set current hdmi mode
    meson_mode_set_policy_input(mModeConType, &mConData);
    getDisplayMode(mCurrentMode);

    if (!meson_mode_support_mode(mModeConType, priority, mCurrentMode)) {
        setBootEnv(UBOOTENV_HDR_FORCE_MODE, type);
        setSourceOutputMode(mCurrentMode);
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

    if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE])) {
        strcpy(value, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
    } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK])) {
        strcpy(value, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
    } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE])) {
        strcpy(value, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]);
    } else if (strstr(hdr_policy, DV_POLICY_FORCE_MODE)) {
       strcpy(value, DV_POLICY_FORCE_MODE);
    }
    MESON_LOGI("get uboot HdrStrategy is [%s]", value);
}

int32_t ModePolicy::setHdrPriority(int32_t type) {
    MESON_LOGI("setHdrPriority is [%s]\n", meson_hdrPriorityToString(type));

    meson_mode_set_policy_input(mModeConType, &mConData);
    getDisplayMode(mCurrentMode);

    if (!meson_mode_support_mode(mModeConType, type, mCurrentMode)) {
        std::string value = std::to_string(type);

        setSourceOutputMode(mCurrentMode);
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

    value = (meson_hdr_priority_e)atoi(hdr_priority);
    if ((value >= MESON_DOLBY_VISION_PRIORITY && value <= MESON_SDR_PRIORITY)
        || (value >= MESON_G_DV_HDR10_HLG && value <= MESON_G_SDR)) {
        MESON_LOGI("%s is [%s]", __FUNCTION__, meson_hdrPriorityToString(value));
    } else {
        MESON_LOGE("%s [%d] is invalid", __FUNCTION__, value);
    }

    MESON_LOGI("get uboot HdrPriority is [%s]", meson_hdrPriorityToString(value));
    return (int32_t)value;
}

int32_t ModePolicy::getCurrentHdrPriority(void) {
    meson_hdr_priority_e value = MESON_DOLBY_VISION_PRIORITY;

    std::string cur_hdr_priority;
    getDisplayAttribute(DISPLAY_HDR_PRIORITY, cur_hdr_priority);

    value = (meson_hdr_priority_e)atoi(cur_hdr_priority.c_str());
    if ((value >= MESON_DOLBY_VISION_PRIORITY && value <= MESON_SDR_PRIORITY)
        || (value >= MESON_G_DV_HDR10_HLG && value <= MESON_G_SDR)) {
        MESON_LOGI("%s is [%s]", __FUNCTION__, meson_hdrPriorityToString(value));
    } else {
        MESON_LOGE("%s [%d] is invalid", __FUNCTION__, value);
    }

    return (int32_t)value;
}

void ModePolicy::gethdrforcemode(char* value) {
    if (!value) {
        MESON_LOGE("%s value is NULL\n", __FUNCTION__);
        return;
    }

    bool ret = false;
    char hdr_force_mode[MESON_MODE_LEN] = {0};

    memset(hdr_force_mode, 0, MESON_MODE_LEN);
    ret = getBootEnv(UBOOTENV_HDR_FORCE_MODE, hdr_force_mode);
    if (ret) {
        strcpy(value, hdr_force_mode);
    } else {
        strcpy(value, FORCE_DV);
    }

    MESON_LOGI("get hdr force mode is [%s]", value);
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
bool ModePolicy::isTvSupportDolbyVision() {
    if (DISPLAY_TYPE_TV == mDisplayType) {
        MESON_LOGI("Current Device is TV, no dv_cap\n");
        return false;
    }

    std::string dv_cap;
    getDisplayAttribute(DISPLAY_DOLBY_VISION_CAP2, dv_cap);

    if (strstr(dv_cap.c_str(), "DolbyVision RX support list") == NULL) {
        return false;
    }

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

void ModePolicy::getHdrUserInfo(meson_hdr_info_t *data) {
    if (!data) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return;
    }

    char hdr_force_mode[MESON_MODE_LEN] = {0};
    gethdrforcemode(hdr_force_mode);
    data->hdr_force_mode = (meson_hdr_force_mode_e)atoi(hdr_force_mode);

    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);
    data->hdr_policy = (meson_hdr_policy_e)atoi(hdr_policy);

    data->hdr_priority = (meson_hdr_priority_e)getHdrPriority();

    MESON_LOGI("hdr_policy:%d, hdr_priority :%d(0x%x), hdr_force_mode:%d\n",
            data->hdr_policy,
            data->hdr_priority, data->hdr_priority,
            data->hdr_force_mode);

    data->is_enable_dv = isDolbyVisionEnable();
    data->is_tv_supportDv = isTvSupportDolbyVision();
    data->is_tv_supportHDR = isTvSupportHDR();
    data->is_hdr_resolution_priority = isHdrResolutionPriority();
    data->is_lowpower_mode = isLowPowerMode();


    char ubootenv_dv_type[MESON_MODE_LEN];
    bool ret = getBootEnv(UBOOTENV_USER_DV_TYPE, ubootenv_dv_type);
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

    getConnectorUserData(data, dinfo);

    //get hdmi dv_info
    getDvCap(&data->hdr_info);
    // get Hdr info
    getHdrUserInfo(&data->hdr_info);

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
        case static_cast<int>(MESON_POLICY_DV):
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
        } else {
            sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
            usleep(100000); // add 100ms delay after av mute
        }
        setDisplayMode(displayMode);
        return;
    }


    //hdmi connect
    setSourceDisplay(OUTPUT_MODE_STATE_POWER);
}

void ModePolicy::setActiveConfig(std::string mode) {
    mReason = OUTPUT_CHANGE_BY_HWC;
    MESON_LOGI("setDisplayed by hwc %s", mode.c_str());
    meson_mode_set_policy(mModeConType, MESON_POLICY_INVALID);
    setSourceOutputMode(mode.c_str());
    meson_mode_set_policy(mModeConType, mPolicy);
    mReason = OUTPUT_CHANGE_BY_INIT;
}

int32_t ModePolicy::getPreferredBootConfig(std::string &config) {
    if (DISPLAY_TYPE_TV == mDisplayType) {
        char curMode[MESON_MODE_LEN] = {0};
        getDisplayMode(curMode);
        config = curMode;
    } else {
        mConData.state = static_cast<meson_mode_state>(OUTPUT_MODE_STATE_INIT);
        mConData.con_info.is_bestcolorspace = true;

        //2. scene logic process
        meson_mode_set_policy(mModeConType, MESON_POLICY_BEST);
        meson_mode_set_policy_input(mModeConType, &mConData);
        meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

        config = mSceneOutInfo.displaymode;
    }

    MESON_LOGI("getPreferredDisplayConfig [%s]", config.c_str());
    meson_mode_set_policy(mModeConType, mPolicy);
    return 0;
}



int32_t ModePolicy::setBootConfig(drm_mode_info_t & config) {
    MESON_LOGI("set boot display config to %s\n", config.name);
    setBootEnv(UBOOTENV_ISBESTMODE, "false");
    if (strstr(config.name, "cvbs") != NULL
        || strstr(config.name, "pal") != NULL
        || strstr(config.name, "ntsc") != NULL) {
        setBootEnv(UBOOTENV_CVBSMODE, config.name);
    } else if (strstr(config.name, "hz") != NULL) {
        setBootEnv(UBOOTENV_HDMIMODE, config.name);
    }
    mPolicy = MESON_POLICY_INVALID;
    meson_mode_set_policy(mModeConType, mPolicy);

    if (fabs(config.refreshRate - std::floor(config.refreshRate)) > 1e-2) {
        setBootEnv(UBOOTENV_FRAC_RATE_POLICY, "1");
    } else {
        setBootEnv(UBOOTENV_FRAC_RATE_POLICY, "0");
    }

    return 0;
}

int32_t ModePolicy::clearBootConfig() {
    MESON_LOGI("clear boot display \n");
    setBootEnv(UBOOTENV_ISBESTMODE, "true");
    mPolicy = MESON_POLICY_BEST;
    meson_mode_set_policy(mModeConType, mPolicy);

    //save hdmi resolution to env
    setBootEnv(UBOOTENV_HDMIMODE, "none");
    //need to keep the same value with the defaul value
    setBootEnv(UBOOTENV_FRAC_RATE_POLICY, "1");

    return 0;
}

int32_t ModePolicy::clearUserDisplayConfig() {
    SYS_LOGI("clear user display config\n");

    //clear user color format
    setBootEnv(UBOOTENV_USER_COLORATTRIBUTE, "none");
    //clear user dv
    setBootEnv(UBOOTENV_USER_DV_TYPE, "none");

    //2. set hdmi mode for trigger setting
    getDisplayMode(mCurrentMode);
    setSourceOutputMode(mCurrentMode);

    return 0;
}

int32_t ModePolicy::getCurrentSupportDeepColor(std::string& color) {
    int ret = 0;
    char dc_cap[MESON_MAX_STR_LEN] = {0};

    getDisplayMode(mCurrentMode);
    ret = meson_mode_get_support_color(mModeConType, mCurrentMode, dc_cap);
    color = dc_cap;

    SYS_LOGI("%s current mode: %s supported color: %s", __func__, mCurrentMode, dc_cap);
    return ret;
}

int32_t ModePolicy::setColorSpace(std::string &colorspace) {
    SYS_LOGI("user change color space to %s\n", colorspace.c_str());
    setBootEnv(UBOOTENV_USER_COLORATTRIBUTE, colorspace.c_str());

    getDisplayMode(mCurrentMode);
    saveDeepColorAttr(mCurrentMode, colorspace.c_str());

    //2. set hdmi mode for trigger setting
    setSourceOutputMode(mCurrentMode);

    return 0;
}

void ModePolicy::dump(String8 &dumpstr) {
    dumpstr.append("---------------------------------------------------------"
        "-----------------------------\n");
    dumpstr.append("ModePolicy :\n");
    dumpstr.appendFormat("MesonModePolicy: %s \n", meson_modePolicyToString(mPolicy));
    dumpstr.appendFormat("MesonHdrPriority: %s \n", meson_hdrPriorityToString(mHdr_priority));
    dumpstr.appendFormat("MesonHdrPolicy: %s \n", meson_hdrPolicyToString(mHdr_policy));
    dumpstr.appendFormat("DV enable: %s \n", mDvInfo.dv_enable);
    dumpstr.appendFormat("Policy Out :displaymode: %s, deepcolor: %s, dv_type: %d\n",
        mSceneOutInfo.displaymode, mSceneOutInfo.deepcolor, mSceneOutInfo.dv_type);

    /* dump detail policy in info*/
    if (DebugHelper::getInstance().dumpDetailInfo()) {
        dumpstr.append("\nPolicy In :\n    HDR info: \n");
        dumpstr.appendFormat("\t enable dv: %s \n", mConData.hdr_info.is_enable_dv ? "Y" : "N");
        dumpstr.appendFormat("\t TV support HDR: %s \n", mConData.hdr_info.is_tv_supportHDR ? "Y" : "N");
        dumpstr.appendFormat("\t TV support Dv: %s \n", mConData.hdr_info.is_tv_supportDv ? "Y" : "N");
        dumpstr.appendFormat("\t HDR Resolution Priority: %s \n", mConData.hdr_info.is_hdr_resolution_priority ? "Y" : "N");
        dumpstr.appendFormat(
            "\t ubootenv dv type: %s \n"
            "\t tv dv cap: %s \n"
            "\t tv dv max supported resolution: %s \n"
            "\t dv deepcolor: %s \n"
            "\t HdrPriority: %s \n"
            "\t HdrPolicy: %s \n"
            "\t HdrForceMode: %d \n \n",
            mConData.hdr_info.ubootenv_dv_type, mConData.hdr_info.dv_cap,
            mConData.hdr_info.dv_max_mode, mConData.hdr_info.dv_deepcolor,
            meson_hdrPriorityToString(mConData.hdr_info.hdr_priority),
            meson_hdrPolicyToString(mConData.hdr_info.hdr_policy),
            mConData.hdr_info.hdr_force_mode);
        dumpstr.append("    Connector info: \n");
        dumpstr.appendFormat("\t mConnectType:%d\n", mConnectorType);
        dumpstr.appendFormat("\t bestcolorspace: %s \n", mConData.con_info.is_bestcolorspace ? "Y" : "N");
        dumpstr.appendFormat("\t support4k: %s \n", mConData.con_info.is_support4k ? "Y" : "N");
        dumpstr.appendFormat("\t support4k30HZ: %s \n", mConData.con_info.is_support4k30HZ ? "Y" : "N");
        dumpstr.appendFormat("\t deepcolor: %s \n", mConData.con_info.is_deepcolor ? "Y" : "N");
        dumpstr.appendFormat(
            "\t ubootenv cvbsmode: %s \n"
            "\t ubootenv hdmimode: %s \n"
            "\t ubootenv colorattr: %s \n"
            "device colorspace cap:\n%s \n",
            mConData.con_info.ubootenv_cvbsmode, mConData.con_info.ubootenv_hdmimode,
            mConData.con_info.ubootenv_colorattr, mConData.con_info.dc_cap);
    }
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
        case DRM_MODE_CONNECTOR_VIRTUAL:
            mDisplayType = DISPLAY_TYPE_MBOX;
            mConnectorType = ConnectorType::CONN_TYPE_DUMMY;
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
    char ubootvar[256] = {0};
    char outputMode[MESON_MODE_LEN] = {0};
    strcpy(outputMode, mode);

    drm_mode_info_t brrMode;
    if (findBrrMode(mode, brrMode)) {
        strcpy(outputMode, brrMode.name);
    }

    sprintf(ubootvar, "ubootenv.var.%s_deepcolor", outputMode);
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
    if (strstr(outputMode, "cvbs") != NULL
        || strstr(outputMode, "pal") != NULL
        || strstr(outputMode, "ntsc") != NULL) {
        setBootEnv(UBOOTENV_CVBSMODE, (char *)outputMode);
    } else if (strcmp(outputMode, "null") && strcmp(outputMode, "dummy_l")) {
        std::string colorAttr;
        char colorDepth[MESON_MODE_LEN] = {0};
        char colorSpace[MESON_MODE_LEN] = {0};
        char dvstatus[MESON_MODE_LEN]   = {0};

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

        // 2.3 save amdolby status/dv_type
        // In follow sink mode: 0:disable 1:STD(or enable dv) 2:LL YUV 3: LL RGB
        // In follow source mode: dv is disable  in uboot.
        if (isMboxSupportDolbyVision()) {
            sprintf(dvstatus, "%d", mSceneOutInfo.dv_type);
            setBootEnv(UBOOTENV_DOLBYSTATUS, dvstatus);
            setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);

            MESON_LOGI("dvstatus %s dv_type %d dv_enable %s\n",
                dvstatus, mSceneOutInfo.dv_type, mDvInfo.dv_enable);

        } else {
            MESON_LOGI("MBOX is not support dv, dvstatus %s dv_type %d dv_enable %s\n",
                dvstatus, mSceneOutInfo.dv_type, mDvInfo.dv_enable);
        }

        MESON_LOGI("colorattr: %s, outputMode %s, cd %s, cs %s\n",
            colorAttr.c_str(), outputMode, colorDepth, colorSpace);
    }
}


void ModePolicy::enableDolbyVision(int DvMode) {
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
    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);
    if ((DISPLAY_TYPE_MBOX == mDisplayType) || (DISPLAY_TYPE_REPEATER == mDisplayType)) {
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

        if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK])) {
            setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
            if (isDolbyVisionEnable()) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
            }
        } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE])) {
            setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
            if (isDolbyVisionEnable()) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
            }
        } else if (strstr(hdr_policy, DV_POLICY_FORCE_MODE)) {
            setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]);
            if (isDolbyVisionEnable()) {
                setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FORCE_MODE);
            }
        }
    }

    usleep(100000);//100ms
    setDisplayAttribute(DISPLAY_DOLBY_VISION_ENABLE, DV_ENABLE);
    if (strstr(hdr_policy, DV_POLICY_FORCE_MODE)) {
        char hdr_force_mode[MESON_MODE_LEN] = {0};
        gethdrforcemode(hdr_force_mode);
        if (strstr(hdr_force_mode, FORCE_DV)) {
            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_DV);
        } else if (strstr(hdr_force_mode, FORCE_HDR10)) {
            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_HDR10);
        } else if (strstr(hdr_force_mode, DV_DISABLE_FORCE_SDR)) {
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
        setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_MODE_IPT_TUNNEL);
    }
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

    if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK])) {
        setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
    } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE])) {
        setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
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

    if (mConnector && mConnector->isConnected()) {
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
    setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);

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


int32_t ModePolicy::setDvMode(std::string &dv_mode) {
    MESON_LOGI("%s dv mode:%s", __FUNCTION__, dv_mode.c_str());

    if (DISPLAY_TYPE_TV == mDisplayType) {
        //1. update prop
        strcpy(mConData.hdr_info.ubootenv_dv_type, dv_mode.c_str());

        //2. apply to driver
        if (strstr(dv_mode.c_str(), "0")) {
            strcpy(mDvInfo.dv_enable, "0");
            setTvDolbyVisionDisable();
        } else {
            strcpy(mDvInfo.dv_enable, "1");
            setTvDolbyVisionEnable();
        }

        //3. save env
        setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);
    } else {
        //1. update dv env
        strcpy(mConData.hdr_info.ubootenv_dv_type, dv_mode.c_str());

        if (strstr(dv_mode.c_str(), "0")) {
            strcpy(mDvInfo.dv_enable, "0");
        } else {
            strcpy(mDvInfo.dv_enable, "1");
        }

        //Save user prefer dv mode only user change dv through UI
        setBootEnv(UBOOTENV_USER_DV_TYPE, mConData.hdr_info.ubootenv_dv_type);
        setBootEnv(UBOOTENV_DV_ENABLE, mDvInfo.dv_enable);
        setBootEnv(UBOOTENV_DOLBYSTATUS, dv_mode.c_str());

        //2. set hdmi mode for trigger setting
        setSourceOutputMode(mCurrentMode);
    }

    return 0;
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

    if (!isTvSupportALLM()) {
        SYS_LOGI("setALLMMode: TV not support ALLM\n");
        return;
    }

    int perState = -1;
    char cur_allm_state[MESON_MODE_LEN] = {0};
    sysfs_get_string(AUTO_LOW_LATENCY_MODE, cur_allm_state, MESON_MODE_LEN);
    perState = atoi(cur_allm_state);
    if (perState == state) {
        SYS_LOGI("setALLMMode: the ALLM_Mode is not changed :%d\n", state);
        return;
    }

    bool isTVSupportDV = isTvSupportDolbyVision();

    char ubootenv_dv_enable[MESON_MODE_LEN] = {0};
    std::string cur_ColorAttribute;
    char ubootenv_dv_type[MESON_MODE_LEN] = {0};
    bool ret = false;

    switch (state) {
        case -1:
            [[fallthrough]];
        case 0:
            //1. disable allm
            sysfs_set_string(AUTO_LOW_LATENCY_MODE, ALLM_MODE[0]);
            MESON_LOGI("setALLMMode: ALLM_Mode: %s", ALLM_MODE[0]);
            //2.1 get dv status before enable allm
            getBootEnv(UBOOTENV_DV_ENABLE, ubootenv_dv_enable);
            //2.2 get current hdmi output resolution
            getDisplayMode(mCurrentMode);
            //2.3 get current hdmi output color space
            getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, cur_ColorAttribute);
            //2.4 get dv type before enable allm
            ret = getBootEnv(UBOOTENV_USER_DV_TYPE, ubootenv_dv_type);
            //3 enable dv
            //when TV and current resolution support dv and dv is enable before enable allm
            if (isTVSupportDV
                && !strcmp(ubootenv_dv_enable, "1")
                && !(meson_mode_support_mode(mModeConType, MESON_DOLBY_VISION_PRIORITY, mCurrentMode))) {
                sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
                // restore doblyvision when set -1/0 to ALLM
                if (!ret) {
                    //best dv policy:sink-led -->source led
                    if (strstr(mConData.hdr_info.dv_deepcolor, "DV_RGB_444_8BIT") != NULL
                        && strstr(cur_ColorAttribute.c_str(), "444,8bit") != NULL) {
                        enableDolbyVision(DOLBY_VISION_SET_ENABLE);
                        mSceneOutInfo.dv_type = DOLBY_VISION_SET_ENABLE;
                    } else if (strstr(mConData.hdr_info.dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL
                            && strstr(cur_ColorAttribute.c_str(), "422,12bit") != NULL) {
                        enableDolbyVision(DOLBY_VISION_SET_ENABLE_LL_YUV);
                        mSceneOutInfo.dv_type = DOLBY_VISION_SET_ENABLE_LL_YUV;
                    } else {
                        SYS_LOGI("can't enable dv for dv_deepcolor: %s and curColorAttribute: %s\n",
                            mConData.hdr_info.dv_deepcolor, cur_ColorAttribute.c_str());
                    }
                } else if (!strcmp(ubootenv_dv_type, "2") && strstr(cur_ColorAttribute.c_str(), "422,12bit") != NULL) {
                    enableDolbyVision(DOLBY_VISION_SET_ENABLE_LL_YUV);
                    mSceneOutInfo.dv_type = DOLBY_VISION_SET_ENABLE_LL_YUV;
                } else if (!strcmp(ubootenv_dv_type, "1") && strstr(cur_ColorAttribute.c_str(), "444,8bit") != NULL) {
                    enableDolbyVision(DOLBY_VISION_SET_ENABLE);
                    mSceneOutInfo.dv_type = DOLBY_VISION_SET_ENABLE;
                } else {
                    SYS_LOGI("can't enable dv for curColorAttribute: %s\n", cur_ColorAttribute.c_str());
                }
                sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "-1");
            }
            break;
        case 1:
            //when TV support dv and dv is enable
            if (isTVSupportDV && isDolbyVisionEnable()) {
                mSceneOutInfo.dv_type = DOLBY_VISION_SET_DISABLE;
                // disable the doblyvision when ALLM enable
                sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
                disableDolbyVision(DOLBY_VISION_SET_DISABLE);
                sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "-1");
            }
            //2. enable allm
            sysfs_set_string(AUTO_LOW_LATENCY_MODE, ALLM_MODE[2]);
            MESON_LOGI("setALLMMode: ALLM_Mode: %s", ALLM_MODE[2]);
            break;
        default:
            MESON_LOGE("setALLMMode: ALLM_Mode: error state[%d]", state);
            break;
    }
}

bool ModePolicy::isTvConnector() {
    auto type = mConnector->getType();
    if (type == DRM_MODE_CONNECTOR_MESON_LVDS_A || type == DRM_MODE_CONNECTOR_MESON_LVDS_B ||
            type == DRM_MODE_CONNECTOR_MESON_LVDS_C || type == DRM_MODE_CONNECTOR_MESON_VBYONE_A ||
            type == DRM_MODE_CONNECTOR_MESON_VBYONE_B || type == DRM_MODE_CONNECTOR_LVDS ||
            type == LEGACY_NON_DRM_CONNECTOR_PANEL)
        return true;

    return false;
}

int32_t ModePolicy::setAutoLowLatencyMode(bool enabled) {
    auto type = mConnector->getType();
    if (type == DRM_MODE_CONNECTOR_HDMIA) {
        if (!isTvSupportALLM()) {
            return HWC2_ERROR_UNSUPPORTED;
        }

        if (enabled) {
            sysfs_set_string(LOW_LATENCY, LOW_LATENCY_ENABLE);
        } else {
            sysfs_set_string(LOW_LATENCY, LOW_LATENCY_DISABLE);
        }
        setALLMMode(enabled);
        return HWC2_ERROR_NONE;
    }

    // do nothing for tv
    if (isTvConnector())
        return HWC2_ERROR_NONE;

    return HWC2_ERROR_UNSUPPORTED;
}

void ModePolicy::setAllowedHdrTypes(uint32_t allowedHdrTypes, bool isAuto, bool passThrough) {
    //Match content Dynamic range
    if (passThrough) {
        setBootEnv(UBOOTENV_HDR_PREFERRED_POLICY, MESON_HDR_PREFERRED_POLICY[MESON_HDR_MATCH_CONTENT]);
    }
    // force SDR when autoAllowedHdrTypes are empty on system-preferred conversion
    else if (allowedHdrTypes == 0 && isAuto) {
        uint32_t userHdrType = 0;
        userHdrType = userHdrType | (1 << HAL_HDR_DOLBY_VISION);
        userHdrType = userHdrType | (1 << HAL_HDR_HDR10);
        userHdrType = userHdrType | (1 << HAL_HDR_HLG);
        userHdrType = userHdrType | (1 << DRM_INVALID);
        string keyValue = std::to_string(userHdrType);
        setBootEnv(UBOOTENV_USER_PREFERRED_HDR_TYPE, keyValue.c_str());
        setBootEnv(UBOOTENV_HDR_PREFERRED_POLICY, MESON_HDR_PREFERRED_POLICY[MESON_HDR_SYSTEM_PREFERRED]);
        return;
    } else {
        //bit0-bit4 for hdr type and 1 is disable 0 is enable
        uint32_t userHdrType = 0;
        userHdrType = userHdrType | (1 << HAL_HDR_DOLBY_VISION);
        userHdrType = userHdrType | (1 << HAL_HDR_HDR10);
        userHdrType = userHdrType | (1 << HAL_HDR_HLG);
        userHdrType = userHdrType | (1 << DRM_INVALID);
        allowedHdrTypes ^= userHdrType;
        allowedHdrTypes &= userHdrType;
        string keyValue = std::to_string(allowedHdrTypes);
        setBootEnv(UBOOTENV_USER_PREFERRED_HDR_TYPE, keyValue.c_str());
        if (isAuto)
            setBootEnv(UBOOTENV_HDR_PREFERRED_POLICY, MESON_HDR_PREFERRED_POLICY[MESON_HDR_SYSTEM_PREFERRED]);
        else
            setBootEnv(UBOOTENV_HDR_PREFERRED_POLICY, MESON_HDR_PREFERRED_POLICY[MESON_HDR_FORCE]);

        MESON_LOGD("%s AllowedHdrType %d isAuto %d keyValue %s \n", __func__, allowedHdrTypes, isAuto, keyValue.c_str());
    }

    return;
}

int32_t ModePolicy::getPreferredHdrConversionType(void) {
    int32_t outHdrConversionType = -1;

    char auto_policy[MESON_MODE_LEN] = {0};
    memset(auto_policy, 0, MESON_MODE_LEN);
    getBootEnv(UBOOTENV_HDR_PREFERRED_POLICY, auto_policy);

    //Match content Dynamic range
    if (strstr(auto_policy, MESON_HDR_PREFERRED_POLICY[MESON_HDR_MATCH_CONTENT])) {
        mHdr_policy   = MESON_HDR_POLICY_SOURCE;
        mHdr_priority = MESON_G_DV_HDR10_HLG;
    }
    //System-preferred conversion or Force conversion
    else {
        bool isAuto = true;
        if (strstr(auto_policy, MESON_HDR_PREFERRED_POLICY[MESON_HDR_FORCE])) {
            isAuto = false;
        }

        char user_hdr_type[MESON_MODE_LEN] = {0};
        memset(user_hdr_type, 0, MESON_MODE_LEN);
        bool ret = getBootEnv(UBOOTENV_USER_PREFERRED_HDR_TYPE, user_hdr_type);
        int32_t allowedHdrType = atoi(user_hdr_type);
        if (ret) {
            allowedHdrType = atoi(user_hdr_type);
        } else {
            allowedHdrType = 0; //all hdr enable as default
        }

        //force SDR when autoAllowedHdrTypes are empty on system-preferred conversion
        uint32_t userHdrType = 0;
        userHdrType = userHdrType | (1 << HAL_HDR_DOLBY_VISION);
        userHdrType = userHdrType | (1 << HAL_HDR_HDR10);
        userHdrType = userHdrType | (1 << HAL_HDR_HLG);
        userHdrType = userHdrType | (1 << DRM_INVALID);
        string keyValue = std::to_string(userHdrType);
        if (isAuto && strstr (user_hdr_type, keyValue.c_str())) {
            mHdr_priority = MESON_G_SDR;
            outHdrConversionType = DRM_INVALID;
            mHdr_policy = MESON_HDR_POLICY_SINK;
        } else {
            bool containDVType = false, containHDR10Type = false,
                 containHLGType = false, containSDRType = false;

            MESON_LOGD("allowedHdrType 0x%x ", allowedHdrType);
            if (!(allowedHdrType & (1 << HAL_HDR_DOLBY_VISION)))
                containDVType = true;
            if (!(allowedHdrType & (1 << HAL_HDR_HDR10)))
                containHDR10Type = true;
            if (!(allowedHdrType & (1 << HAL_HDR_HLG)))
                containHLGType = true;
            if (!(allowedHdrType & (1 << DRM_INVALID)))
                containSDRType = true;

            drm_hdr_capabilities hdrCaps;
            mConnector->getHdrCapabilities(&hdrCaps);

            if (isAuto) {
                if (hdrCaps.DolbyVisionSupported) {
                    if (containHLGType && containHDR10Type && containDVType) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_DV_HDR10_HLG;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_DOLBY_VISION);
                    } else if (containHDR10Type && containDVType) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_DV_HDR10;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_DOLBY_VISION);
                    } else if (containHLGType && containDVType) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_DV_HLG;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_DOLBY_VISION);
                    } else if (containHLGType && containHDR10Type) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_HDR10_HLG;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HDR10);
                    } else if (containDVType) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_DV;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_DOLBY_VISION);
                    } else if (containHDR10Type) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_HDR10;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HDR10);
                    } else if (containHLGType) {
                        mHdr_priority = MESON_G_HLG;
                        mHdr_policy   = MESON_HDR_POLICY_SOURCE;      //follow source due to dv can't HLG output
                    } else {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_SDR;
                        outHdrConversionType = DRM_INVALID;  //force sdr
                    }
                } else {
                    if (containHLGType && containHDR10Type) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_HDR10_HLG;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HDR10);
                    } else if (containHDR10Type) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_HDR10;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HDR10);
                    } else if (containHLGType) {
                        mHdr_policy   = MESON_HDR_POLICY_SOURCE;     //follow source due to dv can't HLG output
                        mHdr_priority = MESON_G_HLG;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HLG);
                    } else {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_SDR;
                        outHdrConversionType = DRM_INVALID; //force sdr
                    }
                }
            } else {
                if (containDVType) {
                    if (hdrCaps.DolbyVisionSupported) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_DV;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_DOLBY_VISION);
                    } else {
                        mHdr_policy = MESON_HDR_POLICY_SOURCE;     //follow source
                        mHdr_priority = MESON_G_DV_HDR10_HLG;
                    }
                } else if (containHDR10Type) {
                    if (hdrCaps.HDR10Supported) {
                        mHdr_policy   = MESON_HDR_POLICY_SINK;
                        mHdr_priority = MESON_G_HDR10;
                        outHdrConversionType = static_cast<int32_t>(HAL_HDR_HDR10);
                    } else {
                        mHdr_policy   = MESON_HDR_POLICY_SOURCE;  //follow source
                        mHdr_priority = MESON_G_DV_HDR10_HLG;
                    }
                } else if (containHLGType) {
                    if (hdrCaps.HLGSupported) {
                        if (isMboxSupportDolbyVision()) {
                            mHdr_policy = MESON_HDR_POLICY_SOURCE;     //follow source due to dv can't HLG output
                            mHdr_priority = MESON_G_HLG;
                        } else {
                            mHdr_policy   = MESON_HDR_POLICY_SINK;
                            mHdr_priority = MESON_G_HLG;
                            outHdrConversionType = static_cast<int32_t>(HAL_HDR_HLG);
                        }
                    } else {
                        mHdr_policy   = MESON_HDR_POLICY_SOURCE;  //follow source
                        mHdr_priority = MESON_G_DV_HDR10_HLG;
                    }
                } else {
                    mHdr_policy   = MESON_HDR_POLICY_SINK;
                    mHdr_priority = MESON_G_SDR;
                    outHdrConversionType = DRM_INVALID;  //force sdr
                }
            }

            //need return invalid type when connect sdr TV
            if (!isTvSupportDolbyVision() &&
                !isTvSupportHDR()) {
                outHdrConversionType = DRM_INVALID;  //force sdr
            }
        }
    }

    MESON_LOGD("hdr_policy:%d hdr_priority:0x%x outHdrConversionType:%d ", mHdr_policy, mHdr_priority, outHdrConversionType);
    return outHdrConversionType;
}

/*
 * return value bigger than 0 means error happened
 */
int32_t ModePolicy::setHdrConversionPolicy(bool passthrough, int32_t forceType) {
    int32_t ret = HWC2_ERROR_NONE;
    bool modeChanged = false;
    MESON_LOGD("%s passthrough %d forceType %s",
            __func__, passthrough, hdrConversionTypeToString(forceType));

    if (passthrough || (forceType == -1)) {
        setBootEnv(UBOOTENV_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);

        char hdr_priority[MESON_MODE_LEN] = {0};
        sprintf(hdr_priority, "%d", mHdr_priority);
        setBootEnv(UBOOTENV_HDR_PRIORITY, hdr_priority);
        // set current hdmi mode
        getDisplayMode(mCurrentMode);
        modeChanged = setSourceOutputMode(mCurrentMode);
    } else {
        std::string type = FORCE_DV;
        meson_hdr_priority_e priority = MESON_DOLBY_VISION_PRIORITY;
        switch (forceType) {
            case DRM_DOLBY_VISION: {
                priority = MESON_DOLBY_VISION_PRIORITY;
                type = FORCE_DV;
                break;
            }
            case DRM_HDR10:{
                priority = MESON_HDR10_PRIORITY;
                type = FORCE_HDR10;
                break;
            }
            case DRM_HLG:{
                priority = MESON_HDR10_PRIORITY;
                type = FORCE_HLG;
                break;
            }
            case DRM_INVALID: {
                priority = MESON_SDR_PRIORITY;
                type = DV_DISABLE_FORCE_SDR;
                break;
            }
            default:
                MESON_LOGE("setHdrConversionStrategy: error type[%d]", forceType);
                ret = HWC2_ERROR_UNSUPPORTED;
                break;
        }

        if (!ret) {
            getDisplayMode(mCurrentMode);
            meson_mode_set_policy_input(mModeConType, &mConData);
            if (!meson_mode_support_mode(mModeConType, priority, mCurrentMode)) {
                char hdr_policy[MESON_MODE_LEN] = {0};
                sprintf(hdr_policy, "%d", mHdr_policy);
                setBootEnv(UBOOTENV_HDR_POLICY, hdr_policy);

                char hdr_priority[MESON_MODE_LEN] = {0};
                sprintf(hdr_priority, "%d", mHdr_priority);
                setBootEnv(UBOOTENV_HDR_PRIORITY, hdr_priority);

                modeChanged = setSourceOutputMode(mCurrentMode);
            } else {
                MESON_LOGW("%s mode check failed\n", __func__);
                ret = HWC2_ERROR_UNSUPPORTED;
            }
        }
    }

    if (!modeChanged) {
        ret = -EEXIST;
    }

    mInitialized = true;
    return ret;
}

/*
* apply setting
*/
bool ModePolicy::applyDisplaySetting(bool force) {
    //quiescent boot need not output
    bool quiescent = sys_get_bool_prop("ro.boot.quiescent", false);

    MESON_LOGI("quiescent_mode is %d\n", quiescent);
    if (quiescent  && (mState == OUTPUT_MODE_STATE_INIT)) {
        MESON_LOGI("don't need to setting hdmi when quiescent mode\n");
        return false;
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
        MESON_LOGI("get uenv frc policy is %s and current value is %s\n",frac_rate_policy, cur_frac_rate_policy);
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
    getDisplayAttribute(DISPLAY_HDR_POLICY, cur_hdr_policy);
    MESON_LOGI("cur hdr policy:%s\n", cur_hdr_policy.c_str());

    std::string cur_hdr_force_mode;
    getDisplayAttribute(DISPLAY_FORCE_HDR_MODE, cur_hdr_force_mode);
    MESON_LOGI("cur hdr force mode:%s\n", cur_hdr_force_mode.c_str());

    std::string cur_dv_mode;
    getDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, cur_dv_mode);

    std::string cur_dv_policy;
    getDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, cur_dv_policy);
    MESON_LOGI("cur dv policy:%s\n", cur_dv_policy.c_str());

    char hdr_force_mode[MESON_MODE_LEN] = {0};
    gethdrforcemode(hdr_force_mode);

    char hdr_policy[MESON_MODE_LEN] = {0};
    getHdrStrategy(hdr_policy);

    if (!isDolbyVisionEnable()) {
        if (strstr(cur_hdr_policy.c_str(), hdr_policy) == NULL) {
            MESON_LOGI("set hdr policy from:%s to %s\n", cur_hdr_policy.c_str(), hdr_policy);
            hdr_policy_change = true;
        } else if (!strcmp(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]) && (strstr(cur_hdr_force_mode.c_str(), hdr_force_mode) == NULL)) {
            MESON_LOGI("set hdr force mode from:%s to %s\n", cur_hdr_force_mode.c_str(), hdr_force_mode);
            hdr_policy_change = true;
        }
    } else {
        if (strstr(cur_dv_policy.c_str(), hdr_policy) == NULL) {
            MESON_LOGI("set dv policy from:%s to %s\n", cur_dv_policy.c_str(), hdr_policy);
            hdr_policy_change = true;
        } else if ((mSceneOutInfo.dv_type != DOLBY_VISION_SET_DISABLE)
                && (!strcmp(hdr_policy, DV_POLICY_FORCE_MODE)
                    && (strstr(meson_dvModeTypeToString(cur_dv_mode.c_str()), hdr_force_mode) == NULL))) {
            MESON_LOGI("set dv force mode from:%s to %s\n", meson_dvModeTypeToString(cur_dv_mode.c_str()), hdr_force_mode);
            hdr_policy_change = true;
        }
    }

    //update hdr priority
    bool hdr_priority_change = false;
    meson_hdr_priority_e cur_hdr_priority;
    cur_hdr_priority = (meson_hdr_priority_e)getCurrentHdrPriority();

    meson_hdr_priority_e hdr_priority;
    hdr_priority = (meson_hdr_priority_e)getHdrPriority();

    if (cur_hdr_priority != hdr_priority) {
        SYS_LOGI("set hdr priority from:%x to %x\n", cur_hdr_priority, hdr_priority);
        hdr_priority_change = true;
    }

    // 4. check amdolby vision
    int  dv_type  = DOLBY_VISION_SET_DISABLE;
    bool dv_change  = false;
    bool dvmode_change = false;

    dv_type   = mSceneOutInfo.dv_type;
    dv_change = checkDolbyVisionStatusChanged(dv_type);
    if (isMboxSupportDolbyVision() && dv_change) {
        //4.1 set avmute when signal change at boot
        if ((OUTPUT_MODE_STATE_INIT == mState)
            && (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]))) {
            sysfs_set_string(DISPLAY_HDMI_AVMUTE_SYSFS, "1");
        }
        //4.2 Set dv_type by scene
        //4.2.1 set dvmode_change to true when dv change at UI switch
        //set avmute-close phy>-set hdr/dv policy>set mode-clear avmute ---enable hdcp
        if (OUTPUT_MODE_STATE_SWITCH == mState) {
            dvmode_change = true;
        } else {
            //4.2.2 In other scenarios, set DV directly
            if (DOLBY_VISION_SET_DISABLE != dv_type) {//enable or disable dolby vision core
                enableDolbyVision(dv_type);
            } else {
                disableDolbyVision(dv_type);
            }
        }
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

    // 5.1 check connector ready
    bool connectorReady = true;
    if (mConnector && !mConnector->isReady()) {
        connectorReady = false;
        MESON_LOGI("connector not ready");
    }

    // if support qms, don't apply changes if only refrsh rate change during initialization
    // let framework handle it
    if (mConnector->supportVrr() && !mInitialized) {
        MESON_LOGI("QMS initialized, attr_change:%d, hdr policy:%d, priority:%d, ready:%d, modechange:%d, frac:%d",
                attr_change, hdr_policy_change,
                hdr_priority_change, connectorReady,
                modeChange, frac_rate_policy_change);

        if (!attr_change && !hdr_policy_change && !hdr_priority_change && connectorReady) {
            if ((!modeChange && frac_rate_policy_change) ||
                    (modeChange && isSameGroup(curDisplayMode, final_displaymode))) {
                    MESON_LOGD("support qms and initialized, let framework handle it");
                    return false;
            }
        }
    }

    //6. check any change
    bool isNeedChange = false;

    if (modeChange || attr_change || frac_rate_policy_change || hdr_policy_change || hdr_priority_change || !connectorReady || dvmode_change) {
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
        //apply hdr policy to driver sysfs
        if (hdr_policy_change) {
            if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK])) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
                if (isDolbyVisionEnable()) {
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
                }
            } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE])) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
                if (isDolbyVisionEnable()) {
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
                }
            } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]) || strstr(hdr_policy, DV_POLICY_FORCE_MODE)) {
                char hdr_force_mode[MESON_MODE_LEN] = {0};
                gethdrforcemode(hdr_force_mode);
                setDisplayAttribute(DISPLAY_FORCE_HDR_MODE, hdr_force_mode);
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]);
                if (isDolbyVisionEnable()) {
                    setDisplayAttribute(DISPLAY_DOLBY_VISION_POLICY, DV_POLICY_FORCE_MODE);
                    if (strstr(hdr_force_mode, FORCE_DV)) {
                        setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_DV);
                    } else if (strstr(hdr_force_mode, FORCE_HDR10)) {
                        setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, FORCE_HDR10);
                    } else if (strstr(hdr_force_mode, DV_DISABLE_FORCE_SDR)) {
                        // 8bit or not
                        std::string cur_ColorAttribute;
                        getDisplayAttribute(DISPLAY_HDMI_COLOR_ATTR, cur_ColorAttribute);
                        if (cur_ColorAttribute.find("8bit", 0) != std::string::npos) {
                            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_ENABLE_FORCE_SDR_8BIT);
                        } else {
                            setDisplayAttribute(DISPLAY_DOLBY_VISION_MODE, DV_ENABLE_FORCE_SDR_10BIT);
                        }
                    }
                }
            }
        }

        //apply hdr priority to driver sysfs
        if (hdr_priority_change) {
            char tmp[MESON_MODE_LEN] = {0};
            sprintf(tmp, "%d", hdr_priority);
            setDisplayAttribute(DISPLAY_HDR_PRIORITY, tmp);
        }

        //apply enable or disable dolby vision core
        if (dvmode_change) {
            if (DOLBY_VISION_SET_DISABLE != dv_type) {
                enableDolbyVision(dv_type);
            } else {
                disableDolbyVision(dv_type);
            }
        }

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

    saveHdmiParamToEnv();

    return isNeedChange;
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
            if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK])) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SINK]);
            } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE])) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);
            } else if (strstr(hdr_policy, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE])) {
                setDisplayAttribute(DISPLAY_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_FORCE]);
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
    char hdmimode[MESON_MODE_LEN] = {0};

    if (DISPLAY_TYPE_TV == mDisplayType) {
        return false;
    }

    if (!getBootEnv(UBOOTENV_HDMIMODE, hdmimode) || (strstr(hdmimode, "hz") == NULL)) {
        //if hdmimode is empty or no resolution is saved to enable the auto to best strategy
        return true;
    }

    return !getBootEnv(UBOOTENV_ISBESTMODE, isBestMode) || strcmp(isBestMode, "true") == 0;
}

bool ModePolicy::isBestColorSpace() {
    bool ret = false;
    char user_colorattr[MESON_MODE_LEN] = {0};
    if (DISPLAY_TYPE_TV == mDisplayType) {
        return false;
    }

    ret = getBootEnv(UBOOTENV_USER_COLORATTRIBUTE, user_colorattr);

    if (!ret) {
        return true;
    } else if (strstr(user_colorattr, "bit") == NULL) {
        return true;
    }

    return false;
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
    char curDisplayMode[MESON_MODE_LEN]    = {0};
    getDisplayMode(curDisplayMode);
    if (mConnector->getType() == DRM_MODE_CONNECTOR_VIRTUAL) {
        if (strcmp(curDisplayMode, "dummy_l") != 0 || !mConnector->isReady()) {
            setDisplayMode("dummy_l");
        }
        return;
    } else if ((isHdmiUsed() == true) && (isConnected() == false)) {
        if (isVMXCertification()) {
           setDisplayMode("576cvbs");
        } else {
           setDisplayMode("dummy_l");
        }
        return;
    } else if (mConnector->getType() == DRM_MODE_CONNECTOR_TV){
        char cvbsOutmode[MESON_MODE_LEN] = {0};
        if (getBootEnv(UBOOTENV_CVBSMODE, cvbsOutmode)) {
            if (strcmp(cvbsOutmode, curDisplayMode) != 0) {
                setDisplayMode(cvbsOutmode);
            }
        } else {
            setDisplayMode("576cvbs");
        }
        return;
    }

    //2. update hdmi info when boot and hdmi plug/suspend/resume
    if ((state == OUTPUT_MODE_STATE_INIT) ||
        (state == OUTPUT_MODE_STATE_POWER)) {
        memset(&mConData, 0, sizeof(meson_policy_in));
        memset(&mDvInfo, 0, sizeof(hdmi_dv_info_t));
        mState = state;
        mConData.state = static_cast<meson_mode_state>(state);

        auto hdrConversionType = -1;
        hdrConversionType = getPreferredHdrConversionType();
        //Match content Dynamic range or invalid case
        if (hdrConversionType == -1) {
            setBootEnv(UBOOTENV_HDR_POLICY, MESON_HDR_POLICY[MESON_HDR_POLICY_SOURCE]);

            char hdr_priority[MESON_MODE_LEN] = {0};
            sprintf(hdr_priority, "%d", mHdr_priority);
            setBootEnv(UBOOTENV_HDR_PRIORITY, hdr_priority);
        } else {
            char hdr_policy[MESON_MODE_LEN] = {0};
            sprintf(hdr_policy, "%d", mHdr_policy);
            setBootEnv(UBOOTENV_HDR_POLICY, hdr_policy);

            char hdr_priority[MESON_MODE_LEN] = {0};
            sprintf(hdr_priority, "%d", mHdr_priority);
            setBootEnv(UBOOTENV_HDR_PRIORITY, hdr_priority);
        }

        getConnectorData(&mConData, &mDvInfo);

        strcpy(mConData.cur_displaymode, mConData.con_info.ubootenv_hdmimode);
     }

    //3. hdmi edid parse error and hpd = 1
    //set default reolsution and color format
    if ((isHdmiEdidParseOK() == false) &&
        (isConnected() == true)) {
        setDefaultMode();
        return;
    }

    // TOD sceneProcess
    if (isBestPolicy()) {
        mPolicy = MESON_POLICY_BEST;
    } else {
        mPolicy = MESON_POLICY_INVALID;
    }

    meson_mode_set_policy(mModeConType, mPolicy);
    meson_mode_set_policy_input(mModeConType, &mConData);
    meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

    //5. apply settings to driver
    applyDisplaySetting();
}

bool ModePolicy::setSourceOutputMode(const char* outputmode, bool force) {
    std::lock_guard<std::mutex> lock(mMutex);
    bool ret = true;

    if (DISPLAY_TYPE_TV == mDisplayType) {
        setSinkOutputMode(outputmode, false);
    } else {

        getConnectorUserData(&mConData, &mDvInfo);

        getHdrUserInfo(&mConData.hdr_info);

        mState = OUTPUT_MODE_STATE_SWITCH;
        mConData.state = static_cast<meson_mode_state>(mState);

        strcpy(mConData.cur_displaymode, outputmode);
        meson_mode_set_policy_input(mModeConType, &mConData);
        meson_mode_get_policy_output(mModeConType, &mSceneOutInfo);

        ret = applyDisplaySetting(force);
    }

    return ret;
}

void ModePolicy::getConnectorUserData(struct meson_policy_in* data, hdmi_dv_info_t *dinfo) {
    if (!data || !dinfo) {
        MESON_LOGE("%s data is NULL\n", __FUNCTION__);
        return;
    }

    bool ret = false;

    //hdmi color space best policy flag
    data->con_info.is_bestcolorspace = isBestColorSpace();

    MESON_LOGI("isbestColorspace:%d\n",
            data->con_info.is_bestcolorspace);

    getDisplayMode(mCurrentMode);
    ret = getBootEnv(UBOOTENV_HDMIMODE, data->con_info.ubootenv_hdmimode);
    if (!ret) {
        //if env is null,use none as default value
        strcpy(data->con_info.ubootenv_hdmimode, "none");
    }
    getBootEnv(UBOOTENV_CVBSMODE, data->con_info.ubootenv_cvbsmode);
    MESON_LOGI("hdmi_current_mode:%s, ubootenv hdmimode:%s cvbsmode:%s\n",
            mCurrentMode,
            data->con_info.ubootenv_hdmimode,
            data->con_info.ubootenv_cvbsmode);

    ret = getBootEnv(UBOOTENV_USER_COLORATTRIBUTE, data->con_info.ubootenv_colorattr);
    if (!ret) {
        //if env is null,use none as default value
        strcpy(data->con_info.ubootenv_colorattr, "none");
    }
    MESON_LOGI("ubootenv_colorattribute:%s\n",
            data->con_info.ubootenv_colorattr);

    //if no dolby_status env set to std for enable amdolby vision
    //if box support amdolby vision
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
    ret = getBootEnv(UBOOTENV_USER_DV_TYPE, ubootenv_dv_type);
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

bool ModePolicy::findBrrMode(drm_mode_info_t &currentMode, drm_mode_info_t &brrMode) {
    brrMode = currentMode;

    if (mConnector->supportVrr() == false)
        return false;

    for (auto it = mModes.begin(); it != mModes.end(); it++) {
        auto cfg = it->second;
        if (cfg.groupId == brrMode.groupId) {
            if (cfg.refreshRate > brrMode.refreshRate) {
                brrMode = cfg;
            }
        }
    }

    return true;
}

bool ModePolicy::findBrrMode(const char *currentMode, drm_mode_info_t &brrMode) {
    if (!currentMode) {
        return false;
    }

    if (mConnector->supportVrr() == false)
        return false;

    /* find current drm mode */
    std::map<uint32_t, drm_mode_info_t>::iterator curIt = mModes.end();
    for (auto it = mModes.begin(); it != mModes.end(); it++) {
        auto cfg = it->second;
        if (!strcmp(cfg.name, currentMode)) {
            curIt = it;
            break;
        }
    }

    if (curIt == mModes.end()) {
        return false;
    }

    return findBrrMode(curIt->second, brrMode);
}

bool ModePolicy::isSameGroup(const char *curDisplayMode, const char *finalDisplayMode) {
    if (!curDisplayMode || !finalDisplayMode) {
        return false;
    }

    std::map<uint32_t, drm_mode_info_t>::iterator curIt = mModes.end();
    std::map<uint32_t, drm_mode_info_t>::iterator finalIt = mModes.end();
    for (auto it = mModes.begin(); it != mModes.end(); it++) {
        auto cfg = it->second;
        if (!strcmp(cfg.name, curDisplayMode)) {
            curIt = it;
        }
        if (!strcmp(cfg.name, finalDisplayMode)) {
            finalIt = it;
        }
    }

    if (curIt == mModes.end() || finalIt == mModes.end()) {
        return false;
    }

    if (curIt->second.groupId == finalIt->second.groupId) {
        return true;
    }

    return false;
}

