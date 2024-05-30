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
    drm_connector_type_t getType(bool extend_connector_type = false);
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
    bool getDvCap(std::string & dv_cap);
    bool isDvEnable();
    int32_t setContentType(uint32_t contentType);
    void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut);
    int32_t setAutoLowLatencyMode(bool on);

    void dump(String8 & dumpstr);

    int32_t setMode(drm_mode_info_t & mode );
    bool checkFracMode(const drm_mode_info_t & mode) override;
    int32_t getColorDepth(uint32_t & colorDepth );
    ENUM_HDMI_COLOR_SPACE getColorSpace();
    int32_t setHDMIContentType(uint32_t contentType);

    bool isTvSupportALLM();
    bool supportSourceLed();
    bool supportSinkLed();
    int32_t setAVMute(uint32_t mute);

    int32_t getHdrPriority(uint32_t & hdrPriority);
    int32_t setHdrPriority(uint32_t value);
    int32_t setColorAttribute(const std::string & hdmiColorAttr);
    int32_t applyConnectorProps(drmModeAtomicReqPtr &req);

    bool isReady() override;
    bool supportVrr() override;

    /*drm package internal use.*/
public:
    uint32_t getEncoderId() {return mEncoderId;}
    uint32_t getModeBlobId(drm_mode_info_t & mode);
    int getDrmModeByBlobId(drmModeModeInfo & drmmode, uint32_t blobid);
    int getCrtcProp(std::shared_ptr<DrmProperty> & prop);
    int getUpdateProp(std::shared_ptr<DrmProperty> & prop);

    int DrmMode2Mode(drmModeModeInfo & drmmode, drm_mode_info_t & mode);
    bool isTvType();
    bool isHDMIType();

    std::mutex mMutex;
    bool getHdrType(std::string & hdrType);

protected:
    int32_t loadConnectorInfo(drmModeConnectorPtr metadata);
    int32_t loadDisplayModes(drmModeConnectorPtr p);
    int32_t loadProperties(drmModeConnectorPtr p);
    int32_t parseDvCapabilities();

    int32_t parseHdrCapabilities();
    int32_t groupDisplayModes();
    int32_t loadVrrModeGroups();
    bool isSeamlessMode(const drm_mode_info_t & mode, const drm_mode_info_t &groupMode);

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
    /* for seamless group id */
    std::map<uint32_t, std::vector<drm_mode_info_t *>> mMesonGroupModes;
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
    std::shared_ptr<DrmProperty> mHdrStatus;
    std::shared_ptr<DrmProperty> mContentType;
    std::shared_ptr<DrmProperty> mAVMute;
    std::shared_ptr<DrmProperty> mDvCaps;
    std::shared_ptr<DrmProperty> mReady;
    /*for lcd now*/
    std::shared_ptr<DrmProperty> mMesonConnectorType;
    /* for vrr */
    std::shared_ptr<DrmProperty> mVrrCap;
    /*for mask hdr */
    std::shared_ptr<DrmProperty> mHdrPriority;

    /*TODO:should convert to prop.*/
    drm_hdr_capabilities mHdrCapabilities;

    /* support VRR or not */
    bool mSupportVrr = false;
    bool mSupportDv = false;

    drm_meson_vrr_mode_groups_t mVrrModeGroup;
};

#endif/*DRM_CONNECTOR_H*/
