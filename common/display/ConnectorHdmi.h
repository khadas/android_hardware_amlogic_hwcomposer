/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef CONNECTOR_HDMI_H
#define CONNECTOR_HDMI_H
#include <HwDisplayConnector.h>

class ConnectorHdmi : public HwDisplayConnector {
public:
    ConnectorHdmi(int32_t drvFd, uint32_t id);
    virtual ~ConnectorHdmi();

public:
    virtual int32_t loadProperities();

    virtual const char * getName();
    virtual drm_connector_type_t getType();
    virtual bool isRemovable();
    virtual bool isConnected();
    virtual bool isSecure() ;

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);
    virtual void getHdrCapabilities(drm_hdr_capabilities * caps);

    virtual void dump(String8& dumpstr);

protected:
    bool checkConnectState();

    int32_t loadDisplayModes();

    /*parse hdr info.*/
    int32_t getLineValue(const char *lineStr, const char *magicStr);
    int32_t parseHdrCapabilities();

private:
    bool mConnected;
    bool mSecure;

    drm_hdr_capabilities mHdrCapabilities;

    char mName[64];
};

#endif
