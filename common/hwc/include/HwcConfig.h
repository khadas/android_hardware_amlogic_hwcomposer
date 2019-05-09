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

typedef enum {
    HWC_HDMI_ONLY = 0,
    HWC_PANEL_ONLY,
    HWC_HDMI_CVBS,
    HWC_CVBS_ONLY,
    HWC_CONNECTOR_NULL,
} hwc_connector_t;

typedef enum {
    /*primary:
    viu1 + connector from config
    extend:
    viu2 + connector from config*/
    HWC_PIPE_DEFAULT = 0,

    /*primary:
    viu2 + connector from config
    extend:
    viu1 + connector from config*/
    HWC_PIPE_VIU2_VIU1 = 0,

    /*primary:
        when postprocessor disable: viu1 -> conntector
        when postprocessor enable: viu1->vdin->viu2->conntector
    extend:
        NONE*/
    HWC_PIPE_VIU1VDINVIU2,

    /*primary:
        for headless mode.
    extend:
        NONE
    */
    HWC_PIPE_DUMMY,
} hwc_pipe_policy_t;

class HwcConfig {
public:
    static uint32_t getDisplayNum();
    static int32_t getFramebufferSize(int disp, uint32_t & width, uint32_t & height);

    static hwc_connector_t getConnectorType(int disp);
    static hwc_pipe_policy_t getPipeline();

    static hwc_modes_policy_t getModePolicy(int disp);

    static bool isHeadlessMode();
    static int32_t headlessRefreshRate();

    /*get feature */
    static bool preDisplayCalibrateEnabled();
    static bool softwareVsyncEnabled();
    static bool primaryHotplugEnabled();
    static bool enableExtendDisplay();
    static bool secureLayerProcessEnabled();
    static bool cursorPlaneDisabled();
    static bool defaultHdrCapEnabled();
    static bool forceClientEnabled();

    static void dump(String8 & dumpstr);
};
#endif/*HWC_CONFIG_H*/
