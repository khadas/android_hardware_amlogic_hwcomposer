/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC2_REAL_MODE_MGR_H
#define HWC2_REAL_MODE_MGR_H

#include "HwcModeMgr.h"

/*
 * RealModeMgr:
 * This class designed for TV or reference box to support
 * real active modes. App UI size will be updated when
 * Display timings  changed.
 */

class RealModeMgr : public HwcModeMgr {
public:
    RealModeMgr();
    ~RealModeMgr();

    hwc_modes_policy_t getPolicyType();
    const char * getName();

    void setFramebufferSize(uint32_t w, uint32_t h);
    void setDisplayResources(std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector> & connector);
    int32_t update();
    bool needCallHotPlug() { return mCallOnHotPlug; };
    int32_t getDisplayMode(drm_mode_info_t & mode);

    int32_t getDisplayConfigs(uint32_t * outNumConfigs, uint32_t * outConfigs);
    int32_t getDisplayAttribute(uint32_t config, int32_t attribute,
            int32_t* outValue, int32_t caller);
    int32_t getActiveConfig(uint32_t * outConfig, int32_t caller);
    int32_t setActiveConfig(uint32_t config);

    // for seamless mode switch
    bool isSeamlessSwitch(uint32_t config);

    int32_t getPreferredBootConfig(int32_t* outConfig);
    int32_t setBootConfig(int32_t config);
    int32_t clearBootConfig();
    void setModePolicy(std::shared_ptr<IModePolicy> policy);

    void resetTags();
    void dump(String8 & dumpstr);

    // for filter 16:9 mode
    bool is16_9Mode(drm_mode_info_t mode);
    bool isFakeSizeMode();
    void processModeList(std::map<uint32_t, drm_mode_info_t> & modeList,
            drm_mode_info_t & currentMode);

    // for Interlace mode
    int32_t setPerferredMode(std::string mode);

protected:
    int32_t updateActiveConfig(drm_mode_info_t activeMode);
    bool isSupportModeForCurrentDevice(drm_mode_info_t mode);
    void reset();
    int32_t setModeLocked(drm_mode_info_t & mode);
    bool isTvConnector();
    void mapToFakeSizeMode(drm_mode_info_t & mode, drm_mode_info_t & currentMode);
    void dynamicMapMode(std::string mode);
    int32_t setActiveConfigLocked(uint32_t config);

protected:
    std::shared_ptr<HwDisplayConnector> mConnector;
    std::shared_ptr<HwDisplayCrtc> mCrtc;
    std::shared_ptr<IModePolicy> mModePolicy;

    uint32_t mHwcFbWidth;
    uint32_t mHwcFbHeight;

    std::map<uint32_t, drm_mode_info_t> mModes;
    drm_mode_info_t mLatestRealMode;
    uint32_t mActiveConfigId;

    // protect mMode and mActiveConfigId
    std::mutex mMutex;

    // Dolby Vision enabled or not
    bool mDvEnabled;

    bool mCallOnHotPlug;
    bool mIsFakeSizeMode;
};

#endif // REAL_MODE_MGR_H
