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
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* blend mode for composition or display.
 * The define is same as hwc2_blend_mode.
 * For hwc1, we need do convert.
 */
typedef enum {
    DRM_BLEND_MODE_INVALID = 0,
    DRM_BLEND_MODE_NONE = 1,
    DRM_BLEND_MODE_PREMULTIPLIED = 2,
    DRM_BLEND_MODE_COVERAGE = 3,
} drm_blend_mode_t;

typedef enum {
    DRM_REVERSE_MODE_NONE = 0,
    DRM_REVERSE_MODE_X = 1,
    DRM_REVERSE_MODE_Y = 2,
    DRM_REVERSE_MODE_ALL = 3,
} drm_reverse_mode_t;

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
typedef enum _ENUM_DRM_HDR_TYPE {
    DRM_HDR10PLUS      = 0,
    DRM_DOLBYVISION_STD,
    DRM_DOLBYVISION_LL,
    DRM_HDR10_ST2084,
    DRM_HDR10_TRADITIONAL,
    DRM_HDR_HLG,
    DRM_SDR
} ENUM_DRM_HDR_TYPE;

typedef enum _ENUM_HDMI_COLOR_SPACE {
   HDMI_COLOR_SPACE_RGB      = 0,
   HDMI_COLOR_SPACE_422,
   HDMI_COLOR_SPACE_444,
   HDMI_COLOR_SPACE_420,
   HDMI_COLOR_SPACE_RESERVED,
} ENUM_HDMI_COLOR_SPACE;

typedef enum _ENUM_HDMI_CONTENT_TYPE {
   HDMI_CONTENT_TYPE_NO_DATA,
   HDMI_CONTENT_TYPE_GRAPHIC,
   HDMI_CONTENT_TYPE_PHOTO,
   HDMI_CONTENT_TYPE_CINEMA,
   HDMI_CONTENT_TYPE_GAME,
   HDMI_CONTENT_TYPE_MAX
} ENUM_HDMI_CONTENT_TYPE;


typedef enum _ENUM_DRM_HDR_CONVERSION_TYPE {
    /*The value INVALID
     * is used to depict SDR outputType.*/
    DRM_INVALID       = 0,
    DRM_DOLBY_VISION,
    DRM_HDR10,
    DRM_HLG,
    DRM_HDR10_PLUS,
    DRM_DOLBY_VISION_4K30,
} ENUM_DRM_HDR_CONVERSION_TYPE;

typedef struct drm_hdr_conversion_capability {
    uint32_t sourceType;
    uint32_t outputType;
    bool addsLatency;
} drm_hdr_conversion_capability_t;

typedef enum drm_fb_type {
    /*scattered buffer, can be used for rendering.*/
    DRM_FB_UNDEFINED = 0,
    /*scattered buffer, can be used for rendering.*/
    DRM_FB_RENDER = 1,
    /*contiguous buffer, can be used for scanout.*/
    DRM_FB_SCANOUT,
    /*no image data, fill with color.*/
    DRM_FB_COLOR,
    /*indicate cursor ioctl in plane.*/
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
    /* display decoration */
    DRM_FB_DECORATION,
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
    DRM_PIPE_VOUT3 = 2,

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
    PLANE_SUPPORT_1 = (1 << 9),
    PLANE_SUPPORT_2 = (1 << 10),
    PLANE_SUPPORT_3 = (1 << 11),
    PLANE_SUPPORT_4K = (1 << 12),
    PLANE_SUPPORT_8K = (1 << 13),
    PLANE_SUPPORT_MOSAIC = (1 << 14),
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
    DRM_EVENT_VOUT3_MODE_CHANGED,
    DRM_EVENT_HDMITX_SUSPEND_RESUME,
    DRM_EVENT_ALL = 0xFF
} drm_display_event;

typedef enum {
    DRM_EVENT_DISABLE = 0,
    DRM_EVENT_ENABLE,
    DRM_EVENT_SUSPEND,
    DRM_EVENT_RESUME,
    DRM_EVENT_INVALID,
} drm_event_state;

/*From kernel/include/drm/amlogic/meson_connector_dev.h*/
/*amlogic extend connector type: for original type is not enough.
 *start from: 0xff,
 *extend connector: 0x100 ~ 0xfff,
 *legacy panel type for non-drm: 0x1000 ~
 */
#define DRM_MODE_MESON_CONNECTOR_PANEL_START 0xff
#define DRM_MODE_MESON_CONNECTOR_PANEL_END   0xfff

enum {
	DRM_MODE_CONNECTOR_MESON_LVDS_A = 0x100,
	DRM_MODE_CONNECTOR_MESON_LVDS_B = 0x101,
	DRM_MODE_CONNECTOR_MESON_LVDS_C = 0x102,

	DRM_MODE_CONNECTOR_MESON_VBYONE_A = 0x110,
	DRM_MODE_CONNECTOR_MESON_VBYONE_B = 0x111,

	DRM_MODE_CONNECTOR_MESON_MIPI_A = 0x120,
	DRM_MODE_CONNECTOR_MESON_MIPI_B = 0x121,

	DRM_MODE_CONNECTOR_MESON_EDP_A = 0x130,
	DRM_MODE_CONNECTOR_MESON_EDP_B = 0x131,
};

#define LEGACY_NON_DRM_CONNECTOR_START 0x1000
enum {
    LEGACY_NON_DRM_CONNECTOR_PANEL = 0x1001,
};

#define DRM_MODE_CONNECTOR_INVALID_TYPE 0xffffffff

typedef uint32_t drm_connector_type_t;

typedef struct drm_hdr_capabilities {
    bool DolbyVisionSupported;
    bool HLGSupported;
    bool HDR10Supported;
    bool HDR10PlusSupported;
    bool DOLBY_VISION_4K30_Supported;

    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} drm_hdr_capabilities_t;

typedef enum {
    DRM_UNSPECIFIED = 0,
    DRM_RGBA_8888 = 1,
    DRM_RGBX_8888 = 2,
    DRM_RGB_888 = 3,
    DRM_RGB_565 = 4,
    DRM_BGRA_8888 = 5,
    DRM_YCBCR_422_SP = 16,
    DRM_YCRCB_420_SP = 17,
    DRM_YCBCR_422_I = 20,
    DRM_RGBA_FP16 = 22,
    DRM_RAW16 = 32,
    DRM_BLOB = 33,
    DRM_IMPLEMENTATION_DEFINED = 34,
    DRM_YCBCR_420_888 = 35,
    DRM_RAW_OPAQUE = 36,
    DRM_RAW10 = 37,
    DRM_RAW12 = 38,
    DRM_RGBA_1010102 = 43,
    DRM_Y8 = 538982489,
    DRM_Y16 = 540422489,
    DRM_YV12 = 842094169,
    DRM_DEPTH_16 = 48,
    DRM_DEPTH_24 = 49,
    DRM_DEPTH_24_STENCIL_8 = 50,
    DRM_DEPTH_32F = 51,
    DRM_DEPTH_32F_STENCIL_8 = 52,
    DRM_STENCIL_8 = 53,
    DRM_YCBCR_P010 = 54,
    DRM_HSV_888 = 55,
    DRM_R_8 = 56,
    DRM_R_16_UINT = 57,
    DRM_RG_1616_UINT = 58,
    DRM_RGBA_10101010 = 59,
} drm_pixel_format_t;

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
} drm_hdr_metadata_t;

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

typedef struct drm_meson_present_fence {
        uint32_t crtc_idx;
        uint32_t fd;
} drm_meson_present_fence_t;

/*the invalid zorder value definition.*/
#define INVALID_ZORDER 0xFFFFFFFF
#define FB_SIZE_1080P_W 1920
#define FB_SIZE_1080P_H 1080
#define FB_SIZE_4K_W 3840
#define FB_SIZE_4K_H 2160
#define FB_SIZE_8K_W 7680
#define FB_SIZE_8K_H 4320

#define RENDER_TARGET 1
#define RENDER_TEXTURE 2

#define DISPLAY_DEFAULT 0
#define DISPLAY_VIDEOTUNNEL 1
#define DISPLAY_WHITEBOARD 2

const char * drmFbTypeToString(drm_fb_type_t fbtype);
const char * drmPlaneBlankToString(drm_plane_blank_t planetype);
bool drmHdrCapsDiffer(const drm_hdr_capabilities &hdr1, const drm_hdr_capabilities &hdr2);

const char * drmConnTypeToString(drm_connector_type_t conn_type);
drm_connector_type_t drmStringToConnType(const char *name);

const char *hdrConversionTypeToString(int32_t type);

#endif/*DRM_TYPES_H*/
