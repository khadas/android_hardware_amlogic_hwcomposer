/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "DrmConnector.h"

DrmConnector::DrmConnector(drmModeConnectorPtr p)
    : HwDisplayConnector(),
    mId(p->connector_id),
    mType(p->connector_type),
    mState(p->connection),
    mPhyWidth(p->mmWidth),
    mPhyHeight(p->mmHeight) {

    loadDisplayModes(p);
}

DrmConnector::~DrmConnector() {

}

uint32_t DrmConnector::getId() {
    return mId;
}

const char * DrmConnector::getName() {
    const char * name;
    switch(mType) {
        case DRM_MODE_CONNECTOR_HDMIA:
            name = "HDMI";
            break;
        case DRM_MODE_CONNECTOR_TV:
            name = "CVBS";
            break;
        case DRM_MODE_CONNECTOR_LVDS:
            name = "PANEL";
            break;
        default:
            name = "UNKNOWN";
            break;
    };

    return name;
}

drm_connector_type_t DrmConnector::getType() {
    return mType;
}

int32_t DrmConnector::update() {
    if (mState == DRM_MODE_CONNECTED) {
        /*load modes*/

    }

    return 0;
}

int32_t DrmConnector::getModes(
    std::map<uint32_t, drm_mode_info_t> & modes) {
    modes = mModes;
    return 0;
}

bool DrmConnector::isSecure() {
    MESON_LOG_EMPTY_FUN();
    return false;
}

bool DrmConnector::isConnected() {
    if (mState == DRM_MODE_UNKNOWNCONNECTION)
        MESON_LOGE("Unkonw connection state (%s)", getName());

    if (mState == DRM_MODE_CONNECTED)
        return true;
    return false;
}

void DrmConnector::getHdrCapabilities(drm_hdr_capabilities * caps) {
    UNUSED(caps);
    MESON_LOG_EMPTY_FUN();
}

int32_t DrmConnector::getIdentificationData(std::vector<uint8_t>& idOut) {
    UNUSED(idOut);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

std::string DrmConnector::getCurrentHdrType() {
    return "SDR";
}

int32_t DrmConnector::setContentType(uint32_t contentType) {
    UNUSED(contentType);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void DrmConnector::getSupportedContentTypes(
    std::vector<uint32_t> & supportedContentTypesOut) {
    UNUSED(supportedContentTypesOut);
    MESON_LOG_EMPTY_FUN();
}

int32_t DrmConnector::setAutoLowLatencyMode(bool on) {
    UNUSED(on);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmConnector::loadDisplayModes(drmModeConnectorPtr p) {
    drmModeModeInfoPtr drmModes = p->modes;
    drm_mode_info_t modeInfo;
    for (int i = 0;i < p->count_modes; i ++) {
        strncpy(modeInfo.name, drmModes[i].name, DRM_DISPLAY_MODE_LEN);
        modeInfo.pixelW = drmModes[i].hdisplay;
        modeInfo.pixelH = drmModes[i].vdisplay;
        modeInfo.dpiX = (modeInfo.pixelW  * 25.4f) / mPhyWidth;
        modeInfo.dpiY = (modeInfo.pixelH   * 25.4f) / mPhyHeight;
        modeInfo.refreshRate = drmModes[i].vrefresh;
        mModes.emplace(mModes.size(), modeInfo);

        MESON_LOGI("add display mode (%s, %dx%d, %f)",
            modeInfo.name, modeInfo.pixelW, modeInfo.pixelH, modeInfo.refreshRate);
    }

    MESON_LOGI("loadDisplayModes (%d) end", mModes.size());
    return 0;
}


void DrmConnector::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
}

