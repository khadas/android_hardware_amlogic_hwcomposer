/*
// Copyright(c) 2016 Amlogic Corporation
*/

#ifndef SOFT_VSYNC_OBSERVER_H
#define SOFT_VSYNC_OBSERVER_H

#include <SimpleThread.h>

namespace android {
namespace amlogic {

class IDisplayDevice;

class SoftVsyncObserver {
public:
    SoftVsyncObserver(IDisplayDevice& disp);
    virtual ~SoftVsyncObserver();

public:
    virtual bool initialize();
    virtual void deinitialize();
    virtual void setRefreshRate(int rate);
    virtual bool control(bool enabled);
    virtual nsecs_t getRefreshPeriod() const { return mRefreshPeriod; }

private:
    IDisplayDevice& mDisplayDevice;
    int  mDevice;
    bool mEnabled;
    int mRefreshRate;
    nsecs_t mRefreshPeriod;
    mutable Mutex mLock;
    Condition mCondition;
    mutable nsecs_t mNextFakeVSync;
    bool mExitThread;
    bool mInitialized;

private:
    DECLARE_THREAD(VsyncEventPollThread, SoftVsyncObserver);
};

} // namespace amlogic
} // namespace android



#endif /* SOFT_VSYNC_OBSERVER_H */

