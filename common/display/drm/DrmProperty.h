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
    DrmProperty(drmModePropertyPtr p, uint32_t objectId, uint64_t value);
    ~DrmProperty();

    uint32_t getId() { return mPropRes.prop_id; }

    int setValue(uint64_t val);
    uint64_t getValue();

    /*get prop data*/
    int getBlobData(std::vector<uint8_t> & blob);
    int getEnumValueWithName(const char *name, uint64_t & val);
    int getRangeValue(uint64_t & min, uint64_t & max);

    bool isImmutable() {return mPropRes.flags & DRM_MODE_PROP_IMMUTABLE;}

    int apply(drmModeAtomicReqPtr req);

    void dump(String8 &dumpstr);

protected:
    int32_t getBlobData(std::vector<uint8_t> & blob, uint32_t blobId);

protected:
    uint64_t mValue;
    uint32_t mType;
    uint32_t mComponetId;

    drmModePropertyRes mPropRes;
};

#endif
