/*
// Copyright (c) 2017 Amlogic
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
*/

//#define LOG_NDEBUG 0
#include <HwcTrace.h>
#include <binder/IServiceManager.h>
#include <utils/Tokenizer.h>
#include "DisplayHdmi.h"
#include <pthread.h>

namespace android {
namespace amlogic {

#define DEFAULT_DISPMODE "1080p60hz"
#define DEFAULT_DISPLAY_DPI 160

DisplayHdmi::DisplayHdmi() {
#if 0 //debug only.
    //mAllModes.push_back("480p60hz");
    //mAllModes.push_back("576p50hz");
    //mAllModes.push_back("720p50hz");
    //mAllModes.push_back("720p60hz");
    mAllModes.push_back("1080p60hz");
    mAllModes.push_back("1080p50hz");
    mAllModes.push_back("1080p30hz");
    mAllModes.push_back("1080p25hz");
    mAllModes.push_back("1080p24hz");
    mAllModes.push_back("2160p24hz");
    mAllModes.push_back("2160p25hz");
    mAllModes.push_back("2160p30hz");
    mAllModes.push_back("2160p50hz");
    mAllModes.push_back("2160p60hz");
#endif

    if (Utils::get_bool_prop("ro.sf.full_activemode")) {
        mWorkMode = LOGIC_ACTIVEMODE;
    } else {
        mWorkMode = NONE_ACTIVEMODE;
    }
    mActiveConfigId = VMODE_MAX;
}

DisplayHdmi::~DisplayHdmi() {
}

void DisplayHdmi::initialize(framebuffer_info_t& framebufferInfo,
                                            DisplayNotify notifyFn, void* notifyData) {
    reset();
    std::string dispMode;
    calcDefaultMode(framebufferInfo, dispMode);
    buildSingleConfigList(dispMode);
    updateActiveConfig(dispMode);
    mFbWidth = framebufferInfo.info.xres;
    mFbHeight = framebufferInfo.info.yres;
    mNotifyFn = notifyFn;
    mNotifyData = notifyData;

    if (mWorkMode == NONE_ACTIVEMODE) {
        if (0 != pthread_create(&mDisplayMonitorThread,
                                                                NULL, monitorDisplayMode, (void*)this)) {
            ALOGE("ERROR: Display monitor thread create failed.");
        }
        mMonitorExit = false;
        mHotPlugComplete = false;
    }
}

void DisplayHdmi::deinitialize() {
    if (mWorkMode == NONE_ACTIVEMODE) {
        mMonitorExit = true;
        pthread_join(mDisplayMonitorThread, NULL);
    }

    reset();
}

void DisplayHdmi::reset() {
    clearSupportedConfigs();
    mActiveConfigStr.clear();
    mNotifyFn = NULL;
    mConnected = false;
    mActiveConfigId = VMODE_MAX;
    mPhyWidth = mPhyHeight = 0;
}

bool DisplayHdmi::updateHotplug(bool connected,
                                framebuffer_info_t* framebufferInfo) {
    Mutex::Autolock _l(mUpdateLock);
    bool ret = true;
    int32_t rate;
    mConnected = connected;

    if (!connected) {
        DTRACE("hdmi disconnected, keep old display configs.");
        mHotPlugComplete = true;
        return true;
    }

    // update sink information.
    readDisplayPhySize();

    if (updateDisplayConfigs() == NO_ERROR)
        mHotPlugComplete = true;
    else
        mHotPlugComplete = false;

    return true;
}

status_t DisplayHdmi::updateDisplayConfigs() {
    // read current hdmi display config.
    std::string activemode;
    if (readHdmiDispMode(activemode) != NO_ERROR) {
        //ETRACE("get active display mode failed.");
        mHotPlugComplete = false;
        return BAD_VALUE;
    }

    if (mActiveConfigStr.compare(activemode) != 0) {
        //ETRACE("active mode display mode differs(%s, %s)",
        //        mActiveConfigStr.c_str(), activemode.c_str());
       /*
        * TODO:
        * For active mode ,should HWC to take care all of the display things.
        * update Actvie config only happend when hwc::setactiveconfig called.
        * Now called here for first plugin init, it's a temp solution.
        */
        updateActiveConfig(activemode);
    }

    // update output capacity of new sink.
    if (updateSupportedConfigs() != NO_ERROR) {
        ETRACE("updateHotplug: No supported configs, use default configs");
        buildSingleConfigList(activemode);
    }

    return NO_ERROR;
}

int DisplayHdmi::updateSupportedConfigs() {
    if (mWorkMode == NONE_ACTIVEMODE) {
        /*
         * NONE_ACTIVEMODE:
         * Only one config, keep the size & mode id, update the dpi & vsync period.
         */
        DTRACE("Simple Active Mode!!!");
        DisplayConfig* curConfig = mSupportDispConfigs.valueAt(0);
        DisplayConfig* config = createConfigByModeStr(mActiveConfigStr);
        if (config && curConfig) {
            curConfig->mRefreshRate = config->mRefreshRate;
            curConfig->mDpiX = config->mDpiX;
            curConfig->mDpiY = config->mDpiY;
        }
        if (config)
            delete config;
        return NO_ERROR;
    }

    if (mWorkMode != REAL_ACTIVEMODE && mWorkMode != LOGIC_ACTIVEMODE)
        return BAD_VALUE;

    // clear display modes
    clearSupportedConfigs();

    std::vector<std::string> supportDispModes;
    std::string::size_type pos;

    bool isConfiged = readConfigFile("/system/etc/displayModeList.cfg", &supportDispModes);
    if (isConfiged) {
        DTRACE("Read supported modes from cfg file.");
    } else {
        readEdidList(supportDispModes);
        if (supportDispModes.size() == 0) {
            ETRACE("SupportDispModeList null!!!");
            return BAD_VALUE;
        }
    }

    for (size_t i = 0; i < supportDispModes.size(); i++) {
        if (!supportDispModes[i].empty()) {
            pos = supportDispModes[i].find('*');
            if (pos != std::string::npos) {
                supportDispModes[i].erase(pos, 1);
                DTRACE("modify support display mode:%s", supportDispModes[i].c_str());
            }

            DisplayConfig* config = createConfigByModeStr(supportDispModes[i]);
            if (config) {
                vmode_e vmode = vmode_name_to_mode(supportDispModes[i].c_str());
                DTRACE("add display mode pair (%d, %s)", vmode, supportDispModes[i].c_str());
                mSupportDispConfigs.add(vmode, config);
            }
        }
    }
    return NO_ERROR;
}

int DisplayHdmi::buildSingleConfigList(std::string& defaultMode) {
    if (!isDispModeValid(defaultMode)) {
        ETRACE("buildSingleConfigList with invalidate mode (%s)", defaultMode.c_str());
        return BAD_VALUE;
    }

    DisplayConfig* config = createConfigByModeStr(defaultMode);
    if (config) {
        vmode_e vmode = vmode_name_to_mode(defaultMode.c_str());
        DTRACE("add display mode pair (%d, %s)", vmode, defaultMode.c_str());
        mSupportDispConfigs.add(vmode, config);
    }

    return NO_ERROR;
}

int DisplayHdmi::calcDefaultMode(framebuffer_info_t& framebufferInfo,
    std::string& defaultMode) {
    const struct vinfo_s* mode =
        findMatchedMode(framebufferInfo.info.xres, framebufferInfo.info.yres, 60);
    if (mode == NULL) {
        defaultMode = DEFAULT_DISPMODE;
    } else {
        defaultMode = mode->name;
    }

    DTRACE("calcDefaultMode %s", defaultMode.c_str());
    return NO_ERROR;
}

DisplayConfig* DisplayHdmi::createConfigByModeStr(std::string & modeStr) {
    vmode_e vmode = vmode_name_to_mode(modeStr.c_str());
    const struct vinfo_s * vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        ETRACE("addSupportedConfigByModeStr meet error mode (%s, %d)", modeStr.c_str(), vmode);
        return NULL;
    }

    int dpiX  = DEFAULT_DISPLAY_DPI, dpiY = DEFAULT_DISPLAY_DPI;
    if (mPhyWidth > 0 && mPhyHeight > 0) {
        dpiX = (vinfo->width  * 25.4f) / mPhyWidth;
        dpiY = (vinfo->height  * 25.4f) / mPhyHeight;
    }

    DisplayConfig *config = new DisplayConfig(modeStr.c_str(),
        vinfo->sync_duration_num,
        vinfo->width,
        vinfo->height,
        dpiX,
        dpiY);

    return config;
}

int DisplayHdmi::updateActiveConfig(std::string& activeMode) {
   /*
     * For NONE_ACTIVEMODE:
     * 1) not update active config id(excpete first time), we use a fixed mode id.
     * 2) always update active config string.
     * For OTHER MODES:
     * 1) always update active config string & active config id.
     */
    if (mActiveConfigId == VMODE_MAX || mWorkMode != NONE_ACTIVEMODE) {
        mActiveConfigId = vmode_name_to_mode(activeMode.c_str());
    }
    mActiveConfigStr = activeMode;
    DTRACE("updateActiveConfig to (%s, %d)", mActiveConfigStr.c_str(), mActiveConfigId);
    return NO_ERROR;
}

int DisplayHdmi::setDisplayMode(const char* displaymode) {
    DTRACE("setDisplayMode to %s", displaymode);
    std::string strmode(displaymode);
    writeHdmiDispMode(strmode);
    updateActiveConfig(strmode);
    return NO_ERROR;
}

status_t DisplayHdmi::readDisplayPhySize() {
    struct vinfo_base_s info;
    if (read_vout_info(&info) == 0) {
        mPhyWidth = info.screen_real_width;
        mPhyHeight = info.screen_real_height;
    } else {
        mPhyWidth = mPhyHeight = 0;
    }
    DTRACE("readDisplayPhySize physical size (%d x %d)", mPhyWidth, mPhyHeight);
    return NO_ERROR;
}

int DisplayHdmi::getDisplayConfigs(uint32_t* outNumConfigs,
        hwc2_config_t* outConfigs) {
    size_t i;

    if (!isConnected()) {
        ETRACE("hdmi is not connected.");
    }

    *outNumConfigs = mSupportDispConfigs.size();

    if (NULL != outConfigs) {
        for (i = 0; i < mSupportDispConfigs.size(); i++) {
            outConfigs[i] = mSupportDispConfigs.keyAt(i);
        }
    }

    return NO_ERROR;
}

int DisplayHdmi::getDisplayAttribute(hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute,
        int32_t* outValue) {
    if (!isConnected()) {
        ETRACE("hdmi is not connected.");
    }

    DisplayConfig *configChosen = NULL;
    int modeIdx = mSupportDispConfigs.indexOfKey((vmode_e)config);
    if (modeIdx >= 0)
        configChosen = mSupportDispConfigs.valueAt(modeIdx);
    if (!configChosen) {
        ETRACE("failed to get display config: %d", config);
        return BAD_VALUE;
    }

    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            if (configChosen->getRefreshRate()) {
                *outValue = 1e9 / configChosen->getRefreshRate();
            }
        break;
        case HWC2_ATTRIBUTE_WIDTH:
            if (mWorkMode == REAL_ACTIVEMODE ||
                        mWorkMode == NONE_ACTIVEMODE) {
                *outValue = configChosen->getWidth();
            } else if (mWorkMode == LOGIC_ACTIVEMODE) {
                *outValue = mFbWidth;
            }
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
            if (mWorkMode == REAL_ACTIVEMODE ||
                        mWorkMode == NONE_ACTIVEMODE) {
                *outValue = configChosen->getHeight();
            } else if (mWorkMode == LOGIC_ACTIVEMODE) {
                *outValue = mFbHeight;
            }
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = configChosen->getDpiX() * 1000.0f;
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue =  configChosen->getDpiY() * 1000.0f;
        break;
        default:
            ETRACE("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return NO_ERROR;
}

int DisplayHdmi::getActiveConfig(hwc2_config_t* outConfig) {
    if (!isConnected()) {
        ETRACE("hdmi is not connected.");
    }
    //DTRACE("getActiveConfig to config(%d).", mActiveConfigId);
    *outConfig = mActiveConfigId;
    return NO_ERROR;
}

int DisplayHdmi::setActiveConfig(int modeId) {
    if (mWorkMode == NONE_ACTIVEMODE) {
        DTRACE("NONE_ACTVIEMODE, skip setactive config.");
        return NO_ERROR;
    }

    if (!isConnected()) {
        ETRACE("hdmi display is not connected.");
    }

    DTRACE("setActiveConfig to mode(%d).", modeId);
    int modeIdx = mSupportDispConfigs.indexOfKey((const vmode_e)modeId);
    if (modeIdx >= 0) {
        char *dispMode = NULL;
        dispMode = mSupportDispConfigs.valueAt(modeIdx)->getDisplayMode();
        DTRACE("setActiveConfig to (%d, %s).", modeId, dispMode);
        setDisplayMode(dispMode);
        return NO_ERROR;
    } else {
        ETRACE("set invalild active config (%d)", modeId);
        return BAD_VALUE;
    }
}

bool DisplayHdmi::readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes) {
    const char* WHITESPACE = " \t\r";

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(String8(configPath), &tokenizer);

    if (status) {
        DTRACE("Error %d opening display config file %s.", status, configPath);
        return false;
    } else {
        while (!tokenizer->isEof()) {
            tokenizer->skipDelimiters(WHITESPACE);
            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {
                String8 token = tokenizer->nextToken(WHITESPACE);
                const char* dispMode = token.string();
                if (strstr(dispMode, "hz")) {
                    ETRACE("dispMode %s.", dispMode);
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

int DisplayHdmi::clearSupportedConfigs() {
    // reset display configs
    for (size_t i = 0; i < mSupportDispConfigs.size(); i++) {
        DisplayConfig *config = mSupportDispConfigs.valueAt(i);
        if (config)
            delete config;
    }
    mSupportDispConfigs.clear();
    return NO_ERROR;
}

bool DisplayHdmi::chkPresent() {
    bool bConnect = false;
    std::string dispMode;
    if (!readHdmiDispMode(dispMode)) {
        bConnect = isDispModeValid(dispMode);
    }

    DTRACE("chkPresent %s", bConnect ? "connected" : "disconnected");
    return bConnect;
}

bool DisplayHdmi::isDispModeValid(std::string & dispmode){
    if (dispmode.empty())
        return false;

    vmode_e mode = vmode_name_to_mode(dispmode.c_str());
    //DTRACE("isDispModeValid get mode (%d)", mode);
    if (mode == VMODE_MAX)
        return false;

    if (want_hdmi_mode(mode) == 0)
        return false;

    return true;
}

bool DisplayHdmi::isHotplugComplete() {
    Mutex::Autolock _l(mUpdateLock);
    return mHotPlugComplete;
}

/*
  * ONLY FOR NONE_ACTIVEMODE. Monitor two cases:
  * 1) hotplug not update complete, for the displaymode not set completed by other modules.
  * 2) displaymode changed by other modules.
  */
void DisplayHdmi::monitorDisplayMode() {
    do {
        usleep(1000000);//1s
        Mutex::Autolock _l(mUpdateLock);
        std::string activemode;

        if (isConnected() && readHdmiDispMode(activemode) == NO_ERROR &&
                        (!mHotPlugComplete ||mActiveConfigStr.compare(activemode) != 0)) {
            //DTRACE("monitorDisplayMode update display information.");
            if (updateDisplayConfigs() == 0) {
                mHotPlugComplete = true;
                if (mNotifyFn)
                    mNotifyFn(mNotifyData);
            }
        }
    } while (!mMonitorExit);
}

void* DisplayHdmi::monitorDisplayMode(void *param) {
    if (param) {
        DisplayHdmi * pObj = (DisplayHdmi*)param;
        pObj->monitorDisplayMode();
    }

    return NULL;
}

sp<ISystemControlService> DisplayHdmi::getSystemControlService() {
    sp<ISystemControlService> systemControlService;
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ETRACE("Couldn't get default ServiceManager\n");
        return systemControlService;
    }

    systemControlService = interface_cast<ISystemControlService>(sm->getService(String16("system_control")));
    if (systemControlService == NULL) {
        ETRACE("Couldn't get connection to SystemControlService\n");
    }

    return systemControlService;
}

int DisplayHdmi::readHdmiDispMode(std::string &dispmode) {
    sp<ISystemControlService> syscontrol = getSystemControlService();
    if (syscontrol.get() && syscontrol->getActiveDispMode(&dispmode)) {
        //DTRACE("get current displaymode %s", dispmode.c_str());
        if (!isDispModeValid(dispmode)) {
            //DTRACE("active mode %s not valid", dispmode.c_str());
            return BAD_VALUE;
        }
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::getActiveDispMode FAIL.");
        return FAILED_TRANSACTION;
    }
}

int DisplayHdmi::writeHdmiDispMode(std::string &dispmode) {
    sp<ISystemControlService> syscontrol = getSystemControlService();
    if (syscontrol.get() && syscontrol->setActiveDispMode(dispmode)) {
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::setActiveDispMode FAIL.");
        return FAILED_TRANSACTION;
    }
}

int DisplayHdmi::readEdidList(std::vector<std::string>& edidlist) {
    sp<ISystemControlService> syscontrol = getSystemControlService();
    if (syscontrol.get() && syscontrol->getSupportDispModeList(&edidlist)) {
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::readEdidList FAIL.");
        return FAILED_TRANSACTION;
    }
}

void DisplayHdmi::dump(Dump& d) {
        d.append("Connector (HDMI, %s, %d, %d)\n", mActiveConfigStr.c_str(), mActiveConfigId, mWorkMode);
        d.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        d.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");
        for (size_t i = 0; i < mSupportDispConfigs.size(); i++) {
            vmode_e mode = mSupportDispConfigs.keyAt(i);
            DisplayConfig *config = mSupportDispConfigs.valueAt(i);
            if (config) {
                d.append("%s %2d     |       %4d       |   %5d   |    %5d    |"
                    "    %3d    |    %3d    \n",
                         (mode == (int)mActiveConfigId) ? "*   " : "    ",
                         mode,
                         config->getRefreshRate(),
                         config->getWidth(),
                         config->getHeight(),
                         config->getDpiX(),
                         config->getDpiY());
            }
        }
    }

} // namespace amlogic
} // namespace android
