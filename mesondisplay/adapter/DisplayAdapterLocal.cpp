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
};

bool DisplayAdapterLocal::getDisplayMode(string& mode, ConnectorType displayType) {
    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    drm_mode_info_t mode_in;

    DisplayTypeConv(type, displayType);
    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->getMode(mode_in);
        mode = mode_in.name;
    }
    DEBUG_INFO("GetDisplayMode:%s", mode.c_str());
    return true;
}

bool DisplayAdapterLocal::setDisplayMode(const string& mode, ConnectorType displayType) {
    drm_connector_type_t type;
    std::shared_ptr<HwDisplayConnector> connector;
    drm_mode_info_t mock;
    strncpy(mock.name, mode.c_str(), DRM_DISPLAY_MODE_LEN);
    DisplayTypeConv(type, displayType);

    DEBUG_INFO("SetDisplay[%s] Mode to \"%s\"", type == DRM_MODE_CONNECTOR_HDMI ? "HDMI" :
            (type == DRM_MODE_CONNECTOR_PANEL ? "panel":"dummy"), mode.c_str());

    HwDisplayManager::getInstance().getConnector(connector, type);
    if (connector && connector->mCrtc) {
        connector->mCrtc->setMode(mock);
    }
    DEBUG_INFO("SetDisplayMode done");
    return true;
};

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
};


}; //namespace meson
