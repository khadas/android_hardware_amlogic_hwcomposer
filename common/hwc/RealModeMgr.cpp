/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include <MesonLog.h>
#include <HwcConfig.h>
#include <hardware/hwcomposer2.h>
#include <systemcontrol.h>
#include <math.h>

#include "RealModeMgr.h"

#define DEFAULT_DPI (159)
#define CVBS_MODE_W (1280)
#define CVBS_MODE_H (720)

#define UBOOTENV_FRAC_RATE_POLICY  "ubootenv.var.frac_rate_policy"
#define FRC_POLICY_PROP            "vendor.sys.frc_policy"


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/*
 * sync with DISPLAY_MODE_LIST from systemcontrol/DisplayMode.cpp
 * and filter the interlace mode
 * */
static const char* DISPLAY_MODE_LIST[] = {
    "480p60hz",     // MODE_480P
    "480cvbs",      // MODE_480CVBS
    "576p50hz",     // MODE_576P
    "576cvbs",      // MODE_576CVBS
    "720p50hz",     // MODE_720P50HZ
    "720p60hz",     // MODE_720P
    "1280x720p100hz",    // MODE_720P100HZ
    "1280x720p120hz",    // MODE_720P120HZ
    "768p50hz",     // MODE_768P50HZ
    "768p60hz",     // MODE_768P60HZ
    "1080p24hz",    // MODE_1080P24HZ
    "1080p25hz",    // MODE_1080P25HZ
    "1080p30hz",    // MODE_1080P30HZ
    "1080p48hz",    // MODE_1080P48HZ
    "1080p50hz",    // MODE_1080P50HZ
    "1080p59hz",    // MODE_1080P59HZ
    "1080p60hz",    // MODE_1080P
    "1920x1080p100hz",   // MODE_1080P100HZ
    "1920x1080p120hz",   // MODE_1080P120HZ
    "3840x1080p100hz", // MODE_4K1K100HZ
    "3840x1080p119hz", // MODE_4K1K119HZ
    "3840x1080p120hz", // MODE_4K1K120HZ
    "3840x1080p200hz", // MODE_4K1K200HZ
    "3840x1080p239hz", // MODE_4K1K239HZ
    "3840x1080p240hz", // MODE_4K1K240HZ
    "3840x1080p288hz", // MODE_4K1K288HZ
    "2160p24hz",    // MODE_4K2K24HZ
    "2160p25hz",    // MODE_4K2K25HZ
    "2160p30hz",    // MODE_4K2K30HZ
    "2160p48hz",    // MODE_2160P48HZ
    "2160p50hz",    // MODE_4K2K50HZ
    "2160p59hz",    // MODE_4K2K59HZ
    "2160p60hz",    // MODE_4K2K60HZ
    "3840x2160p100hz",   // MODE_4K2K100HZ
    "3840x2160p119hz",   // MODE_4K2K119HZ
    "3840x2160p120hz",   // MODE_4K2K120HZ
    "3840x2160p144hz",   // MODE_4K2K144HZ
    "smpte24hz",    // MODE_4K2KSMPTE24HZ
    "smpte30hz",    // MODE_4K2KSMPTE30HZ
    "smpte50hz",    // MODE_4K2KSMPTE50HZ
    "smpte60hz",    // MODE_4K2KSMPTE60HZ
    "smpte100hz",   // MODE_4K2KSMPTE100HZ
    "smpte120hz",  // MODE_4K2KSMPTE120HZ
    "7680x4320p24hz",    // MODE_8K4K24HZ
    "7680x4320p25hz",    // MODE_8K4K25HZ
    "7680x4320p30hz",    // MODE_8K4K30HZ
    "7680x4320p48hz",    // MODE_8K4K48HZ
    "7680x4320p50hz",    // MODE_8K4K50HZ
    "7680x4320p60hz",    // MODE_8K4K60HZ
    "480x320p60hz"
    "640x480p60hz"
    "800x480p60hz"
    "800x600p60hz"
    "1024x600p60hz"
    "1024x768p60hz"
    "1280x480p60hz"
    "1280x800p60hz"
    "1280x1024p60hz"
    "1360x768p60hz"
    "1440x900p60hz"
    "1600x900p60hz"
    "1600x1200p60hz"
    "1680x1050p60hz"
    "1920x720p60hz"
    "1920x1200p60hz"
    "1920x1400p60hz"
    "2560x1080p60hz"
    "2560x1440p60hz"
    "2560x1600p60hz"
    "3440x1440p60hz"
    "panel",        // MODE_PANEL
    "pal_m",        // MODE_PAL_M
    "pal_n",        // MODE_PAL_N
    "ntsc_m",       // MODE_NTSC_M
};

static const drm_mode_info_t fakeInitialMode = {
    .name              = "FAKE_INITIAL_MODE",
    .dpiX              = DEFAULT_DPI,
    .dpiY              = DEFAULT_DPI,
    .pixelW            = 1920,
    .pixelH            = 1080,
    .refreshRate       = DEFAULT_REFRESH_RATE,
    .groupId           = 0,
};

RealModeMgr::RealModeMgr() {
    mLatestRealMode = fakeInitialMode;
    mDvEnabled = false;
    mCallOnHotPlug = true;
    mHwcFbWidth = 0;
    mHwcFbHeight = 0;
    mActiveConfigId = 0;
    mIsFakeSizeMode =false;
}

RealModeMgr::~RealModeMgr() {
}

hwc_modes_policy_t RealModeMgr::getPolicyType() {
    return REAL_MODE_POLICY;
}

const char * RealModeMgr::getName() {
    return "RealModeMgr";
}

void RealModeMgr::setFramebufferSize(uint32_t w, uint32_t h) {
    mHwcFbWidth = w;
    mHwcFbHeight =h;
}

void RealModeMgr::setDisplayResources(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector) {
    mConnector = connector;
    mCrtc = crtc;
}

int32_t RealModeMgr::updateActiveConfig(drm_mode_info_t activeMode) {
    for (auto it = mModes.begin(); it != mModes.end(); ++it) {
        if (strncmp(activeMode.name, it->second.name, DRM_DISPLAY_MODE_LEN) == 0 &&
            fabs(activeMode.refreshRate - it->second.refreshRate) < 1e-2) {
            mActiveConfigId = it->first;
            MESON_LOGV("%s activeConfigId = %d", __func__, mActiveConfigId);
            return HWC2_ERROR_NONE;
        }
    }

    mActiveConfigId = mModes.size() - 1;
    MESON_LOGD("%s failed to find [%s], default set activeConfigId to [%d]",
            __func__, activeMode.name, mActiveConfigId);

    return HWC2_ERROR_NONE;
}

void RealModeMgr::reset() {
    mModes.clear();
    mActiveConfigId = -1;
    mIsFakeSizeMode = false;
}

bool RealModeMgr::isFakeSizeMode() {
    return mIsFakeSizeMode;
}

bool RealModeMgr::isTvConnector() {
    uint32_t type = mConnector->getType();
    if (type == DRM_MODE_CONNECTOR_MESON_LVDS_A || type == DRM_MODE_CONNECTOR_MESON_LVDS_B ||
            type == DRM_MODE_CONNECTOR_MESON_LVDS_C || type == DRM_MODE_CONNECTOR_MESON_VBYONE_A ||
            type == DRM_MODE_CONNECTOR_MESON_VBYONE_B || type == DRM_MODE_CONNECTOR_LVDS ||
            type == LEGACY_NON_DRM_CONNECTOR_PANEL)
        return true;

    return false;
}

void RealModeMgr::mapToFakeSizeMode(drm_mode_info_t & mode, drm_mode_info_t & currentMode) {
    uint32_t fakePixelW = FB_SIZE_1080P_W;
    uint32_t fakePixelH = FB_SIZE_1080P_H;

    // cvbs mode need map to 720p
    if (mConnector->getType() == DRM_MODE_CONNECTOR_TV) {
        fakePixelW = CVBS_MODE_W;
        fakePixelH = CVBS_MODE_H;
    // if mode is DLG mode (4k1k), <= 120hz map to 4k mode, >120hz map to  1080p
    } else if (mode.pixelW == FB_SIZE_4K_W && mode.pixelH == FB_SIZE_1080P_H) {
        if (mode.refreshRate < 200) {
            fakePixelW = FB_SIZE_4K_W;
            fakePixelH = FB_SIZE_4K_H;
        } else {
            fakePixelW = FB_SIZE_1080P_W;
            fakePixelH = FB_SIZE_1080P_H;
        }
    // other map to FB size
    } else {
        HwcConfig::getFramebufferSize(0, fakePixelW, fakePixelH);
    }

    mode.dpiX = ((float)fakePixelW / mode.pixelW) * mode.dpiX;
    mode.dpiY = ((float)fakePixelH / mode.pixelH) * mode.dpiY;
    mode.pixelW = fakePixelW;
    mode.pixelH = fakePixelH;

    if (!strcmp(mode.name, currentMode.name) &&
        fabs(mode.refreshRate - currentMode.refreshRate) < 0.001) {
        currentMode = mode;
        mIsFakeSizeMode = true;
    }
}

void RealModeMgr::processModeList(std::map<uint32_t, drm_mode_info_t> & modeList,
        drm_mode_info_t & currentMode) {
    for (auto it = modeList.begin(); it != modeList.end(); ) {
        bool bRemove = false;
        bool bMap = false;
        auto & mode = it->second;

        // cvbs connector, map cvbs mode to fake size mode
        if (mConnector->getType() == DRM_MODE_CONNECTOR_TV) {
            if (strstr(mode.name,"cvbs") || !strcmp(mode.name, currentMode.name)) {
                bMap = true;
            } else {
                bRemove = true;
            }
        } else { // other connector
            // filter non 16:9 feature is enable
            if (HwcConfig::getModeCondition()) {
                MESON_LOGV("RealModeMgr::filter 16:9 mode enabled");
                if (!is16_9Mode(mode)) {
                    // if current mode is not 16:9 mode, we don't filter it
                    if (!strcmp(currentMode.name, mode.name)) {
                        bMap = true;
                    } else {
                        bRemove = true;
                    }
                }
            } else {
                // if mode is DLG mode (4k1k), need map it to 4k mode
                if (mode.pixelW == FB_SIZE_4K_W && mode.pixelH == FB_SIZE_1080P_H) {
                    bMap = true;
                } else {
                    if (strstr(mode.name, "768p") && isTvConnector()) {
                        bMap = true;
                    }
                }
            }
        }

        // need map to fake size Mode
        if (bMap)
            mapToFakeSizeMode(mode, currentMode);

        if (bRemove) {
            it = modeList.erase(it);
        } else {
            it++;
        }
    }
}

int32_t RealModeMgr::update() {
    // if mode changed by setActiveConfig then mode Id not changed
    // when setActiveConfig, no need reload display modes
    if (!mCallOnHotPlug)
        return HWC2_ERROR_NONE;

    std::lock_guard<std::mutex> lock(mMutex);
    bool useFakeMode = true;
    drm_mode_info_t realMode;
    std::map<uint32_t, drm_mode_info_t> connectorModeList;
    int largestUsedModeId = -1;

    if (HwcConfig::primaryHotplugEnabled()) {
        for (auto it = mModes.begin(); it != mModes.end(); ++it) {
            int configId = static_cast<int>(it->first);
            if (configId > largestUsedModeId)
                largestUsedModeId = configId;
            }
    }

    // if mode changed by setActiveConfig then mode Id not changed
    // otherwise change config Id sequential
    int nextModeId = largestUsedModeId + 1;
    MESON_LOGD("RealModeMgr::update: nextModeId:%d", nextModeId);

    /* reset ModeList */
    reset();
    if (mConnector->isConnected()) {
        mDvEnabled = mConnector->isDvEnable();
        MESON_LOGD("RealModeMgr::update mDvEnabled(%d)", mDvEnabled);
        mConnector->getModes(connectorModeList);
        int ret = mCrtc->getMode(realMode);
        if (ret == 0) {
            /*
             * If the current mode is dummy_l and connector has connected,
             * we are in suspend process. Do not report dummy_l to frameworks,
             * report the previous active mode instead
             */
            if (realMode.name[0] != 0 && strcmp(realMode.name, "dummy_l")) {
                MESON_LOGD("RealModeMgr::update get current mode:%s", realMode.name);
                processModeList(connectorModeList, realMode);

                for (auto it = connectorModeList.begin(); it != connectorModeList.end(); it++) {
                    if (!strcmp(realMode.name, it->second.name)) {
                        if (!strstr(realMode.name, "i") &&
                            !strstr(realMode.name, "pal") &&
                            !strstr(realMode.name, "ntsc")) {
                            mModes.emplace(nextModeId++, it->second);
                        }
                        useFakeMode = false;
                    } else if (isSupportModeForCurrentDevice(it->second)) {
                        mModes.emplace(nextModeId++, it->second);
                    }
                }

                if (strstr(realMode.name, "pal") ||
                    strstr(realMode.name, "ntsc") ||
                    strstr(realMode.name, "i")) {
                    dynamicMapMode(realMode.name);
                }

            }
        } else {
            MESON_LOGI("RealModeMgr::update could not get current mode:%d", ret);
        }
    } else {
        MESON_LOGD("RealModeMgr::update connector is not connected");
    }

    if (!useFakeMode) {
        mLatestRealMode = realMode;
        //todo: replace the displayid for dualDisplay
        sc_update_density(HWC_DISPLAY_PRIMARY, realMode.pixelW, realMode.pixelH);
    } else {
        strncpy(mLatestRealMode.name, "FAKE_PREVIOUS_MODE", DRM_DISPLAY_MODE_LEN);
        mModes.emplace(nextModeId++, mLatestRealMode);
    }

    MESON_LOGD("RealModeMgr::update use %s mode (%dx%d-%.2f)",
                useFakeMode ? "previous" : "current",
                mLatestRealMode.pixelW, mLatestRealMode.pixelH,
                mLatestRealMode.refreshRate);

    updateActiveConfig(mLatestRealMode);

    return HWC2_ERROR_NONE;
}

bool RealModeMgr::is16_9Mode(drm_mode_info_t mode) {
     if (!(mode.pixelW % 16) && !(mode.pixelH % 9) && (mode.pixelW / 16 == mode.pixelH / 9)) {
         return true;
     }
     return false;
}

int32_t RealModeMgr::getDisplayMode(drm_mode_info_t & mode) {
    return mCrtc->getMode(mode);
}

int32_t  RealModeMgr::getDisplayConfigs(
    uint32_t * outNumConfigs, uint32_t * outConfigs) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outNumConfigs = mModes.size();

    if (outConfigs) {
        std::map<uint32_t, drm_mode_info_t>::iterator it =
            mModes.begin();
        for (uint32_t index = 0; it != mModes.end(); ++it, ++index) {
            outConfigs[index] = it->first;
            MESON_LOGV("realmode getDisplayConfigs outConfig[%d]: %d %s.",
                    index, outConfigs[index], it->second.name);
        }
    }
    return HWC2_ERROR_NONE;
}

int32_t  RealModeMgr::getDisplayAttribute(
    uint32_t config, int32_t attribute, int32_t * outValue,
    int32_t caller __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::map<uint32_t, drm_mode_info_t>::iterator it;
    it = mModes.find(config);

    if (it != mModes.end()) {
        drm_mode_info_t curMode = it->second;
        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *outValue = curMode.pixelW;
                break;
            case HWC2_ATTRIBUTE_HEIGHT:
                *outValue = curMode.pixelH;
                break;
            case HWC2_ATTRIBUTE_VSYNC_PERIOD:
                if (HwcConfig::isHeadlessMode()) {
                    *outValue = 1e9 / HwcConfig::headlessRefreshRate();
                } else {
                    float vsync_scale = 1.0;
                    if (HwcConfig::getVsyncScaleFactor() > 0.0) {
                        vsync_scale = HwcConfig::getVsyncScaleFactor();
                    }
                    *outValue = vsync_scale * 1e9 / curMode.refreshRate;
                }
                break;
            case HWC2_ATTRIBUTE_DPI_X:
                *outValue = curMode.dpiX;
                break;
            case HWC2_ATTRIBUTE_DPI_Y:
                *outValue = curMode.dpiY;
                break;
            case HWC2_ATTRIBUTE_CONFIG_GROUP:
                *outValue = curMode.groupId;
                break;
            default:
                MESON_LOGE("Unknown display attribute(%d)", attribute);
                break;
        }
    } else {
        MESON_LOGE("[%s]: no support display config: %d, mModes size:%zu",
                __func__, config, mModes.size());
        return HWC2_ERROR_UNSUPPORTED;
    }

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::getActiveConfig(uint32_t * outConfig, int32_t caller __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outConfig = mActiveConfigId;

    MESON_LOGV("RealModeMgr:getActiveConfig:%d", *outConfig);

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::setActiveConfig(uint32_t config) {
    std::lock_guard<std::mutex> lock(mMutex);
    return setActiveConfigLocked(config);
}

int32_t RealModeMgr::setActiveConfigLocked(uint32_t config) {
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mModes.find(config);

    MESON_LOGD("[%s] %d", __func__, config);
    if (it != mModes.end()) {
        drm_mode_info_t cfg = it->second;

        if (strncmp(cfg.name, fakeInitialMode.name, DRM_DISPLAY_MODE_LEN) == 0) {
            MESON_LOGD("setActiveConfig default mode not supported");
            return HWC2_ERROR_NONE;
        }

        if (setModeLocked(cfg) != 0) {
            return HWC2_ERROR_SEAMLESS_NOT_ALLOWED;
        }
    } else {
        MESON_LOGE("set invalid active config (%d)", config);
        return HWC2_ERROR_NOT_VALIDATED;
    }

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::setPerferredMode(std::string mode) {
    MESON_LOGD("RealModeMgr %s, nextMode:%s", __func__, mode.c_str());

    std::lock_guard<std::mutex> lock(mMutex);

    if (!mConnector->isConnected()) {
        MESON_LOGE("No connector!");
        return HWC2_ERROR_NOT_VALIDATED;
    }

    //1. map mode
    dynamicMapMode(mode);

    //2. setActiveConfig for same resolution and refreshrate mode
    drm_mode_info_t curMode;
    curMode.refreshRate = DEFAULT_REFRESH_RATE;
    mCrtc->getMode(curMode);
    drm_mode_info_t desiredMode;
    for (auto it : mModes) {
        if (strcmp(it.second.name, mode.c_str()) == 0 &&
            mConnector->checkFracMode(it.second) == mConnector->checkFracMode(curMode)) {
            desiredMode = it.second;
            break;
        }
    }
    if (strstr(desiredMode.name, "576cvbs") || strstr(desiredMode.name, "pal_n")) {
        desiredMode.pixelW = 720;
        desiredMode.pixelH = 576;
    } else if (strstr(desiredMode.name, "480cvbs") || strstr(desiredMode.name, "pal_m") || strstr(desiredMode.name, "ntsc_m")) {
        desiredMode.pixelW = 720;
        desiredMode.pixelH = 480;
    }
    MESON_LOGD("curMode %s %d %d  desiredMode %s %d %d", curMode.name, curMode.pixelW, curMode.pixelH, desiredMode.name,
    desiredMode.pixelW, desiredMode.pixelH);
    if (desiredMode.pixelW == curMode.pixelW && desiredMode.pixelH == curMode.pixelH
        && abs(desiredMode.refreshRate - curMode.refreshRate) < 0.001) {
        setActiveConfigLocked(mActiveConfigId);
    }

    return HWC2_ERROR_NONE;
}

void RealModeMgr::dynamicMapMode(std::string mode) {
    std::string dst("");
    if (mode.find("pal") != -1) {
        dst = "pal";
    } else if (mode.find("ntsc") != -1) {
        dst = "ntsc";
    } else if (mode.find("cvbs") != -1) {
        dst = "cvbs";
    } else if (mode.find("i") != -1) {
        dst = "i";
    } else if (mode.find("p") != -1) {
        dst = "p";
    }
    std::map<uint32_t, drm_mode_info_t> connecterModeList;
    mConnector->getModes(connecterModeList);

    for (auto iterMode = mModes.begin(); iterMode != mModes.end(); ++iterMode) {
        drm_mode_info_t itmode = iterMode->second;
        std::string modeName(itmode.name);
        if (strstr(itmode.name, "576cvbs") || strstr(itmode.name, "pal_n")) {
            itmode.pixelW = 720;
            itmode.pixelH = 576;
        } else if (strstr(itmode.name, "480cvbs") || strstr(itmode.name, "pal_m") || strstr(itmode.name, "ntsc_m")) {
            itmode.pixelW = 720;
            itmode.pixelH = 480;
        }
        for (auto iterConnector : connecterModeList) {
            std::string connectorModeName(iterConnector.second.name);
            if (abs(itmode.refreshRate - iterConnector.second.refreshRate) < 0.001 &&
                    itmode.pixelW == iterConnector.second.pixelW &&
                    itmode.pixelH == iterConnector.second.pixelH &&
                    connectorModeName.find(dst) != -1 ) {
                strlcpy(iterMode->second.name, iterConnector.second.name, sizeof(iterMode->second.name));
                iterMode->second.groupId = iterConnector.second.groupId;
                MESON_LOGD("mode %s:%d map to %s:%d", itmode.name, itmode.groupId,
                        iterMode->second.name, iterMode->second.groupId);
                break;
            }
        }
    }
}

// The request config is the same group of the latest active config
bool RealModeMgr::isSeamlessSwitch(uint32_t config) {
    std::map<uint32_t, drm_mode_info_t>::iterator it = mModes.find(config);

    // same as the current active config
    if (config == mActiveConfigId) {
        return false;
    }

    if (it != mModes.end()) {
        drm_mode_info_t cfg = it->second;
        if (cfg.groupId == mLatestRealMode.groupId)
            return true;
    }

    return false;
}

// TODO: remove the sc related default boot config api when mesondisplay sdk is ready
int32_t RealModeMgr::getPreferredBootConfig(int32_t* outConfig) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outConfig = mActiveConfigId;

    std::string prefMode;
    int32_t ret = -1;
    if (mModePolicy.get()) {
        ret = mModePolicy->getPreferredBootConfig(prefMode);
    } else {
        ret = sc_getPreferredDisplayConfig(HWC_DISPLAY_PRIMARY, prefMode);
    }

    if (!ret) {
        for (auto it = mModes.begin(); it != mModes.end(); ++it) {
            if (strncmp(prefMode.c_str(), it->second.name, DRM_DISPLAY_MODE_LEN) == 0 &&
                    mConnector->checkFracMode(it->second)) {
                *outConfig = it->first;
                [[maybe_unused]] drm_mode_info_t cfg = it->second;
                MESON_LOGD("%s outConfig = %d (%dx%d@%f)", __func__, *outConfig,
                        cfg.pixelW, cfg.pixelH, cfg.refreshRate);
                break;
            }
        }
    }

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::setBootConfig(int32_t config) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mModes.find(config);

    if (it != mModes.end()) {
        drm_mode_info_t cfg = it->second;

        if (strncmp(cfg.name, fakeInitialMode.name, DRM_DISPLAY_MODE_LEN) == 0) {
            MESON_LOGD("set boot config not supported");
            return HWC2_ERROR_BAD_PARAMETER;
        }

        MESON_LOGD("%s %dx%d@%.2f", __func__, cfg.pixelW, cfg.pixelH, cfg.refreshRate);
        std::string dispmode(cfg.name);

        if (mModePolicy.get()) {
            mModePolicy->setBootConfig(cfg);
        } else {
            sc_setBootDisplayConfig(HWC_DISPLAY_PRIMARY, dispmode);

            if (fabs(cfg.refreshRate - floor(cfg.refreshRate)) > 1e-2) {
                meson_mode_set_ubootenv(UBOOTENV_FRAC_RATE_POLICY, "1");
            } else {
                meson_mode_set_ubootenv(UBOOTENV_FRAC_RATE_POLICY, "0");
            }
        }
    } else {
        MESON_LOGE("set invalid boot config (%d)", config);
        return HWC2_ERROR_BAD_CONFIG;
    }

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::clearBootConfig() {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_LOGD("%s", __func__);
    if (mModePolicy.get()) {
        mModePolicy->clearBootConfig();
    } else {
        sc_clearBootDisplayConfig(HWC_DISPLAY_PRIMARY);
    }
    return HWC2_ERROR_NONE;
}

void RealModeMgr::setModePolicy(std::shared_ptr<IModePolicy> policy) {
    mModePolicy = policy;
    MESON_LOGD("%s", __func__);
}

bool RealModeMgr::isSupportModeForCurrentDevice(drm_mode_info_t mode) {
    // some hdmi output is not suitable for current device
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

int32_t RealModeMgr::setModeLocked(drm_mode_info_t & mode) {
    bool seamless = (mode.groupId == mLatestRealMode.groupId);
    mLatestRealMode = mode;

    MESON_LOGD("RealModeMgr::setActiveConfig setMode: %s, seamless:%d",
            mode.name, seamless);
    updateActiveConfig(mode);

    //todo: replace the displayid for dualDisplay
    sc_update_density(HWC_DISPLAY_PRIMARY, mLatestRealMode.pixelW, mLatestRealMode.pixelH);

    if (seamless) {
        mConnector->setMode(mode);
        // seamless mode switch, only vsync period change
        mCrtc->setMode(mode, seamless);
    }  else {
        mCallOnHotPlug = false;
        if (fabs(mode.refreshRate - floor(mode.refreshRate)) > 1e-2) {
            sc_set_property(FRC_POLICY_PROP, "1");
        } else {
            sc_set_property(FRC_POLICY_PROP, "0");
        }

        // set the display mode through systemControl
        // As it will need update the colorspace/colordepth too.
        std::string dispmode(mode.name);
        if (mModePolicy.get())
            mModePolicy->setActiveConfig(dispmode);
        else
            sc_set_display_mode(dispmode);
    }
    return 0;
}

void RealModeMgr::dump(String8 & dumpstr) {
    dumpstr.appendFormat("RealModeMgr:(%s)\n", mLatestRealMode.name);
    dumpstr.append("-----------------------------------------------------------"
        "--------------------------------------------------\n");
    dumpstr.append("|  CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   |      NAME      | GROUP_ID |\n");
    dumpstr.append("+-----------+------------------+-----------+------------+"
        "-----------+-----------+----------------+----------+\n");

    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mModes.begin();

    for (; it != mModes.end(); ++it) {
        int mode = it->first;
        drm_mode_info_t config = it->second;
        dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
            "   %5d   |   %5d   | %14s |    %2d    |\n",
            (mode == (int)mActiveConfigId) ? "*   " : "    ",
            mode,
            config.refreshRate,
            config.pixelW,
            config.pixelH,
            config.dpiX,
            config.dpiY,
            config.name,
            config.groupId);
    }
    dumpstr.append("-----------------------------------------------------------"
        "--------------------------------------------------\n");
}
