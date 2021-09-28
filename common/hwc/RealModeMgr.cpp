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

#define DEFUALT_DPI (159)
#define DEFAULT_REFRESH_RATE (60.0f)

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
    "768p60hz",     // MODE_768P
    "1080p24hz",    // MODE_1080P24HZ
    "1080p25hz",    // MODE_1080P25HZ
    "1080p30hz",    // MODE_1080P30HZ
    "1080p50hz",    // MODE_1080P50HZ
    "1080p60hz",    // MODE_1080P
    "2160p24hz",    // MODE_4K2K24HZ
    "2160p25hz",    // MODE_4K2K25HZ
    "2160p30hz",    // MODE_4K2K30HZ
    "2160p50hz",    // MODE_4K2K50HZ
    "2160p60hz",    // MODE_4K2K60HZ
    "smpte24hz",    // MODE_4K2KSMPTE
    "smpte30hz",    // MODE_4K2KSMPTE30HZ
    "smpte50hz",    // MODE_4K2KSMPTE50HZ
    "smpte60hz",    // MODE_4K2KSMPTE60HZ
    "panel",        // MODE_PANEL
    "pal_m",        // MODE_PAL_M
    "pal_n",        // MODE_PAL_N
    "ntsc_m",       // MODE_NTSC_M
};

static const drm_mode_info_t fakeInitialMode = {
    .name              = "FAKE_INITIAL_MODE",
    .dpiX              = DEFUALT_DPI,
    .dpiY              = DEFUALT_DPI,
    .pixelW            = 1920,
    .pixelH            = 1080,
    .refreshRate       = DEFAULT_REFRESH_RATE,
    .groupId           = 0,
};

RealModeMgr::RealModeMgr() {
    mPreviousMode = fakeInitialMode;
    mDvEnabled = false;
    mCallOnHotPlug = true;
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
            MESON_LOGV("%s aciveConfigId = %d", __func__, mActiveConfigId);
            return HWC2_ERROR_NONE;
        }
    }

    mActiveConfigId = mModes.size()-1;
    MESON_LOGD("%s failed to find [%s], default set activeConfigId to [%d]",
            __func__, activeMode.name, mActiveConfigId);

    return HWC2_ERROR_NONE;
}

void RealModeMgr::reset() {
    mModes.clear();
    mActiveConfigId = -1;
}

void RealModeMgr::resetTags() {
    mCallOnHotPlug = true;
};

int32_t RealModeMgr::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool useFakeMode = true;
    drm_mode_info_t realMode;
    std::map<uint32_t, drm_mode_info_t> connecterModeList;

    /* reset ModeList */
    reset();
    if (mConnector->isConnected()) {
        mDvEnabled = sc_is_dolby_version_enable();
        MESON_LOGD("RealModeMgr::update mDvEnabled(%d)", mDvEnabled);

        mConnector->getModes(connecterModeList);
        int ret = mCrtc->getMode(realMode);
        if (ret == 0) {
            if (realMode.name[0] != 0) {
                mCurMode = realMode;
                mPreviousMode = realMode;

                MESON_LOGD("RealModeMgr::update get current mode:%s", realMode.name);
                for (auto it = connecterModeList.begin(); it != connecterModeList.end(); it++) {
                    /* not filter the current mode */
                    if (!strcmp(mCurMode.name, it->second.name)) {
                        mModes.emplace(mModes.size(), it->second);
                        useFakeMode = false;
                    } else if (isSupportModeForCurrentDevice(it->second)) {
                        mModes.emplace(mModes.size(), it->second);
                    }
                }
            } else {
                MESON_LOGI("RealModeMgr::update could not get current mode");
            }
        } else {
            strncpy(mCurMode.name, "invalid", DRM_DISPLAY_MODE_LEN);
            MESON_LOGI("RealModeMgr::update could not get current mode:%d", ret);
        }
    } else {
        MESON_LOGD("RealModeMgr::update no connector");
    }

    if (useFakeMode) {
        mCurMode = mPreviousMode;
        MESON_LOGD("RealModeMgr::update use previous mode");
        strncpy(mCurMode.name, "FAKE_PREVIOUS_MODE", DRM_DISPLAY_MODE_LEN);
        mModes.emplace(mModes.size(), mCurMode);
    }

    updateActiveConfig(mCurMode);

    return HWC2_ERROR_NONE;
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
                    *outValue = 1e9 / curMode.refreshRate;
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

    return HWC2_ERROR_NONE;
}

int32_t RealModeMgr::setActiveConfig(uint32_t config) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mModes.find(config);

    MESON_LOGV("[%s] %d", __func__, config);
    if (it != mModes.end()) {
        drm_mode_info_t cfg = it->second;

        updateActiveConfig(cfg);
        if (strncmp(cfg.name, fakeInitialMode.name, DRM_DISPLAY_MODE_LEN) == 0) {
            MESON_LOGD("setActiveConfig default mode not supported");
            return HWC2_ERROR_NONE;
        }

        mPreviousMode = cfg;

        std::string bestDolbyVision;
        bool needRecoveryBestDV = false;
        if (mDvEnabled) {
            if (!sc_read_bootenv(UBOOTENV_BESTDOLBYVISION, bestDolbyVision)) {
                if (bestDolbyVision.empty()|| bestDolbyVision == "true") {
                    MESON_LOGD("RealModeMgr set BestDVPolicy: false");
                    sc_set_bootenv(UBOOTENV_BESTDOLBYVISION, "false");
                    needRecoveryBestDV = true;
                }
            }
        }

        mCallOnHotPlug = false;
        mConnector->setMode(cfg);

        // set the display mode through systemControl
        // As it will need update the colorspace/colordepth too.
        MESON_LOGD("RealModeMgr::setActiveConfig setMode: %s", cfg.name);
        std::string dispmode(cfg.name);
        sc_set_display_mode(dispmode);

        // If we need recovery best dobly vision policy, then recovery it.
        if (mDvEnabled && needRecoveryBestDV) {
            MESON_LOGD("RealModeMgr recovery BestDVPolicy: true");
            sc_set_bootenv(UBOOTENV_BESTDOLBYVISION, "true");
        }
    } else {
        MESON_LOGE("set invalild active config (%d)", config);
        return HWC2_ERROR_NOT_VALIDATED;
    }

    return HWC2_ERROR_NONE;
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

void RealModeMgr::dump(String8 & dumpstr) {
    dumpstr.appendFormat("RealModeMgr:(%s)\n", mCurMode.name);
    dumpstr.append("-----------------------------------------------------------"
        "---------------------------------------------------\n");
    dumpstr.append("|  CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   |      NAME      |  GROUP_ID |\n");
    dumpstr.append("+-----------+------------------+-----------+------------+"
        "-----------+-----------+----------------+-----------+\n");

    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mModes.begin();

    for (; it != mModes.end(); ++it) {
        int mode = it->first;
        drm_mode_info_t config = it->second;
        dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    | %14s |    %3d    |\n",
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
        "---------------------------------------------------\n");
}
