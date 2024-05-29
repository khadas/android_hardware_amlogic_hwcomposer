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

#include <BasicTypes.h>

#define HWC_HDMI_CVBS 0xffff0000

typedef enum {
    /*primary:
    viu1 + connector from config*/
    HWC_PIPE_DEFAULT = 0,

    /* two display:
    viu1 + connector from config
    extend:
    viu2 + connector from config*/
    HWC_PIPE_DUAL,

    /*primary:
        when postprocessor disable: viu1 -> connector
        when postprocessor enable: viu1->vdin->viu2->connector
    extend:
        NONE*/
    HWC_PIPE_LOOPBACK,

    HWC_PIPE_MULTI,

} hwc_pipe_policy_t;

typedef enum {
    FIXED_SIZE_POLICY = 0,
    FULL_ACTIVE_POLICY = 1,
    ACTIVE_MODE_POLICY,
    REAL_MODE_POLICY,
} hwc_modes_policy_t;

class HwcConfig {
public:
    static uint32_t getDisplayNum();
    static int32_t getFramebufferSize(int disp, uint32_t & width, uint32_t & height);

    static uint32_t getConnectorType(int disp);
    static hwc_pipe_policy_t getPipeline();

    static hwc_modes_policy_t getModePolicy(int disp);
    static bool getModeCondition();

    static bool isHeadlessMode();
    static int32_t headlessRefreshRate();

    /*get feature */
    static bool preDisplayCalibrateEnabled();
    static bool softwareVsyncEnabled();
    static bool primaryHotplugEnabled();
    static bool secureLayerProcessEnabled();
    static bool cursorPlaneDisabled();
    static bool defaultHdrCapEnabled();

    static bool alwaysVdinLoopback();
    static bool dynamicSwitchConnectorEnabled();
    static bool dynamicSwitchViuEnabled();
    static bool seamlessSwitchEnabled();
    static float getMaxRefreshRate();
    static float getVsyncScaleFactor();
    static bool AiSrProcessorEnabled();
    static bool AiPqProcessorEnabled();
    static bool AiColorProcessorEnabled();
    static bool mosaicEnabled();
    static bool UvmDetachEnabled();
    static bool DiProcessorEnabled();
    static int32_t getSupportDiChannelNumber();
    static int32_t getSupportAiSrChannelNumber();
    static int32_t getSupportAiPqChannelNumber();
    static int32_t getSupportAiColorChannelNumber();
    static void setClientIsSf(const bool clientIsSF) { mClientIsSf = clientIsSF; };
    static void dump(String8 & dumpstr);
public:
    static bool mClientIsSf;
};
#endif/*HWC_CONFIG_H*/
