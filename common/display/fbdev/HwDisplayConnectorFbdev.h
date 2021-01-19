/* Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CONNECTOR_FBDEV_H
#define HW_DISPLAY_CONNECTOR_FBDEV_H

#include <string>
#include <vector>

#include <utils/String8.h>
#include <utils/Errors.h>
#include <sys/types.h>
#include <DrmTypes.h>
#include <BasicTypes.h>
#include <HwDisplayConnector.h>


class HwDisplayConnectorFbdev : public HwDisplayConnector{
public:
    HwDisplayConnectorFbdev(int32_t drvFd, uint32_t id);
    virtual ~HwDisplayConnectorFbdev();

    virtual int32_t setCrtcId(uint32_t crtcid);
    virtual uint32_t getCrtcId();

    virtual int32_t update() = 0;

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);

    virtual const char * getName() = 0;
    virtual drm_connector_type_t getType() = 0;
    virtual bool isSecure() = 0;
    virtual bool isConnected() = 0;

    virtual void getHdrCapabilities(drm_hdr_capabilities * caps) = 0;
    virtual int32_t getIdentificationData(std::vector<uint8_t>& idOut);
    virtual void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut);

    virtual int32_t setAutoLowLatencyMode(bool on);
    virtual int32_t setContentType(uint32_t contentType);

    virtual void dump(String8 & dumpstr) = 0;

    virtual int32_t setMode(drm_mode_info_t & mode __unused) { return 0;};
    virtual uint32_t getId() { return mId;};

protected:
    virtual void loadPhysicalSize();
    virtual int32_t addDisplayMode(std::string& mode);

    int32_t mDrvFd;
    uint32_t mId;
    uint32_t mCrtcId;

    uint32_t mPhyWidth;
    uint32_t mPhyHeight;

    std::map<uint32_t, drm_mode_info_t> mDisplayModes;
    std::vector<uint32_t> mSupportedContentTypes;
};

#endif/*HW_DISPLAY_CONNECTOR_FBDEV_H*/
