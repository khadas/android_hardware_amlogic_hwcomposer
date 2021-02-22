/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HW_DISPLAY_VSYNC_H
#define HW_DISPLAY_VSYNC_H

#include <mutex>
#include <condition_variable>
#include <utils/threads.h>
#include <time.h>
#include <pthread.h>

#include <HwDisplayCrtc.h>

class HwcVsyncObserver {
public:
    virtual ~HwcVsyncObserver() {}
    virtual void onVsync(int64_t timestamp, uint32_t vsyncPeriodNanos) = 0;
    virtual void onVTVsync(int64_t timestamp, uint32_t vsyncPeriodNanos) = 0;
};

class HwcVsync {
public:
    HwcVsync();
    ~HwcVsync();

    int32_t setObserver(HwcVsyncObserver * observer);
    int32_t setSoftwareMode();
    int32_t setMixMode();
    int32_t setHwMode(std::shared_ptr<HwDisplayCrtc> & crtc);
    int32_t setPeriod(nsecs_t period);
    int32_t setEnabled(bool enabled);
    int32_t setVideoTunnelEnabled(bool enabled);

    void dump(String8 & dumpstr);

protected:
    static void * vsyncThread(void * data);
    int32_t waitSoftwareVsync(nsecs_t& vsync_timestamp);
    int32_t waitMixVsync(nsecs_t& vsync_timestamp);
    int32_t waitHwVsync(nsecs_t& vsync_timestamp);

protected:
    bool mSoftVsync;
    bool mEnabled;
    bool mVTEnabled;
    bool mExit;
    /* mix software and hardware vsync */
    bool mMixVsync;
    bool mMixRebase;

    nsecs_t mVsyncTime;
    nsecs_t mReqPeriod;
    nsecs_t mPreTimeStamp;

    HwcVsyncObserver * mObserver;
    std::shared_ptr<HwDisplayCrtc> mCrtc;

    std::mutex mStatLock;
    std::condition_variable mStateCondition;
    pthread_t hw_vsync_thread;
};

#endif/*HW_DISPLAY_VSYNC_H*/
