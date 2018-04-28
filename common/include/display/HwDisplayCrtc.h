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

typedef struct display_flip_info_t {
    int           out_fen_fd;
    unsigned int  background_w;
    unsigned int  background_h;
    unsigned int  fullScreen_w;
    unsigned int  fullScreen_h;
    unsigned int  curPosition_x;
    unsigned int  curPosition_y;
    unsigned int  curPosition_w;
    unsigned int  curPosition_h;
} display_flip_info_t;

class HwDisplayCrtc {
public:
    HwDisplayCrtc(int drvFd, int32_t id);
    ~HwDisplayCrtc();

    int32_t setUp(std::shared_ptr<HwDisplayConnector>  connector,
                   std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> planes);
#ifdef HWC_MANAGE_DISPLAY_MODE
    int32_t setMode(drm_mode_info_t &mode);
#else
    int32_t updateMode(std::string & displayMode);
#endif

    int32_t getModeId();

    int32_t parseDftFbSize(uint32_t & width, uint32_t & height);

    int32_t prePageFlip();

    int32_t pageFlip(int32_t &out_fence);

protected:
    int32_t getZoomInfo(display_zoom_info_t & zoomInfo);
    int32_t updateDisplayInfo();

    int32_t mId;
    int mDrvFd;
    std::string mCurMode;
    uint32_t mFbWidth, mFbHeight;
    int mBackupZoomPos[4];

    display_flip_info_t mDisplayInfo;
    display_flip_info_t mBackupDisplayInfo;

    std::shared_ptr<HwDisplayConnector>  mConnector;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> mPlanes;
};

#endif/*HW_DISPLAY_CRTC_H*/
