/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "ConnectorCvbs.h"

ConnectorCvbs::ConnectorCvbs(int32_t drvFd, uint32_t id)
    :   HwDisplayConnectorFbdev(drvFd, id) {
    snprintf(mName, 64, "CVBS-%d", id);
}

ConnectorCvbs::~ConnectorCvbs() {
}

int32_t ConnectorCvbs::update() {
    loadPhysicalSize();
    mDisplayModes.clear();

    std::string cvbs576("576cvbs");
    std::string cvbs480("480cvbs");
    std::string pal_m("pal_m");
    std::string pal_n("pal_n");
    std::string ntsc_m("ntsc_m");
    addDisplayMode(cvbs576);
    addDisplayMode(cvbs480);
    addDisplayMode(pal_m);
    addDisplayMode(pal_n);
    addDisplayMode(ntsc_m);
    return 0;
}

const char * ConnectorCvbs::getName() {
    return mName;
}

drm_connector_type_t ConnectorCvbs::getType() {
    return DRM_MODE_CONNECTOR_TV;
}

bool ConnectorCvbs::isSecure() {
    return false;
}

bool ConnectorCvbs::isConnected() {
    return true;
}

void ConnectorCvbs::getHdrCapabilities(drm_hdr_capabilities * caps) {
    /* cvbs has no hdr capabilitites */
    if (caps == nullptr) {
        MESON_LOGE("[%s] parameter caps is null, please check", __func__);
        return;
    }

    MESON_LOGD("cvbs connector getHadrCapabilities none");
    memset(caps, 0, sizeof(drm_hdr_capabilities));
}

void ConnectorCvbs::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
}

