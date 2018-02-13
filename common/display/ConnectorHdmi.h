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

class DisplayConfig;

class ConnectorHdmi : public HwDisplayConnector {
public:
    ConnectorHdmi(/*int32_t ConnectorDrv,uint32_t ConnectorId*/);
    virtual ~ConnectorHdmi();

public:
    virtual int init();
    virtual drm_connector_type_t getType();
    virtual uint32_t getModesCount();
    virtual bool isRemovable();
    virtual bool isConnected();
    virtual bool isSecure() ;
    virtual KeyedVector<int,DisplayConfig*>  getModesInfo();
    virtual void dump(String8& dumpstr);

protected:
 bool isDispModeValid(std::string& dispmode);

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
     status_t readHdmiDispMode(std::string &dispmode);
     status_t readHdmiPhySize();

    // operations on mSupportDispModes
    status_t clearSupportedConfigs();
    status_t updateSupportedConfigs();
    status_t addSupportedConfig(std::string& mode);

    int32_t parseHdrCapabilities();
    int32_t getLineValue(const char *lineStr, const char *magicStr);

private:
    // configures variables.
    KeyedVector<int, DisplayConfig*> mSupportDispConfigs;
    sp<ISystemControlService> mSystemControl;

    bool mConnected;
    bool mSecure;

    int mPhyWidth;
    int mPhyHeight;

    hdr_dev_capabilities_t mHdrCapabilities;
};

#endif


