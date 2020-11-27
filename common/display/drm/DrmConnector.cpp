/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

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


DrmConnector::DrmConnector(int drmFd, drmModeConnectorPtr p)
    : HwDisplayConnector(),
    mDrmFd(drmFd),
    mId(p->connector_id),
    mType(p->connector_type) {

    mState = p->connection;
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
    /*clear old mode list.*/
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto & it : mDrmModes)
        drmModeDestroyPropertyBlob(mDrmFd, it.first);
    mMesonModes.clear();
    mDrmModes.clear();

    /*add new display mode list.*/
    drmModeModeInfoPtr drmModes = p->modes;
    drm_mode_info_t modeInfo;
    uint32_t blobid = 0;
    MESON_LOGD("Connector %s loadDisplayModes get %d modes", getName(), p->count_modes);
    for (int i = 0;i < p->count_modes; i ++) {
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

        strncpy(modeInfo.name, mesonVinfo->name, DRM_DISPLAY_MODE_LEN);
        modeInfo.pixelW = drmModes[i].hdisplay;
        modeInfo.pixelH = drmModes[i].vdisplay;
        modeInfo.dpiX = (modeInfo.pixelW * 25.4f) / mPhyWidth;
        modeInfo.dpiY = (modeInfo.pixelH * 25.4f) / mPhyHeight;
        modeInfo.refreshRate = drmModes[i].vrefresh;

        if (drmModeCreatePropertyBlob(mDrmFd, &drmModes[i], sizeof(drmModes[i]), &blobid) != 0) {
            MESON_LOGE("CreateProp for mode failed %s", mesonVinfo->name);
            continue;
        }

        mDrmModes.emplace(blobid, drmModes[i]);
        mMesonModes.emplace(blobid, modeInfo);
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
    for (const auto & it : mMesonModes) {
        if (strcmp(it.second.name, mode.name) == 0) {
            return it.first;
        }
    }

    MESON_LOGE("getModeBlobId failed [%s]", mode.name);
    return 0;
}

int DrmConnector::getModeByBlobId(drm_mode_info_t & mode, uint32_t blobid) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mMesonModes.find(blobid);
    if (it != mMesonModes.end()) {
        memcpy(&mode, &(it->second), sizeof(drm_mode_info_t));
        return 0;
    }
    return -EINVAL;
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
    uint32_t modeBlobId = 0;
    for (const auto & it : mDrmModes) {
        if (memcmp(&drmmode, &it.second, sizeof(drmModeModeInfo)) == 0) {
            modeBlobId = it.first;
            break;
        }
    }

    if (modeBlobId == 0) {
        MESON_LOGE("DrmMode2Mode find drm mode failed (%s)", drmmode.name);
        return -EINVAL;
    }

    return getModeByBlobId(mode, modeBlobId );
}

void DrmConnector::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Connector (%s, %d x %d, %s, %s) mId(%d) mCrtcId(%d)\n",
        getName(), mPhyWidth, mPhyHeight,
        isSecure() ? "secure" : "unsecure", isConnected() ? "Connected" : "Removed",
        mId, getCrtcId());

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

void DrmConnector::getHdrCapabilities(drm_hdr_capabilities * caps) {
    if (mType != DRM_MODE_CONNECTOR_HDMIA)
        return;

    if (caps) {
        *caps = mHdrCapabilities;
    }
}

