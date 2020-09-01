/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_CONNECTOR_H
#define DRM_CONNECTOR_H

#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>
#include <HwDisplayCrtc.h>


class DrmConnector : public HwDisplayConnector {
public:
    DrmConnector(drmModeConnectorPtr p);
    ~DrmConnector();

    uint32_t getId();
    const char * getName();
    drm_connector_type_t getType();
    int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);

    bool isSecure();
    bool isConnected();

    int32_t update();

    /*set & get currentn crtc id*/
    int32_t setCrtcId(uint32_t crtcid);
    uint32_t getCrtcId();

    /*no drm stard props*/
    int32_t getIdentificationData(std::vector<uint8_t>& idOut);
    void getHdrCapabilities(drm_hdr_capabilities * caps);
    std::string getCurrentHdrType();
    int32_t setContentType(uint32_t contentType);
    void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut);
    int32_t setAutoLowLatencyMode(bool on);

    void dump(String8 & dumpstr);

    /*TODO: unused function, need remove.*/
    bool isRemovable() { MESON_LOG_EMPTY_FUN(); return false; }
    int32_t setMode(drm_mode_info_t & mode __unused) { MESON_LOG_EMPTY_FUN(); return false; }

    /*drm package internal use.*/
public:
    int32_t setEncoderId(uint32_t encoderid);
    uint32_t getEncoderId();

protected:
    int32_t loadDisplayModes(drmModeConnectorPtr p);

protected:
    uint32_t mId;
    uint32_t mType;
    uint32_t mCrtcId;
    uint32_t mEncoderId;
    drmModeConnection mState;

    /*mode_id, modeinfo. mode_id is created by userspace, not from kernel.*/
    std::map<uint32_t, drm_mode_info_t> mModes;

    /*HxW in millimeters*/
    uint32_t mPhyWidth;
    uint32_t mPhyHeight;
};

#endif/*DRM_CONNECTOR_H*/
