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
            typeStr = "color";
            break;
        case DRM_FB_CURSOR:
            typeStr = "cursor";
            break;
        case DRM_FB_VIDEO_OMX_PTS:
            typeStr = "omx-pts";
            break;
        case DRM_FB_VIDEO_OMX_PTS_SECOND:
            typeStr = "omx-pts-2";
            break;
        case DRM_FB_VIDEO_OVERLAY:
            typeStr = "video";
            break;
        case DRM_FB_VIDEO_SIDEBAND:
            typeStr = "sideband";
            break;
        case DRM_FB_VIDEO_SIDEBAND_TV:
            typeStr = "sideband-tv";
            break;
        case DRM_FB_VIDEO_SIDEBAND_SECOND:
            typeStr = "sideband-2";
            break;
        case DRM_FB_VIDEO_DMABUF:
            typeStr = "video-dma";
            break;
        case DRM_FB_VIDEO_TUNNEL_SIDEBAND:
            typeStr = "vt-sideband";
            break;
        case DRM_FB_VIDEO_UVM_DMA:
            typeStr = "uvm-dma";
            break;
        default:
            typeStr = "unknown";
            break;
    }
    return typeStr;
}

const char * drmPlaneBlankToString(drm_plane_blank_t blankType) {
    const char * typeStr;
    switch (blankType) {
        case UNBLANK:
            typeStr = "UnBlank";
            break;
        case BLANK_FOR_NO_CONTENT:
            typeStr = "Blank";
            break;
        case BLANK_FOR_SECURE_CONTENT:
            typeStr = "Secure-Blank";
            break;
        default:
            typeStr = "Unknown";
            break;
    }

    return typeStr;
}

/* whether the hdr/DV capabilities of hdr1 different from that of hdr2 */
bool drmHdrCapsDiffer(const drm_hdr_capabilities &hdr1, const drm_hdr_capabilities &hdr2) {
    bool differ = false;
    if (hdr1.DolbyVisionSupported != hdr2.DolbyVisionSupported ) {
        differ = true;
    } else if (hdr1.HLGSupported != hdr2.HLGSupported) {
        differ = true;
    } else if(hdr1.HDR10Supported != hdr2.HDR10Supported) {
        differ = true;
    } else if (hdr1.HDR10PlusSupported != hdr2.HDR10PlusSupported) {
        differ = true;
    }

    return differ;
}
