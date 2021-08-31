/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#ifndef _MESON_VIDEO_TUNNEL_CONSUMER_H
#define _MESON_VIDEO_TUNNEL_CONSUMER_H

#include <sys/types.h>
#include <vector>

#include <video_tunnel.h>
#include <VTBufferItem.h>
#include <SyncFence.h>

class VideoTunnelConsumer {
public:
    VideoTunnelConsumer();
    VideoTunnelConsumer(int tunnelId);
    ~VideoTunnelConsumer();

    int consumerConnect();
    int consumerDisconnect();
    int acquireBuffer(VTBufferItem &item, bool block = true);
    int releaseBuffer(VTBufferItem &item);
    int recvCmd(enum vt_cmd &cmd, struct vt_cmd_data &data, bool block = true);
    int setBlockMode(bool block);

    int handleCmd();
    int getReleaseFence() { return mReleaseFence; }

protected:
    int mDevFd;
    int mTunnelId;
    int mTimes;
    int mReleaseFence;
    std::shared_ptr<SyncTimeline> mTimeLine;
};

#endif /* _MESON_VIDEO_TUNNEL_CONSUMER_H */
