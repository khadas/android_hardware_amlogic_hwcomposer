/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "MultiDisplayDrmPipe.h"
#include <HwcConfig.h>
#include <MesonLog.h>
#include <HwDisplayManager.h>

MultiDisplayDrmPipe::MultiDisplayDrmPipe()
    : HwcDisplayPipe() {
}

MultiDisplayDrmPipe::~MultiDisplayDrmPipe() {
}

int32_t MultiDisplayDrmPipe::init(
    std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps) {
    drm_mode_info_t displayMode = {
        DRM_DISPLAY_MODE_NULL,
        0, 0,
        0, 0,
        60.0,
        0
    };

    HwcDisplayPipe::init(hwcDisps);

    for (auto stat : mPipeStats) {
        switch (stat.second->cfg.modeConnectorType) {
            case DRM_MODE_CONNECTOR_MESON_LVDS_A:
            case DRM_MODE_CONNECTOR_MESON_LVDS_B:
            case DRM_MODE_CONNECTOR_MESON_LVDS_C:
            case DRM_MODE_CONNECTOR_MESON_VBYONE_A:
            case DRM_MODE_CONNECTOR_MESON_VBYONE_B:
            case DRM_MODE_CONNECTOR_MESON_MIPI_A:
            case DRM_MODE_CONNECTOR_MESON_MIPI_B:
            case DRM_MODE_CONNECTOR_MESON_EDP_A:
            case DRM_MODE_CONNECTOR_MESON_EDP_B:
            case DRM_MODE_CONNECTOR_LVDS:
            case DRM_MODE_CONNECTOR_HDMIA:
                {
                    std::map<uint32_t, drm_mode_info_t> modes;
                    stat.second->modeConnector->getModes(modes);
                    strcpy(displayMode.name, modes[0].name);
                    stat.second->modeCrtc->setMode(displayMode);
                    MESON_LOGI("init connector[%d] mode [%s]",
                        stat.second->cfg.modeConnectorType,
                        displayMode.name);
                }
                break;
            default:
                MESON_LOGE("unknown connector type %d",
                    stat.second->cfg.modeConnectorType);
                break;
        }
    }

    return 0;
}

int32_t MultiDisplayDrmPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    switch (hwcid) {
        case 0:
            cfg.hwcPipeIdx = DRM_PIPE_VOUT1;
            break;
        case 1:
            cfg.hwcPipeIdx = DRM_PIPE_VOUT2;
            break;
        case 2:
            cfg.hwcPipeIdx = DRM_PIPE_VOUT3;
            break;
        default:
            MESON_LOGE("Unsupported display id %d", hwcid);
            return -EINVAL;
    };
    cfg.modePipeIdx = cfg.hwcPipeIdx;
    cfg.modeConnectorType = cfg.hwcConnectorType = getConnetorCfg(hwcid);
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    return 0;
}

void MultiDisplayDrmPipe::handleEvent(drm_display_event event, int val) {
    if (event == DRM_EVENT_HDMITX_HOTPLUG) {
        MESON_LOGD("Hotplug handle value %d.",val);
        bool connected = (val == 0) ? false : true;
        for (auto statIt : mPipeStats) {
            if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
                statIt.second->modeConnector->update();
                statIt.second->hwcDisplay->onHotplug(connected);
                /*update to default mode.*/
                drm_mode_info_t displayMode;
                std::map<uint32_t, drm_mode_info_t> modes;
                statIt.second->modeConnector->getModes(modes);
                strcpy(displayMode.name, modes[0].name);
                statIt.second->modeCrtc->setMode(displayMode);
                MESON_LOGD("%s:update hdmi to mode [%s]", __func__, displayMode.name);
            }
        }
    } else {
        HwcDisplayPipe::handleEvent(event, val);
    }
}

void MultiDisplayDrmPipe::dump(String8 & dumpstr) {
    getHwDisplayManager()->dump(dumpstr);
    dumpstr.append("DisplayPipe:[MultiDisplayDrmPipe]\n");
}
