/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define VT_DEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include "VideoTunnelThread.h"
#include "VideoTunnelDev.h"
#include "MesonHwc2.h"
#include <MesonLog.h>

#include <condition_variable>

VideoTunnelThread::VideoTunnelThread(Hwc2Display * display) {
    mExit = false;
    mVsyncComing = false;
    mDisplay = display;

    createThread();
}

VideoTunnelThread::~VideoTunnelThread() {
    MESON_LOGD("%s, Destroy VideoTunnelThread thread", __func__);
    mExit = true;
    pthread_join(mVtBufferThread, NULL);
    pthread_join(mVtCmdThread, NULL);
}

int VideoTunnelThread::createThread() {
    int ret;

    MESON_LOGD("%s, Create VideoTunnelThread thread", __func__);

    ret = pthread_create(&mVtBufferThread, NULL, bufferThreadMain, (void *)this);
    if (ret) {
        MESON_LOGE("failed to create VideoTunnel BUfferThread: %s",
                strerror(ret));
        return ret;
    }

    ret = pthread_create(&mVtCmdThread, NULL, cmdThreadMain, (void *)this);
    if (ret) {
        MESON_LOGE("failed to create VideoTunnel CmdThread: %s",
                strerror(ret));
        return ret;
    }

    return 0;
}

void VideoTunnelThread::onVtVsync(int64_t timestamp __unused,
        uint32_t vsyncPeriodNanos __unused) {
    std::unique_lock<std::mutex> stateLock(mVtLock);
    mVsyncComing = true;
    stateLock.unlock();
    mVtCondition.notify_all();
}

void VideoTunnelThread::handleVideoTunnelLayers() {
    ATRACE_CALL();
    int32_t outPresentFence = -1;

    std::unique_lock<std::mutex> stateLock(mVtLock);
    while (!mVsyncComing) {
        mVtCondition.wait(stateLock);
    }
    mVsyncComing = false;
    stateLock.unlock();

    mDisplay->acquireVtLayers();

    if (mDisplay->handleVtDisplayConnection()) {
        mDisplay->presentDisplay(&outPresentFence, false);

        /* need close the present fence */
        if (outPresentFence >= 0)
            close(outPresentFence);
    }
    mDisplay->releaseVtLayers();
}

void* VideoTunnelThread::bufferThreadMain(void *data) {
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

int VideoTunnelThread::handleVideoTunnelCmds() {
    int32_t outPresentFence = -1;
    /*
     * poll videotunnel cmd,
     * will wait in videotunnel driver until recieve cmds
     */
    if (VideoTunnelDev::getInstance().pollCmds())
        return -EAGAIN;

    mDisplay->recieveVtCmds();

    // present display once to disable video
    mDisplay->presentDisplay(&outPresentFence, false);
    if (outPresentFence >= 0)
        close(outPresentFence);

    return 0;
}

// videotunnel cmd handle thread
void* VideoTunnelThread::cmdThreadMain(void *data) {
    VideoTunnelThread* pThis = (VideoTunnelThread*)data;

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("VideoTunnel Cmd Thread exit video tunnel loop");
            pthread_exit(0);
            return NULL;
        }

        pThis->handleVideoTunnelCmds();
    }

    return NULL;
}
