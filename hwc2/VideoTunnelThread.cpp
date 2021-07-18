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
    mGameExit = false;
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
    ATRACE_CALL();
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

    {
        /* lock it as game mode thread may present vide too */
        std::lock_guard<std::mutex> lock(mMutex);
        mDisplay->acquireVtLayers();

        if (mDisplay->handleVtDisplayConnection()) {
            mDisplay->presentVideo(&outPresentFence);

            /* need close the present fence */
            if (outPresentFence >= 0)
                close(outPresentFence);
        }
        mDisplay->releaseVtLayers();
    }
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
    if (VideoTunnelDev::getInstance().pollCmds() <= 0)
        return -EAGAIN;

    int ret = mDisplay->recieveVtCmds();
    if (ret < 0)
        return ret;

    if (ret & VT_CMD_DISABLE_VIDEO) {
        // present display once to disable video
        mDisplay->presentVideo(&outPresentFence);
        if (outPresentFence >= 0)
            close(outPresentFence);
    } else if (ret & VT_CMD_GAME_MODE_ENABLE) {
        startGameMode();
    } else if (ret & VT_CMD_GAME_MODE_DISABLE) {
        stopGameMode();
    }

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

int VideoTunnelThread::startGameMode() {
    MESON_LOGD("%s", __func__);
    // start game thread
    mGameExit = false;
    if (!mGameExit) {
        int ret = pthread_create(&mVtGameModeThread, NULL, gameModeThreadMain, (void *)this);
        if (ret) {
            MESON_LOGE("failed to create VideoTunnel BUfferThread: %s",
                    strerror(ret));
            return ret;
        }
    } else {
        MESON_LOGD("%s game mode thread alread in running", __func__);
    }

    return 0;
}

int VideoTunnelThread::stopGameMode() {
    MESON_LOGD("%s", __func__);
    mGameExit = true;
    pthread_join(mVtGameModeThread, NULL);

    return 0;
}

int VideoTunnelThread::handleGameMode() {
    /*
     * poll videotunnel game buffer
     * will wait in videotunnel driver until recieve buffers
     */
    int32_t outPresentFence = -1;
    int ret = VideoTunnelDev::getInstance().pollGameModeBuffer();

   /* success */
   if (ret == 0) {
       std::lock_guard<std::mutex> lock(mMutex);
       do {
           mDisplay->acquireVtLayers();

           if (mDisplay->handleVtDisplayConnection()) {
               mDisplay->presentVideo(&outPresentFence);

               /* need close the present fence */
               if (outPresentFence >= 0)
                   close(outPresentFence);
           }
           mDisplay->releaseVtLayers();
       } while (mDisplay->newGameBuffer());
   }

    return ret;
}

void *VideoTunnelThread::gameModeThreadMain(void *data) {
    VideoTunnelThread* pThis = (VideoTunnelThread*)data;

    // set videotunnel thread to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        MESON_LOGE("Couldn't set SCHED_FIFO for videotunnelthread");
    }

    while (true) {
        if (pThis->mGameExit) {
            MESON_LOGD("VideoTunnel game mode Thread exit");
            pthread_exit(0);
            return NULL;
        }

        int ret = pThis->handleGameMode();
        if (ret != 0 && ret != -EAGAIN) {
            MESON_LOGE("handle Game mode error:%d, exit", ret);
            return NULL;
        }
    }

    return NULL;
}
