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

#include <HwDisplayPlane.h>

typedef struct osd_plane_info_t {
    unsigned int    xoffset;
    unsigned int    yoffset;
    int             in_fen_fd;
    int             out_fen_fd;
    int             width;
    int             height;
    int             format;
    int             shared_fd;
    unsigned int    op;
    unsigned int    type; /*direct render or ge2d*/
    unsigned int    dst_x;
    unsigned int    dst_y;
    unsigned int    dst_w;
    unsigned int    dst_h;
    int             byte_stride;
    int             stride;
    unsigned int    zorder;
    unsigned int    blend_mode;
    unsigned int    plane_alpha;
    unsigned int    reserve;
} osd_plane_info_t;

class OsdPlane : public HwDisplayPlane {
public:
    OsdPlane(int32_t drvFd, uint32_t id);
    ~OsdPlane();

    uint32_t getPlaneType() {return mPlaneType;}

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t blank();

    int32_t pageFlip(int32_t &outFence);

    void dump(String8 & dumpstr);

protected:
    int32_t getProperties();

    void dumpPlaneInfo();

private:
    int32_t mPriorFrameRetireFd;

    osd_plane_info_t mPlaneInfo;
    // std::shared_ptr<DrmFence> mRetireFence;
};


 #endif/*OSD_PLANE_H*/
