/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <HwDisplayManager.h>
#include "HwDisplayConnector.h"
#include "HwDisplayCrtc.h"
#include "DisplayAdapterLocal.h"

#include "misc.h"
#include "MesonHwc2.h"
#include "HwcConfig.h"
#include "MesonLog.h"
#include <sys/utsname.h>

#define SYSFS_DISPLAY_MODE              "/sys/class/display/mode"

#define GET_CRTC_BY_CONNECTOR(type) \
        std::shared_ptr<HwDisplayConnector> connector; \
        HwDisplayCrtc * crtc = NULL; \
        getHwDisplayManager()->getConnector(connector, type); \
        if (connector) { \
            crtc = connector->getCrtc(); \
        } \

namespace meson{

using namespace std;

using ConnectorType = DisplayAdapter::ConnectorType;
using BackendType = DisplayAdapter::BackendType;


#define SYS_FS_BUFFER_LEN_MAX 4096

bool update_sys_node(DisplayAttributeInfo& info, const string& in, string& out, UpdateType type) {
    bool ret = false;
    char buffer[SYS_FS_BUFFER_LEN_MAX]={0};
    const char* node = info.sysfs_node;
    bool is_read_only = info.is_read_only;
    bool is_write_only = info.is_write_only;

    switch (type) {
        case UT_SET_VALUE:
            if (is_read_only)
                break;
            info.new_value = in;
            if (0 == sysfs_set_string(node, in.c_str())) {
                info.current_value = in;
                ret = true;
            }
            break;
        case UT_GET_VALUE:
            if (is_write_only)
                break;
            if (0 == sysfs_get_string(node, buffer, SYS_FS_BUFFER_LEN_MAX)) {
                out = buffer;
                info.current_value = buffer;
                ret = true;
            }
            break;
        default:
            break;
    }
    return ret;
}

void DisplayTypeConv(drm_connector_type_t& type, ConnectorType displayType) {
    switch (displayType) {
        case DisplayAdapter::CONN_TYPE_HDMI:
            type = DRM_MODE_CONNECTOR_HDMIA;
            break;
        case DisplayAdapter::CONN_TYPE_PANEL:
            type = DRM_MODE_CONNECTOR_LVDS;
            break;
        case DisplayAdapter::CONN_TYPE_DUMMY:
            type = DRM_MODE_CONNECTOR_VIRTUAL;
            break;
        case DisplayAdapter::CONN_TYPE_CVBS:
            type = DRM_MODE_CONNECTOR_TV;
            break;
    }
}

void DisplayModeConv(DisplayModeInfo& mode, drm_mode_info_t& mode_in) {
    mode.name = mode_in.name;
    mode.dpiX = mode_in.dpiX;
    mode.dpiY = mode_in.dpiY;
    mode.pixelW = mode_in.pixelW;
    mode.pixelH = mode_in.pixelH;
    mode.refreshRate = mode_in.refreshRate;
}


DisplayAdapterLocal::DisplayAdapterLocal() {
    struct utsname buf;
    int major = 0;
    int minor = 0;
    if (0 == uname(&buf)) {
        if (sscanf(buf.release, "%d.%d", &major, &minor) != 2) {
            major = 0;
        }
    }
    if (major == 0) {
        MESON_LOGV("Can't determine kernel version for access sysfs!");
    }

#define DA_DEFINE(ID, INIT_VAL, UPDATE_FUN)  \
    display_attrs[DA_##ID] = { .name = DISPLAY_##ID, .attr_id = DA_##ID, .current_value = INIT_VAL, .new_value = INIT_VAL, .status_flags = 0, .update_fun = UPDATE_FUN, .sysfs_node = NULL, .is_read_only = false, .is_write_only = false }

    DA_DEFINE(DOLBY_VISION_CAP, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_ENABLE, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_MODE, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_STATUS, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_POLICY, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_LL_POLICY, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_HDR_10_POLICY, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_GRAPHICS_PRIORITY, "0", update_sys_node);
    DA_DEFINE(HDR_CAP, "0", update_sys_node);
    DA_DEFINE(HDR_POLICY, "0", update_sys_node);
    DA_DEFINE(HDR_MODE, "0", update_sys_node);
    DA_DEFINE(SDR_MODE, "0", update_sys_node);
    DA_DEFINE(HDMI_COLOR_ATTR, "0", update_sys_node);
    DA_DEFINE(HDMI_AVMUTE, "0", update_sys_node);

#define DA_SET_NODE(ID, NODE) \
    display_attrs[DA_##ID].sysfs_node = NODE
    if (major >= 5) {
        DA_SET_NODE(DOLBY_VISION_ENABLE ,"/sys/module/aml_media/parameters/dolby_vision_enable");
        DA_SET_NODE(DOLBY_VISION_STATUS ,"/sys/module/aml_media/parameters/dolby_vision_status");
        DA_SET_NODE(DOLBY_VISION_POLICY ,"/sys/module/aml_media/parameters/dolby_vision_policy");
        DA_SET_NODE(DOLBY_VISION_LL_POLICY ,"/sys/module/aml_media/parameters/dolby_vision_ll_policy");
        DA_SET_NODE(DOLBY_VISION_HDR_10_POLICY ,"/sys/module/aml_media/parameters/dolby_vision_hdr10_policy");
        DA_SET_NODE(DOLBY_VISION_GRAPHICS_PRIORITY ,"/sys/module/aml_media/parameters/dolby_vision_graphics_priority");
        DA_SET_NODE(HDR_POLICY ,"/sys/module/aml_media/parameters/hdr_policy");
        DA_SET_NODE(HDR_MODE ,"/sys/module/aml_media/parameters/hdr_mode");
        DA_SET_NODE(SDR_MODE ,"/sys/module/aml_media/parameters/sdr_mode");
    } else {
        DA_SET_NODE(DOLBY_VISION_ENABLE ,"/sys/module/amdolby_vision/parameters/dolby_vision_enable");
        DA_SET_NODE(DOLBY_VISION_STATUS ,"/sys/module/amdolby_vision/parameters/dolby_vision_status");
        DA_SET_NODE(DOLBY_VISION_POLICY ,"/sys/module/amdolby_vision/parameters/dolby_vision_policy");
        DA_SET_NODE(DOLBY_VISION_LL_POLICY ,"/sys/module/amdolby_vision/parameters/dolby_vision_ll_policy");
        DA_SET_NODE(DOLBY_VISION_HDR_10_POLICY ,"/sys/module/amdolby_vision/parameters/dolby_vision_hdr10_policy");
        DA_SET_NODE(DOLBY_VISION_GRAPHICS_PRIORITY ,"/sys/module/amdolby_vision/parameters/dolby_vision_graphics_priority");
        DA_SET_NODE(HDR_POLICY ,"/sys/module/am_vecm/parameters/hdr_policy");
        DA_SET_NODE(HDR_MODE ,"/sys/module/am_vecm/parameters/hdr_mode");
        DA_SET_NODE(SDR_MODE ,"/sys/module/am_vecm/parameters/sdr_mode");
    }
    DA_SET_NODE(DOLBY_VISION_CAP ,"/sys/class/amhdmitx/amhdmitx0/dv_cap");
    DA_SET_NODE(DOLBY_VISION_MODE ,"/sys/class/amdolby_vision/dv_mode");
    DA_SET_NODE(HDR_CAP ,"/sys/class/amhdmitx/amhdmitx0/hdr_cap");
    DA_SET_NODE(HDMI_COLOR_ATTR ,"/sys/class/amhdmitx/amhdmitx0/attr");
    DA_SET_NODE(HDMI_AVMUTE ,"/sys/devices/virtual/amhdmitx/amhdmitx0/avmute");

#define DA_SET_READ_ONLY(ID) \
    display_attrs[DA_##ID].is_read_only = true
    DA_SET_READ_ONLY(DOLBY_VISION_CAP);
    DA_SET_READ_ONLY(DOLBY_VISION_STATUS);
    DA_SET_READ_ONLY(HDR_CAP);
#define DA_SET_WRITE_ONLY(ID) \
    display_attrs[DA_##ID].is_write_only= true
    DA_SET_WRITE_ONLY(DOLBY_VISION_MODE);
}

DisplayAdapter::BackendType DisplayAdapterLocal::displayType() {
    return DISPLAY_TYPE_FBDEV;
}

bool DisplayAdapterLocal::getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    map<uint32_t, drm_mode_info_t> modes;
    DisplayTypeConv(type, displayType);
    getHwDisplayManager()->getConnector(connector, type);
    if (connector) {
        connector->getModes(modes);
        displayModeList.clear();
        DisplayModeInfo mode;
        for (auto it : modes) {
            DisplayModeConv(mode, it.second);
            displayModeList.push_back(mode);
        }
    }
    return true;;
}

bool DisplayAdapterLocal::getDisplayMode(string& mode, ConnectorType displayType) {
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        crtc->readCurDisplayMode(mode);
    }

    MESON_LOGV("GetDisplayMode:%s", mode.c_str());
    return true;
}

bool DisplayAdapterLocal::setDisplayMode(const string& mode, ConnectorType displayType) {
    drm_connector_type_t type;
    drm_mode_info_t mock;
    strncpy(mock.name, mode.c_str(), DRM_DISPLAY_MODE_LEN);
    DisplayTypeConv(type, displayType);

    MESON_LOGD("SetDisplay[%s] Mode to \"%s\"", type == DRM_MODE_CONNECTOR_HDMIA ? "HDMI" :
            (type == DRM_MODE_CONNECTOR_LVDS ? "panel":"dummy"), mode.c_str());

    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        crtc->setMode(mock);
    } else {
        sysfs_set_string(SYSFS_DISPLAY_MODE, mode.c_str());
        MESON_LOGE("SetDisplayMode %s , no crtc", mode.c_str());
    }

    return true;
}

bool DisplayAdapterLocal::captureDisplayScreen(const native_handle_t **outBufferHandle) {
    // get framebuffer width and height
    uint32_t fbW = 1290;
    uint32_t fbH = 1080;
    HwcConfig::getFramebufferSize(0, fbW, fbH);

    if (sys_get_bool_prop("vendor.hwc.3dmode", false)) {
        fbW = 960;
        fbH = 540;
    }

    // format is always HAL_PIXEL_FORMAT_RGB_888
    native_handle_t* hnd = gralloc_alloc_dma_buf(fbW, fbH, HAL_PIXEL_FORMAT_RGB_888, true, false);
    if (hnd == nullptr || am_gralloc_get_buffer_fd(hnd) < 0) {
        MESON_LOGE("DisplayAdapterLocal::captureDisplayScreen alloc buffer error");
        return false;
    }

    int32_t ret = MesonHwc2::getInstance().captureDisplayScreen(hnd);
    *outBufferHandle = hnd;

    if (ret != 1) {
        gralloc_unref_dma_buf(hnd);
        gralloc_free_dma_buf(hnd);
    }

    return ret == 1 ? true : false;
}

bool DisplayAdapterLocal::setDisplayRect(const Rect rect, ConnectorType displayType) {
    bool ret = false;
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    drm_rect_wh_t drm_rect;

    MESON_LOGV("SetDisplay[%s] DisplayRect to \"(%s)\"", type == DRM_MODE_CONNECTOR_HDMIA ? "HDMI" :
            (type == DRM_MODE_CONNECTOR_LVDS ? "panel" : "cvbs"), rect.toString().c_str());

    drm_rect.x = rect.x;
    drm_rect.y = rect.y;
    drm_rect.w = rect.w;
    drm_rect.h = rect.h;

    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        crtc->setViewPort(drm_rect);
        ret = true;
    }
    MESON_LOGV("SetDisplayViewPort %s", ret ? "done" : "faild");
    return ret;
}

bool DisplayAdapterLocal::getDisplayRect(Rect& rect, ConnectorType displayType) {
    bool ret = false;
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    drm_rect_wh_t drm_rect;

    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        crtc->getViewPort(drm_rect);
        DEBUG_INFO("SetDisplay[%s] view port to \"(%s)\"", type == DRM_MODE_CONNECTOR_HDMIA ? "HDMI" :
                type == DRM_MODE_CONNECTOR_LVDS ? "panel" : "cvbs", rect.toString().c_str());
        rect.x = drm_rect.x;
        rect.y = drm_rect.y;
        rect.h = drm_rect.h;
        rect.w = drm_rect.w;
        ret = true;
    }
    MESON_LOGV("SetDisplayViewPort %s", ret ? "doen" : "faild");
    return ret;
}

bool DisplayAdapterLocal::dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) {
    Json::Value ret;
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    int i = 0;
    for (i = 0; i < DA_DISPLAY_ATTRIBUTE__COUNT; i++) {
        std::string value;
        if (display_attrs[i].update_fun) {
            display_attrs[i].update_fun(display_attrs[i], "", value, UT_GET_VALUE);
        }
        Json::Value item;
        item["id"] = display_attrs[i].attr_id;
        item["value"] = display_attrs[i].current_value;
        //item["status flags"] = display_attrs[i].status_flags;
        ret[display_attrs[i].name] = item;
    }
    json = ret;

    return true;
};


DisplayAttributeInfo* DisplayAdapterLocal::getDisplayAttributeInfo(const string& name, ConnectorType displayType) {
    UNUSED(displayType);
    int i = 0;
    for (i = 0; i < DA_DISPLAY_ATTRIBUTE__COUNT; i++) {
        if (display_attrs[i].name == name) {
            return &(display_attrs[i]);
        }
    }
    MESON_LOGV("Access invalid display attribute named \"%s\"", name.c_str());
    return NULL;
}

bool DisplayAdapterLocal::setDisplayAttribute(
        const string& name, const string& value,
        ConnectorType displayType) {
    bool ret = false;
    MESON_LOGD("SetDisplay[%s] attr to \"%s\"", name.c_str(), value.c_str());

    /*
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);

    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        string dispattr (value);
        crtc->setDisplayAttribute(dispattr);
    */
    drm_connector_type_t type;
    string out;
    DisplayTypeConv(type, displayType);
    DisplayAttributeInfo* info = getDisplayAttributeInfo(name, displayType);
    if (info && info->update_fun) {
       ret = info->update_fun(*info, value, out, UT_SET_VALUE);
    }
    if (ret == false) {
        MESON_LOGV("Set display attribute \"%s\" fail", name.c_str());
    }
    return ret;
};

bool DisplayAdapterLocal::getDisplayAttribute(
        const string& name, string& value,
        ConnectorType displayType) {
    bool ret = false;
    string out = "";
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    /*
    GET_CRTC_BY_CONNECTOR(type);
    if (crtc) {
        crtc->getDisplayAttribute(value);
    */
    DisplayAttributeInfo* info = getDisplayAttributeInfo(name, displayType);
    if (info && info->update_fun) {
       if (info->update_fun(*info, "", out, UT_GET_VALUE)) {
           value = out;
           ret = true;
       }
    }
    if (ret == false) {
        MESON_LOGV("Get display attribute \"%s\" fail", name.c_str());
    }
    MESON_LOGD("getDisplayAttribute \"%s\": \"%s\"", name.c_str(), value.c_str());
    return ret;
};

std::unique_ptr<DisplayAdapter> DisplayAdapterLocal::create(DisplayAdapter::BackendType type) {
    switch (type) {
        case BackendType::DISPLAY_TYPE_DRM:
            return {nullptr};
        case BackendType::DISPLAY_TYPE_FBDEV:
            return static_cast<std::unique_ptr<DisplayAdapter>>(std::make_unique<DisplayAdapterLocal>());
        default:
            return static_cast<std::unique_ptr<DisplayAdapter>>(std::make_unique<DisplayAdapterLocal>());
    }
}

std::unique_ptr<DisplayAdapter> DisplayAdapterCreateLocal(DisplayAdapter::BackendType type) {
    return DisplayAdapterLocal::create(type);
}


}; //namespace meson
