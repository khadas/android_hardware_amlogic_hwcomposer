/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "ConnectorPanel.h"
#include "AmVinfo.h"
#include <MesonLog.h>

ConnectorPanel::ConnectorPanel() {
}

ConnectorPanel::~ConnectorPanel() {
}

int32_t ConnectorPanel::init(){
    return 0;
}

drm_connector_type_t ConnectorPanel::getType() {
    return DRM_MODE_CONNECTOR_PANEL;
}

bool ConnectorPanel::isRemovable() {
    return false;
}

bool ConnectorPanel::isConnected(){
    return true;
}

bool ConnectorPanel::isSecure(){
    return true;
}

uint32_t ConnectorPanel::getModesCount() {
    return 1;
}

auto ConnectorPanel::getSystemControlService() {
    static bool bGot = false;

#if PLATFORM_SDK_VERSION >= 26
    sp<ISystemControl> systemControl = ISystemControl::getService();

    if (bGot)
        return systemControl;

    mDeathRecipient = new SystemControlDeathRecipient();
    Return<bool> linked = systemControl->linkToDeath(mDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        MESON_LOGE("Transaction error in linking to system service death: %s", linked.description().c_str());
    } else if (!linked) {
        MESON_LOGE("Unable to link to system service death notifications");
    } else {
        MESON_LOGV("Link to system service death notification successful");
    }

#else
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        MESON_LOGE("Couldn't get default ServiceManager\n");
        return NULL;
    }
    sp<ISystemControlService> systemControl = interface_cast<ISystemControlService>(sm->getService(String16("system_control")));

    if (bGot)
        return systemControl;

    if (systemControl == NULL) {
        MESON_LOGE("Couldn't get connection to SystemControlService\n");
        return NULL;
    }
#endif

    bGot = true;
    return systemControl;
}


std::string ConnectorPanel::readDispMode(std::string &displaymode) {
    auto scs = getSystemControlService();
    if (scs == NULL) {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
    }

#if PLATFORM_SDK_VERSION >= 26
    scs->getActiveDispMode([&displaymode](const Result &ret, const hidl_string&supportDispModes) {
        if (Result::OK == ret) {
            displaymode = supportDispModes.c_str();
        } else {
            MESON_LOGE("syscontrol::getActiveDispMode Error");
        }
    });

    if (displaymode.empty()) {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
    }

#else
    if (scs->getActiveDispMode(&displaymode)) {
    } else {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
    }
#endif
    mDisplayMode = displaymode;
    return mDisplayMode;
}


KeyedVector<int,DisplayConfig*> ConnectorPanel::getModesInfo() {
    std::string dispmode;
    readPhySize();
    readDispMode(dispmode);

    vmode_e vmode = vmode_name_to_mode(mDisplayMode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        MESON_LOGE("addSupportedConfig meet error mode (%s, %d)", mDisplayMode.c_str(), vmode);
    }

    int dpiX  = DEFAULT_DISPLAY_DPI, dpiY = DEFAULT_DISPLAY_DPI;
    if (mPhyWidth > 16 && mPhyHeight > 9) {
        dpiX = (vinfo->width  * 25.4f) / mPhyWidth;
        dpiY = (vinfo->height  * 25.4f) / mPhyHeight;
    }

    mconfig = new DisplayConfig(mDisplayMode,
                                        vinfo->sync_duration_num,
                                        vinfo->width,
                                        vinfo->height,
                                        dpiX,
                                        dpiY,
                                        false);

    // add normal refresh rate config, like 24hz, 30hz...
    MESON_LOGE("add display mode pair (%d, %s)", mSupportDispConfigs.size(), mDisplayMode.c_str());
    mSupportDispConfigs.add(mSupportDispConfigs.size(), mconfig);

    return mSupportDispConfigs;
}

int32_t ConnectorPanel::readPhySize() {
    mPhyWidth = DEFAULT_DISPLAY_DPI;
    mPhyHeight = DEFAULT_DISPLAY_DPI;
    return 0;
}

void ConnectorPanel:: dump(String8& dumpstr) {
    getModesInfo();
    dumpstr.appendFormat("Connector (Panel, %d, %d)\n",
                 getModesCount(),
                 1);
        dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        dumpstr.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");

            int mode = mSupportDispConfigs.keyAt(0);
            DisplayConfig *config = mSupportDispConfigs.valueAt(0);
            if (config) {
                dumpstr.appendFormat(" %2d     |      %.3f      |   %5d   |   %5d    |"
                    "    %3d    |    %3d    \n",
                         mode,
                         config->getRefreshRate(),
                         config->getWidth(),
                         config->getHeight(),
                         config->getDpiX(),
                         config->getDpiY());
            }
    // HDR info
    dumpstr.append("  HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%zu\n", mHdrCapabilities.dvSupport?1:0);
    dumpstr.appendFormat("    HDR10=%zu, maxLuminance=%zu, avgLuminance=%zu, minLuminance=%zu\n",
        mHdrCapabilities.hdrSupport?1:0, mHdrCapabilities.maxLuminance, mHdrCapabilities.avgLuminance, mHdrCapabilities.minLuminance);


}

