/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <sys/ioctl.h>
#include <MesonLog.h>
#include <HwDisplayManager.h>
#include <HwDisplayConnector.h>
#include <systemcontrol.h>
#include <DrmTypes.h>

#include "HwConnectorFactory.h"
#include "DummyPlane.h"
#include "OsdPlane.h"
#include "CursorPlane.h"
#include "LegacyVideoPlane.h"
#include "AmFramebuffer.h"

ANDROID_SINGLETON_STATIC_INSTANCE(HwDisplayManager)

HwDisplayManager::HwDisplayManager() {
    loadDrmResources();

#if defined(WIDTH_PRIMARY_FRAMEBUFFER) && \
        defined(HEIGHT_PRIMARY_FRAMEBUFFER)
        mDefFbWidth  = WIDTH_PRIMARY_FRAMEBUFFER;
        mDefFbHeight = HEIGHT_PRIMARY_FRAMEBUFFER;
#else
        MESON_LOGE("HwDisplayManager need define the"
            "WIDTH_PRIMARY_FRAMEBUFFER and WIDTH_PRIMARY_FRAMEBUFFER.");
#endif

#if MESON_HW_DISPLAY_VSYNC_SOFTWARE
    mVsync = std::make_shared<HwDisplayVsync>(true, this);
#else
    mVsync = std::make_shared<HwDisplayVsync>(false, this);
#endif

    HwDisplayEventListener::getInstance().registerHandler(DRM_EVENT_ANY, this);
}

HwDisplayManager::~HwDisplayManager() {
    freeDrmResources();

    if (mCursorPlane) {
        delete mCursorPlane;
        mCursorPlane = NULL;
    }
}

int32_t HwDisplayManager::getHwDisplayIds(uint32_t * displayNum,
    hw_display_id * hwDisplayIds) {
    *displayNum = count_pipes;

    if (hwDisplayIds) {
        int i;
        for (i = 0 ;i < count_pipes; ++i) {
            hwDisplayIds[i] = pipes[i].crtc_id;
        }
    }

    return 0;
}

int32_t HwDisplayManager::getPlanes(uint32_t hwDisplayId,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    int ret = -ENXIO, i = 0;
    mMutex.lock();
    planes.clear();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            int iplane = 0;
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
    int ret = -ENXIO, i = 0;
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
    int ret = -ENXIO, i = 0;
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
    std::map<hw_display_id, HwDisplayObserver *>::iterator it;
    switch (event) {
        case DRM_EVENT_PRIMARY_BOOT:
            {
                MESON_LOGD("Primary boot observer size %d.", mObserver.size());
                for (it = mObserver.begin(); it != mObserver.end(); ++it)
                    for (int i = 0; i < count_pipes; i++)
                        if (pipes[i].crtc_id == it->first) {
                            it->second->onHotplug((val == 0) ? false : true);
                            break;
                        }
            }
            break;
        case DRM_EVENT_HDMITX_HOTPLUG:
        case DRM_EVENT_HDMITX_HDCP:
            {
                MESON_LOGD("Hotplug observer size %d.", mObserver.size());
                for (it = mObserver.begin(); it != mObserver.end(); ++it)
                    for (int i = 0; i < count_pipes; i++)
                        if (pipes[i].crtc_id == it->first &&
                            mConnectors[pipes[i].connector_id]->getType() ==
                            DRM_MODE_CONNECTOR_HDMI) {
                            it->second->onHotplug((val == 0) ? false : true);
                            break;
                        }
            }
            break;
        case DRM_EVENT_MODE_CHANGED:
            {
                #ifndef HWC_MANAGE_DISPLAY_MODE
                    /*TODO: update which crtc? */
                    std::string dispmode;
                    if (0 == sc_get_display_mode(dispmode)) {
                        if (count_crtcs > 1) {
                            MESON_LOG_EMPTY_FUN();
                            MESON_LOGE("Dual display not supported.");
                        } else if (count_crtcs == 1 &&
                                mConnectors[pipes[0].connector_id]->isConnected()) {
                            mCrtcs[crtc_ids[0]]->updateMode(dispmode);

                            if (mCursorPlane) {
                                //set osd mouse scale axis
                                int position[4] = { 0, 0, 0, 0 };//x,y,w,h
                                if (0 == sc_get_osd_position(dispmode, position)) {
                                    char axis[MAX_STR_LEN] = {0};
                                    sprintf(axis, "%d %d %d %d",
                                            mDefFbWidth, mDefFbHeight, position[2], position[3]);
                                    mCursorPlane->updateOsdPosition(axis);
                                } else {
                                    MESON_LOGE("GetOsdPosition by sc failed.");
                                }
                            }
                        }
                    } else {
                        MESON_LOGE("GetDisplayMode by sc failed.");
                    }
                #endif

                MESON_LOGD("Mode change observer size %d.", mObserver.size());
                for (it = mObserver.begin(); it != mObserver.end(); ++it) {
                    it->second->onModeChanged(val);
                    break;
                }
            }
            break;
        default:
            MESON_LOGE("Receive unknow event %d", event);
            break;
    }
}

int32_t HwDisplayManager::waitVBlank(nsecs_t & timestamp) {
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>>::iterator it = mPlanes.begin();
    int32_t drvFd = it->second->getDrvFd();

    if (ioctl(drvFd, FBIO_WAITFORVSYNC, &timestamp) == -1) {
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
    int i = 0;
   dumpstr.append("---------------------------------------------------------"
       "-----------------------------\n");

    for (i = 0; i < count_pipes; i ++) {
        dumpstr.appendFormat("Crtc %d :\n", i);
        int planeNum =  pipes[i].planes_num;
        int j;
        for (j = 0; j < planeNum; j++) {
                int planeId =  pipes[i].plane_ids[j];
                dumpstr.appendFormat("Plane (%s, %s, %x)\n",
                    mPlanes.find(planeId)->second->getName(),
                    drmPlaneTypeToString(
                        (drm_plane_type_t)mPlanes.find(planeId)->second->getPlaneType()),
                    mPlanes.find(planeId)->second->getCapabilities());
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
    count_crtcs = HWC_CRTC_NUM;
    count_connectors = count_crtcs;

    crtc_ids = new uint32_t [count_crtcs];
    crtc_ids[0] = CRTC_IDX_MIN + 0;

    count_connectors = 1;
    connector_ids = new uint32_t [count_connectors];
    connector_ids[0] = CONNECTOR_IDX_MIN + 0;

    int i = 0;
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
    HwDisplayCrtc * crtc = new HwDisplayCrtc(fd, crtcid);
    mCrtcs.emplace(crtcid, std::move(crtc));
    return 0;
}

int32_t HwDisplayManager::loadConnector(uint32_t connector_id) {
    drm_connector_type_t connector_type = DRM_MODE_CONNECTOR_HDMI;
    if (strcasecmp(HWC_PRIMARY_CONNECTOR_TYPE, "hdmi") == 0) {
        connector_type = DRM_MODE_CONNECTOR_HDMI;
    } else if (strcasecmp(HWC_PRIMARY_CONNECTOR_TYPE, "cvbs") == 0) {
        connector_type = DRM_MODE_CONNECTOR_CVBS;
    } else if (strcasecmp(HWC_PRIMARY_CONNECTOR_TYPE, "panel") == 0) {
        connector_type = DRM_MODE_CONNECTOR_PANEL;
    }

    HwDisplayConnector* connector = HwConnectorFactory::create(
            connector_type, -1, connector_id);

    mConnectors.emplace(connector_id, std::move(connector));
    return 0;
}

int32_t HwDisplayManager::loadPlanes() {
    /* scan /dev/graphics/fbx to get planes */
#ifdef HWC_HEADLESS
    int plane_idx = OSD_PLANE_IDX_MIN;
    DummyPlane * plane = new DummyPlane(-1, plane_idx);
    mPlanes.emplace(plane_idx, std::move(plane));
#else
    int fd = -1;
    char path[64];
    int count_osd = 0, count_video = 0;
    int idx = 0, plane_idx = 0, video_idx_max = 0;
    int capability = 0x0, planeType = 0;

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
                HwDisplayPlane * plane;
                if (capability & OSD_HW_CURSOR) {
                    plane = new CursorPlane(fd, plane_idx);
                    mCursorPlane = plane;
                } else {
                    plane = new OsdPlane(fd, plane_idx);
                }
                mPlanes.emplace(plane_idx, std::move(plane));
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
            plane_idx = video_idx_max + idx;
            LegacyVideoPlane * plane = new LegacyVideoPlane(fd, plane_idx);
            mPlanes.emplace(plane_idx, std::move(plane));
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
            LegacyVideoPlane * plane = new LegacyVideoPlane(fd, plane_idx);
            mPlanes.emplace(plane_idx, std::move(plane));
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get osd planes (%d), video planes (%d)", count_osd, count_video);
#endif

    return 0;
}

int32_t HwDisplayManager::buildDisplayPipes() {
    /*TODO: need update for dual display.*/

    count_pipes = 1;

    pipes[0].crtc_id = crtc_ids[0];
    pipes[0].connector_id = connector_ids[0];
    pipes[0].planes_num = mPlanes.size();
    pipes[0].plane_ids = new uint32_t [pipes[0].planes_num];

    int i = 0;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>>::iterator it = mPlanes.begin();
    for (; it!=mPlanes.end(); ++it) {
        pipes[0].plane_ids[i] = it->first;
        i++;
    }

    return 0;
}

