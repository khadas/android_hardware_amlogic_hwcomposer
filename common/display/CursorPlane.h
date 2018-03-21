/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef CURSOR_PLANE_H
#define CURSOR_PLANE_H

#include <HwDisplayPlane.h>
#include <MesonLog.h>
#include <misc.h>
#include <linux/ioctl.h>
#include <linux/fb.h>

#define BPP_2 2
#define BPP_3 3
#define BPP_4 4
#define BYTE_ALIGN_32 32
#define HWC_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

/* FBIO */
#define FBIOPUT_OSD_REVERSE          0x4515
#define FBIOPUT_OSD_SYNC_BLANK       0x451c
#define FB_IOC_MAGIC   'O'
#define FBIOPUT_OSD_CURSOR     _IOWR(FB_IOC_MAGIC, 0x0,  struct fb_cursor)

#define DISPLAY_FB1_SCALE_AXIS          "/sys/class/graphics/fb1/scale_axis"
#define DISPLAY_FB1_SCALE               "/sys/class/graphics/fb1/scale"

static inline size_t round_up_to_page_size(size_t x)
{
	return (x + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
}

typedef struct cursor_plane_info_t {
    int             fbSize;
    int             transform;
    unsigned int    dst_x, dst_y;
    unsigned int    zorder;

    //buffer handle
    int             format;
    int             shared_fd;
    int             stride;
    int             buf_w, buf_h;

    //information get from osd
    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
} cursor_plane_info_t;

class CursorPlane : public HwDisplayPlane {
public:
    CursorPlane(int32_t drvFd, uint32_t id);
    ~CursorPlane();

    const char * getName();
    uint32_t getPlaneType() {return mPlaneType;}
    int32_t getCapabilities() {return 0x0;};
    int32_t setPlane(std::shared_ptr<DrmFramebuffer> & fb);
    int32_t updateOsdPosition(const char * axis);
    int32_t blank(bool blank);

    // int32_t pageFlip(int32_t &outFence);
    void dump(String8 & dumpstr);

protected:
    void dumpPlaneInfo();

private:
    int32_t setCursorPosition(int32_t x, int32_t y);
    int32_t updatePlaneInfo(int xres, int yres);
    int32_t updateCursorBuffer();

    int32_t mLastTransform;
    bool mCursorPlaneBlank;
    cursor_plane_info_t mPlaneInfo;
    std::shared_ptr<DrmFramebuffer> mDrmFb;

    char mName[64];
};

 #endif/*CURSOR_PLANE_H*/
