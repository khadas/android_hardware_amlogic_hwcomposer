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

#include "RealModeMgr.h"

#define DEFUALT_DPI (159)
#define DEFAULT_REFRESH_RATE (60.0f)

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

int32_t RealModeMgr::updateActiveConfig(const char* activeMode) {
    for (auto it = mModes.begin(); it != mModes.end(); ++it) {
        if (strncmp(activeMode, it->second.name, DRM_DISPLAY_MODE_LEN) == 0) {
            mActiveConfigId = it->first;
            MESON_LOGV("%s aciveConfigId = %d", __func__, mActiveConfigId);
            return HWC2_ERROR_NONE;
        }
    }

    mActiveConfigId = mModes.size()-1;
    MESON_LOGD("%s failed to find [%s], default set activeConfigId to [%d]",
            __func__, activeMode, mActiveConfigId);

    return HWC2_ERROR_NONE;
}

void RealModeMgr::reset() {
    mModes.clear();
    mActiveConfigId = -1;
}

int32_t RealModeMgr::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool useFakeMode = true;
    drm_mode_info_t realMode;
    std::map<uint32_t, drm_mode_info_t> supportModes;

    /* reset ModeList */
    reset();
    if (mConnector->isConnected()) {
        mConnector->getModes(supportModes);
        if (mCrtc->getMode(realMode) == 0) {
            if (realMode.name[0] != 0) {
                mCurMode = realMode;
                mPreviousMode = realMode;
                useFakeMode = false;
            }
        }

        mDvEnabled = sc_is_dolby_version_enable();
        MESON_LOGD("RealModeMgr::update mDvEnabled(%d)", mDvEnabled);

        for (auto it = supportModes.begin(); it != supportModes.end(); it++) {
            mModes.emplace(mModes.size(), it->second);
        }
    }

    if (useFakeMode) {
        mCurMode = mPreviousMode;
        strncpy(mCurMode.name, "FAKE_PREVIOUS_MODE", DRM_DISPLAY_MODE_LEN);
        mModes.emplace(mModes.size(), mCurMode);
    }

    updateActiveConfig(mCurMode.name);

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
        MESON_LOGE("[%s]: no support display config: %d", __func__, config);
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

        updateActiveConfig(cfg.name);
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
