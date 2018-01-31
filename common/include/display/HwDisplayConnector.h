/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CONNECTOR_H
#define HW_DISPLAY_CONNECTOR_H

#include <DrmTypes.h>
#include <BasicTypes.h>

class HwDisplayConnector {
public:
    HwDisplayConnector(int32_t drvFd, uint32_t id) { mDrvFd = drvFd; mId = id; }
    virtual ~HwDisplayConnector() { }

    virtual drm_connector_type_t getType() = 0;

    virtual uint32_t getModesCount() = 0;
    virtual int32_t getDisplayModes(drm_mode_info_t * modes) = 0;

    virtual bool isConnected() = 0;
    virtual bool isSecure() = 0;

    virtual void dump(String8 & dumpstr) = 0;

protected:
    int32_t mDrvFd;
    uint32_t mId;
};

#endif/*HW_DISPLAY_CONNECTOR_H*/
