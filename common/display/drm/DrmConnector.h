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
#include "DrmProperty.h"


class DrmConnector : public HwDisplayConnector {
public:
    DrmConnector(int drmFd, drmModeConnectorPtr p);
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

    /*edid blob*/
    int32_t getIdentificationData(std::vector<uint8_t>& idOut);

    /*no drm stard props*/
    void updateHdrCaps();
    void getHdrCapabilities(drm_hdr_capabilities * caps);
    std::string getCurrentHdrType();
    int32_t setContentType(uint32_t contentType);
    void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut);
    int32_t setAutoLowLatencyMode(bool on);

    void dump(String8 & dumpstr);

    int32_t setMode(drm_mode_info_t & mode );

    /*drm package internal use.*/
public:
    uint32_t getEncoderId() {return mEncoderId;}
    uint32_t getModeBlobId(drm_mode_info_t & mode);
    int getDrmModeByBlobId(drmModeModeInfo & drmmode, uint32_t blobid);
    int getCrtcProp(std::shared_ptr<DrmProperty> & prop);
    int getUpdateProp(std::shared_ptr<DrmProperty> & prop);

    int DrmMode2Mode(drmModeModeInfo & drmmode, drm_mode_info_t & mode);

    std::mutex mMutex;

protected:
    int32_t loadConnectorInfo(drmModeConnectorPtr metadata);
    int32_t loadDisplayModes(drmModeConnectorPtr p);
    int32_t loadProperties(drmModeConnectorPtr p);

    int32_t parseHdrCapabilities();

protected:
    int mDrmFd;
    uint32_t mId;
    uint32_t mType;
    uint32_t mEncoderId;
    drmModeConnection mState;
    int32_t mFracMode;

    /*mode_id, modeinfo. mode_id is created by userspace, not from kernel.*/
    std::map<uint32_t, drmModeModeInfo> mDrmModes;
    std::map<uint32_t, drm_mode_info_t> mMesonModes;
    std::vector<float> mFracRefreshRates;

    /*HxW in millimeters*/
    uint32_t mPhyWidth;
    uint32_t mPhyHeight;

    /*Connector props*/
    std::shared_ptr<DrmProperty> mCrtcId;
    std::shared_ptr<DrmProperty> mEdid;
    std::shared_ptr<DrmProperty> mColorSpace;
    std::shared_ptr<DrmProperty> mColorDepth;
    std::shared_ptr<DrmProperty> mHdrCaps;
    std::shared_ptr<DrmProperty> mUpdate;

    /*TODO:shuld convert to prop.*/
    drm_hdr_capabilities mHdrCapabilities;
};

#endif/*DRM_CONNECTOR_H*/
