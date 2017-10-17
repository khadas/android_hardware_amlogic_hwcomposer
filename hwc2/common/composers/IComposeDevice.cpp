/*
// Copyright (c) 2016 Amlogic
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
//
*/


#include <HwcTrace.h>
#include <IComposeDevice.h>
#include <IDisplayDevice.h>


namespace android {
namespace amlogic {

IComposeDevice::IComposeDevice(IDisplayDevice& disp)
    : mDisplayDevice(disp),
      mInitialized(false)
{
}

IComposeDevice::~IComposeDevice()
{
    WARN_IF_NOT_DEINIT();
}

bool IComposeDevice::initialize(framebuffer_info_t* fbInfo)
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mInitialized = true;
    return true;
}

void IComposeDevice::deinitialize()
{
    mInitialized = false;
}

} // namespace amlogic
} // namesapce android

