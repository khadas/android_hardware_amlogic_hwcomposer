/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <MesonLog.h>
#include <HwDisplayManager.h>
#include <HwDisplayConnector.h>
#include <systemcontrol.h>
#include <DrmTypes.h>
#include <HwcConfig.h>

#include "HwConnectorFactory.h"
#include "DummyPlane.h"
#include "OsdPlane.h"
#include "CursorPlane.h"
#include "LegacyVideoPlane.h"
#include "LegacyExtVideoPlane.h"
#include "HwcVideoPlane.h"
#include "AmFramebuffer.h"

ANDROID_SINGLETON_STATIC_INSTANCE(HwDisplayManager)

HwDisplayManager::HwDisplayManager() {
    loadDrmResources();

    bool bSoftwareVsync = HwcConfig::softwareVsyncEnabled();
    mVsync = std::make_shared<HwDisplayVsync>(bSoftwareVsync, this);

    HwDisplayEventListener::getInstance().registerHandler(DRM_EVENT_ALL, this);
}

HwDisplayManager::~HwDisplayManager() {
    freeDrmResources();
}

int32_t HwDisplayManager::getHwDisplayIds(uint32_t * displayNum,
    hw_display_id * hwDisplayIds) {
    *displayNum = count_pipes;

    if (hwDisplayIds) {
        uint32_t i;
        for (i = 0 ;i < count_pipes; ++i) {
            hwDisplayIds[i] = pipes[i].crtc_id;
        }
    }

    return 0;
}

int32_t HwDisplayManager::getPlanes(uint32_t hwDisplayId,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    int ret = -ENXIO;
    uint32_t i = 0;
    mMutex.lock();
    planes.clear();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            uint32_t iplane = 0;
            for (iplane = 0; iplane < pipes[i].planes_num; iplane ++) {
                planes.push_back(mPlanes.find(pipes[i].plane_ids[iplane])->second);
            }

            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::getCrtc(hw_display_id hwDisplayId,
        std::shared_ptr<HwDisplayCrtc> & crtc) {
    int ret = -ENXIO;
    uint32_t i = 0;
    mMutex.lock();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            crtc = mCrtcs.find(pipes[i].crtc_id)->second;
            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::getConnector(hw_display_id hwDisplayId,
        std::shared_ptr<HwDisplayConnector> & connector) {
    int ret = -ENXIO;
    uint32_t i = 0;
    mMutex.lock();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            connector = mConnectors.find(pipes[i].connector_id)->second;
            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::enableVBlank(bool enabled) {
    mVsync->setEnabled(enabled);
    return 0;
}

int32_t HwDisplayManager::updateRefreshPeriod(int32_t period) {
    mVsync->setPeriod(period);
    return 0;
}

int32_t HwDisplayManager::registerObserver(hw_display_id hwDisplayId,
    HwDisplayObserver * observer) {
    mObserver.emplace(hwDisplayId, observer);
    return 0;
}

int32_t HwDisplayManager::unregisterObserver(hw_display_id hwDisplayId) {
    std::map<hw_display_id, HwDisplayObserver * >::iterator it;
    it = mObserver.find(hwDisplayId);
    if (it != mObserver.end())
        mObserver.erase(it);

    return 0;
}

void HwDisplayManager::handle(drm_display_event event, int val) {
    /*TODO: need update for dual display.*/
    std::map<hw_display_id, HwDisplayObserver *>::iterator it;
    switch (event) {
        case DRM_EVENT_HDMITX_HOTPLUG:
            {
                MESON_LOGD("Hotplug observer size %d to handle value %d.",
                    mObserver.size(), val);
                for (it = mObserver.begin(); it != mObserver.end(); ++it)
                    for (uint32_t i = 0; i < count_pipes; i++)
                        if (pipes[i].crtc_id == it->first &&
                            mConnectors[pipes[i].connector_id]->getType() ==
                                DRM_MODE_CONNECTOR_HDMI) {
                                it->second->onHotplug((val == 0) ? false : true);
                                break;
                        }
            }
            break;
        case DRM_EVENT_HDMITX_HDCP:
            {
                MESON_LOGD("Hdcp observer size %d to handle value %d.",
                    mObserver.size(), val);
                for (it = mObserver.begin(); it != mObserver.end(); ++it) {
                    for (uint32_t i = 0; i < count_pipes; i++) {
                        if (pipes[i].crtc_id == it->first &&
                            mConnectors[pipes[i].connector_id]->getType() ==
                                DRM_MODE_CONNECTOR_HDMI) {
                                it->second->onUpdate((val == 0) ? false : true);
                                break;
                        }
                    }
                }
            }
            break;
        case DRM_EVENT_MODE_CHANGED:
            {
                if (count_crtcs > 1) {
                    MESON_ASSERT(0, " %s Dual display not supported.", __func__);
                }

                MESON_LOGD("Mode change observer size %d.", mObserver.size());
                for (it = mObserver.begin(); it != mObserver.end(); ++it) {
                    it->second->onModeChanged(val);
                    break;
                }
            }
            break;
        default:
            MESON_LOGE("Receive unhandled event %d", event);
            break;
    }
}

int32_t HwDisplayManager::waitVBlank(nsecs_t & timestamp) {
    auto it = mPlanes.begin();
    int32_t drvFd = it->second->getDrvFd();

    if (ioctl(drvFd, FBIO_WAITFORVSYNC_64, &timestamp) == -1) {
        MESON_LOGE("fb ioctl vsync wait error, fb handle: %d", drvFd);
        return -1;
    } else {
        if (timestamp != 0) {
            return 0;
        } else {
            MESON_LOGE("wait for vsync fail");
            return -1;
        }
    }
}

void HwDisplayManager::onVsync(int64_t timestamp) {
    std::map<hw_display_id, HwDisplayObserver *>::iterator it;
    for (it = mObserver.begin(); it != mObserver.end(); ++it) {
        it->second->onVsync(timestamp);
    }
}

void HwDisplayManager::dump(String8 & dumpstr) {
    uint32_t i = 0;
    dumpstr.append("---------------------------------------------------------"
       "-----------------------------\n");

    for (i = 0; i < count_pipes; i ++) {
        dumpstr.appendFormat("Crtc %d :\n", i);
        int planeNum =  pipes[i].planes_num;
        int j;
        for (j = 0; j < planeNum; j++) {
                int planeId =  pipes[i].plane_ids[j];
                dumpstr.appendFormat("Plane (%s, %s, 0x%x, %d)\n",
                    mPlanes.find(planeId)->second->getName(),
                    drmPlaneTypeToString(
                        (drm_plane_type_t)mPlanes.find(planeId)->second->getPlaneType()),
                    mPlanes.find(planeId)->second->getCapabilities(),
                    mPlanes.find(planeId)->second->getFixedZorder());
        }

        int connectorId = pipes[i].connector_id;
        dumpstr.appendFormat("Connector (%s, %d)\n",
            mConnectors.find(connectorId)->second->getName(),
            mConnectors.find(connectorId)->second->isSecure());

        dumpstr.append("\n");
    }
}

/********************************************************************
 *   The following functions need update with drm.                  *
 *   Now is hard code for 1 crtc , 1 connector.                     *
 ********************************************************************/
int32_t HwDisplayManager::loadDrmResources() {
    count_crtcs = HwcConfig::getDisplayNum();
    count_connectors = count_crtcs;

    crtc_ids = new uint32_t [count_crtcs];
    crtc_ids[0] = CRTC_IDX_MIN + 0;

    count_connectors = 1;
    connector_ids = new uint32_t [count_connectors];
    connector_ids[0] = CONNECTOR_IDX_MIN + 0;

    uint32_t i = 0;
    for (; i < count_crtcs; i++) {
        loadCrtc(crtc_ids[i]);
    }

    for (i = 0; i < count_connectors; i++) {
        loadConnector(connector_ids[i]);
    }

    loadPlanes();

    buildDisplayPipes();
    return 0;
}

int32_t HwDisplayManager::freeDrmResources() {
    if (crtc_ids) {
        delete crtc_ids;
        crtc_ids = NULL;
    }

    if (connector_ids) {
        delete connector_ids;
        connector_ids = NULL;
    }

    count_crtcs = count_connectors = 0;

    mCrtcs.clear();
    mConnectors.clear();
    mPlanes.clear();
    return 0;
}

int32_t HwDisplayManager::loadCrtc(uint32_t crtcid) {
    /* use fb0 to do display crtc */
    int fd = open("/dev/graphics/fb0", O_RDWR, 0);
    std::shared_ptr<HwDisplayCrtc> crtc = std::make_shared<HwDisplayCrtc>(fd, crtcid);
    mCrtcs.emplace(crtcid, crtc);
    return 0;
}

int32_t HwDisplayManager::loadConnector(uint32_t connector_id) {
    drm_connector_type_t connector_type = HwcConfig::getConnectorType(0);
    std::shared_ptr<HwDisplayConnector> connector = HwConnectorFactory::create(
            connector_type, -1, connector_id);

    mConnectors.emplace(connector_id, connector);
    return 0;
}

int32_t HwDisplayManager::loadPlanes() {
    /* scan /dev/graphics/fbx to get planes */
    int fd = -1;
    char path[64];
    int count_osd = 0, count_video = 0;
    int idx = 0, plane_idx = 0, video_idx_max = 0;
    int capability = 0x0;

    do {
        snprintf(path, 64, "/dev/graphics/fb%u", idx);
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            plane_idx = OSD_PLANE_IDX_MIN + idx;
            if (ioctl(fd, FBIOGET_OSD_CAPBILITY, &capability) != 0) {
                MESON_LOGE("osd plane get capibility ioctl (%d) return(%d)", capability, errno);
                return -EINVAL;
            }
            if (capability & OSD_LAYER_ENABLE) {
                if (capability & OSD_HW_CURSOR) {
                    if (!HwcConfig::cursorPlaneDisabled()) {
                        std::shared_ptr<CursorPlane> plane = std::make_shared<CursorPlane>(fd, plane_idx);
                        mPlanes.emplace(plane_idx, plane);
                    }
                } else {
                    std::shared_ptr<OsdPlane> plane = std::make_shared<OsdPlane>(fd, plane_idx);
                    mPlanes.emplace(plane_idx, plane);
                }
                count_osd ++;
            }
        }
        idx ++;
    } while(fd >= 0);

    video_idx_max = VIDEO_PLANE_IDX_MIN;
    idx = 0;
    do {
        if (idx == 0) {
            snprintf(path, 64, "/dev/amvideo");
        } else {
            snprintf(path, 64, "/dev/amvideo%u", idx);
        }
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            plane_idx = video_idx_max + count_video;
            std::shared_ptr<LegacyVideoPlane> plane = std::make_shared<LegacyVideoPlane>(fd, plane_idx);
            mPlanes.emplace(plane_idx, plane);
            count_video ++;
            plane_idx = plane_idx + count_video;
            std::shared_ptr<LegacyExtVideoPlane> extPlane = std::make_shared<LegacyExtVideoPlane>(fd, plane_idx);
            mPlanes.emplace(plane_idx, extPlane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    video_idx_max = video_idx_max + count_video;
    idx = 0;
    do {
        snprintf(path, 64, "/dev/video_hwc%u", idx);
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            plane_idx = video_idx_max + idx;
            std::shared_ptr<HwcVideoPlane> plane = std::make_shared<HwcVideoPlane>(fd, plane_idx);
            mPlanes.emplace(plane_idx, plane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get osd planes (%d), video planes (%d)", count_osd, count_video);

    return 0;
}

int32_t HwDisplayManager::buildDisplayPipes() {
    /*TODO: need update for dual display.*/

    count_pipes = 1;

    pipes[0].crtc_id = crtc_ids[0];
    pipes[0].connector_id = connector_ids[0];
    pipes[0].plane_ids = new uint32_t [mPlanes.size()];

    int i = 0;
    auto it = mPlanes.begin();
    for (; it!=mPlanes.end(); ++it) {
        if (it->second->getPossibleCrtcs() & 1) {
            pipes[0].plane_ids[i] = it->first;
            i++;
        }
    }

    pipes[0].planes_num = i;

    std::shared_ptr<HwDisplayCrtc> crtc;
    std::shared_ptr<HwDisplayConnector> connector;

    crtc = mCrtcs.find(pipes[0].crtc_id)->second;
    connector = mConnectors.find(pipes[0].connector_id)->second;
    crtc->setUp(connector, mPlanes);
    return 0;
}

