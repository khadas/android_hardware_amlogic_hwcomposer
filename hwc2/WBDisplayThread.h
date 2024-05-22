/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef WB_DISPLAY_THREAD_H
#define WB_DISPLAY_THREAD_H

#include <mutex>
#include <pthread.h>
#include <utils/threads.h>
#include <condition_variable>
#include "Hwc2Display.h"

class WBDisplayThread {
public:
    WBDisplayThread(Hwc2Display * display);
    ~WBDisplayThread();
    void onWBVsync(int64_t timestamp __unused, uint32_t vsyncPeriodNanos __unused);

protected:
    static void *WBDisplayThreadMain(void *data);
    void handleWBDisplay();
    int32_t createThread();
    void destroyThread();


protected:
    bool mExit;
    bool needRefresh;
    std::mutex mMutex;
    std::mutex mRefrashMutex;
    std::condition_variable mWBCondition;
    pthread_t mWBDisplayThread;
    Hwc2Display * mDisplay;
    char mName[64];
};

#endif
