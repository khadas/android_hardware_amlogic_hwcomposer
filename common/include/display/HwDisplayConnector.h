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

#include <DrmTypes.h>
#include <BasicTypes.h>

#define DEFAULT_DISPLAY_DPI 160

class HwDisplayConnector {
public:
    HwDisplayConnector(int32_t drvFd, uint32_t id);
    virtual ~HwDisplayConnector();

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes);

    virtual drm_connector_type_t getType() = 0;
    virtual bool isRemovable() = 0;
    virtual bool isSecure() = 0;
    virtual bool isConnected() = 0;

    virtual void getHdrCapabilities(drm_hdr_capabilities * caps) = 0;
    virtual  void dump(String8 & dumpstr) = 0;

protected:
    virtual void loadPhysicalSize();
    int32_t mDrvFd;
    uint32_t mId;

    int mPhyWidth;
    int mPhyHeight;

    std::map<uint32_t, drm_mode_info_t> mDisplayModes;
};

#endif/*HW_DISPLAY_CONNECTOR_H*/
