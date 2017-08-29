//#define LOG_NDEBUG 0
#include <HwcTrace.h>
#include <binder/IServiceManager.h>
#include <utils/Tokenizer.h>
#include <thread>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include "DisplayHdmi.h"
#include <cutils/properties.h>

namespace android {
namespace amlogic {

void DisplayHdmi::SystemControlDeathRecipient::serviceDied(
        uint64_t, const wp<::android::hidl::base::V1_0::IBase>&) {
    ETRACE("system_control died, need reconnect it\n");
}

DisplayHdmi::DisplayHdmi(hwc2_display_t id) {
    mDisplayId = id;

    mTrebleSystemControlEnable = property_get_bool("persist.system_control.treble", true);

    initModes();
}

DisplayHdmi::~DisplayHdmi() {
    mAllModes.clear();
}

void DisplayHdmi::initBinderService() {
    mTrebleSystemControl = nullptr;
    sp<ISystemControl> control = nullptr;

    if (mTrebleSystemControlEnable) {
        control = ISystemControl::getService();
        mDeathRecipient = new SystemControlDeathRecipient();
        Return<bool> linked = control->linkToDeath(mDeathRecipient, /*cookie*/ 0);
        if (!linked.isOk()) {
            ETRACE("Transaction error in linking to system service death: %s", linked.description().c_str());
        } else if (!linked) {
            ETRACE("Unable to link to system service death notifications");
        } else {
            ITRACE("Link to system service death notification successful");
        }

        mTrebleSystemControl = control;
    }
    else {
        sp<IServiceManager> sm = defaultServiceManager();
        if (sm == NULL) {
            ETRACE("Couldn't get default ServiceManager\n");
            return ;
        }

        mSystemControlService = interface_cast<ISystemControlService>(sm->getService(String16("system_control")));
        if (mSystemControlService == NULL) {
            ETRACE("Couldn't get connection to SystemControlService\n");
            return ;
        }
    }
}

void DisplayHdmi::initialize() {
    reset();
}

void DisplayHdmi::deinitialize() {
    reset();
}

void DisplayHdmi::reset() {
    mConnected = false;
    mActiveDisplayConfigItem = 0;
    mActiveRefreshRate = 60;
    memset(mActiveDisplaymode, 0, HWC_DISPLAY_MODE_LENGTH);
    mSupportDispModes.clear();
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs[i];
        if (config)
            delete config;
    }
    mDisplayConfigs.clear();
}

bool DisplayHdmi::updateHotplug(bool connected,
    framebuffer_info_t * framebufferInfo,
    private_handle_t* framebufferHnd) {
    bool ret = true;
    int32_t rate;
    initBinderService();

    mConnected = connected;

    if (!isConnected()) {
        ETRACE("disp: %d disconnect", (int32_t)mDisplayId);
        return true;
    }

    mFramebufferInfo = framebufferInfo;
    mFramebufferHnd = framebufferHnd;

    if (-1 == updateDisplayModeList()) {
         //hdmi plug out when system is starting up
        std::string dispMode;
        int width, height;
        if (mTrebleSystemControlEnable) {
            mTrebleSystemControl->getActiveDispMode([&dispMode](const Result &ret, const hidl_string& mode) {
                if (Result::OK == ret) {
                    dispMode = mode.c_str();
                }
            });
        }
        else {
            mSystemControlService->getActiveDispMode(&dispMode);
        }

        ret = calcMode2Config(dispMode.c_str(), &rate, &width, &height);
        if (!ret) {
            dispMode = std::string("1080p60hz");
            rate = 60;
        }
        ETRACE("only init one display config: %s", dispMode.c_str());
        strcpy(mActiveDisplaymode, dispMode.c_str());

        // reset display configs
        for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
            DisplayConfig *config = mDisplayConfigs[i];
            if (config)
                delete config;
        }
        mDisplayConfigs.clear();

        // use active fb dimension as config width/height
        DisplayConfig *config = new DisplayConfig(mActiveDisplaymode,
                                          rate,
                                          mFramebufferInfo->info.xres,
                                          mFramebufferInfo->info.yres,
                                          mFramebufferInfo->xdpi,
                                          mFramebufferInfo->ydpi);
        // add it to the front of other configs
        mDisplayConfigs.push_back(config);

        // init the active display config
        mActiveDisplayConfigItem = 0;
        mActiveRefreshRate = rate;
        //ETRACE("Active display mode %s, refresh rate: %d", mActiveDisplaymode, rate);
    } else {
        //hdmi plug in when system is starting up
        updateActiveDisplayMode();
        updateDisplayConfigures();
        updateActiveDisplayConfigure();

        std::string strmode(mActiveDisplaymode);
        if (mTrebleSystemControlEnable) {
            hidl_string mode(strmode);
            Result ret = mTrebleSystemControl->setActiveDispMode(mode);
            if (Result::OK == ret) {
            }
        }
        else {
            mSystemControlService->setActiveDispMode(strmode);
        }
    }

    std::thread t1(&DisplayHdmi::setSurfaceFlingerActiveMode, this);
    t1.detach();

    return true;
}

int DisplayHdmi::updateDisplayModeList() {
    // clear display modes
    mSupportDispModes.clear();

    bool fullActiveMode = Utils::get_bool_prop("ro.sf.full_activemode");
    bool isConfiged = readConfigFile("/vendor/etc/displayModeList.cfg", &mSupportDispModes);
    if (isConfiged) {
        return 0;
    }

    if (!fullActiveMode) {
        ALOGD("Simple Active Mode!!!");
        return -1;
    }

    std::vector<std::string> getSupportDispModes;
    std::string::size_type pos;
    if (mTrebleSystemControlEnable) {
        mTrebleSystemControl->getSupportDispModeList(
            [&getSupportDispModes](const Result &ret, const hidl_vec<hidl_string>& modeList) {
            if (Result::OK == ret) {
                for (size_t i = 0; i < modeList.size(); i++) {
                    getSupportDispModes.push_back(modeList[i]);
                }
            }
        });
    }
    else {
        mSystemControlService->getSupportDispModeList(&getSupportDispModes);
    }

    if (getSupportDispModes.size() == 0) {
        ALOGD("SupportDispModeList null!!!");
        return -1;
    }

    for (size_t i = 0; i < getSupportDispModes.size(); i++) {
        //ALOGD("get support display mode:%s", getSupportDispModes[i].c_str());
        while (!getSupportDispModes[i].empty()) {
            pos = getSupportDispModes[i].find('*');
            if (pos != std::string::npos) {
                getSupportDispModes[i].erase(pos, 1);
                //ALOGD("modify support display mode:%s", getSupportDispModes[i].c_str());
            } else {
                break;
            }
        }
    }


    for (size_t k = 0; k < mAllModes.size(); k++) {
        for (size_t j = 0; j < getSupportDispModes.size(); j++) {
            if (!getSupportDispModes[j].empty()) {
                if (mAllModes[k] == getSupportDispModes[j]) {
                    mSupportDispModes.push_back(getSupportDispModes[j]);
                    ALOGD("support display mode:%s", getSupportDispModes[j].c_str());
                }
            }
        }
    }

    return 0;
}

int DisplayHdmi::updateActiveDisplayMode() {
    std::string dispMode;

    if (mTrebleSystemControlEnable) {
        mTrebleSystemControl->getActiveDispMode([&dispMode](const Result &ret, const hidl_string& mode) {
            if (Result::OK == ret) {
                dispMode = mode.c_str();
            }
        });
    }
    else {
        mSystemControlService->getActiveDispMode(&dispMode);
    }

    strcpy(mActiveDisplaymode, dispMode.c_str());

    int refreshRate = 60;
    if (strstr(mActiveDisplaymode, "60hz") != NULL) {
        refreshRate = 60;
    } else if (strstr(mActiveDisplaymode, "50hz") != NULL) {
        refreshRate = 50;
    } else if (strstr(mActiveDisplaymode, "30hz") != NULL) {
        refreshRate = 30;
    } else if (strstr(mActiveDisplaymode, "25hz") != NULL) {
        refreshRate = 25;
    } else if ((strstr(mActiveDisplaymode, "24hz") != NULL)
                    || (strstr(mActiveDisplaymode, "smpte") != NULL)) {
        refreshRate = 24;
    } else
        ETRACE("displaymode (%s) doesn't  specify HZ", mActiveDisplaymode);

    ALOGD("Active display mode: (%s), refresh rate: (%d)", mActiveDisplaymode, refreshRate);

    mActiveRefreshRate = refreshRate;

    return 0;
}

int DisplayHdmi::setDisplayMode(const char* displaymode) {
    ALOGD("setDisplayMode to %s", displaymode);

    std::string strmode(displaymode);
    if (mTrebleSystemControlEnable) {
        hidl_string mode(strmode);
        Result ret = mTrebleSystemControl->setActiveDispMode(mode);
        if (Result::OK == ret) {
        }
    }
    else {
        mSystemControlService->setActiveDispMode(strmode);
    }

    updateActiveDisplayMode();

    return 0;
}

int DisplayHdmi::updateDisplayConfigures() {
    size_t i;

    std::string dispMode;
    int refreshRate, width, height;

    // reset display configs
    for (i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs[i];
        if (config)
            delete config;
    }
    mDisplayConfigs.clear();

    for (i =0; i < mSupportDispModes.size(); i ++) {
        dispMode = mSupportDispModes[i];
        calcMode2Config(dispMode.c_str(), &refreshRate, &width, &height);

        // init dimension as config width/height, set xdpi/ydpi after
        DisplayConfig *config = new DisplayConfig(dispMode.c_str(),
            refreshRate, width, height,
            mFramebufferInfo->xdpi,
            mFramebufferInfo->ydpi);
        // add it to the front of other configs
        mDisplayConfigs.push_back(config);
    }
    return 0;
}

int DisplayHdmi::updateActiveDisplayConfigure() {
    size_t i;

    DisplayConfig *dispConfig = NULL;
    for (i = 0; i < mDisplayConfigs.size(); i++) {
        dispConfig = mDisplayConfigs[i];
        if (!dispConfig) {
            continue;
        }
        if (0 == strncmp(mActiveDisplaymode, dispConfig->getDisplayMode(),
            HWC_DISPLAY_MODE_LENGTH-1)) {
            mActiveDisplayConfigItem = i;
            ALOGD("updateActiveDisplayConfigure to config(%d)", mActiveDisplayConfigItem);
            dispConfig->setDpi(mFramebufferInfo->xdpi, mFramebufferInfo->ydpi);
            break;
        }
    }
    return 0;
}

int DisplayHdmi::getDisplayConfigs(uint32_t* outNumConfigs,
        hwc2_config_t* outConfigs) {
    size_t i;

    if (!isConnected()) {
        //ETRACE("display %d is not connected.", (int32_t)mDisplayId);
    }

    for (i = 0; i < mDisplayConfigs.size(); i++) {
        if (NULL != outConfigs)
            outConfigs[i] = i;
    }

    *outNumConfigs = i;

    return HWC2_ERROR_NONE;
}

int DisplayHdmi::getDisplayAttribute(hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute,
        int32_t* outValue) {

    if (!isConnected()) {
        //ETRACE("display %d is not connected.", (int32_t)mDisplayId);
    }

    DisplayConfig *configChosen = mDisplayConfigs[config];
    if  (!configChosen) {
        ETRACE("failed to get display config: %d", config);
        return HWC2_ERROR_NONE;
    }

    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            if (configChosen->getRefreshRate()) {
                *outValue = 1e9 / configChosen->getRefreshRate();
            }
        break;
        case HWC2_ATTRIBUTE_WIDTH:
            *outValue = configChosen->getWidth();
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
            *outValue = configChosen->getHeight();
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = configChosen->getDpiX() * 1000.0f;
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = configChosen->getDpiY() * 1000.0f;
        break;
        default:
            ETRACE("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return HWC2_ERROR_NONE;
}

int DisplayHdmi::getActiveConfig(hwc2_config_t* outConfig) {
    if (!isConnected()) {
        //ETRACE("display %d is not connected.", (int32_t)mDisplayId);
    }
    //ALOGD("getActiveConfig to config(%d).", mActiveDisplayConfigItem);
    *outConfig = mActiveDisplayConfigItem;

    return HWC2_ERROR_NONE;
}

int DisplayHdmi::setActiveConfig(int id) {
    DisplayConfig *dispConfig = NULL;
    char *dispMode = NULL;

    if (!isConnected()) {
        //ETRACE("display %d is not connected.", (int32_t)mDisplayId);
    }

    ALOGD("setActiveConfig to config(%d).", id);
    mActiveDisplayConfigItem = id;
    dispConfig = mDisplayConfigs[id];
    if  (!dispConfig) {
        ETRACE("failed to get display config: %d", id);
        return HWC2_ERROR_BAD_CONFIG;
    }

    dispConfig->setDpi(mFramebufferInfo->xdpi,
        mFramebufferInfo->ydpi);

    dispMode = dispConfig->getDisplayMode();
    if  (!dispMode) {
        ETRACE("failed to get display mode by config: %d", id);
        return HWC2_ERROR_BAD_CONFIG;
    }

    setDisplayMode(dispMode);

    return HWC2_ERROR_NONE;
}

bool DisplayHdmi::calcMode2Config(const char *dispMode, int* refreshRate,
    int* width, int* height) {

    if (NULL == dispMode) {
        ETRACE("dispMode is NULL");
        return false;
    }

    if (strstr(dispMode, "60hz") != NULL) {
        *refreshRate = 60;
    } else if (strstr(dispMode, "50hz") != NULL) {
        *refreshRate = 50;
    } else if (strstr(dispMode, "30hz") != NULL) {
        *refreshRate = 30;
    } else if (strstr(dispMode, "25hz") != NULL) {
        *refreshRate = 25;
    } else if ((strstr(dispMode, "24hz") != NULL)
                    || (strstr(dispMode, "smpte") != NULL)) {
        *refreshRate = 24;
    } else {
        ETRACE("displaymode (%s) doesn't  specify HZ", dispMode);
        return false;
    }

    if (strstr(dispMode, "2160") != NULL) {
        *width = 3840;
        *height = 2160;
    } else if (strstr(dispMode, "1080") != NULL) {
        *width = 1920;
        *height = 1080;
    } else if (strstr(dispMode, "720") != NULL) {
        *width = 1280;
        *height = 720;
    } else if (strstr(dispMode, "576") != NULL) {
        *width = 720;
        *height = 576;
    } else if (strstr(dispMode, "480") != NULL) {
        *width = 640;
        *height = 480;
    } else {
       // smpte and panle imcomplete!!!!!
        ETRACE("calcMode2Config displaymode (%s) doesn't  specify HZ", dispMode);
        return false;
    }

    //DTRACE("calcMode2Config (%s) refreshRate(%d), (%dx%d)", dispMode, *refreshRate, *width, *height);
    return true;
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

void DisplayHdmi::setSurfaceFlingerActiveMode() {
    /*
    sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(
        ISurfaceComposer::eDisplayIdMain));

    SurfaceComposerClient::setActiveConfig(dtoken, mDisplayConfigs.size()-mActiveDisplayConfigItem-1);
    */
}

void DisplayHdmi::initModes() {
    //mAllModes.push_back("480p60hz");
    //mAllModes.push_back("576p50hz");
    //mAllModes.push_back("720p50hz");
    //mAllModes.push_back("720p60hz");
    mAllModes.push_back("1080p60hz");
    mAllModes.push_back("1080p50hz");
    mAllModes.push_back("1080p30hz");
    mAllModes.push_back("1080p25hz");
    mAllModes.push_back("1080p24hz");
    //mAllModes.push_back("2160p24hz");
    //mAllModes.push_back("2160p25hz");
    //mAllModes.push_back("2160p30hz");
    //mAllModes.push_back("2160p50hz");
    //mAllModes.push_back("2160p60hz");
}

void DisplayHdmi::dump(Dump& d) {
        d.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        d.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");
        for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
            DisplayConfig *config = mDisplayConfigs[i];
            if (config) {
                d.append("%s %2d     |       %4d       |   %5d   |    %4d    |"
                    "    %3d    |    %3d    \n",
                         (i == (size_t)mActiveDisplayConfigItem) ? "*   " : "    ",
                         i,
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



