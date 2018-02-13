

 /* Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CONNECTOR_H
#define HW_DISPLAY_CONNECTOR_H

#include <string>
#include <vector>

#include <utils/String8.h>
#include <utils/Errors.h>
#include <sys/types.h>
#include <utils/KeyedVector.h>

#include <DrmTypes.h>
#include <BasicTypes.h>

#include <hardware/hardware.h>
#include <gralloc_priv.h>
#include <ISystemControlService.h>


#if PLATFORM_SDK_VERSION >= 26
#include <vendor/amlogic/hardware/systemcontrol/1.0/ISystemControl.h>
using ::vendor::amlogic::hardware::systemcontrol::V1_0::ISystemControl;
using ::vendor::amlogic::hardware::systemcontrol::V1_0::Result;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
#endif

#define DEFAULT_DISPLAY_DPI 160

typedef struct hdr_dev_capabilities {
    bool init;
    bool dvSupport;
    bool hdrSupport;
    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} hdr_dev_capabilities_t;

class DisplayConfig {
public:
    DisplayConfig(const std::string dm,
        int rr,
        int w = 0,
        int h = 0,
        int dpix = 0,
        int dpiy = 0,
        bool frac = false)
        : mDisplayMode(dm),
          mRefreshRate(rr),
          mWidth(w),
          mHeight(h),
          mDpiX(dpix),
          mDpiY(dpiy),
          mFracRate(frac)
    {}

public:
    std::string getDisplayMode() const { return mDisplayMode; };
    float getRefreshRate() const {
        float actualRate = 0.0f;

        if (mRefreshRate) {
            if (mFracRate) {
                actualRate = (mRefreshRate * 1000) / (float)1001;
            } else {
                actualRate = mRefreshRate;
            }
        }
        return actualRate;
    };
    int getWidth() const { return mWidth; };
    int getHeight() const { return mHeight; };
    int getDpiX() const { return mDpiX; };
    int getDpiY() const { return mDpiY; };
    void setDpi(int dpix, int dpiy) {
        mDpiX = dpix;
        mDpiY = dpiy;
    };
    bool getFracRatePolicy() { return mFracRate; };

private:
    std::string mDisplayMode;
    int mRefreshRate;
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
    bool mFracRate;
};


class HwDisplayConnector {
public:
    HwDisplayConnector(/*int32_t drvFd, uint32_t id*/) {
        // mDrvFd = drvFd;
         // mId = id;
          }
    virtual ~HwDisplayConnector(){}
public:
    virtual int init() = 0;
    virtual drm_connector_type_t getType() = 0;

    virtual bool isRemovable() = 0;
    virtual bool isConnected() = 0;
    virtual bool isSecure() = 0;
    virtual uint32_t getModesCount() = 0;
    virtual KeyedVector<int, DisplayConfig*> getModesInfo() = 0;
    virtual  void dump(String8 & dumpstr) = 0;

private:
    //int32_t mDrvFd;
    //uint32_t mId;
};

#endif/*HW_DISPLAY_CONNECTOR_H*/
