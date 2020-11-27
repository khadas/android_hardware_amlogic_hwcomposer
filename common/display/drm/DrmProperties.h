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

#define DRM_HDMI_PROP_COLORSPACE    "Color Space"
#define DRM_HDMI_PROP_COLORDEPTH    "Color Depth"
#define DRM_HDMI_PROP_HDRCAP              "HDR DV Cap"


/*meson prop*/
#define DRM_CONNECTOR_PROP_UPDATE "UPDATE"

#endif
