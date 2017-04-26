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

#ifndef AML_DISPLAY_HDMI_H
#define AML_DISPLAY_HDMI_H

#include <gralloc_priv.h>
#include <utils/String8.h>
#include <utils/Errors.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <linux/fb.h>
#include <string>
#include <vector>
#include <pthread.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <Hwcomposer.h>
#include <Utils.h>
#include <ISystemControlService.h>
#include <gui/SurfaceComposerClient.h>
#include <AmVinfo.h>

#define HWC_DISPLAY_MODE_LENGTH 32

namespace android {
namespace amlogic {

// display config
class DisplayConfig {
friend class DisplayHdmi;
public:
    DisplayConfig(const char* dm,
		int rr,
		int w = 0,
		int h = 0,
		int dpix = 0,
		int dpiy = 0)
        : mRefreshRate(rr),
          mWidth(w),
          mHeight(h),
          mDpiX(dpix),
          mDpiY(dpiy)
    {
        memset(mDisplayMode, 0, HWC_DISPLAY_MODE_LENGTH);
        int length = strlen(dm);
        length = (length <= HWC_DISPLAY_MODE_LENGTH-1)?length:(HWC_DISPLAY_MODE_LENGTH-1);
        memcpy(mDisplayMode, dm, length);
    }
public:
    char* getDisplayMode() const { return (char*)(&mDisplayMode[0]); };
    int getRefreshRate() const { return mRefreshRate; };
    int getWidth() const { return mWidth; };
    int getHeight() const { return mHeight; };
    int getDpiX() const { return mDpiX; };
    int getDpiY() const { return mDpiY; };
    void setDpi(int dpix, int dpiy) {
        mDpiX = dpix;
        mDpiY = dpiy;
    };

private:
    char mDisplayMode[HWC_DISPLAY_MODE_LENGTH];
    int mRefreshRate;
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
};

typedef void (*DisplayNotify)(void*);

class DisplayHdmi {
public:
    DisplayHdmi();
    ~DisplayHdmi();

    void initialize(framebuffer_info_t& framebufferInfo, DisplayNotify notifyFn, void* notifyData);
    void deinitialize();
    bool updateHotplug(bool connected, framebuffer_info_t* framebufferInfo = NULL);

    status_t getDisplayConfigs(uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    status_t getDisplayAttribute(hwc2_config_t config, int32_t  attribute, int32_t* outValue);
    status_t getActiveConfig(hwc2_config_t* outConfig);
    status_t setActiveConfig(int modeId);
    //int setPowerMode(int power) {return 0;};

    inline bool isConnected() {return mConnected;};
    bool chkPresent();

    void dump(Dump& d);

protected:
    /* hdmi operations:
      * TODO: need move all these operations to HAL.
     */
    sp<ISystemControlService> getSystemControlService();
    status_t readEdidList(std::vector<std::string> &edidlist);
    status_t writeHdmiDispMode(std::string &dispmode);
    status_t readHdmiDispMode(std::string &dispmode);
    status_t readHdmiPhySize(framebuffer_info_t& fbInfo);

    void reset();

    //operations on mSupportDispModes
    status_t clearSupportedConfigs();
    status_t updateSupportedConfigs();
    status_t updateDisplayConfigs();
    status_t readDisplayPhySize();
    DisplayConfig* createConfigByModeStr(std::string & modeStr);

    //ensure the active mode equals the current displaymode.
    status_t chkActiveConfig(std::string& activeMode);
    status_t updateActiveConfig(std::string& activeMode);

    status_t setDisplayMode(const char* displaymode);
    bool isDispModeValid(std::string & dispmode);
    bool readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes);
    status_t calcDefaultMode(framebuffer_info_t& framebufferInfo, std::string& defaultMode);
    status_t buildSingleConfigList(std::string& defaultMode);

    bool checkVinfo(framebuffer_info_t *fbInfo);

    //functions for NONE_ACTIVEMODE.
    static void* monitorDisplayMode(void *param);
    void monitorDisplayMode();
    bool isHotplugComplete();

private:
    bool mConnected;

    //for NONE_ACTIVEMODE, other module may set the displaymode, we need monitor if output mode changed.
    pthread_t mDisplayMonitorThread;
    //for NONE_ACTIVEMODE, displaymode update will later than hotplug event, need waiting the displaymode update again.
    bool mHotPlugComplete;
    DisplayNotify mNotifyFn;
    void* mNotifyData;
    bool mMonitorExit;
    Mutex mUpdateLock;

    //configures variables.
    hwc2_config_t mActiveConfigId; //for amlogic, it is vmode_e.
    std::string mActiveConfigStr;
    KeyedVector<vmode_e, DisplayConfig*> mSupportDispConfigs;

    //physical size in mm.
    int mPhyWidth;
    int mPhyHeight;
    //framebuffer size.
    int mFbWidth;
    int mFbHeight;

    /*
     *work modes:
     *REAL_ACTIVEMODE & LOGIC_ACTIVEMODE:
     *    really get supported configs from driver. And hwc controlls the active config of system.
     *NONE_ACTIVEMODE:
     *    only one virtual mode(dpi, vsync are real, the resolution is the framebuffer size.)
     */
    enum {
        REAL_ACTIVEMODE = 0,
        LOGIC_ACTIVEMODE, //return the logic size which is framebuffer size.
        NONE_ACTIVEMODE //no active mode list, always return a default config.
    };
    int mWorkMode;
};
} // namespace amlogic
} // namespace android

#endif
