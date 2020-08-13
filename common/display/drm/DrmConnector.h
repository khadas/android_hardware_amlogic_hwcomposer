/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_CONNECTOR_H
#define DRM_CONNECTOR_H

#include <string>
#include <vector>

#include <utils/String8.h>
#include <utils/Errors.h>
#include <sys/types.h>

#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>
#include <HwDisplayCrtc.h>


class DrmConnector : public HwDisplayConnector {
public:
    DrmConnector();
    ~DrmConnector();

    uint32_t getId();
    const char * getName();
    drm_connector_type_t getType();



    int32_t loadProperities();
    int32_t update();

    int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);

    bool isSecure();
    bool isConnected();

    void getHdrCapabilities(drm_hdr_capabilities * caps);
    int32_t getIdentificationData(std::vector<uint8_t>& idOut);
    std::string getCurrentHdrType();

    int32_t setContentType(uint32_t contentType);
    void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut);

    int32_t setAutoLowLatencyMode(bool on);

    void dump(String8 & dumpstr);



    /*TODO: unused function, need remove.*/
    bool isRemovable() { MESON_LOG_EMPTY_FUN(); return false; }
    int32_t setMode(drm_mode_info_t & mode __unused) { MESON_LOG_EMPTY_FUN(); return false; }

    /*can use pipe id instead.*/
    int32_t setCrtc(HwDisplayCrtc * crtc __unused) { MESON_LOG_EMPTY_FUN(); return 0; }
    HwDisplayCrtc * getCrtc()  { MESON_LOG_EMPTY_FUN(); return NULL; }

};

#endif/*DRM_CONNECTOR_H*/
