/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _CONNECTORHDMI_H
#define _CONNECTORHDMI_H
#include <HwDisplayConnector.h>

#include <hardware/hardware.h>
#include <ISystemControlService.h>
#include "AmVinfo.h"
#define HDMI_FRAC_RATE_POLICY "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"

using namespace android;
class FBContext;

class DisplayConfig;

typedef struct hdr_capabilities {
    bool init;
    bool dvSupport;
    bool hdrSupport;
    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} hdr_capabilities_t;

class ConnectorHdmi : public
                      HwDisplayConnector {

public:
    ConnectorHdmi(/*int32_t ConnectorDrv,uint32_t ConnectorId*/);
    virtual ~ConnectorHdmi();

public:
    virtual int init();
    virtual drm_connector_type_t getType();
    virtual uint32_t getModesCount();
    virtual bool isConnected();
    virtual bool isSecure() ;
    virtual KeyedVector<int,DisplayConfig*>  updateConnectedConfigs();
    virtual void dump(String8& dumpstr);

protected:

 bool isDispModeValid(std::string& dispmode);
 bool updateHotplug(bool connected, framebuffer_info_t& framebufferInfo);
#if PLATFORM_SDK_VERSION >= 26
    struct SystemControlDeathRecipient : public android::hardware::hidl_death_recipient  {
        // hidl_death_recipient interface
        virtual void serviceDied(uint64_t cookie,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override{};
    };
    sp<SystemControlDeathRecipient> mDeathRecipient = nullptr;
#endif

     auto getSystemControlService();
     sp<ISystemControlService> mSC;
     status_t readEdidList(std::vector<std::string> &edidlist);
     status_t writeHdmiDispMode(std::string &dispmode);
     status_t readHdmiDispMode(std::string &dispmode);
     status_t readHdmiPhySize(framebuffer_info_t& fbInfo);


    // operations on mSupportDispModes
    status_t clearSupportedConfigs();
    status_t updateSupportedConfigs();
    status_t addSupportedConfig(std::string& mode);
    // ensure the active mode equals the current displaymode.

    bool readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes);

    status_t calcDefaultMode(framebuffer_info_t& framebufferInfo, std::string& defaultMode);
    status_t buildSingleConfigList(std::string& defaultMode);
    int updateDisplayAttributes(framebuffer_info_t &framebufferInfo);
    int32_t parseHdrCapabilities();
    int32_t getLineValue(const char *lineStr, const char *magicStr);

private:
    // configures variables.
    KeyedVector<int, DisplayConfig*> mSupportDispConfigs;
    DisplayConfig *mconfig;
    FBContext* mFramebufferContext;
    sp<ISystemControlService> mSystemControl;
    // physical size in mm.
    int mPhyWidth;
    int mPhyHeight;
    // framebuffer size.
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
    bool mFracRate;
    int mRefreshRate;
    bool mConnected;
    bool mSecure;

    std::string mDisplayMode;
    std::string mDefaultDispMode;

    hdr_capabilities_t mHdrCapabilities;
};

#endif


