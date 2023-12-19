/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <errno.h>
#include "utile.h"
#include <unistd.h>
#include "DisplayAdapterLocal.h"
#include <fcntl.h>
#include <sys/utsname.h>
namespace meson{
//For recovery mode for temporary.
#define SYSFS_DISPLAY_MODE              "/sys/class/display/mode"
#define SYSFS_DISPLAY_MODE2             "/sys/class/display2/mode"
#define SYSFS_DISPLAY_DEBUG             "/sys/class/display/debug"

#define MAX_BUFFER_LEN_EDID             4096
#define READ_BUFFER_LEN 64
#define FORMAT_DISPLAY_HDMI_EDID        "/sys/class/amhdmitx/amhdmitx0/disp_cap"//RX support display mode
#define DRM_DISPLAY_MODE_NULL ("null")

#define FS_READ(path, buffer, len) \
do { \
    int fd;\
    if ((fd = open(path, O_RDONLY)) < 0) { \
        perror(path);\
        DEBUG_INFO("open file %s error!", path);\
        goto error_handle; \
    } \
    if (read(fd, buffer, len) == -1) { \
        perror(path); \
        DEBUG_INFO("Read file %s error!", path); \
        close(fd); \
        goto error_handle; \
    } \
    close(fd); \
} while (0)

#define FS_WRITE(path, buffer, len) \
do { \
    int fd;\
    if ((fd = open(path, O_RDWR)) < 0) { \
        perror(path);\
        DEBUG_INFO("open file %s error!", path);\
        goto error_handle; \
    } \
    if (write(fd, buffer, len) == -1) { \
        perror(path); \
        DEBUG_INFO("Read file %s error!", path); \
        close(fd); \
        goto error_handle; \
    } \
    close(fd); \
} while (0)

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
            FS_WRITE(node, in.c_str() , in.length());
            info.current_value = in;
            ret = true;
            break;
        case UT_GET_VALUE:
            if (is_write_only)
                break;
            FS_READ(node, buffer, SYS_FS_BUFFER_LEN_MAX);
            out = buffer;
            info.current_value = buffer;
            ret = true;
            break;
        default:
            break;
    }
error_handle:
    return ret;
}


#define CONNECT_HDMI_INDEX CONN1;
#define CONNECT_PANEL_INDEX CONN2;
#define CONNECT_DUMMY_INDEX CONN3;

using namespace std;
using ConnectorType = DisplayAdapter::ConnectorType;
typedef enum {
    CONN1 = 0,
    CONN2 = 1,
    CONN3 = 2,
} connector_index;

void DisplayTypeConvToIndex(ConnectorType displayType, connector_index& type) {
    switch (displayType) {
        case DisplayAdapter::CONN_TYPE_HDMI:
            type = CONNECT_HDMI_INDEX;
            break;
        case DisplayAdapter::CONN_TYPE_PANEL:
            type = CONNECT_PANEL_INDEX;
            break;
        default:
            type = CONNECT_DUMMY_INDEX;
            break;
    }
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
        DEBUG_INFO("Can't determine kernel version for access sysfs!");
    }

#define DA_DEFINE(ID, INIT_VAL, UPDATE_FUN)  \
    display_attrs[DA_##ID] = { .name = DISPLAY_##ID, .attr_id = DA_##ID, .current_value = INIT_VAL, .new_value = INIT_VAL, .status_flags = 0, .update_fun = UPDATE_FUN, .sysfs_node = NULL, .is_read_only = false, .is_write_only = false }

    DA_DEFINE(DOLBY_VISION_CAP, "0", update_sys_node);
    DA_DEFINE(DOLBY_VISION_CAP2, "0", update_sys_node);
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
    DA_DEFINE(FR_HINT, "0", update_sys_node);
    DA_DEFINE(HDR_PRIORITY, "0", update_sys_node);
    DA_DEFINE(FORCE_HDR_MODE, "0", update_sys_node);

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
    DA_SET_NODE(DOLBY_VISION_CAP2 ,"/sys/class/amhdmitx/amhdmitx0/dv_cap2");
    DA_SET_NODE(DOLBY_VISION_MODE ,"/sys/class/amdolby_vision/dv_mode");
    DA_SET_NODE(HDR_CAP ,"/sys/class/amhdmitx/amhdmitx0/hdr_cap");
    DA_SET_NODE(HDMI_COLOR_ATTR ,"/sys/class/amhdmitx/amhdmitx0/attr");
    DA_SET_NODE(HDMI_AVMUTE ,"/sys/devices/virtual/amhdmitx/amhdmitx0/avmute");
    DA_SET_NODE(FR_HINT, "/sys/class/display/fr_hint");
    DA_SET_NODE(HDR_PRIORITY, "/sys/class/amhdmitx/amhdmitx0/hdr_priority_mode");
    DA_SET_NODE(FORCE_HDR_MODE, "/sys/module/aml_media/parameters/force_output");

#define DA_SET_READ_ONLY(ID) \
    display_attrs[DA_##ID].is_read_only = true
    DA_SET_READ_ONLY(DOLBY_VISION_CAP);
    DA_SET_READ_ONLY(DOLBY_VISION_CAP2);
    DA_SET_READ_ONLY(DOLBY_VISION_STATUS);
    DA_SET_READ_ONLY(HDR_CAP);
#define DA_SET_WRITE_ONLY(ID) \
    display_attrs[DA_##ID].is_write_only= true
    DA_SET_WRITE_ONLY(DOLBY_VISION_MODE);
}

DisplayAdapter::BackendType DisplayAdapterLocal::displayType() {
    return DISPLAY_TYPE_FBDEV;
}

bool GetHDMIEDID(char* buffer, int len) {
    FS_READ(FORMAT_DISPLAY_HDMI_EDID, buffer, len);
    return true;
error_handle:
    DEBUG_INFO("Get EDID error:%s", strerror(errno));
    return false;
}

bool DisplayAdapterLocal::getSupportDisplayModes(vector<DisplayModeInfo>& displayModeList, ConnectorType displayType) {
    DisplayModeInfo mode = {
           DRM_DISPLAY_MODE_NULL,
           0, 0,
           0, 0,
           60.0,
    };
    if (displayType == DisplayAdapter::CONN_TYPE_HDMI) {
        displayModeList.clear();
        char edid_buf[MAX_BUFFER_LEN_EDID];
        const char *delim = "\n";
        char *ptr = NULL;
        if (false == GetHDMIEDID(edid_buf, MAX_BUFFER_LEN_EDID)) {
            return false;
        }
        edid_buf[MAX_BUFFER_LEN_EDID - 1] = '\0';
        ptr = strtok(edid_buf, delim);
        while (ptr != NULL) {
            int len = strlen(ptr);
            if (ptr[len - 1] == '*')
                ptr[len - 1] = '\0';

            mode.name = ptr;
            //TODO: Add other info
            displayModeList.push_back(mode);
            ptr = strtok(NULL, delim);
        };
    } else {
        //TODO: now just show the current mode beside HDMI
        displayModeList.clear();
        getDisplayMode(mode.name, displayType);
        displayModeList.push_back(mode);
    }

    return true;
};

bool DisplayAdapterLocal::getDisplayMode(string& mode, ConnectorType displayType) {
    connector_index conn;
    char buf[READ_BUFFER_LEN] = {0};
    const char* path = NULL;
    const char *delim = "\n ";
    char *ptr = NULL;

    DisplayTypeConvToIndex(displayType, conn);
    if (conn == CONN1) {
        path = SYSFS_DISPLAY_MODE;
    } else if (conn == CONN2) {
        path = SYSFS_DISPLAY_MODE2;
    } else {
        DEBUG_INFO("No such device");
        assert(0);
        return false;
    }

    FS_READ(path, buf, READ_BUFFER_LEN);
    buf[READ_BUFFER_LEN - 1] = '\0';
    ptr = strtok(buf, delim);
    if (ptr != NULL) {
        mode = ptr;
    } else {
        mode = buf;
    }
    return true;
error_handle:
    return false;
}

bool DisplayAdapterLocal::setDisplayMode(const string& mode, ConnectorType displayType) {
    connector_index conn ;
    DisplayTypeConvToIndex(displayType, conn);
    const char* path = NULL;

    if (conn == CONN1) {
        path = SYSFS_DISPLAY_MODE;
    } else if (conn == CONN2) {
        path = SYSFS_DISPLAY_MODE2;
    } else {
        DEBUG_INFO("No such device");
        assert(0);
        return false;
    }

    //need to set "1" to debug for set mode
    FS_WRITE(SYSFS_DISPLAY_DEBUG, "1", 1);

    FS_WRITE(path, mode.c_str(), mode.length());

    return true;
error_handle:
    return false;
};

bool DisplayAdapterLocal::setPerferredMode(const string& mode, ConnectorType displayType) {
    bool ret = false;
    UNUSED(mode);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::getCurrentSupportDeepColor(string &color, ConnectorType displayType) {
    bool ret = false;
    UNUSED(color);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setColorSpace(const string& colorspace, ConnectorType displayType) {
    bool ret = false;
    UNUSED(colorspace);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::clearUserDisplayConfig(ConnectorType displayType) {
    bool ret = false;
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setDvMode(const string& dv_mode, ConnectorType displayType) {
    bool ret = false;
    UNUSED(dv_mode);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::captureDisplayScreen(const native_handle_t **outBufferHandle) {
    *outBufferHandle = nullptr;
    return true;
}

bool DisplayAdapterLocal::getWhiteBoardHanle(const native_handle_t **outBufferHandle, int& fd __unused) {
    *outBufferHandle = nullptr;
    return true;
}

bool DisplayAdapterLocal::enableSyncProtection(bool mode) {
    bool ret = false;
    UNUSED(mode);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setWhiteBoardMode(bool mode, int callingPid) {
    bool ret = false;
    UNUSED(mode);
    UNUSED(callingPid);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::getWhiteBoardMode(bool& mode) {
    bool ret = false;
    UNUSED(mode);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setWBDisplayFrame(int x, int y) {
    bool ret = false;
    UNUSED(x);
    UNUSED(y);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::hideVideoLayer(bool hide) {
    bool ret = false;
    UNUSED(hide);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setHdrConversionStrategy(uint32_t passThrough, uint32_t forceMode) {
    bool ret = false;
    UNUSED(passThrough);
    UNUSED(forceMode);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::setDisplayRect(const Rect rect, ConnectorType displayType) {
    bool ret = false;
    UNUSED(rect);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::getDisplayRect(Rect& rect, ConnectorType displayType) {
    bool ret = false;
    UNUSED(rect);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return ret;
}

int32_t DisplayAdapterLocal::setFrameRate(float frameRate) {
    UNUSED(frameRate);
    NOTIMPLEMENTED;
    return 0;
}

bool DisplayAdapterLocal::setUbootenv(const string & key, const string & value) {
    bool ret = false;
    UNUSED(key);
    UNUSED(value);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::getUbootenv(const string & key, string & value) {
    bool ret = false;
    UNUSED(key);
    UNUSED(value);
    NOTIMPLEMENTED;
    return ret;
}

DisplayAttributeInfo* DisplayAdapterLocal::getDisplayAttributeInfo(const string& name, ConnectorType displayType) {
    UNUSED(displayType);
    int i = 0;
    for (i = 0; i < DA_DISPLAY_ATTRIBUTE__COUNT; i++) {
        if (display_attrs[i].name == name) {
            return &(display_attrs[i]);
        }
    }
    DEBUG_INFO("Access invalid display attribute named \"%s\"", name.c_str());
    return NULL;
}

bool DisplayAdapterLocal::setDisplayAttribute(
        const string& name, const string& value, ConnectorType displayType) {
    bool ret = false;

    string out;
    DisplayAttributeInfo* info = getDisplayAttributeInfo(name, displayType);
    if (info && info->update_fun) {
       ret = info->update_fun(*info, value, out, UT_SET_VALUE);
    }
    if (ret == false) {
        DEBUG_INFO("Set display attribute \"%s\" fail", name.c_str());
    }
    return ret;
}

bool DisplayAdapterLocal::getDisplayAttribute(
        const string& name, string& value, ConnectorType displayType) {
    bool ret = false;
    string out = "";
    DisplayAttributeInfo* info = getDisplayAttributeInfo(name, displayType);
    if (info && info->update_fun) {
       if (info->update_fun(*info, "", out, UT_GET_VALUE)) {
           value = out;
           ret = true;
       }
    }
    if (ret == false) {
        DEBUG_INFO("Get display attribute \"%s\" fail", name.c_str());
    }
    return ret;
}

bool DisplayAdapterLocal::getDisplayVsyncAndPeriod(int64_t& vsyncTimestamp, int32_t& vsyncPeriodNanos) {
    UNUSED(vsyncTimestamp);
    UNUSED(vsyncPeriodNanos);
    NOTIMPLEMENTED;
    return false;
}

bool DisplayAdapterLocal::disableSidebandStream(bool isDisable){
    bool ret = false;
    UNUSED(isDisable);
    NOTIMPLEMENTED;
    return ret;
}

bool DisplayAdapterLocal::dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) {
    Json::Value ret;
    UNUSED(displayType);
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

bool DisplayAdapterLocal::setKeystoneCorrection(const string& params) {
    UNUSED(params);
    NOTIMPLEMENTED;
    return false;
}

bool DisplayAdapterLocal::setReverseMode(int type) {
    UNUSED(type);
    NOTIMPLEMENTED;
    return false;
}

std::shared_ptr<DisplayAdapter> DisplayAdapterLocal::create(DisplayAdapter::BackendType type) {
    switch (type) {
        case BackendType::DISPLAY_TYPE_DRM:
            return {nullptr};
        case BackendType::DISPLAY_TYPE_FBDEV:
            return static_cast<std::shared_ptr<DisplayAdapter>>(std::make_shared<DisplayAdapterLocal>());
        default:
            return static_cast<std::shared_ptr<DisplayAdapter>>(std::make_shared<DisplayAdapterLocal>());
    }
}

std::shared_ptr<DisplayAdapter> DisplayAdapterCreateLocal(DisplayAdapter::BackendType type) {
    return DisplayAdapterLocal::create(type);
};

}; //namespace meson
