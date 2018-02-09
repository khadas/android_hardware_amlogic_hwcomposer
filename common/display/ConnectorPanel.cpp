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

ConnectorPanel::ConnectorPanel() {
}

ConnectorPanel::~ConnectorPanel() {
}

int ConnectorPanel::init(){
    return NO_ERROR;
}

drm_connector_type_t ConnectorPanel::getType() {
    return DRM_MODE_CONNECTOR_PANEL;
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

KeyedVector<int,DisplayConfig*> ConnectorPanel::getModesInfo() {
#if 0
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

#endif
    return mSupportDispConfigs;
}

void ConnectorPanel:: dump(String8& dumpstr) {

}

