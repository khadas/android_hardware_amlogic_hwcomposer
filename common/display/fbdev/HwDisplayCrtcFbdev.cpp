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
#include <OmxUtil.h>
#include <inttypes.h>
#include <sys/utsname.h>

#include "AmVinfo.h"
#include "AmFramebuffer.h"
#include "HwDisplayCrtcFbdev.h"

static vframe_master_display_colour_s_t nullHdr;

#define VIU1_DISPLAY_MODE_SYSFS "/sys/class/display/mode"
#define VIU2_DISPLAY_MODE_SYSFS "/sys/class/display2/mode"
#define VIU_DISPLAY_ATTR_SYSFS "/sys/class/amhdmitx/amhdmitx0/attr"

HwDisplayCrtcFbdev::HwDisplayCrtcFbdev(int drvFd, int32_t id)
    : HwDisplayCrtc() {
    MESON_ASSERT(id == CRTC_VOUT1_ID || id == CRTC_VOUT2_ID, "Invalid crtc id %d", id);
    mId = id;
    mPipe = GET_PIPE_IDX_BY_ID(id);
    mDrvFd = drvFd;
    mFirstPresent = true;
    mBinded = false;
    /*for old vpu, always one channel.
    *for new vpu, it can be 1 or 2.
    */
    mOsdChannels = 1;
    memset(&nullHdr, 0, sizeof(nullHdr));

    hdrVideoInfo = malloc(sizeof(vframe_master_display_colour_s_t));
}

HwDisplayCrtcFbdev::~HwDisplayCrtcFbdev() {
    free(hdrVideoInfo);
}

int32_t HwDisplayCrtcFbdev::bind(
    std::shared_ptr<HwDisplayConnector>  connector) {
    if (mBinded) {
        if (mConnector.get())
            mConnector->setCrtcId(0);
        mConnector.reset();
        mBinded =  false;
    }

    mConnector = connector;
    mConnector->setCrtcId(mPipe);
    mBinded = true;
    return 0;
}

int32_t HwDisplayCrtcFbdev::unbind() {
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
        if (mConnector.get())
            mConnector->setCrtcId(0);
        mConnector.reset();
        mBinded = false;
    }
    return 0;
}

int32_t HwDisplayCrtcFbdev::getId() {
    return mId;
}

uint32_t HwDisplayCrtcFbdev::getPipe() {
    return mPipe;
}

int32_t HwDisplayCrtcFbdev::setMode(drm_mode_info_t & mode) {
    /*DRM_DISPLAY_MODE_NULL is always allowed.*/
    MESON_LOGI("Crtc setMode: %s", mode.name);
    std::string dispmode(mode.name);
    return writeCurDisplayMode(dispmode);
}

int32_t HwDisplayCrtcFbdev::getMode(drm_mode_info_t & mode) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mConnected || mCurModeInfo.name[0] == 0)
        return -EFAULT;

    mode = mCurModeInfo;
    return 0;
}

int32_t HwDisplayCrtcFbdev::waitVBlank(nsecs_t & timestamp) {
    int32_t ret = ioctl(mDrvFd, FBIO_WAITFORVSYNC_64, &timestamp);
    if (ret == -1) {
        ret = -errno;
        MESON_LOGE("fb ioctl vsync wait error, ret: %d", ret);
        return ret;
    } else {
        if (timestamp != 0) {
            return 0;
        } else {
            MESON_LOGE("wait for vsync fail");
            return -EINVAL;
        }
    }
}

int32_t HwDisplayCrtcFbdev::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    MESON_ASSERT(mConnector, "Crtc need setuped before load Properities.");

    mModes.clear();
    mConnected = mConnector->isConnected();

    if (mConnected) {
        mConnector->getModes(mModes);

        /*1. update current displayMode.*/
        std::string displayMode;
        readCurDisplayMode(displayMode);
        if (displayMode.empty()) {
             MESON_LOGE("displaymode should not null when connected.");
        } else {
            for (auto it = mModes.begin(); it != mModes.end(); it ++) {
                MESON_LOGD("update: (%s) mode (%s)", displayMode.c_str(), it->second.name);
                if (strcmp(it->second.name, displayMode.c_str()) == 0) {
                    memcpy(&mCurModeInfo, &it->second, sizeof(drm_mode_info_t));
                    break;
                }
            }
            MESON_LOGD("crtc(%d) update (%s) (%" PRIuFAST16 ") -> (%s).",
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

int32_t HwDisplayCrtcFbdev::setDisplayFrame(display_zoom_info_t & info) {
    mScaleInfo = info;
    /*not used now, clear to 0.*/
    mScaleInfo.crtc_w = 0;
    mScaleInfo.crtc_h = 0;
    return 0;
}

int32_t HwDisplayCrtcFbdev::prePageFlip() {
    /*nothing to do*/
    return 0;
}

int32_t HwDisplayCrtcFbdev::pageFlip(int32_t &out_fence) {
    if (mFirstPresent) {
        mFirstPresent = false;
        closeLogoDisplay();
    }

    osd_page_flip_info_t flipInfo;
    flipInfo.background_w = mScaleInfo.framebuffer_w;
    flipInfo.background_h = mScaleInfo.framebuffer_h;
    flipInfo.fullScreen_w = mScaleInfo.framebuffer_w;
    flipInfo.fullScreen_h = mScaleInfo.framebuffer_h;
    flipInfo.curPosition_x = mScaleInfo.crtc_display_x;
    flipInfo.curPosition_y = mScaleInfo.crtc_display_y;
    flipInfo.curPosition_w = mScaleInfo.crtc_display_w;
    flipInfo.curPosition_h = mScaleInfo.crtc_display_h;
    flipInfo.hdr_mode = 1;/*force to 1, for video is not synced with osd.*/

    ioctl(mDrvFd, FBIOPUT_OSD_DO_HWC, &flipInfo);

    if (DebugHelper::getInstance().discardOutFence()) {
        std::shared_ptr<DrmFence> outfence =
            std::make_shared<DrmFence>(flipInfo.out_fen_fd);
        outfence->waitForever("crtc-output");
        out_fence = -1;
    } else {
        out_fence = (flipInfo.out_fen_fd >= 0) ? flipInfo.out_fen_fd : -1;
    }

    return 0;
}

int32_t HwDisplayCrtcFbdev::getHdrMetadataKeys(
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

int32_t HwDisplayCrtcFbdev::setHdrMetadata(
    std::map<drm_hdr_meatadata_t, float> & hdrmedata) {
    if (updateHdrMetadata(hdrmedata) == true)
        return set_hdr_info((vframe_master_display_colour_s_t*)hdrVideoInfo);

    return 0;
}

bool HwDisplayCrtcFbdev::updateHdrMetadata(
    std::map<drm_hdr_meatadata_t, float> & hdrmedata) {
    vframe_master_display_colour_s_t newHdr;
    memset(&newHdr,0,sizeof(vframe_master_display_colour_s_t));
    if (!hdrmedata.empty()) {
        for (auto iter = hdrmedata.begin(); iter != hdrmedata.end(); ++iter) {
            switch (iter->first) {
                case DRM_DISPLAY_RED_PRIMARY_X:
                    newHdr.primaries[2][0] = (u32)(iter->second * 50000); //mR.x
                    break;
                case DRM_DISPLAY_RED_PRIMARY_Y:
                    newHdr.primaries[2][1] = (u32)(iter->second * 50000); //mR.Y
                    break;
                case DRM_DISPLAY_GREEN_PRIMARY_X:
                    newHdr.primaries[0][0] = (u32)(iter->second * 50000);//mG.x
                    break;
                case DRM_DISPLAY_GREEN_PRIMARY_Y:
                    newHdr.primaries[0][1] = (u32)(iter->second * 50000);//mG.y
                    break;
                case DRM_DISPLAY_BLUE_PRIMARY_X:
                    newHdr.primaries[1][0] = (u32)(iter->second * 50000);//mB.x
                    break;
                case DRM_DISPLAY_BLUE_PRIMARY_Y:
                    newHdr.primaries[1][1] = (u32)(iter->second * 50000);//mB.Y
                    break;
                case DRM_WHITE_POINT_X:
                    newHdr.white_point[0] = (u32)(iter->second * 50000);//mW.x
                    break;
                case DRM_WHITE_POINT_Y:
                    newHdr.white_point[1] = (u32)(iter->second * 50000);//mW.Y
                    break;
                case DRM_MAX_LUMINANCE:
                    newHdr.luminance[0] = (u32)(iter->second * 1000); //mMaxDL
                    break;
                case DRM_MIN_LUMINANCE:
                    newHdr.luminance[1] = (u32)(iter->second * 10000);//mMinDL
                    break;
                case DRM_MAX_CONTENT_LIGHT_LEVEL:
                    newHdr.content_light_level.max_content = (u32)(iter->second); //mMaxCLL
                    newHdr.content_light_level.present_flag = 1;
                    break;
                case DRM_MAX_FRAME_AVERAGE_LIGHT_LEVEL:
                    newHdr.content_light_level.max_pic_average = (u32)(iter->second);//mMaxFALL
                    newHdr.content_light_level.present_flag = 1;
                    break;
                default:
                    MESON_LOGE("unkown key %d",iter->first);
                    break;
            }
        }
    }

    if (memcmp(hdrVideoInfo, &nullHdr, sizeof(vframe_master_display_colour_s_t)) == 0)
        return false;
    newHdr.present_flag = 1;

    if (memcmp(hdrVideoInfo, &newHdr, sizeof(vframe_master_display_colour_s_t)) == 0)
        return false;

    vframe_master_display_colour_s_t * hdrinfo =
        (vframe_master_display_colour_s_t *)hdrVideoInfo;
    *hdrinfo = newHdr;
    return true;
}

void HwDisplayCrtcFbdev::closeLogoDisplay() {
    struct utsname buf;
    int major = 0;
    int minor = 0;
    if (uname(&buf) == 0) {
        if (sscanf(buf.release, "%d.%d", &major, &minor) != 2) {
            major = 0;
        }
    }
    if (major == 0)
        MESON_LOGE("Can't determine kernel version!");

    if (major >= 5) {
        sysfs_set_string(DISPLAY_LOGO_INDEX54, "-1");
    } else {
        sysfs_set_string(DISPLAY_LOGO_INDEX, "-1");
    }

    sysfs_set_string(DISPLAY_FB0_FREESCALE_SWTICH, "0x10001");
    sysfs_set_string(DISPLAY_FB0_FREE_FB_MEM, "1");
}

int32_t HwDisplayCrtcFbdev::readCurDisplayMode(std::string & dispmode) {
    const char *path = (mPipe == DRM_PIPE_VOUT1) ? VIU1_DISPLAY_MODE_SYSFS : VIU2_DISPLAY_MODE_SYSFS;
    int32_t ret = read_sysfs(path, dispmode);
    return ret;
}

int32_t HwDisplayCrtcFbdev::writeCurDisplayMode(std::string & dispmode) {
    const char *path = (mPipe == DRM_PIPE_VOUT1) ? VIU1_DISPLAY_MODE_SYSFS : VIU2_DISPLAY_MODE_SYSFS;
    return sysfs_set_string(path, dispmode.c_str());
}

int32_t HwDisplayCrtcFbdev::writeCurDisplayAttr(std::string & dispattr) {
    int32_t ret = 0;
    ret = sc_write_sysfs(VIU_DISPLAY_ATTR_SYSFS, dispattr);
    return ret;
}
