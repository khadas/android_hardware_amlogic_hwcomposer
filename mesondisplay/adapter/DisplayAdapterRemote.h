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
    bool setPrefDisplayMode(const string& mode, ConnectorType displayType) override;
    bool captureDisplayScreen(const native_handle_t **outBufferHandle) override;
    bool setDisplayRect(const Rect rect, ConnectorType displayType);
    bool getDisplayRect(Rect& rect, ConnectorType displayType);

    static std::unique_ptr<DisplayAdapter> create();
    DisplayAdapterRemote();
    ~DisplayAdapterRemote() = default;
private:

    // For later start server.
    bool connectServerIfNeed();
    std::unique_ptr<DisplayClient> ipc;
    DISALLOW_COPY_AND_ASSIGN(DisplayAdapterRemote);
};

} //namespace android