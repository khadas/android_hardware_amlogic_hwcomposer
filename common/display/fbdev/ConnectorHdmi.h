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
#include "HwDisplayConnectorFbdev.h"

class ConnectorHdmi : public HwDisplayConnectorFbdev {
friend class HwDisplayCrtc;
public:
    ConnectorHdmi(int32_t drvFd, uint32_t id);
    virtual ~ConnectorHdmi();

public:
    virtual int32_t update();

    virtual const char * getName();
    virtual drm_connector_type_t getType();
    virtual bool isConnected();
    virtual bool isSecure() ;

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);
    virtual void getHdrCapabilities(drm_hdr_capabilities * caps);
    virtual std::string getCurrentHdrType();

    virtual void dump(String8& dumpstr);

    virtual int32_t setMode(drm_mode_info_t & mode);
    int32_t getIdentificationData(std::vector<uint8_t>& idOut) override;
    int32_t setAutoLowLatencyMode(bool on) override;
    int32_t setContentType(uint32_t contentType) override;

protected:
    virtual int32_t addDisplayMode(std::string& mode);
    bool checkConnectState();

    int32_t loadDisplayModes();
    int32_t loadSupportedContentTypes();

    void getDvSupportStatus();
    /*parse hdr info.*/
    virtual void parseEDID();

private:
    char mName[64];
    bool mConnected;
    bool mSecure;

    std::vector<uint8_t> mEDID;
    bool mIsEDIDValid;

    int32_t mFracMode;
    std::vector<float> mFracRefreshRates;
    drm_hdr_capabilities mHdrCapabilities;
    std::string mCurrentHdrType;
};

#endif
