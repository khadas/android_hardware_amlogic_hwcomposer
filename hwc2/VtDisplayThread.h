/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VT_DISPLAY_THREAD_H
#define VT_DISPLAY_THREAD_H

#include <mutex>
#include <pthread.h>
#include <utils/threads.h>
#include <condition_variable>


#include "Hwc2Display.h"
#include "VideoTunnelDev.h"

class VtDisplayThread {
public:
    VtDisplayThread(Hwc2Display * display);
    ~VtDisplayThread();
    void onVtVsync(int64_t timestamp, uint32_t vsyncPeriodNanos);
    void onFrameAvailableForGameMode();

protected:
    static void *VtDisplayThreadMain(void *data);
    void handleVtDisplay();
    int32_t createThread();
    void destroyThread();

protected:
    bool mExit;
    bool needRefresh;
    std::mutex mMutex;
    std::condition_variable mVtCondition;
    pthread_t mVtDisplayThread;
    Hwc2Display * mDisplay;
};

#endif
