/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "HwDisplayManager.h"
#include "HwDisplayConnector.h"
#include "HwDisplayCrtc.h"
#include "DisplayAdapterLocal.h"

#include "misc.h"
#include "MesonHwc2.h"
#include "HwcConfig.h"
#include "MesonLog.h"

#define SYSFS_DISPLAY_MODE              "/sys/class/display/mode"

namespace meson{

using namespace std;
using ConnectorType = DisplayAdapter::ConnectorType;
using BackendType = DisplayAdapter::BackendType;

void DisplayTypeConv(drm_connector_type_t& type, ConnectorType displayType) {
    switch (displayType) {
        case DisplayAdapter::CONN_TYPE_HDMI:
            type = DRM_MODE_CONNECTOR_HDMI;
            break;
        case DisplayAdapter::CONN_TYPE_PANEL:
            type = DRM_MODE_CONNECTOR_PANEL;
            break;
        case DisplayAdapter::CONN_TYPE_DUMMY:
            type = DRM_MODE_CONNECTOR_DUMMY;
            break;
        case DisplayAdapter::CONN_TYPE_CVBS:
            type = DRM_MODE_CONNECTOR_CVBS;
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
}

DisplayAdapter::BackendType DisplayAdapterLocal::displayType() {
    return DISPLAY_TYPE_FBDEV;
}

bool DisplayAdapterLocal::getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    map<uint32_t, drm_mode_info_t> modes;
    DisplayTypeConv(type, displayType);
    HwDisplayManager::getInstance().getConnector(connector, type);
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
    std::shared_ptr<HwDisplayConnector> connector;

    DisplayTypeConv(type, displayType);
    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->readCurDisplayMode(mode);
    }

    MESON_LOGV("GetDisplayMode:%s", mode.c_str());
    return true;
}

bool DisplayAdapterLocal::setDisplayMode(const string& mode, ConnectorType displayType) {
    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    drm_mode_info_t mock;
    strncpy(mock.name, mode.c_str(), DRM_DISPLAY_MODE_LEN);
    DisplayTypeConv(type, displayType);

    MESON_LOGD("SetDisplay[%s] Mode to \"%s\"", type == DRM_MODE_CONNECTOR_HDMI ? "HDMI" :
            (type == DRM_MODE_CONNECTOR_PANEL ? "panel":"dummy"), mode.c_str());

    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->setMode(mock);
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
    std::shared_ptr<HwDisplayConnector> connector;
    drm_rect_wh_t drm_rect;

    MESON_LOGV("SetDisplay[%s] DisplayRect to \"(%s)\"", type == DRM_MODE_CONNECTOR_HDMI ? "HDMI" :
            (type == DRM_MODE_CONNECTOR_PANEL ? "panel" : "cvbs"), rect.toString().c_str());

    drm_rect.x = rect.x;
    drm_rect.y = rect.y;
    drm_rect.w = rect.w;
    drm_rect.h = rect.h;
    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->setViewPort(drm_rect);
        ret = true;
    }
    MESON_LOGV("SetDisplayViewPort %s", ret ? "doen" : "faild");
    return ret;
}

bool DisplayAdapterLocal::getDisplayRect(Rect& rect, ConnectorType displayType) {
    bool ret = false;
    drm_connector_type_t type;
    DisplayTypeConv(type, displayType);
    std::shared_ptr<HwDisplayConnector> connector;
    drm_rect_wh_t drm_rect;

    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->getViewPort(drm_rect);
        DEBUG_INFO("SetDisplay[%s] view port to \"(%s)\"", type == DRM_MODE_CONNECTOR_HDMI ? "HDMI" :
                type == DRM_MODE_CONNECTOR_PANEL ? "panel" : "cvbs", rect.toString().c_str());
        rect.x = drm_rect.x;
        rect.y = drm_rect.y;
        rect.h = drm_rect.h;
        rect.w = drm_rect.w;
        ret = true;
    }
    MESON_LOGV("SetDisplayViewPort %s", ret ? "doen" : "faild");
    return ret;
}

bool DisplayAdapterLocal::setDisplayAttribute(
        const string& name, const string& value,
        ConnectorType displayType) {
    bool ret = false;

    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    DisplayTypeConv(type, displayType);

    MESON_LOGD("SetDisplay[%s] attr to \"%s\"", name.c_str(), value.c_str());

    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        string dispattr (value);
        connector->mCrtc->setDisplayAttribute(dispattr);
    }

    return ret;
}

bool DisplayAdapterLocal::getDisplayAttribute(
        const string& name, string& value,
        ConnectorType displayType) {
    bool ret = false;

    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    DisplayTypeConv(type, displayType);


    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->getDisplayAttribute(value);
    }

    MESON_LOGV("GetDisplay[%s] attr \"%s\"", name.c_str(), value.c_str());

    return ret;
}

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
