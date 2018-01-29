/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "ConnectorHdmi.h"

#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>

#include <MesonLog.h>
#include <HwDisplayConnector.h>

ConnectorHdmi::ConnectorHdmi(int32_t drvFd, uint32_t id) :
    HwDisplayConnector(drvFd, id) {
}

ConnectorHdmi::~ConnectorHdmi() {
}

drm_connector_type_t ConnectorHdmi::getType() {
    return DRM_MODE_CONNECTOR_HDMI;
}

uint32_t ConnectorHdmi::getModesCount() {
    return 0;
}

int32_t ConnectorHdmi::getDisplayModes(
    drm_mode_info_t * modes) {
    return 0;
}

bool ConnectorHdmi::isConnected() {
    return true;
}

bool ConnectorHdmi::isSecure() {
    return false;
}

#if 0
HwDisplayConnector::HwDisplayConnector(
    int drvFd, int32_t id, int32_t type) {

}

HwDisplayConnector::~HwDisplayConnector() {
    if (mDrvFd) {
        close(mDrvFd);
    }
}

int HwDisplayConnector::init() {

    reset();
    std::string dispMode;
    mFramebufferContext = new FBContext();
    framebuffer_info_t* fbInfo = mFramebufferContext->getInfo();
    calcDefaultMode(fbInfo, dispMode);
    buildSingleConfigList(dispMode);
    updateActiveConfig(dispMode);
    mFbWidth = fbInfo.info.xres;
    mFbHeight = fbInfo.info.yres;
    if (mSC == NULL) mSC = getSystemControlService();
}

void HwDisplayConnector::deinitialize() {
    reset();
}

void HwDisplayConnector::reset() {
    clearDispConfigs(mHwcSupportDispConfigs);
    clearDispConfigs(mSfSupportDispConfigs);
    mActiveConfigStr.clear();
    mConnected = false;
    mSFActiveConfigId = mHWCActiveConfigId = mFakeActiveConfigId = VMODE_NULL;
    mPhyWidth = mPhyHeight = 0;
    mSC = NULL;
    // mExtModeSet = 0;
    mExtModeSet = 1;
    mDefaultModeSupport = false;
}

sp<ISystemControlService> HwDisplayConnector::getSystemControlService() {
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


bool HwDisplayConnector::readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes) {
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
                    DTRACE("dispMode %s.", dispMode);
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

int HwDisplayConnector::setActiveConfig(int modeId) {
    if (!isConnected()) {
        DTRACE("[%s]: hdmi display is not connected.", __func__);
        return BAD_VALUE;
    }

    DTRACE("setActiveConfig to mode(%d).", modeId);
    int modeIdx = mSfSupportDispConfigs.indexOfKey((const vmode_e)modeId);
    if (modeIdx >= 0) {
        DisplayConfig* cfg = mSfSupportDispConfigs.valueAt(modeIdx);
        std::string dM = cfg->getDisplayMode();

        // It is possible that default mode is not supported by the sink
        // and it was only advertised to the FWK to force 1080p UI.
        // Trap this case and do nothing. FWK will keep thinking
        // 1080p is supported and set.
        if (!mDefaultModeSupport && dM == std::string(DEFAULT_DISPMODE)) {
            DTRACE("setActiveConfig default mode not supported");
            return NO_ERROR;
        }

        DTRACE("setActiveConfig to (%d, %s).", modeId, dM.c_str());
        setDisplayMode(dM, cfg->getFracRatePolicy());

        // update real active config.
        updateActiveConfig(dM);
        // mExtModeSet = 1;
        return NO_ERROR;
    } else {
        ETRACE("set invalild active config (%d)", modeId);
        return BAD_VALUE;
    }
}

int HwDisplayConnector::getActiveConfig(hwc2_config_t* outConfig) {
    if (!isConnected()) {
        DTRACE("[%s]: hdmi is not connected.", __func__);
    }

    if (mSFActiveConfigId == VMODE_NULL)
        *outConfig = mFakeActiveConfigId;
    else
        *outConfig = mSFActiveConfigId;

    VTRACE("getActiveConfig mExtModeSet: %d config(%d).", mExtModeSet, *outConfig);
    return NO_ERROR;
}

int HwDisplayConnector::clearDispConfigs(KeyedVector<int, DisplayConfig*> &dispConfigs) {
    // reset display configs
    for (size_t i = 0; i < dispConfigs.size(); i++) {
        HwDisplayConnector *config = dispConfigs.valueAt(i);
        if (config)
            delete config;
    }
    dispConfigs.clear();
    return NO_ERROR;
}


bool HwDisplayConnector::isDispModeValid(std::string & dispmode){
    DTRACE("isDispModeValid %s", dispmode.c_str());
    if (dispmode.empty())
        return false;

    vmode_e mode = vmode_name_to_mode(dispmode.c_str());
    DTRACE("isDispModeValid get mode (%d)", mode);
    if (mode == VMODE_MAX)
        return false;

    if (want_hdmi_mode(mode) == 0)
        return false;

    return true;
}


int HwDisplayConnector::calcDefaultMode(framebuffer_info_t& framebufferInfo,
        std::string& defaultMode) {
    const struct vinfo_s * mode =
        findMatchedMode(framebufferInfo.info.xres, framebufferInfo.info.yres, 60);
    if (mode == NULL) {
        defaultMode = DEFAULT_DISPMODE;
    } else {
        defaultMode = mode->name;
    }

    defaultMode = DEFAULT_DISPMODE;

    VTRACE("calcDefaultMode %s", defaultMode.c_str());
    return NO_ERROR;
}

int HwDisplayConnector::buildSingleConfigList(std::string& defaultMode) {
    if (!isDispModeValid(defaultMode)) {
        ETRACE("buildSingleConfigList with invalidate mode (%s)", defaultMode.c_str());
        return false;
      }

    int ret = addHwcDispConfigs(defaultMode);

    return ret;
}

int HwDisplayConnector::addHwcDispConfigs(std::string& mode) {
    vmode_e vmode = vmode_name_to_mode(mode.c_str());
    const struct vinfo_s* vinfo = get_tv_info(vmode);
    if (vmode == VMODE_MAX || vinfo == NULL) {
        ETRACE("addHwcDispConfigs meet error mode (%s, %d)", mode.c_str(), vmode);
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

    // We add fractional modes first. This has two (intentional) side-effects:
    // 1 - we select fractional modes ahead of integral when mapping
    //     display mode strings (i.e. 1080p30hz") to display modes in
    //     updateActiveConfig
    // 2 - the 'fake' default mode at boot becomes 1080p60hz. This means
    //     that a user selecting 1080p (59.94Hz) from the UI will have
    //     their mode request properly actioned, not discarded because
    //     the current mode matches the requested mode
    // Both of these issues should be addressed by properly distiguishing
    // between fractional and integral modes in all parts of the display
    // management pipeline.

    // add frac refresh rate config, like 23.976hz, 29.97hz...
    if (vinfo->sync_duration_num == REFRESH_24kHZ
        || vinfo->sync_duration_num == REFRESH_30kHZ
        || vinfo->sync_duration_num == REFRESH_60kHZ
        || vinfo->sync_duration_num == REFRESH_120kHZ
        || vinfo->sync_duration_num == REFRESH_240kHZ) {
        DisplayConfig *fracConfig = new DisplayConfig(mode,
                                        vinfo->sync_duration_num,
                                        vinfo->width,
                                        vinfo->height,
                                        dpiX,
                                        dpiY,
                                        true);
        mHwcSupportDispConfigs.add(mHwcSupportDispConfigs.size(), fracConfig);
    }

    // add normal refresh rate config, like 24hz, 30hz...
    VTRACE("add display mode pair (%d, %s)", mHwcSupportDispConfigs.size(), mode.c_str());
    mHwcSupportDispConfigs.add(mHwcSupportDispConfigs.size(), config);

    return NO_ERROR;
}

int HwDisplayConnector::readHdmiDispMode(std::string &dispmode) {
    if (mSC.get() && mSC->getActiveDispMode(&dispmode)) {
        DTRACE("get current displaymode %s", dispmode.c_str());
        if (!isDispModeValid(dispmode)) {
            ETRACE("active mode %s not valid", dispmode.c_str());
            return BAD_VALUE;
        }
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::getActiveDispMode FAIL.");
        return FAILED_TRANSACTION;
    }
}

status_t HwDisplayConnector::readHdmiPhySize(framebuffer_info_t& fbInfo) {
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

bool HwDisplayConnector::chkPresent() {
    bool bConnect = false;
    std::string dispMode;
    if (!readHdmiDispMode(dispMode)) {
        bConnect = isDispModeValid(dispMode);
    }

    DTRACE("chkPresent %s", bConnect ? "connected" : "disconnected");
    return bConnect;
}

int HwDisplayConnector::writeHdmiDispMode(std::string &dispmode) {
    if (mSC.get() && mSC->setActiveDispMode(dispmode)) {
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::setActiveDispMode FAIL.");
        return FAILED_TRANSACTION;
    }
}

int HwDisplayConnector::readEdidList(std::vector<std::string>& edidlist) {
    if (mSC.get() && mSC->getSupportDispModeList(&edidlist)) {
        return NO_ERROR;
    } else {
        ETRACE("syscontrol::readEdidList FAIL.");
        return FAILED_TRANSACTION;
    }
}

int HwDisplayConnector::updateActiveConfig(std::string& activeMode) {
    mActiveConfigStr = activeMode;

    for (size_t i = 0; i < mHwcSupportDispConfigs.size(); i++) {
        DisplayConfig * cfg = mHwcSupportDispConfigs.valueAt(i);
        if (activeMode == cfg->getDisplayMode()) {
            mHWCActiveConfigId = mHwcSupportDispConfigs.keyAt(i);
            mFakeActiveConfigId = mHwcSupportDispConfigs.size()-1;
            VTRACE("updateActiveConfig to (%s, %d)", activeMode.c_str(), mHWCActiveConfigId);
            return NO_ERROR;
        }
    }

    // If we reach here we are trying to set an unsupported mode. This can happen as
    // SystemControl does not guarantee to keep the EDID mode list and the active
    // mode id synchronised. We therefore handle the case where the active mode is
    // not supported by ensuring something sane is set instead.
    // NOTE: this is only really a workaround - HWC should instead guarantee that
    // the display mode list and active mode reported to SF are kept in sync with
    // hot plug events.
    mHWCActiveConfigId = mHwcSupportDispConfigs.size()-1;
    mFakeActiveConfigId = mHWCActiveConfigId;

    return NO_ERROR;
}

bool HwDisplayConnector::updateHotplug(bool connected,
        framebuffer_info_t& framebufferInfo) {
    bool ret = true;
    int32_t rate;

    if (!connected) {
        DTRACE("hdmi disconnected, keep old display configs.");
        // return true;
    }

    updateDisplayAttributes(framebufferInfo);

    // MR : TODO : Clean up this hot plug logic. Updating the modes before reading
    //             hdmi modes is counter-intuitive. The intention here was to
    //             provide a default mode when no display is connected.
    if (updateConfigs() != HWC2_ERROR_NONE) {
        ETRACE("updateHotplug: No supported display list, set default configs.");
        std::string dM (DEFAULT_DISPMODE);
        buildSingleConfigList(dM);
    }

    std::string activemode;
    if (readHdmiDispMode(activemode) != HWC2_ERROR_NONE) {
        std::string dM (DEFAULT_DISPMODE);
        ETRACE("get active display mode failed.");
        updateActiveConfig(dM);
        return false;
    }
    updateActiveConfig(activemode);

    return true;
}

int HwDisplayConnector::updateConfigs() {
    // clear display modes
    clearDispConfigs(mHwcSupportDispConfigs);

    std::vector<std::string> supportDispModes;
    std::string::size_type pos;
    std::string dM (DEFAULT_DISPMODE);
    mDefaultModeSupport = false;

    bool isConfiged = readConfigFile("/system/etc/displayModeList.cfg", &supportDispModes);
    if (isConfiged) {
        VTRACE("Read supported modes from cfg file.");
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
                VTRACE("modify support display mode:%s", supportDispModes[i].c_str());
            }

            // skip default / fake active mode as we add it to the end
            if (supportDispModes[i] != dM)
                addHwcDispConfigs(supportDispModes[i]);
            else
                mDefaultModeSupport = true;
        }
    }

    // Add default mode as last, unconditionally at boot or only if supported otherwise
    if (mHwcSupportDispConfigs.size() == 0
            || !mExtModeSet || mDefaultModeSupport) {
        addHwcDispConfigs(dM);
    }

    return NO_ERROR;
}



int HwDisplayConnector::updateSfDispConfigs() {
    // clear display modes
    clearDispConfigs(mSfSupportDispConfigs);
    for (int i = 0; i < mHwcSupportDispConfigs.size(); i++) {
        DisplayConfig *hwcCfg = mHwcSupportDispConfigs.valueAt(i);
        // Copy cfg.
        DisplayConfig *sfCfg = new DisplayConfig(hwcCfg->getDisplayMode(),
                                                hwcCfg->getRefreshRate(),
                                                hwcCfg->getWidth(),
                                                hwcCfg->getHeight(),
                                                hwcCfg->getDpiX(),
                                                hwcCfg->getDpiY(),
                                                hwcCfg->getFracRatePolicy());
        mSfSupportDispConfigs.add(mHwcSupportDispConfigs.keyAt(i), sfCfg);
        DTRACE("config[%d]: %s", i, sfCfg->getDisplayMode().c_str());
    }

    // Set active config id used by SF.
    mSFActiveConfigId = mHWCActiveConfigId;

    return NO_ERROR;
}

void HwDisplayConnector::switchRatePolicy(bool fracRatePolicy) {
    if (fracRatePolicy) {
        if (mSC.get() && mSC->writeSysfs(String16(HDMI_FRAC_RATE_POLICY), String16("1"))) {
            DTRACE("Switch to frac rate policy SUCCESS.");
        } else {
            DTRACE("Switch to frac rate policy FAIL.");
        }
    } else {
        if (mSC.get() && mSC->writeSysfs(String16(HDMI_FRAC_RATE_POLICY), String16("0"))) {
            DTRACE("Switch to normal rate policy SUCCESS.");
        } else {
            DTRACE("Switch to normal rate policy FAIL.");
        }
    }
}


int HwDisplayConnector::setDisplayMode(std::string& dm, bool policy) {
    DTRACE("setDisplayMode to %s", dm.c_str());
    switchRatePolicy(policy);
    writeHdmiDispMode(dm);
    return NO_ERROR;
}

int HwDisplayConnector::getDisplayAttribute(hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute,
        int32_t* outValue,
        int32_t caller) {
    if (!isConnected()) {
        DTRACE("[%s]: hdmi is not connected.", __func__);
    }

    KeyedVector<int, DisplayConfig*> *configs = NULL;
    if (CALL_FROM_SF == caller) {
        configs = &mSfSupportDispConfigs;
    } else if (CALL_FROM_HWC == caller) {
        configs = &mHwcSupportDispConfigs;
    }
    if (!configs) {
        ETRACE("[%s]: no support display config: %d", __func__, config);
        return BAD_VALUE;
    }

    DisplayConfig *configChosen = NULL;
    int modeIdx = configs->indexOfKey((vmode_e)config);
    if (modeIdx >= 0) {
        configChosen = configs->valueAt(modeIdx);
    }

    if (!configChosen) {
        ETRACE("[%s]: failed to get display config: %d", __func__, config);
        return BAD_VALUE;
    }

    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD: {
            float refreshRate = configChosen->getFracRefreshRate();
            if (0 == (int)refreshRate) {
                WTRACE("FPS was 0 : setting to default");
                refreshRate = DEFAULT_REFRESH;
            }
            *outValue = 1e9 / refreshRate;
        }
        break;
        case HWC2_ATTRIBUTE_WIDTH:
#if FAKE_4K_DISPLAY
            *outValue = DEFAULT_WIDTH;
#else
            *outValue = configChosen->getWidth();
#endif
            if (0 == *outValue) {
                WTRACE("width was 0 : setting to default");
                *outValue = DEFAULT_WIDTH;
            }
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
#if FAKE_4K_DISPLAY
            *outValue = DEFAULT_HEIGHT;
#else
            *outValue = configChosen->getHeight();
#endif
            if (0 == *outValue) {
                WTRACE("height was 0 : setting to default");
                *outValue = DEFAULT_HEIGHT;
            }
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = configChosen->getDpiX() * 1000.0f;
            if (0 == *outValue) {
                WTRACE("dpi was 0 : setting to default");
                *outValue = DEFAULT_DISPLAY_DPI;
            }
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = configChosen->getDpiY() * 1000.0f;
            if (0 == *outValue) {
                WTRACE("dpi was 0 : setting to default");
                *outValue = DEFAULT_DISPLAY_DPI;
            }
        break;
        default:
            ETRACE("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return NO_ERROR;
}

/*need define HwDisplayConnector as the DisplayConfig*/
HwDisplayConnector * HwDisplayConnector::createConnector(
    int drvFd, int32_t id, int32_t type) {
    std::string dispMode;
    mFramebufferContext = new FBContext();
    framebuffer_info_t* fbInfo = mFramebufferContext->getInfo();
    calcDefaultMode(fbInfo,dispMode);
    buildSingleConfigList(dispMode);
    for (size_t i = 0; i < mHwcSupportDispConfigs.size(); i++) {
        HwDisplayConnector * cfg = mHwcSupportDispConfigs.valueAt(i);
        if (activeMode == cfg->getDisplayMode())
             return  cfg;
    }

    // If we reach here we are trying to set an unsupported mode. This can happen as
    // SystemControl does not guarantee to keep the EDID mode list and the active
    // mode id synchronised. We therefore handle the case where the active mode is
    // not supported by ensuring something sane is set instead.
    // NOTE: this is only really a workaround - HWC should instead guarantee that
    // the display mode list and active mode reported to SF are kept in sync with
    // hot plug events.
       mHWCActiveConfigId = mHwcSupportDispConfigs.size()-1;
       return mHwcSupportDispConfigs.valueAt(mHWCActiveConfigId);

}

int HwDisplayConnector::parseConfigFile()
{
    const char* WHITESPACE = " \t\r";

   // SysTokenizer* tokenizer;
      Tokenizer* tokenizer;
    int status = Tokenizer::open(String8(pConfigPath), &tokenizer);
    if (status) {
        ETRACE("Error %d opening display config file %s.", status, pConfigPath);
    } else {
        while (!tokenizer->isEof()) {
            ITRACE("Parsing %s: %s", (tokenizer->getLocation()).string(), (tokenizer->peekRemainderOfLine()).string());

            tokenizer->skipDelimiters(WHITESPACE);
            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {

                const char *token = (tokenizer->nextToken(WHITESPACE)).string();
                if (!strcmp(token, DEVICE_STR_MBOX)) {
                    mDisplayType = DISPLAY_TYPE_MBOX;
                } else if (!strcmp(token, DEVICE_STR_TV)) {
                    mDisplayType = DISPLAY_TYPE_TV;
                } else {
                    DTRACE("%s: Expected keywordgot '%s'.", (tokenizer->getLocation()).string(), token);
                    break;

                tokenizer->skipDelimiters(WHITESPACE);
                tokenizer->nextToken(WHITESPACE);
                tokenizer->skipDelimiters(WHITESPACE);
                strcpy(mDefaultMode, tokenizer->nextToken(WHITESPACE));
            }

            tokenizer->nextLine();
        }
        delete tokenizer;
    }
    return status;
}


void HwDisplayConnector::dump(Dump& d) {
        d.append("Connector (HDMI, %s, %d, %d)\n",
                 mRealActiveConfigStr.c_str(),
                 mRealActiveConfigId,
                 mWorkMode);
        d.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
            "   DPI_X   |   DPI_Y   \n");
        d.append("------------+------------------+-----------+------------+"
            "-----------+-----------\n");
        for (size_t i = 0; i < mSupportDispConfigs.size(); i++) {
            int mode = mSupportDispConfigs.keyAt(i);
            DisplayConfig *config = mSupportDispConfigs.valueAt(i);
            if (config) {
                d.append("%s %2d     |      %.3f      |   %5d   |   %5d    |"
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



/****the above HwDisplayConnector func should implied.*********/
/*****the below fuction of HwDisplayConnector is DRM func workflow ****/

bool HwDisplayConnector::state() const {
  return state_;
}


const &HwDisplayConnector::dpms_property() const {
  return dpms_property_;
}

bool HwDisplayConnector::setPowerMode(int /*mode*/)
{
    return true;
}

const &HwDisplayConnector::crtc_id_property() const {
  return crtc_id_property_;
}

void HwDisplayConnector::set_encoder(DrmEncoder *encoder) {
  encoder_ = encoder;
}


bool HwDisplayConnector::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    if ((configs > 0) || !attributes || !values) {
        ELOGTRACE("invalid parameters");
        return false;
    }

    if (!mConnected) {
        ILOGTRACE("dummy device is not connected");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / 60;
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = 1280;
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = 720;
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = 0;
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = 0;
            break;
        default:
            ELOGTRACE("unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool HwDisplayConnector::compositionComplete()
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

const char* HwDisplayConnector::getName() const
{
    return "Dummy";
}

int HwDisplayConnector::getType() const
{
    return mDisp;
}

void HwDisplayConnector::onVsync(int64_t timestamp)
{
    if (!mConnected)
        return;

    mHwc.vsync(mDisp, timestamp);
}


int HwDisplayConnector::UpdateModes() {
  int fd = drm_->fd();

  drmModeConnectorPtr c = drmModeGetConnector(fd, id_);
  if (!c) {
    ALOGE("Failed to get connector %d", id_);
    return -ENODEV;
  }

  state_ = c->connection;

  std::vector<DrmMode> new_modes;
  for (int i = 0; i < c->count_modes; ++i) {
    bool exists = false;
    for (const DrmMode &mode : modes_) {
      if (mode == c->modes[i]) {
        new_modes.push_back(mode);
        exists = true;
        break;
      }
    }
    if (exists)
      continue;

    DrmMode m(&c->modes[i]);
    m.set_id(drm_->next_mode_id());
    new_modes.push_back(m);
  }
  modes_.swap(new_modes);
  return 0;
}


int32_t HwDisplayConnector::id() const {
      return mid;
  }

void HwDisplayConnector::set_display(int display) {
         mdisplay_ = display;
  }

bool HwDisplayConnector::built_in() const {
    return type_ == DRM_MODE_CONNECTOR_LVDS || type_ == DRM_MODE_CONNECTOR_eDP ||
           type_ == DRM_MODE_CONNECTOR_DSI || type_ == DRM_MODE_CONNECTOR_VIRTUAL;
  }

uint32_t HwDisplayConnector::mm_width() const {
  return mm_width_;
}

uint32_t HwDisplayConnector::mm_height() const {
  return mm_height_;
}
#endif
