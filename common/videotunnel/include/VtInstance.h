/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef MESON_VT_INSTANCE_H
#define MESON_VT_INSTANCE_H

#include <memory>
#include <vector>

//#include <BitsMap.h>
#include <DrmSync.h>

#include "video_tunnel.h"
#include "VtConsumer.h"

class VtInstance : public VtConsumer::VtReleaseListener {
public:
    VtInstance(int tunnelId);
    virtual ~VtInstance();

    int32_t registerVtConsumer(std::shared_ptr<VtConsumer> & consumer);
    int32_t unregisterVtConsumer(std::shared_ptr<VtConsumer> & consumer);

    int32_t connect();
    void disconnect();
    int32_t acquireBuffer();
    void releaseBufferLocked(std::shared_ptr<VtBufferItem> & item);
    void releaseInstanceBuffers();
    int32_t recieveCmds();
    void setReleaseFence();

    int32_t onFrameDisplayed(int bufferFd, int fenceFd) override;

    bool needDestroyThisInstance();

private:
    int mTunnelId;
    std::vector<std::shared_ptr<VtConsumer>> mConsumers;
    std::vector<std::shared_ptr<VtBufferItem>> mBufferQueue;
    //std::shared_ptr<BitsMap> mSlotBitmap;
    std::mutex mMutex;

    char mName[64];

};

#endif  /* MESON_VT_INSTANCE_H */
