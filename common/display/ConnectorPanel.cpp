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

ConnectorPanel::ConnectorPanel(int32_t drvFd, uint32_t id)
    :   HwDisplayConnector(drvFd, id) {
    snprintf(mName, 64, "Panel-%d", id);
}

ConnectorPanel::~ConnectorPanel() {
}

int32_t ConnectorPanel::loadProperities() {
    loadPhysicalSize();
    loadDisplayModes();
    return 0;
}

const char * ConnectorPanel::getName() {
    return mName;
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

int32_t ConnectorPanel::loadDisplayModes() {
    vmode_e vmode;
    struct vinfo_base_s info;
    int ret = -1 /*read_vout_info(&info)*/;
    if (ret == 0)
        vmode = info.mode;
    else {
        vmode = VMODE_MAX;
        MESON_LOGE("read vout info error return %d", ret);
    }
    MESON_LOGD("readDisplayPhySize vmode: %d", vmode);
    //Tmp
    vmode = VMODE_1080P;
    for (int i = 0; i < 2; i++) {
        const struct vinfo_s* vinfo = get_tv_info(vmode);
        if (vmode == VMODE_MAX || vinfo == NULL) {
            MESON_LOGE("loadDisplayModes meet error mode (%d)", vmode);
            return -ENOENT;
        }

        std::string dispmode = vinfo->name;
        addDisplayMode(dispmode);
        int pos = dispmode.find("60hz", 0);
        if (pos > 0) {
            dispmode.replace(pos, 4, "50hz");
            addDisplayMode(dispmode);
        } else {
            MESON_LOGE("loadDisplayModes can not find 60hz in %s", dispmode.c_str());
        }
        vmode = VMODE_4K2K_60HZ;
    }

    return 0;
}

void ConnectorPanel::getHdrCapabilities(drm_hdr_capabilities * caps) {
    if (caps) {
        memset(caps, 0, sizeof(drm_hdr_capabilities));
    }
}

void ConnectorPanel:: dump(String8& dumpstr) {
    dumpstr.appendFormat("Connector (Panel,  %d)\n",1);
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");

    std::map<uint32_t, drm_mode_info_t>::iterator it = mDisplayModes.begin();
    for ( ; it != mDisplayModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    \n",
                 it->first,
                 it->second.refreshRate,
                 it->second.pixelW,
                 it->second.pixelH,
                 it->second.dpiX,
                 it->second.dpiY);
    }
}

