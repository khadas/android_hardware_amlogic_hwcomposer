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
#include "DrmEncoder.h"
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
    loadResources();
    initPipe();
}

DrmDevice::~DrmDevice() {
    freeResources();
}

int32_t DrmDevice::getPlanes(
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto & it : mPlanes) {
        planes.push_back(it.second);
    }
    return 0;
}

int32_t DrmDevice::getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto & it : mCrtcs) {
        crtcs.push_back(it.second);
    }
    return 0;
}

int32_t DrmDevice::getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConnectors.find(type);
    if (it != mConnectors.end()) {
        connector = it->second;
        MESON_LOGD("get existing connector %d-%p", type, connector.get());
    } else {
        MESON_ASSERT(0, "unsupported connector type %d", type);
    }

    return 0;
}

std::shared_ptr<HwDisplayCrtc> DrmDevice::getCrtcById(uint32_t crtcid) {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mCrtcs.find(crtcid);
    if (it != mCrtcs.end()) {
        return it->second;
    } else {
        MESON_LOGE("getCrtc by id(%d) failed.", crtcid);
        return NULL;
    }
}

std::shared_ptr<HwDisplayCrtc> DrmDevice::getCrtcByPipe(uint32_t pipeIdx) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto & it : mCrtcs) {
        if (it.second->getPipe() == pipeIdx)
            return it.second;
    }

    MESON_LOGE("DrmDevice getCrtcByPipe failed with pipeindx(%d)", pipeIdx);
    return NULL;
}

int32_t DrmDevice::getPipeCfg(uint32_t pipeIdx, HwDisplayPipe & pipecfg) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPipes.find(pipeIdx);
    if (it != mPipes.end()) {
        pipecfg = it->second;
        return 0;
    } else {
        MESON_LOGE("getPipeCfg: pipe (%d) is not binded", pipeIdx);
        return -EPIPE;
    }
}

int32_t DrmDevice::bind(
    std::shared_ptr<HwDisplayCrtc> & crtc,
    std::shared_ptr<HwDisplayConnector>  & connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint32_t pipeidx = crtc->getPipe();

    /*check if same pipe cfg, if yes return, if no unbind.*/
    HwDisplayPipe pipecfg;
    auto it = mPipes.find(pipeidx);
    if (it != mPipes.end()) {
        pipecfg = it->second;
        MESON_LOGD("drm current pipe(%d)->crtc(%d)->connector(%d)",
            pipeidx, pipecfg.crtc_id, pipecfg.connector_id);
        if (pipecfg.crtc_id == crtc->getId()
            && pipecfg.connector_id == connector->getId()) {
            MESON_LOGD("Request same pipe as current, nothing to do.");
            return 0;
        } else {
            
        }
    }

    /*bind to new pipe cfg*/
    pipecfg.crtc_id = crtc->getId();
    pipecfg.connector_id = connector->getId();

    connector->setCrtcId(pipecfg.crtc_id);
    /*encoder only used when bootup.*/

    /*TODO: apply bind.*/


    mPipes.emplace(pipeidx, pipecfg);
    return 0;
}

int32_t DrmDevice::unbind(std::shared_ptr<HwDisplayCrtc> & crtc) {
    uint32_t pipeidx = crtc->getPipe();
    MESON_LOGD("unlock pipe (%d)", pipeidx);

    auto it = mPipes.find(pipeidx);
    if (it != mPipes.end()) {
        mPipes.erase(it);
    }

    return 0;
}

void DrmDevice::initPipe() {
    std::lock_guard<std::mutex> lock(mMutex);

    for (auto & et : mEncoders) {
        int crtcid = et.second->getCrtcId();
        if (crtcid == 0) /*not bind*/
            continue;

        HwDisplayPipe pipeCfg;
        uint32_t encoder_id = et.second->getId();

        /*get crtc index*/
        pipeCfg.crtc_id = crtcid;
        auto crtcit = mCrtcs.find(crtcid);
        MESON_ASSERT(crtcit != mCrtcs.end(), "initPipe get crtc (%d) failed", crtcid);
        uint32_t pipe = crtcit->second->getPipe();

        /*get connector on this encoder*/
        for (auto & ct : mConnectors) {
            DrmConnector * connector = (DrmConnector *)(ct.second.get());
            if (connector->getEncoderId() == encoder_id) {
                pipeCfg.connector_id = connector->getId();
                connector->setCrtcId(pipeCfg.crtc_id);
                break;
            }
        }

        mPipes.emplace(pipe, pipeCfg);
        MESON_LOGD("initPipe (%d) crtc(%d)-encoder(%d)-connector(%d))",
            pipe, pipeCfg.crtc_id, encoder_id, pipeCfg.connector_id);
    }
}

void DrmDevice::loadResources() {
    std::lock_guard<std::mutex> lock(mMutex);
    int ret = loadDrmResources();
    MESON_ASSERT(ret != 0, "loadDrmResources failed (%d)", ret);
    ret = loadNonDrmResources();
    MESON_ASSERT(ret != 0, "loadNonDrmPlanes  failed (%d)", ret);
}

int32_t DrmDevice::loadDrmResources() {
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
        std::shared_ptr<HwDisplayCrtc> crtc = std::make_shared<DrmCrtc>(metadata, i);
        mCrtcs.emplace(crtc->getId(), std::move(crtc));
        drmModeFreeCrtc(metadata);
    }
    /*Encoder*/
    for (int i = 0; i < drmRes->count_encoders; i++) {
        drmModeEncoderPtr metadata = drmModeGetEncoder(
            mDrmFd, drmRes->encoders[i]);
        std::shared_ptr<DrmEncoder> encoder = std::make_shared<DrmEncoder>(metadata);
        mEncoders.emplace(encoder->getId(), std::move(encoder));
        drmModeFreeEncoder(metadata);
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
    /*load Drm planes*/
    drmModePlaneResPtr planeRes = drmModeGetPlaneResources(mDrmFd);
    for (int i = 0; i < planeRes->count_planes; i ++) {
        drmModePlanePtr metadata = drmModeGetPlane(
            mDrmFd, planeRes->planes[i]);
        std::shared_ptr<HwDisplayPlane> plane = std::make_shared<DrmPlane>(metadata);
        mPlanes.emplace(plane->getId(), std::move(plane));
        drmModeFreePlane(metadata);
    }
    drmModeFreePlaneResources(planeRes);

    return 0;
}

int32_t DrmDevice::loadNonDrmResources() {
    /*legacy video plane.*/
    MESON_LOGD( "Legacy video plane not supported any more.");

    /*hwc video plane.*/
    int fd = -1, idx = 0, count_video = 0, obj_idx;
    int video_idx_max = 0xffff0000;
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
            mPlanes.emplace(obj_idx, plane);
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get non drm video planes (%d)", count_video);
    return 0;
}

int32_t DrmDevice::freeResources() {
    mCrtcs.clear();
    mEncoders.clear();
    mConnectors.clear();
    mPlanes.clear();

    if (mDrmFd >= 0)
        close(mDrmFd);
    return 0;
}


