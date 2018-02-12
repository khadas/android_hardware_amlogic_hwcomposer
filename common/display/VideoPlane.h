/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef VIDEO_PLANE_H
#define VIDEO_PLANE_H

#include <HwDisplayPlane.h>


class VideoPlane : public HwDisplayPlane {
public:
    VideoPlane(int32_t drvFd, uint32_t id);
    ~VideoPlane();

    uint32_t getPlaneType() {return mPlaneType;}

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t blank();

    int32_t pageFlip(int32_t &outFence) {return 0;}

    void dump(String8 & dumpstr);

private:
    bool shouldUpdate(std::shared_ptr<DrmFramebuffer> &fb);
    int32_t getMute(bool& output);
    int32_t setMute(bool status);

    int32_t mBackupTransform;
    drm_rect_t mBackupDisplayFrame;

    bool mPlaneMute;
};


 #endif/*VIDEO_PLANE_H*/
