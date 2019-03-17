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

int32_t HwcConfig::getFramebufferSize(int disp, uint32_t & width, uint32_t & height) {
    char uiMode[PROPERTY_VALUE_MAX] = {0};
    if (disp == 0) {
        /*primary display*/
        if (sys_get_string_prop("vendor.ui_mode", uiMode) > 0) {
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
            width  = HWC_PRIMARY_FRAMEBUFFER_WIDTH;
            height = HWC_PRIMARY_FRAMEBUFFER_HEIGHT;
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
    return HWC_DISPLAY_NUM;
}

drm_connector_type_t HwcConfig::getConnectorType(int disp) {
    drm_connector_type_t connector_type = DRM_MODE_CONNECTOR_INVALID;
    const char * connectorstr = NULL;
    if (disp == 0) {
        #ifdef HWC_PRIMARY_CONNECTOR_TYPE
            connectorstr = HWC_PRIMARY_CONNECTOR_TYPE;
        #else
            MESON_ASSERT(0, "HWC_PRIMARY_CONNECTOR_TYPE not set.");
        #endif
    } else {
        #ifdef HWC_EXTEND_CONNECTOR_TYPE
            connectorstr = HWC_EXTEND_CONNECTOR_TYPE;
        #else
            MESON_ASSERT(0, "HWC_EXTEND_CONNECTOR_TYPE not set.");
        #endif
    }

    if (connectorstr != NULL) {
        if (strcasecmp(connectorstr, "hdmi") == 0) {
            connector_type = DRM_MODE_CONNECTOR_HDMI;
        } else if (strcasecmp(connectorstr, "panel") == 0) {
            connector_type = DRM_MODE_CONNECTOR_PANEL;
        } else if (strcasecmp(connectorstr, "cvbs") == 0) {
            connector_type = DRM_MODE_CONNECTOR_CVBS;
        } else {
            MESON_LOGE("%s-%d get connector type failed.", __func__, disp);
        }
    }

    MESON_LOGD("%s-%d get connector type %s-%d",
        __func__, disp, connectorstr, connector_type);
    return connector_type;
}

hwc_modes_policy_t HwcConfig::getModePolicy() {
#ifdef HWC_ENABLE_ACTIVE_MODE
    return FULL_ACTIVE_POLICY;
#else
    return FIXED_SIZE_POLICY;
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

bool HwcConfig::fracRefreshRateEnabled() {
#ifdef ENABLE_FRACTIONAL_REFRESH_RATE
    return true;
#else
    return false;
#endif
}

bool HwcConfig::softwareVsyncEnabled() {
#ifdef HWC_ENABLE_SOFTWARE_VSYNC
    return true;
#else
    return false;
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

bool HwcConfig::forceClientEnabled() {
#ifdef HWC_FORCE_CLIENT_COMPOSITION
    return true;
#else
    return false;
#endif
}

void HwcConfig::dump(String8 & dumpstr) {
    #ifdef HWC_RELEASE
    dumpstr.append("HwcConfigs (RELEASE):\n");
    #else
    dumpstr.append("HwcConfigs (DEBUG):\n");
    #endif

    if (isHeadlessMode()) {
        dumpstr.appendFormat("\t HeadlessMode refreshrate: %d", headlessRefreshRate());
        dumpstr.append("\n");
    } else {
        int displaynum = getDisplayNum();
        for (int i = 0; i < displaynum; i++) {
            dumpstr.appendFormat("Display:(%d) \n", i);
            uint32_t w,h;
            getFramebufferSize(i, w, h);
            dumpstr.appendFormat("\t Fb: %d x %d", w, h);
            dumpstr.appendFormat("\t Conntecor: %d", getConnectorType(i));
            dumpstr.appendFormat("\t ModePolicy: %d", getModePolicy());
            dumpstr.append("\n");
            dumpstr.appendFormat("\t SoftwareVsync: %s", softwareVsyncEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t CursorPlane: %s", cursorPlaneDisabled() ? "N" : "Y");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t PrimaryHotplug: %s", primaryHotplugEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t SecureLayer: %s", secureLayerProcessEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t FracRefreshRate: %s", fracRefreshRateEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t PreDisplayCalibrate: %s", preDisplayCalibrateEnabled() ? "Y" : "N");
            dumpstr.append("\n");
            dumpstr.appendFormat("\t DefaultHdr: %s", defaultHdrCapEnabled() ? "Y" : "N");
            dumpstr.appendFormat("\t ForceClient: %s", forceClientEnabled() ? "Y" : "N");
            dumpstr.append("\n");
        }
    }

    dumpstr.append("\n");
}

