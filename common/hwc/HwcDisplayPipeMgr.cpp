/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "HwcDisplayPipeMgr.h"

#include <HwDisplayManager.h>
#include <HwDisplayPlane.h>
#include <HwDisplayConnector.h>
#include <HwcConfig.h>
#include <MesonLog.h>

ANDROID_SINGLETON_STATIC_INSTANCE(HwcDisplayPipeMgr)

HwcDisplayPipeMgr::PipeStat::PipeStat() {
    cfg.hwcCrtcId = cfg.modeCrtcId = 0;
    cfg.hwcConnectorType = cfg.modeConnectorType = DRM_MODE_CONNECTOR_INVALID;
    cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
}

HwcDisplayPipeMgr::PipeStat::~PipeStat() {
    hwcDisplay.reset();
    hwcCrtc.reset();
    hwcConnector.reset();
    hwcPlanes.clear();

    hwcPostProcessor.reset();
    modeMgr.reset();
    modeCrtc.reset();
    modeConnector.reset();
}

HwcDisplayPipeMgr::HwcDisplayPipeMgr() {
    /*load hw display resource.*/
    mPostProcessor = false;
    mPipePolicy =  HwcConfig::getPipeline();

    HwDisplayManager::getInstance().getCrtcs(mCrtcs);
    HwDisplayManager::getInstance().getPlanes(mPlanes);

    for (uint32_t i = 0; i < HwcConfig::getDisplayNum(); i++) {
        std::shared_ptr<PipeStat> stat = std::make_shared<PipeStat>();
        mPipeStats.emplace(i, stat);

         /*create modeMgr*/
         uint32_t fbW = 0, fbH = 0;
         HwcConfig::getFramebufferSize (i, fbW, fbH);
         std::shared_ptr<HwcModeMgr> modeMgr =
             createModeMgr(HwcConfig::getModePolicy(i));
         modeMgr->setFramebufferSize(fbW, fbH);
         stat->modeMgr = modeMgr;
         /*create vsync for display.*/
         stat->hwcVsync = std::make_shared<HwcVsync>();
    }

    HwDisplayEventListener::getInstance().registerHandler(
        DRM_EVENT_ALL, (HwDisplayEventHandler*)this);
}

HwcDisplayPipeMgr::~HwcDisplayPipeMgr() {
}

int32_t HwcDisplayPipeMgr::setHwcDisplay(
    uint32_t disp, std::shared_ptr<HwcDisplay> & hwcDisp) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto stat = mPipeStats.find(disp)->second;
    stat->hwcDisplay = hwcDisp;
    return 0;
}

int32_t HwcDisplayPipeMgr::getCrtc(
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

int32_t HwcDisplayPipeMgr::getPlanes(
    int32_t crtcid, std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
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

int32_t HwcDisplayPipeMgr::getConnector(
    drm_connector_type_t type, std::shared_ptr<HwDisplayConnector> & connector) {
    auto it = mConnectors.find(type);
    if (it != mConnectors.end()) {
        connector = it->second;
    } else {
        HwDisplayManager::getInstance().getConnector(connector, type);
        mConnectors.emplace(type, connector);
    }

    return 0;
}

int32_t HwcDisplayPipeMgr::getPostProcessor(
    hwc_post_processor_t type, std::shared_ptr<HwcPostProcessor> & processor) {
    if (INVALID_POST_PROCESSOR == type) {
        processor = NULL;
        return 0;
    }

    MESON_ASSERT(0, "NO IMPLEMENT.");
    return 0;
}

drm_connector_type_t HwcDisplayPipeMgr::chooseConnector(
    hwc_connector_t config) {
    switch (config) {
        case HWC_PANEL_ONLY:
            return DRM_MODE_CONNECTOR_PANEL;
        case HWC_CVBS_ONLY:
            return DRM_MODE_CONNECTOR_CVBS;
        case HWC_HDMI_ONLY:
            return DRM_MODE_CONNECTOR_HDMI;
        case HWC_HDMI_CVBS:
            return DRM_MODE_CONNECTOR_HDMI;
        default:
            MESON_ASSERT(0, "Unknow connector config %d", config);
    }

    return DRM_MODE_CONNECTOR_INVALID;
}

int32_t HwcDisplayPipeMgr::getDisplayPipe(
    uint32_t hwcdisp, PipeCfg & cfg) {
    drm_connector_type_t connector =
        chooseConnector(HwcConfig::getConnectorType(hwcdisp));

    switch (mPipePolicy) {
        case HWC_PIPE_DEFAULT:
            if (hwcdisp == 0) {
                cfg.hwcCrtcId = CRTC_VOUT1;
            } else if (hwcdisp == 1) {
                cfg.hwcCrtcId = CRTC_VOUT2;
            }
            cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;
            cfg.modeCrtcId = cfg.hwcCrtcId;
            cfg.modeConnectorType = cfg.hwcConnectorType = connector;
            break;

        case HWC_PIPE_VIU1VDINVIU2:
            MESON_ASSERT(hwcdisp == 0, "Only one display for this policy.");
            cfg.hwcPostprocessorType = INVALID_POST_PROCESSOR;//VDIN_POST_PROCESSOR;
            if (mPostProcessor) {
                cfg.hwcCrtcId = CRTC_VOUT1;
                cfg.hwcConnectorType = DRM_MODE_CONNECTOR_DUMMY;

                cfg.modeCrtcId = CRTC_VOUT2;
                cfg.modeConnectorType = connector;
            } else {
                cfg.modeCrtcId = cfg.hwcCrtcId = CRTC_VOUT1;
                cfg.modeConnectorType = cfg.hwcConnectorType = connector;
            }
            break;

        case HWC_PIPE_DUMMY:
        default:
            MESON_ASSERT(0, "Unsupported pipe %d ", mPipePolicy);
            break;
    }

    return 0;
}

int32_t HwcDisplayPipeMgr::initDisplays() {
    std::lock_guard<std::mutex> lock(mMutex);
    return updatePipe();
}

int32_t HwcDisplayPipeMgr::updatePipe() {
    MESON_LOGD("HwcDisplayPipeMgr::updatePipe.");

    for (uint32_t i = 0; i < HwcConfig::getDisplayNum(); i++) {
        auto statIt = mPipeStats.find(i);
        auto stat = statIt->second;

        PipeCfg cfg;
        getDisplayPipe(i, cfg);

        if (memcmp((const void *)&cfg, (const void *)&(stat->cfg), sizeof(PipeCfg)) == 0)
            continue;

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

        if (resChanged) {
            MESON_LOGD("HwcDisplayPipeMgr::updatePipe %d changed", i);
            stat->hwcCrtc->bind(stat->hwcConnector, stat->hwcPlanes);
            stat->hwcCrtc->loadProperities();
            stat->hwcCrtc->update();

            if (cfg.modeCrtcId != cfg.hwcCrtcId) {
                std::vector<std::shared_ptr<HwDisplayPlane>> planes;
                getPlanes (cfg.modeCrtcId,  planes);
                stat->modeCrtc->bind(stat->modeConnector, planes);
                stat->modeCrtc->loadProperities();
                stat->modeCrtc->update();
            }

            stat->modeMgr->setDisplayResources(stat->modeCrtc, stat->modeConnector);
            stat->modeMgr->update();

            if (HwcConfig::softwareVsyncEnabled()) {
                stat->hwcVsync->setSoftwareMode();
            } else {
                stat->hwcVsync->setHwMode(stat->modeCrtc);
            }

            stat->hwcDisplay->setVsync(stat->hwcVsync);
            stat->hwcDisplay->setModeMgr(stat->modeMgr);
            stat->hwcDisplay->setDisplayResource(
                stat->hwcCrtc, stat->hwcConnector, stat->hwcPlanes);
        }
    }

    return 0;
}

/*display event handle*/
void HwcDisplayPipeMgr::handle(drm_display_event event, int val) {
    std::lock_guard<std::mutex> lock(mMutex);
    /*TODO: need update for dual display.*/
    switch (event) {
        case DRM_EVENT_HDMITX_HDCP:
            {
                MESON_LOGD("Hdcp handle value %d.", val);
                for (auto statIt : mPipeStats) {
                    if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMI) {
                        statIt.second->hwcDisplay->onUpdate((val == 0) ? false : true);
                    }
                }
            }
            break;
        case DRM_EVENT_HDMITX_HOTPLUG:
            {
                MESON_LOGD("Hotplug handle value %d.",val);
                for (auto statIt : mPipeStats) {
                    if (statIt.second->modeConnector->getType() == DRM_MODE_CONNECTOR_HDMI) {
                        statIt.second->hwcDisplay->onHotplug((val == 0) ? false : true);
                    }
                }
            }
            break;
        /*VIU1 mode changed.*/
        case DRM_EVENT_VOUT1_MODE_CHANGED:
        case DRM_EVENT_VOUT2_MODE_CHANGED:
            {
                int crtcid = CRTC_VOUT1;
                if (event == DRM_EVENT_VOUT2_MODE_CHANGED)
                    crtcid = CRTC_VOUT2;
                for (auto statIt : mPipeStats) {
                    if (statIt.second->modeCrtc->getId() == crtcid) {
                        statIt.second->modeCrtc->update();
                        statIt.second->modeMgr->update();
                        statIt.second->hwcDisplay->onModeChanged(val);
                        /*update display dynamic info.*/
                        drm_mode_info_t mode;
                        if (HwcConfig::softwareVsyncEnabled()) {
                            if (0 == statIt.second->modeMgr->getDisplayMode(mode)) {
                                statIt.second->hwcVsync->setPeriod(1e9 / mode.refreshRate);
                            }
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

int32_t HwcDisplayPipeMgr::update(uint32_t flags) {
    MESON_LOGE("HwcDisplayPipeMgr::update %x", flags);

    std::lock_guard<std::mutex> lock(mMutex);
    /*handle postprocessor on display -*/
    if ((flags & rPostProcessorStart) || (flags & rPostProcessorStop)) {
        bool bEnable = flags & rPostProcessorStart ? true : false;

        /*handle for different pipeline config*/
        if (mPipePolicy == HWC_PIPE_VIU1VDINVIU2) {
            std::shared_ptr<PipeStat> stat = mPipeStats.find(0)->second;
            if (mPostProcessor == bEnable)
                return 0;

            mPostProcessor = bEnable;

            if (!bEnable) {
                stat->hwcPostProcessor->stop();
                stat->hwcDisplay->setPostProcessor(NULL);
            }

            drm_mode_info_t curMode;
            /*get current display mode.*/
            stat->modeCrtc->getMode(curMode);

            MESON_LOGE("get cur mode %s",curMode.name);
            /*reset vout displaymode, for we need do pipeline switch*/
            stat->hwcCrtc->unbind();
            stat->modeCrtc->unbind();
            /*update display pipe.*/
            updatePipe();

            if (bEnable) {
                /*set viu1 to dummyplane */
                std::map<uint32_t, drm_mode_info_t> modes;
                stat->hwcConnector->getModes(modes);
                MESON_ASSERT(modes.size() > 0, "no modes got.");

                MESON_LOGE("bEnable: get mode %s",modes[0].name);
                stat->hwcCrtc->setMode(modes[0]);
                /*set viu2 to plane*/
                stat->modeCrtc->setMode(curMode);
                stat->modeMgr->update();
            } else {
                /*set viu1 to plane */
                stat->modeCrtc->setMode(curMode);
                stat->modeMgr->update();
                /*viu2 keep null.*/
            }

            if (bEnable) {
                stat->hwcDisplay->setPostProcessor(stat->hwcPostProcessor);
                stat->hwcPostProcessor->start();
            }
        }

    }

    return 0;
}

