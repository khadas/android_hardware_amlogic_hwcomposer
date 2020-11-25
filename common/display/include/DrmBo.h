/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DRM_BO_H
#define DRM_BO_H

#include <DrmFramebuffer.h>
#include <misc.h>

/*Drm buffer object.
 * TODO:
 * 1. will be default display buffer to replace DrmFraembuffer.
 * 2. drm framebuffer will be a adaptive layer to convert android native handle to DrmBo.
 */

#define BUF_PLANE_NUM 4
class DrmBo {
public:
    DrmBo();
    ~DrmBo();
    int32_t import(std::shared_ptr<DrmFramebuffer> & fb);
    int32_t release();

public:
    /*dma buffer information.*/
    uint32_t fbId;
    uint32_t handles[BUF_PLANE_NUM];
    uint32_t pitches[BUF_PLANE_NUM];
    uint32_t offsets[BUF_PLANE_NUM];
    uint64_t modifiers[BUF_PLANE_NUM];

    uint32_t width;
    uint32_t height;
    uint32_t format;

    drm_rect_t srcRect;
    drm_rect_t crtcRect;
    float alpha; /*plane alpha*/
    uint32_t blend;/*blend mode*/
    uint32_t z;
    int inFence;

    /*special buffer: No support now*/
    uint32_t color;

protected:
    void refHandle(uint32_t hnd);
    void unrefHandle(uint32_t hnd);

    static std::map<uint32_t, int> mHndRefs;
};

uint32_t covertToDrmFormat(uint32_t format);
uint64_t convertToDrmModifier(int afbcMask);


#endif
