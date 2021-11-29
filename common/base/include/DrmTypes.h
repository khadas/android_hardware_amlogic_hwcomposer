/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRM_TYPES_H
#define DRM_TYPES_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <system/graphics-base.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* blend mode for compoistion or display.
 * The define is same as hwc2_blend_mode.
 * For hwc1, we need do convert.
 */
typedef enum {
    DRM_BLEND_MODE_INVALID = 0,
    DRM_BLEND_MODE_NONE = 1,
    DRM_BLEND_MODE_PREMULTIPLIED = 2,
    DRM_BLEND_MODE_COVERAGE = 3,
} drm_blend_mode_t;

typedef struct drm_rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} drm_rect_t;

typedef struct drm_rect_wh {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} drm_rect_wh_t;

typedef struct drm_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} drm_color_t;

typedef enum drm_fb_type {
    /*scattered buffer, can be used for rendering.*/
    DRM_FB_UNDEFINED = 0,
    /*scattered buffer, can be used for rendering.*/
    DRM_FB_RENDER = 1,
    /*contiguous buffer, can be used for scanout.*/
    DRM_FB_SCANOUT,
    /*no image data, fill with color.*/
    DRM_FB_COLOR,
    /*indicate curosr ioctl in plane.*/
    DRM_FB_CURSOR,
    /*buffer with overlay flag, no image data*/
    DRM_FB_VIDEO_OVERLAY,
    /*special handle for video, no image data*/
    DRM_FB_VIDEO_SIDEBAND,
    /* TV sideband handle */
    DRM_FB_VIDEO_SIDEBAND_TV,
    DRM_FB_VIDEO_SIDEBAND_SECOND,
    /*no image data, but with pts.*/
    DRM_FB_VIDEO_OMX_PTS,
    DRM_FB_VIDEO_OMX_PTS_SECOND,
    /*real image data, and is contiguous buf*/
    DRM_FB_VIDEO_DMABUF,
    /*fake buf for di composer output.*/
    DRM_FB_DI_COMPOSE_OUTPUT,
    /*for videotunnel*/
    DRM_FB_VIDEO_TUNNEL_SIDEBAND,
    /*uvm dma buffer*/
    DRM_FB_VIDEO_UVM_DMA,
} drm_fb_type_t;

typedef struct drm_mode_info {
    char name[DRM_DISPLAY_MODE_LEN];
    uint32_t dpiX, dpiY;
    uint32_t pixelW, pixelH;
    float refreshRate;
    int32_t groupId;
} drm_mode_info_t;

#define DRM_DISPLAY_MODE_NULL ("null")

typedef enum {
    CURSOR_PLANE = DRM_PLANE_TYPE_CURSOR,
    OSD_PLANE = DRM_PLANE_TYPE_OVERLAY,
    OSD_PLANE_PRIMARY = DRM_PLANE_TYPE_PRIMARY,

    /*VIDEO OVERLAY PLANE TYPES.*/
    LEGACY_VIDEO_PLANE = (1 << 9) | DRM_PLANE_TYPE_OVERLAY,
    LEGACY_EXT_VIDEO_PLANE =  (1 << 10) | DRM_PLANE_TYPE_OVERLAY,
    HWC_VIDEO_PLANE  =  (1 << 11) | DRM_PLANE_TYPE_OVERLAY,

    INVALID_PLANE = 0xffffffff,
} meson_plane_type_t;

/*the index of crtc*/
enum {
    DRM_PIPE_VOUT1 = 0,
    DRM_PIPE_VOUT2 = 1,

    DRM_PIPE_INVALID = 31,
};

typedef enum {
    PLANE_SHOW_LOGO = (1 << 0),
    PLANE_SUPPORT_ZORDER = (1 << 1),
    PLANE_SUPPORT_FREE_SCALE = (1 << 2),
    PLANE_PRIMARY = (1 << 3),
    PLANE_NO_PRE_BLEND = (1 << 4),
    PLANE_PRE_BLEND_1 = (1 << 5),
    PLANE_PRE_BLEND_2  = (1 << 6),
    PLANE_SUPPORT_AFBC  = (1 << 7),
    PLANE_SUPPORT_ALPHA = (1 << 8),
} drm_plane_capacity_t;

typedef enum {
    UNBLANK = 0,
    BLANK_FOR_NO_CONTENT = 1,
    BLANK_FOR_SECURE_CONTENT = 2,
} drm_plane_blank_t;

typedef enum {
    DRM_EVENT_HDMITX_HOTPLUG = 1,
    DRM_EVENT_HDMITX_HDCP,
    DRM_EVENT_VOUT1_MODE_CHANGED,
    DRM_EVENT_VOUT2_MODE_CHANGED,
    DRM_EVENT_ALL = 0xFF
} drm_display_event;

typedef uint32_t drm_connector_type_t;

typedef struct drm_hdr_capabilities {
    bool DolbyVisionSupported;
    bool HLGSupported;
    bool HDR10Supported;
    bool HDR10PlusSupported;

    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} drm_hdr_capabilities_t;

/*same as hwc2_per_frame_metadata_key_t*/
typedef enum {
    /* SMPTE ST 2084:2014.
     * Coordinates defined in CIE 1931 xy chromaticity space
     */
    DRM_DISPLAY_RED_PRIMARY_X = 0,
    DRM_DISPLAY_RED_PRIMARY_Y = 1,
    DRM_DISPLAY_GREEN_PRIMARY_X = 2,
    DRM_DISPLAY_GREEN_PRIMARY_Y = 3,
    DRM_DISPLAY_BLUE_PRIMARY_X = 4,
    DRM_DISPLAY_BLUE_PRIMARY_Y = 5,
    DRM_WHITE_POINT_X = 6,
    DRM_WHITE_POINT_Y = 7,
    /* SMPTE ST 2084:2014.
     * Units: nits
     * max as defined by ST 2048: 10,000 nits
     */
    DRM_MAX_LUMINANCE = 8,
    DRM_MIN_LUMINANCE = 9,

    /* CTA 861.3
     * Units: nits
     */
    DRM_MAX_CONTENT_LIGHT_LEVEL = 10,
    DRM_MAX_FRAME_AVERAGE_LIGHT_LEVEL = 11,
} drm_hdr_meatadata_t;

/*
display_zoom_info used to pass the calibrate display frame.
Now, it is used to pass the scale info of current vpu.
So, the crtc_display_w may be not follow the crtc size.
*/
typedef struct display_zoom_info {
    int32_t framebuffer_w;
    int32_t framebuffer_h;

    /*crtc w,h not used now.*/
    int32_t crtc_w;
    int32_t  crtc_h;

    /*scaled display axis*/
    int32_t crtc_display_x;
    int32_t crtc_display_y;
    int32_t crtc_display_w;
    int32_t crtc_display_h;
} display_zoom_info_t;

/*the invalid zorder value defination.*/
#define INVALID_ZORDER 0xFFFFFFFF

const char * drmFbTypeToString(drm_fb_type_t fbtype);
const char * drmPlaneBlankToString(drm_plane_blank_t planetype);
bool drmHdrCapsDiffer(const drm_hdr_capabilities &hdr1, const drm_hdr_capabilities &hdr2);

#endif/*DRM_TYPES_H*/
