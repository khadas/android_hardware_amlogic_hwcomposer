/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "ActiveModeMgr.h"
#include <HwcConfig.h>
#include <MesonLog.h>
#include <systemcontrol.h>
#include <hardware/hwcomposer2.h>

#include <string>
#include <math.h>

#define DEFUALT_DPI (160)
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

ActiveModeMgr::ActiveModeMgr()
    : mCallOnHotPlug(true),
      mPreviousMode(fakeInitialMode),
      mDvEnabled(false) {
}

ActiveModeMgr::~ActiveModeMgr() {
}

hwc_modes_policy_t ActiveModeMgr::getPolicyType() {
    return ACTIVE_MODE_POLICY;
}

const char * ActiveModeMgr::getName() {
    return "ActiveMode";
}

void ActiveModeMgr::setFramebufferSize(uint32_t w, uint32_t h) {
    mFbWidth = w;
    mFbHeight = h;
}

void ActiveModeMgr::setDisplayResources(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector) {
    mConnector = connector;
    mCrtc = crtc;
}

/*
 * when connected to a 4K TV
 * 1. No DV support or DV supported for all 2160P modes or supoort DV is disabled
 *    then reports to framework only 1080Ps which are mapped to 2160P modes
 * 2. DV support in 2160P <= 2160P30hz
 *    then reports to framework only 1080Ps which are mapped to 1080P modes
 *
 * when connected to a 1080P TV, then reports only 1080Ps which are mapped to 1080P modes
 *
 * when conneted to a TV < 1080P TVs, then reports only 1080Ps which maps to the modes
 * having the highest native resolution; for example 720Ps.
 */

int32_t ActiveModeMgr::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool useFakeMode = true;
    drm_mode_info_t activeMode;
    std::map<uint32_t, drm_mode_info_t> supportedModes;

    reset();

    if (mConnector->isConnected()) {
        mConnector->getModes(supportedModes);
        if (mCrtc->getMode(activeMode) == 0) {
            mCurMode = activeMode;
            mPreviousMode = activeMode;
            useFakeMode = false;
        } else {
            strncpy(mCurMode.name, "invalid", DRM_DISPLAY_MODE_LEN);
        }

        // detect the Dobly vision support status
        bool unused = false;
        std::string highestModeForDv;
        sc_sink_support_dv(highestModeForDv, unused);
        mDvEnabled = sc_is_dolby_version_enable();
        MESON_LOGD("ActiveModeMgr::update mDvEnabled(%d), highestModeForDv:%s",
                mDvEnabled, highestModeForDv.c_str());

        for (auto it = supportedModes.begin(); it != supportedModes.end(); it++) {
            // support dolby version
            if (mDvEnabled) {
                // filter 4K modes if DV support not all the 2160P modes
                if (strstr(mCurMode.name, it->second.name) == NULL &&
                        strstr(highestModeForDv.c_str(), "2160p60hz") == NULL) {
                    if ((strstr(it->second.name, "2160p") != NULL)
                            || (strstr(it->second.name, "smpte") != NULL)) {
                        continue;
                    }
                }
            }

            mHwcActiveModes.emplace(mHwcActiveModes.size(), it->second);
        }
    }

    if (useFakeMode) {
        mCurMode = mPreviousMode;
        strncpy(mCurMode.name, "FAKE_PREVIOUS_MODE", DRM_DISPLAY_MODE_LEN);
        mHwcActiveModes.emplace(mHwcActiveModes.size(), mCurMode);
    }

    updateHwcActiveConfig(mCurMode);
    updateSfDispConfigs();

    return 0;
}

int32_t ActiveModeMgr::getDisplayMode(drm_mode_info_t & mode) {
    return mCrtc->getMode(mode);
}

int32_t ActiveModeMgr::getDisplayConfigs(
    uint32_t * outNumConfigs, uint32_t * outConfigs) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outNumConfigs = mSfActiveModes.size();
    if (outConfigs) {
        std::map<uint32_t, drm_mode_info_t>::iterator it =
            mSfActiveModes.begin();
        for (uint32_t index = 0; it != mSfActiveModes.end(); ++it, ++index) {
            outConfigs[index] = it->first;
            MESON_LOGV("outConfig[%d]: %d.", index, outConfigs[index]);
        }
    }
    return HWC2_ERROR_NONE;
}


bool ActiveModeMgr::isFracRate(float refreshRate) {
    return refreshRate > floor(refreshRate) ? true : false;
}

int32_t ActiveModeMgr::updateSfDispConfigs() {
    // key is the fps, value is display mode
    std::map<float, drm_mode_info_t> tmpList;
    std::map<float, drm_mode_info_t>::iterator tmpIt;

    for (auto it = mHwcActiveModes.begin(); it != mHwcActiveModes.end(); ++it) {
        //first check the fps, if there is not the same fps, add it to sf list
        //then check the width, add the biggest width one.
        drm_mode_info_t cfg = it->second;
        std::string cMode = cfg.name;
        float cFps = cfg.refreshRate;
        uint32_t cWidth = cfg.pixelW;

        // if current mode < 1080P, filter the mode that bigger than current mode
        if (mCurMode.pixelW < 1920) {
            if (cWidth > mCurMode.pixelW)
                continue;
        }

        tmpIt = tmpList.find(cFps);
        if (tmpIt != tmpList.end()) {
            drm_mode_info_t iCfg = tmpIt->second;
            uint32_t iWidth = iCfg.pixelW;
            if ((cWidth >= iWidth) && (cWidth != 4096)) {
                tmpList.erase(tmpIt);
                tmpList.emplace(cFps, it->second);
            }
        } else {
                tmpList.emplace(cFps, it->second);
        }
    }

    // mSFActiveModes and mSFActiveConfigId are reseted in update()
    auto activeIt = mHwcActiveModes.find(mHwcActiveConfigId);
    for (tmpIt = tmpList.begin(); tmpIt != tmpList.end(); ++tmpIt) {
        mSfActiveModes.emplace(mSfActiveModes.size(), tmpIt->second);

        if (activeIt != mHwcActiveModes.end()) {
            if (!strncmp(activeIt->second.name, tmpIt->second.name, DRM_DISPLAY_MODE_LEN)
                && activeIt->second.refreshRate == tmpIt->second.refreshRate) {
                mSfActiveConfigId = mSfActiveModes.size() - 1;
            }
        }
    }

    if (mSfActiveConfigId == -1) {
        if (activeIt != mHwcActiveModes.end())
            mSfActiveModes.emplace(mSfActiveModes.size(), activeIt->second);
        mSfActiveConfigId = mSfActiveModes.size() - 1;
    }

    MESON_LOGD("update sf active id : %d", mSfActiveConfigId);

    return HWC2_ERROR_NONE;
}

int32_t  ActiveModeMgr::getDisplayAttribute(
    uint32_t config, int32_t attribute, int32_t * outValue, int32_t caller __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::map<uint32_t, drm_mode_info_t>::iterator it;
    it = mSfActiveModes.find(config);

    if (it != mSfActiveModes.end()) {
        drm_mode_info_t curMode = it->second;
        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *outValue = mFbWidth;
                break;
            case HWC2_ATTRIBUTE_HEIGHT:
                *outValue = mFbHeight;
                break;
            case HWC2_ATTRIBUTE_VSYNC_PERIOD:
#ifdef HWC_HEADLESS
                *outValue = 1e9 / (HWC_HEADLESS_REFRESHRATE);
#else
                *outValue = 1e9 / curMode.refreshRate;
#endif
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
                MESON_LOGE("Unkown display attribute(%d)", attribute);
                break;
        }
        return HWC2_ERROR_NONE;
   }
   else {
        MESON_LOGE("[%s]: no support display config: %d", __func__, config);
        return HWC2_ERROR_UNSUPPORTED;
   }
}

int32_t  ActiveModeMgr::updateHwcActiveConfig(drm_mode_info_t activeMode) {
    for (auto it = mHwcActiveModes.begin(); it != mHwcActiveModes.end(); ++it) {
        if (!strncmp(activeMode.name, it->second.name, DRM_DISPLAY_MODE_LEN) &&
            fabs(activeMode.refreshRate - it->second.refreshRate) < 1e-2) {
            mHwcActiveConfigId = it->first;
            MESON_LOGD("%s to (%s, %d)", __func__, activeMode.name, mHwcActiveConfigId);
            return HWC2_ERROR_NONE;
        }
    }

    mHwcActiveConfigId = mHwcActiveModes.size()-1;
    MESON_LOGD("%s something error to (%s, %d)", __func__, activeMode.name, mHwcActiveConfigId);

    return HWC2_ERROR_NONE;
}


int32_t  ActiveModeMgr::updateSfActiveConfig(
    uint32_t configId, [[maybe_unused]] drm_mode_info_t cfg) {
    mSfActiveConfigId = configId;
    MESON_LOGD("updateSfActiveConfig(mode) to (%s, %d)", cfg.name, mSfActiveConfigId);
    return HWC2_ERROR_NONE;
}

int32_t ActiveModeMgr::getActiveConfig(
    uint32_t * outConfig, int32_t caller __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outConfig = mSfActiveConfigId;
    return HWC2_ERROR_NONE;
}

int32_t ActiveModeMgr::setActiveConfig(uint32_t configId) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mSfActiveModes.find(configId);
    if (it != mSfActiveModes.end()) {
        drm_mode_info_t cfg = it->second;

        if (strncmp(cfg.name, fakeInitialMode.name, DRM_DISPLAY_MODE_LEN) == 0) {
            MESON_LOGD("setActiveConfig fake mode not supported");
            return HWC2_ERROR_NONE;
        }

        // update real active config.
        updateSfActiveConfig(configId, cfg);
        updateHwcActiveConfig(cfg);
        MESON_LOGD("ActiveModeMgr::setActiveConfig %d, name:%s", configId, cfg.name);

        mCallOnHotPlug = false;

        // Disable auto best DolbyVision mode selection policy
        // So that when we update the display mode,
        // SystemContrl doesn't change to some other mode.
        std::string bestDolbyVision;
        bool needRecoveryBestDV = false;
        if (mDvEnabled) {
            if (!sc_read_bootenv(UBOOTENV_BESTDOLBYVISION, bestDolbyVision)) {
                if (bestDolbyVision.empty()|| bestDolbyVision == "true") {
                    sc_set_bootenv(UBOOTENV_BESTDOLBYVISION, "false");
                    needRecoveryBestDV = true;
                }
            }
        }

        mConnector->setMode(cfg);
        mCrtc->setMode(cfg);

        // If we need recovery best dobly vision policy, then recovery it.
        if (mDvEnabled && needRecoveryBestDV) {
            sc_set_bootenv(UBOOTENV_BESTDOLBYVISION, "true");
        }

        return HWC2_ERROR_NONE;
    } else {
        MESON_LOGE("set invalild active config (%d)", configId);
        return HWC2_ERROR_NOT_VALIDATED;
    }
}

void ActiveModeMgr::reset() {
    mHwcActiveModes.clear();
    mSfActiveModes.clear();
    mSfActiveConfigId = mHwcActiveConfigId = -1;
    mCallOnHotPlug = true;
}

void ActiveModeMgr::resetTags() {
    mCallOnHotPlug = true;
};

void ActiveModeMgr::dump(String8 & dumpstr) {
    dumpstr.appendFormat("ActiveModeMgr(hwc): %s\n", mCurMode.name);
    dumpstr.append("---------------------------------------------------------"
        "-------------------------------------------------\n");
    dumpstr.append("|   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   |   FRAC    |     mode   |   GROUP_ID   |\n");
    dumpstr.append("+------------+------------------+-----------+------------+"
        "-----------+-----------+-----------+-----------+--------------+\n");
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mHwcActiveModes.begin();
    for (; it != mHwcActiveModes.end(); ++it) {
        int mode = it->first;
        drm_mode_info_t config = it->second;
        dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    |   %d    |    %s   |    %3d    |\n",
            (mode == (int)mHwcActiveConfigId) ? "*   " : "    ",
            mode,
            config.refreshRate,
            config.pixelW,
            config.pixelH,
            config.dpiX,
            config.dpiY,
            isFracRate(config.refreshRate),
            config.name,
            config.groupId);
    }
    dumpstr.append("---------------------------------------------------------"
        "-------------------------------------------------\n");

    dumpstr.appendFormat("ActiveModeMgr(sf): %s\n", mCurMode.name);
    dumpstr.append("---------------------------------------------------------"
        "-------------------------------------------------\n");
    dumpstr.append("|   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   |   FRAC    |     mode   |   GROUP_ID   |\n");
    dumpstr.append("+------------+------------------+-----------+------------+"
        "-----------+-----------+-----------+-----------+--------------+\n");
    std::map<uint32_t, drm_mode_info_t>::iterator it1 =
        mSfActiveModes.begin();
    for (; it1 != mSfActiveModes.end(); ++it1) {
        int mode1 = it1->first;
        drm_mode_info_t config = it1->second;
        dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    |   %d    |    %s   |    %3d    |\n",
            (mode1 == (int)mSfActiveConfigId) ? "*   " : "    ",
            mode1,
            config.refreshRate,
            config.pixelW,
            config.pixelH,
            config.dpiX,
            config.dpiY,
            isFracRate(config.refreshRate),
            config.name,
            config.groupId);
    }
    dumpstr.append("---------------------------------------------------------"
        "-------------------------------------------------\n");
}
