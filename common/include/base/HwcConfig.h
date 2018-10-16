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


class HwcConfig {
public:
    static int32_t getDisplayNum();
    static drm_connector_type_t getConnectorType(int disp);
    static int32_t getFramebufferSize(int disp, uint32_t & width, uint32_t & height);
    static bool isHeadlessMode();
    static int32_t headlessRefreshRate();

    /*get feature */
    static bool softwareVsyncEnabled();
    static bool primaryHotplugEnabled();
    static bool secureLayerProcessEnabled();
    static bool cursorPlaneDisabled();
};
#endif/*HWC_CONFIG_H*/
