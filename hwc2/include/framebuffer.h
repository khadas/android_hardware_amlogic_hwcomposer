/*
 * Copyright (c) 2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file is modified by Amlogic, Inc. 2018.02.27.
 */

#ifndef FRAMEBUFFER_API_H_
#define FRAMEBUFFER_API_H_

#include <hardware/hardware.h>

struct private_handle_t;
struct private_module_t;

enum {
    GLES_COMPOSE_MODE = 0,
    DIRECT_COMPOSE_MODE = 1,
    GE2D_COMPOSE_MODE = 2,
};

enum {
    OSD_BLANK_OP_BIT = 0x00000001,
};

typedef struct framebuffer_info_t{
    // gralloc module.
    private_module_t *grallocModule;

    buffer_handle_t currentBuffer;

    //set by device.
    int   displayType;
    int   fbIdx;

    //information get from osd
    struct fb_var_screeninfo info;//need to fbpost
    struct fb_fix_screeninfo finfo;

    int   fd;//for fbpost use
    int   fbSize;

    float xdpi;
    float ydpi;
    float fps;
    int   flipFlags;

    // GE2D composer mode used only.
    int   yOffset;
}framebuffer_info_t;

typedef struct hwc_fb_sync_request_t{
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
    int             stride;
    unsigned int    reserve;
} hwc_fb_sync_request_t;

// Initialize the framebuffer (must keep module lock before calling
int init_frame_buffer_locked(struct framebuffer_info_t* info);

#ifndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
int init_cursor_buffer_locked(struct framebuffer_info_t* info);
int update_cursor_buffer_locked(struct framebuffer_info_t* cbinfo, int xres, int yres);
#endif

int fb_post_locked(struct framebuffer_info_t* fbinfo,buffer_handle_t buffer);
int fb_post_with_fence_locked(
        struct framebuffer_info_t* fbinfo,
        buffer_handle_t hnd,
        int in_fence);
int hwc_fb_post_with_fence_locked(
        struct framebuffer_info_t* fbinfo,
        struct hwc_fb_sync_request_t* sync_req,
        buffer_handle_t hnd);
#if PLATFORM_SDK_VERSION < 26
int hwc_old_fb_post_with_fence_locked(
        struct framebuffer_info_t* fbinfo,
        struct hwc_fb_sync_request_t* fb_sync_req,
        buffer_handle_t hnd);
int hwc_new_fb_post_with_fence_locked(
        struct framebuffer_info_t* fbinfo,
        struct hwc_fb_sync_request_t* sync_req,
        buffer_handle_t hnd);
#endif
int getOsdIdx(int display_type);
int bits_per_pixel();

//for egl to get framebuffer count
extern unsigned int get_num_fb_buffers();
extern bool osd_afbcd_enable();

#endif
