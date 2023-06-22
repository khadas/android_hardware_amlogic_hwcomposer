/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <DrmTypes.h>
#include <strings.h>

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

struct conn_type_name_list {
    uint32_t type;
    const char *name;
};

static struct conn_type_name_list drm_connector_enum_list[] = {
    {DRM_MODE_CONNECTOR_HDMIA,          "HDMI-A"},
    {DRM_MODE_CONNECTOR_TV,             "CVBS"},
    {DRM_MODE_CONNECTOR_LVDS,           "LVDS"},
    {DRM_MODE_CONNECTOR_MESON_LVDS_A,   "LVDS-A"},
    {DRM_MODE_CONNECTOR_MESON_LVDS_B,   "LVDS-B"},
    {DRM_MODE_CONNECTOR_MESON_LVDS_C,   "LVDS-C"},
    {DRM_MODE_CONNECTOR_MESON_VBYONE_A, "VBYONE-A"},
    {DRM_MODE_CONNECTOR_MESON_VBYONE_B, "VBYONE-B"},
    {DRM_MODE_CONNECTOR_MESON_MIPI_A,   "MIPI-A"},
    {DRM_MODE_CONNECTOR_MESON_MIPI_B,   "MIPI-B"},
    {DRM_MODE_CONNECTOR_MESON_EDP_A,    "EDP-A"},
    {DRM_MODE_CONNECTOR_MESON_EDP_B,    "EDP-B"},
    {DRM_MODE_CONNECTOR_VIRTUAL,    "VIRTUAL"},

    {LEGACY_NON_DRM_CONNECTOR_PANEL,    "panel"},
    {DRM_MODE_CONNECTOR_Unknown,        "Unknown"}
};

const char * drmConnTypeToString(
    drm_connector_type_t conn_type) {
    const char*name = NULL;
    int i = 0;
    do {
        if (drm_connector_enum_list[i].type == conn_type) {
            name = drm_connector_enum_list[i].name;
            break;
        } else {
            i++;
        }
    } while (drm_connector_enum_list[i].type != DRM_MODE_CONNECTOR_Unknown);

    return name;
}

drm_connector_type_t drmStringToConnType(
    const char *name) {
    drm_connector_type_t type = DRM_MODE_CONNECTOR_INVALID_TYPE;
    int i = 0;
    do {
        if (strcasecmp(name, drm_connector_enum_list[i].name) == 0) {
            type = drm_connector_enum_list[i].type;
            break;
        } else {
            i++;
        }
    } while (drm_connector_enum_list[i].type != DRM_MODE_CONNECTOR_Unknown);

    return type;
}

const char *hdrConversionTypeToString(int32_t type) {
    const char * typeStr;
    switch (type) {
        case DRM_INVALID:
            typeStr = "SDR";
            break;
        case DRM_DOLBY_VISION:
            typeStr = "DV";
            break;
        case DRM_HDR10:
            typeStr = "HDR10";
            break;
        case DRM_HLG:
            typeStr = "HLG";
            break;
        case DRM_HDR10_PLUS:
            typeStr = "HDR10+";
            break;
        case DRM_DOLBY_VISION_4K30:
            typeStr = "DV4k30";
            break;
        default:
            typeStr = "INVALID";
            break;
    }
    return typeStr;
}
