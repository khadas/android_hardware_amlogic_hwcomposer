/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#ifndef _MESON_VIDEO_TUNNEL_PRODUCER_H
#define _MESON_VIDEO_TUNNEL_PRODUCER_H

#include <video_tunnel.h>
#include <VTBufferItem.h>

class VideoTunnelProducer {
public:
    VideoTunnelProducer();
    VideoTunnelProducer(int tunnelId);
    ~VideoTunnelProducer();

    int producerConnect();
    int producerDisconnect();

    int queueBuffer(VTBufferItem &item);
    int dequeueBuffer(VTBufferItem &item, bool block = true);
    int sendCmd(vt_cmd cmd, int data);

    int getVideoTunnelId() const { return mTunnelId; };
    int allocVideoTunnelId();
    int freeVideoTunnelId();

protected:

    int mDevFd;
    int mTunnelId;
};

#endif /* _MESON_VIDEO_TUNNEL_PRODUCER_H */
