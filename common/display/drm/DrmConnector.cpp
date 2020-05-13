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
#include "DrmDevice.h"
#include <inttypes.h>
#include <limits>

#include <xf86drm.h>
#include <string.h>
#include <misc.h>
#include <systemcontrol.h>

#include "../include/AmVinfo.h"

#define EDID_MIN_LEN (128)

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
 //       {DRM_HDMI_PROP_COLORSPACE, &mColorSpace},
//        {DRM_HDMI_PROP_COLORDEPTH, &mColorDepth},
//        {DRM_HDMI_PROP_HDRCAP, &mHdrCaps},
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
    uint32_t blobid = 0;
    MESON_LOGD("Connector %s loadDisplayModes get %d modes", getName(), p->count_modes);
    for (int i = 0;i < p->count_modes; i ++) {
        strncpy(modeInfo.name, drmModes[i].name, DRM_DISPLAY_MODE_LEN);
        modeInfo.pixelW = drmModes[i].hdisplay;
        modeInfo.pixelH = drmModes[i].vdisplay;
        modeInfo.dpiX = (modeInfo.pixelW * 25.4f) / mPhyWidth * 1000;
        modeInfo.dpiY = (modeInfo.pixelH * 25.4f) / mPhyHeight * 1000;
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

        if (bNonFractionMode) {
            // add normal refresh rate config, like 24hz, 30hz...
            modeInfo.groupId = mMesonModes.size();
            mMesonModes.emplace(mMesonModes.size(), modeInfo);
        }

        mDrmModes.emplace(blobid, drmModes[i]);
        MESON_LOGI("add display mode (%s-%s-%d, %dx%d, %f)",
            drmModes[i].name, modeInfo.name, blobid,
            modeInfo.pixelW, modeInfo.pixelH, modeInfo.refreshRate);
    }

    MESON_LOGI("loadDisplayModes (%" PRIuFAST16 ") end", mMesonModes.size());
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
        loadDisplayModes(metadata);

        if (mType == DRM_MODE_CONNECTOR_HDMIA) {
            if (mPhyWidth == 0 || mPhyHeight == 0) {
                struct vinfo_base_s vinfo;
                if (read_vout_info(0, &vinfo) == 0) {
                    mPhyWidth = vinfo.screen_real_width;
                    mPhyHeight = vinfo.screen_real_height;
                    MESON_LOGE("read screen size %dx%d from vinfo",
                        mPhyWidth,mPhyHeight);
                }
            }
            parseHdmiHdrCapabilities(mHdrCapabilities);
        }
    } else {
        MESON_LOGE("DrmConnector[%s] still DISCONNECTED.", getName());
        memset(&mHdrCapabilities, 0, sizeof(mHdrCapabilities));
    }

    return 0;
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

bool DrmConnector::isConnected() {
    if (mState == DRM_MODE_CONNECTED)
        return true;

    if (mState == DRM_MODE_UNKNOWNCONNECTION)
        MESON_LOGE("Unknown connection state (%s)", getName());
    return false;
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
        if (strstr(drmmode.name, it.second.name)) {
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
    dumpstr.appendFormat("Connector (%s, %d, %d x %d, %s, %s) mId(%d) mCrtcId(%d) mFracMode(%d)\n",
        getName(), getType(), mPhyWidth, mPhyHeight,
        isSecure() ? "secure" : "unsecure", isConnected() ? "Connected" : "Removed",
        mId, getCrtcId(), mFracMode);

    //dump display config.
    if (mEdid)
        mEdid->dump(dumpstr);
    dumpstr.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    dumpstr.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");

    for ( auto it = mMesonModes.begin(); it != mMesonModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |  %12s  |      %.3f      |   %5d   |   %5d    |"
            "    %3d    |    %3d    \n",
                 it->first,
                 it->second.name,
                 it->second.refreshRate,
                 it->second.pixelW,
                 it->second.pixelH,
                 it->second.dpiX,
                 it->second.dpiY);
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
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return "sdr";

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
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return -ENOENT;

    return setHdmiContentType(contentType);
}

int32_t DrmConnector::setAutoLowLatencyMode(bool on) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return -ENOENT;

    return sc_set_hdmi_allm(on);
}

void DrmConnector::updateHdrCaps() {
    parseHdmiHdrCapabilities(mHdrCapabilities);
}

void DrmConnector::getHdrCapabilities(drm_hdr_capabilities * caps) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return;

    if (caps) {
        *caps = mHdrCapabilities;
    }
}

