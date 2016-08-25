/*
// Copyright(c) 2016 Amlogic Corporation
*/

#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <PrimaryDevice.h>
#include <Utils.h>

namespace android {
namespace amlogic {

PrimaryDevice::PrimaryDevice(Hwcomposer& hwc)
    : PhysicalDevice(DEVICE_PRIMARY, hwc)
{
    CTRACE();
}

PrimaryDevice::~PrimaryDevice()
{
    CTRACE();
}

bool PrimaryDevice::initialize()
{
    if (!PhysicalDevice::initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize physical device");
    }

    UeventObserver *observer = Hwcomposer::getInstance().getUeventObserver();
    if (observer) {
        observer->registerListener(
            Utils::getHotplugString(),
            hotplugEventListener,
            this);
    } else {
        ETRACE("Uevent observer is NULL");
    }

    return true;
}

void PrimaryDevice::deinitialize()
{
    PhysicalDevice::deinitialize();
}

void PrimaryDevice::hotplugEventListener(void *data)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    if (pThis) {
        pThis->hotplugListener();
    }
}

void PrimaryDevice::hotplugListener()
{
    bool ret;

    CTRACE();

    // update display configs
    ret = updateDisplayConfigs();
    if (ret == false) {
        ETRACE("failed to update display config");
        return;
    }

    DTRACE("hotpug event: %d", isConnected());

    getDevice().hotplug(getDisplayId(), isConnected());
}

} // namespace amlogic
} // namespace android
