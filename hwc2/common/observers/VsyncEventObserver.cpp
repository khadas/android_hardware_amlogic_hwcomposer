/*
// Copyright (c) 2014 Intel Corporation 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include <HwcTrace.h>
#include <VsyncEventObserver.h>
#include <PhysicalDevice.h>
#include <Utils.h>

namespace android {
namespace amlogic {

VsyncEventObserver::VsyncEventObserver(PhysicalDevice& disp)
    : mLock(),
      mCondition(),
      mDisplayDevice(disp),
      mDevice(IDisplayDevice::DEVICE_COUNT),
      mFbHnd(-1),
      mPreTimeStamp(0),
      mDebug(false),
      mEnabled(false),
      mExitThread(false),
      mInitialized(false)
{
    CTRACE();
}

VsyncEventObserver::~VsyncEventObserver()
{
    WARN_IF_NOT_DEINIT();
}

bool VsyncEventObserver::initialize(framebuffer_info_t& fbInfo)
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mExitThread = false;
    mEnabled = false;
    mDevice = mDisplayDevice.getId();
    // mFbHnd = Utils::checkAndDupFd(fbInfo.fd);
    mFbHnd = fbInfo.fd;
    if (mFbHnd < 0) {
        DEINIT_AND_RETURN_FALSE("invalid fb handle, failed initial vsync observer.");
    }

    mThread = new VsyncEventPollThread(this);
    if (!mThread.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync event poll thread.");
    }

    mThread->run("VsyncEventObserver", PRIORITY_URGENT_DISPLAY);

    mInitialized = true;
    return true;
}

void VsyncEventObserver::deinitialize()
{
    if (mEnabled) {
        WTRACE("vsync is still enabled");
        control(false);
    }
    mInitialized = false;
    mExitThread = true;
    mEnabled = false;
    mCondition.signal();

    if (mThread.get()) {
        mThread->requestExitAndWait();
        mThread = NULL;
    }
}

void VsyncEventObserver::setRefreshPeriod(nsecs_t period)
{
    Mutex::Autolock _l(mLock);

    if (period <= 0) {
        WTRACE("invalid refresh period %d", period);
    } else if (mRefreshPeriod != period) {
        mRefreshPeriod = period;
    }
}

bool VsyncEventObserver::control(bool enabled)
{
    ATRACE("enabled = %d on device %d", enabled, mDevice);
    if (enabled == mEnabled) {
        WTRACE("vsync state %d is not changed", enabled);
        return true;
    }

    Mutex::Autolock _l(mLock);

    mEnabled = enabled;
    mCondition.signal();
    return true;
}

bool VsyncEventObserver::wait(int64_t& timestamp)
{
    int64_t ret = 0;

    if (ioctl(mFbHnd, FBIO_WAITFORVSYNC, &timestamp) == -1) {
        ETRACE("fb ioctl vsync wait error, fb handle: %d", mFbHnd);
        return false;
    } else {
        if (timestamp != 0) {
            if (mDebug) {
                nsecs_t period = timestamp - mPreTimeStamp;
                if (mPreTimeStamp != 0) VTRACE("wait for vsync success, peroid: %lld", period);
                mPreTimeStamp = timestamp;
            }
            return true;
        } else {
            ETRACE("wait for vsync fail");
            return false;
        }
    }
}

bool VsyncEventObserver::threadLoop()
{
    {
        // scope for lock
        Mutex::Autolock _l(mLock);
        while (!mEnabled) {
            mCondition.wait(mLock);
            if (mExitThread) {
                ITRACE("exiting thread loop");
                return false;
            }
        }
    }

    if (mEnabled) {
        int64_t timestamp;

        // Sleep for a vsync if the display is not connected, or if we
        // fail to wait for the vsync signal for some reason. Otherwise
        // this thread will run unthrottled...
        if (!mDisplayDevice.isConnected() ||
            !wait(timestamp)) {
            WTRACE("failed to wait for vsync on display %d, vsync enabled %d", mDevice, mEnabled);
            usleep(mRefreshPeriod/1000);
            return true;
        }

        // send vsync event notification.
        mDisplayDevice.onVsync(timestamp);
    }

    return true;
}

} // namespace amlogic
} // namesapce android
