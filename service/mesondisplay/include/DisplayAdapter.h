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
#include <inttypes.h>
#include <json/json.h>

namespace meson{
using namespace std;
#define DISPLAY_DOLBY_VISION_CAP                "Dolby Vision CAP"
#define DISPLAY_DOLBY_VISION_CAP2               "Dolby Vision CAP2"
#define DISPLAY_DOLBY_VISION_ENABLE             "Dolby Vision Enable"
#define DISPLAY_DOLBY_VISION_MODE               "Dolby Vision Mode"
#define DISPLAY_DOLBY_VISION_STATUS             "Dolby Vision Status"
#define DISPLAY_DOLBY_VISION_POLICY             "Dolby Vision Policy"
#define DISPLAY_DOLBY_VISION_LL_POLICY          "Dolby Vision LL Policy"
#define DISPLAY_DOLBY_VISION_HDR_10_POLICY      "Dolby Vision HDR 10 Policy"
#define DISPLAY_DOLBY_VISION_GRAPHICS_PRIORITY  "Dolby Vision Graphics Priority"
#define DISPLAY_HDR_POLICY                      "HDR Policy"
#define DISPLAY_HDR_MODE                        "HDR Mode"
#define DISPLAY_SDR_MODE                        "SDR Mode"
#define DISPLAY_HDR_CAP                         "HDR CAP"
#define DISPLAY_HDMI_COLOR_ATTR                 "HDMI Color ATTR"
#define DISPLAY_HDMI_AVMUTE                     "HDMI Avmute"
#define DISPLAY_FR_HINT                         "FRAME HINT"
#define DISPLAY_HDR_PRIORITY                    "HDR Priority"
#define DISPLAY_FORCE_HDR_MODE                  "FORCE HDR Mode"

typedef struct {
    string name;
    uint32_t dpiX, dpiY;
    uint32_t pixelW, pixelH;
    float refreshRate;
} DisplayModeInfo;

typedef struct _Rect{
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    string toString() const {
        return std::to_string(x) + "," + std::to_string(y) \
            + "," + std::to_string(w) + "," + std::to_string(h);
    }
    _Rect(int32_t X, int32_t Y, int32_t W, int32_t H) {
        x = X;
        y = Y;
        w = W;
        h = H;
    };
    _Rect(const char* str) {
        int count = 0;
        count = sscanf(str, "%" SCNd32 ",%" SCNd32 ",%" SCNd32 ",%" SCNd32 , &x, &y, &w, &h);
        if (count != 4) {
            x = 0;
            y = 0;
            w = 0;
            h = 0;
        }
    };
    _Rect() {
        x = 0;
        y = 0;
        w = 0;
        h = 0;
    };
} Rect;

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
        CONN_TYPE_UNKNOWN = -1,
        CONN_TYPE_DUMMY = 0,
        CONN_TYPE_HDMI = 1,
        CONN_TYPE_PANEL = 2,
        CONN_TYPE_CVBS = 3,
        CONN_TYPE_HDMIA =  4,
        CONN_TYPE_TV =  5,
        CONN_TYPE_LVDS =  6,
        CONN_TYPE_LVDS_A =  7,
        CONN_TYPE_LVDS_B =  8,
        CONN_TYPE_LVDS_C =  9,
        CONN_TYPE_VBYONE_A =  10,
        CONN_TYPE_VBYONE_B =  11,
        CONN_TYPE_MIPI_A =  12,
        CONN_TYPE_MIPI_B =  13,
        CONN_TYPE_EDP_A =  14,
        CONN_TYPE_EDP_B =  15,
        CONN_TYPE_HDMIA_A = 16,
        CONN_TYPE_HDMIA_B = 17,
        CONN_TYPE_HDMIA_C = 18,
        CONN_TYPE_HDMIB_A = 19,
        CONN_TYPE_HDMIB_B = 20,
        CONN_TYPE_HDMIB_C = 21,
    } ConnectorType;
    virtual AdapterType type() = 0;
    virtual BackendType displayType() = 0;
    virtual bool isReady() {
        return true;
    }
    virtual bool getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
        UNUSED(displayModeList);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool getDisplayIds(vector<int>& displayIdList) {
        UNUSED(displayIdList);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool getConnectorType (uint32_t displayId, ConnectorType & outDisplayType) {
        UNUSED(displayId);
        UNUSED(outDisplayType);
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

    virtual bool setPerferredMode(const string& mode, ConnectorType displayType) {
        UNUSED(mode);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool getCurrentSupportDeepColor(string &color, ConnectorType displayType) = 0;

    virtual bool setColorSpace(const string& colorspace, ConnectorType displayType) {
        UNUSED(colorspace);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool clearUserDisplayConfig(ConnectorType displayType) {
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool setDvMode(const string& dv_mode, ConnectorType displayType) {
        UNUSED(dv_mode);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool captureDisplayScreen(const native_handle_t **outBufferHandle) = 0;

    virtual bool getWhiteBoardHanle(const native_handle_t **outBufferHandle, int& fd) = 0;
    virtual bool setWhiteBoardMode(bool mode, int callingPid = 0) = 0;
    virtual bool getWhiteBoardMode(bool& mode) = 0;
    virtual bool setWBDisplayFrame(int x, int y) = 0;
    virtual bool hideVideoLayer(bool hide) = 0;
    virtual bool enableSyncProtection(bool mode) = 0;

    virtual bool setHdrConversionStrategy(uint32_t passThrough, uint32_t forcemode);

    virtual bool setDisplayRect(const Rect rect, ConnectorType displayType) {
        UNUSED(rect);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool getDisplayRect(Rect& rect, ConnectorType displayType) {
        UNUSED(rect);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };

    virtual bool setUbootenv(const string& key, const string& value) = 0;
    virtual bool getUbootenv(const string& key, string& value) = 0;

    virtual bool setDisplayAttribute(const string& name, const string& value, ConnectorType displayType) = 0;
    virtual bool getDisplayAttribute(const string& name, string& value, ConnectorType displayType) = 0;
    //for AFR
    virtual int32_t setFrameRate(float frameRate) = 0;
    /* get  vsync timestamp and vsync period in nanos */
    virtual bool getDisplayVsyncAndPeriod(int64_t& timestamp, int32_t& vsyncPeriodNanos) = 0;

    virtual bool dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) {
        UNUSED(json);
        UNUSED(displayType);
        NOTIMPLEMENTED;
        return false;
    };
    virtual bool disableSidebandStream(bool isDisable) = 0;
    virtual bool setKeystoneCorrection(const string& params) = 0;
    virtual bool setReverseMode(int type) = 0;

    virtual ~DisplayAdapter() = default;
    DisplayAdapter() = default;
private:
    DisplayAdapter(const DisplayAdapter&) = delete;
    void operator=(const DisplayAdapter&) = delete;
};


bool Json2DisplayMode(const Json::Value& json, DisplayModeInfo& mode);
bool DisplayMode2Json(const DisplayModeInfo& mode, Json::Value& json);

std::string JsonValue2String(const Json::Value& json);
bool String2JsonValue(const std::string& document, Json::Value& out);

std::shared_ptr<DisplayAdapter> DisplayAdapterCreateLocal(DisplayAdapter::BackendType type);
std::unique_ptr<DisplayAdapter> DisplayAdapterCreateRemote();

typedef DisplayAdapter* create_DisplayAdapterRemote();
typedef void destroy_DisplayAdapterRemote(DisplayAdapter *adapter);

};

