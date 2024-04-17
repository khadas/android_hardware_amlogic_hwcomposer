/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_CRTC_H
#define DRM_CRTC_H

#include <stdlib.h>
#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayCrtc.h>
#include <HwDisplayConnector.h>
#include <HwDisplayPlane.h>
#include "DrmProperty.h"

#define DRM_IOCTL_MESON_CREAT_PRESENT_FENCE    DRM_IOWR(DRM_COMMAND_BASE + \
               0x20, drm_meson_present_fence_t)

/*hdmitx relatde*/
#define DRM_IOCTL_MESON_GET_VRR_RANGE DRM_IOWR(DRM_COMMAND_BASE + \
               0x13, drm_meson_vrr_mode_groups_t)

class DrmCrtc : public HwDisplayCrtc {
public:
    DrmCrtc(int drmFd, drmModeCrtcPtr p, uint32_t pipe);
    ~DrmCrtc();

    /*public apis*/
    int32_t getId();
    uint32_t getPipe();

    int32_t update();

    int32_t getMode(drm_mode_info_t & mode);
    int32_t setMode(drm_mode_info_t & mode, bool seamless = false);

    int32_t waitVBlank(nsecs_t & timestamp);

    int32_t prePageFlip();
    int32_t pageFlip(int32_t & out_fence);
    int32_t updatePropertyValue();
    int32_t atomicClearMode();

    /*TODO:should refact*/
    int32_t readCurDisplayMode(std::string & dispmode);
    int32_t writeCurDisplayAttr(std::string & dispattr __unused) { MESON_LOG_EMPTY_FUN(); return 0; }

    /*unused function only for FBDEV*/
    int32_t setDisplayFrame(display_zoom_info_t & info __unused) { return 0; }
    int32_t getHdrMetadataKeys(std::vector<drm_hdr_metadata_t> & keys __unused) { return 0; }
    int32_t setHdrMetadata(std::map<drm_hdr_metadata_t, float> & hdrmedata __unused) { return 0; }

    int32_t setPendingMode();
    void closeLogoDisplay();
    uint64_t getWrFlag() override;

    int32_t getConversionCaps(std::vector<drm_hdr_conversion_capability>&
                    hdrconversionCaps);
    int32_t getPixelFormats(std::vector<uint32_t>& pixelFormats);

    void dump(String8 & dumpstr);

    /*internal drm package api*/
public:
    drmModeAtomicReqPtr getAtomicReq();

    int setConnectorId(uint32_t connectorId);

protected:
    int32_t loadProperties();
    int32_t initConversionCaps();
    int32_t setModeLocked(drm_mode_info_t & mode, bool seamless = false);

protected:
	int mDrmFd;
    uint32_t mId;
    /*
     * Pipe is the crtc index in kernel.
     * connector report possible crtc with shifted mask.
     */
    uint32_t mPipe;
    drmModeModeInfo mDrmMode;
    drm_mode_info mMesonMode;

    /*crtc propertys*/
    std::shared_ptr<DrmProperty> mActive;
    std::shared_ptr<DrmProperty> mModeBlobId;
    std::shared_ptr<DrmProperty> mOutFencePtr;
    std::shared_ptr<DrmProperty> mVrrEnabled;
    std::shared_ptr<DrmProperty> mVideoPixelFormat;
    std::shared_ptr<DrmProperty> mOsdPixelFormat;
    std::shared_ptr<DrmProperty> mHdrConversionCaps;
    std::shared_ptr<DrmProperty> mBrrUpdate;
    std::shared_ptr<DrmProperty> mWrFlag;

    drmModeAtomicReqPtr mReq;

    std::mutex mMutex;
    uint32_t mConnectorId;
    std::vector<drm_mode_info> mPendingModes;

    std::vector<drm_hdr_conversion_capability> mDrmHdrConversionCaps;

    /* uboot logo closed */
    bool mLogoClosed;
};

#endif/*DRM_CRTC_H*/

