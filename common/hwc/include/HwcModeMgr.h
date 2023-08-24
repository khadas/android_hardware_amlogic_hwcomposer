/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef IHWC_MODE_MGR_H
#define IHWC_MODE_MGR_H

#include <BasicTypes.h>
#include <HwDisplayConnector.h>
#include <HwDisplayCrtc.h>
#include <HwcConfig.h>
#include "IModePolicy.h"
#include "mode_ubootenv.h"

#define UBOOTENV_HDMIMODE               "ubootenv.var.hdmimode"
#define UBOOTENV_OUTPUTMODE             "ubootenv.var.outputmode"

enum {
    CALL_FROM_SF = 0,
    CALL_FROM_HWC = 1,
};

/*
 *For different connectors, we need different manage strategy.
 *This class defines basic interface to manage display modes.
 */
class HwcModeMgr {
public:
    HwcModeMgr() {}
    virtual ~HwcModeMgr() {}

    virtual hwc_modes_policy_t getPolicyType() = 0;
    virtual const char * getName() = 0;

    virtual void setFramebufferSize(uint32_t w, uint32_t h) = 0;
    virtual void setDisplayResources(
        std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector> & connector) = 0;
    virtual int32_t update() = 0;
    virtual int32_t getDisplayMode(drm_mode_info_t & mode) = 0;

    virtual int32_t  getDisplayConfigs(
        uint32_t* outNumConfigs, uint32_t* outConfigs) = 0;
    virtual int32_t  getDisplayAttribute(
        uint32_t config, int32_t attribute, int32_t* outValue,
        int32_t caller = CALL_FROM_HWC) = 0;
    virtual int32_t getActiveConfig(uint32_t * outConfig,
        int32_t caller = CALL_FROM_HWC) = 0;
    virtual int32_t setActiveConfig(uint32_t config) = 0;
    // for seamless mode switch
    virtual bool isSeamlessSwitch(uint32_t config) = 0;

    /* for default mode */
    virtual int32_t getPreferredBootConfig(
        int32_t* outConfig) = 0;
    virtual int32_t setBootConfig(int32_t config) = 0;
    virtual int32_t clearBootConfig() = 0;
    virtual void setModePolicy(std::shared_ptr<IModePolicy> /* policy*/) {};

    virtual void dump(String8 & dumpstr) = 0;

    // for filter 16:9 mode
    virtual bool is16_9Mode(drm_mode_info_t mode) = 0;
    virtual int32_t setPerferredMode(std::string mode) = 0;

    bool needCallHotPlug() { return mCallOnHotPlug; };
    void resetTags(bool hotplugTag = true) { mCallOnHotPlug = hotplugTag; };

public:
    bool mCallOnHotPlug = true;
};

std::shared_ptr<HwcModeMgr> createModeMgr(hwc_modes_policy_t policy);

#endif/*IHWC_MODE_MGR_H*/
