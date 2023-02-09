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
#include <misc.h>
#include <cutils/properties.h>
#include "WBDisplayThread.h"

WBDisplayThread::WBDisplayThread(Hwc2Display * display) {
    mDisplay = display;
    mExit = false;
    needRefresh = false;
    snprintf(mName, 64, "WBDisplayThread-%d", mDisplay->getDisplayId());

    createThread();
}

WBDisplayThread::~WBDisplayThread() {
    MESON_LOGD("%s %s, Destroy White Board Display Thread", mName, __func__);
    mExit = true;
    pthread_join(mWBDisplayThread, NULL);
}

int32_t WBDisplayThread::createThread() {
    int32_t ret;

    ret = pthread_create(&mWBDisplayThread, NULL, WBDisplayThreadMain, (void *)this);
    if (ret) {
        MESON_LOGE("%s %s, failed to create White board CmdThread: %s",
            mName, __func__, strerror(ret));
    }

    return ret;
}

void WBDisplayThread::destroyThread() {
    std::lock_guard<std::mutex> lock(mMutex);
    mExit = true;
}

void WBDisplayThread::onWBVsync(int64_t timestamp  __unused, uint32_t vsyncPeriodNanos __unused) {
    ATRACE_CALL();
    std::unique_lock<std::mutex> stateLock(mMutex);
    needRefresh = true;
    stateLock.unlock();
    mWBCondition.notify_all();
}

void* WBDisplayThread::WBDisplayThreadMain(void *data) {
    WBDisplayThread* pThis = (WBDisplayThread*)data;

    // set white board display thread to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        MESON_LOGE("%s %s, Couldn't set SCHED_FIFO for white board thread",
            pThis->mName, __func__);
    }

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("%s %s, WBDisplayThread exit white board loop",
                pThis->mName, __func__);
            pthread_exit(0);
            return NULL;
        }

        pThis->handleWBDisplay();
    }

}

void WBDisplayThread::handleWBDisplay() {
    ATRACE_CALL();

    std::unique_lock<std::mutex> stateLock(mMutex);
    while (!needRefresh) {
        mWBCondition.wait(stateLock);
    }
    needRefresh = false;
    stateLock.unlock();

    {
        std::unique_lock<std::mutex> stateLock(mRefrashMutex);
        do {
            //TODO callback SF refresh will removes
            mDisplay->mObserver->refresh();
        } while (0);

    }

}
