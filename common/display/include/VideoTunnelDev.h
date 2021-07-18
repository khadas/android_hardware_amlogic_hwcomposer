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

    int32_t connect(int tunnelId);
    int32_t disconnect(int tunnelId);

    int32_t acquireBuffer(int tunnelId, int& bufferFd, int64_t& timeStamp);
    int32_t releaseBuffer(int tunnelId, int bufferFd, int fenceFd);
    int32_t recieveCmd(int tunnelId, enum vt_cmd& cmd, int& cmdData);

    int32_t setNonBlockMode();
    int32_t pollGameModeBuffer();

    int32_t pollCmds();

private:
    int mDrvFd;
};

#endif /* VIDEO_TUNNEL_DEV_H */
