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
#ifndef __VSYNC_EVENT_OBSERVER_H__
#define __VSYNC_EVENT_OBSERVER_H__

#include <SimpleThread.h>
#include <Hwcomposer.h>
#include "framebuffer.h"

namespace android {
namespace amlogic {

class PhysicalDevice;

class VsyncEventObserver {
public:
    VsyncEventObserver(PhysicalDevice& disp);
    virtual ~VsyncEventObserver();

public:
    virtual bool initialize(framebuffer_info_t& framebufferInfo);
    virtual void deinitialize();
    virtual bool control(bool enabled);
    virtual void setRefreshPeriod(nsecs_t period);
    virtual nsecs_t getRefreshPeriod() const { return mRefreshPeriod; }

private:
    bool wait(int64_t& timestamp);

    mutable Mutex mLock;
    Condition mCondition;
    PhysicalDevice& mDisplayDevice;
    int     mDevice;
    int     mFbHnd;
    nsecs_t mRefreshPeriod;
    nsecs_t mPreTimeStamp;
    bool    mDebug;
    bool    mEnabled;
    bool    mExitThread;
    bool    mInitialized;

private:
    DECLARE_THREAD(VsyncEventPollThread, VsyncEventObserver);
};

} // namespace amlogic
} // namespace android



#endif /* __VSYNC_EVENT_OBSERVER_H__ */
