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

    if (HwcConfig::getDisplayNum() == 2) {
        int displayId = 1;
        std::shared_ptr<PipeStat> stat = mPipeStats.find(displayId)->second;

        /* If display2 mode is null, set to default mode. */
        std::map<uint32_t, drm_mode_info_t> modes;
        stat->modeConnector->getModes(modes);
        MESON_ASSERT(modes.size() > 0, "no modes got.");

        MESON_LOGI("initDisplays viu2: set mode (%s)",modes[0].name);
        stat->hwcCrtc->setMode(modes[0]);
        stat->modeCrtc->setMode(modes[0]);
        stat->hwcCrtc->update();
        stat->modeMgr->update();
    }

    return 0;
}

int32_t FixedDisplayPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    drm_connector_type_t  connector = getConnetorCfg(hwcid);
    if (hwcid == 0) {
        cfg.hwcCrtcId = CRTC_VOUT1;
    } else if (hwcid == 1) {
        cfg.hwcCrtcId = CRTC_VOUT2;
    }
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    cfg.modeCrtcId = cfg.hwcCrtcId;
    cfg.modeConnectorType = cfg.hwcConnectorType = connector;
    return 0;
}
