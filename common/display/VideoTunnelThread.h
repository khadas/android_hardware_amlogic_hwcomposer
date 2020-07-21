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

#include <vector>
#include <mutex>

#include <DrmFramebuffer.h>
#include "VideoComposerDev.h"

#include <pthread.h>
#include <utils/threads.h>
#include "video_tunnel.h"

class VideoTunnelThread {
    public:
        VideoTunnelThread(int32_t composerFd, drm_fb_type_t type);
        ~VideoTunnelThread();

        int start();
        void stop();
        int setPlane(std::shared_ptr<DrmFramebuffer> fb,uint32_t id, uint32_t zorder);

    protected:
        static void *threadMain(void * data);
        int handleVideoTunnelBuffers();

    protected:
        enum {
            PROCESSOR_START = 0,
            PROCESSOR_STOP,
        };

        bool mThreadExit;
        int mVTFd;
        int mVideoTunnelId;
        int mStat;
        int mDrvFd;
        video_frames_info_t mVideoFramesInfo;
        pthread_t mVideoTunnelThread;
};

#endif
