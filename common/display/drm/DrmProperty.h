/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRM_PROPERTY_H
#define DRM_PROPERTY_H

#include <stdint.h>
#include <string>
#include <xf86drmMode.h>
#include <drm/drm_mode.h>
#include "DrmProperties.h"

class DrmProperty {
public:
    DrmProperty(drmModePropertyPtr p, uint64_t value);
    ~DrmProperty();

    int32_t setValue(uint64_t val);
    uint64_t getValue();

    /*get prop data*/
    int32_t getBlobData(std::vector<uint8_t> & blob);

    bool isImmutable() {return mPropRes.flags & DRM_MODE_PROP_IMMUTABLE;}

    void dump(String8 &dumpstr);

protected:
    int32_t getBlobData(std::vector<uint8_t> & blob, uint32_t blobId);


protected:
    uint64_t mValue;
    uint32_t mType;

    drmModePropertyRes mPropRes;
};

#endif
