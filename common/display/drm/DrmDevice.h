/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRM_DEVICE_H
#define DRM_DEVICE_H

#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <BasicTypes.h>
#include <DrmTypes.h>
#include <HwDisplayManager.h>
#include "DrmEncoder.h"

class DrmDevice : public HwDisplayManager {
public:
    DrmDevice();
    ~DrmDevice();

    int32_t getPlanes(
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes);
    int32_t getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs);
    int32_t getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type);

    std::shared_ptr<HwDisplayCrtc> getCrtcById(uint32_t crtcid);
    std::shared_ptr<HwDisplayCrtc> getCrtcByPipe(uint32_t pipeIdx);
    std::shared_ptr<HwDisplayConnector> getConnectorById(uint32_t connectorid);

    int32_t getPipeCfg(uint32_t pipeIdx, HwDisplayPipe & pipecfg);

    int32_t bind(
        std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector>  & connector,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes);
    int32_t unbind(std::shared_ptr<HwDisplayCrtc> & crtc);

    void dump(String8 & dumpstr);

/*api in display module*/
public:
    int getDeviceFd() {return mDrmFd;}
    static std::shared_ptr<DrmDevice> & getInstance();
    static void destroyInstance();

    HwDisplayPlane* getPrimaryPlane(int pipeId);

protected:
    void loadResources();
    void loadPipe();
    int32_t freeResources();

private:
    int32_t loadDrmResources();
    int32_t loadNonDrmResources();

protected:
    std::mutex mMutex;
    int mDrmFd;

    std::map<uint32_t, std::shared_ptr<HwDisplayCrtc>> mCrtcs;
    std::map<uint32_t, std::shared_ptr<DrmEncoder>> mEncoders;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> mPlanes;
    std::map<uint32_t, std::shared_ptr<HwDisplayConnector>> mConnectors;

    std::map<uint32_t, HwDisplayPipe> mPipes;

    static std::shared_ptr<DrmDevice> mInstance;
};

std::shared_ptr<DrmDevice> & getDrmDevice();
#endif/*DRM_DEVICE_H*/

