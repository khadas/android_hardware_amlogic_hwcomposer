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


#include "DrmPlane.h"
#include "DrmConnector.h"

class DrmCrtc : public HwDisplayCrtc {
public:
    DrmCrtc();
    ~DrmCrtc();

    int32_t bind(std::shared_ptr<HwDisplayConnector>  connector,
                   std::vector<std::shared_ptr<HwDisplayPlane>> planes);
    int32_t unbind();

    int32_t loadProperities();
    int32_t update();

    int32_t getId();
    int32_t getMode(drm_mode_info_t & mode);
    int32_t setMode(drm_mode_info_t & mode);

    int32_t waitVBlank(nsecs_t & timestamp);
    int32_t pageFlip(int32_t & out_fence);

    void setViewPort(const drm_rect_wh_t viewPort);
    void getViewPort(drm_rect_wh_t & viewPort);

    /*TODO:should refact*/
    int32_t readCurDisplayMode(std::string & dispmode __unused) { MESON_LOG_EMPTY_FUN(); return 0; }
    int32_t setDisplayAttribute(std::string& dispattr __unused) { MESON_LOG_EMPTY_FUN(); return 0; }
    int32_t getDisplayAttribute(std::string& dispattr __unused) { MESON_LOG_EMPTY_FUN(); return 0; }
    int32_t writeCurDisplayAttr(std::string & dispattr __unused) { MESON_LOG_EMPTY_FUN(); return 0; }

    /*unused function only for FBDEV*/
    int32_t setDisplayFrame(display_zoom_info_t & info __unused) { return 0; }
    int32_t setOsdChannels(int32_t channels __unused) { return 0; }
    int32_t getHdrMetadataKeys(std::vector<drm_hdr_meatadata_t> & keys __unused) { return 0; }
    int32_t setHdrMetadata(std::map<drm_hdr_meatadata_t, float> & hdrmedata __unused) { return 0; }

protected:



protected:
    std::mutex mMutex;
    drm_rect_wh_t mViewPort;

};

#endif/*DRM_CRTC_H*/

