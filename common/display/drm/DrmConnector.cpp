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

#include <xf86drm.h>
#include <string.h>
#include <misc.h>

#include "../fbdev/AmVinfo.h"

/*TODO: re-use legacy hdmi sysfs.*/
extern int32_t parseHdmiHdrCapabilities(drm_hdr_capabilities & hdrCaps);
extern bool loadHdmiCurrentHdrType(std::string & hdrType);
extern int32_t setHdmiALLM(bool on);
extern int32_t loadHdmiSupportedContentTypes(std::vector<uint32_t> & supportedContentTypes);
extern int32_t setHdmiContentType(uint32_t contentType);
extern int32_t switchRatePolicy(bool fracRatePolicy);
extern bool getFracModeStatus();

DrmConnector::DrmConnector(int drmFd, drmModeConnectorPtr p)
    : HwDisplayConnector(),
    mDrmFd(drmFd),
    mId(p->connector_id),
    mType(p->connector_type) {

    mState = p->connection;
    mFracMode = HWC_HDMI_FRAC_MODE;
    loadProperties(p);

    if (mState == DRM_MODE_CONNECTED) {
        mEncoderId = p->encoder_id;
        mPhyWidth = p->mmWidth;
        mPhyHeight = p->mmHeight;
        loadDisplayModes(p);
        parseHdmiHdrCapabilities(mHdrCapabilities);
    } else {
        memset(&mHdrCapabilities, 0, sizeof(mHdrCapabilities));
    }
}

DrmConnector::~DrmConnector() {

}

int32_t DrmConnector::loadProperties(drmModeConnectorPtr p) {
    ATRACE_CALL();
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } connectorProps[] = {
        {DRM_CONNECTOR_PROP_CRTCID, &mCrtcId},
        {DRM_CONNECTOR_PROP_EDID, &mEdid},
        {DRM_CONNECTOR_PROP_UPDATE, &mUpdate},
 //       {DRM_HDMI_PROP_COLORSPACE, &mColorSpace},
//        {DRM_HDMI_PROP_COLORDEPTH, &mColorDepth},
//        {DRM_HDMI_PROP_HDRCAP, &mHdrCaps},
    };
    const int connectorPropsNum = sizeof(connectorProps)/sizeof(connectorProps[0]);
    int initedProps = 0;

    for (int i = 0; i < p->count_props; i++) {
        drmModePropertyPtr prop =
            drmModeGetProperty(mDrmFd, p->props[i]);
        for (int j = 0; j < connectorPropsNum; j++) {
            if (strcmp(prop->name, connectorProps[j].propname) == 0) {
                std::shared_ptr<DrmProperty> tmpprop= *connectorProps[j].drmprop;
                if (!tmpprop) {
                    *(connectorProps[j].drmprop) =
                        std::make_shared<DrmProperty>(prop, mId, p->prop_values[i]);
                } else {
                    tmpprop->setValue(p->prop_values[i]);
                }
                initedProps ++;
                break;
            }
        }
       drmModeFreeProperty(prop);
    }

    return 0;
}

int32_t DrmConnector::loadDisplayModes(drmModeConnectorPtr p) {
    ATRACE_CALL();
    /*clear old mode list.*/
    std::lock_guard<std::mutex> lock(mMutex);
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
#if 0
        const struct vinfo_s * mesonVinfo = NULL;

        if (false /*getType() == DRM_MODE_CONNECTOR_HDMIA*/) {
            mesonVinfo = findMatchedVoutMode(drmModes[i]);
        } else {
            vmode_e ivmode = vmode_name_to_mode(drmModes[i].name);
            mesonVinfo = get_tv_info(ivmode);
        }

        if (!mesonVinfo) {
            MESON_LOGE("find mode failed[%s %d x %d -%u]",
                    drmModes[i].name, drmModes[i].hdisplay, drmModes[i].vdisplay, drmModes[i].vrefresh);
            continue;
        }
#endif

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

        if (mFracMode == MODE_ALL || mFracMode == MODE_NON_FRACTION) {
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

uint32_t DrmConnector::getId() {
    return mId;
}

const char * DrmConnector::getName() {
    const char * name;
    switch(mType) {
        case DRM_MODE_CONNECTOR_HDMIA:
            name = "HDMI";
            break;
        case DRM_MODE_CONNECTOR_TV:
            name = "CVBS";
            break;
        case DRM_MODE_CONNECTOR_LVDS:
            name = "PANEL";
            break;
        default:
            name = "UNKNOWN";
            break;
    };

    return name;
}

drm_connector_type_t DrmConnector::getType() {
    return mType;
}

int32_t DrmConnector::update() {
    ATRACE_CALL();
    drmModeConnectorPtr metadata = drmModeGetConnector(mDrmFd, mId);
    /*update state*/
    mState = metadata->connection;
    mEncoderId = metadata->encoder_id;
    mPhyWidth = metadata->mmWidth;
    mPhyHeight = metadata->mmHeight;
    if (mState == DRM_MODE_CONNECTED) {
        /*update prop*/
        loadProperties(metadata);
        loadDisplayModes(metadata);
        parseHdmiHdrCapabilities(mHdrCapabilities);
    } else {
        MESON_LOGE("DrmConnector[%s] still DISCONNECTED.", getName());
    }

    drmModeFreeConnector(metadata);
    return 0;
}

int32_t DrmConnector::setCrtcId(uint32_t crtcid) {
    return mCrtcId->setValue(crtcid);
}

uint32_t DrmConnector::getCrtcId() {
    return (uint32_t)mCrtcId->getValue();
}

int32_t DrmConnector::getModes(
    std::map<uint32_t, drm_mode_info_t> & modes) {
    std::lock_guard<std::mutex> lock(mMutex);
    modes = mMesonModes;
    return 0;
}

int32_t DrmConnector::setMode(drm_mode_info_t & mode) {
    if (mFracMode == MODE_NON_FRACTION)
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
    if (mEdid) {
        return mEdid->getBlobData(idOut);
    }

    MESON_LOGD("No edid for connector (%s)", getName());
    return -EINVAL;
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
    std::lock_guard<std::mutex> lock(mMutex);
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

            memcpy(&mode, &(it.second), sizeof(drm_mode_info_t));
            return 0;
        }
    }

    MESON_LOGE("DrmMode2Mode find drm mode failed (%s)", drmmode.name);
    return -EINVAL;
}

void DrmConnector::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Connector (%s, %d x %d, %s, %s) mId(%d) mCrtcId(%d) mFracMode(%d)\n",
        getName(), mPhyWidth, mPhyHeight,
        isSecure() ? "secure" : "unsecure", isConnected() ? "Connected" : "Removed",
        mId, getCrtcId(), mFracMode);

    //dump display config.
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

    for ( auto it = mDrmModes.begin(); it != mDrmModes.end(); ++it) {
        dumpstr.appendFormat(" %2d     |  %12s  |  %2d    |   %5d   |   %5d    |\n",
                 it->first,
                 it->second.name,
                 it->second.vrefresh,
                 it->second.hdisplay,
                 it->second.vdisplay);
    }

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

    return setHdmiALLM(on);
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

