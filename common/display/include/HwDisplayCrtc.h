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

class HwDisplayPlane;

class HwDisplayCrtc {
public:
    HwDisplayCrtc() { }
    virtual ~HwDisplayCrtc() {}

    virtual int32_t getId() = 0;
    /*pipe is index of crtc*/
    virtual uint32_t getPipe() = 0;

    /*update informations, current display mode now.*/
    virtual int32_t update() = 0;

    /*for hdr metadata.*/
    virtual int32_t getHdrMetadataKeys(std::vector<drm_hdr_meatadata_t> & keys) = 0;
    virtual int32_t setHdrMetadata(std::map<drm_hdr_meatadata_t, float> & hdrmedata) = 0;

    /*get/set current display mode.*/
    virtual int32_t getMode(drm_mode_info_t & mode) = 0;
    virtual int32_t setMode(drm_mode_info_t & mode) = 0;

    virtual int32_t waitVBlank(nsecs_t & timestamp) = 0;

    /*Functions for compose & pageflip*/
    /*set the crtc display axis, and source axis,
    * it is used to do scale in vpu.
    */
    virtual int32_t setDisplayFrame(display_zoom_info_t & info) = 0;

    virtual int32_t prePageFlip();
    virtual int32_t pageFlip(int32_t & out_fence) = 0;

    virtual int32_t readCurDisplayMode(std::string & dispmode) = 0;
    virtual int32_t writeCurDisplayAttr(std::string & dispattr) = 0;
};

#endif/*HW_DISPLAY_CRTC_H*/
