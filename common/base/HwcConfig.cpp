/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <HwcConfig.h>
#include <MesonLog.h>
#include <cutils/properties.h>
#include <systemcontrol.h>
#include <misc.h>
#include <DrmTypes.h>
#include "mode_ubootenv.h"
#include <stdlib.h>

#define UBOOTENV_PRIMARY_CONNECTOR0_TYPE "ubootenv.var.connector0_type"
#define UBOOTENV_PRIMARY_CONNECTOR_TYPE "ubootenv.var.connector_type"
#define UBOOTENV_EXTEND_CONNECTOR_TYPE  "ubootenv.var.connector1_type"
#define UBOOTENV_EXTEND2_CONNECTOR_TYPE "ubootenv.var.connector2_type"

bool HwcConfig::mClientIsSf = true;
int32_t HwcConfig::getFramebufferSize(int disp, uint32_t & width, uint32_t & height) {
    char uiMode[PROPERTY_VALUE_MAX] = {0};
	char value[PROPERTY_VALUE_MAX];

    if (disp == 0) {
        /*primary display*/
        if (sys_get_string_prop("persist.vendor.hwc.ui_mode", uiMode) > 0) {
            if (!strncmp(uiMode, "720", 3)) {
                width  = 1280;
                height = 720;
            } else if (!strncmp(uiMode, "1080", 4)) {
                width  = 1920;
                height = 1080;
            } else {
                MESON_ASSERT(0, "%s: get not support mode [%s] from vendor.ui_mode",
                    __func__, uiMode);
            }
        } else {
        #ifdef HWC_PRIMARY_FRAMEBUFFER_WIDTH
            property_get("sys.lcd.reverse", value, "0");
            if (atoi(value) == 1) {
               width = HWC_PRIMARY_FRAMEBUFFER_HEIGHT;
               height = HWC_PRIMARY_FRAMEBUFFER_WIDTH;
            } else if(atoi(value) == 2) {
               width = 1920*2;
               height = 1200*2;
            } else {
               width  = HWC_PRIMARY_FRAMEBUFFER_WIDTH;
               height = HWC_PRIMARY_FRAMEBUFFER_HEIGHT;
            }
        #else
            MESON_ASSERT(0, "HWC_PRIMARY_FRAMEBUFFER_WIDTH not set.");
        #endif
        }
    } else {
    /*extend display*/
    #ifdef HWC_EXTEND_FRAMEBUFFER_WIDTH
        width = HWC_EXTEND_FRAMEBUFFER_WIDTH;
        height = HWC_EXTEND_FRAMEBUFFER_HEIGHT;
    #else
        MESON_ASSERT(0, "HWC_EXTEND_FRAMEBUFFER_WIDTH not set.");
    #endif
    }

    MESON_LOGI("HwcConfig::default frame buffer size (%d x %d)", width, height);
    return 0;
}

uint32_t HwcConfig::getDisplayNum() {
    static uint32_t displayNum = -1;
    if (displayNum  == -1 ) {
        char prop[PROPERTY_VALUE_MAX] = {0};
        if (sys_get_string_prop("persist.vendor.hwc.display_number", prop) > 0) {
            displayNum = strtol(prop, NULL, 10);
        } else {
            displayNum = HWC_DISPLAY_NUM;
        }
        MESON_LOGD("displayNum %d",displayNum);
    }
    return displayNum;
}

uint32_t HwcConfig::getConnectorType(int disp) {
    uint32_t connector_type = DRM_MODE_CONNECTOR_INVALID_TYPE;
    char strval[PROP_VALUE_LEN_MAX];
    const char * connectorstr = NULL;
    bool isDrmBackend = false;

    if (access("/dev/dri/card0", R_OK | W_OK) == 0)
        isDrmBackend = true;

    if (disp == 0) {
        if (isDrmBackend) {
            connectorstr = ([]() -> const char* {
                const char* str = meson_mode_get_ubootenv(UBOOTENV_PRIMARY_CONNECTOR_TYPE);
                return str ? str : meson_mode_get_ubootenv(UBOOTENV_PRIMARY_CONNECTOR0_TYPE);
            })();
            MESON_LOGD("%s, get %s from uboot env, return %s",
                    __func__, UBOOTENV_PRIMARY_CONNECTOR0_TYPE, connectorstr);
        }

        if (connectorstr == NULL) {
            #ifdef HWC_PRIMARY_CONNECTOR_TYPE
                if (sys_get_string_prop("persist.vendor.hwc.connector-0", strval) > 0)
                    connectorstr = strval;
                else
                    connectorstr = HWC_PRIMARY_CONNECTOR_TYPE;
            #else
                MESON_ASSERT(0, "HWC_PRIMARY_CONNECTOR_TYPE not set.");
            #endif
        }
    } else if (disp == 1) {
        if (isDrmBackend) {
            connectorstr = meson_mode_get_ubootenv(UBOOTENV_EXTEND_CONNECTOR_TYPE);
            MESON_LOGD("%s, get %s from uboot env, return %s",
                    __func__, UBOOTENV_EXTEND_CONNECTOR_TYPE, connectorstr);
        }

        if (connectorstr == NULL) {
            #ifdef HWC_EXTEND_CONNECTOR_TYPE
                if (sys_get_string_prop("persist.vendor.hwc.connector-1", strval) > 0)
                    connectorstr = strval;
                else
                    connectorstr = HWC_EXTEND_CONNECTOR_TYPE;
            #else
                MESON_ASSERT(0, "HWC_EXTEND_CONNECTOR_TYPE not set.");
            #endif
        }
    } else {
        if (isDrmBackend) {
            connectorstr = meson_mode_get_ubootenv(UBOOTENV_EXTEND2_CONNECTOR_TYPE);
            MESON_LOGD("%s, get %s from uboot env, return %s",
                    __func__, UBOOTENV_EXTEND2_CONNECTOR_TYPE, connectorstr);
        }

        if (connectorstr == NULL) {
            if (sys_get_string_prop("persist.vendor.hwc.connector-2", strval) > 0)
                connectorstr = strval;
        }
    }

    if (connectorstr != NULL) {
        /*hdmi str = hdmi + cvbs*/
        if (strcasecmp(connectorstr, "hdmi") == 0) {
            connector_type = HWC_HDMI_CVBS;
        } else if (strcasecmp(connectorstr, "panel") == 0) {
            connector_type = DRM_MODE_CONNECTOR_LVDS;
        } else if (strcasecmp(connectorstr, "hdmi-only") == 0) {
            connector_type = DRM_MODE_CONNECTOR_HDMIA;
        } else {
            std::string con_str(connectorstr);
            std::replace(con_str.begin(), con_str.end(), '_', '-');
            // If the connector type is not in standard HDMI DRM type, convert it to standard
            uint32_t extend_connector_type = drmStringToConnType(con_str.c_str());
            if (extend_connector_type >= DRM_MODE_CONNECTOR_MESON_HDMIA_A &&
                extend_connector_type <= DRM_MODE_CONNECTOR_MESON_HDMIA_C) {
                connector_type = DRM_MODE_CONNECTOR_HDMIA;
            } else if (extend_connector_type >= DRM_MODE_CONNECTOR_MESON_HDMIB_A &&
                       extend_connector_type <= DRM_MODE_CONNECTOR_MESON_HDMIB_C) {
                connector_type = DRM_MODE_CONNECTOR_HDMIB;
            } else {
                connector_type = extend_connector_type;
            }
        }

        MESON_ASSERT(connector_type != DRM_MODE_CONNECTOR_INVALID_TYPE,
            "get invalid type %s", connectorstr);
    }

    MESON_LOGD("%s-%d get connector type %s-%d",
        __func__, disp, connectorstr, connector_type);
    return connector_type;
}

hwc_pipe_policy_t HwcConfig::getPipeline() {
    const char * pipeStr = "default";
    char prop[PROPERTY_VALUE_MAX] = {0};
    if (sys_get_string_prop("persist.vendor.hwc.pipeline", prop) > 0) {
        pipeStr = prop;
        MESON_LOGD("pipeStr %s", pipeStr);
    } else {
#ifdef HWC_PIPELINE
        pipeStr = HWC_PIPELINE;
#endif
    }

    if (strcasecmp(pipeStr, "default") == 0) {
        return HWC_PIPE_DEFAULT;
    } else if (strcasecmp(pipeStr, "dual") == 0) {
        return HWC_PIPE_DUAL;
    } else if (strcasecmp(pipeStr, "VIU1VDINVIU2") == 0) {
        return HWC_PIPE_LOOPBACK;
    } else if (strcasecmp(pipeStr, "multidisplay") == 0) {
        return HWC_PIPE_MULTI;
    } else {
        MESON_ASSERT(0, "getPipeline %s failed.", pipeStr);
    }

    return HWC_PIPE_DEFAULT;
}

hwc_modes_policy_t HwcConfig::getModePolicy(int disp) {
    UNUSED(disp);
#ifdef HWC_ENABLE_FULL_ACTIVE_MODE
    return FULL_ACTIVE_POLICY;
#elif defined HWC_ENABLE_ACTIVE_MODE
    return ACTIVE_MODE_POLICY;
#elif defined HWC_ENABLE_REAL_MODE
    return REAL_MODE_POLICY;
#else
    return FIXED_SIZE_POLICY;
#endif
}

bool HwcConfig::getModeCondition() {
    const char* prop = "persist.vendor.filter.non16_9";
#ifdef HWC_FILTER_16_9_MODE
    if (!sys_get_bool_prop(prop, true)) {
        return false;
    }

    return true;
#else
    if (sys_get_bool_prop(prop, false)) {
        return true;
    }

    return false;
#endif
}

bool HwcConfig::isHeadlessMode() {
#ifdef HWC_ENABLE_HEADLESS_MODE
        return true;
#else
        return false;
#endif
}

int32_t HwcConfig::headlessRefreshRate() {
#ifdef HWC_HEADLESS_REFRESHRATE
        return HWC_HEADLESS_REFRESHRATE;
#else
        MESON_ASSERT(0, "HWC_HEADLESS_REFRESHRATE not set.");
        return 1;
#endif
}

bool HwcConfig::softwareVsyncEnabled() {
#ifdef HWC_ENABLE_SOFTWARE_VSYNC
    return true;
#else
    if (getVsyncScaleFactor() > 0.0) {
        return true;
    } else {
        return false;
    }
#endif
}

bool HwcConfig::preDisplayCalibrateEnabled() {
#ifdef HWC_ENABLE_PRE_DISPLAY_CALIBRATE
    return true;
#else
    return false;
#endif
}

bool HwcConfig::primaryHotplugEnabled() {
#ifdef HWC_ENABLE_PRIMARY_HOTPLUG
    return true;
#else
    return false;
#endif
}

bool HwcConfig::secureLayerProcessEnabled() {
#ifdef HWC_ENABLE_SECURE_LAYER_PROCESS
        return true;
#else
        return false;
#endif
}

bool HwcConfig::cursorPlaneDisabled() {
#ifdef HWC_DISABLE_CURSOR_PLANE
        return true;
#else
        return false;
#endif
}

bool HwcConfig::defaultHdrCapEnabled() {
#ifdef HWC_ENABLE_DEFAULT_HDR_CAPABILITIES
    return true;
#else
    return false;
#endif
}

bool HwcConfig::alwaysVdinLoopback() {
#ifdef HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK
    return true;
#else
    return false;
#endif
}

bool HwcConfig::dynamicSwitchConnectorEnabled() {
#ifdef HWC_DYNAMIC_SWITCH_CONNECTOR
    return true;
#else
    return false;
#endif
}

bool HwcConfig::dynamicSwitchViuEnabled() {
#ifdef HWC_DYNAMIC_SWITCH_VIU
    return true;
#else
    return false;
#endif
}

bool HwcConfig::seamlessSwitchEnabled() {
#ifdef HWC_ENABLE_SEAMLESS_MODE_SWITCH
    return true;
#else
    return false;
#endif
}

float  HwcConfig::getMaxRefreshRate() {
#ifdef HWC_ENFORCES_MAX_REFRESH_RATE
    return (float)HWC_ENFORCES_MAX_REFRESH_RATE;
#else
    return 0.0f;
#endif
}

float HwcConfig::getVsyncScaleFactor() {
    const char* prop = "persist.vendor.hwc.vsync_scale";
#ifdef HWC_VSYNC_PERIOD_SCALE_FACTOR
    if (sys_get_bool_prop(prop, false) && mClientIsSf) {
        return (float)HWC_VSYNC_PERIOD_SCALE_FACTOR;
    } else {
        return 0.0f;
    }
#else
    return 0.0f;
#endif
}

bool HwcConfig::AiSrProcessorEnabled() {
#ifdef ENABLE_VIDEO_AISR
    return true;
#else
    return false;
#endif
}

bool HwcConfig::AiPqProcessorEnabled() {
#ifdef ENABLE_VIDEO_AIPQ
    return true;
#else
    return false;
#endif
}

bool HwcConfig::AiColorProcessorEnabled() {
#ifdef ENABLE_VIDEO_AICOLOR
    return true;
#else
    return false;
#endif
}

bool HwcConfig::DiProcessorEnabled() {
#ifdef ENABLE_VIDEO_DI
    return true;
#else
    return false;
#endif
}

int32_t HwcConfig::getSupportDiChannelNumber() {
    int ret = 0;

#ifdef ENABLE_VIDEO_DI
    char value[PROPERTY_VALUE_MAX];
    const char* str = "vendor.hwc.di_channel_number";

    if (property_get(str, value, NULL) > 0)
        ret = strtol(value, NULL, 10);
#endif

    return ret;
}

int32_t HwcConfig::getSupportAiSrChannelNumber() {
    /* currently, only one channel for aisr is supported */
    return 1;
}

int32_t HwcConfig::getSupportAiPqChannelNumber() {
    /* currently, only one channel for aipq is supported */
    return 1;
}

int32_t HwcConfig::getSupportAiColorChannelNumber() {
/* currently, only one channel for aicolor is supported */
    return 1;
}

bool HwcConfig::mosaicEnabled() {
#ifdef ENABLE_VIDEO_MOSAIC
    if (!property_get_bool("vendor.hwc.mosaic_enable", false))
        return false;

    return true;
#else
    return false;
#endif
}

bool HwcConfig::UvmDetachEnabled() {
#ifdef HWC_UVM_DETACH
    return true;
#else
    return false;
#endif
}

int32_t HwcConfig::getVirtualDisplayRotation() {
    char rotationStr[PROPERTY_VALUE_MAX] = {0};
    int32_t rotation = ROTATION_0;
    if (sys_get_string_prop("vendor.hwc.rotation", rotationStr)) {
        rotation = atoi(rotationStr) / 90;
    }
    return rotation;
}

void HwcConfig::dump(String8 & dumpstr) {
    if (isHeadlessMode()) {
        dumpstr.appendFormat("\t HeadlessMode refreshrate: %d", headlessRefreshRate());
        dumpstr.append("\n");
    } else {
        int displaynum = getDisplayNum();
        for (int i = 0; i < displaynum; i++) {
            uint32_t w,h;
            getFramebufferSize(i, w, h);

            dumpstr.appendFormat("Display (%d) %d x %d :\n", i, w, h);
            dumpstr.appendFormat("\t ConnectorConfig: %d", getConnectorType(i));
            dumpstr.appendFormat("\t ModePolicy: %d", getModePolicy(i));
            dumpstr.append("\n");
            dumpstr.appendFormat("\t PipelineConfig: %d", getPipeline());
            dumpstr.append("\n");
            dumpstr.appendFormat("\t SoftwareVsync: %s", softwareVsyncEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t CursorPlane: %s", cursorPlaneDisabled() ? "N" : "Y");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t PrimaryHotplug: %s", primaryHotplugEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t SecureLayer: %s", secureLayerProcessEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t PreDisplayCalibrate: %s", preDisplayCalibrateEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t DefaultHdr: %s", defaultHdrCapEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t DynamicSwitchConnector: %s", dynamicSwitchConnectorEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t DynamicSwitchViu: %s", dynamicSwitchViuEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t SeamlessSwitch: %s", seamlessSwitchEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t getVsyncScaleFactor: %f", getVsyncScaleFactor());
            dumpstr.append("\n");
            dumpstr.appendFormat("\t AiSrProcessor: %s", AiSrProcessorEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t AiPqProcessor: %s", AiPqProcessorEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t Mosaic: %s", mosaicEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t AiColorProcessor: %s", AiColorProcessorEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t UvmDetach: %s", UvmDetachEnabled() ? "Y" : "N");
            dumpstr.append("\n");
        }
    }

    dumpstr.append("\n");
}

