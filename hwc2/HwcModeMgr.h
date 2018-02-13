/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef IHWC_MODE_MGR_H
#define IHWC_MODE_MGR_H

#include <hardware/hwcomposer2.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>

/*
 *For different connectors, we need different manage strategy.
 *This class defines basic interface to manage display modes.
 */
class HwcModeMgr {
public:
    HwcModeMgr() {}
    virtual ~HwcModeMgr() {}

    virtual const char * getName();

    virtual hwc2_error_t  getDisplayConfigs(
        uint32_t* outNumConfigs, hwc2_config_t* outConfigs) = 0;
    virtual hwc2_error_t  getDisplayAttribute(
        hwc2_config_t config, int32_t attribute, int32_t* outValue) = 0;
    virtual hwc2_error_t getActiveConfig(hwc2_config_t* outConfig) = 0;
    virtual hwc2_error_t setActiveConfig(hwc2_config_t config) = 0;
};

std::shared_ptr<HwcModeMgr> createModeMgr(std::shared_ptr<HwDisplayConnector>& connector);

#endif/*IHWC_MODE_MGR_H*/