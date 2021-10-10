/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#include <errno.h>
#include <VideoTunnelProducer.h>

VideoTunnelProducer::VideoTunnelProducer() : mTunnelId (-1) {
    mDevFd = meson_vt_open();
}

VideoTunnelProducer::VideoTunnelProducer(int tunnelId) {
    mDevFd = meson_vt_open();
    mTunnelId = tunnelId;
}

VideoTunnelProducer::~VideoTunnelProducer() {
    if (mDevFd >= 0)
        meson_vt_close(mDevFd);
}

int VideoTunnelProducer::producerConnect() {
    return meson_vt_connect(mDevFd, mTunnelId, 0);
}

int VideoTunnelProducer::producerDisconnect() {
    return meson_vt_disconnect(mDevFd, mTunnelId, 0);
}

int VideoTunnelProducer::queueBuffer(VTBufferItem &item) {
	return meson_vt_queue_buffer(mDevFd, mTunnelId,
            item.getBufferFd(), -1, item.getTimeStamp());
}

int VideoTunnelProducer::dequeueBuffer(VTBufferItem &item, bool block) {
    int bufferFd = -1;
    int fenceFd = -1;
    int ret;

    if (block) {
        do {
            ret = meson_vt_dequeue_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd);
        } while (ret == -EAGAIN);
    } else {
        ret = meson_vt_dequeue_buffer(mDevFd, mTunnelId, &bufferFd, &fenceFd);
    }

    item.setBufferFd(bufferFd);
    item.setReleaseFenceFd(fenceFd);

    return ret;
}

int VideoTunnelProducer::cancelBuffer() {
    return meson_vt_cancel_buffer(mDevFd, mTunnelId);
}

int VideoTunnelProducer::sendCmd(vt_cmd cmd, int data) {
    return meson_vt_send_cmd(mDevFd, mTunnelId, cmd, data);
}

int VideoTunnelProducer::allocVideoTunnelId() {
    if (mDevFd < 0) {
        return -1;
    }

	return meson_vt_alloc_id(mDevFd, &mTunnelId);
}

int VideoTunnelProducer::freeVideoTunnelId() {
    if (mTunnelId >= 0 && mDevFd >= 0) {
		meson_vt_free_id(mDevFd, mTunnelId);
    }

    return 0;
}
