/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VIDEO_TUNNEL_DEV_H
#define VIDEO_TUNNEL_DEV_H

#include <utils/Singleton.h>
#include <video_tunnel.h>

class VideoTunnelDev : public android::Singleton<VideoTunnelDev> {
public:
    VideoTunnelDev();
    ~VideoTunnelDev();

    enum class VtPollStatus { // flags for pollBufferAndCmds
        eBufferReady = 0x01,
        eCmdReady = 0x02,
        eNotReady = 0x03,
        eInvalidStatus = 0x04,
    };

    int32_t connect(int tunnelId);
    int32_t disconnect(int tunnelId);

    int32_t acquireBuffer(int tunnelId, int& bufferFd, int64_t& timeStamp);
    int32_t releaseBuffer(int tunnelId, int bufferFd, int fenceFd);
    int32_t recieveCmd(int tunnelId, enum vt_cmd& cmd, struct vt_cmd_data & cmdData);

    int32_t setNonBlockMode();
    int32_t pollGameModeBuffer();
    int32_t pollCmds();
    VtPollStatus pollBufferAndCmds();

    int32_t setDisplayVsyncInfo(uint64_t timestamp, uint32_t vsyncPeriodNanos);

private:
    int mDrvFd;
};

#endif /* VIDEO_TUNNEL_DEV_H */
