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

#include <video_tunnel.h>
#include <VtConsumer.h>

class VtInstance : public VtConsumer::VtReleaseListener {
public:
    VtInstance();
    virtual ~VtInstance();

    int32_t registerVtConsumer(std::shared_ptr<VtConsumer> & consumer);
    int32_t unregisterVtConsumer(std::shared_ptr<VtConsumer> & consumer);

    int32_t onFrameRelease(VtBufferItem &item, int fenceFd) override;

private:
    int mTunnelId;
    std::vector<std::shared_ptr<VtConsumer>> mConsumers;

    // todo need define vtBuffer?
};

#endif  /* MESON_VT_INSTANCE_H */
