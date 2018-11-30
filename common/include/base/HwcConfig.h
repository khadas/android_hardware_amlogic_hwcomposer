/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_CONFIG_H
#define HWC_CONFIG_H

#include <DrmTypes.h>
#include <BasicTypes.h>

#define CALL_FROM_SF  0
#define CALL_FROM_HWC 1

typedef enum hwc_modes_policy {
    FIXED_SIZE_POLICY = 0,
    FULL_ACTIVE_POLICY
}hwc_modes_policy_t;

class HwcConfig {
public:
    static int32_t getDisplayNum();
    static drm_connector_type_t getConnectorType(int disp);
    static int32_t getFramebufferSize(int disp, uint32_t & width, uint32_t & height);
    static hwc_modes_policy_t getModePolicy();
    static bool isHeadlessMode();
    static int32_t headlessRefreshRate();
    static bool fracRefreshRateEnabled();

    /*get feature */
    static bool preDisplayCalibrateEnabled();
    static bool softwareVsyncEnabled();
    static bool primaryHotplugEnabled();
    static bool secureLayerProcessEnabled();
    static bool cursorPlaneDisabled();
    static bool defaultHdrCapEnabled();
    static bool forceClientEnabled();

    static void dump(String8 & dumpstr);
};
#endif/*HWC_CONFIG_H*/
