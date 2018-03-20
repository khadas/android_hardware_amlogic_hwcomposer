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
    DRM_FB_RENDER = 1 << 0,
    /*contiguous buffer, can be used for scanout.*/
    DRM_FB_SCANOUT = 1 << 1,
    /*no image data, fill with color.*/
    DRM_FB_COLOR = 1 << 2,
    /*indicate curosr ioctl in plane.*/
    DRM_FB_CURSOR =  1 << 3,
    /*no image data, but with pts.*/
    DRM_FB_VIDEO_OMX = 1 << 4,
    /*buffer with overlay flag, no image data*/
    DRM_FB_VIDEO_OVERLAY = 1 << 5,
    /*special handle for video/TV, no image data*/
    DRM_FB_VIDEO_SIDEBAND = 1 << 6,
} drm_fb_type_t;

#define DRM_DISPLAY_MODE_LEN (64)

typedef struct drm_mode_info {
    char name[DRM_DISPLAY_MODE_LEN];
    uint32_t dpiX, dpiY;
    uint32_t pixelW, pixelH;
    float refreshRate;
} drm_mode_info_t;

typedef enum {
    CURSOR_PLANE = 1<< 0,
    OSD_PLANE = 1 << 1,
    VIDEO_PLANE =  1 << 2,
} drm_plane_type_mask;

typedef enum {
    drm_event_hdmitx_hotplug = 1,
    drm_event_hdmitx_hdcp,
    drm_event_mode_changed,
    drm_event_any = 0xFF
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

#define DRM_CONNECTOR_HDMI_NAME "HDMI"
#define DRM_CONNECTOR_CVBS_NAME "CVBS"
#define DRM_CONNECTOR_PANEL_NAME "PANEL"

#endif/*DRM_TYPES_H*/
