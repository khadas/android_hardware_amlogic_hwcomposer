/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <HwDisplayManager.h>
#include <MesonLog.h>
#include <DebugHelper.h>
#include <cutils/properties.h>
#include <systemcontrol.h>
#include <misc.h>
#include <math.h>

#include "HwDisplayCrtcDrm.h"
#include "AmVinfo.h"
#include "AmFramebuffer.h"

#define VIU1_DISPLAY_MODE_SYSFS "/sys/class/display/mode"
#define VIU2_DISPLAY_MODE_SYSFS "/sys/class/display2/mode"

HwDisplayCrtcDrm::HwDisplayCrtcDrm(int drvFd, int32_t id)
    : HwDisplayCrtc(drvFd, id) {
    MESON_ASSERT(id == CRTC_VOUT1 || id == CRTC_VOUT2, "Invalid crtc id %d", id);

    mId = id;
    mDrvFd = drvFd;
    mBinded = false;
    /*for old vpu, always one channel.
    *for new vpu, it can be 1 or 2.
    */
    mViewPort.x = mViewPort.y = mViewPort.w = mViewPort.h = 0;
}

HwDisplayCrtcDrm::~HwDisplayCrtcDrm() {
}

int32_t HwDisplayCrtcDrm::bind(
    std::shared_ptr<HwDisplayConnector>  connector,
    std::vector<std::shared_ptr<HwDisplayPlane>> planes) {
    if (mBinded) {
        #if 0/*disable for drm*/
        if (mConnector.get())
            mConnector->setCrtc(NULL);
        #endif
        mConnector.reset();
        mPlanes.clear();
        mBinded =  false;
    }

    mConnector = connector;
    #if 0/*disable for drm*/
    mConnector->setCrtc(this);
    #endif
    mPlanes = planes;
    mBinded = true;
    return 0;
}

int32_t HwDisplayCrtcDrm::unbind() {
    /*TODO: temp disable here.
    * systemcontrol and hwc set display mode
    * at the same time, there is a timing issue now.
    * Just disable it here, later will remove systemcontrol
    * set displaymode when hotplug.
    */
    if (mBinded) {
        #if 0
        static drm_mode_info_t nullMode = {
            DRM_DISPLAY_MODE_NULL,
            0, 0,
            0, 0,
            60.0
        };
        std::string dispmode(nullMode.name);
        writeCurDisplayMode(dispmode);
        #endif
        #if 0 /*disable for drm*/
        if (mConnector.get())
            mConnector->setCrtc(NULL);
	#endif
        mConnector.reset();
        mPlanes.clear();
        mBinded = false;
    }
    return 0;
}

int32_t HwDisplayCrtcDrm::loadProperities() {
    /*load static information when pipeline present.*/
    {
        std::lock_guard<std::mutex> lock(mMutex);
        MESON_ASSERT(mConnector, "Crtc need setuped before load Properities.");
        mConnector->loadProperities();
        mModes.clear();
        mConnected = mConnector->isConnected();
        if (mConnected) {
            mConnector->getModes(mModes);
            /*TODO: add display mode filter here
            * to remove unsupported displaymode.
            */
        }
    }

    return 0;
}

int32_t HwDisplayCrtcDrm::getId() {
    return mId;
}

int32_t HwDisplayCrtcDrm::setMode(drm_mode_info_t & mode) {
    MESON_LOG_EMPTY_FUN();

    /*DRM_DISPLAY_MODE_NULL is always allowed.*/
    MESON_LOGI("Crtc setMode: %s", mode.name);
    std::string dispmode(mode.name);

    return writeCurDisplayMode(dispmode);
}

int32_t HwDisplayCrtcDrm::setDisplayAttribute(std::string& dispattr __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::getDisplayAttribute(std::string& dispattr __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::getMode(drm_mode_info_t & mode) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mConnected || mCurModeInfo.name[0] == 0)
        return -EFAULT;

    mode = mCurModeInfo;
    return 0;
}

int32_t HwDisplayCrtcDrm::waitVBlank(nsecs_t & timestamp __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::update() {
    /*update dynamic information which may change.*/
    std::lock_guard<std::mutex> lock(mMutex);
    memset(&mCurModeInfo, 0, sizeof(drm_mode_info_t));
    if (mConnector)
        mConnector->update();
    if (mConnected) {
        /*1. update current displayMode.*/
        std::string displayMode;
        readCurDisplayMode(displayMode);
        if (displayMode.empty()) {
             MESON_LOGE("displaymode should not null when connected.");
        } else {
            for (auto it = mModes.begin(); it != mModes.end(); it ++) {
                MESON_LOGD("update: (%s) mode (%s)", displayMode.c_str(), it->second.name);
                if (strcmp(it->second.name, displayMode.c_str()) == 0
                     && it->second.refreshRate == it->second.refreshRate) {
                    memcpy(&mCurModeInfo, &it->second, sizeof(drm_mode_info_t));
                    break;
                }
            }
            MESON_LOGD("crtc(%d) update (%s) (%d) -> (%s).",
                mId, displayMode.c_str(), mModes.size(), mCurModeInfo.name);
        }
    } else {
        /*clear mode info.*/
        memset(&mCurModeInfo, 0, sizeof(mCurModeInfo));
        /* TODO: temp disable mode setting in HWC. */
        #if 0
        strcpy(mCurModeInfo.name, DRM_DISPLAY_MODE_NULL);
        setMode(mCurModeInfo);
        #else
        MESON_LOGD("crtc(%d) update with no connector", mId);
        #endif
    }

    return 0;
}

int32_t HwDisplayCrtcDrm::setDisplayFrame(display_zoom_info_t & info __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::setOsdChannels(int32_t channels __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::pageFlip(int32_t &out_fence __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t HwDisplayCrtcDrm::getHdrMetadataKeys(
    std::vector<drm_hdr_meatadata_t> & keys) {
    static drm_hdr_meatadata_t supportedKeys[] = {
        DRM_DISPLAY_RED_PRIMARY_X,
        DRM_DISPLAY_RED_PRIMARY_Y,
        DRM_DISPLAY_GREEN_PRIMARY_X,
        DRM_DISPLAY_GREEN_PRIMARY_Y,
        DRM_DISPLAY_BLUE_PRIMARY_X,
        DRM_DISPLAY_BLUE_PRIMARY_Y,
        DRM_WHITE_POINT_X,
        DRM_WHITE_POINT_Y,
        DRM_MAX_LUMINANCE,
        DRM_MIN_LUMINANCE,
        DRM_MAX_CONTENT_LIGHT_LEVEL,
        DRM_MAX_FRAME_AVERAGE_LIGHT_LEVEL,
    };

    for (uint32_t i = 0;i < sizeof(supportedKeys)/sizeof(drm_hdr_meatadata_t); i++) {
        keys.push_back(supportedKeys[i]);
    }

    return 0;
}

int32_t HwDisplayCrtcDrm::setHdrMetadata(
    std::map<drm_hdr_meatadata_t, float> & hdrmedata __unused) {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

int32_t  HwDisplayCrtcDrm::readCurDisplayMode(std::string & dispmode) {
    int32_t ret = 0;
    if (mId == CRTC_VOUT1) {
        ret = read_sysfs(VIU1_DISPLAY_MODE_SYSFS, dispmode);
    }  else if (mId == CRTC_VOUT2) {
        ret = read_sysfs(VIU2_DISPLAY_MODE_SYSFS, dispmode);
    }

    return ret;
}

int32_t HwDisplayCrtcDrm::writeCurDisplayMode(std::string & dispmode __unused) {
    #if 0
    const char *path =  (mId == CRTC_VOUT1) ? VIU1_DISPLAY_MODE_SYSFS : VIU2_DISPLAY_MODE_SYSFS;
    return sysfs_set_string(path, dispmode.c_str());
    #else
    MESON_LOG_EMPTY_FUN();
    return 0;
    #endif
}

int32_t HwDisplayCrtcDrm::writeCurDisplayAttr(std::string & dispattr __unused) {
#if 0
    int32_t ret = 0;
    ret = sc_write_sysfs(VIU_DISPLAY_ATTR_SYSFS, dispattr);
    return ret;
#else
    MESON_LOG_EMPTY_FUN();
    return 0;
#endif
}

void HwDisplayCrtcDrm::setViewPort(const drm_rect_wh_t viewPort) {
    std::lock_guard<std::mutex> lock(mMutex);
    mViewPort = viewPort;
}

void HwDisplayCrtcDrm::getViewPort(drm_rect_wh_t & viewPort) {
    std::lock_guard<std::mutex> lock(mMutex);
    viewPort = mViewPort;
}
