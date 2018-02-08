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
#include <gralloc_priv.h>
#include <ISystemControlService.h>
#include "AmVinfo.h"
#include <HwDisplayConnector.h>
#define HDMI_FRAC_RATE_POLICY "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"

using namespace android;

class DisplayConfig;
class FBContext;

class ConnectorPanel :public HwDisplayConnector {
public:

   ConnectorPanel();
   virtual ~ConnectorPanel();
   virtual int init();
   virtual drm_connector_type_t getType();
   virtual uint32_t getModesCount();
   virtual bool isConnected();
   virtual bool isSecure();
   virtual KeyedVector<int,DisplayConfig*>updateConnectedConfigs();
   virtual void dump(String8 & dumpstr);

protected:

    status_t readPhySize(framebuffer_info_t& fbInfo);
    int calcDefaultMode(framebuffer_info_t& framebufferInfo,std::     string& defaultMode);
    bool  isDispModeValid(std::string& dispmode);

private:
    sp<ISystemControlService> mSC;
    std::string mDisplayMode;
    KeyedVector<int, DisplayConfig*> mSupportDispConfigs;
    DisplayConfig *mconfig;

    int mRefreshRate;
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
    bool mFracRate;
    int mPhyWidth;
    int mPhyHeight;

    int32_t mDrvFd;
    uint32_t mId;

    bool mSecure;
    bool mConnected;
};






#endif
