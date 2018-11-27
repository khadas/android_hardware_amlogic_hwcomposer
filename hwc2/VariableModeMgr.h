/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VARIABLE_MODE_MGR_H
#define VARIABLE_MODE_MGR_H

#include "HwcModeMgr.h"

/*
 * VariableModesMgr:
 * This class designed for removeable device(hdmi, cvbs)
 * to support real activeModes.
 * Config list will changed when device disconnect/connect.
 */
class VariableModeMgr : public  HwcModeMgr {
public:
    VariableModeMgr();
    ~VariableModeMgr();

    hwc_modes_policy_t getPolicyType();
    const char * getName();

    void setFramebufferSize(uint32_t w, uint32_t h);
    void setDisplayResources(
        std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector> & connector);
    int32_t update();

    hwc2_error_t  getDisplayConfigs(
        uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    hwc2_error_t  getDisplayAttribute(
        hwc2_config_t config, int32_t attribute, int32_t* outValue, int32_t caller);
    hwc2_error_t getActiveConfig(hwc2_config_t* outConfig, int32_t caller);
    hwc2_error_t setActiveConfig(hwc2_config_t config);

    void dump(String8 & dumpstr);

protected:
    int32_t initDefaultDispResources();
    hwc2_error_t updateHwcDispConfigs();
    hwc2_error_t updateSfDispConfigs();
    hwc2_error_t updateHwcActiveConfig(const char * activeMode);

    void reset();
    const drm_mode_info_t findMatchedMode(
        uint32_t width, uint32_t height, float refreshrate);


protected:
    std::shared_ptr<HwDisplayConnector> mConnector;
    std::shared_ptr<HwDisplayCrtc> mCrtc;

    uint32_t mFbWidth;
    uint32_t mFbHeight;

    bool mIsInit; // first boot flag
    bool mExtModeSet; // setActiveConfig() flag
    bool mDefaultModeSupport;

    std::string mActiveConfigStr;
    hwc2_config_t mFakeConfigId;
    drm_mode_info_t mDefaultMode;

    // Used for HWC
    hwc2_config_t mHwcActiveConfigId;
    std::map<uint32_t, drm_mode_info_t> mHwcActiveModes;

    // Passed to SF
    hwc2_config_t mSfActiveConfigId;
    std::map<uint32_t, drm_mode_info_t> mSfActiveModes;

};

#endif/*VARIABLE_MODE_MGR_H*/
