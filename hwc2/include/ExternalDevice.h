/*
// Copyright (c) 2016 Amlogic Corporation
*/
#ifndef EXTERNAL_DEVICE_H
#define EXTERNAL_DEVICE_H

#include <PhysicalDevice.h>
#include <IHdcpControl.h>
#include <SimpleThread.h>

namespace android {
namespace amlogic {


class ExternalDevice : public PhysicalDevice {

public:
    ExternalDevice(Hwcomposer& hwc, DeviceControlFactory* controlFactory);
    virtual ~ExternalDevice();
public:
    virtual bool initialize();
    virtual void deinitialize();
    virtual bool setDrmMode(drmModeModeInfo& value);
    virtual void setRefreshRate(int hz);
    virtual int  getActiveConfig();
    virtual bool setActiveConfig(int index);
    int getRefreshRate();

private:
    static void HdcpLinkStatusListener(bool success, void *userData);
    void HdcpLinkStatusListener(bool success);
    void setDrmMode();
protected:
    IHdcpControl *mHdcpControl;

private:
    static void hotplugEventListener(void *data);
    void hotplugListener();

private:
    Condition mAbortModeSettingCond;
    drmModeModeInfo mPendingDrmMode;
    bool mHotplugEventPending;
    int mExpectedRefreshRate;

private:
    DECLARE_THREAD(ModeSettingThread, ExternalDevice);
};

}
}

#endif /* EXTERNAL_DEVICE_H */
