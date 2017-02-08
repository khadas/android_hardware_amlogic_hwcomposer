#ifndef AML_DISPLAY_HDMI_H
#define AML_DISPLAY_HDMI_H

#include <gralloc_priv.h>
#include <utils/String8.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <linux/fb.h>
#include <string>
#include <vector>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <Hwcomposer.h>
#include <SoftVsyncObserver.h>
#include <Utils.h>
#include <ISystemControlService.h>
#include <gui/SurfaceComposerClient.h>

#define HWC_DISPLAY_MODE_LENGTH 32

namespace android {
namespace amlogic {

// display config
class DisplayConfig {
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

class DisplayHdmi {
public:
    DisplayHdmi(hwc2_display_t id);
    ~DisplayHdmi();

    void initialize();
    void deinitialize();
    void reset();
    bool updateHotplug(bool connected, framebuffer_info_t * framebufferInfo,
        private_handle_t* framebufferHnd);
    int updateDisplayModeList();
    int updateActiveDisplayMode();
    int setDisplayMode(const char* displaymode);
    int updateDisplayConfigures();
    int updateActiveDisplayConfigure();

    int getDisplayConfigs(uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    int getDisplayAttribute(hwc2_config_t config, int32_t  attribute, int32_t* outValue);
    int getActiveConfig(hwc2_config_t* outConfig);
    int setActiveConfig(int id);
    //int setPowerMode(int power) {return 0;};

    inline bool isConnected() {return mConnected;};
    int getActiveRefreshRate()  {return mActiveRefreshRate;};
    bool calcMode2Config(const char *dispMode, int* refreshRate, int* width, int* height);
    bool readConfigFile(const char* configPath, std::vector<std::string>* supportDispModes);
    void setSurfaceFlingerActiveMode();
    void initModes();

    void dump(Dump& d);

private:
    hwc2_display_t mDisplayId;   //0-primary 1-external
    bool mConnected;
    sp<ISystemControlService> mSystemControlService;
    sp<SurfaceComposerClient> mComposerClient;

    //display outputmode as 4k20hz, 1080p60hz, panel. etc.
    std::vector<std::string> mAllModes;
    std::vector<std::string> mSupportDispModes;
    char mActiveDisplaymode[HWC_DISPLAY_MODE_LENGTH];
    int mActiveRefreshRate;

    std::vector<DisplayConfig*> mDisplayConfigs;
    hwc2_config_t mActiveDisplayConfigItem;

    framebuffer_info_t *mFramebufferInfo;
    private_handle_t  *mFramebufferHnd;
};

} // namespace amlogic
} // namespace android

#endif
