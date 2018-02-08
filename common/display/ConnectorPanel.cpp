/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <utils/Errors.h>
#include <HwDisplayConnector.h>
#include "ConnectorPanel.h"
#include <MesonLog.h>

using namespace android;

ConnectorPanel::ConnectorPanel()
  : mPhyWidth(1920),
    mPhyHeight(1080),
    mWidth(1920),
    mHeight(1080),
    mDpiX(160),
    mDpiY(160),
    mFracRate(true),
    mRefreshRate(60),
    mConnected(true),
    mSecure(true),
    mDisplayMode("1080p60hz"){

}

ConnectorPanel::~ConnectorPanel(){
}


int ConnectorPanel::init(){

    return NO_ERROR;

}

drm_connector_type_t ConnectorPanel::getType(){

return DRM_MODE_CONNECTOR_PANEL;
}

bool ConnectorPanel::isDispModeValid(std::string& dispmode){

    MESON_LOGE("isDispModeValid %s", dispmode.c_str());
    if (dispmode.empty())
        return false;

    vmode_e mode = vmode_name_to_mode(dispmode.c_str());
    MESON_LOGE("isDispModeValid get mode (%d)", mode);
    if (mode == VMODE_MAX)
        return false;

    if (want_hdmi_mode(mode) == 0)
        return false;

    return true;

}


bool ConnectorPanel::isConnected(){
    return true;
}

bool ConnectorPanel::isSecure(){
    return true;
}

uint32_t ConnectorPanel::getModesCount(){

    return 1;
}

status_t ConnectorPanel::readPhySize(framebuffer_info_t& fbInfo) {
    struct fb_var_screeninfo vinfo;
    if ((fbInfo.fd >= 0) && (ioctl(fbInfo.fd, FBIOGET_VSCREENINFO, &vinfo) == 0)) {
        if (int32_t(vinfo.width) > 16 && int32_t(vinfo.height) > 9) {
            mPhyWidth = vinfo.width;
            mPhyHeight = vinfo.height;
        }
        return NO_ERROR;
    }
    return BAD_VALUE;
}

//check the calcDefaultMode can instead of the getDisplayModes
int ConnectorPanel::calcDefaultMode(framebuffer_info_t& framebufferInfo,
        std::string& defaultMode) {
    const struct vinfo_s * mode =
        findMatchedMode(framebufferInfo.info.xres, framebufferInfo.info.yres, 60);
    if (mode == NULL) {
        defaultMode = DEFAULT_DISPMODE;
    } else {
        defaultMode = mode->name;
    }

    defaultMode = DEFAULT_DISPMODE;

    MESON_LOGE("calcDefaultMode %s", defaultMode.c_str());
    return NO_ERROR;
}


KeyedVector<int,DisplayConfig*> ConnectorPanel::updateConnectedConfigs() {
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

void ConnectorPanel:: dump(String8& dumpstr)

{
        updateConnectedConfigs();
        dumpstr.appendFormat("Connector (pannel, %s, %d)\n",
    mDisplayMode.c_str(),
                 1);
        dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        dumpstr.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");
               dumpstr.appendFormat("%s %s     |      %.3f      |   %5d   |   %5d    |"
                    "    %3d    |    %3d    \n",
                           "*   ",
                         mDisplayMode.c_str(),
                         mconfig->getRefreshRate(),
                         mconfig->getWidth(),
                         mconfig->getHeight(),
                         mconfig->getDpiX(),
                         mconfig->getDpiY());

}




