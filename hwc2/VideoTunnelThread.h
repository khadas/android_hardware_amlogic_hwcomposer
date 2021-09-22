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
    static void *bufferThreadMain(void * data);

    // for vt cmd process thread
    int handleVideoTunnelCmds();
    static void *cmdThreadMain(void *data);

    // game mode thread
    int stopGameMode();
    int startGameMode();
    int handleGameMode();
    static void *gameModeThreadMain(void *data);

protected:
    bool mExit;
    bool mGameExit;
    bool mVsyncComing;
    std::mutex mMutex;

    // videotunnel cmd thread
    pthread_t mVtCmdThread;
    pthread_t mVtBufferThread;
    pthread_t mVtGameModeThread;

    std::mutex mVtLock;
    std::condition_variable mVtCondition;
    Hwc2Display * mDisplay;

    // for game mode
    std::mutex mVtGameLock;
    std::condition_variable mVtGameCondition;
};

#endif
