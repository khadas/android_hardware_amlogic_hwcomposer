/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VariableModeMgr.h"

#include <MesonLog.h>
#include <systemcontrol.h>

#include <string>

#define DEFUALT_DPI (159)
#define DEFAULT_REFRESH_RATE_60 (60.0f)
#define DEFAULT_REFRESH_RATE_50 (50.0f)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* used for init default configs */
static const drm_mode_info_t mode_info[] = {
    { /* VMODE_720P */
        .name              = "720p60hz",
        .dpiX              = DEFUALT_DPI,
        .dpiY              = DEFUALT_DPI,
        .pixelW            = 1280,
        .pixelH            = 720,
        .refreshRate       = DEFAULT_REFRESH_RATE_60,
    },
    { /* VMODE_1080P */
        .name              = "1080p60hz",
        .dpiX              = DEFUALT_DPI,
        .dpiY              = DEFUALT_DPI,
        .pixelW            = 1920,
        .pixelH            = 1080,
        .refreshRate       = DEFAULT_REFRESH_RATE_60,
    },
    { /* VMODE_720P_50hz */
        .name              = "720p50hz",
        .dpiX              = DEFUALT_DPI,
        .dpiY              = DEFUALT_DPI,
        .pixelW            = 1280,
        .pixelH            = 720,
        .refreshRate       = DEFAULT_REFRESH_RATE_50,
    },
    { /* VMODE_1080P_50HZ */
        .name              = "1080p50hz",
        .dpiX              = DEFUALT_DPI,
        .dpiY              = DEFUALT_DPI,
        .pixelW            = 1920,
        .pixelH            = 1080,
        .refreshRate       = DEFAULT_REFRESH_RATE_50,
    },
    { /* DefaultMode */
        .name              = "DefaultMode",
        .dpiX              = DEFUALT_DPI,
        .dpiY              = DEFUALT_DPI,
        .pixelW            = 1920,
        .pixelH            = 1080,
        .refreshRate       = DEFAULT_REFRESH_RATE_60,
    },
};

VariableModeMgr::VariableModeMgr()
    : mIsInit(true) {
}

VariableModeMgr::~VariableModeMgr() {

}

hwc_modes_policy_t VariableModeMgr::getPolicyType() {
    return FULL_ACTIVE_POLICY;
}

const char * VariableModeMgr::getName() {
    return "VariableMode";
}

void VariableModeMgr::setDisplayResources(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector> & connector) {
    mConnector = connector;
    mCrtc = crtc;
    mCrtc->parseDftFbSize(mFbWidth, mFbHeight);

    /*
     * Only when it is first boot will come here,
     * initialize a default display mode according to the given FB size,
     * and will return this default mode to SF before it calls setActiveConfig().
     */
    if (mIsInit) {
        mIsInit = false;
        reset();
        initDefaultDispResources();
    }

    updateDisplayResources();
}

int32_t VariableModeMgr::initDefaultDispResources() {
    mDefaultMode =
        findMatchedMode(mFbWidth, mFbHeight, DEFAULT_REFRESH_RATE_60);
    mActiveModes.emplace(mActiveModes.size(), mDefaultMode);
    updateActiveConfig(mDefaultMode.name);
    MESON_LOGV("initDefaultDispResources (%s)", mDefaultMode.name);
    return 0;
}

int32_t VariableModeMgr::updateDisplayResources() {
    bool useFakeMode = false;

    if (mConnector->isConnected()) {
        updateDisplayConfigs();
        std::string dispmode;
        if (0 == sc_get_display_mode(dispmode) && dispmode.compare("null") != 0) {
            updateActiveConfig(dispmode.data());
        } else {
            useFakeMode = true;
            MESON_LOGD("Get invalid display mode.");
        }
    } else
        useFakeMode = true;

    if (useFakeMode)
        updateActiveConfig(mDefaultMode.name);

    return 0;
}

hwc2_error_t VariableModeMgr::getDisplayConfigs(
    uint32_t * outNumConfigs, hwc2_config_t * outConfigs) {
    *outNumConfigs = mActiveModes.size();

    if (outConfigs) {
        std::map<uint32_t, drm_mode_info_t>::iterator it =
            mActiveModes.begin();
        for (uint32_t index = 0; it != mActiveModes.end(); ++it, ++index) {
            outConfigs[index] = it->first;
            MESON_LOGD("outConfig[%d]: %d.", index, outConfigs[index]);
        }
    }
    return HWC2_ERROR_NONE;
}

hwc2_error_t VariableModeMgr::updateDisplayConfigs() {
    std::map<uint32_t, drm_mode_info_t> activeModes;
    mActiveModes.clear();

    mConnector->getModes(activeModes);
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        activeModes.begin();
    for (; it != activeModes.end(); ++it)
        // skip default / fake active mode as we add it to the end
        if (memcmp(&mDefaultMode, &it->second, sizeof(mDefaultMode)))
            mActiveModes.emplace(mActiveModes.size(), it->second);
        else
            mDefaultModeSupport = true;

    // Add default mode as last, unconditionally in all cases. This is to ensure
    // availability of 1080p mode always.
    mActiveModes.emplace(mActiveModes.size(), mDefaultMode);
    return HWC2_ERROR_NONE;
}

hwc2_error_t  VariableModeMgr::getDisplayAttribute(
    hwc2_config_t config, int32_t attribute, int32_t * outValue) {
    MESON_LOGV("getDisplayAttribute: config %d, fakeConfig %d,"
        "activeConfig %d, mExtModeSet %d", config, mFakeConfigId,
        mActiveConfigId, mExtModeSet);
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mActiveModes.find(config);
    if (it != mActiveModes.end()) {
        drm_mode_info_t curMode = it->second;

        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *outValue = curMode.pixelW;
                break;
            case HWC2_ATTRIBUTE_HEIGHT:
                *outValue = curMode.pixelH;
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
            default:
                MESON_LOGE("Unkown display attribute(%d)", attribute);
                break;
        }

        return HWC2_ERROR_NONE;
    } else {
        MESON_LOGE("[%s]: no support display config: %d", __func__, config);
        return HWC2_ERROR_UNSUPPORTED;
    }
}

hwc2_error_t  VariableModeMgr::updateActiveConfig(
    const char * activeMode) {
    mActiveConfigStr = activeMode;

    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mActiveModes.begin();
    for (; it != mActiveModes.end(); ++it) {
        if (strncmp(activeMode, it->second.name, DRM_DISPLAY_MODE_LEN) == 0) {
            mActiveConfigId = it->first;
            mFakeConfigId = mActiveModes.size()-1;
            MESON_LOGD("updateActiveConfig to (%s, %d)", activeMode, mActiveConfigId);
            return HWC2_ERROR_NONE;
        }
    }

    // If we reach here we are trying to set an unsupported mode. This can happen as
    // SystemControl does not guarantee to keep the EDID mode list and the active
    // mode id synchronised. We therefore handle the case where the active mode is
    // not supported by ensuring something sane is set instead.
    // NOTE: this is only really a workaround - HWC should instead guarantee that
    // the display mode list and active mode reported to SF are kept in sync with
    // hot plug events.
    mActiveConfigId = mActiveModes.size()-1;
    mFakeConfigId = mActiveConfigId;
    MESON_LOGD("updateActiveConfig something error to (%s, %d)", activeMode, mActiveConfigId);

    return HWC2_ERROR_NONE;
}

hwc2_error_t VariableModeMgr::getActiveConfig(
    hwc2_config_t * outConfig) {
    *outConfig = mExtModeSet ? mActiveConfigId : mFakeConfigId;
    MESON_LOGD("getActiveConfig (%d), mActiveConfigId %d, mFakeConfigId %d, mExtModeSet %d",
        *outConfig, mActiveConfigId, mFakeConfigId, mExtModeSet);
    return HWC2_ERROR_NONE;
}

hwc2_error_t VariableModeMgr::setActiveConfig(
    hwc2_config_t config) {
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mActiveModes.find(config);
    if (it != mActiveModes.end()) {
        drm_mode_info_t cfg = it->second;

        // update real active config.
        updateActiveConfig(cfg.name);

        // It is possible that default mode is not supported by the sink
        // and it was only advertised to the FWK to force 1080p UI.
        // Trap this case and do nothing. FWK will keep thinking
        // 1080p is supported and set.
        if (!mDefaultModeSupport
            && strncmp(cfg.name, mDefaultMode.name, DRM_DISPLAY_MODE_LEN) == 0) {
            MESON_LOGD("setActiveConfig default mode not supported");
            return HWC2_ERROR_NONE;
        }

        std::string mode(cfg.name);
        // Determine frac / normal display config
        float frac = cfg.refreshRate - (int)cfg.refreshRate;
        bool fracRatePolicy = (frac > 0 || frac < 0) ? true : false;
        mCrtc->updateActiveMode(mode, fracRatePolicy);

        mExtModeSet = true;
        MESON_LOGD("setActiveConfig %d, mExtModeSet %d",
            config, mExtModeSet);
        return HWC2_ERROR_NONE;
    } else {
        MESON_LOGE("set invalild active config (%d)", config);
        return HWC2_ERROR_NOT_VALIDATED;
    }
}

void VariableModeMgr::reset() {
    mActiveModes.clear();
    mDefaultModeSupport = false;
    mActiveConfigId = mFakeConfigId = -1;
    mExtModeSet = false;
}

const drm_mode_info_t VariableModeMgr::findMatchedMode(
    uint32_t width, uint32_t height, float refreshrate) {
    uint32_t i = 0;
    uint32_t size = ARRAY_SIZE(mode_info);
    for (i = 0; i < size; i++) {
        if (mode_info[i].pixelW == width &&
            mode_info[i].pixelH == height &&
            mode_info[i].refreshRate == refreshrate) {
        return mode_info[i];
        }
    }
    return mode_info[size-1];
}

void VariableModeMgr::dump(String8 & dumpstr) {
    dumpstr.appendFormat("VariableModeMgr: %s\n", mActiveConfigStr.c_str());
    dumpstr.append("---------------------------------------------------------"
        "-------------------------\n");
    dumpstr.append("|   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   |\n");
    dumpstr.append("+------------+------------------+-----------+------------+"
        "-----------+-----------+\n");
    std::map<uint32_t, drm_mode_info_t>::iterator it =
        mActiveModes.begin();
    for (; it != mActiveModes.end(); ++it) {
        int mode = it->first;
        drm_mode_info_t config = it->second;
        dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    \n",
            (mode == (int)mActiveConfigId) ? "*   " : "    ",
            mode,
            config.refreshRate,
            config.pixelW,
            config.pixelH,
            config.dpiX,
            config.dpiY);
    }
    dumpstr.append("---------------------------------------------------------"
        "-------------------------\n");
}

