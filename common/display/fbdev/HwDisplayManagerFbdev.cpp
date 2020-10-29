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
#include <HwDisplayCrtc.h>
#include <systemcontrol.h>
#include <DrmTypes.h>
#include <VideoComposerDev.h>

#include "HwConnectorFactory.h"
#include "DummyPlane.h"
#include "OsdPlane.h"
#include "CursorPlane.h"
#include "LegacyVideoPlane.h"
#include "LegacyExtVideoPlane.h"
#include "HwcVideoPlane.h"
#include "AmFramebuffer.h"
#include "HwDisplayCrtcFbdev.h"
#include "HwDisplayManagerFbdev.h"

HwDisplayManagerFbdev::HwDisplayManagerFbdev()
    : HwDisplayManager() {
    loadDrmResources();
}

HwDisplayManagerFbdev::~HwDisplayManagerFbdev() {
    freeDrmResources();
}

int32_t HwDisplayManagerFbdev::getPlanes(
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    mMutex.lock();
    for (const auto & plane : mPlanes)
        planes.push_back(plane.second);
    mMutex.unlock();
    return 0;
}

int32_t HwDisplayManagerFbdev::getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs) {
    mMutex.lock();
    for (const auto & crtc : mCrtcs)
        crtcs.push_back(crtc.second);
    mMutex.unlock();
    return 0;
}

int32_t HwDisplayManagerFbdev::getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type) {
    mMutex.lock();

    auto it = mConnectors.find(type);
    if (it != mConnectors.end()) {
        connector = it->second;
        MESON_LOGD("get existing connector %d-%p", type, connector.get());
    } else {
        int connectorId = CONNECTOR_IDX_MIN + mConnectors.size();
        std::shared_ptr<HwDisplayConnector> newConnector = HwConnectorFactory::create(
                type, -1, connectorId);
        MESON_ASSERT(newConnector != NULL, "unsupported connector type %d", type);
        MESON_LOGD("create new connector %d-%p", type, newConnector.get());
        connector = newConnector;
        mConnectors.emplace(type, newConnector);
    }

    mMutex.unlock();
    return 0;
}

std::shared_ptr<HwDisplayCrtc> HwDisplayManagerFbdev::getCrtcById(
    uint32_t crtcid) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto & c : mCrtcs) {
        if (c.second->getId() == crtcid) {
            return c.second;
        }
    }

    return NULL;;
}

std::shared_ptr<HwDisplayCrtc> HwDisplayManagerFbdev::getCrtcByPipe(
    uint32_t pipeIdx) {
    for (const auto & c : mCrtcs) {
        if (c.second->getPipe() == pipeIdx) {
            return c.second;
        }
    }

    return NULL;
}

int32_t HwDisplayManagerFbdev::bind(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector>  & connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes __unused) {
    HwDisplayCrtcFbdev * fbcrtc = (HwDisplayCrtcFbdev *)crtc.get();
    return fbcrtc->bind(connector);
}

int32_t HwDisplayManagerFbdev::unbind(std::shared_ptr<HwDisplayCrtc> & crtc) {
    HwDisplayCrtcFbdev * fbcrtc = (HwDisplayCrtcFbdev *)crtc.get();
    return fbcrtc->unbind();
}


/********************************************************************
 *   The following functions need update with drm.                  *
 *   Now is hard code for 1 crtc , 1 connector.                     *
 ********************************************************************/
int32_t HwDisplayManagerFbdev::loadDrmResources() {
    mMutex.lock();
    /* must call loadPlanes() first, now it is the only
    *valid function.
    */
    loadPlanes();
    loadConnectors();
    loadCrtcs();

    mMutex.unlock();
    return 0;
}

int32_t HwDisplayManagerFbdev::freeDrmResources() {
    mCrtcs.clear();
    mConnectors.clear();
    mPlanes.clear();
    return 0;
}

int32_t HwDisplayManagerFbdev::loadCrtcs() {
    MESON_LOGV("Crtc loaded in loadPlanes: %d", mCrtcs.size());
    return 0;
}

int32_t HwDisplayManagerFbdev::loadConnectors() {
    MESON_LOGV("No connector api, set empty, will create when hwc get.");
    return 0;
}

int32_t HwDisplayManagerFbdev::loadPlanes() {
    /* scan /dev/graphics/fbx to get planes */
    int fd = -1;
    char path[64];
    int count_osd = 0, count_video = 0;
    int idx = 0, plane_idx = 0, video_idx_max = 0;
    int capability = 0x0;

    /*osd plane.*/
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
                    std::shared_ptr<CursorPlane> plane = std::make_shared<CursorPlane>(fd, plane_idx);
                    mPlanes.emplace(plane_idx, plane);
                } else {
                    std::shared_ptr<OsdPlane> plane = std::make_shared<OsdPlane>(fd, plane_idx);
                    mPlanes.emplace(plane_idx, plane);

                    /*add valid crtc, for fbdev, crtc id = crtc index.*/
                    uint32_t crtcs = plane->getPossibleCrtcs();
                    if ((crtcs & (1 << DRM_PIPE_VOUT1)) && mCrtcs.count(CRTC_VOUT1_ID) == 0) {
                        std::shared_ptr<HwDisplayCrtcFbdev> crtc =
                            std::make_shared<HwDisplayCrtcFbdev>(::dup(fd), CRTC_VOUT1_ID);
                        mCrtcs.emplace(CRTC_VOUT1_ID, crtc);
                    } else if ((crtcs & (1 << DRM_PIPE_VOUT2)) && mCrtcs.count(CRTC_VOUT2_ID) == 0) {
                        std::shared_ptr<HwDisplayCrtcFbdev> crtc =
                            std::make_shared<HwDisplayCrtcFbdev>(::dup(fd), CRTC_VOUT2_ID);
                        mCrtcs.emplace(CRTC_VOUT2_ID, crtc);
                    }
                }
                count_osd ++;
            }
        }
        idx ++;
    } while(fd >= 0);

    /*legacy video plane.*/
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
            std::shared_ptr<LegacyVideoPlane> plane =
                std::make_shared<LegacyVideoPlane>(fd, plane_idx);
            mPlanes.emplace(plane_idx, plane);
            count_video ++;
            plane_idx = plane_idx + count_video;
            std::shared_ptr<LegacyExtVideoPlane> extPlane =
                std::make_shared<LegacyExtVideoPlane>(fd, plane_idx);
            mPlanes.emplace(plane_idx, extPlane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    /*hwc video plane.*/
    video_idx_max = video_idx_max + count_video;
    idx = 0;
    do {
        snprintf(path, 64, "/dev/video_composer.%u", idx);
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            createVideoComposerDev(::dup(fd), idx);

            plane_idx = video_idx_max + idx;
            std::shared_ptr<HwcVideoPlane> plane =
                std::make_shared<HwcVideoPlane>(fd, plane_idx);
            plane->setAmVideoPath(idx);
            mPlanes.emplace(plane_idx, plane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get osd planes (%d), video planes (%d)", count_osd, count_video);

    return 0;
}


