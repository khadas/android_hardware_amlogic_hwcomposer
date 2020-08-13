/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "HwcDisplayPipe.h"
#include "FixedDisplayPipe.h"
#include "LoopbackDisplayPipe.h"
#include "DualDisplayPipe.h"
#include <HwcConfig.h>
#include <systemcontrol.h>
#include <misc.h>

#include <HwDisplayManager.h>

#define HWC_BOOTED_PROP "vendor.sys.hwc.booted"
#define DEFAULT_REFRESH_RATE (60.0f)

HwcDisplayPipe::PipeStat::PipeStat(uint32_t id) {
    hwcId = id;
    cfg.hwcCrtcId = cfg.modeCrtcId = 0;
    cfg.hwcConnectorType = cfg.modeConnectorType = DRM_MODE_CONNECTOR_Unknown;
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
}

HwcDisplayPipe::PipeStat::~PipeStat() {
    hwcDisplay.reset();
    hwcCrtc.reset();
    hwcConnector.reset();
    hwcPlanes.clear();

    hwcPostProcessor.reset();
    modeMgr.reset();
    modeCrtc.reset();
    modeConnector.reset();
}

HwcDisplayPipe::HwcDisplayPipe() {
    /*load display resources.*/
    getHwDisplayManager()->getCrtcs(mCrtcs);
    getHwDisplayManager()->getPlanes(mPlanes);
}

HwcDisplayPipe::~HwcDisplayPipe() {
}

int32_t HwcDisplayPipe::init(std::map<uint32_t, std::shared_ptr<HwcDisplay>> & hwcDisps) {
    std::lock_guard<std::mutex> lock(mMutex);
    HwDisplayEventListener::getInstance().registerHandler(
        DRM_EVENT_ALL, (HwDisplayEventHandler*)this);

    for (auto dispIt = hwcDisps.begin(); dispIt != hwcDisps.end(); dispIt++) {
        uint32_t hwcId = dispIt->first;
        std::shared_ptr<PipeStat> stat = std::make_shared<PipeStat>(hwcId);
        mPipeStats.emplace(hwcId, stat);

        /*set HwcDisplay*/
        stat->hwcDisplay = dispIt->second;
        /*set modeMgr*/
        uint32_t fbW = 0, fbH = 0;
        HwcConfig::getFramebufferSize (hwcId, fbW, fbH);
        std::shared_ptr<HwcModeMgr> modeMgr =
        createModeMgr(HwcConfig::getModePolicy(hwcId));
        modeMgr->setFramebufferSize(fbW, fbH);
        stat->modeMgr = modeMgr;
        /*create vsync.*/
        stat->hwcVsync = std::make_shared<HwcVsync>();
        /*init display pipe.*/
        updatePipe(stat);

        /* in case of composer servce restart */
        if (sys_get_bool_prop(HWC_BOOTED_PROP, false)) {
            MESON_LOGD("composer service has restarted, need blank display");
            stat->hwcDisplay->blankDisplay();
        }

    }

    return 0;
}

int32_t HwcDisplayPipe::getCrtc(
    int32_t crtcid, std::shared_ptr<HwDisplayCrtc> & crtc) {
    for (auto crtcIt = mCrtcs.begin(); crtcIt != mCrtcs.end(); ++ crtcIt) {
        if ((*crtcIt)->getId() == crtcid) {
            crtc = *crtcIt;
            break;
        }
    }
    MESON_ASSERT(crtc != NULL, "get crtc %d failed.", crtcid);

    return 0;
}

int32_t HwcDisplayPipe::getPlanes(
    int32_t crtcid, std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    planes.clear();
    for (auto planeIt = mPlanes.begin(); planeIt != mPlanes.end(); ++ planeIt) {
        std::shared_ptr<HwDisplayPlane> plane = *planeIt;
        if (plane->getPossibleCrtcs() & crtcid) {
            if ((plane->getPlaneType() == CURSOR_PLANE) &&
                HwcConfig::cursorPlaneDisabled()) {
                continue;
            }
            planes.push_back(plane);
        }
    }
    MESON_ASSERT(planes.size() > 0, "get planes for crtc %d failed.", crtcid);
    return 0;
}

int32_t HwcDisplayPipe::getConnector(
    drm_connector_type_t type, std::shared_ptr<HwDisplayConnector> & connector) {
    auto it = mConnectors.find(type);
    if (it != mConnectors.end()) {
        connector = it->second;
    } else {
        getHwDisplayManager()->getConnector(connector, type);
        mConnectors.emplace(type, connector);
        /*TODO: init current status, for we may need it later.*/
        connector->update();
    }

    return 0;
}

int32_t HwcDisplayPipe::getPostProcessor(
    hwc_post_processor_t type, std::shared_ptr<HwcPostProcessor> & processor) {
    UNUSED(type);
    UNUSED(processor);
    processor = NULL;
    return 0;
}

drm_connector_type_t HwcDisplayPipe::getConnetorCfg(uint32_t hwcid) {
    drm_connector_type_t  connector = DRM_MODE_CONNECTOR_Unknown;
    switch (HwcConfig::getConnectorType(hwcid)) {
        case HWC_PANEL_ONLY:
            connector = DRM_MODE_CONNECTOR_LVDS;
            break;
        case HWC_HDMI_ONLY:
            connector = DRM_MODE_CONNECTOR_HDMIA;
            break;
        case HWC_CVBS_ONLY:
            connector = DRM_MODE_CONNECTOR_TV;
            break;
        case HWC_HDMI_CVBS:
            {
                std::shared_ptr<HwDisplayConnector> hwConnector;
                getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);
                if (hwConnector->isConnected()) {
                    connector = DRM_MODE_CONNECTOR_HDMIA;
                } else {
                    connector = DRM_MODE_CONNECTOR_TV;
                }
            }
            break;
        default:
            MESON_ASSERT(0, "Unknow connector config");
            break;
    }

    return connector;
}

int32_t HwcDisplayPipe::updatePipe(std::shared_ptr<PipeStat> & stat) {
    MESON_LOGD("HwcDisplayPipe::updatePipe.");

    PipeCfg cfg;
    getPipeCfg(stat->hwcId, cfg);

    if (memcmp((const void *)&cfg, (const void *)&(stat->cfg), sizeof(PipeCfg)) == 0) {
            MESON_LOGD("Config is not updated.");
            return 0;
    }

    /*update pipestats*/
    bool resChanged = false;
    if (cfg.hwcCrtcId != stat->cfg.hwcCrtcId) {
        getCrtc(cfg.hwcCrtcId, stat->hwcCrtc);
        getPlanes(cfg.hwcCrtcId, stat->hwcPlanes);
        stat->cfg.hwcCrtcId = cfg.hwcCrtcId;
        resChanged = true;
    }
    if (cfg.hwcConnectorType != stat->cfg.hwcConnectorType) {
        getConnector(cfg.hwcConnectorType, stat->hwcConnector);
        stat->cfg.hwcConnectorType = cfg.hwcConnectorType;
        resChanged = true;
    }
    if (cfg.hwcPostprocessorType != stat->cfg.hwcPostprocessorType) {
        getPostProcessor(cfg.hwcPostprocessorType, stat->hwcPostProcessor);
        stat->cfg.hwcPostprocessorType = cfg.hwcPostprocessorType;
        resChanged = true;
    }
    if (cfg.modeCrtcId != stat->cfg.modeCrtcId) {
        getCrtc(cfg.modeCrtcId, stat->modeCrtc);
        stat->cfg.modeCrtcId = cfg.modeCrtcId;
        resChanged = true;
    }
    if (cfg.modeConnectorType != stat->cfg.modeConnectorType) {
        getConnector(cfg.modeConnectorType, stat->modeConnector);
        stat->cfg.modeConnectorType = cfg.modeConnectorType;
        resChanged = true;
    }

    MESON_LOGD("HwcDisplayPipe::updatePipe (%d) [%s], crtc (%d) connector (%d)",
        stat->hwcId, resChanged ? "CHANGED" : "NOT-CHANGED",
        cfg.hwcCrtcId, cfg.hwcConnectorType);

    if (resChanged) {
        /*reset vout displaymode, it will be null.*/
        MESON_LOGD("HwcDisplayPipe::updatePipe %d changed", stat->hwcId);
        stat->hwcCrtc->unbind();
        stat->hwcCrtc->bind(stat->hwcConnector, stat->hwcPlanes);
        stat->hwcCrtc->loadProperities();
        stat->hwcCrtc->update();

        if (cfg.modeCrtcId != cfg.hwcCrtcId) {
            std::vector<std::shared_ptr<HwDisplayPlane>> planes;
            getPlanes (cfg.modeCrtcId,  planes);
            stat->modeCrtc->unbind();
            stat->modeCrtc->bind(stat->modeConnector, planes);
            stat->modeCrtc->loadProperities();
            stat->modeCrtc->update();
        }

        stat->modeMgr->setDisplayResources(stat->modeCrtc, stat->modeConnector);
        stat->modeMgr->update();

        MESON_LOGD("updatePipe connector:%s, connected:%d", stat->hwcConnector->getName(),
                stat->hwcConnector->isConnected());

        if (HwcConfig::softwareVsyncEnabled() || stat->hwcConnector->isConnected() == false) {
            stat->hwcVsync->setSoftwareMode();
        } else {
            stat->hwcVsync->setHwMode(stat->modeCrtc);
        }

        drm_mode_info_t mode;
        if (0 == stat->modeMgr->getDisplayMode(mode)) {
            stat->hwcVsync->setPeriod(1e9 / mode.refreshRate);
        }

        stat->hwcDisplay->setVsync(stat->hwcVsync);
        stat->hwcDisplay->setModeMgr(stat->modeMgr);
        stat->hwcDisplay->setDisplayResource(
            stat->hwcCrtc, stat->hwcConnector, stat->hwcPlanes);

        stat->hwcDisplay->setPostProcessor(stat->hwcPostProcessor);
    }

    return 0;
}

int32_t HwcDisplayPipe::handleRequest(uint32_t flags) {
    UNUSED(flags);
    return 0;
}

void HwcDisplayPipe::handleEvent(drm_display_event event, int val) {
    std::lock_guard<std::mutex> lock(mMutex);
    switch (event) {
        case DRM_EVENT_HDMITX_HDCP:
            {
                MESON_LOGD("Hdcp handle value %d.", val);
                for (auto statIt : mPipeStats) {
                    if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
                        statIt.second->modeCrtc->update();
                        statIt.second->hwcDisplay->onUpdate((val == 0) ? false : true);
                    }
                }
            }
            break;
        case DRM_EVENT_HDMITX_HOTPLUG:
            {
                MESON_LOGD("Hotplug handle value %d.",val);
                bool connected = (val == 0) ? false : true;
                for (auto statIt : mPipeStats) {
                    if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMIA) {
                        statIt.second->modeConnector->update();
                        statIt.second->hwcDisplay->onHotplug(connected);
                    } else if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_TV) {
                        /*
                         * Now Hdmi is plugIn. Switch from cvbs to hdmi.
                         * onHotplug DISCONNECT for CVBS in onHotplug(true) of Hwc2Display.
                         * onHotplug CONNECT for hdmi in onModeChanged() of Hwc2Display
                         * when displayPipe is ready.
                         */
                        if (connected == true) {
                            MESON_LOGD("hdmi connected, switch from cvbs");
                            statIt.second->hwcDisplay->onHotplug(connected);
                        }
                    }
                }
            }
            break;
        /*VIU1 mode changed.*/
        case DRM_EVENT_VOUT1_MODE_CHANGED:
        case DRM_EVENT_VOUT2_MODE_CHANGED:
            {
                MESON_LOGD("ModeChange state: [%s]", val == 1 ? "Complete" : "Begin to change");
                if (val == 1) {
                    int crtcid = CRTC_VOUT1;
                    if (event == DRM_EVENT_VOUT2_MODE_CHANGED)
                        crtcid = CRTC_VOUT2;
                    for (auto statIt : mPipeStats) {
                        if (statIt.second->modeCrtc->getId() == crtcid) {
                            statIt.second->modeCrtc->loadProperities();
                            statIt.second->modeCrtc->update();
                            statIt.second->modeMgr->update();
                            statIt.second->hwcDisplay->onModeChanged(val);
                            /*update display dynamic info.*/
                            drm_mode_info_t mode;
                            if (0 == statIt.second->modeMgr->getDisplayMode(mode)) {
                                statIt.second->hwcVsync->setPeriod(1e9 / mode.refreshRate);
                                if (HwcConfig::softwareVsyncEnabled()) {
                                    statIt.second->hwcVsync->setSoftwareMode();
                                } else {
                                    statIt.second->hwcVsync->setHwMode(statIt.second->modeCrtc);
                                }
                            } else {
                                /* could not get mode, switch to software vsync */
                                statIt.second->hwcVsync->setPeriod(1e9 / DEFAULT_REFRESH_RATE);
                                statIt.second->hwcVsync->setSoftwareMode();
                            }
                        }
                    }
                } else {
                    for (auto statIt : mPipeStats) {
                        statIt.second->hwcDisplay->onModeChanged(val);
                    }
                }
            }
            break;

        default:
            MESON_LOGE("Receive unhandled event %d", event);
            break;
    }
}

int32_t HwcDisplayPipe::initDisplayMode(std::shared_ptr<PipeStat> & stat) {
    switch (stat->cfg.modeConnectorType) {
        case DRM_MODE_CONNECTOR_TV:
            {
                #if 0
                const char * cvbs_config_key = "ubootenv.var.cvbsmode";
                std::string modeName;
                if (0 == sc_read_bootenv(cvbs_config_key, modeName)) {
                    stat->modeCrtc->writeCurDisplayMode(modeName);
                }
                #else
                /*TODO:*/
                #endif
            }
            break;
        case DRM_MODE_CONNECTOR_HDMIA:
            {
                /*TODO:*/
            }
            break;
        case DRM_MODE_CONNECTOR_LVDS:
            {
                /*TODO*/
            }
            break;
        default:
            MESON_LOGE("Do Nothing in updateDisplayMode .");
            break;
    };

    return 0;
}

std::shared_ptr<HwcDisplayPipe> createDisplayPipe(hwc_pipe_policy_t pipet) {
    switch (pipet) {
        case HWC_PIPE_DEFAULT:
            return std::make_shared<FixedDisplayPipe>();
        case HWC_PIPE_DUAL:
            return std::make_shared<DualDisplayPipe>();
        case HWC_PIPE_LOOPBACK:
            return std::make_shared<LoopbackDisplayPipe>();
        default:
            MESON_ASSERT(0, "unknown display pipe %d", pipet);
    };

    return NULL;
}


