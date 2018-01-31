/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef CONNECTOR_HDMI_H

#include <HwDisplayConnector.h>
#include <BasicTypes.h>

#if 0
#include <ISystemControlService.h>

#define HDMI_FRAC_RATE_POLICY "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"

enum {
    REFRESH_24kHZ = 24,
    REFRESH_30kHZ = 30,
    REFRESH_60kHZ = 60,
    REFRESH_120kHZ = 120,
    REFRESH_240kHZ = 240
};

class FBContext {
public:
    FBContext()
    : mStatus(false)
    {
        mFBInfo = new framebuffer_info_t();
    }
    virtual ~FBContext(){}

    virtual framebuffer_info_t* getInfo() { return mFBInfo; }
    virtual bool getStatus() { return mStatus; }
    virtual void setStatus(bool status) { mStatus = status; }
private:
    framebuffer_info_t *mFBInfo;
    bool mStatus;
};

public:
    //std::string getDisplayMode() const { return mDisplayMode; };
    float getFracRefreshRate() const {
        float actualRate = 0.0f;

        if (mRefreshRate) {
            if (mFracRate) {
                actualRate = (mRefreshRate * 1000) / (float)1001;
            } else {
                actualRate = mRefreshRate;
            }
        }
        return actualRate;
    };
    int getRefreshRate() const { return mRefreshRate; };
    int getWidth() const { return mWidth; };
    int getHeight() const { return mHeight; };
    int getDpiX() const { return mDpiX; };
    int getDpiY() const { return mDpiY; };
    void setDpi(int dpix, int dpiy) {
        mDpiX = dpix;
        mDpiY = dpiy;
    };
    bool getFracRatePolicy() { return mFracRate; };
    sp<ISystemControlService>getSystemControlService();
    bool readConfigFile(const char* configpath, std::vector<std::
            string>* supportDispModes);
    int setActiveConfig(int modeId);
    int getActiveConfig(hwc2_config_t* outConfig);
    int clearDispConfigs(KeyedVector<int ,DisplayConfig*>&dispConfigs);
    bool isDispModeValid(std::string &dispmode);
    int calcDefaultMode(framebuffer_info_t&framebufferInfo,std::string& defaultMode);
    int buildSingleConfigList(std::string& defaultMode);
    int addHwcDispConfigs(std::string& mode);
    int readHdmiDispMode(std::string &dispmode);
    status_t readHdmiPhySize(framebuffer_info_t& fbInfo);
    bool chkPresent();
    int writeHdmiDispMode(std::string &dispmode);
    int readEdidList(std::vector<std::string>&edidlist);
    int updateActiveConfig(std::string& activeMode);
    bool updateHotplug(bool connected ,framebuffer_info_t&             framebufferInfo);
    int updateConfigs();
    int updateSfDispConfigs();
    void switchRatePolicy(bool fracRatePolicy);
    int setDisplayMode(std::string& dm,bool policy);
    int getDisplayAttribute(hwc2_config_t config,
            int32_t attribute,
            int32_t* outValue,
            int32_t caller);
    static HwDisplayConnector * createConnector(int drvFd, int32_t id, int32_t type);
    int parseConfigFile();
    virtual ~HwDisplayConnector();
    int init();
    void deinitialize();
    void reset();
    void dump(dump& d);
protected:
    HwDisplayConnector(int drvFd, int32_t id, int32_t type);
    int mDrvFd;

private:
    std::string mDisplayMode;
    int mRefreshRate;
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
    bool mFracRate;
};
#endif

class ConnectorHdmi : public HwDisplayConnector{
public:
    ConnectorHdmi(int32_t drvFd, uint32_t id);
    virtual ~ConnectorHdmi();

    virtual drm_connector_type_t getType();

    virtual uint32_t getModesCount();
    virtual int32_t getDisplayModes(drm_mode_info_t * modes);

    virtual bool isConnected();
    virtual bool isSecure();

    virtual void dump(String8 & dumpstr);

};

#endif/*CONNECTOR_HDMI_H*/
