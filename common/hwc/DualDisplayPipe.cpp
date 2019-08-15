/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DualDisplayPipe.h"
#include <HwcConfig.h>
#include <MesonLog.h>
#include <systemcontrol.h>
#include <HwDisplayManager.h>
#include <hardware/hwcomposer2.h>

#define DRM_DISPLAY_MODE_PANEL ("panel")
#define DRM_DISPLAY_MODE_DEFAULT ("1080p60hz")

DualDisplayPipe::DualDisplayPipe()
    : HwcDisplayPipe() {
        /*Todo:init status need to get from??*/
        mPrimaryConnectorType = DRM_MODE_CONNECTOR_INVALID;
        mExtendConnectorType = DRM_MODE_CONNECTOR_INVALID;
        mHdmi_connected = true;
}

DualDisplayPipe::~DualDisplayPipe() {
}

int32_t DualDisplayPipe::init(
    std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps) {
    HwcDisplayPipe::init(hwcDisps);

    MESON_ASSERT(HwcConfig::getDisplayNum() == 2,
        "DualDisplayPipe need 2 hwc display.");

    /*set vout displaymode*/
    for (auto stat : mPipeStats) {
        #ifndef HWC_DYNAMIC_SWITCH_CONNECTOR
        drm_mode_info_t curMode;
        if (stat.second->modeCrtc->getMode(curMode) < 0 &&
            stat.second->modeConnector->isConnected()) {
            /*do not do crtc/connector update after set displaymode,
            will do it when we get mode change event.*/
            initDisplayMode(stat.second);
        }
        #else
        static drm_mode_info_t displayMode = {
            DRM_DISPLAY_MODE_PANEL,
            0, 0,
            0, 0,
            60.0
        };
        switch (stat.second->cfg.modeConnectorType) {
            case DRM_MODE_CONNECTOR_CVBS:
                {
                    const char * cvbs_config_key = "ubootenv.var.cvbsmode";
                    std::string modeName;
                    if (0 == sc_read_bootenv(cvbs_config_key, modeName)) {
                        stat.second->modeCrtc->writeCurDisplayMode(modeName);
                    }
                }
                break;
            case DRM_MODE_CONNECTOR_HDMI:
                {
                    strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                }
                break;
            case DRM_MODE_CONNECTOR_PANEL:
                {
                    strcpy(displayMode.name, DRM_DISPLAY_MODE_PANEL);
                }
                break;
            default:
                MESON_LOGE("Do Nothing in updateDisplayMode .");
                break;
        };
        stat.second->modeMgr->getDisplayAttribute(0, HWC2_ATTRIBUTE_WIDTH, (int32_t *)&displayMode.pixelW);
        stat.second->modeMgr->getDisplayAttribute(0, HWC2_ATTRIBUTE_HEIGHT, (int32_t *)&displayMode.pixelH);
        MESON_LOGI("set mode (%s):%dx%d",displayMode.name, displayMode.pixelW, displayMode.pixelH);
        stat.second->hwcCrtc->setMode(displayMode);
        stat.second->modeCrtc->setMode(displayMode);
        #endif
    }
    return 0;
}

int32_t DualDisplayPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    drm_connector_type_t  connector = getConnetorCfg(hwcid);
    if (hwcid == 0) {
        cfg.hwcCrtcId = CRTC_VOUT1;
        mPrimaryConnectorType = connector;
    } else if (hwcid == 1) {
        cfg.hwcCrtcId = CRTC_VOUT2;
        mExtendConnectorType = connector;
    }
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    cfg.modeCrtcId = cfg.hwcCrtcId;
    #ifdef HWC_DYNAMIC_SWITCH_CONNECTOR
    if (mPrimaryConnectorType == DRM_MODE_CONNECTOR_HDMI &&
        mExtendConnectorType != DRM_MODE_CONNECTOR_INVALID) {
        if (hwcid == 0) {
            if (mHdmi_connected == true)
                cfg.modeConnectorType = cfg.hwcConnectorType = mPrimaryConnectorType;
            else
                cfg.modeConnectorType = cfg.hwcConnectorType = mExtendConnectorType;
        }
        if (hwcid == 1) {
            if (mHdmi_connected == true)
                cfg.modeConnectorType = cfg.hwcConnectorType = mExtendConnectorType;
            else
                cfg.modeConnectorType = cfg.hwcConnectorType = mPrimaryConnectorType;
        }
    } else {
        cfg.modeConnectorType = cfg.hwcConnectorType = connector;
    }
    #else
    cfg.modeConnectorType = cfg.hwcConnectorType = connector;
    #endif
    return 0;
}

void DualDisplayPipe::handleEvent(drm_display_event event, int val) {
    if (event == DRM_EVENT_HDMITX_HOTPLUG) {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_LOGD("Hotplug handle value %d.",val);
        bool connected = (val == 0) ? false : true;
        mHdmi_connected = connected;
        #ifdef HWC_DYNAMIC_SWITCH_CONNECTOR
        static drm_mode_info_t displayMode = {
            DRM_DISPLAY_MODE_NULL,
            0, 0,
            0, 0,
            60.0
        };
        if (mPrimaryConnectorType == DRM_MODE_CONNECTOR_HDMI &&
            mExtendConnectorType != DRM_MODE_CONNECTOR_INVALID) {
            for (auto statIt : mPipeStats) {
                statIt.second->hwcCrtc->setMode(displayMode);
                statIt.second->modeCrtc->setMode(displayMode);
            }
            for (auto statIt : mPipeStats) {
                /*reset vout displaymode, for we need do pipeline switch*/
                statIt.second->hwcCrtc->unbind();
            }
            /*update display pipe.*/
            for (auto statIt : mPipeStats) {
                updatePipe(statIt.second);
            }
            /*update display mode*/
            for (auto statIt : mPipeStats) {
                if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMI) {
                    if (connected == false) {
                        strcpy(displayMode.name, DRM_DISPLAY_MODE_NULL);
                    } else {
                        strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                    }
                } else if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_PANEL) {
                    strcpy(displayMode.name, DRM_DISPLAY_MODE_PANEL);
                }
                statIt.second->modeMgr->getDisplayAttribute(0, HWC2_ATTRIBUTE_WIDTH, (int32_t *)&displayMode.pixelW);
                statIt.second->modeMgr->getDisplayAttribute(0, HWC2_ATTRIBUTE_HEIGHT, (int32_t *)&displayMode.pixelH);
                MESON_LOGI("set mode (%s):%dx%d",displayMode.name, displayMode.pixelW, displayMode.pixelH);
                statIt.second->hwcCrtc->setMode(displayMode);
                statIt.second->modeCrtc->setMode(displayMode);
                statIt.second->modeMgr->update();
            }
        }
        #endif
        for (auto statIt : mPipeStats) {
            if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMI) {
                statIt.second->modeConnector->update();
                statIt.second->hwcDisplay->onHotplug(connected);
            }
        }
    } else {
        MESON_LOGI("Receive DualDisplayPipe unhandled event %d", event);
        HwcDisplayPipe::handleEvent(event, val);
    }
}
