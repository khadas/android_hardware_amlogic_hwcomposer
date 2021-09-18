/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "ConnectorPanel.h"
#include <HwDisplayCrtc.h>
#include <misc.h>
#include <MesonLog.h>
#include "AmFramebuffer.h"
#include "AmVinfo.h"
#include <fcntl.h>
#include <string>
#include <systemcontrol.h>
#include "Dv.h"

#define DV_SUPPORT_INFO_LEN_MAX (40)

ConnectorPanel::ConnectorPanel(int32_t drvFd, uint32_t id)
    :   HwDisplayConnectorFbdev(drvFd, id) {
    parseLcdInfo();
    if (mTabletMode) {
        snprintf(mName, 64, "Tablet-%d", id);
    } else {
        snprintf(mName, 64, "TV-%d", id);
    }
}

ConnectorPanel::~ConnectorPanel() {
}

int32_t ConnectorPanel::update() {
    loadPhysicalSize();
    loadDisplayModes();
    parseHdrCapabilities();
    return 0;
}

const char * ConnectorPanel::getName() {
    return mName;
}

drm_connector_type_t ConnectorPanel::getType() {
    return DRM_MODE_CONNECTOR_LVDS;
}

bool ConnectorPanel::isConnected(){
    return true;
}

bool ConnectorPanel::isSecure(){
    return true;
}

int32_t ConnectorPanel::parseLcdInfo() {
    /*
    typical info:
    "lcd vinfo:\n"
    "    lcd_mode:              %s\n"
    "    name:                  %s\n"
    "    mode:                  %d\n"
    "    width:                 %d\n"
    "    height:                %d\n"
    "    field_height:          %d\n"
    "    aspect_ratio_num:      %d\n"
    "    aspect_ratio_den:      %d\n"
    "    sync_duration_num:     %d\n"
    "    sync_duration_den:     %d\n"
    "    screen_real_width:     %d\n"
    "    screen_real_height:    %d\n"
    "    htotal:                %d\n"
    "    vtotal:                %d\n"
    "    fr_adj_type:           %d\n"
    "    video_clk:             %d\n"
    "    viu_color_fmt:         %d\n"
    "    viu_mux:               %d\n\n",
    */
#if BUILD_KERNEL_5_4 == true
    char val[PROP_VALUE_LEN_MAX];
    std::string lcdPath;
    mModeName = "panel";
    if (sys_get_string_prop("persist.vendor.hwc.lcdpath", val) > 0 && strcmp(val, "0") != 0) {
        lcdPath = "/sys/class/aml_lcd/lcd";
        lcdPath.append(val);
        lcdPath.append("/vinfo");
        mModeName.append(val);
    } else {
        lcdPath = "/sys/class/aml_lcd/lcd0/vinfo";
    }
    const char * lcdInfoPath = lcdPath.c_str();
#else
    const char * lcdInfoPath = "/sys/class/lcd/vinfo";
#endif

    const int valLenMax = 64;
    std::string lcdInfo;

    if (sc_read_sysfs(lcdInfoPath, lcdInfo) == 0 &&
        lcdInfo.size() > 0) {
       // MESON_LOGD("Lcdinfo:(%s)", lcdInfo.c_str());

        std::size_t lineStart = 0;

        /*parse lcd mode*/
        const char * modeStr = " lcd_mode: ";
        lineStart = lcdInfo.find(modeStr);
        lineStart += strlen(modeStr);
        std::string valStr = lcdInfo.substr(lineStart, valLenMax);
        MESON_LOGD("lcd_mode: value [%s]", valStr.c_str());
        if (valStr.find("tablet", 0) != std::string::npos) {
            mTabletMode = true;
        } else {
            mTabletMode = false;
        }

        if (mTabletMode) {
            /*parse display info mode*/
            const char * infoPrefix[] = {
                " width:",
                " height:",
                " sync_duration_num:",
                " sync_duration_den:",
                " screen_real_width:",
                " screen_real_height:",
            };
            const int infoValueIdx[] = {
                LCD_WIDTH,
                LCD_HEIGHT,
                LCD_SYNC_DURATION_NUM,
                LCD_SYNC_DURATION_DEN,
                LCD_SCREEN_REAL_WIDTH,
                LCD_SCREEN_REAL_HEIGHT,
            };
            static int infoNum = sizeof(infoValueIdx) / sizeof(int);

            MESON_LOGD("------------Lcdinfo parse start------------\n");
            for (int i = 0; i < infoNum; i ++) {
                lineStart = lcdInfo.find(infoPrefix[i], lineStart);
                lineStart += strlen(infoPrefix[i]);
                std::string valStr = lcdInfo.substr(lineStart, valLenMax);
                mLcdValues[infoValueIdx[i]] = (uint32_t)std::stoul(valStr);
                MESON_LOGD("[%s] : [%d]\n", infoPrefix[i], mLcdValues[infoValueIdx[i]]);
            }
            MESON_LOGD("------------Lcdinfo parse end------------\n");
        }
    } else {
        MESON_LOGE("parseLcdInfo ERROR.");
    }

    return 0;
}

int32_t ConnectorPanel::loadDisplayModes() {
    mDisplayModes.clear();

    if (mTabletMode) {
        uint32_t dpiX  = DEFAULT_DISPLAY_DPI, dpiY = DEFAULT_DISPLAY_DPI;
        if (mPhyWidth > 16 && mPhyHeight > 9) {
            dpiX = (LCD_WIDTH  * 25.4f) / LCD_SCREEN_REAL_WIDTH;
            dpiY = (LCD_HEIGHT  * 25.4f) / LCD_SCREEN_REAL_HEIGHT;
            MESON_LOGI("add display mode lcd real dpi (%d, %d)", dpiX, dpiY);
        }
        drm_mode_info_t modeInfo = {
            "",
            dpiX,
            dpiY,
            mLcdValues[LCD_WIDTH],
            mLcdValues[LCD_HEIGHT],
            (float)mLcdValues[LCD_SYNC_DURATION_NUM]/mLcdValues[LCD_SYNC_DURATION_DEN],
            0};
        strncpy(modeInfo.name, mModeName.c_str(), sizeof(mModeName));
        mDisplayModes.emplace(mDisplayModes.size(), modeInfo);
        MESON_LOGD("use default value,get display mode: %s", modeInfo.name);
    } else {
        std::string dispmode;
        int pipeidx = GET_PIPE_IDX_BY_ID(mCrtcId);
        if (0 == read_vout_mode(pipeidx, dispmode) ) {
            MESON_LOGD("ConnectorPanel current mode  (%s) . ", dispmode.c_str());
        } else {
            MESON_LOGE("ConnectorPanel current mod invalid. ");
        }

        addDisplayMode(dispmode);
        //for tv display mode.
        const unsigned int pos = dispmode.find("60hz", 0);
        if (pos != std::string::npos) {
            dispmode.replace(pos, 4, "50hz");
            addDisplayMode(dispmode);
        } else {
            MESON_LOGD("loadDisplayModes can not find 60hz in %s", dispmode.c_str());
        }
    }
    return 0;
}

int32_t ConnectorPanel::parseHdrCapabilities() {
    memset(&mHdrCapabilities, 0, sizeof(drm_hdr_capabilities));
    constexpr int sDefaultMinLumiance = 0;
    constexpr int sDefaultMaxLumiance = 500;

    mHdrCapabilities.DolbyVisionSupported = getDvSupportStatus();
    mHdrCapabilities.HLGSupported = true;
    mHdrCapabilities.HDR10Supported = true;
    mHdrCapabilities.maxLuminance = sDefaultMaxLumiance;
    mHdrCapabilities.avgLuminance = sDefaultMaxLumiance;
    mHdrCapabilities.minLuminance = sDefaultMinLumiance;

    return NO_ERROR;
}

void ConnectorPanel::getHdrCapabilities(drm_hdr_capabilities * caps) {

    if (caps) {
        *caps = mHdrCapabilities;
    }
}

void ConnectorPanel:: dump(String8& dumpstr) {
    dumpstr.appendFormat("Connector (Panel,  %d)\n",1);
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");

    std::map<uint32_t, drm_mode_info_t>::iterator it = mDisplayModes.begin();
    for ( ; it != mDisplayModes.end(); ++it) {
        dumpstr.appendFormat("   %6d   |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    \n",
                 it->first,
                 it->second.refreshRate,
                 it->second.pixelW,
                 it->second.pixelH,
                 it->second.dpiX,
                 it->second.dpiY);
    }
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");
}

