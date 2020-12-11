/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "DrmConnector.h"
#include "DrmDevice.h"
#include <inttypes.h>

DrmConnector::DrmConnector(int drmFd, drmModeConnectorPtr p)
    : HwDisplayConnector(),
    mDrmFd(drmFd),
    mId(p->connector_id),
    mType(p->connector_type),
    mState(p->connection),
    mPhyWidth(p->mmWidth),
    mPhyHeight(p->mmHeight) {

    mEncoderId = p->encoder_id;
    loadDisplayModes(p);
    loadProperties(p);
}

DrmConnector::~DrmConnector() {

}

int32_t DrmConnector::loadProperties(drmModeConnectorPtr p) {
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } connectorProps[] = {
        {DRM_CONNECTOR_PROP_CRTCID, &mCrtcId},
        {DRM_CONNECTOR_PROP_EDID, &mEdid},
        {DRM_HDMI_PROP_COLORSPACE, &mColorSpace},
        {DRM_HDMI_PROP_COLORDEPTH, &mColorDepth},
        {DRM_HDMI_PROP_HDRCAP, &mHdrCaps},
    };
    const int connectorPropsNum = sizeof(connectorProps)/sizeof(connectorProps[0]);
    int initedProps = 0;

    for (int i = 0; i < p->count_props; i++) {
        drmModePropertyPtr prop =
            drmModeGetProperty(mDrmFd, p->props[i]);
        for (int j = 0; j < connectorPropsNum; j++) {
            if (strcmp(prop->name, connectorProps[j].propname) == 0) {
                *(connectorProps[j].drmprop) =
                    std::make_shared<DrmProperty>(prop, mId, p->prop_values[i]);
                initedProps ++;
                break;
            }
        }
       drmModeFreeProperty(prop);
    }

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

    MESON_LOGI("loadDisplayModes (%" PRIuFAST16 ") end", mModes.size());
    return 0;
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
    MESON_LOG_EMPTY_FUN();
    if (mState == DRM_MODE_CONNECTED) {
    }

    return 0;
}

int32_t DrmConnector::setCrtcId(uint32_t crtcid) {
    mCrtcId->setValue(crtcid);
    return 0;
}

uint32_t DrmConnector::getCrtcId() {
    return (uint32_t)mCrtcId->getValue();
}

int32_t DrmConnector::setEncoderId(uint32_t encoderid) {
    mEncoderId = encoderid;
    return 0;
}

uint32_t DrmConnector::getEncoderId() {
    return mEncoderId;
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
    if (mHdrCaps) {
        MESON_LOG_EMPTY_FUN();
    } else {
        memset(caps, 0, sizeof(drm_hdr_capabilities));
    }
}

int32_t DrmConnector::getIdentificationData(std::vector<uint8_t>& idOut) {
    if (mEdid) {
        return mEdid->getBlobData(idOut);
    }

    MESON_LOGE("No edid for connector (%s)", getName());
    return -EINVAL;
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

void DrmConnector::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
}

