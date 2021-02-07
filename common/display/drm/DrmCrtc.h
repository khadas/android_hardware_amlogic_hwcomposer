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

class DrmCrtc : public HwDisplayCrtc {
public:
    DrmCrtc(int drmFd, drmModeCrtcPtr p, uint32_t pipe);
    ~DrmCrtc();

    /*public apis*/
    int32_t getId();
    uint32_t getPipe();

    int32_t update();

    int32_t getMode(drm_mode_info_t & mode);
    int32_t setMode(drm_mode_info_t & mode);

    int32_t waitVBlank(nsecs_t & timestamp);

    int32_t prePageFlip();
    int32_t pageFlip(int32_t & out_fence);

    /*TODO:should refact*/
    int32_t readCurDisplayMode(std::string & dispmode);
    int32_t writeCurDisplayAttr(std::string & dispattr __unused) { MESON_LOG_EMPTY_FUN(); return 0; }

    /*unused function only for FBDEV*/
    int32_t setDisplayFrame(display_zoom_info_t & info __unused) { return 0; }
    int32_t getHdrMetadataKeys(std::vector<drm_hdr_meatadata_t> & keys __unused) { return 0; }
    int32_t setHdrMetadata(std::map<drm_hdr_meatadata_t, float> & hdrmedata __unused) { return 0; }

    int32_t setPendingMode();

    void dump(String8 & dumpstr);

    /*internal drm package api*/
public:
    drmModeAtomicReqPtr getAtomicReq();

    int setConnectorId(uint32_t connectorId);

protected:
    int32_t loadProperties();

private:
    void blankAllPlanes();

protected:
	int mDrmFd;
    uint32_t mId;
    /*Pipe is the crtc index in kernel.
   *connector report possible crtc with shifted mask.
  */
    uint32_t mPipe;
    drmModeModeInfo mDrmMode;
    drm_mode_info mMesonMode;

    /*crtc propertys*/
    std::shared_ptr<DrmProperty> mActive;
    std::shared_ptr<DrmProperty> mModeBlobId;
    std::shared_ptr<DrmProperty> mOutFencePtr;

    drmModeAtomicReqPtr mReq;

    std::mutex mMutex;
    uint32_t mConnectorId;
    std::vector<drm_mode_info> mPendingModes;
};

#endif/*DRM_CRTC_H*/

