/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>

#include "DrmConnector.h"

DrmConnector::DrmConnector()
    : HwDisplayConnector() {
}

DrmConnector::~DrmConnector() {
}

uint32_t DrmConnector::getId() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

const char * DrmConnector::getName() {
    MESON_LOG_EMPTY_FUN();
    return NULL;
}

drm_connector_type_t DrmConnector::getType() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmConnector::update() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t DrmConnector::getModes(std::map<uint32_t, drm_mode_info_t> & modes) {
    UNUSED(modes);
    MESON_LOG_EMPTY_FUN();
    return 0;
}

bool DrmConnector::isSecure() {
    MESON_LOG_EMPTY_FUN();
    return false;
}

bool DrmConnector::isConnected() {
    MESON_LOG_EMPTY_FUN();
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

void DrmConnector::dump(String8 & dumpstr) {
    UNUSED(dumpstr);
}

