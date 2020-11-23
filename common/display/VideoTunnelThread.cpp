/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VideoTunnelThread.h"

#include <MesonLog.h>
#include <sys/ioctl.h>

#define VT_ROLE_CONSUMER (1)

VideoTunnelThread::VideoTunnelThread(int32_t composerFd, drm_fb_type_t type) {
    mThreadExit = false;
    mStat = PROCESSOR_STOP;

    if (type == DRM_FB_VIDEO_TUNNEL_SIDEBAND)
        mVideoTunnelId = 0;
    else
        mVideoTunnelId = 1;

    mDrvFd = composerFd;

    mVTFd = meson_vt_open();
    if (mVTFd < 0 ) {
        return;
    }
}

VideoTunnelThread::~VideoTunnelThread() {
    mThreadExit = true;
    meson_vt_disconnect(mVTFd, mVideoTunnelId, VT_ROLE_CONSUMER);

    pthread_join(mVideoTunnelThread, NULL);

    if (mVTFd >= 0) {
        meson_vt_close(mVTFd);
        mVTFd = -1;
    }

    if (mDrvFd >= 0) {
        close(mDrvFd);
        mDrvFd = -1;
    }

    mStat = PROCESSOR_STOP;
}

int VideoTunnelThread::start() {
    int ret;

    if (mDrvFd < 0 )
        return -1;

    if (mStat == PROCESSOR_START)
        return 0;

    MESON_LOGD("VideoTunnelThread %d start thread", mVideoTunnelId);
    ret = meson_vt_connect(mVTFd, mVideoTunnelId, VT_ROLE_CONSUMER);
    if (ret < 0) {
        close(mVTFd);
        mVTFd = -1;
        return ret;
    }

    mThreadExit = false;
    ret = pthread_create(&mVideoTunnelThread, NULL, threadMain, (void *)this);
    if (ret) {
        MESON_LOGE("failed to start VideoTunnelThread: %s",
                strerror(ret));
        mThreadExit = true;
        return ret;
    }

    mStat = PROCESSOR_START;
    return 0;
}

void VideoTunnelThread::stop() {
    if (mStat == PROCESSOR_STOP)
        return;

    MESON_LOGD("VideoTunnelThread %d, stop thread", mVideoTunnelId);
    mThreadExit = true;
    pthread_join(mVideoTunnelThread, NULL);

    meson_vt_disconnect(mVTFd, mVideoTunnelId, VT_ROLE_CONSUMER);
    mStat = PROCESSOR_STOP;
}

int VideoTunnelThread::setPlane (
        std::shared_ptr<DrmFramebuffer> fb,
        uint32_t id, uint32_t zorder) {
    video_frame_info_t *vFrameInfo;
    drm_rect_t dispFrame = fb->mDisplayFrame;
    drm_rect_t srcCrop = fb->mSourceCrop;

    vFrameInfo = &mVideoFramesInfo.frame_info[0];

    vFrameInfo->type = 0;
    vFrameInfo->sideband_type = 0;
    vFrameInfo->dst_x = dispFrame.left;
    vFrameInfo->dst_y = dispFrame.top;
    vFrameInfo->dst_w = dispFrame.right - dispFrame.left;
    vFrameInfo->dst_h = dispFrame.bottom - dispFrame.top;

    vFrameInfo->crop_x = srcCrop.left;
    vFrameInfo->crop_y = srcCrop.top;
    vFrameInfo->crop_w = srcCrop.right - srcCrop.left;
    vFrameInfo->crop_h = srcCrop.bottom - srcCrop.top;
    vFrameInfo->buffer_w = -1;
    vFrameInfo->buffer_h = -1;
    vFrameInfo->zorder = fb->mZorder;
    vFrameInfo->transform = fb->mTransform;

    mVideoFramesInfo.frame_count = 1;
    mVideoFramesInfo.layer_index = id;
    mVideoFramesInfo.disp_zorder = zorder;

    return 0;
}

int VideoTunnelThread::handleVideoTunnelBuffers() {
    int ret;
    int buffer_fd, fence_fd;
    int64_t time_stamp;
    video_frame_info_t *vFrameInfo;

    if (mVTFd < 0)
        return -1;

    ret = meson_vt_acquire_buffer(mVTFd, mVideoTunnelId,
                                  &buffer_fd, &fence_fd, &time_stamp);
    if (ret < 0)
        return ret;

    vFrameInfo = &mVideoFramesInfo.frame_info[0];
    vFrameInfo->fd = buffer_fd;
    if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_FRAMES, &mVideoFramesInfo) != 0) {
        MESON_LOGE("VideoTunnelThread %d, video composer ioctl error, %s(%d), mDrvFd = %d",
                    mVideoTunnelId, strerror(errno), errno, mDrvFd);
        return -1;
    }

    meson_vt_release_buffer(mVTFd,
                            mVideoTunnelId,
                            buffer_fd,
                            vFrameInfo->composer_fen_fd);

    return 0;
}

void* VideoTunnelThread::threadMain(void *data) {
    int ret;

    VideoTunnelThread* pThis = (VideoTunnelThread*)data;

    while (true) {
        if (pThis->mThreadExit) {
            MESON_LOGD("VideoTunnelThread %d, exit video tunnel loop",
                    pThis->mVideoTunnelId);
            pthread_exit(0);
        }

        ret = pThis->handleVideoTunnelBuffers();
        if (ret == -EAGAIN) {
            // acquire buffer timeout, try again;
            continue;
        }

        if (ret < 0) {
            MESON_LOGD("VideoTunnelThread %d, acquire buffer failed: %d, exit thread loop",
                    pThis->mVideoTunnelId, ret);
            pthread_exit(0);
        }
    }
}

