/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

//#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <MesonLog.h>
#include "VideoTunnelDev.h"
#include <poll.h>

#define VT_ROLE_CONSUMER  1
#define VT_POLL_TIMEOUT 32  //ms

ANDROID_SINGLETON_STATIC_INSTANCE(VideoTunnelDev)

VideoTunnelDev::VideoTunnelDev() {
    mDrvFd = meson_vt_open();
    MESON_ASSERT(mDrvFd > 0, "videotunnel dev open failed");
    // set videotunnel to non block mode by default
    int ret = meson_vt_set_mode(mDrvFd, 0);
    if (ret != 0)
        MESON_LOGE("videotunnelDev set to non block mode failed(%d)", ret);
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

int32_t VideoTunnelDev::recieveCmd(int tunnelId, enum vt_cmd& cmd, struct vt_cmd_data& cmdData) {
    return meson_vt_recv_cmd(mDrvFd, tunnelId, &cmd, &cmdData);
}

/*
 * Return of -EAGAIN means the operation timeout or error.
 * On success, return of 0 means videotunnel can receive cmds
 */
int32_t VideoTunnelDev::pollGameModeBuffer() {
    struct pollfd fds[1];
    fds[0].fd = mDrvFd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    int pollrtn = poll(fds, 1, VT_POLL_TIMEOUT);
    if (pollrtn > 0) {
        /* can ready to read */
        if (fds[0].revents == POLLIN)
            return 0;
        else if (fds[0].revents == POLLERR)
            return -EINVAL;
    }

    return -EAGAIN;
}

int32_t VideoTunnelDev::setNonBlockMode() {
    return meson_vt_set_mode(mDrvFd, 0);
}

int32_t VideoTunnelDev::pollCmds() {
    return meson_vt_poll_cmd(mDrvFd, VT_POLL_TIMEOUT);
}

int32_t VideoTunnelDev::setDisplayVsyncInfo(uint64_t timestamp, uint32_t vsyncPeriodNanos) {
    // ignored tunnel id now
    return meson_vt_setDisplayVsyncAndPeroid(mDrvFd, -1, timestamp, vsyncPeriodNanos);
}
