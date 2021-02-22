/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VIDEO_TUNNEL_THREAD_H
#define VIDEO_TUNNEL_THREAD_H

#include "Hwc2Display.h"
#include "Hwc2Layer.h"

#include <vector>
#include <mutex>
#include <pthread.h>
#include <utils/threads.h>
#include <condition_variable>

class VideoTunnelThread {
public:
        VideoTunnelThread(Hwc2Display * display);
        ~VideoTunnelThread();

        void onVtVsync(int64_t timestamp, uint32_t vsyncPeriodNanos);

protected:
        void handleVideoTunnelLayers();
        int createThread();
        static void *threadMain(void * data);

protected:
        bool mExit;
        bool mVsyncComing;
        bool mSkipValidate;
        pthread_t mVtThread;
        std::mutex mVtLock;
        std::condition_variable mVtCondition;
        Hwc2Display * mDisplay;
};

#endif
