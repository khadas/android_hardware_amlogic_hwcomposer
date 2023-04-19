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

#define DRM_DISPLAY_MODE_PANEL ("panel")
#define DRM_DISPLAY_MODE_DEFAULT ("1080p60hz")
#define DRM_DISPLAY_ATTR_DEFAULT ("444,8bit")
#define LCD_MUTE   "/sys/class/lcd/test"

DualDisplayPipe::DualDisplayPipe()
    : HwcDisplayPipe() {
        /*Todo:init status need to get from??*/
        mPrimaryConnectorType = DRM_MODE_CONNECTOR_Unknown;
        mExtendConnectorType = DRM_MODE_CONNECTOR_Unknown;
        mHdmi_connected = false;
}

DualDisplayPipe::~DualDisplayPipe() {
}

int32_t DualDisplayPipe::init(
    std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps) {
    /*update hdmi connect status*/
    std::shared_ptr<HwDisplayConnector> hwConnector;
    getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);
    hwConnector->update();
    mHdmi_connected = hwConnector->isConnected();

    HwcDisplayPipe::init(hwcDisps);

    MESON_ASSERT(HwcConfig::getDisplayNum() == 2,
        "DualDisplayPipe need 2 hwc display.");

    /*check dual pipeline display mode both init status*/
    bool init = true;
    for (auto stat : mPipeStats) {
        drm_mode_info_t curMode;
        int32_t ret = stat.second->modeCrtc->getMode(curMode);
        if ((ret < 0 || !strcmp(curMode.name, DRM_DISPLAY_MODE_NULL)) &&
            stat.second->modeConnector->isConnected()) {
            init = false;
        }
    }
    if (init == true)
        return 0;
    /*reinit dual display pipeline display mode*/
    drm_mode_info_t displayMode = {
        DRM_DISPLAY_MODE_NULL,
        0, 0,
        0, 0,
        60.0,
        0
    };
    /*set vout displaymode*/
    for (auto stat : mPipeStats) {
        switch (stat.second->cfg.modeConnectorType) {
            case DRM_MODE_CONNECTOR_TV:
                {
                    /*ToDo: add cvbs support*/
                }
                break;
            case DRM_MODE_CONNECTOR_HDMIA:
                {
                    /*move to lateInit()*/
                }
                break;
            case DRM_MODE_CONNECTOR_LVDS:
            case DRM_MODE_CONNECTOR_MESON_LVDS_A:
            case DRM_MODE_CONNECTOR_MESON_LVDS_B:
            case DRM_MODE_CONNECTOR_MESON_LVDS_C:
            case DRM_MODE_CONNECTOR_MESON_VBYONE_A:
            case DRM_MODE_CONNECTOR_MESON_VBYONE_B:
            case DRM_MODE_CONNECTOR_MESON_MIPI_A:
            case DRM_MODE_CONNECTOR_MESON_MIPI_B:
            case DRM_MODE_CONNECTOR_MESON_EDP_A:
            case DRM_MODE_CONNECTOR_MESON_EDP_B:
            case LEGACY_NON_DRM_CONNECTOR_PANEL:
                {
                    std::map<uint32_t, drm_mode_info_t> panelModes;
                    stat.second->modeConnector->getModes(panelModes);
                    strcpy(displayMode.name, panelModes[0].name);
                    stat.second->modeCrtc->setMode(displayMode);
                }
                break;
            default:
                MESON_LOGE("Do Nothing in updateDisplayMode .");
                break;
        };
        MESON_LOGI("init set mode (%s)",displayMode.name);
    }
    return 0;
}

void DualDisplayPipe::lateInit() {
    drm_mode_info_t displayMode = {
       DRM_DISPLAY_MODE_NULL,
       0, 0,
       0, 0,
       60.0,
       0
    };
    if (mHdmi_connected == true) {
    /*update display mode*/
        for (auto statIt : mPipeStats) {
            MESON_LOGD("Update init display mode for HDMI");
            if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
                std::string displayattr(DRM_DISPLAY_ATTR_DEFAULT);
                std::string prefdisplayMode;
                /*get hdmi prefect display mode*/
                if (sc_get_pref_display_mode(prefdisplayMode) == false) {
                    strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                    MESON_LOGD("init sc_get_pref_display_mode fail! use default mode");
                } else {
                    if (strcmp("null", prefdisplayMode.c_str()) == 0 ) {
                        strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                    } else {
                        strcpy(displayMode.name, prefdisplayMode.c_str());
                    }
                }
                MESON_LOGD("HDMI INIT SET display mode %s.", displayMode.name);
                std::string dispmode(displayMode.name);
                sc_set_display_mode(dispmode);
            }
        }
    }
}

int32_t DualDisplayPipe::getPipeCfg(uint32_t hwcid, PipeCfg & cfg) {
    /*get hdmi hpd state firstly for init default config*/
    drm_connector_type_t  connector = getConnectorCfg(hwcid);

    if (connector == DRM_MODE_CONNECTOR_HDMIA) {
        std::shared_ptr<HwDisplayConnector> hwConnector;
        getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);

        switch (mEventState) {
        // plug out and suspend
        case DRM_EVENT_DISABLE :
        case DRM_EVENT_SUSPEND:
            if (hasDummyConnector()) {
                connector = DRM_MODE_CONNECTOR_VIRTUAL;
            }
            break;
        // plug in
        case DRM_EVENT_ENABLE :
            connector = DRM_MODE_CONNECTOR_HDMIA;
            break;
        // wake up
        case DRM_EVENT_RESUME:
        // init
        default:
            if (hwConnector->isConnected()) {
                connector = DRM_MODE_CONNECTOR_HDMIA;
            } else {
                if (hasDummyConnector()) {
                    connector = DRM_MODE_CONNECTOR_VIRTUAL;
                }
            }
        }
    }

    if (hwcid == 0) {
        if (HwcConfig::dynamicSwitchViuEnabled() == true &&
            mHdmi_connected == true &&
            connector == DRM_MODE_CONNECTOR_LVDS) {
            cfg.hwcPipeIdx = DRM_PIPE_VOUT2;
        } else {
            cfg.hwcPipeIdx = DRM_PIPE_VOUT1;
        }
        mPrimaryConnectorType = connector;
    } else if (hwcid == 1) {
        if (HwcConfig::dynamicSwitchViuEnabled() == true &&
            mHdmi_connected == true &&
            connector == DRM_MODE_CONNECTOR_HDMIA)
            cfg.hwcPipeIdx = DRM_PIPE_VOUT1;
        else
            cfg.hwcPipeIdx = DRM_PIPE_VOUT2;

        mExtendConnectorType = connector;
    }
    MESON_LOGD("dual pipe line getpipecfg hwcid=%d, pipeidx = %d, connector=%d.",
        hwcid, cfg.hwcPipeIdx, connector);
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
    cfg.modePipeIdx = cfg.hwcPipeIdx;
    if (HwcConfig::dynamicSwitchConnectorEnabled() == true &&
        (mPrimaryConnectorType == DRM_MODE_CONNECTOR_HDMIA ||
         mPrimaryConnectorType == DRM_MODE_CONNECTOR_VIRTUAL) &&
        mExtendConnectorType != DRM_MODE_CONNECTOR_Unknown) {
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
    return 0;
}

void DualDisplayPipe::handleEvent(drm_display_event event, int val) {
    if (event == DRM_EVENT_HDMITX_HOTPLUG) {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_LOGD("Hotplug handle value %d.",val);
        bool connected = (val == 0 || val == DRM_EVENT_SUSPEND) ? false : true;
        mEventState = val;

        std::shared_ptr<HwDisplayConnector> hwConnector;
        getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);
        hwConnector->update();
        mHdmi_connected = hwConnector->isConnected();

        drm_mode_info_t displayMode = {
            DRM_DISPLAY_MODE_NULL,
            0, 0,
            0, 0,
            60.0,
            0
        };

        /*reset vout displaymode, for we need do pipeline switch*/
        for (auto statIt : mPipeStats) {
            //TODO: need disable vsycn before setmode to null.
            std::string curMode;
            statIt.second->modeCrtc->readCurDisplayMode(curMode);
            if (strcmp(curMode.c_str(), "panel") == 0) {
                statIt.second->modeCrtc->setMode(displayMode);
                MESON_LOGE("set panel to NULL  displaymode ");
            }
        }

        MESON_LOGD("Update pipeline");
        /*update display pipe.*/
        for (auto statIt : mPipeStats) {
            updatePipe(statIt.second);
        }

        /*update display mode*/
        /*handle hdmi firstly, for vout2 bug which cost too much time may caused
         */
        for (auto statIt : mPipeStats) {
            auto connectorType = statIt.second->modeConnector->getType();
            if (connectorType == DRM_MODE_CONNECTOR_HDMIA ||
                    connectorType == DRM_MODE_CONNECTOR_VIRTUAL) {
                MESON_LOGD("Update display mode for HDMI");
                std::string displayattr(DRM_DISPLAY_ATTR_DEFAULT);
                std::string prefdisplayMode;
                if (connected) {
                    /*get hdmi prefect display mode*/
                    if (sc_get_pref_display_mode(prefdisplayMode) == false) {
                        strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                        prefdisplayMode = DRM_DISPLAY_MODE_DEFAULT;
                        MESON_LOGD("sc_get_pref_display_mode fail! use default mode");
                    } else {
                        if (strcmp("null", prefdisplayMode.c_str()) == 0 ) {
                            strcpy(displayMode.name, DRM_DISPLAY_MODE_DEFAULT);
                            prefdisplayMode = DRM_DISPLAY_MODE_DEFAULT;
                        } else {
                            strcpy(displayMode.name, prefdisplayMode.c_str());
                        }
                    }
                    statIt.second->modeConnector->update();
                    statIt.second->hwcDisplay->onHotplug(connected);

                    MESON_LOGD("HDMI SET display mode %s.", displayMode.name);
                    //strcpy(displayModeTemp.name, prefdisplayMode.c_str());
                    statIt.second->modeCrtc->setMode(displayMode);
                } else {
                    statIt.second->modeConnector->update();
                    statIt.second->hwcDisplay->onHotplug(connected);
                }
            }
        }

        for (auto statIt : mPipeStats) {
            if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_LVDS) {
                MESON_LOGD("Update display mode for PANEL");
                std::string lcd_mute("8");
                sc_write_sysfs(LCD_MUTE, lcd_mute);
                strcpy(displayMode.name, DRM_DISPLAY_MODE_PANEL);
                statIt.second->modeCrtc->setMode(displayMode);
                usleep(20000);
                std::string lcd_unmute("0");
                sc_write_sysfs(LCD_MUTE, lcd_unmute);
                MESON_LOGD("HwcDisplayPipe::handleEvent lcd unmute");
            }
        }
    } else {
        HwcDisplayPipe::handleEvent(event, val);
    }
}
