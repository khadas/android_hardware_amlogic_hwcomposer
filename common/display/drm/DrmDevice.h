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

#include <BasicTypes.h>
#include <HwDisplayManager.h>

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

protected:
    int32_t loadDrmResources();
    int32_t loadNonDrmPlanes();
    int32_t freeDrmResources();

protected:
    std::mutex mMutex;
    int mDrmFd;

    std::vector<std::shared_ptr<HwDisplayPlane>> mPlanes;
    std::vector<std::shared_ptr<HwDisplayCrtc>> mCrtcs;
    std::map<drm_connector_type_t, std::shared_ptr<HwDisplayConnector>> mConnectors;
};

#endif/*DRM_DEVICE_H*/

