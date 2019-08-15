/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "FixedDisplayPipe.h"
#include <HwcConfig.h>
#include <MesonLog.h>

FixedDisplayPipe::FixedDisplayPipe()
    : HwcDisplayPipe() {
}

FixedDisplayPipe::~FixedDisplayPipe() {
}

int32_t FixedDisplayPipe::init(
    std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps) {
    HwcDisplayPipe::init(hwcDisps);
    return 0;
}

int32_t FixedDisplayPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    drm_connector_type_t  connector = getConnetorCfg(hwcid);
    cfg.hwcCrtcId = CRTC_VOUT1;
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    cfg.modeCrtcId = cfg.hwcCrtcId;
    cfg.modeConnectorType = cfg.hwcConnectorType = connector;
    return 0;
}
