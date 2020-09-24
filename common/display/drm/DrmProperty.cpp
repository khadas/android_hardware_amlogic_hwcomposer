/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <string.h>
#include <errno.h>

#include <MesonLog.h>
#include "DrmDevice.h"
#include "DrmProperty.h"

DrmProperty::DrmProperty(drmModePropertyPtr p, uint64_t value)
    : mValue(value) {
    memcpy(&mPropRes, p, sizeof(mPropRes));
    if (p->count_values > 0) {
        size_t contentLen = p->count_values * sizeof(uint64_t);
        mPropRes.values = (uint64_t*)malloc(contentLen);
        memcpy(&mPropRes.values, p->values, contentLen);
    } else {
        mPropRes.values = NULL;
    }
    if (p->count_enums > 0) {
        size_t contentLen = p->count_enums * sizeof(struct drm_mode_property_enum);
        mPropRes.enums = (struct drm_mode_property_enum *)malloc(contentLen);
        memcpy(&mPropRes.enums, p->enums, contentLen);
    } else {
        mPropRes.enums = NULL;
    }
    if (p->count_blobs > 0) {
        size_t contentLen = p->count_blobs * sizeof(uint32_t);
        mPropRes.blob_ids = (uint32_t*)malloc(contentLen);
        memcpy(&mPropRes.blob_ids, p->blob_ids, contentLen);
    } else {
        mPropRes.blob_ids = NULL;
    }

    mType = 0;
    if (drm_property_type_is(p, DRM_MODE_PROP_RANGE)) {
        mType = DRM_MODE_PROP_RANGE;
        MESON_ASSERT(p->count_values > 0, " NO RANGE SPECIFIED");
    } else if (drm_property_type_is(p, DRM_MODE_PROP_ENUM)) {
        mType = DRM_MODE_PROP_ENUM;
        MESON_ASSERT(p->count_enums > 0, " NO ENUM SPECIFIED");
    } else if (drm_property_type_is(p, DRM_MODE_PROP_OBJECT)) {
        mType = DRM_MODE_PROP_OBJECT;
    } else if (drm_property_type_is(p, DRM_MODE_PROP_BLOB)) {
        mType = DRM_MODE_PROP_BLOB;
        MESON_ASSERT(p->count_blobs > 0, " NO BLOB SPECIFIED");
    }
    MESON_ASSERT(mType !=0, "UNKNOWN type for prop (%s)", mPropRes.name);

    MESON_LOGD("DrmProperty: %s (%d), value %lld",
        mPropRes.name, mPropRes.prop_id, mValue);
}

DrmProperty::~DrmProperty() {
    if (mPropRes.values)
        free(mPropRes.values);
    if (mPropRes.enums)
        free(mPropRes.enums);
    if (mPropRes.blob_ids)
        free(mPropRes.blob_ids);
}

int32_t DrmProperty::setValue(uint64_t val) {
    if (isImmutable())
        return -EIO;

    if (mType == DRM_MODE_PROP_RANGE) {
        if (val < mPropRes.values[0] || val > mPropRes.values[1]) {
            MESON_LOGE("[%s] RAGNE ERROR (%lld) -> (%lld,%lld)",
                mPropRes.name, val, mPropRes.values[0], mPropRes.values[1]);
            return -EINVAL;
        }
    } else if (mType == DRM_MODE_PROP_ENUM) {
        if (val >= mPropRes.count_enums) {
            MESON_LOGE("[%s] ENUM IDX ERROR (%lld) -> (%d)",
                mPropRes.name, val, mPropRes.count_enums);
            return -EINVAL;
        }
    }

    mValue = val;
    return 0;
}

uint64_t DrmProperty::getValue() {
    switch (mType) {
        case DRM_MODE_PROP_OBJECT:
        case DRM_MODE_PROP_RANGE:
        case DRM_MODE_PROP_BLOB:
            return mValue;

        case DRM_MODE_PROP_ENUM:
            return mPropRes.enums[mValue].value;

        default:
            MESON_LOGE("unknown type");
            return 0;
    }
}

int32_t DrmProperty::getBlobData(std::vector<uint8_t> & blob) {
    if (mType != DRM_MODE_PROP_BLOB)
        return -EINVAL;

    return getBlobData(blob, mValue);
}

int32_t DrmProperty::getBlobData(std::vector<uint8_t> & blob, uint32_t blobId) {
    drmModePropertyBlobPtr blobProp =
        drmModeGetPropertyBlob(getDrmDevice()->getDeviceFd(), blobId);
    if (!blobProp) {
    	return -EIO;
    }

    uint8_t * blobData = (uint8_t *)blobProp->data;

    blob.clear();
    blob.resize(blobProp->length);
    for (int i = 0; i < blobProp->length; i++) {
        blob.push_back(blobData[i]);
    }

    drmModeFreePropertyBlob(blobProp);

    return 0;
}

void DrmProperty::dump(String8 &dumpstr) {
    dumpstr.appendFormat("DrmProperty: [%s]-[%d], value [%llu], immutable[%d]:",
        mPropRes.name, mPropRes.prop_id, mValue, isImmutable());

    if (mType == DRM_MODE_PROP_RANGE) {
    	dumpstr.append("\t\tvalues:");
    	for (int i = 0; i < mPropRes.count_values; i++)
    		dumpstr.appendFormat(" %llu ", mPropRes.values[i]);
    	dumpstr.append("\n");
    } else if (mType == DRM_MODE_PROP_ENUM) {
    	dumpstr.append("\t\tenums:");
    	for (int i = 0; i < mPropRes.count_enums; i++)
    		dumpstr.appendFormat(" %s=%llu",mPropRes.enums[i].name,
    		       mPropRes.enums[i].value);
    	dumpstr.append("\n");
    } else if (mType == DRM_MODE_PROP_BLOB) {
    	dumpstr.append("\t\tblobs:");
    	for (int i = 0; i < mPropRes.count_blobs; i++) {
            std::vector<uint8_t> blob;
            if (0 == getBlobData(blob, mPropRes.blob_ids[i])) {
                for ( uint8_t val : blob) {
                        dumpstr.appendFormat("%.2hhx", val);
                }
                dumpstr.append("\n");
            }
        }
	dumpstr.append("\n");
    }
}


