/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef OSD_PLANE_H
#define OSD_PLANE_H

#ifdef HWC_ENABLE_DRM_BACKEND
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#endif
#include <MesonLog.h>
#include <misc.h>
#include "AmFramebuffer.h"
#include "HwDisplayPlaneFbdev.h"

class OsdPlane : public HwDisplayPlaneFbdev {
public:
    OsdPlane(int32_t drvFd, uint32_t id);
    ~OsdPlane();

    const char * getName();
    uint32_t getType();
    uint32_t getCapabilities();
    int32_t getFixedZorder();
    uint32_t getPossibleCrtcs();
    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb, uint32_t zorder, int blankOp);

    void dump(String8 & dumpstr);

protected:
    int32_t getProperties();
    void createPatternFb();
#ifdef HWC_ENABLE_DRM_BACKEND
    uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
#endif
private:
    bool mBlank;
    uint32_t mPossibleCrtcs;
    osd_plane_info_t mPlaneInfo;
#ifdef HWC_ENABLE_DRM_BACKEND
    /*drm mode resource*/
    drmModeResPtr mRes_mode;
    /*drm plane*/
    drmModePlaneResPtr mRes_plane;
    drmModePlanePtr mPtr_plane;
    drmModeObjectPropertiesPtr mProperties_plane;
    drmModePropertyRes **mProperty_plane;
    /*drm crtc*/
    drmModeCrtcPtr mPtr_crtc;
    drmModeObjectPropertiesPtr mProperties_crtc;
    drmModePropertyRes **mProperty_crtc;
    /*drm encoder*/
    drmModeEncoderPtr mPtr_encoder;
    /*drm connector*/
    drmModeConnectorPtr mPtr_connector;
    drmModeObjectPropertiesPtr mProperties_connector;
    drmModePropertyRes **mProperty_connector;
    /*drm framebuffer*/
    //drmModeFBPtr mPtr_fb;
#endif
    std::shared_ptr<DrmFramebuffer> mDrmFb;
    std::shared_ptr<DrmFramebuffer> mPatternFb;

    char mName[64];
};


 #endif/*OSD_PLANE_H*/
