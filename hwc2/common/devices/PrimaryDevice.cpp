/*
// Copyright (c) 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <PrimaryDevice.h>
#include <Utils.h>

namespace android {
namespace amlogic {

PrimaryDevice::PrimaryDevice(Hwcomposer& hwc, DeviceControlFactory* controlFactory)
    : PhysicalDevice(DEVICE_PRIMARY, hwc, controlFactory)
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
