/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRM_PROPERTIES_H
#define DRM_PROPERTIES_H

#define DRM_MODE_PROP_CRTCID "CRTC_ID"

/*CRTC PROPS*/
#define DRM_CRTC_PROP_ACTIVE  "ACTIVE"
#define DRM_CRTC_PROP_MODEID "MODE_ID"
#define DRM_CRTC_PROP_OUTFENCEPTR  "OUT_FENCE_PTR"
#define DRM_CRTC_PROP_VRR_ENABLED  "VRR_ENABLED"
#define DRM_CRTC_PROP_VIDEO_PIXEL_FORMAT "video_pixelformat"
#define DRM_CRTC_PROP_OSD_PIXEL_FORMAT "osd_pixelformat"
#define DRM_CRTC_PROP_HDR_CONVERSION_CAP  "hdr_conversion_cap"
#define DRM_CRTC_PROP_BRR_UPDATE  "brr_update"

/*CTM*/
#define DRM_CRTC_PROP_DEGAMMA  "DEGAMMA_LUT"
#define DRM_CRTC_PROP_CTM  "CTM"
#define DRM_CRTC_PROP_GAMMA  "GAMMA_LUT"

/*PLANE PROPS*/
#define DRM_PLANE_PROP_FBID  "FB_ID"
#define DRM_PLANE_PROP_INFENCE "IN_FENCE_FD"
#define DRM_PLANE_PROP_SRCX  "SRC_X"
#define DRM_PLANE_PROP_SRCY  "SRC_Y"
#define DRM_PLANE_PROP_SRCW  "SRC_W"
#define DRM_PLANE_PROP_SRCH  "SRC_H"
#define DRM_PLANE_PROP_CRTCID DRM_MODE_PROP_CRTCID
#define DRM_PLANE_PROP_CRTCX  "CRTC_X"
#define DRM_PLANE_PROP_CRTCY  "CRTC_Y"
#define DRM_PLANE_PROP_CRTCW  "CRTC_W"
#define DRM_PLANE_PROP_CRTCH  "CRTC_H"
#define DRM_PLANE_PROP_Z "zpos"
#define DRM_PLANE_PROP_ALPHA "alpha"
#define DRM_PLANE_PROR_IN_FORMATS "IN_FORMATS"
#define DRM_PLANE_PROP_MAX_FB_SIZE "max_fb_size"
#define DRM_PLANE_PROP_UNSUPPORT_NONAFBC "unsupport_nonafbc"

#define DRM_PLANE_PROP_BLENDMODE "pixel blend mode"
#define DRM_PLANE_PROP_BLENDMODE_NONE "None"
#define DRM_PLANE_PROP_BLENDMODE_PREMULTI "Pre-multiplied"
#define DRM_PLANE_PROP_BLENDMODE_COVERAGE "Coverage"

#define DRM_PLANE_PROP_TYPE  "type"

/*not used*/
#define DRM_PLANE_PROP_ROTATION "rotation"

/*HDMI PROPRS*/
#define DRM_CONNECTOR_PROP_CRTCID  DRM_MODE_PROP_CRTCID
#define DRM_CONNECTOR_PROP_EDID "EDID"
#define DRM_CONNECTOR_PROP_VRRCAP  "vrr_capable"
#define DRM_CONNECTOR_PROP_HDR_PRIORITY "hdr_priority"

#define DRM_HDMI_PROP_COLORSPACE    "color_space"
#define DRM_HDMI_PROP_COLORDEPTH    "color_depth"
#define DRM_HDMI_PROP_HDRCAP              "HDR DV Cap"
#define DRM_HDMI_PROP_HDR_STATUS     "hdmi_hdr_status"
#define DRM_HDMI_PROP_CONTENT_TYPE   "content type"
#define DRM_HDMI_PROP_HDMI_AV_MUTE   "MESON_DRM_HDMITX_PROP_AVMUTE"
#define DRM_HDMI_PROP_DV_CAP         "dv_cap"
#define DRM_HDMI_PROP_READY          "ready"

/*meson prop*/
#define DRM_CONNECTOR_PROP_UPDATE "UPDATE"
#define DRM_CONNECTOR_PROP_MESON_TYPE "meson.connector_type"

#define DRM_PLANE_PROP_OCCUPY "meson.plane.occupied"

#define DRM_PLANE_PROP_SECURE "SEC_EN"
#define DRM_PLANE_PROP_REVERSE "rotation_reflect"
#define DRM_PLANE_PROP_REVERSE_NONE "reflect-0"
#define DRM_PLANE_PROP_REVERSE_X "reflect-x"
#define DRM_PLANE_PROP_REVERSE_Y "reflect-y"
#define DRM_PLANE_PROP_REVERSE_ALL "reflect-all"

#endif
