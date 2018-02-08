/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#include "ConnectorHdmi.h"
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>
#include <utils/Tokenizer.h>
#include <MesonLog.h>

using namespace android;

ConnectorHdmi::ConnectorHdmi()
  : mPhyWidth(1920),
    mPhyHeight(1080),
    mWidth(1920),
    mHeight(1080),
    mDpiX(160),
    mDpiY(160),
    mFracRate(true),
    mRefreshRate(60),
    mConnected(true),
    mSecure(true),
    mDisplayMode("1080p60hz"),
    mDefaultDispMode("1080p60hz") {

}

ConnectorHdmi::~ConnectorHdmi()
{

}

int ConnectorHdmi::init()
{
    clearSupportedConfigs();
    mFramebufferContext = new FBContext();
    framebuffer_info_t* fbInfo = mFramebufferContext->getInfo();
    calcDefaultMode(*fbInfo, mDefaultDispMode);
    buildSingleConfigList(mDefaultDispMode);
    //updateActiveConfig(dispMode);
    mWidth = fbInfo->info.xres;
    mHeight = fbInfo->info.yres;
    return NO_ERROR;
}

drm_connector_type_t ConnectorHdmi::getType() {
    return DRM_MODE_CONNECTOR_HDMI;
}

uint32_t ConnectorHdmi::getModesCount() {
    std::vector<std::string> supportDispModes;
    readEdidList(supportDispModes);
     return supportDispModes.size();
}

bool ConnectorHdmi::isConnected() {
    bool bConnect = false;
    std::string dispMode;
    if (!readHdmiDispMode(dispMode)) {
        bConnect = isDispModeValid(dispMode);
    }

    MESON_LOGE("chkPresent %s", bConnect ? "connected" : "disconnected");
    return bConnect;

}


/* hdmi the common func begin
 *
 *                                                     */
bool ConnectorHdmi ::isDispModeValid(std::string & dispmode){
    MESON_LOGE("isDispModeValid %s", dispmode.c_str());
    if (dispmode.empty())
        return false;

    vmode_e mode = vmode_name_to_mode(dispmode.c_str());
    MESON_LOGE("isDispModeValid get mode (%d)", mode);
    if (mode == VMODE_MAX)
        return false;

    if (want_hdmi_mode(mode) == 0)
        return false;

    return true;
}

auto ConnectorHdmi::getSystemControlService() {
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

int ConnectorHdmi::readEdidList(std::vector<std::string>& edidlist) {
    auto scs = getSystemControlService();
    if (scs == NULL) {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
        return FAILED_TRANSACTION;
    }

#if PLATFORM_SDK_VERSION >= 26
    scs->getSupportDispModeList([&edidlist](const Result &ret, const hidl_vec<hidl_string> supportDispModes) {
        if (Result::OK == ret) {
            for (size_t i = 0; i < supportDispModes.size(); i++) {
                edidlist.push_back(supportDispModes[i]);
            }
        } else {
            edidlist.clear();
        }
    });

    if (edidlist.empty()) {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
        return FAILED_TRANSACTION;
    }

    return NO_ERROR;
#else
    if (scs->getSupportDispModeList(&edidlist)) {
        return NO_ERROR;
    } else {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
        return FAILED_TRANSACTION;
    }
#endif
}

int ConnectorHdmi::calcDefaultMode(framebuffer_info_t& framebufferInfo,
        std::string& defaultMode) {
    const struct vinfo_s * mode =
        findMatchedMode(framebufferInfo.info.xres, framebufferInfo.info.yres, 60);
    if (mode == NULL) {
        defaultMode = DEFAULT_DISPMODE;
    } else {
        defaultMode = mode->name;
    }

    defaultMode = DEFAULT_DISPMODE;

    MESON_LOGE("calcDefaultMode %s", defaultMode.c_str());
    return NO_ERROR;
}

int ConnectorHdmi::buildSingleConfigList(std::string& defaultMode) {
    if (!isDispModeValid(defaultMode)) {
        MESON_LOGE("buildSingleConfigList with invalidate mode (%s)", defaultMode.c_str());
        return false;
      }

    int ret = addSupportedConfig(defaultMode);

    return ret;
}

int ConnectorHdmi::readHdmiDispMode(std::string &dispmode) {
    if (mSC.get() && mSC->getActiveDispMode(&dispmode)) {
        MESON_LOGE("get current displaymode %s", dispmode.c_str());

        mDisplayMode = dispmode;
        if (!isDispModeValid(dispmode)) {
            MESON_LOGE("active mode %s not valid", dispmode.c_str());
            return BAD_VALUE;
        }
        return NO_ERROR;
    } else {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
        return FAILED_TRANSACTION;
    }
}

status_t ConnectorHdmi::readHdmiPhySize(framebuffer_info_t& fbInfo) {
    struct fb_var_screeninfo vinfo;
    if ((fbInfo.fd >= 0) && (ioctl(fbInfo.fd, FBIOGET_VSCREENINFO, &vinfo) == 0)) {
        if (int32_t(vinfo.width) > 16 && int32_t(vinfo.height) > 9) {
            mPhyWidth = vinfo.width;
            mPhyHeight = vinfo.height;
        }
        return NO_ERROR;
    }
    return BAD_VALUE;
}

bool ConnectorHdmi::isSecure() {

   auto scs = getSystemControlService();
    int status = 0;
    if (scs == NULL) {
        MESON_LOGE("syscontrol::isHDCPTxAuthSuccess FAIL.");
        return false;
    }

    #if PLATFORM_SDK_VERSION >= 26
    Result rtn = scs->isHDCPTxAuthSuccess();
    MESON_LOGE("hdcp status: %d", rtn);
    if (rtn == Result::OK)
       mSecure = true;
    else
       mSecure = false ;
    #else
    scs->isHDCPTxAuthSuccess(status);
    MESON_LOGE("hdcp status: %d", status);
    if (status == 1)
       mSecure =true;
    else
        mSecure =false;
    #endif

    return mSecure;

}

bool ConnectorHdmi::updateHotplug(bool connected,
        framebuffer_info_t& framebufferInfo) {
    bool ret = true;
    int32_t rate;

    if (!connected) {
        MESON_LOGE("hdmi disconnected, keep old display configs.");
        // return true;
    }

     updateDisplayAttributes(framebufferInfo);

    // MR : TODO : Clean up this hot plug logic. Updating the modes before reading
    //             hdmi modes is counter-intuitive. The intention here was to
    //             provide a default mode when no display is connected.
    if (updateSupportedConfigs() != NO_ERROR) {
        MESON_LOGE("updateHotplug: No supported display list, set default configs.");
        std::string dM (DEFAULT_DISPMODE);
        buildSingleConfigList(dM);
    }

    std::string activemode;
    if (readHdmiDispMode(activemode) != NO_ERROR) {
        std::string dM (DEFAULT_DISPMODE);
        MESON_LOGE("get active display mode failed.");
        //updateActiveConfig(dM);
        return false;
    }
    //updateActiveConfig(activemode);

    return true;
}

int ConnectorHdmi::updateDisplayAttributes(framebuffer_info_t& framebufferInfo) {
    if (readHdmiPhySize(framebufferInfo) != NO_ERROR) {
        mPhyWidth = mPhyHeight = 0;
    }
    MESON_LOGE("updateDisplayAttributes physical size (%d x %d)", mPhyWidth, mPhyHeight);
    return NO_ERROR;
}

int ConnectorHdmi::updateSupportedConfigs() {
    // clear display modes
    clearSupportedConfigs();

    std::vector<std::string> supportDispModes;
    std::string::size_type pos;
    std::string dM (mDefaultDispMode);

    bool isConfiged = readConfigFile("/system/etc/displayModeList.cfg", &supportDispModes);
    if (isConfiged) {
        MESON_LOGE("Read supported modes from cfg file.");
    } else {
        readEdidList(supportDispModes);
        if (supportDispModes.size() == 0) {
            MESON_LOGE("SupportDispModeList null!!!");
            return BAD_VALUE;
        }
    }
    for (size_t i = 0; i < supportDispModes.size(); i++) {
        if (!supportDispModes[i].empty()) {
            pos = supportDispModes[i].find('*');
            if (pos != std::string::npos) {
                supportDispModes[i].erase(pos, 1);
                MESON_LOGE("modify support display mode:%s", supportDispModes[i].c_str());
            }

            // skip default / fake active mode as we add it to the end
            if (supportDispModes[i] != dM)
                addSupportedConfig(supportDispModes[i]);
        }
    }

    addSupportedConfig(dM);
    return NO_ERROR;
}

bool ConnectorHdmi::readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes) {
    const char* WHITESPACE = " \t\r";

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(String8(configPath), &tokenizer);

    if (status) {
        MESON_LOGE("Error %d opening display config file %s.", status, configPath);
        return false;
    } else {
        while (!tokenizer->isEof()) {
            tokenizer->skipDelimiters(WHITESPACE);
            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {
                String8 token = tokenizer->nextToken(WHITESPACE);
                const char* dispMode = token.string();
                if (strstr(dispMode, "hz")) {
                    MESON_LOGE("dispMode %s.", dispMode);
                    (*supportDispModes).push_back(std::string(dispMode));
                }
            }

            tokenizer->nextLine();
        }
        delete tokenizer;
    }

    size_t num = (*supportDispModes).size();

    if (num <= 0) {
        return false;
    } else {
        return true;
    }
}

int ConnectorHdmi::addSupportedConfig(std::string& mode) {
    vmode_e vmode = vmode_name_to_mode(mode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        MESON_LOGE("addSupportedConfig meet error mode (%s, %d)", mode.c_str(), vmode);
        return BAD_VALUE;
    }

    int dpiX  = DEFAULT_DISPLAY_DPI, dpiY = DEFAULT_DISPLAY_DPI;
    if (mPhyWidth > 16 && mPhyHeight > 9) {
        dpiX = (vinfo->width  * 25.4f) / mPhyWidth;
        dpiY = (vinfo->height  * 25.4f) / mPhyHeight;
    }

    DisplayConfig *config = new DisplayConfig(mode,
                                            vinfo->sync_duration_num,
                                            vinfo->width,
                                            vinfo->height,
                                            dpiX,
                                            dpiY,
                                            false);

    // add normal refresh rate config, like 24hz, 30hz...
    MESON_LOGE("add display mode pair (%d, %s)", mSupportDispConfigs.size(), mode.c_str());
    mSupportDispConfigs.add(mSupportDispConfigs.size(), config);

    return NO_ERROR;
}

KeyedVector<int,DisplayConfig*> ConnectorHdmi::updateConnectedConfigs() {

    updateSupportedConfigs();
    return mSupportDispConfigs;

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

    memset(&mHdrCapabilities, 0, sizeof(hdr_capabilities_t));
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

int ConnectorHdmi::clearSupportedConfigs() {
    // reset display configs
    for (size_t i = 0; i < mSupportDispConfigs.size(); i++) {
        DisplayConfig *config = mSupportDispConfigs.valueAt(i);
        if (config)
            delete config;
    }
    mSupportDispConfigs.clear();
    return NO_ERROR;
}

void ConnectorHdmi:: dump(String8& dumpstr)
{
    updateSupportedConfigs();
    parseHdrCapabilities();
    dumpstr.appendFormat("Connector (HDMI, %s, %d, %d)\n",
    mDisplayMode.c_str(),
                 mSupportDispConfigs.size(),
                 1);
        dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        dumpstr.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");

        for (size_t i = 0; i < mSupportDispConfigs.size(); i++) {

            int mode = mSupportDispConfigs.keyAt(i);
            DisplayConfig *config = mSupportDispConfigs.valueAt(i);
            if (config) {
                dumpstr.appendFormat("%s %2d     |      %.3f      |   %5d   |   %5d    |"
                    "    %3d    |    %3d    \n",
                        (config->getDisplayMode() == mDisplayMode)? "   *":"    ",
                         mode,
                         config->getRefreshRate(),
                         config->getWidth(),
                         config->getHeight(),
                         config->getDpiX(),
                         config->getDpiY());
            }
        }
    // HDR info
    dumpstr.append("  HDR Capabilities:\n");
    dumpstr.append("    DolbyVision1=%zu\n", mHdrCapabilities.dvSupport?1:0);
    dumpstr.appendFormat("    HDR10=%zu, maxLuminance=%zu, avgLuminance=%zu, minLuminance=%zu\n",
        mHdrCapabilities.hdrSupport?1:0, mHdrCapabilities.maxLuminance, mHdrCapabilities.avgLuminance, mHdrCapabilities.minLuminance);

}
/*hdmi the common func end*/

