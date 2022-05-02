/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MULTI_DISPLAY_DRM_PIPE_H
#define MULTI_DISPLAY_DRM_PIPE_H
#include <HwcDisplayPipe.h>

class MultiDisplayDrmPipe : public HwcDisplayPipe {
public:
    MultiDisplayDrmPipe();
    ~MultiDisplayDrmPipe();

    int32_t init(std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps);
    void handleEvent(drm_display_event event, int val);
    void dump(String8 & dumpstr);

protected:
    int32_t getPipeCfg(uint32_t hwcid, PipeCfg & cfg);
};

#endif
