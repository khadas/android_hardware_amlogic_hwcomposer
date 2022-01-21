/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VT_HANDLE_EVENTS_THREAD_H
#define VT_HANDLE_EVENTS_THREAD_H

#include <mutex>
#include <pthread.h>
#include <utils/threads.h>

#include "VideoTunnelDev.h"

class VtHandleEventsThread {
public:
    VtHandleEventsThread();
    ~VtHandleEventsThread();
    void startThread();
    void stopThread();

protected:
    int createThread();
    static void *vtThreadMain(void * data);
    int32_t handleVideoTunnelEvents();

protected:
    bool mExit;
    std::mutex mVtMutex;

    pthread_t mVtThread;
};

#endif

