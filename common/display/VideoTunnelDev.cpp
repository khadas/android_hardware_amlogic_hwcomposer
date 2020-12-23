/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

//#define LOG_NDEBUG 1


#include <MesonLog.h>
#include "VideoTunnelDev.h"
#include <poll.h>

#define VT_ROLE_CONSUMER  1
#define VT_POOL_TIMEOUT 32  //ms

ANDROID_SINGLETON_STATIC_INSTANCE(VideoTunnelDev)

VideoTunnelDev::VideoTunnelDev() {
    mDrvFd = meson_vt_open();
    MESON_ASSERT(mDrvFd > 0, "videotunnel dev open failed");
}

VideoTunnelDev::~VideoTunnelDev() {
    if (mDrvFd > 0)
        meson_vt_close(mDrvFd);
}

int32_t VideoTunnelDev::connect(int tunnelId) {
    return meson_vt_connect(mDrvFd, tunnelId, VT_ROLE_CONSUMER);
}

int32_t VideoTunnelDev::disconnect(int tunnelId) {
    return meson_vt_disconnect(mDrvFd, tunnelId, VT_ROLE_CONSUMER);
}

int32_t VideoTunnelDev::acquireBuffer(int tunnelId, int& bufferFd, int64_t& timeStamp) {
    int fenceFd;
    return meson_vt_acquire_buffer(mDrvFd, tunnelId, &bufferFd, &fenceFd, &timeStamp);
}

int32_t VideoTunnelDev::releaseBuffer(int tunnelId, int bufferFd, int fenceFd) {
    return meson_vt_release_buffer(mDrvFd, tunnelId, bufferFd, fenceFd);
}

int32_t VideoTunnelDev::recieveCmd(int tunnelId, enum vt_cmd& cmd, int& cmdData) {
    int clientId;
    return meson_vt_recv_cmd(mDrvFd, tunnelId, &cmd, &cmdData, &clientId);
}

/*
 * Return of 0 means the operation timeout or error.
 * On success, return of 1 means videotunnel can acquire buffer
 */
int32_t VideoTunnelDev::pollBuffer() {
    struct pollfd fds[1];
    fds[0].fd = mDrvFd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    int pollrtn = poll(fds, 1, VT_POOL_TIMEOUT);
    if (pollrtn > 0 && fds[0].revents == POLLIN) {
        return 1;
    }

    return 0;
}
