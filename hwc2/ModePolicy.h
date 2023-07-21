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
#include <string.h>

#include <HwDisplayConnector.h>

#include "mode_policy.h"
#include "mode_ubootenv.h"
#include "IModePolicy.h"
#include "HDCPTxAuth.h"

#include "DisplayAdapterLocal.h"
#include <pthread.h>

#define PROP_HDMIONLY                   "ro.vendor.platform.hdmionly"
#define PROP_SUPPORT_4K                 "ro.vendor.platform.support.4k"
#define PROP_SUPPORT_OVER_4K30          "ro.vendor.platform.support.over.4k30"
#define PROP_LCD_DENSITY                "ro.sf.lcd_density"
#define PROP_WINDOW_WIDTH               "const.window.w"
#define PROP_WINDOW_HEIGHT              "const.window.h"
#define PROP_HAS_CVBS_MODE              "ro.vendor.platform.has.cvbsmode"
#define PROP_BEST_OUTPUT_MODE           "ro.vendor.platform.best_outputmode"
#define PROP_BOOTVIDEO_SERVICE          "service.bootvideo"
#define PROP_DEEPCOLOR                  "vendor.sys.open.deepcolor" //default close this function, when reboot
#define PROP_DOLBY_VISION_FEATURE       "ro.vendor.platform.support.dolbyvision"
#define PROP_SUPPORT_DOLBY_VISION       "vendor.system.support.dolbyvision"
#define PROP_DOLBY_VISION_CERTIFICATION "persist.vendor.sys.dolbyvision.certification"
#define PROP_DOLBY_VISION_PRIORITY      "persist.vendor.sys.graphics.priority"
#define PROP_ALWAYS_DOLBY_VISION        "vendor.system.always.dolbyvision"
#define PROP_HDR_MODE_STATE             "persist.vendor.sys.hdr.state"
#define PROP_SDR_MODE_STATE             "persist.vendor.sys.sdr.state"
#define PROP_DISPLAY_SIZE_CHECK         "vendor.display-size.check"
#define PROP_ENABLE_SDR2HDR             "ro.vendor.sdr2hdr.enable"
#define PROP_HDMI_FRAMERATE_PRIORITY    "persist.vendor.sys.framerate.priority"
#define PROP_HDR_RESOLUTION_PRIORITY    "persist.vendor.hdr.resolution.priority"

#define PROP_DISPLAY_SIZE               "vendor.display-size"
#define PROP_DISPLAY_ALLM               "vendor.allm.support"
#define PROP_DISPLAY_GAME               "vendor.contenttype_game.support"
#define HDMI_FRC_POLICY_PROP            "vendor.sys.frc_policy"

#define HDR_MODE_OFF                    "0"
#define HDR_MODE_ON                     "1"
#define HDR_MODE_AUTO                   "2"

#define SDR_MODE_OFF                    "0"
#define SDR_MODE_AUTO                   "2"

#define SUFFIX_10BIT                    "10bit"
#define SUFFIX_12BIT                    "12bit"
#define SUFFIX_14BIT                    "14bit"
#define SUFFIX_RGB                      "rgb"

#define DEFAULT_DEEP_COLOR_ATTR         "444,8bit"
#define DEFAULT_420_DEEP_COLOR_ATTR     "420,8bit"

#define DEFAULT_EDID_CRCHEAD            "checkvalue: "

#define UBOOTENV_DIGITAUDIO             "ubootenv.var.digitaudiooutput"
#define UBOOTENV_TESTMODE               "ubootenv.var.testmode"
#define UBOOTENV_CVBSMODE               "ubootenv.var.cvbsmode"
#define UBOOTENV_OUTPUTMODE             "ubootenv.var.outputmode"
#define UBOOTENV_ISBESTMODE             "ubootenv.var.is.bestmode"
#define UBOOTENV_BESTDOLBYVISION        "ubootenv.var.bestdolbyvision"
#define UBOOTENV_EDIDCRCVALUE           "ubootenv.var.hdmichecksum"
#define UBOOTENV_HDMICOLORSPACE         "ubootenv.var.hdmi_colorspace"
#define UBOOTENV_HDMICOLORDEPTH         "ubootenv.var.hdmi_colordepth"
#define UBOOTENV_DOLBYSTATUS            "ubootenv.var.dolby_status"
/*
 * save user set dv mode
 * systemcontrol output dv signal base this value
 * 0:disable dv
 * 1:sink-led
 * 2:source-led
 */
#define UBOOTENV_USER_DV_TYPE                "ubootenv.var.user_prefer_dv_type"

#define UBOOTENV_DV_ENABLE              "ubootenv.var.dv_enable"
#define UBOOTENV_FRAC_RATE_POLICY       "ubootenv.var.frac_rate_policy"
#define UBOOTENV_HDR_PRIORITY           "ubootenv.var.hdr_priority"
#define UBOOTENV_SDR2HDR                "ubootenv.var.sdr2hdr"

#define PROP_DEEPCOLOR_CTL              "persist.sys.open.deepcolor" // 8, 10, 12
#define PROP_PIXFMT                     "persist.sys.open.pixfmt" // rgb, ycbcr
#define PROP_VMX                        "persist.vendor.sys.vmx"

#define SYS_DISPLAY_RESOLUTION          "/sys/class/video/device_resolution"

#define HDR_POLICY_SINK                 "0"
#define HDR_POLICY_SOURCE               "1"
#define HDR_POLICY_FORCE                "4"

#define DV_HDR_SINK_SOURCE_BYPASS       "0"
#define DV_HDR_SINK_PROCESS             "1"
#define DV_HDR_SOURCE_PROCESS           "2"
#define DV_HDR_SINK_SOURCE_PROCESS      "3"
#define DV_ENABLE_FORCE_SDR_10BIT       "4"
#define DV_ENABLE_FORCE_SDR_8BIT        "5"

#define DV_SINK_LED                     "0"
#define DV_SOURCE_LED                   "1"
#define FORCE_DV                        "2"
#define FORCE_HDR10                     "3"
#define FORCE_HLG                       "5"
#define DV_DISABLE_FORCE_SDR            "1"

#define MODE_480I                       "480i60hz"
#define MODE_480P                       "480p60hz"
#define MODE_480CVBS                    "480cvbs"
#define MODE_576I                       "576i50hz"
#define MODE_576P                       "576p50hz"
#define MODE_576CVBS                    "576cvbs"
#define MODE_720P50HZ                   "720p50hz"
#define MODE_720P                       "720p60hz"
#define MODE_768P                       "768p60hz"
#define MODE_720P100HZ                  "720p100hz"
#define MODE_720P120HZ                  "720p120hz"
#define MODE_1080P24HZ                  "1080p24hz"
#define MODE_1080P25HZ                  "1080p25hz"
#define MODE_1080P30HZ                  "1080p30hz"
#define MODE_1080I50HZ                  "1080i50hz"
#define MODE_1080P50HZ                  "1080p50hz"
#define MODE_1080I                      "1080i60hz"
#define MODE_1080P                      "1080p60hz"
#define MODE_1080P_100HZ                "1080p100hz"
#define MODE_1080P_120HZ                "1080p120hz"
#define MODE_4K2K24HZ                   "2160p24hz"
#define MODE_4K2K25HZ                   "2160p25hz"
#define MODE_4K2K30HZ                   "2160p30hz"
#define MODE_4K2K50HZ                   "2160p50hz"
#define MODE_4K2K60HZ                   "2160p60hz"
#define MODE_4K2K100HZ                  "2160p100hz"
#define MODE_4K2K120HZ                  "2160p120hz"
#define MODE_4K2KSMPTE                  "smpte24hz"
#define MODE_4K2KSMPTE30HZ              "smpte30hz"
#define MODE_4K2KSMPTE50HZ              "smpte50hz"
#define MODE_4K2KSMPTE60HZ              "smpte60hz"
#define MODE_4K2KSMPTE100HZ             "smpte100hz"
#define MODE_4K2KSMPTE120HZ             "smpte120hz"
#define MODE_PANEL                      "panel"
#define MODE_PAL_M                      "pal_m"
#define MODE_PAL_N                      "pal_n"
#define MODE_NTSC_M                      "ntsc_m"

#define MODE_1080P_PREFIX               "1080p"
#define MODE_4K2KSMPTE_PREFIX           "smpte"


#define DOLBY_VISION_SET_ENABLE_LL_RGB      3
#define DOLBY_VISION_SET_ENABLE_LL_YUV      2
#define DOLBY_VISION_SET_ENABLE             1
#define DOLBY_VISION_SET_DISABLE            0


#define DV_ENABLE                       "Y"
#define DV_DISABLE                      "N"

#define DV_POLICY_FOLLOW_SINK           "0"
#define DV_POLICY_FOLLOW_SOURCE         "1"
#define DV_POLICY_FORCE_MODE            "2"
#define DV_HDR10_POLICY                 "3"

#define DV_MODE_BYPASS                  "0x0"
#define DV_MODE_IPT_TUNNEL              "0x2"
#define DV_MODE_FORCE_HDR10             "0x3"
#define DV_MODE_FORCE_SDR10             "0x4"
#define DV_MODE_FORCE_SDR8              "0x5"

#define BYPASS_PROCESS                  "0"
#define SDR_PROCESS                     "1"
#define HDR_PROCESS                     "2"
#define DV_PROCESS                      "3"

#define FULL_WIDTH_1080                 1920
#define FULL_HEIGHT_1080                1080

typedef enum {
    OUTPUT_MODE_STATE_INIT               = 0,
    OUTPUT_MODE_STATE_POWER              = 1,//hot plug
    OUTPUT_MODE_STATE_SWITCH             = 2,//user switch the mode
    OUTPUT_MODE_STATE_SWITCH_ADAPTER     = 3,//video auto switch the mode
    OUTPUT_MODE_STATE_RESERVE            = 4,
    OUTPUT_MODE_STATE_ADAPTER_END        = 5 //end hint video auto switch the mode
} output_mode_state;

typedef enum {
    OUTPUT_CHANGE_BY_INIT               = 0,
    OUTPUT_CHANGE_BY_USER               = 1,
    OUTPUT_CHANGE_BY_PLUG               = 2,
    OUTPUT_CHANGE_BY_HWC                = 3
} output_change_reason;

typedef enum {
    HDMI_SINK_TYPE_NONE                 = 0,
    HDMI_SINK_TYPE_SINK                 = 1,
    HDMI_SINK_TYPE_REPEATER             = 2,
    HDMI_SINK_TYPE_RESERVE              = 3
} hdmi_sink_type;


typedef struct hdmi_dv_info {
    char ubootenv_dv_type[MESON_MODE_LEN];
    char dv_cap[MESON_MAX_STR_LEN];
    char dv_displaymode[MESON_MODE_LEN];
    char dv_deepcolor[MESON_DV_MODE_LEN];
    int  dv_type;
    char dv_enable[MESON_MODE_LEN];
    char dv_cur_displaymode[MESON_MODE_LEN];
    char dv_final_displaymode[MESON_MODE_LEN];
    char dv_final_deepcolor[MESON_MODE_LEN];
} hdmi_dv_info_t;

using ConnectorType = meson::DisplayAdapter::ConnectorType;

class ModePolicy : public IModePolicy {
public:
    ModePolicy();
    ModePolicy(std::shared_ptr<meson::DisplayAdapter> adapter, const uint32_t displayId);
    ~ModePolicy();

    int32_t bindConnector(std::shared_ptr<HwDisplayConnector> & connector) override;
    bool setPolicy(int32_t policy) override;
    int32_t initialize() override;
    void onHotplug(bool connected) override;
    void setActiveConfig(std::string mode);

    //user change display settings by UI
    int32_t clearUserDisplayConfig();
    int32_t setColorSpace(std::string &colorspace);
    int32_t setDvMode(std::string &dv_mode);

    int32_t getPreferredBootConfig(std::string &config);
    int32_t setBootConfig(std::string &config);
    int32_t clearBootConfig();

    int32_t setHdrConversionPolicy(bool passthrough, int32_t forceType, bool overWriteEnv = true);
    int32_t setAutoLowLatencyMode(bool enabled);

    void dump(String8 &dumpstr) override;

protected:
    int32_t setDisplayMode(std::string &dispmode);
    int32_t setDisplayMode(const char *mode);
    bool setDisplayAttribute(const std::string cmd, const std::string attribute);
    bool getDisplayAttribute(const std::string cmd, std::string& attribute);

private:
    // Uboot env
    bool getBootEnv(const char* key, char* value);
    int getBootenvInt(const char* key, int defaultVal);
    void setBootEnv(const char* key, const char* value);

    // HDR functions
    int32_t setHdrStrategy(int32_t policy, const char *type);
    void getHdrStrategy(char* value);
    int32_t setHdrPriority(int32_t type);
    int32_t getHdrPriority();
    void gethdrforcemode(char* value);
    bool isDolbyVisionEnable();
    bool isTvDolbyVisionEnable();
    bool isMboxSupportDolbyVision();
    bool isTvSupportDolbyVision();
    bool isTvSupportHDR();
    bool isLowPowerMode();
    bool isHdrResolutionPriority();

    void getDvCap(struct meson_hdr_info *data);
    void getHdrUserInfo(meson_hdr_info_t *data);

    void getHdmiEdidStatus(char* edidstatus, int32_t len);
    int getHdmiSinkType();
    void getHdmiDcCap(char* dc_cap, int32_t len);

    bool isFilterEdid();
    void setFilterEdidList(std::map<int, std::string> filterEdidList);
    bool initColorAttribute(char* supportedColorList, int len);
    bool isSupportHdmiMode(const char *hdmi_mode, const char *supportedColorList);
    void filterHdmiDispcap(meson_connector_info* data);
    /* color deep attr */
    bool isModeSupportDeepColorAttr(const char *mode, const char * color);

    bool isFrameratePriority();
    bool isSupport4K();
    bool isSupport4K30Hz();
    bool isSupportDeepColor();

    int32_t getConnectorData(struct meson_policy_in* data, hdmi_dv_info_t *dinfo);
    void getConnectorUserData(struct meson_policy_in* data, hdmi_dv_info_t *dinfo);

    void initHdrSdrMode();
    int updateDolbyVisionType(void);
    bool checkDolbyVisionDeepColorChanged(int state);
    bool getCurDolbyVisionState(int state, output_mode_state mode_state);
    void setHdrMode(const char* mode);
    void setSdrMode(const char* mode);
    bool checkDolbyVisionStatusChanged(int state);
    void saveHdmiParamToEnv();
    void enableDolbyVision(int DvMode);


    void initGraphicsPriority();
    bool isEdidChange();

    void disableDolbyVision(int DvMode);

    void getPosition(const char* curMode, int *position);
    void setPosition(const char* curMode, int left, int top, int width, int height);

    void setDigitalMode(const char* mode);
    bool getDisplayMode(char* mode);

    bool isMatchMode(char* curmode, const char* outputmode);

    // ALLM
    bool isTvSupportALLM();
    bool getContentTypeSupport(const char* type);
    bool getGameContentTypeSupport();
    bool getSupportALLMContentTypeList(std::vector<std::string> *supportModes);
    void setALLMMode(int state);
    bool isTvConnector();

    void applyDisplaySetting(bool force = false);

    void setSourceDisplay(output_mode_state state);
    void setSourceOutputMode(const char* outputmode, bool force = false);
    bool isVMXCertification();
    bool isConnected();
    bool isHdmiUsed(void);

    void setSinkDisplay(bool initState);
    void setSinkOutputMode(const char* outputmode, bool initState);

    void saveDeepColorAttr(const char* mode, const char* dcValue);
    void setTvDolbyVisionEnable(void);
    void setTvDolbyVisionDisable(void);
    int getDolbyVisionType();
    bool isHdmiEdidParseOK(void);
    bool isBestPolicy();
    bool isBestColorSpace();
    void setDefaultMode();

    // new added function
    void drmMode2MesonMode(meson_mode_info_t &mesonMode, drm_mode_info_t &drmMode);
    void getSupportedModes();
    bool isModeSupported(drm_mode_info_t mode);

protected:
    static void * threadMain(void * data);

private:
    std::shared_ptr<meson::DisplayAdapter> mAdapter;
    std::shared_ptr<HwDisplayConnector> mConnector;
    ConnectorType  mConnectorType;
    meson_connector_type_e mModeConType;

    struct meson_policy_in mConData;
    struct meson_policy_out mSceneOutInfo;
    enum meson_mode_policy mPolicy;
    output_change_reason mReason;
    output_mode_state mState;
    hdmi_dv_info_t mDvInfo;

    std::map<int, std::string> mFilterEdid;

    int32_t mDisplayType;
    pthread_mutex_t mEnvLock;

    char mCurrentMode[MESON_MODE_LEN];

    std::string mDefaultUI;

    int mDisplayWidth;
    int mDisplayHeight;
    std::mutex mMutex;

    // a thread to handle uevent
    pthread_t mThread;
    uint32_t mDisplayId;

    std::map<uint32_t, drm_mode_info_t> mModes;
    std::shared_ptr<HDCPTxAuth> mTxAuth;
    // for hdr control
};
