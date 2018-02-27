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

#define DISPLAY_LOGO_INDEX              "/sys/module/fb/parameters/osd_logo_index"
#define DISPLAY_FB0_FREESCALE_SWTICH    "/sys/class/graphics/fb0/free_scale_switch"

/* FBIO */
#define FBIOPUT_OSD_SYNC_RENDER_ADD  0x4519
#define FBIOPUT_OSD_HWC_ENABLE       0x451a
#define FBIOPUT_OSD_SYNC_BLANK       0x451c

enum {
    GLES_COMPOSE_MODE = 0,
    DIRECT_COMPOSE_MODE = 1,
    GE2D_COMPOSE_MODE = 2,
};

enum {
    OSD_BLANK_OP_BIT = 0x00000001,
};

typedef enum {
    OSD_AFBC_EN             = (1 << 31),
    OSD_TILED_HEADER_EN     = (1 << 18),
    OSD_SUPER_BLOCK_ASPECT  = (1 << 16),
    OSD_BLOCK_SPLIT         = (1 << 9),
    OSD_YUV_TRANSFORM       = (1 << 8),
} afbc_format_mask;

enum {
    OSD_SYNC_REQUEST_MAGIC            = 0x54376812,
    OSD_SYNC_REQUEST_RENDER_MAGIC_V1  = 0x55386816,
    OSD_SYNC_REQUEST_RENDER_MAGIC_V2  = 0x55386817,
};

typedef struct osd_plane_info_t {
    int             magic;
    int             len;
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
    int             pixel_stride;
    int             afbc_inter_format;
    unsigned int    zorder;
    unsigned int    blend_mode;
    int             plane_alpha;
    int             reserve;
} osd_plane_info_t;

class OsdPlane : public HwDisplayPlane {
public:
    OsdPlane(int32_t drvFd, uint32_t id);
    ~OsdPlane();

    uint32_t getPlaneType() {return mPlaneType;}

    int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb);
    int32_t getCapabilities();

    int32_t blank(bool blank);

    // int32_t pageFlip(int32_t &outFence);

    void dump(String8 & dumpstr);

protected:
    int32_t getProperties();

    void dumpPlaneInfo();

    int translateInternalFormat(uint64_t internalFormat);

    String8 compositionTypeToString();

private:
    bool mFirstPresentDisplay;
    bool mOsdPlaneBlank;

    osd_plane_info_t mPlaneInfo;
    std::shared_ptr<DrmFramebuffer> mDrmFb;
};


 #endif/*OSD_PLANE_H*/
