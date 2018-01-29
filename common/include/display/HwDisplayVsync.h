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

class HwVsyncObserver {
public:
    HwVsyncObserver() {}
    virtual ~HwVsyncObserver() {}
    virtual void onVsync(int64_t timestamp) = 0;
};

class HwDisplayVsync : public android::Thread {
public:
    HwDisplayVsync(bool softwareVsync, HwVsyncObserver * observer);
    ~HwDisplayVsync();

    int32_t setEnabled(bool enabled);

    /*for software vsync.*/
    int32_t setPeriod(nsecs_t period);

    void dump();

protected:
    bool threadLoop();
    int32_t waitSoftwareVsync(nsecs_t& vsync_timestamp);


protected:
    bool mSoftVsync;
    bool mEnabled;
    bool mExit;
    nsecs_t mPeriod;

    HwVsyncObserver * mObserver;

    std::mutex mStatLock;
    std::condition_variable mStateCondition;
};

#endif/*HW_DISPLAY_VSYNC_H*/
