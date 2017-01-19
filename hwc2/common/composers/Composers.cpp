/*
// Copyright(c) 2016 Amlogic Corporation
*/

#include <HwcTrace.h>
#include <Composers.h>
#include <IDisplayDevice.h>


namespace android {
namespace amlogic {

Composers::Composers(IDisplayDevice& disp)
    : mDisplayDevice(disp),
      mInitialized(false)
{
}

Composers::~Composers()
{
    WARN_IF_NOT_DEINIT();
}

bool Composers::initialize(framebuffer_info_t* fbInfo)
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mInitialized = true;
    return true;
}

void Composers::deinitialize()
{
    mInitialized = false;
}

} // namespace amlogic
} // namesapce android

