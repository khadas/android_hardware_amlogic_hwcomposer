/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>
#include <MesonLog.h>

#include "VtDisplayThread.h"

VtDisplayThread::VtDisplayThread(Hwc2Display * display) {
    mDisplay = display;
    mExit = false;
    needRefresh = false;
    snprintf(mName, 64, "VtDisplayThread-%d", mDisplay->getDisplayId());

    createThread();
}

VtDisplayThread::~VtDisplayThread() {
    MESON_LOGD("%s %s, Destroy Video Tunnel Display Thread", mName, __func__);
    mExit = true;
    pthread_join(mVtDisplayThread, NULL);
}

int32_t VtDisplayThread::createThread() {
    int32_t ret;

    ret = pthread_create(&mVtDisplayThread, NULL, VtDisplayThreadMain, (void *)this);
    if (ret) {
        MESON_LOGE("%s %s, failed to create VideoTunnel CmdThread: %s",
            mName, __func__, strerror(ret));
    }

    return ret;
}

void VtDisplayThread::destroyThread() {
    std::lock_guard<std::mutex> lock(mMutex);
    mExit = true;
}

void VtDisplayThread::onVtVsync(int64_t timestamp, uint32_t vsyncPeriodNanos) {
    ATRACE_CALL();
    //set vsync info to videotunnel driver
    if (VideoTunnelDev::getInstance().setDisplayVsyncInfo(timestamp, vsyncPeriodNanos) < 0)
        MESON_LOGV("%s %s, failed set display vsync info to videotunnel", mName, __func__);

    std::unique_lock<std::mutex> stateLock(mMutex);
    needRefresh = true;
    stateLock.unlock();
    mVtCondition.notify_all();

}
void VtDisplayThread::onFrameAvailableForGameMode() {
    ATRACE_CALL();

    std::unique_lock<std::mutex> stateLock(mMutex);
    needRefresh = true;
    stateLock.unlock();
    mVtCondition.notify_all();
}
void* VtDisplayThread::VtDisplayThreadMain(void *data) {
    VtDisplayThread* pThis = (VtDisplayThread*)data;

    // set videotunnel thread to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        MESON_LOGE("%s %s, Couldn't set SCHED_FIFO for videotunnelthread",
            pThis->mName, __func__);
    }

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("%s %s, VtDisplayThread exit video tunnel loop",
                pThis->mName, __func__);
            pthread_exit(0);
            return NULL;
        }

        pThis->handleVtDisplay();
    }

}

void VtDisplayThread::handleVtDisplay() {
    ATRACE_CALL();
    int32_t outPresentFence = -1;

    std::unique_lock<std::mutex> stateLock(mMutex);
    while (!needRefresh) {
        mVtCondition.wait(stateLock);
    }
    needRefresh = false;
    stateLock.unlock();

    {
        std::unique_lock<std::mutex> stateLock(mRefrashMutex);
        do {
            mDisplay->setVtLayersPresentTime();
            if (mDisplay->handleVtDisplayConnection()) {
                mDisplay->presentVtVideo(&outPresentFence);

                /* need close the present fence */
                if (outPresentFence >= 0)
                    close(outPresentFence);
            }
            mDisplay->releaseVtLayers();
        } while (mDisplay->newGameBuffer());

    }
}

