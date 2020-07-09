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
namespace meson{
//For recovery mode for temporary.
#define SYSFS_DISPLAY_MODE              "/sys/class/display/mode"
#define SYSFS_DISPLAY_MODE2             "/sys/class/display2/mode"
#define MAX_BUFFER_LEN_EDID             4096
#define READ_BUFFER_LEN 64
#define FORMAT_DISPLAY_HDMI_EDID        "/sys/class/amhdmitx/amhdmitx0/disp_cap"//RX support display mode

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
    DisplayModeInfo mode;
    if (displayType == DisplayAdapter::CONN_TYPE_HDMI) {
        displayModeList.clear();
        char edid_buf[MAX_BUFFER_LEN_EDID];
        const char *delim = "\n";
        char *ptr = NULL;
        if (false == GetHDMIEDID(edid_buf, MAX_BUFFER_LEN_EDID)) {
            return false;
        }
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
    mode = buf;
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

    FS_WRITE(path, mode.c_str(), mode.length());

    return true;
error_handle:
    return false;
};

bool DisplayAdapterLocal::captureDisplayScreen(const native_handle_t **outBufferHandle) {
    *outBufferHandle = nullptr;
    return true;
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

DisplayAttributeInfo* DisplayAdapterLocal::getDisplayAttributeInfo(const string& name, ConnectorType displayType) {
    UNUSED(name);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return NULL;
}

bool DisplayAdapterLocal::setDisplayAttribute(
        const string& name, const string& value, ConnectorType displayType) {
    UNUSED(name);
    UNUSED(value);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return false;
}

bool DisplayAdapterLocal::getDisplayAttribute(
        const string& name, string& value, ConnectorType displayType) {
    UNUSED(name);
    UNUSED(value);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return false;
}

bool DisplayAdapterLocal::dumpDisplayAttribute(Json::Value& json, ConnectorType displayType) {
    UNUSED(json);
    UNUSED(displayType);
    NOTIMPLEMENTED;
    return false;
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
};

}; //namespace meson
