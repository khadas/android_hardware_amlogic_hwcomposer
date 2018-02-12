/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _CONNECTORPANEL_H
#define _CONNECTORPANEL_H
#include <HwDisplayConnector.h>

class FBContext;
class DisplayConfig;

class ConnectorPanel :public HwDisplayConnector {
public:
   ConnectorPanel();
   virtual ~ConnectorPanel();
   virtual int init();
   virtual drm_connector_type_t getType();
   virtual uint32_t getModesCount();
   virtual bool isConnected();
   virtual bool isSecure();
   virtual KeyedVector<int,DisplayConfig*>getModesInfo();
   virtual void dump(String8 & dumpstr);

protected:
   int32_t getLineValue(const char *lineStr,const char *magicStr);
   int32_t parseHdrCapabilities();
   status_t readPhySize();
   std::string readDispMode(std::string &dispmode);
#if PLATFORM_SDK_VERSION >= 26
    struct SystemControlDeathRecipient : public android::hardware::hidl_death_recipient  {
        // hidl_death_recipient interface
        virtual void serviceDied(uint64_t cookie,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override{};
    };
    sp<SystemControlDeathRecipient> mDeathRecipient = nullptr;
#endif
     auto getSystemControlService();

private:
    sp<ISystemControlService> mSC;
    KeyedVector<int, DisplayConfig*> mSupportDispConfigs;

    FBContext *mFramebufferContext;

    std::string mDisplayMode;
    DisplayConfig *mconfig;

    int mPhyWidth;
    int mPhyHeight;

    hdr_dev_capabilities_t mHdrCapabilities;
};






#endif
