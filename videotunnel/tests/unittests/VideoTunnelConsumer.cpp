/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#include <errno.h>

#include <VideoTunnelConsumer.h>
#include <video_tunnel.h>

VideoTunnelConsumer::VideoTunnelConsumer() {
    mDevFd = meson_vt_open();
}

VideoTunnelConsumer::VideoTunnelConsumer(int tunnelId) : mTunnelId(tunnelId) {
    mDevFd = meson_vt_open();
}

VideoTunnelConsumer::~VideoTunnelConsumer() {
    if (mDevFd >= 0)
        meson_vt_close(mDevFd);
}

int VideoTunnelConsumer::consumerConnect() {
    return meson_vt_connect(mDevFd, mTunnelId, 1);
}

int VideoTunnelConsumer::consumerDisconnect() {
    return meson_vt_disconnect(mDevFd, mTunnelId, 1);
}

int VideoTunnelConsumer::acquireBuffer(VTBufferItem &item, bool block) {
    int bufferFd;
    int fenceFd;
    int ret;

    if (block) {
        do {
            ret = meson_vt_acquire_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd);
        } while (ret == -EAGAIN);
    } else {
        ret = meson_vt_acquire_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd);
    }

    item.setBufferFd(bufferFd);

    return ret;
}

int VideoTunnelConsumer::releaseBuffer(VTBufferItem &item) {
    return meson_vt_release_buffer(mDevFd, mTunnelId, item.getBufferFd(), -1);
}

int VideoTunnelConsumer::recvCmd(enum vt_cmd &cmd, int &data, int &client, bool block) {
    int ret;
    if (block) {
        do {
            ret = meson_vt_recv_cmd(mDevFd, mTunnelId, &cmd, &data, &client);
        } while (ret == -EAGAIN);
    } else {
        ret = meson_vt_recv_cmd(mDevFd, mTunnelId, &cmd, &data, &client);
    }

    return ret;
}
