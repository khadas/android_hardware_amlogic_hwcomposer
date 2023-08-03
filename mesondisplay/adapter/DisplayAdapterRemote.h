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
#include "utile.h"
#include "DisplayAdapter.h"
#include "DisplayClient.h"

namespace meson{
using namespace std;

class DisplayAdapterRemote: public DisplayAdapter {
public:
    AdapterType type() override { return ADAPTER_TYPE_REMOTE; }
    BackendType displayType() override;
    virtual bool isReady() override {
        return connectServerIfNeed();
    }
    bool getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) override;
    bool getDisplayMode(string& mode, ConnectorType displayType) override;
    bool setDisplayMode(const string& mode, ConnectorType displayType) override;
    bool captureDisplayScreen(const native_handle_t **outBufferHandle) override;
    bool setDisplayRect(const Rect rect, ConnectorType displayType);
    bool getDisplayRect(Rect& rect, ConnectorType displayType);
    bool setDisplayAttribute(const string& name, const string& value, ConnectorType displayType) override;
    bool getDisplayAttribute(const string& name, string& value, ConnectorType displayType) override;
    bool getDisplayVsyncAndPeriod(int64_t& vsyncTimestamp, int32_t& vsyncPeriodNanos) override;
    int32_t setFrameRate(float frameRate) override;
    bool getWhiteBoardHanle(const native_handle_t **outBufferHandle,int& fd) override;
    bool setWriteBoardMode(bool mode, int callingpid = 0) override;
    bool getWriteBoardMode(bool& mode) override;
    bool setWBDisplayFrame(int x, int y) override;
    bool hideVideoLayer(bool hide) override;
    bool setHdrConversionStrategy(uint32_t passThrough, uint32_t forcemode) override;

    bool dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) override;
    bool enableSyncProtection(bool mode) override;
    bool disableSidebandStream(bool isDisable) override;
    bool setPerferredMode(const string& mode, ConnectorType displayType) override;
    bool setColorSpace(const string& colorspace, ConnectorType displayType) override;
    bool clearUserDisplayConfig(ConnectorType displayType) override;
    bool setDvMode(const string& dv_mode, ConnectorType displayType) override;

    static std::unique_ptr<DisplayAdapter> create();
    DisplayAdapterRemote();
    ~DisplayAdapterRemote() = default;
private:

    // For later start server.
    bool connectServerIfNeed();
    DisplayClient *ipc;
    DISALLOW_COPY_AND_ASSIGN(DisplayAdapterRemote);
};

} //namespace android
