/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VideoTunnelThread.h"
#include "VideoTunnelDev.h"
#include <MesonLog.h>

#include <time.h>
#include <condition_variable>

#define VT_DEBUG 0

VideoTunnelThread::VideoTunnelThread(Hwc2Display * display) {
    mExit = false;
    mVsyncComing = false;
    mStat = PROCESSOR_STOP;
    mDisplay = display;
}

VideoTunnelThread::~VideoTunnelThread() {
    if (mStat != PROCESSOR_STOP) {
        mExit = true;
        pthread_join(mVtThread, NULL);
        mStat = PROCESSOR_STOP;
    }
}

int VideoTunnelThread::start() {
    int ret;
    if (mStat == PROCESSOR_START)
        return 0;

    MESON_LOGD("%s, VideoTunnelThread start thread", __func__);

    ret = pthread_create(&mVtThread, NULL, threadMain, (void *)this);
    if (ret) {
        MESON_LOGE("failed to start VideoTunnelThread: %s",
                strerror(ret));
        return ret;
    }

    mExit = false;
    mStat = PROCESSOR_START;
    return 0;
}

void VideoTunnelThread::stop() {
    if (mStat == PROCESSOR_STOP)
        return;

    MESON_LOGD("%s, VideoTunnelThread stop thread", __func__);
    mExit = true;
    pthread_join(mVtThread, NULL);
    mStat = PROCESSOR_STOP;
}

void VideoTunnelThread::onVtVsync(int64_t timestamp,
        uint32_t vsyncPeriodNanos __unused) {
    std::unique_lock<std::mutex> stateLock(mVtLock);
    mVsyncComing = true;
    mVsyncTimestamp = timestamp;
    stateLock.unlock();
    mVtCondition.notify_all();
}

void VideoTunnelThread::handleVideoTunnelLayers() {
    uint32_t outNumTypes, outNumRequests;
    int32_t outPresentFence = -1;
    struct timespec spec;

    spec.tv_sec = 0;
    spec.tv_nsec = 5000000;

    VideoTunnelDev::getInstance().pollBuffer();
    std::unique_lock<std::mutex> stateLock(mVtLock);
    while (!mVsyncComing) {
        mVtCondition.wait(stateLock);
    }
    mVsyncComing = false;
    stateLock.unlock();

    nanosleep(&spec, NULL); /* sleep 5ms */
    if (mDisplay->getPreDisplayTime() < mVsyncTimestamp) {
        if (mDisplay->handleVtDisplayConnection()) {
            mDisplay->validateDisplay(&outNumTypes, &outNumRequests);
            mDisplay->presentDisplay(&outPresentFence);
        }
    }
}

void* VideoTunnelThread::threadMain(void *data) {
    VideoTunnelThread* pThis = (VideoTunnelThread*)data;

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("VideoTunnelThread exit video tunnel loop");
            pthread_exit(0);
        }

        pThis->handleVideoTunnelLayers();
    }
}

