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

class ConnectorHdmi : public HwDisplayConnector {
public:
    ConnectorHdmi(int32_t drvFd, uint32_t id);
    virtual ~ConnectorHdmi();

public:
    virtual drm_connector_type_t getType();
    virtual bool isRemovable();
    virtual bool isConnected();
    virtual bool isSecure() ;

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);
    virtual void getHdrCapabilities(drm_hdr_capabilities * caps);

    virtual void dump(String8& dumpstr);

protected:
    void loadInfo();

    int32_t loadDisplayModes();
    int32_t addDisplayMode(std::string& mode);

    /*parse hdr info.*/
    int32_t getLineValue(const char *lineStr, const char *magicStr);
    int32_t parseHdrCapabilities();

protected:
    bool isDispModeValid(std::string& dispmode);

     status_t readHdmiDispMode(std::string &dispmode);

private:
    bool mConnected;
    bool mSecure;
    bool mLoaded;

    drm_hdr_capabilities mHdrCapabilities;
};

#endif


