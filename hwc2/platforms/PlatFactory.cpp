/*
// Copyright(c) 2016 Amlogic Corporation
*/
#include <HwcTrace.h>
#include <PrimaryDevice.h>
//#include <ExternalDevice.h>
#include <VirtualDevice.h>
#include <Hwcomposer.h>
#include <PlatFactory.h>

namespace android {
namespace amlogic {

PlatFactory::PlatFactory()
{
    CTRACE();
}

PlatFactory::~PlatFactory()
{
    CTRACE();
}

IDisplayDevice* PlatFactory::createDisplayDevice(int disp)
{
    CTRACE();
    //when createDisplayDevice is called, Hwcomposer has already finished construction.
    Hwcomposer &hwc = Hwcomposer::getInstance();

    class PlatDeviceControlFactory: public DeviceControlFactory {
    public:
    };

    switch (disp) {
        case IDisplayDevice::DEVICE_PRIMARY:
            return new PrimaryDevice(hwc);
        case IDisplayDevice::DEVICE_VIRTUAL:
            return new VirtualDevice(hwc);
        case IDisplayDevice::DEVICE_EXTERNAL:
            // return new ExternalDevice(hwc, new PlatDeviceControlFactory());
        default:
            ETRACE("invalid display device %d", disp);
            return NULL;
    }
}

Hwcomposer* Hwcomposer::createHwcomposer()
{
    CTRACE();
    Hwcomposer *hwc = new Hwcomposer(new PlatFactory());
    return hwc;
}

} //namespace amlogic
} //namespace android
