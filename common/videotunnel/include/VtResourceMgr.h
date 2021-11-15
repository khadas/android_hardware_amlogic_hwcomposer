/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef MESON_VT_RESOURCE_MGR
#define MESON_VT_RESOURCE_MGR

#include <memory>
#include <map>

#include <utils/Singleton.h>
#include <video_tunnel.h>
#include <VtConsumer.h>

class VtResourceMgr : public android::Singleton<VtResourceMgr> {
public:
    VtResourceMgr();
    ~VtResourceMgr();

    int32_t connectInstance(int tunnelId, std::shared_ptr<VtConsumer> & consumer);
    int32_t disconnectInstance(int tunnelId, std::shared_ptr<VtConsumer> & consumer);

    int32_t acquireBuffer();
    int32_t recieveCmd();

private:
    //std::map<int, std::shared_ptr<VtInstance>> mInstances;
};

#endif /* MESON_VT_RESOURCE_MGR */
