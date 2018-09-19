/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <fcntl.h>
#include <MesonLog.h>
#include <misc.h>
#include <systemcontrol.h>

#include "AmVinfo.h"
#include "ConnectorHdmi.h"

#define HDMI_FRAC_RATE_POLICY "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"
#define HDMI_TX_HPD_STATE   "/sys/class/amhdmitx/amhdmitx0/hpd_state"

ConnectorHdmi::ConnectorHdmi(int32_t drvFd, uint32_t id)
    :   HwDisplayConnector(drvFd, id) {
    mConnected = false;
    mSecure = false;

    snprintf(mName, 64, "HDMI-%d", id);
}

ConnectorHdmi::~ConnectorHdmi() {
}

int32_t ConnectorHdmi::loadProperities() {
    mConnected = checkConnectState();
    if (mConnected) {
        sc_get_hdmitx_hdcp_state(mSecure);
        loadPhysicalSize();
        loadDisplayModes();
        parseHdrCapabilities();
    }

    return 0;
}

const char * ConnectorHdmi::getName() {
    return mName;
}

drm_connector_type_t ConnectorHdmi::getType() {
    return DRM_MODE_CONNECTOR_HDMI;
}

bool ConnectorHdmi::isRemovable() {
    return true;
}

bool ConnectorHdmi::isConnected() {
    return mConnected;
}

bool ConnectorHdmi::isSecure() {
    return mSecure;
}

bool ConnectorHdmi::checkConnectState() {
    return sysfs_get_int(HDMI_TX_HPD_STATE, 0) == 1 ? true : false;
}

int32_t ConnectorHdmi::loadDisplayModes() {
    std::vector<std::string> supportDispModes;
    std::string::size_type pos;

    if (sc_get_hdmitx_mode_list(supportDispModes) < 0) {
        MESON_LOGE("SupportDispModeList null!!!");
        return -ENOENT;
    }

    mDisplayModes.clear();
    for (size_t i = 0; i < supportDispModes.size(); i++) {
        if (!supportDispModes[i].empty()) {
            pos = supportDispModes[i].find('*');
            if (pos != std::string::npos) {
                supportDispModes[i].erase(pos, 1);
                //MESON_LOGE("modify support display mode:%s", supportDispModes[i].c_str());
            }

            addDisplayMode(supportDispModes[i]);
        }
    }

    return 0;
}

int32_t ConnectorHdmi::getModes(std::map<uint32_t, drm_mode_info_t> & modes) {
    return HwDisplayConnector::getModes(modes);
}

void ConnectorHdmi::getHdrCapabilities(drm_hdr_capabilities * caps) {
    if (caps) {
        *caps = mHdrCapabilities;
    }
}

void ConnectorHdmi::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Connector (HDMI, %d x %d, %s, %s)\n",
        mPhyWidth, mPhyHeight, isSecure() ? "secure" : "unsecure",
        isConnected() ? "Connected" : "Removed");

    //dump display config.
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");

    std::map<uint32_t, drm_mode_info_t>::iterator it = mDisplayModes.begin();
    for ( ; it != mDisplayModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    \n",
                 it->first,
                 it->second.refreshRate,
                 it->second.pixelW,
                 it->second.pixelH,
                 it->second.dpiX,
                 it->second.dpiY);
    }

    // HDR info
    dumpstr.append("  HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%d\n",
        mHdrCapabilities.DolbyVisionSupported ? 1 : 0);
    dumpstr.appendFormat("    HDR10=%d, maxLuminance=%d,"
        "avgLuminance=%d, minLuminance=%d\n",
        mHdrCapabilities.HDR10Supported ? 1 : 0,
        mHdrCapabilities.maxLuminance,
        mHdrCapabilities.avgLuminance,
        mHdrCapabilities.minLuminance);
}

int32_t ConnectorHdmi::getLineValue(const char *lineStr, const char *magicStr) {
    int len = 0;
    char value[100] = {0};
    const char *pos = NULL;

    if ((NULL == lineStr) || (NULL == magicStr)) {
        MESON_LOGE("line string: %s, magic string: %s\n", lineStr, magicStr);
        return 0;
    }

    if (NULL != (pos = strstr(lineStr, magicStr))) {
        pos = pos + strlen(magicStr);
        const char* start = pos;
        while (*start != '\n' && (strlen(start) > 0))
            start++;

        len = start - pos;
        strncpy(value, pos, len);
        value[len] = '\0';
        return atoi(value);
    }

    return 0;
}

/*******************************************
* cat /sys/class/amhdmitx/amhdmitx0/hdr_cap
* Supported EOTF:
*     Traditional SDR: 1
*     Traditional HDR: 0
*     SMPTE ST 2084: 1
*     Future EOTF: 0
* Supported SMD type1: 1
* Luminance Data
*     Max: 0
*     Avg: 0
*     Min: 0
* cat /sys/class/amhdmitx/amhdmitx0/dv_cap
* DolbyVision1 RX support list:
*     2160p30hz: 1
*     global dimming
*     colorimetry
*     IEEEOUI: 0x00d046
*     DM Ver: 1
*******************************************/
int32_t ConnectorHdmi::parseHdrCapabilities() {
    // DolbyVision1
    const char *DV_PATH = "/sys/class/amhdmitx/amhdmitx0/dv_cap";
    // HDR
    const char *HDR_PATH = "/sys/class/amhdmitx/amhdmitx0/hdr_cap";

    char buf[1024+1] = {0};
    char* pos = buf;
    int fd, len;

    memset(&mHdrCapabilities, 0, sizeof(drm_hdr_capabilities));
    if ((fd = open(DV_PATH, O_RDONLY)) < 0) {
        MESON_LOGE("open %s fail.", DV_PATH);
        goto exit;
    }

    len = read(fd, buf, 1024);
    if (len < 0) {
        MESON_LOGE("read error: %s, %s\n", DV_PATH, strerror(errno));
        goto exit;
    }
    close(fd);

    if ((NULL != strstr(pos, "2160p30hz")) || (NULL != strstr(pos, "2160p60hz")))
        mHdrCapabilities.DolbyVisionSupported= true;
    // dobly version parse end

    memset(buf, 0, 1024);
    if ((fd = open(HDR_PATH, O_RDONLY)) < 0) {
        MESON_LOGE("open %s fail.", HDR_PATH);
        goto exit;
    }

    len = read(fd, buf, 1024);
    if (len < 0) {
        MESON_LOGE("read error: %s, %s\n", HDR_PATH, strerror(errno));
        goto exit;
    }

    pos = strstr(pos, "SMPTE ST 2084: ");
    if ((NULL != pos) && ('1' == *(pos + strlen("SMPTE ST 2084: ")))) {
        mHdrCapabilities.HDR10Supported = true;

        mHdrCapabilities.maxLuminance = getLineValue(pos, "Max: ");
        mHdrCapabilities.avgLuminance = getLineValue(pos, "Avg: ");
        mHdrCapabilities.minLuminance = getLineValue(pos, "Min: ");
    }

    MESON_LOGD("dolby version support:%d, hdr support:%d max:%d, avg:%d, min:%d\n",
        mHdrCapabilities.DolbyVisionSupported ? 1:0,
        mHdrCapabilities.HDR10Supported ? 1:0,
        mHdrCapabilities.maxLuminance,
        mHdrCapabilities.avgLuminance,
        mHdrCapabilities.minLuminance);

exit:
    close(fd);
    return NO_ERROR;
}

