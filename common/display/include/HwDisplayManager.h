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
};

std::shared_ptr<HwDisplayManager> getHwDisplayManager();
int32_t destroyHwDisplayManager();

#endif/*HW_DISPLAY_MANAGER_H*/
