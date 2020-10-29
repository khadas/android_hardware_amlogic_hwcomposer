/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef HWC_VIDEO_PLANE_H
#define HWC_VIDEO_PLANE_H

#include <sys/types.h>
#include <VideoComposerDev.h>
#include <HwDisplayPlane.h>

class HwcVideoPlane : public HwDisplayPlane {
public:
    HwcVideoPlane(int32_t drvFd, uint32_t id);
    ~HwcVideoPlane();

    const char * getName();
    uint32_t getType();
    uint32_t getCapabilities();
    int32_t getFixedZorder();
    uint32_t getPossibleCrtcs();
    int32_t setCrtcId(uint32_t crtcid __unused) { return 0; }


    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    void setAmVideoPath(int32_t id);
    int32_t setPlane(std::shared_ptr<DrmFramebuffer> fb, uint32_t zorder, int blankOp);

    void setDebugFlag(int dbgFlag);
    uint32_t getId();

    void dump(String8 & dumpstr);

protected:
    int32_t getVideodisableStatus(int & status);
    int32_t setVideodisableStatus(int status);
    int32_t getProperties();

protected:
    int32_t mDrvFd;
    uint32_t mId;
    int32_t mCapability;

    char mName[64];
    char mAmVideosPath[64];
    std::shared_ptr<VideoComposerDev> mVideoComposer;
    std::shared_ptr<DrmFramebuffer> mVideoFb;
    drm_fb_type_t mDisplayedVideoType;
    bool mBlank;
};

 #endif/*HWC_VIDEO_PLANE_H*/
