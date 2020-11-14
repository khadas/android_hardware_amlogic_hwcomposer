/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once
#include "utile.h"
#include "DisplayAdapter.h"

namespace meson{

enum DisplayAttribute {
    DA_DOLBY_VISION_CAP,
    DA_DOLBY_VISION_CAP2,
    DA_DOLBY_VISION_ENABLE,
    DA_DOLBY_VISION_MODE,
    DA_DOLBY_VISION_STATUS,
    DA_DOLBY_VISION_POLICY,
    DA_DOLBY_VISION_LL_POLICY,
    DA_DOLBY_VISION_HDR_10_POLICY,
    DA_DOLBY_VISION_GRAPHICS_PRIORITY,
    DA_HDR_CAP,
    DA_HDR_POLICY,
    DA_HDR_MODE,
    DA_SDR_MODE,
    DA_HDMI_COLOR_ATTR,
    DA_HDMI_AVMUTE,
    DA_DISPLAY_ATTRIBUTE__COUNT
};

enum UpdateType {
    UT_GET_VALUE,
    UT_SET_VALUE,
    UT_UPSATE_TYPE__COUNT
};

struct _DisplayAttributeInfo;
typedef bool (update_fun)(struct _DisplayAttributeInfo& info, const string& in, string& out, UpdateType type);

typedef struct _DisplayAttributeInfo {
    const char* name;
    uint32_t attr_id;
    string current_value;
    string new_value;
    uint32_t status_flags;
    update_fun* update_fun;
    const char* sysfs_node;
    bool is_read_only;
    bool is_write_only;
} DisplayAttributeInfo;


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
    bool dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) override;

    static std::unique_ptr<DisplayAdapter> create(DisplayAdapter::BackendType type);
    DisplayAdapterLocal();
    ~DisplayAdapterLocal() = default;
private:
    DisplayAttributeInfo* getDisplayAttributeInfo(const string& name, ConnectorType displayType);
    DisplayAttributeInfo display_attrs[DA_DISPLAY_ATTRIBUTE__COUNT];

    DISALLOW_COPY_AND_ASSIGN(DisplayAdapterLocal);
};

}; //namespace meson
