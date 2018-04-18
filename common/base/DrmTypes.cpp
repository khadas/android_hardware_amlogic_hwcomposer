/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <DrmTypes.h>

const char * drmFbTypeToString(drm_fb_type_t fbtype) {
    const char * typeStr;
    switch (fbtype) {
        case DRM_FB_RENDER:
            typeStr = "render";
            break;
        case DRM_FB_SCANOUT:
            typeStr = "scanout";
            break;
        case DRM_FB_COLOR:
            typeStr = "solidcolor";
            break;
        case DRM_FB_CURSOR:
            typeStr = "cursor";
            break;
        case DRM_FB_VIDEO_OMX_PTS:
            typeStr = "omx-pts";
            break;
        case DRM_FB_VIDEO_OVERLAY:
            typeStr = "video";
            break;
        case DRM_FB_VIDEO_SIDEBAND:
            typeStr = "sideband";
            break;
        default:
            typeStr = "unknown";
            break;
    }
    return typeStr;
}

const char * drmPlaneTypeToString(drm_plane_type_t planetype) {
    const char * typeStr;
    switch (planetype) {
        case INVALID_PLANE:
            return "NULL";
        case OSD_PLANE:
            return "OSD";
        case CURSOR_PLANE:
            return "CURSOR";
        case LEGACY_VIDEO_PLANE:
            return "AmVideo";
        case HWC_VIDEO_PLANE:
            return "HwcVideo";
        default:
            return "UNKNOWN";
    }
}
