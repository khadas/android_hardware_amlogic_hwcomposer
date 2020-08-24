/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <drm.h>

#include <MesonLog.h>
#include <HwcVideoPlane.h>

#include "DrmDevice.h"
#include "DrmCrtc.h"
#include "DrmConnector.h"
#include "DrmPlane.h"

#define MESON_DRM_DRIVER_NAME "meson"

static std::shared_ptr<DrmDevice> gDrmInstance;

std::shared_ptr<HwDisplayManager> getHwDisplayManager() {
    if (!gDrmInstance) {
        gDrmInstance = std::make_shared<DrmDevice>();
    }

    return gDrmInstance;
}

int32_t destroyHwDisplayManager() {
    gDrmInstance.reset();
    return 0;
}

DrmDevice::DrmDevice()
    : HwDisplayManager() {
    loadDrmResources();
}

DrmDevice::~DrmDevice() {
    freeDrmResources();
}

int32_t DrmDevice::getPlanes(
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    mMutex.lock();
    planes = mPlanes;
    mMutex.unlock();
    return 0;
}

int32_t DrmDevice::getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs) {
    mMutex.lock();
    crtcs = mCrtcs;
    mMutex.unlock();
    return 0;
}

int32_t DrmDevice::getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type) {
    mMutex.lock();

    auto it = mConnectors.find(type);
    if (it != mConnectors.end()) {
        connector = it->second;
        MESON_LOGD("get existing connector %d-%p", type, connector.get());
    } else {
        MESON_ASSERT(0, "unsupported connector type %d", type);
    }

    mMutex.unlock();
    return 0;
}

int32_t DrmDevice::loadDrmResources() {
    mMutex.lock();

    mDrmFd = drmOpen(MESON_DRM_DRIVER_NAME, NULL);
    MESON_ASSERT(mDrmFd >= 0, "Open drm device %s failed.", MESON_DRM_DRIVER_NAME);

    int ret = drmSetClientCap(mDrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    MESON_ASSERT(ret != 0, "DRM_CLIENT_CAP_UNIVERSAL_PLANES failed(%d).", ret);
    ret = drmSetClientCap(mDrmFd, DRM_CLIENT_CAP_ATOMIC, 1);
    MESON_ASSERT(ret != 0, "DRM_CLIENT_CAP_ATOMIC failed (%d).", ret);

    /*load crtc & connector & encoder.*/
    drmModeResPtr drmRes = drmModeGetResources(mDrmFd);
    MESON_ASSERT(drmRes != 0, "drmModeGetResources failed().");
    /*Crtc*/
    for (int i = 0; i < drmRes->count_crtcs; i++) {
        drmModeCrtcPtr metadata = drmModeGetCrtc(
            mDrmFd, drmRes->crtcs[i]);
        std::shared_ptr<HwDisplayCrtc> crtc = std::make_shared<DrmCrtc>(metadata);
        mCrtcs.push_back(std::move(crtc));
        drmModeFreeCrtc(metadata);
    }
    /*Connectors*/
    for (int i = 0; i < drmRes->count_connectors; i ++) {
        drmModeConnectorPtr metadata = drmModeGetConnector(
            mDrmFd,drmRes->connectors[i]);
        std::shared_ptr<HwDisplayConnector> connector =
            std::make_shared<DrmConnector>(metadata);
        mConnectors.emplace(connector->getType(), std::move(connector));
        drmModeFreeConnector(metadata);
    }
    drmModeFreeResources(drmRes);

    /*load Drm planes & private planes*/
    drmModePlaneResPtr planeRes = drmModeGetPlaneResources(mDrmFd);
    for (int i = 0; i < planeRes->count_planes; i ++) {
        drmModePlanePtr metadata = drmModeGetPlane(
            mDrmFd, planeRes->planes[i]);
        std::shared_ptr<HwDisplayPlane> plane = std::make_shared<DrmPlane>(metadata);
        mPlanes.push_back(std::move(plane));
        drmModeFreePlane(metadata);
    }
    drmModeFreePlaneResources(planeRes);
    /*amlogic video planes.*/
    loadNonDrmPlanes();

    mMutex.unlock();
    return 0;
}

int32_t DrmDevice::loadNonDrmPlanes() {
    /*legacy video plane.*/
    MESON_LOGD( "Legacy video plane not supported any more.");

    /*hwc video plane.*/
    int fd = -1, idx = 0, count_video = 0, obj_idx = 0xff00;
    int video_idx_max = 0xff;
    char path[64];

    do {
        snprintf(path, 64, "/dev/video_composer.%u", idx);
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            createVideoComposerDev(::dup(fd), idx);

            obj_idx = video_idx_max + idx;
            std::shared_ptr<HwcVideoPlane> plane =
                std::make_shared<HwcVideoPlane>(fd, obj_idx);
            plane->setAmVideoPath(idx);
            mPlanes.push_back(plane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get non drm video planes (%d)", count_video);

    return 0;
}

int32_t DrmDevice::freeDrmResources() {
    mCrtcs.clear();
    mConnectors.clear();
    mPlanes.clear();

    if (mDrmFd >= 0)
        close(mDrmFd);
    return 0;
}


