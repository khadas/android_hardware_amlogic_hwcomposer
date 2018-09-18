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

#include <hardware/hwcomposer_defs.h>

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

typedef struct drm_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} drm_color_t;

typedef enum drm_fb_type {
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
    /*special handle for video/TV, no image data*/
    DRM_FB_VIDEO_SIDEBAND,
    /*no image data, but with pts.*/
    DRM_FB_VIDEO_OMX_PTS,
    /*no image data, but with pts.*/
    DRM_FB_VIDEO_OMX_V4L,
} drm_fb_type_t;

#define DRM_DISPLAY_MODE_LEN (64)

typedef struct drm_mode_info {
    char name[DRM_DISPLAY_MODE_LEN];
    uint32_t dpiX, dpiY;
    uint32_t pixelW, pixelH;
    float refreshRate;
} drm_mode_info_t;

typedef enum {
    INVALID_PLANE = 0,
    OSD_PLANE,
    CURSOR_PLANE,
    LEGACY_VIDEO_PLANE,
    HWC_VIDEO_PLANE,
} drm_plane_type_t;

typedef enum {
    PLANE_SHOW_LOGO = (1 << 0),
    PLANE_SUPPORT_ZORDER = (1 << 1),
    PLANE_SUPPORT_FREE_SCALE = (1 << 2),
    PLANE_PRIMARY = (1 << 3),
    PLANE_NO_PRE_BLEND = (1 << 4),
    PLANE_PRE_BLEND_1 = (1 << 5),
    PLANE_PRE_BLEND_2  = (1 << 6),
} drm_plane_capacity_t;

typedef enum {
    UNBLANK = 0,
    BLANK_FOR_NO_CONENT = 1,
    BLANK_FOR_SECURE_CONTENT = 2,
} drm_plane_blank_t;

typedef enum {
    DRM_EVENT_HDMITX_HOTPLUG = 1,
    DRM_EVENT_HDMITX_HDCP,
    DRM_EVENT_MODE_CHANGED,
    DRM_EVENT_PRIMARY_BOOT,
    DRM_EVENT_ANY = 0xFF
} drm_display_event;

typedef enum {
    DRM_MODE_CONNECTOR_HDMI = 0,
    DRM_MODE_CONNECTOR_CVBS,
    DRM_MODE_CONNECTOR_PANEL
} drm_connector_type_t;

typedef struct drm_hdr_capabilities {
    bool DolbyVisionSupported;
    bool HDR10Supported;

    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} drm_hdr_capabilities_t;

/*Defines for old VPU(p212)
 *The gap between different plane
 *is left for variable zorder plane.
 */
enum PLANE_ZORDER {
    LEGACY_VIDEO_PLANE_FIXED_ZORDER = 0x400,
    OSD_PLANE_FIXED_ZORDER = 0x800,
    /*Cursor plane will always on top of other layers.*/
    CURSOR_PLANE_FIXED_ZORDER = 0xFFF,
    MAX_PLANE_ZORDER = 0xFFFFFFFF,
};

#define VARIABLE_PLANE_ZORDER MAX_PLANE_ZORDER

const char * drmPlaneTypeToString(drm_plane_type_t planetype);
const char * drmFbTypeToString(drm_fb_type_t fbtype);
const char * drmPlaneBlankToString(drm_plane_blank_t planetype);

#endif/*DRM_TYPES_H*/
