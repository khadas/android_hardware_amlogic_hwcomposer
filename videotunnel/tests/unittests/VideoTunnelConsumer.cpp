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
    mTimes = 0;
    mReleaseFence = -1;
    mTimeLine = std::make_shared<SyncTimeline>();
}

VideoTunnelConsumer::VideoTunnelConsumer(int tunnelId) : mTunnelId(tunnelId) {
    //VideoTunnelConsumer();
    mDevFd = meson_vt_open();
    mTimes = 0;
    mReleaseFence = -1;
    mTimeLine = std::make_shared<SyncTimeline>();
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
    int64_t timeStamp = 0;
    int ret;

    if (block) {
        do {
            ret = meson_vt_acquire_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd, &timeStamp);
        } while (ret == -EAGAIN);
    } else {
        ret = meson_vt_acquire_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd, &timeStamp);
    }

    item.setBufferFd(bufferFd);
    item.setTimeStamp(timeStamp);

    return ret;
}

int VideoTunnelConsumer::releaseBuffer(VTBufferItem &item) {
    SyncFence fence(mTimeLine.get(), mTimes);
    mTimes++;
    mReleaseFence = fence.getFd();

    return meson_vt_release_buffer(mDevFd, mTunnelId, item.getBufferFd(), mReleaseFence);
}

int VideoTunnelConsumer::recvCmd(enum vt_cmd &cmd, struct vt_cmd_data &data, bool block) {
    int ret;
    if (block) {
        do {
            ret = meson_vt_recv_cmd(mDevFd, mTunnelId, &cmd, &data);
        } while (ret == -EAGAIN);
    } else {
        ret = meson_vt_recv_cmd(mDevFd, mTunnelId, &cmd, &data);
    }

    return ret;
}

int VideoTunnelConsumer::setBlockMode(bool block) {
    return meson_vt_set_mode(mDevFd, (block ? 1 : 0));
}

