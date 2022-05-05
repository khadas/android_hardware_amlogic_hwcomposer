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
#include <FbProcessor.h>
#include <HwcModeMgr.h>
#include <HwcDisplayPipe.h>

#define HWC_HDMI_CVBS 0xffff0000

class HwcConfig {
public:
    static uint32_t getDisplayNum();
    static int32_t getFramebufferSize(int disp, uint32_t & width, uint32_t & height);

    static uint32_t getConnectorType(int disp);
    static hwc_pipe_policy_t getPipeline();

    static hwc_modes_policy_t getModePolicy(int disp);

    static bool isHeadlessMode();
    static int32_t headlessRefreshRate();

    /*get feature */
    static bool preDisplayCalibrateEnabled();
    static bool softwareVsyncEnabled();
    static bool primaryHotplugEnabled();
    static bool secureLayerProcessEnabled();
    static bool cursorPlaneDisabled();
    static bool defaultHdrCapEnabled();
    static bool forceClientEnabled();

    static bool alwaysVdinLoopback();
    static bool dynamicSwitchConnectorEnabled();
    static bool dynamicSwitchViuEnabled();

    static float getMaxRefreshRate();
    static bool AiSrProcessorEnabled();
    static bool AiPqProcessorEnabled();

    static void dump(String8 & dumpstr);
};
#endif/*HWC_CONFIG_H*/
