/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HW_DISPLAY_MANAGER_H
#define HW_DISPLAY_MANAGER_H

#include <time.h>

#include <BasicTypes.h>
#include <HwDisplayCrtc.h>
#include <HwDisplayPlane.h>
#include <HwDisplayConnector.h>

struct HwDisplayPipe {
    uint32_t crtc_id;
    uint32_t connector_id;
};

class HwDisplayManager {
friend class HwDisplayCrtc;
friend class HwDisplayConnector;
friend class HwDisplayPlane;

public:
    HwDisplayManager() { }
    virtual ~HwDisplayManager() { }

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    virtual int32_t getPlanes(
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes) = 0;

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    virtual int32_t getCrtcs(
        std::vector<std::shared_ptr<HwDisplayCrtc>> & crtcs) = 0;

    virtual int32_t getConnector(
        std::shared_ptr<HwDisplayConnector> & connector,
        drm_connector_type_t type) = 0;

    virtual std::shared_ptr<HwDisplayCrtc> getCrtcById(uint32_t crtcid) = 0;
    virtual std::shared_ptr<HwDisplayCrtc> getCrtcByPipe(uint32_t pipeIdx) = 0;

    virtual int32_t bind(
        std::shared_ptr<HwDisplayCrtc> & crtc,
        std::shared_ptr<HwDisplayConnector>  & connector,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes) = 0;
    virtual int32_t unbind(std::shared_ptr<HwDisplayCrtc> & crtc) = 0;

    virtual void dump(String8 & dumpstr) = 0;
};

std::shared_ptr<HwDisplayManager> getHwDisplayManager();
int32_t destroyHwDisplayManager();

#endif/*HW_DISPLAY_MANAGER_H*/
