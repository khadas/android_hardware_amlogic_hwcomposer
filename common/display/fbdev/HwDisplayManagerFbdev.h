/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HW_DISPLAY_MANAGER_FBDEV_H
#define HW_DISPLAY_MANAGER_FBDEV_H

#include <time.h>

#include <BasicTypes.h>
#include <HwDisplayCrtc.h>
#include <HwDisplayPlane.h>
#include <HwDisplayConnector.h>
#include <HwDisplayManager.h>

class HwDisplayManagerFbdev : public HwDisplayManager {
friend class HwDisplayCrtc;
friend class HwDisplayConnector;
friend class HwDisplayPlane;

public:
    HwDisplayManagerFbdev();
    ~HwDisplayManagerFbdev();

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    int32_t getPlanes(
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes);

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    int32_t getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs);

    int32_t getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type);

    std::shared_ptr<HwDisplayCrtc> getCrtcById(uint32_t crtcid);
    std::shared_ptr<HwDisplayCrtc> getCrtcByPipe(uint32_t pipeIdx);

    int32_t bind(
        std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector>  & connector,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes);
    int32_t unbind(std::shared_ptr<HwDisplayCrtc> & crtc);

    void dump(String8 & dumpstr);

/*********************************************
 * drm apis.
 *********************************************/
protected:
    int32_t loadDrmResources();
    int32_t freeDrmResources();

    int32_t loadCrtcs();
    int32_t loadConnectors();
    int32_t loadPlanes();

protected:
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> mPlanes;
    std::map<uint32_t, std::shared_ptr<HwDisplayCrtc>> mCrtcs;
    std::map<drm_connector_type_t, std::shared_ptr<HwDisplayConnector>> mConnectors;

    std::mutex mMutex;
};

#endif/*HW_DISPLAY_MANAGER_FBDEV_H*/
