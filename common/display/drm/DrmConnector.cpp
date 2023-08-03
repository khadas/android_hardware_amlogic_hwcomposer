/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <MesonLog.h>
#include "DrmConnector.h"
#include "DrmCrtc.h"
#include "DrmDevice.h"
#include <inttypes.h>
#include <limits>
#include <hardware/hwcomposer2.h>

#include <xf86drm.h>
#include <string.h>
#include <misc.h>
#include <systemcontrol.h>

#include "../include/AmVinfo.h"
#include "Dv.h"
#include "mode_ubootenv.h"

#define EDID_MIN_LEN (128)
#define HDMI_FRAC_RATE_POLICY "/sys/class/amhdmitx/amhdmitx0/frac_rate_policy"
#define HDMI_TX_ALLM_MODE   "/sys/class/amhdmitx/amhdmitx0/allm_cap"

#define HDR_POLICY          "/sys/module/aml_media/parameters/hdr_policy"
#define DV_POLICY           "/sys/module/aml_media/parameters/dolby_vision_policy"
#define DV_LL_POLICY        "/sys/module/aml_media/parameters/dolby_vision_ll_policy"
#define FORCE_DV_MODE       "sys/class/amdolby_vision/dv_mode"
#define FORCE_HDR_MODE      "/sys/module/aml_media/parameters/force_output"
#define HDMI_ATTR           "/sys/class/amhdmitx/amhdmitx0/attr"

#define HDR_POLICY_SINK               "0"
#define HDR_POLICY_SOURCE             "1"
#define HDR_POLICY_FORCE              "2"

#define FORCE_DV                 "2"
#define FORCE_HDR10              "3"
#define FORCE_HLG                "5"

#define DV_SINK_LED                   "0"
#define DV_SOURCE_LED                 "1"

#define DV_DISABLE_FORCE_SDR         "1"
#define DV_ENABLE_FORCE_SDR          "5"

static const u8 default_1080p_edid[EDID_MIN_LEN] = {
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
0x31, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x05, 0x16, 0x01, 0x03, 0x6d, 0x32, 0x1c, 0x78,
0xea, 0x5e, 0xc0, 0xa4, 0x59, 0x4a, 0x98, 0x25,
0x20, 0x50, 0x54, 0x00, 0x00, 0x00, 0xd1, 0xc0,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a,
0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
0x45, 0x00, 0xf4, 0x19, 0x11, 0x00, 0x00, 0x1e,
0x00, 0x00, 0x00, 0xff, 0x00, 0x4c, 0x69, 0x6e,
0x75, 0x78, 0x20, 0x23, 0x30, 0x0a, 0x20, 0x20,
0x20, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x3b,
0x3d, 0x42, 0x44, 0x0f, 0x00, 0x0a, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
0x00, 0x4c, 0x69, 0x6e, 0x75, 0x78, 0x20, 0x46,
0x48, 0x44, 0x0a, 0x20, 0x20, 0x20, 0x00, 0x05,
};

/*TODO: re-use legacy hdmi sysfs.*/
extern int32_t parseHdmiHdrCapabilities(drm_hdr_capabilities & hdrCaps);
extern bool loadHdmiCurrentHdrType(std::string & hdrType);
extern int32_t loadHdmiSupportedContentTypes(std::vector<uint32_t> & supportedContentTypes);
extern int32_t setHdmiContentType(uint32_t contentType);
extern int32_t switchRatePolicy(bool fracRatePolicy);
extern bool getFracModeStatus();

DrmConnector::DrmConnector(int drmFd, drmModeConnectorPtr p)
    : HwDisplayConnector(),
    mDrmFd(drmFd),
    mId(p->connector_id),
    mType(p->connector_type) {

    mFracMode = HWC_HDMI_FRAC_MODE;
    loadConnectorInfo(p);
    initConversionCaps();
}

DrmConnector::~DrmConnector() {

}

int32_t DrmConnector::loadProperties(drmModeConnectorPtr p __unused) {
    ATRACE_CALL();
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } connectorProps[] = {
        {DRM_CONNECTOR_PROP_CRTCID, &mCrtcId},
        {DRM_CONNECTOR_PROP_EDID, &mEdid},
        {DRM_CONNECTOR_PROP_UPDATE, &mUpdate},
        {DRM_CONNECTOR_PROP_MESON_TYPE, &mMesonConnectorType},
        {DRM_CONNECTOR_PROP_VRRCAP, &mVrrCap},
        {DRM_HDMI_PROP_COLORSPACE, &mColorSpace},
        {DRM_HDMI_PROP_COLORDEPTH, &mColorDepth},
//        {DRM_HDMI_PROP_HDRCAP, &mHdrCaps},
        {DRM_HDMI_PROP_HDR_STATUS, &mHdrStatus},
        {DRM_HDMI_PROP_CONTENT_TYPE, &mContentType},
        {DRM_HDMI_PROP_HDMI_AV_MUTE, &mAVMute},
        {DRM_HDMI_PROP_DV_CAP, &mDvCaps},
        {DRM_CONNECTOR_PROP_CONVERSION_CAP, &mHdrConversionCaps},
        {DRM_CONNECTOR_PROP_FORCE_MODE, &mHdrForceMode}
    };
    const int connectorPropsNum = sizeof(connectorProps)/sizeof(connectorProps[0]);

    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(mDrmFd, mId, DRM_MODE_OBJECT_CONNECTOR);
    MESON_ASSERT(props != NULL, "DrmConnector::loadProperties failed.");

    for (int i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(mDrmFd, props->props[i]);
        if (strcmp(prop->name, DRM_CONNECTOR_PROP_CRTCID) == 0 && mCrtcId.get()) {
            //TODO: WA for only set crtcid prop at initialization
            continue;
        }
        for (int j = 0; j < connectorPropsNum; j++) {
            if (strcmp(prop->name, connectorProps[j].propname) == 0) {
                *(connectorProps[j].drmprop) =
                    std::make_shared<DrmProperty>(prop, mId, props->prop_values[i]);
                break;
            }
        }
       drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    return 0;
}

int32_t DrmConnector::loadDisplayModes(drmModeConnectorPtr p) {
    ATRACE_CALL();
    /*clear old mode list.*/
    for (auto & it : mDrmModes)
        drmModeDestroyPropertyBlob(mDrmFd, it.first);
    mMesonModes.clear();
    mFracRefreshRates.clear();
    mDrmModes.clear();

    /*add new display mode list.*/
    drmModeModeInfoPtr drmModes = p->modes;
    drm_mode_info_t modeInfo;
    memset(&modeInfo, 0, sizeof(modeInfo));
    uint32_t blobid = 0;
    MESON_LOGD("Connector %s loadDisplayModes get %d modes", getName(), p->count_modes);
    for (int i = 0;i < p->count_modes; i ++) {
        strncpy(modeInfo.name, drmModes[i].name, DRM_DISPLAY_MODE_LEN - 1);
        modeInfo.pixelW = drmModes[i].hdisplay;
        modeInfo.pixelH = drmModes[i].vdisplay;
        if (mPhyWidth != 0 && mPhyHeight != 0) {
            modeInfo.dpiX = (modeInfo.pixelW * 25.4f) / mPhyWidth * 1000;
            modeInfo.dpiY = (modeInfo.pixelH * 25.4f) / mPhyHeight * 1000;
        }
        modeInfo.refreshRate = drmModes[i].vrefresh;

        if (drmModeCreatePropertyBlob(mDrmFd, &drmModes[i], sizeof(drmModes[i]), &blobid) != 0) {
            MESON_LOGE("CreateProp for mode failed %s", modeInfo.name);
            continue;
        }

        bool bNonFractionMode = false;
        if (mType == DRM_MODE_CONNECTOR_HDMIA) {
            // default add frac refresh rate config, like 23.976hz, 29.97hz...
            if (modeInfo.refreshRate == REFRESH_24kHZ
                    || modeInfo.refreshRate == REFRESH_30kHZ
                    || modeInfo.refreshRate == REFRESH_60kHZ
                    || modeInfo.refreshRate == REFRESH_120kHZ
                    || modeInfo.refreshRate == REFRESH_240kHZ) {
                    if (mFracMode == MODE_ALL || mFracMode == MODE_FRACTION) {
                        drm_mode_info_t fracMode = modeInfo;
                        fracMode.refreshRate = (modeInfo.refreshRate * 1000) / (float)1001;
                        fracMode.groupId = mMesonModes.size();
                        mMesonModes.emplace(mMesonModes.size(), fracMode);
                        mFracRefreshRates.push_back(fracMode.refreshRate);
                        MESON_LOGD("add fraction display mode (%s)", fracMode.name);
                    }
            } else {
                bNonFractionMode = true;
            }
        }

        if (mType != DRM_MODE_CONNECTOR_HDMIA ||
            mFracMode == MODE_ALL || mFracMode == MODE_NON_FRACTION) {
            bNonFractionMode = true;
        }
         // drm only send frame rate (int)59hz, its real frac refresh rate is 59.94hz
        if (modeInfo.refreshRate == 59.00 || modeInfo.refreshRate == 119.00) {
            modeInfo.refreshRate = ((modeInfo.refreshRate + 1) * 1000) / (float)1001;
        }

        if (bNonFractionMode) {
            // add normal refresh rate config, like 24hz, 30hz...
            modeInfo.groupId = mMesonModes.size();
            mMesonModes.emplace(mMesonModes.size(), modeInfo);
        }

        mDrmModes.emplace(blobid, drmModes[i]);
        MESON_LOGI("add display mode (%s-%s-%d, %dx%d, %f) dpi(%d,%d)",
            drmModes[i].name, modeInfo.name, blobid,
            modeInfo.pixelW, modeInfo.pixelH, modeInfo.refreshRate,
            modeInfo.dpiX, modeInfo.dpiY);
    }

    MESON_LOGI("loadDisplayModes (%" PRIuFAST16 ") end", mMesonModes.size());
    return 0;
}

bool DrmConnector::isTvType() {
    if (mType == DRM_MODE_CONNECTOR_MESON_LVDS_A || mType == DRM_MODE_CONNECTOR_MESON_LVDS_B ||
            mType == DRM_MODE_CONNECTOR_MESON_LVDS_C || mType == DRM_MODE_CONNECTOR_MESON_VBYONE_A ||
            mType == DRM_MODE_CONNECTOR_MESON_VBYONE_B || mType == DRM_MODE_CONNECTOR_LVDS)
        return true;

    return false;
}

bool DrmConnector::supportVrr() {
    if (mVrrCap && mVrrCap->getValue() == 1)
        return true;

    return false;
}

/*
 * If the connector support seamless mode swith,
 * Then the modes with the same resolution can switch seamless to each other
 * and they have the same group Id.
 */
int32_t DrmConnector::groupDisplayModes() {
    /* no need to regenerate groupId if without QMS/VRR support */
    if (!isTvType()) {
        if (!(mSupportVrr = supportVrr())) {
            return 0;
        }
    }
    /* clear the old group modes */
    mMesonGroupModes.clear();

    for (auto & mode : mMesonModes) {
        auto & itMode = mode.second;
        bool needRegroup = true;
        for (auto & groupModes : mMesonGroupModes) {
            int groupId = groupModes.first;
            auto& itGroupModes = groupModes.second;

            // do not check interlace mode,
            // interlace mode does not support vrr
            if (strstr(itMode.name, "i") != NULL)
                break;

            // only support resolution >= 720P
            if (itMode.pixelW < 1280 || itMode.pixelH < 720)
                break;

            if (!itGroupModes.empty()) {
                /* only need check the first item*/
                drm_mode_info_t *gmodePtr = itGroupModes[0];
                if (gmodePtr->pixelW == itMode.pixelW && gmodePtr->pixelH == itMode.pixelH) {
                    itMode.groupId = groupId;
                    itGroupModes.push_back(&itMode);
                    needRegroup = false;
                    break;
                }
            }
        }

        if (needRegroup) {
            itMode.groupId = mMesonGroupModes.size();
            auto iter = mMesonGroupModes.emplace(mMesonGroupModes.size(),
                    std::vector<drm_mode_info_t *> ());
            auto & modesWithSameGroup = iter.first->second;
            modesWithSameGroup.push_back(&itMode);
        }
    }

    return 0;
}

int32_t DrmConnector::loadConnectorInfo(drmModeConnectorPtr metadata) {
    std::lock_guard<std::mutex> lock(mMutex);
    /*update state*/
    mState = metadata->connection;

    /*update prop*/
    loadProperties(metadata);

    if (mState == DRM_MODE_CONNECTED) {
        mEncoderId = metadata->encoder_id;
        mPhyWidth = metadata->mmWidth;
        mPhyHeight = metadata->mmHeight;

        if (mPhyWidth == 0 || mPhyHeight == 0) {
            struct vinfo_base_s vinfo;
            if (read_vout_info(0, &vinfo) == 0) {
                mPhyWidth = vinfo.screen_real_width;
                mPhyHeight = vinfo.screen_real_height;
                MESON_LOGE("read screen size %dx%d from vinfo",
                    mPhyWidth,mPhyHeight);
            }
        }

        updateHdrCaps();
        loadDisplayModes(metadata);
        groupDisplayModes();
    } else {
        MESON_LOGE("DrmConnector[%s] still DISCONNECTED.", getName());
        memset(&mHdrCapabilities, 0, sizeof(mHdrCapabilities));
    }

    return 0;
}

#if (PLATFORM_SDK_VERSION >= 34)
/*use drm property for dv_cap*/
int32_t DrmConnector:: parseDvCapabilities() {
    if (mDvCaps) {
        if (mDvCaps->getValue() != 0 && mSupportDv) {
            mHdrCapabilities.DolbyVisionSupported = true;
            if ((mDvCaps->getValue() & (1 << 2)) == 0)
                mHdrCapabilities.DOLBY_VISION_4K30_Supported = true;
        }

        MESON_LOGD("dolby version 4k30:%d",
                mHdrCapabilities.DOLBY_VISION_4K30_Supported ? 1 : 0);
    } else {
        MESON_LOGD("%s mDvCaps is null",__func__);
    }

    return 0;
}
#endif

bool DrmConnector::supportSourceLed() {
    if (mHdrCapabilities.DolbyVisionSupported &&
            mDvCaps->getValue() & ((1 << 4) | (1 << 5) | (1 << 7)))
        return true;
    else
        return false;
}

bool DrmConnector::supportSinkLed() {
    if (mHdrCapabilities.DolbyVisionSupported &&
            mDvCaps->getValue() & ((1 << 3) | (1 << 6)))
        return true;
    else
        return false;
}

/* bit0: SDR->HDR10 */
/* bit1: SDR->HLG */
/* bit2: HDR10->SDR */
/* bit3: HDR10->HLG */
/* bit4: HLG->SDR */
/* bit5: HLG->HDR10 */
/* bit6: HDR10_PLUS->HDR10 */
/* bit7: HDR10_PLUS->SDR */
/* bit8: HDR10_PLUS->HLG */
/* bit9: CUVA_HDR->SDR */
/* bit10: CUVA_HDR->HDR10 */
/* bit11: CUVA_HDR->HLG */
/* bit12: CUVA_HLG->SDR */
/* bit13: CUVA_HLG->HDR10 */
/* bit14: CUVA_HLG->HLG */
/* bit15: DOLBY_VISION->SDR */
/* bit16: SDR->DOLBY_VISION */
/* bit17: DOLBY_VISION->HDR10*/
/* bit18: HDR10->DOLBY_VISION */
/* bit19: HLG->DOLBY_VISION */
int32_t DrmConnector::initConversionCaps() {
    mDrmHdrConversionCaps.clear();
    if (mHdrConversionCaps) {
        uint32_t value = mHdrConversionCaps->getValue();
        MESON_LOGD("%s mHdrCoversionCaps:0x%x",__func__, value);
        if (value & (1 << 0))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_HDR10, 0});
        if (value & (1 << 1))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_HLG, 0});
        if (value & (1 << 2))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_INVALID, 0});
        if (value & (1 << 3))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_HLG, 0});
        if (value & (1 << 4))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_INVALID, 0});
        if (value & (1 << 5))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_HDR10, 0});
        if (value & (1 << 6))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_HDR10, 0});
        if (value & (1 << 7))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_INVALID, 0});
        if (value & (1 << 8))
            mDrmHdrConversionCaps.push_back({DRM_HDR10_PLUS, DRM_HLG, 0});
        if (value & (1 << 15))
            mDrmHdrConversionCaps.push_back({DRM_DOLBY_VISION, DRM_INVALID, 0});
        if (value & (1 << 16))
            mDrmHdrConversionCaps.push_back({DRM_INVALID, DRM_DOLBY_VISION, 0});
        if (value & (1 << 17))
            mDrmHdrConversionCaps.push_back({DRM_DOLBY_VISION, DRM_HDR10, 0});
        if (value & (1 << 18))
            mDrmHdrConversionCaps.push_back({DRM_HDR10, DRM_DOLBY_VISION, 0});
        if (value & (1 << 19))
            mDrmHdrConversionCaps.push_back({DRM_HLG, DRM_DOLBY_VISION, 0});
    }
    return 0;
}

int32_t DrmConnector::getConversionCaps(std::vector<drm_hdr_conversion_capability>& hdrconversionCaps) {
    hdrconversionCaps.assign(mDrmHdrConversionCaps.begin(), mDrmHdrConversionCaps.end());
    return 0;
}

int32_t DrmConnector::setHdrConversionStrategy (bool passthrough, int32_t forceType) {
    int32_t ret = 0;
    MESON_LOGD("%s passthrough %d forceType %d ", __func__, passthrough, forceType);

    if (passthrough) {
        /*follow source*/
       if (!mSupportDv) {
           sysfs_set_string(HDR_POLICY, HDR_POLICY_SOURCE);
           sysfs_set_string(DV_POLICY, HDR_POLICY_SINK);
       } else {
           sysfs_set_string(DV_POLICY, HDR_POLICY_SOURCE);
           if (supportSinkLed()) {
               sysfs_set_string(DV_LL_POLICY, DV_SINK_LED);
           } else if (supportSourceLed()) {
               sysfs_set_string(DV_LL_POLICY, DV_SOURCE_LED);
           }
       }
        meson_mode_set_ubootenv(UBOOTENV_HDR_POLICY, HDR_POLICY_SOURCE);
    } else {
        /*force mode need set attr before switch policy */
        switch (forceType) {
            case DRM_DOLBY_VISION: {
                if (supportSinkLed()) {
                    /*444,8bit*/
                    sysfs_set_string(HDMI_ATTR, "444,8bit");
                } else {
                    /*422,12bit*/
                    sysfs_set_string(HDMI_ATTR, "422,12bit");
                }
                sysfs_set_string(DV_POLICY, HDR_POLICY_FORCE);
                sysfs_set_string(FORCE_DV_MODE, FORCE_DV);
                break;
            }
            case DRM_HDR10: {
                /*enable dv force HDR10 need ColorDepth >= 10 bit*/
                if (mSupportDv && mColorSpace && mColorDepth && mColorDepth->getValue() < 10) {
                    sysfs_set_string(HDMI_ATTR, "420,10bit");
                }

                if (!mSupportDv) {
                    sysfs_set_string(HDR_POLICY, HDR_POLICY_FORCE);
                    sysfs_set_string(FORCE_HDR_MODE, FORCE_HDR10);
                }
                else {
                    sysfs_set_string(DV_POLICY, HDR_POLICY_FORCE);
                    sysfs_set_string(FORCE_DV_MODE, FORCE_HDR10);
                }
                break;
            }
            case DRM_HLG: {
                sysfs_set_string(HDR_POLICY, HDR_POLICY_FORCE);
                sysfs_set_string(FORCE_HDR_MODE, FORCE_HLG);
                break;
            }
            case DRM_INVALID: {
                if (!mSupportDv) {
                    sysfs_set_string(HDR_POLICY, HDR_POLICY_FORCE);
                    sysfs_set_string(FORCE_HDR_MODE, DV_DISABLE_FORCE_SDR);
                }
                else {
                    sysfs_set_string(DV_POLICY, HDR_POLICY_FORCE);
                    sysfs_set_string(FORCE_DV_MODE, DV_ENABLE_FORCE_SDR);
                }
                break;
            }
            default:
                MESON_LOGE("setHdrConversionStrategy: error type[%d]", forceType);
                ret = HWC2_ERROR_UNSUPPORTED;
                break;
        }

        if (!mSupportDv && !ret)
            meson_mode_set_ubootenv(UBOOTENV_HDR_POLICY, HDR_POLICY_FORCE);
    }
    return ret;
}


/* use Drmproperty for forcemode
 * ToDo*/
int32_t DrmConnector::setForceMode(uint32_t value) {
    int ret = -1;
    if (mHdrForceMode)
        ret = mHdrForceMode->setValue(value);
    if (ret != 0)
        return -EINVAL;
    else
        return 0;
}

/* use Drmproperty for attr
 * ToDo*/
int32_t DrmConnector::setAttrForDV(uint32_t color_space_value, uint32_t color_depth_value) {
    int ret = -1;
    int rtn = -1;
    if (mColorSpace && mColorDepth) {
        if (mColorSpace->getValue() == color_space_value
                && mColorDepth->getValue() == color_depth_value) {
            return 0;
        }
        ret = mColorSpace->setValue(color_space_value);
        rtn = mColorDepth->setValue(color_depth_value);
    }
    if (ret == 0 && rtn == 0 && mCrtcId) {
        std::shared_ptr<HwDisplayCrtc> displayCrtc;
        displayCrtc = getDrmDevice()->getCrtcById(mCrtcId->getValue());
        if (displayCrtc) {
            DrmCrtc * crtc = (DrmCrtc *)displayCrtc.get();
            crtc->prePageFlip();
            drmModeAtomicReqPtr req = crtc->getAtomicReq();
            mColorSpace->apply(req);
            mColorDepth->apply(req);
            crtc->updatePropertyValue();
            return 0;
        } else {
           return -EINVAL;
        }
    }
    else {
        return -EINVAL;
    }
}

uint32_t DrmConnector::getId() {
    return mId;
}

const char * DrmConnector::getName() {
    const char *name = drmConnTypeToString(getType());
    MESON_ASSERT(name, "%s: get name for %d fail.",
        __func__, getType());
    return name;
}

drm_connector_type_t DrmConnector::getType() {
    /*check extend type first*/
    if (mMesonConnectorType.get()) {
        int meson_type = mMesonConnectorType->getValue();
        MESON_LOGD("%s get connector %x", __func__, meson_type);
        return meson_type;
    }

    return mType;
}

int32_t DrmConnector::update() {
    ATRACE_CALL();
    drmModeConnectorPtr metadata = drmModeGetConnector(mDrmFd, mId);
    loadConnectorInfo(metadata);
    drmModeFreeConnector(metadata);
    return 0;
}

int32_t DrmConnector::setCrtcId(uint32_t crtcid) {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCrtcId->setValue(crtcid);
}

uint32_t DrmConnector::getCrtcId() {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mCrtcId ? numeric_limits<uint32_t>::max() : mCrtcId->getValue();
}

int32_t DrmConnector::getModes(
    std::map<uint32_t, drm_mode_info_t> & modes) {
    std::lock_guard<std::mutex> lock(mMutex);
    modes = mMesonModes;
    return 0;
}

int32_t DrmConnector::setMode(drm_mode_info_t & mode) {
    if (mFracMode == MODE_NON_FRACTION ||
        mType != DRM_MODE_CONNECTOR_HDMIA)
        return 0;

    /*update rate policy.*/
    for (auto it = mFracRefreshRates.begin(); it != mFracRefreshRates.end(); it ++) {
        if (*it == mode.refreshRate) {
            switchRatePolicy(true);
            return 0;
        }
    }

    switchRatePolicy(false);
    return 0;
}

bool DrmConnector::checkFracMode(const drm_mode_info_t & mode) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return true;

    if (mFracMode != MODE_ALL) {
        return true;
    }

    // only check frac refresh rate
    if (mode.refreshRate != REFRESH_25kHZ
            && mode.refreshRate != REFRESH_50kHZ) {
        [[maybe_unused]] bool currentIsFrac =
            sysfs_get_int(HDMI_FRAC_RATE_POLICY, 1) == 1 ? true : false;
        bool modeIsFrac =
            std::find(mFracRefreshRates.begin(), mFracRefreshRates.end(),
                    mode.refreshRate) != mFracRefreshRates.end();

#ifndef ENABLE_AIDL
        return (currentIsFrac && modeIsFrac) || (!currentIsFrac && !modeIsFrac);
#else
        // TODO:refactor it when mode policy move to hwc
        // for boot config, preferred is frac policy
        return modeIsFrac;
#endif
    }

    return true;
}

bool DrmConnector::isConnected() {
    if (mState == DRM_MODE_CONNECTED)
        return true;

    if (mState == DRM_MODE_UNKNOWNCONNECTION)
        MESON_LOGE("Unknown connection state (%s)", getName());
    return false;
}

bool DrmConnector:: isTvSupportALLM() {
    return sysfs_get_int(HDMI_TX_ALLM_MODE, 0) == 1 ? true : false;
}

int32_t DrmConnector::getIdentificationData(std::vector<uint8_t>& idOut) {
    int32_t ret = 0;

    if (mEdid)
        ret = mEdid->getBlobData(idOut);

    /*return default edid str for enable multidisplay feature in android.*/
    if (ret != 0 || idOut.size() < EDID_MIN_LEN) {
        idOut.clear();
        MESON_LOGD("No edid for (%s),use default edid instead.", getName());
        for (int i = 0; i < EDID_MIN_LEN; i++) {
            idOut.push_back(default_1080p_edid[i]);
        }
    }

    return 0;
}

int DrmConnector::getCrtcProp(std::shared_ptr<DrmProperty> & prop) {
    prop = mCrtcId;
    return 0;
}

int DrmConnector::getUpdateProp(std::shared_ptr<DrmProperty> & prop) {
    prop = mUpdate;
    return 0;
}

uint32_t DrmConnector::getModeBlobId(drm_mode_info_t & mode) {
    for (const auto & it : mDrmModes) {
        if (!strcmp(it.second.name, mode.name)) {
            return it.first;
        }
    }

    MESON_LOGE("getModeBlobId failed [%s]", mode.name);
    return 0;
}

int DrmConnector::getDrmModeByBlobId(drmModeModeInfo & drmmode, uint32_t blobid) {
    auto it = mDrmModes.find(blobid);
    if (it != mDrmModes.end()) {
        memcpy(&drmmode, &(it->second), sizeof(drmModeModeInfo));
        return 0;
    }
    return -EINVAL;
}

int DrmConnector::DrmMode2Mode(drmModeModeInfo & drmmode, drm_mode_info_t & mode) {
    for (const auto & it : mMesonModes) {
        if (!strncmp(drmmode.name, it.second.name, DRM_DISPLAY_MODE_LEN)) {
            if (mType == DRM_MODE_CONNECTOR_HDMIA) {
                if (mFracMode == MODE_ALL || mFracMode == MODE_FRACTION) {
                    /* frac mode refresh rate */
                    if (drmmode.vrefresh == REFRESH_24kHZ
                            || drmmode.vrefresh == REFRESH_30kHZ
                            || drmmode.vrefresh == REFRESH_60kHZ
                            || drmmode.vrefresh == REFRESH_120kHZ
                            || drmmode.vrefresh == REFRESH_240kHZ) {
                        if (getFracModeStatus()) { // is enable
                            if (it.second.refreshRate == drmmode.vrefresh)
                                continue;
                        } else { // is disable
                            if (it.second.refreshRate == ((drmmode.vrefresh * 1000) / (float)1001))
                                continue;
                        }
                    }
                }
            }

            memcpy(&mode, &(it.second), sizeof(drm_mode_info_t));
            return 0;
        }
    }

    MESON_LOGE("DrmMode2Mode find drm mode failed (%s)", drmmode.name);
    return -EINVAL;
}

void DrmConnector::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Connector (%s, %d, %d x %d, %s, %s) mId(%d)"
        " mCrtcId(%d) mFracMode(%d) vrrCap(%d)\n",
        getName(), getType(), mPhyWidth, mPhyHeight,
        isSecure() ? "secure" : "unsecure",
        isConnected() ? "Connected" : "Removed",
        mId, getCrtcId(), mFracMode, supportVrr());

    //dump display config.
    if (mEdid)
        mEdid->dump(dumpstr);
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   | GroupId \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");

    for ( auto it = mMesonModes.begin(); it != mMesonModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |  %12s  |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    |    %3d   \n",
                 it->first,
                 it->second.name,
                 it->second.refreshRate,
                 it->second.pixelW,
                 it->second.pixelH,
                 it->second.dpiX,
                 it->second.dpiY,
                 it->second.groupId);
    }

#if 0
    for ( auto it = mDrmModes.begin(); it != mDrmModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |  %12s  |  %2d    |   %5d   |   %5d    |\n",
                 it->first,
                 it->second.name,
                 it->second.vrefresh,
                 it->second.hdisplay,
                 it->second.vdisplay);
    }
#endif
    dumpstr.append("---------------------------------------------------------"
        "----------------------------------\n");
}

/*LEGACY IMPLEMENT: MOVE TO DRM LATER.*/
bool DrmConnector::isSecure() {
    /*hwc internal use only, may to remove.*/
    return true;
}

std::string DrmConnector::getCurrentHdrType() {
    std::string hdrType;
    loadHdmiCurrentHdrType(hdrType);
    return hdrType;
}

void DrmConnector::getSupportedContentTypes(
    std::vector<uint32_t> & supportedContentTypesOut) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return;

    loadHdmiSupportedContentTypes(supportedContentTypesOut);
}

int32_t DrmConnector::setContentType(uint32_t contentType) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA) {
        /* while mType not HDMI but contentType is 0, return*/
        if (contentType == CONTENT_TYPE_NONE) {
            return 0;
        } else {
            return -ENOENT;
        }
    }
    return setHDMIContentType(contentType);
}

int32_t DrmConnector::setAutoLowLatencyMode(bool on) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    if (isTvSupportALLM()) {
        return sc_set_hdmi_allm(on);
    } else {
        return HWC2_ERROR_UNSUPPORTED;
    }
}

void DrmConnector::updateHdrCaps() {
    memset(&mHdrCapabilities, 0, sizeof(drm_hdr_capabilities));
    mSupportDv = getDvSupportStatus();

    if (mType == DRM_MODE_CONNECTOR_HDMIA) {
        parseHdmiHdrCapabilities(mHdrCapabilities);
#if (PLATFORM_SDK_VERSION >= 34)
        parseDvCapabilities();
#endif
    }
    /* for TV product*/
    if (mType == DRM_MODE_CONNECTOR_MESON_LVDS_A || mType == DRM_MODE_CONNECTOR_MESON_LVDS_B ||
            mType == DRM_MODE_CONNECTOR_MESON_LVDS_C || mType == DRM_MODE_CONNECTOR_MESON_VBYONE_A ||
            mType == DRM_MODE_CONNECTOR_MESON_VBYONE_B || mType == DRM_MODE_CONNECTOR_LVDS) {
        constexpr int sDefaultMinLumiance = 0;
        constexpr int sDefaultMaxLumiance = 500;

        mHdrCapabilities.DolbyVisionSupported = mSupportDv;
        mHdrCapabilities.HLGSupported = true;
        mHdrCapabilities.HDR10Supported = true;
        mHdrCapabilities.HDR10PlusSupported = true;
        mHdrCapabilities.maxLuminance = sDefaultMaxLumiance;
        mHdrCapabilities.avgLuminance = sDefaultMaxLumiance;
        mHdrCapabilities.minLuminance = sDefaultMinLumiance;
#if (PLATFORM_SDK_VERSION >= 34)
        parseDvCapabilities();
#endif
    MESON_LOGD("dolby version:%d, hlg:%d, hdr10:%d, hdr10+:%d max:%d, avg:%d, min:%d\n",
        mHdrCapabilities.DolbyVisionSupported ? 1 : 0,
        mHdrCapabilities.HLGSupported ? 1 : 0,
        mHdrCapabilities.HDR10Supported ? 1 : 0,
        mHdrCapabilities.HDR10PlusSupported ? 1 : 0,
        mHdrCapabilities.maxLuminance,
        mHdrCapabilities.avgLuminance,
        mHdrCapabilities.minLuminance);
    }
}

void DrmConnector::getHdrCapabilities(drm_hdr_capabilities * caps) {
    if (caps) {
        *caps = mHdrCapabilities;
    }
}
int32_t DrmConnector::setHDMIContentType(uint32_t contentType)
{
    if ( contentType >= HDMI_CONTENT_TYPE_MAX || (!mContentType))
        return -EINVAL;
    int ret = -1;
    ret = mContentType->setValue(contentType);
    if (ret == 0 && mCrtcId) {
        std::shared_ptr<HwDisplayCrtc> displayCrtc;
        displayCrtc = getDrmDevice()->getCrtcById(mCrtcId->getValue());
        if (displayCrtc) {
           DrmCrtc * crtc = (DrmCrtc *)displayCrtc.get();
           drmModeAtomicReqPtr req = crtc->getAtomicReq();
           mContentType->apply(req);
           return 0;
        }
        else {
            return -EINVAL;
        }
    }
    else {
        return -EINVAL;
    }
}
int32_t DrmConnector::setAVMute(uint32_t mute)
{
    MESON_LOGI("setAVMute (%d)", mute);
    if (!mAVMute)
        return -EINVAL;
    int ret = -1;
    ret = mAVMute->setValue(mute);
    if (ret == 0 && mCrtcId) {
        std::shared_ptr<HwDisplayCrtc> displayCrtc;
        displayCrtc = getDrmDevice()->getCrtcById(mCrtcId->getValue());
        if (displayCrtc) {
           DrmCrtc * crtc = (DrmCrtc *)displayCrtc.get();
           drmModeAtomicReqPtr req = crtc->getAtomicReq();
           mAVMute->apply(req);
           return 0;
        }
        else {
            return -EINVAL;
        }
    }
    else {
        return -EINVAL;
    }
}
bool DrmConnector::getHdrType(std::string & hdrType)
{
    bool ret = true;
    ENUM_DRM_HDR_TYPE hdr_type_enum = DRM_SDR;
    if (mHdrStatus) {
        hdr_type_enum = (ENUM_DRM_HDR_TYPE)mHdrStatus->getValue();
    } else {
        hdrType = "SDR";
        ret = false;
        return ret;
    }
    switch (hdr_type_enum) {
        case DRM_HDR10PLUS:
            hdrType = "HDR10Plus-VSIF";
            break;
        case DRM_DOLBYVISION_STD:
            hdrType = "DolbyVision-Std";
            break;
        case DRM_DOLBYVISION_LL:
            hdrType = "DolbyVision-Lowlatency";
            break;
        case DRM_HDR10_ST2084:
            hdrType = "HDR10-GAMMA_ST2084";
            break;
        case DRM_HDR10_TRADITIONAL:
            hdrType = "HDR10-others";
            break;
        case DRM_HDR_HLG:
            hdrType = "HDR10-GAMMA_HLG";
            break;
        case DRM_SDR:
            hdrType = "SDR";
            break;
        default:
            hdrType = "SDR";
            break;
    }
    return ret;
}
int32_t DrmConnector:: getColorDepth(uint32_t & colorDepth )
{
    if (mColorDepth)
        colorDepth = mColorDepth->getValue();
    else
        return -ENOENT;
    return 0;
}
ENUM_HDMI_COLOR_SPACE DrmConnector::getColorSpace()
{
    ENUM_HDMI_COLOR_SPACE color_space = HDMI_COLOR_SPACE_RESERVED;
    if (mColorSpace)
        color_space = (ENUM_HDMI_COLOR_SPACE)mColorSpace->getValue();
    return color_space;
}

