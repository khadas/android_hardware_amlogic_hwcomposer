/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <HwDisplayVsync.h>
#include <HwDisplayManager.h>

HwDisplayVsync::HwDisplayVsync(bool softwareVsync,
    HwVsyncObserver * observer) {
    mSoftVsync = softwareVsync;
    mObserver = observer;
    mEnabled = false;
    mPreTimeStamp = 0;

    run("displayVsync", android::PRIORITY_URGENT_DISPLAY);
}

HwDisplayVsync::~HwDisplayVsync() {
    std::unique_lock<std::mutex> stateLock(mStatLock);
    mExit = true;
    stateLock.unlock();
    mStateCondition.notify_all();

    requestExitAndWait();
}

int32_t HwDisplayVsync::setPeriod(nsecs_t period) {
    mPeriod = period;
    return 0;
}

int32_t HwDisplayVsync::setEnabled(bool enabled) {
    std::unique_lock<std::mutex> stateLock(mStatLock);
    mEnabled = true;
    stateLock.unlock();
    mStateCondition.notify_all();
    return 0;
}

bool HwDisplayVsync::threadLoop() {
    std::unique_lock<std::mutex> stateLock(mStatLock);

    while (!mEnabled) {
        mStateCondition.wait(stateLock);
        if (mExit) {
            MESON_LOGD("exit vsync loop");
            return false;
        }
    }
    stateLock.unlock();

    nsecs_t timestamp;
    int32_t ret;
    if (mSoftVsync) {
        ret = waitSoftwareVsync(timestamp);
    } else {
        ret = HwDisplayManager::getInstance().waitVBlank(timestamp);
    }
    bool debug = false;
    if (debug) {
        nsecs_t period = timestamp - mPreTimeStamp;
        if (mPreTimeStamp != 0)
            MESON_LOGD("wait for vsync success, peroid: %lld", period);
        mPreTimeStamp = timestamp;
    }

    if ( ret == 0 && mObserver) {
        mObserver->onVsync(timestamp);
    }
    return true;
}

int32_t HwDisplayVsync::waitSoftwareVsync(nsecs_t& vsync_timestamp) {
    static nsecs_t vsync_time = 0;
    static nsecs_t old_vsync_period = 0;
    nsecs_t sleep;
    nsecs_t now = systemTime(CLOCK_MONOTONIC);

    //cal the last vsync time with old period
    if (mPeriod != old_vsync_period) {
        if (old_vsync_period > 0) {
            vsync_time = vsync_time +
                    ((now - vsync_time) / old_vsync_period) * old_vsync_period;
        }
        old_vsync_period = mPeriod;
    }

    //set to next vsync time
    vsync_time += mPeriod;

    // we missed, find where the next vsync should be
    if (vsync_time - now < 0) {
        vsync_time = now + (mPeriod -
                 ((now - vsync_time) % mPeriod));
    }

    struct timespec spec;
    spec.tv_sec  = vsync_time / 1000000000;
    spec.tv_nsec = vsync_time % 1000000000;

    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);
    vsync_timestamp = vsync_time;

    return err;
}

