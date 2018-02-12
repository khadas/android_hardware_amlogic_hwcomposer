/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <utils/Errors.h>
#include <HwDisplayConnector.h>
#include "ConnectorPanel.h"
#include <MesonLog.h>

ConnectorPanel::ConnectorPanel() {
}

ConnectorPanel::~ConnectorPanel() {
}

int ConnectorPanel::init(){
    return NO_ERROR;
}

drm_connector_type_t ConnectorPanel::getType() {
    return DRM_MODE_CONNECTOR_PANEL;
}

bool ConnectorPanel::isConnected(){
    return true;
}

bool ConnectorPanel::isSecure(){
    return true;
}

uint32_t ConnectorPanel::getModesCount(){
    return 1;
}

auto ConnectorPanel::getSystemControlService() {
    static bool bGot = false;

#if PLATFORM_SDK_VERSION >= 26
    sp<ISystemControl> systemControl = ISystemControl::getService();

    if (bGot)
        return systemControl;

    mDeathRecipient = new SystemControlDeathRecipient();
    Return<bool> linked = systemControl->linkToDeath(mDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        MESON_LOGE("Transaction error in linking to system service death: %s", linked.description().c_str());
    } else if (!linked) {
        MESON_LOGE("Unable to link to system service death notifications");
    } else {
        MESON_LOGV("Link to system service death notification successful");
    }

#else
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        MESON_LOGE("Couldn't get default ServiceManager\n");
        return NULL;
    }
    sp<ISystemControlService> systemControl = interface_cast<ISystemControlService>(sm->getService(String16("system_control")));

    if (bGot)
        return systemControl;

    if (systemControl == NULL) {
        MESON_LOGE("Couldn't get connection to SystemControlService\n");
        return NULL;
    }
#endif

    bGot = true;
    return systemControl;
}


std::string ConnectorPanel::readDispMode(std::string &displaymode) {
    auto scs = getSystemControlService();
    if (scs == NULL) {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
    }

#if PLATFORM_SDK_VERSION >= 26
    scs->getActiveDispMode([&displaymode](const Result &ret, const hidl_string&supportDispModes) {
        if (Result::OK == ret) {
            displaymode = supportDispModes.c_str();
        } else {
            MESON_LOGE("syscontrol::getActiveDispMode Error");
        }
    });

    if (displaymode.empty()) {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
    }

#else
    if (scs->getActiveDispMode(&displaymode)) {
    } else {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
    }
#endif
    mDisplayMode = displaymode;
    return mDisplayMode;

}


KeyedVector<int,DisplayConfig*> ConnectorPanel::getModesInfo() {
    std::string dispmode;
    readPhySize();
    readDispMode(dispmode);

    MESON_LOGE("get current displaymode %s", mDisplayMode.c_str());
    vmode_e vmode = vmode_name_to_mode(mDisplayMode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        MESON_LOGE("addSupportedConfig meet error mode (%s, %d)", mDisplayMode.c_str(), vmode);
}

    int dpiX  = DEFAULT_DISPLAY_DPI, dpiY = DEFAULT_DISPLAY_DPI;
    if (mPhyWidth > 16 && mPhyHeight > 9) {

        dpiX = (vinfo->width  * 25.4f) / mPhyWidth;
        dpiY = (vinfo->height  * 25.4f) / mPhyHeight;
    }

        mconfig = new DisplayConfig(mDisplayMode,

                                            vinfo->sync_duration_num,
                                            vinfo->width,
                                            vinfo->height,
                                            dpiX,
                                            dpiY,
                                            false);

    // add normal refresh rate config, like 24hz, 30hz...
        MESON_LOGE("add display mode pair (%d, %s)", mSupportDispConfigs.size(), mDisplayMode.c_str());
        mSupportDispConfigs.add(mSupportDispConfigs.size(), mconfig);

    return mSupportDispConfigs;
}

status_t ConnectorPanel::readPhySize() {
            mPhyWidth = DEFAULT_DISPLAY_DPI;
            mPhyHeight = DEFAULT_DISPLAY_DPI;
    return NO_ERROR;
}

int32_t ConnectorPanel::getLineValue(const char *lineStr, const char *magicStr) {
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
int32_t ConnectorPanel::parseHdrCapabilities() {
    // DolbyVision1
    const char *DV_PATH = "/sys/class/amhdmitx/amhdmitx0/dv_cap";
    // HDR
    const char *HDR_PATH = "/sys/class/amhdmitx/amhdmitx0/hdr_cap";

    char buf[1024+1] = {0};
    char* pos = buf;
    int fd, len;

    memset(&mHdrCapabilities, 0, sizeof(hdr_dev_capabilities_t));
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
        mHdrCapabilities.dvSupport = true;
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
        mHdrCapabilities.hdrSupport = true;

        mHdrCapabilities.maxLuminance = getLineValue(pos, "Max: ");
        mHdrCapabilities.avgLuminance = getLineValue(pos, "Avg: ");
        mHdrCapabilities.minLuminance = getLineValue(pos, "Min: ");
    }

    MESON_LOGE("dolby version support:%d, hdr support:%d max:%d, avg:%d, min:%d\n",
        mHdrCapabilities.dvSupport?1:0, mHdrCapabilities.hdrSupport?1:0, mHdrCapabilities.maxLuminance, mHdrCapabilities.avgLuminance, mHdrCapabilities.minLuminance);
exit:
    close(fd);
    return NO_ERROR;
}


void ConnectorPanel:: dump(String8& dumpstr) {
    parseHdrCapabilities();
    getModesInfo();
    dumpstr.appendFormat("Connector (Panel, %d, %d)\n",
                 getModesCount(),
                 1);
        dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        dumpstr.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");

            int mode = mSupportDispConfigs.keyAt(0);
            DisplayConfig *config = mSupportDispConfigs.valueAt(0);
            if (config) {
                dumpstr.appendFormat(" %2d     |      %.3f      |   %5d   |   %5d    |"
                    "    %3d    |    %3d    \n",
                         mode,
                         config->getRefreshRate(),
                         config->getWidth(),
                         config->getHeight(),
                         config->getDpiX(),
                         config->getDpiY());
            }
    // HDR info
    dumpstr.append("  HDR Capabilities:\n");
    dumpstr.appendFormat("    DolbyVision1=%zu\n", mHdrCapabilities.dvSupport?1:0);
    dumpstr.appendFormat("    HDR10=%zu, maxLuminance=%zu, avgLuminance=%zu, minLuminance=%zu\n",
        mHdrCapabilities.hdrSupport?1:0, mHdrCapabilities.maxLuminance, mHdrCapabilities.avgLuminance, mHdrCapabilities.minLuminance);


}

