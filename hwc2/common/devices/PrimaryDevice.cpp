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
            Utils::getHotplugInString(),
            hotplugInEventListener,
            this);

        observer->registerListener(
            Utils::getHotplugOutString(),
            hotplugOutEventListener,
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

void PrimaryDevice::hotplugInEventListener(void *data)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    if (pThis) {
        pThis->hotplugListener(true);
    }
}

void PrimaryDevice::hotplugOutEventListener(void *data)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    if (pThis) {
        pThis->hotplugListener(false);
    }
}

void PrimaryDevice::hotplugListener(bool connected)
{
    CTRACE();

    ETRACE("hotpug event: %d", connected);

    updateHotplugState(connected);
    // update display configs
    if (connected && !updateDisplayConfigs()) {
        ETRACE("failed to update display config");
        return;
    }

    if (connected)
        getDevice().hotplug(getDisplayId(), connected);
}

} // namespace amlogic
} // namespace android
