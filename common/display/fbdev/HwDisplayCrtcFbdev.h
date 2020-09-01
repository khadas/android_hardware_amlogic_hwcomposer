/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CRTC_FBDEV_H
#define HW_DISPLAY_CRTC_FBDEV_H

#include <stdlib.h>
#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>
#include <HwDisplayPlane.h>
#include <HwDisplayCrtc.h>

class HwDisplayPlane;

class HwDisplayCrtcFbdev : public HwDisplayCrtc {
public:
    HwDisplayCrtcFbdev(int drvFd, int32_t id);
    ~HwDisplayCrtcFbdev();

    int32_t bind(std::shared_ptr<HwDisplayConnector>  connector);
    int32_t unbind();

    /*load informations: displaymode list, hdr cap, etc...*/
    int32_t getHdrMetadataKeys(std::vector<drm_hdr_meatadata_t> & keys);

    /*update the dynamic informations, current display mode now.*/
     int32_t update();
    int32_t setHdrMetadata(std::map<drm_hdr_meatadata_t, float> & hdrmedata);

    /*get current display mode.*/
    int32_t getMode(drm_mode_info_t & mode);
    /*set current display mode.*/
    int32_t setMode(drm_mode_info_t & mode);
    /* set current display attribute */
    int32_t setDisplayAttribute(std::string& dispattr);
    int32_t getDisplayAttribute(std::string& dispattr);

    int32_t waitVBlank(nsecs_t & timestamp);

    int32_t getId();
    uint32_t getPipe();

    /*Functions for compose & pageflip*/
    /*set the crtc display axis, and source axis,
    * it is used to do scale in vpu.
    */
    int32_t setDisplayFrame(display_zoom_info_t & info);
    /*
    * set if we need compose all ui layers into one display channel.
    * TODO: need pass it in a general way.
    */
    int32_t setOsdChannels(int32_t channels);

    int32_t pageFlip(int32_t & out_fence);

    int32_t readCurDisplayMode(std::string & dispmode);
    int32_t writeCurDisplayAttr(std::string & dispattr);
    void setViewPort(const drm_rect_wh_t viewPort);
    void getViewPort(drm_rect_wh_t & viewPort);

protected:
    int32_t writeCurDisplayMode(std::string & dispmode);
    void closeLogoDisplay();
    bool updateHdrMetadata(std::map<drm_hdr_meatadata_t, float> & hdrmedata);

protected:
    int32_t mId;
    uint32_t mPipe;
    int mDrvFd;
    uint32_t mOsdChannels;

    bool mFirstPresent;
    bool mConnected;

    drm_mode_info_t mCurModeInfo;
    display_zoom_info_t mScaleInfo;
    drm_rect_wh_t mViewPort;

    std::map<uint32_t, drm_mode_info_t> mModes;
    std::shared_ptr<HwDisplayConnector>  mConnector;

    void * hdrVideoInfo;
    bool mBinded;

    std::mutex mMutex;
};

#endif/*HW_DISPLAY_CRTC_FBDEV_H*/
