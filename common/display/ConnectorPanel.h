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

private:
    sp<ISystemControlService> mSC;
    KeyedVector<int, DisplayConfig*> mSupportDispConfigs;
};






#endif
