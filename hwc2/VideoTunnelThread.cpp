/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define VT_DEBUG 1

#include "VideoTunnelThread.h"
#include "VideoTunnelDev.h"
#include "MesonHwc2.h"
#include <MesonLog.h>

#include <time.h>
#include <condition_variable>

VideoTunnelThread::VideoTunnelThread(Hwc2Display * display) {
    mExit = false;
    mVsyncComing = false;
    mStat = PROCESSOR_STOP;
    mDisplay = display;

    /* get capabilities */
    uint32_t capCount = 0;
    MesonHwc2::getInstance().getCapabilities(&capCount, nullptr);

    std::vector<int32_t> caps(capCount);
    MesonHwc2::getInstance().getCapabilities(&capCount, caps.data());
    caps.resize(capCount);
    /* check whether have skip validate capability */
    if (std::find(caps.begin(), caps.end(), HWC2_CAPABILITY_SKIP_VALIDATE) != caps.end())
        mSkipValidate = true;
    else
        mSkipValidate = false;
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

    std::unique_lock<std::mutex> stateLock(mVtLock);
    while (!mVsyncComing) {
        mVtCondition.wait(stateLock);
    }
    mVsyncComing = false;
    stateLock.unlock();

    nanosleep(&spec, NULL); /* sleep 5ms */
    // need exit?
    if (mExit)
        return;

    if (mDisplay->getPreDisplayTime() < mVsyncTimestamp) {
        if (mDisplay->handleVtDisplayConnection()) {
            if (mSkipValidate) {
                hwc2_error_t ret = mDisplay->presentDisplay(&outPresentFence);
                if (ret == HWC2_ERROR_NOT_VALIDATED) {
                    mDisplay->validateDisplay(&outNumTypes, &outNumRequests);
                    mDisplay->presentDisplay(&outPresentFence);
                }
            } else {
                mDisplay->validateDisplay(&outNumTypes, &outNumRequests);
                mDisplay->presentDisplay(&outPresentFence);
            }

            /* need close the present fence */
            if (outPresentFence >= 0)
                close(outPresentFence);
        }
    }
}

void* VideoTunnelThread::threadMain(void *data) {
    VideoTunnelThread* pThis = (VideoTunnelThread*)data;

    // set videotunnel thread to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        MESON_LOGE("Couldn't set SCHED_FIFO for videotunnelthread");
    }

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("VideoTunnelThread exit video tunnel loop");
            pthread_exit(0);
            return NULL;
        }

        pThis->handleVideoTunnelLayers();
    }
}

