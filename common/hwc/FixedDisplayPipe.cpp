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
#include <misc.h>

#define HDMI_HAS_USED_STATE "/sys/class/amhdmitx/amhdmitx0/hdmi_used"

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

void FixedDisplayPipe::handleEvent(drm_display_event event, int val) {
    if (event == DRM_EVENT_HDMITX_HOTPLUG) {
        std::lock_guard<std::mutex> lock(mMutex);
        bool connected = (val == 0) ? false : true;
        std::shared_ptr<PipeStat> pipe;
        drm_connector_type_t targetConnector = DRM_MODE_CONNECTOR_Unknown;
        for (auto statIt : mPipeStats) {
            hwc_connector_t connectorType = HwcConfig::getConnectorType((int)statIt.first);
            pipe = statIt.second;

            /*update current connector status, now getpipecfg() need
            * read connector status to decide connector.
            */
            statIt.second->modeConnector->update();

            if (connectorType == HWC_HDMI_CVBS) {
                targetConnector = connected ?
                    DRM_MODE_CONNECTOR_HDMIA : DRM_MODE_CONNECTOR_TV;

                MESON_LOGD("handleEvent  DRM_EVENT_HDMITX_HOTPLUG %d VS %d",
                    pipe->cfg.hwcConnectorType, targetConnector);
                if (pipe->cfg.hwcConnectorType != targetConnector &&
                        pipe->cfg.hwcConnectorType == DRM_MODE_CONNECTOR_TV) {
                    #if 0 /*TODO: for fixed pipe, let systemcontrol to set displaymode.*/
                    getHwDisplayManager()->unbind(pipe->hwcCrtc);
                    getHwDisplayManager()->unbind(pipe->modeCrtc);
                    #endif

                    /* we need latest connector status, and no one will update
                    *connector not bind to crtc, we update here.
                    */
                    std::shared_ptr<HwDisplayConnector> hwConnector;
                    getConnector(targetConnector, hwConnector);
                    hwConnector->update();
                    /*update display pipe.*/
                    updatePipe(pipe);
                    /*update display mode, workaround now.*/
                    initDisplayMode(pipe);
                }
                statIt.second->hwcDisplay->onHotplug(connected);
            } else if (connectorType == HWC_HDMI_ONLY) {
                if (connected) {
                    initDisplayMode(pipe);
                }
                statIt.second->hwcDisplay->onHotplug(connected);
            }
        }
    }else {
        HwcDisplayPipe::handleEvent(event, val);
    }
}

int32_t FixedDisplayPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    drm_connector_type_t  connector = getConnetorCfg(hwcid);
    cfg.modePipeIdx = cfg.hwcPipeIdx = DRM_PIPE_VOUT1;
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    cfg.modeConnectorType = cfg.hwcConnectorType = connector;
    return 0;
}

drm_connector_type_t FixedDisplayPipe::getConnetorCfg(uint32_t hwcid) {
    drm_connector_type_t  connector = HwcDisplayPipe::getConnetorCfg(hwcid);
    if (connector == DRM_MODE_CONNECTOR_Unknown ||
            connector == DRM_MODE_CONNECTOR_TV) {
        std::shared_ptr<HwDisplayConnector> hwConnector;
        getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);
        if (hwConnector->isConnected() || hasHdmiConnected()) {
            connector = DRM_MODE_CONNECTOR_HDMIA;
        } else {
            connector = DRM_MODE_CONNECTOR_TV;
        }
    }

    MESON_LOGE("FixedDisplayPipe::getConnetorCfg %d", connector);
    return connector;
}

bool FixedDisplayPipe::hasHdmiConnected() {
    bool ret = sysfs_get_int(HDMI_HAS_USED_STATE, 0) == 1 ? true : false;
    MESON_LOGD("FixedDisplayPipe::hasHdmiConnected:%d", ret);
    return ret;
}
