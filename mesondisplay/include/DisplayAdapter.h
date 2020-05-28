/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#pragma once
#include <stdbool.h>
#include <string>
#include <memory>
#include <vector>
#include <cutils/native_handle.h>
#include "utile.h"

namespace Json {
    class Value;
}

namespace meson{
using namespace std;

typedef struct {
    string name;
    uint32_t dpiX, dpiY;
    uint32_t pixelW, pixelH;
    float refreshRate;
} DisplayModeInfo;


class DisplayAdapter {
public:
    typedef enum {
        ADAPTER_TYPE_REMOTE = 0,
        ADAPTER_TYPE_LOCAL = 1,
    } AdapterType;
    typedef enum {
        DISPLAY_TYPE_DRM = 0,
        DISPLAY_TYPE_FBDEV = 1,
        DISPLAY_TYPE_MAX,
    } BackendType;
    typedef enum {
        CONN_TYPE_DUMMY = 0,
        CONN_TYPE_HDMI = 1,
        CONN_TYPE_PANEL = 2,
    } ConnectorType;
    virtual AdapterType type() = 0;
    virtual BackendType displayType() = 0;
    virtual bool getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
        UNUSED(displayModeList);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };
    virtual bool getDisplayMode(string& mode, ConnectorType displayType) {
        UNUSED(mode);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };
    virtual bool setDisplayMode(const string& mode, ConnectorType displayType) {
        UNUSED(mode);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };
    virtual bool setPrefDisplayMode(const string& mode, ConnectorType displayType) {
        UNUSED(mode);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool captureDisplayScreen(const native_handle_t **outBufferHandle) = 0;

    virtual ~DisplayAdapter() = default;
    DisplayAdapter() = default;
private:
    DisplayAdapter(const DisplayAdapter&) = delete;
    void operator=(const DisplayAdapter&) = delete;
};


bool Json2DisplayMode(const Json::Value& json, DisplayModeInfo& mode);

bool DisplayMode2Json(const DisplayModeInfo& mode, Json::Value& json);

std::unique_ptr<DisplayAdapter> DisplayAdapterCreateLocal(DisplayAdapter::BackendType type);
std::unique_ptr<DisplayAdapter> DisplayAdapterCreateRemote();


};

