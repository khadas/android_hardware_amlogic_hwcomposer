/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once
#include "DisplayAdapter.h"
#include "utile.h"

namespace meson{

class DisplayAdapterLocal: public DisplayAdapter {
public:
    AdapterType type() override { return ADAPTER_TYPE_LOCAL; }
    BackendType displayType() override;

    bool getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) override;
    bool getDisplayMode(string& mode, ConnectorType displayType) override;
    bool setDisplayMode(const string& mode, ConnectorType displayType) override;
    bool setDisplayRect(const Rect rect, ConnectorType displayType);
    bool getDisplayRect(Rect& rect, ConnectorType displayType);
    bool captureDisplayScreen(const native_handle_t **outBufferHandle) override;
    bool setDisplayAttribute(const string& name, const string& value, ConnectorType displayType) override;
    bool getDisplayAttribute(const string& name, string& value, ConnectorType displayType) override;

    static std::unique_ptr<DisplayAdapter> create(DisplayAdapter::BackendType type);
    DisplayAdapterLocal();
    ~DisplayAdapterLocal() = default;
private:

    DISALLOW_COPY_AND_ASSIGN(DisplayAdapterLocal);
};

}; //namespace meson
