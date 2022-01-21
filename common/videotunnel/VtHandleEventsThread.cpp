/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "VtHandleEventsThread.h"
#include "VtInstanceMgr.h"

VtHandleEventsThread::VtHandleEventsThread() {
    mExit = false;
    createThread();
}

VtHandleEventsThread::~VtHandleEventsThread() {
    MESON_LOGD("%s, Destroy VtHandleEventsThread thread", __func__);
    if (!mExit)
        pthread_join(mVtThread, NULL);
}

int VtHandleEventsThread::createThread() {
    int ret = -1;

    MESON_LOGD("%s, Create VtHandleEventsThread thread", __func__);

    ret = pthread_create(&mVtThread, NULL, vtThreadMain, (void *)this);
    if (ret) {
        MESON_LOGE("failed to create VideoTunnel BufferThread: %s",
                strerror(ret));
        return ret;
    }

    return ret;
}

void VtHandleEventsThread::startThread() {
    std::unique_lock<std::mutex> stateLock(mVtMutex);
    if (mExit) {
        pthread_join(mVtThread, NULL);
        mExit = false;
        createThread();
    }
    stateLock.unlock();
}

void VtHandleEventsThread::stopThread() {
    std::unique_lock<std::mutex> stateLock(mVtMutex);
    if (!mExit) {
        mExit = true;
        pthread_join(mVtThread, NULL);
    }
    stateLock.unlock();
}

void *VtHandleEventsThread::vtThreadMain(void * data) {
    VtHandleEventsThread* pThis = (VtHandleEventsThread*)data;
    int ret;

    // set videotunnel thread to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        MESON_LOGE("Couldn't set SCHED_FIFO for VtHandleEventsThread");
    }

    while (true) {
        if (pThis->mExit) {
            MESON_LOGD("VtHandleEventsThread exit video tunnel loop");
            pthread_exit(0);
            return NULL;
        }

        ret = pThis->handleVideoTunnelEvents();
        if (ret != 0 && ret != -EAGAIN) {
            MESON_LOGD("will exit handle Video Tunnel Events Thread ret:%d",
                    ret);
            pThis->mExit = true;
        }
    }

}

int32_t VtHandleEventsThread::handleVideoTunnelEvents() {
    int32_t ret = -1;
    VideoTunnelDev::VtPollStatus status = VtInstanceMgr::getInstance().pollVtEvents();

    if (status == VideoTunnelDev::VtPollStatus::eBufferReady) {
        ret = VtInstanceMgr::getInstance().handleBuffers();
        if (ret < 0 && ret != -EAGAIN)
            return ret;
    } else if (status == VideoTunnelDev::VtPollStatus::eCmdReady) {
        ret = VtInstanceMgr::getInstance().handleCmds();
        if (ret < 0 && ret != -EAGAIN)
            return ret;
    } else if (status == VideoTunnelDev::VtPollStatus::eNotReady) {
        MESON_LOGV("VtHandleEventsThread::%s, current not found valid buffers and cmds",
            __func__);
        ret = 0;
    }

    return ret;
}
