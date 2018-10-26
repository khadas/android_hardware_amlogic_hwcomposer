/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CRTC_H
#define HW_DISPLAY_CRTC_H

#include <stdlib.h>
#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>
#include <HwDisplayPlane.h>

class HwDisplayCrtc {
public:
    HwDisplayCrtc(int drvFd, int32_t id);
    ~HwDisplayCrtc();

    int32_t setUp(std::shared_ptr<HwDisplayConnector>  connector,
                   std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> planes);

    /*load the fixed informations: displaymode list, hdr cap, etc...*/
     int32_t loadProperities();
    /*update the dynamic informations, current display mode now.*/
     int32_t update();

    /*get current display mode.*/
    int32_t getMode(drm_mode_info_t & mode);
    /*set current display mode.*/
    int32_t setMode(drm_mode_info_t & mode);

    int32_t parseDftFbSize(uint32_t & width, uint32_t & height);

    int32_t prePageFlip();
    int32_t pageFlip(int32_t &out_fence);

protected:
    int32_t getZoomInfo(display_zoom_info_t & zoomInfo);
    void closeLogoDisplay();


protected:
    int32_t mId;
    int mDrvFd;

    uint32_t mFbWidth, mFbHeight;
    bool mFirstPresent;

    display_zoom_info_t mBackupZoomInfo;

    drm_mode_info_t mCurModeInfo;
    bool mConnected;
    std::map<uint32_t, drm_mode_info_t> mModes;
    std::shared_ptr<HwDisplayConnector>  mConnector;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> mPlanes;

    std::mutex mMutex;
};

#endif/*HW_DISPLAY_CRTC_H*/
