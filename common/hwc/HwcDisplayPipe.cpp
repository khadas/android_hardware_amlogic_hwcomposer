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
#include "MultiDisplayDrmPipe.h"
#include <HwcConfig.h>
#include <systemcontrol.h>
#include <misc.h>
#include <HwDisplayManager.h>

#define HWC_BOOTED_PROP "vendor.sys.hwc.booted"
#define DEFAULT_REFRESH_RATE (60.0f)

HwcDisplayPipe::PipeStat::PipeStat(uint32_t hwc_id) {
    hwcId = hwc_id;
    cfg.hwcPipeIdx = cfg.modePipeIdx = DRM_PIPE_INVALID;
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
    std::vector<std::shared_ptr<HwDisplayPlane>> planes;
    getHwDisplayManager()->getPlanes(planes);

    /*assign planes to pipe/crtc.
     *1. assign plane with dedicate crtc mask.
     *2. assign osd planes: N + 1 + 1
     */
    int pipeidx = 0;
    std::shared_ptr<HwDisplayPlane> plane;
    int dispNum = HwcConfig::getDisplayNum();
    if (HwcConfig::getPipeline() == HWC_PIPE_LOOPBACK)
        dispNum += 1;

    /*hwcvideo/osd/legacy video/osd/primary from fbdev have deciate crtc mask.*/
    for (pipeidx = 0; pipeidx < dispNum; pipeidx ++) {
        for (auto planeIt = planes.begin(); planeIt != planes.end(); ) {
            plane = *planeIt;
            if (plane->getPossibleCrtcs() == (1 << pipeidx)) {
                mPlanesForPipe.insert(std::make_pair(pipeidx, plane));
                planeIt = planes.erase(planeIt);
            } else
                planeIt++;
        }
    }

    /*default policy: assign osd plane N + 1 + 1*/
    if (planes.size() > 0) {
        pipeidx = HwcConfig::getDisplayNum() - 1;
        for (auto planeIt = planes.rbegin(); planeIt != planes.rend(); planeIt ++) {
            plane = *planeIt;
            if (plane->getPossibleCrtcs() & (1 << pipeidx)) {
                mPlanesForPipe.insert(std::make_pair(pipeidx, plane));
                if (pipeidx >= 1)
                    pipeidx --;
            }
        }

        MESON_ASSERT(mPlanesForPipe.size() >= HwcConfig::getDisplayNum(),
            "planes-%zu < pipe-%d\n", mPlanesForPipe.size(), HwcConfig::getDisplayNum());
    }

    for (pipeidx = 0; pipeidx < dispNum; pipeidx ++) {
        int count = 0;
        auto plane_range = mPlanesForPipe.equal_range(pipeidx);
        for (auto it = plane_range.first; it != plane_range.second; ++it) {
            if (it->second->getType() == OSD_PLANE ||
                it->second->getType() == OSD_PLANE_PRIMARY) {
                count ++;
                MESON_LOGD("Pipe %d get plane %d", pipeidx, it->second->getType());
            }
        }
        MESON_LOGD("Pipe %d: osd planes(%d)\n", pipeidx, count);
    }

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
        stat->hwcVtVsync = std::make_shared<HwcVsync>();
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

int32_t HwcDisplayPipe::getPlanes(
    uint32_t pipeidx, std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    planes.clear();

    auto plane_rage = mPlanesForPipe.equal_range(pipeidx);
    for (auto it = plane_rage.first; it != plane_rage.second; ++it) {
        planes.push_back(it->second);
    }

    MESON_ASSERT(planes.size() > 0, "get planes for crtc %d failed.", pipeidx);
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
    int type = HwcConfig::getConnectorType(hwcid);

    if (type == HWC_HDMI_CVBS) {
        std::shared_ptr<HwDisplayConnector> hwConnector;
        getConnector(DRM_MODE_CONNECTOR_HDMIA, hwConnector);
        if (hwConnector->isConnected()) {
            connector = DRM_MODE_CONNECTOR_HDMIA;
        } else {
            connector = DRM_MODE_CONNECTOR_TV;
        }
    } else {
        connector = type;
    }

    MESON_LOGD("%s: get display %d, connector %d",
        __func__, hwcid, connector);

    return connector;
}

int32_t HwcDisplayPipe::updatePipe(std::shared_ptr<PipeStat> & stat) {
    MESON_LOGD("HwcDisplayPipe::updatePipe %d.", stat->hwcId);

    PipeCfg cfg;
    getPipeCfg(stat->hwcId, cfg);

    if (memcmp((const void *)&cfg, (const void *)&(stat->cfg), sizeof(PipeCfg)) == 0) {
            MESON_LOGD("Config is not updated.");
            return 0;
    }

    /*update pipestats*/
    bool resChanged = false;
    if (cfg.hwcPipeIdx != stat->cfg.hwcPipeIdx) {
        stat->cfg.hwcPipeIdx = cfg.hwcPipeIdx;
        stat->hwcCrtc = getHwDisplayManager()->getCrtcByPipe(cfg.hwcPipeIdx);
        getPlanes(stat->hwcCrtc->getPipe(), stat->hwcPlanes);
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
    if (cfg.modePipeIdx != stat->cfg.modePipeIdx) {
        stat->cfg.modePipeIdx = cfg.modePipeIdx;
        stat->modeCrtc = getHwDisplayManager()->getCrtcByPipe(cfg.modePipeIdx);
        resChanged = true;
    }
    if (cfg.modeConnectorType != stat->cfg.modeConnectorType) {
        getConnector(cfg.modeConnectorType, stat->modeConnector);
        stat->cfg.modeConnectorType = cfg.modeConnectorType;
        resChanged = true;
    }

    MESON_LOGD("HwcDisplayPipe::updatePipe (%d) [%s], crtc (%d) connector (%d)",
        stat->hwcId, resChanged ? "CHANGED" : "NOT-CHANGED",
        cfg.hwcPipeIdx, cfg.hwcConnectorType);

    if (resChanged) {
        /*reset vout displaymode, it will be null.*/
        MESON_LOGD("HwcDisplayPipe::updatePipe %d changed", stat->hwcId);
        // for fbdev backend need unbind/bind first then update
        if (access("/dev/dri/card0", R_OK | W_OK) != 0) {
            getHwDisplayManager()->unbind(stat->hwcCrtc);
            getHwDisplayManager()->bind(stat->hwcCrtc ,stat->hwcConnector ,stat->hwcPlanes);
        }
        stat->hwcCrtc->update();
        stat->hwcConnector->update();

        // for drm backend crct and connector need update first, then unbind/bind
        if (access("/dev/dri/card0", R_OK | W_OK) == 0) {
            getHwDisplayManager()->unbind(stat->hwcCrtc);
            getHwDisplayManager()->bind(stat->hwcCrtc ,stat->hwcConnector ,stat->hwcPlanes);
        }

        if (cfg.modePipeIdx != cfg.hwcPipeIdx) {
            std::vector<std::shared_ptr<HwDisplayPlane>> planes;
            getPlanes (cfg.modePipeIdx, planes);
            if (access("/dev/dri/card0", R_OK | W_OK) != 0)
                getHwDisplayManager()->bind(stat->modeCrtc, stat->modeConnector, planes);
            stat->modeCrtc->update();
            stat->modeConnector->update();
            if (access("/dev/dri/card0", R_OK | W_OK) == 0)
                getHwDisplayManager()->bind(stat->modeCrtc, stat->modeConnector, planes);
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

        stat->hwcVtVsync->setVtMode(stat->modeCrtc);

        drm_mode_info_t mode;
        if (0 == stat->modeMgr->getDisplayMode(mode)) {
            float refresh_rate = mode.refreshRate;
            if (HwcConfig::getMaxRefreshRate() > 0.0f &&
                    mode.refreshRate > HwcConfig::getMaxRefreshRate()) {
                refresh_rate = HwcConfig::getMaxRefreshRate();
            }
            stat->hwcVsync->setPeriod(1e9 / refresh_rate);
            stat->hwcVtVsync->setPeriod(1e9 / refresh_rate);
        }

        stat->hwcDisplay->setVsync(stat->hwcVsync);
        stat->hwcDisplay->setVtVsync(stat->hwcVtVsync);
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
        case DRM_EVENT_VOUT3_MODE_CHANGED:
            {
                int pipeIdx = DRM_PIPE_VOUT1;
                if (event == DRM_EVENT_VOUT2_MODE_CHANGED)
                    pipeIdx = DRM_PIPE_VOUT2;
                else if (event == DRM_EVENT_VOUT3_MODE_CHANGED)
                    pipeIdx = DRM_PIPE_VOUT3;
                MESON_LOGD("ModeChange state: [%s]", val == 1 ? "Complete" : "Begin to change");
                if (val == 1) {
                    for (auto statIt : mPipeStats) {
                        if (statIt.second->modeCrtc->getPipe() == pipeIdx) {
                            statIt.second->modeConnector->update();
                            statIt.second->modeCrtc->update();
                            statIt.second->modeMgr->update();
                            statIt.second->hwcDisplay->onModeChanged(val);
                            /*update display dynamic info.*/
                            drm_mode_info_t mode;
                            if (0 == statIt.second->modeMgr->getDisplayMode(mode)) {
                                float refresh_rate = mode.refreshRate;
                                if (HwcConfig::getMaxRefreshRate() > 0.0f &&
                                        mode.refreshRate > HwcConfig::getMaxRefreshRate()) {
                                    refresh_rate = HwcConfig::getMaxRefreshRate();
                                }
                                statIt.second->hwcVsync->setPeriod(1e9 / refresh_rate);
                                statIt.second->hwcVtVsync->setPeriod(1e9 / refresh_rate);
                                if (HwcConfig::softwareVsyncEnabled()) {
                                    statIt.second->hwcVsync->setSoftwareMode();
                                } else {
                                    statIt.second->hwcVsync->setHwMode(statIt.second->modeCrtc);
                                }
                                statIt.second->hwcVtVsync->setVtMode(statIt.second->modeCrtc);
                            } else {
                                /* could not get mode, switch to software vsync */
                                statIt.second->hwcVsync->setPeriod(1e9 / DEFAULT_REFRESH_RATE);
                                statIt.second->hwcVsync->setSoftwareMode();

                                statIt.second->hwcVtVsync->setPeriod(1e9 / DEFAULT_REFRESH_RATE);
                                statIt.second->hwcVtVsync->setSoftwareMode();
                            }
                        }
                    }
                } else {
                    for (auto statIt : mPipeStats) {
                        if (statIt.second->modeCrtc->getPipe() == pipeIdx) {
                            statIt.second->hwcDisplay->onModeChanged(val);
                        }
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

    stat->modeCrtc->setPendingMode();
    return 0;
}

void HwcDisplayPipe::dump(String8 & dumpstr) {
    getHwDisplayManager()->dump(dumpstr);
}

std::shared_ptr<HwcDisplayPipe> createDisplayPipe(hwc_pipe_policy_t pipet) {
    switch (pipet) {
        case HWC_PIPE_DEFAULT:
            return std::make_shared<FixedDisplayPipe>();
        case HWC_PIPE_DUAL:
            return std::make_shared<DualDisplayPipe>();
        case HWC_PIPE_LOOPBACK:
            return std::make_shared<LoopbackDisplayPipe>();
        case HWC_PIPE_MULTI:
            return std::make_shared<MultiDisplayDrmPipe>();
        default:
            MESON_ASSERT(0, "unknown display pipe %d", pipet);
    };

    return NULL;
}

