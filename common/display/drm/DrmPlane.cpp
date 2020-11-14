/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <DebugHelper.h>

#include "DrmDevice.h"
#include "DrmPlane.h"
#include <drm_fourcc.h>

DrmPlane::DrmPlane(int drmFd, drmModePlanePtr p)
    : HwDisplayPlane(),
    mDrmFd(drmFd),
    mId(p->plane_id),
    mCrtcMask(p->possible_crtcs) {
    mFormatCnt = p->count_formats;

    if (mFormatCnt > 0) {
        mFormats = new uint32_t[mFormatCnt];
        memcpy(mFormats, p->formats, sizeof(uint32_t) * p->count_formats);
    } else {
        mFormats = NULL;
        mFormatCnt = 0;
        MESON_LOGE("No formats passed for plane [%d].\n", mId);
    }

    mModifierCnt = 0;
    mModifiers = NULL;

    loadProperties();
    resloveInFormats();

    mBlank = p->fb_id > 0 ? false : true;
    mDrmBo = std::make_shared<DrmBo>();
    mLastDrmBo = NULL;
}

DrmPlane::~DrmPlane() {
    if (mFormats)
        delete mFormats;
    mFormats = NULL;

    if (mModifiers)
        delete mModifiers;
    mModifiers = NULL;
}

void DrmPlane::loadProperties() {
    struct {
        const char * propname;
        std::shared_ptr<DrmProperty> * drmprop;
    } planeProps[] = {
        {DRM_PLANE_PROP_TYPE, &mType},
        {DRM_PLANE_PROP_FBID, &mFbId},
        {DRM_PLANE_PROP_INFENCE, &mInFence},
        {DRM_PLANE_PROP_SRCX, &mSrcX},
        {DRM_PLANE_PROP_SRCY, &mSrcY},
        {DRM_PLANE_PROP_SRCW, &mSrcW},
        {DRM_PLANE_PROP_SRCH, &mSrcH},
        {DRM_PLANE_PROP_CRTCID, &mCrtcId},
        {DRM_PLANE_PROP_CRTCX, &mCrtcX},
        {DRM_PLANE_PROP_CRTCY, &mCrtcY},
        {DRM_PLANE_PROP_CRTCW, &mCrtcW},
        {DRM_PLANE_PROP_CRTCH, &mCrtcH},
        {DRM_PLANE_PROP_Z, &mZpos},
        {DRM_PLANE_PROR_IN_FORMATS, &mInFormats},
        {DRM_PLANE_PROP_BLENDMODE, &mBlendMode},
        {DRM_PLANE_PROP_ALPHA, &mAlpha},
    };
    const int planePropsNum = sizeof(planeProps)/sizeof(planeProps[0]);
    int initedProps = 0;

    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(mDrmFd, mId, DRM_MODE_OBJECT_PLANE);
    MESON_ASSERT(props != NULL, "DrmPlane::loadProperties failed.");

    for (int i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(mDrmFd, props->props[i]);
        for (int j = 0; j < planePropsNum; j++) {
            if (strcmp(prop->name, planeProps[j].propname) == 0) {
                *(planeProps[j].drmprop) =
                    std::make_shared<DrmProperty>(prop, mId, props->prop_values[i]);
                initedProps ++;
                break;
            }
        }
       drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    if (initedProps != planePropsNum)
        MESON_LOGE("NOT ALL PROPS LOADED, %d-%d.",   initedProps, planePropsNum);
}

const char * DrmPlane::getName() {
    const char * name;
    switch (mType->getValue()) {
        case DRM_PLANE_TYPE_PRIMARY:
            name = "OSD-primary";
            break;
        case DRM_PLANE_TYPE_OVERLAY:
            name = "OSD-overlay";
            break;
        case DRM_PLANE_TYPE_CURSOR:
            name = "cursor";
            break;
        default:
            name = "unknown-drm-plane";
            break;
    };
    return name;
}

uint32_t DrmPlane::getId() {
    return mId;
}

uint32_t DrmPlane::getType() {
    switch (mType->getValue()) {
        case DRM_PLANE_TYPE_PRIMARY:
        case DRM_PLANE_TYPE_OVERLAY:
            return OSD_PLANE;
        case DRM_PLANE_TYPE_CURSOR:
            return CURSOR_PLANE;
        default:
            return INVALID_PLANE;
    };
}

uint32_t DrmPlane::getCapabilities() {
    uint32_t caps = 0;
    if (mType->getValue() == DRM_PLANE_TYPE_PRIMARY) {
        caps |= PLANE_SHOW_LOGO;
        caps |= PLANE_PRIMARY;
    }

    if (!mZpos->isImmutable()) {
        caps |= PLANE_SUPPORT_ZORDER;
    }

    return caps;
}

int32_t DrmPlane::getFixedZorder() {
    MESON_LOG_EMPTY_FUN();
    if (mZpos->isImmutable()) {
        return (int32_t)mZpos->getValue();
    }

    return 0;
}

uint32_t DrmPlane::getPossibleCrtcs() {
    return mCrtcMask;
}

int32_t DrmPlane::setCrtcId(uint32_t crtcid) {
    mCrtc = getDrmDevice()->getCrtcById(crtcid);
    return 0;
}

#define OSD_INPUT_MAX_WIDTH (1920)
#define OSD_INPUT_MAX_HEIGHT (1080)
#define OSD_INPUT_MIN_WIDTH (128)
#define OSD_INPUT_MIN_HEIGHT (128)
bool DrmPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->isRotated())
         return false;

    /*check scanout buffer*/
    switch (fb->mFbType) {
        case DRM_FB_CURSOR:
            if (!am_gralloc_is_coherent_buffer(fb->mBufferHandle))
                return false;
            break;
        case DRM_FB_SCANOUT:
            break;
        default:
            return false;
    }

    unsigned int blendMode = fb->mBlendMode;
    if (blendMode != DRM_BLEND_MODE_NONE
        && blendMode != DRM_BLEND_MODE_PREMULTIPLIED
        && blendMode != DRM_BLEND_MODE_COVERAGE) {
        MESON_LOGE("Blend mode is invalid!");
        return false;
    }

    /*check format*/
    int halFormat = am_gralloc_get_format(fb->mBufferHandle);
    int afbc = am_gralloc_get_vpu_afbc_mask(fb->mBufferHandle);
    int drmFormat = covertToDrmFormat(halFormat);
    uint64_t modifier = convertToDrmModifier(afbc);

    if (drmFormat == DRM_FORMAT_INVALID) {
        MESON_LOGE("Unknown drm format.\n");
        return false;
    }

    /*check vpu limit: blend mode*/
    if (blendMode == DRM_BLEND_MODE_NONE &&
        halFormat == HAL_PIXEL_FORMAT_BGRA_8888) {
        MESON_LOGE("blend mode: %u, Layer format %d not support.", blendMode, halFormat);
        return false;
    }

    if (!validateFormat(drmFormat, modifier))
        return false;

    /*check vpu limit: buffer size*/
    uint32_t sourceWidth = fb->mSourceCrop.bottom - fb->mSourceCrop.top;
    uint32_t sourceHeight = fb->mSourceCrop.right - fb->mSourceCrop.left;
    if (sourceWidth > OSD_INPUT_MAX_HEIGHT ||sourceHeight > OSD_INPUT_MAX_WIDTH)
        return false;
    if (sourceWidth < OSD_INPUT_MIN_HEIGHT ||sourceHeight < OSD_INPUT_MIN_WIDTH)
        return false;
    return true;
}

int32_t DrmPlane::setPlane(
    std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder,
    int blankOp) {
    bool bBlank = blankOp == UNBLANK ? false : true;
    DrmCrtc * crtc = (DrmCrtc *)mCrtc.get();
    drmModeAtomicReqPtr req = crtc->getAtomicReq();
    MESON_ASSERT(req != NULL, " plane get empty req.");

    if (bBlank) {
        if (!mBlank) {
            /*set fbid  =0&&crtc=0, driver will check it.*/
            mFbId->setValue(0);
            mFbId->apply(req);
            mCrtcId->setValue(0);
            mCrtcId->apply(req);
            mLastDrmBo = mDrmBo;
            mDrmBo.reset();
        }
    } else {
        bool bUpdate = false;

        mLastDrmBo = mDrmBo;
        mDrmBo = std::make_shared<DrmBo>();
        if (mDrmBo->import(fb) != 0) {
            MESON_LOGE("DrmBo import failed, return.");
            return 0;
        }

        if (mDrmBo->fbId != mFbId->getValue()) {
            mFbId->setValue(mDrmBo->fbId);
            bUpdate = true;
        }
        if (mCrtcId->getValue() != mCrtc->getId()) {
            mCrtcId->setValue(mCrtc->getId());
            bUpdate = true;
        }
        if (mSrcX->getValue() !=  mDrmBo->srcRect.left ||
            mSrcY->getValue() !=  mDrmBo->srcRect.top ||
            mSrcW->getValue() !=  (mDrmBo->srcRect.right - mDrmBo->srcRect.left)  ||
            mSrcH->getValue() !=  (mDrmBo->srcRect.bottom - mDrmBo->srcRect.top)) {
            mSrcX->setValue(mDrmBo->srcRect.left);
            mSrcY->setValue(mDrmBo->srcRect.top);
            mSrcW->setValue((mDrmBo->srcRect.right - mDrmBo->srcRect.left) <<16 );
            mSrcH->setValue((mDrmBo->srcRect.bottom - mDrmBo->srcRect.top) << 16);
            bUpdate = true;
        }
        if (mCrtcX->getValue() !=  mDrmBo->crtcRect.left ||
            mCrtcY->getValue() !=  mDrmBo->crtcRect.top ||
            mCrtcW->getValue() !=  (mDrmBo->crtcRect.right - mDrmBo->crtcRect.left)  ||
            mCrtcH->getValue() !=  (mDrmBo->crtcRect.bottom - mDrmBo->crtcRect.top)) {
            mCrtcX->setValue(mDrmBo->crtcRect.left);
            mCrtcY->setValue(mDrmBo->crtcRect.top);
            mCrtcW->setValue(mDrmBo->crtcRect.right - mDrmBo->crtcRect.left);
            mCrtcH->setValue(mDrmBo->crtcRect.bottom - mDrmBo->crtcRect.top);
            bUpdate = true;
        }
        if (mZpos->getValue() != zorder) {
            mZpos->setValue(zorder);
            bUpdate = true;
        }
        if (mBlendMode.get()) {
            uint64_t blendMode = 0;
            const char * blendModeStr = NULL;
            switch (mDrmBo->blend) {
                case DRM_BLEND_MODE_NONE:
                        blendModeStr = DRM_PLANE_PROP_BLENDMODE_NONE;
                        break;
                case DRM_BLEND_MODE_PREMULTIPLIED:
                        blendModeStr = DRM_PLANE_PROP_BLENDMODE_PREMULTI;
                        break;
                case DRM_BLEND_MODE_COVERAGE:
                        blendModeStr = DRM_PLANE_PROP_BLENDMODE_COVERAGE;
                        break;
                default:
                    MESON_LOGE("Unknown blend mode.");
                    break;
            };

            mBlendMode->getEnumValueWithName(blendModeStr, blendMode);
            mBlendMode->setValue(blendMode);
        } else {
           // MESON_LOGE("No pixel mode supported in driver.");
        }
        if (mAlpha.get()) {
            uint64_t minVal, maxVal;
            mAlpha->getRangeValue(minVal, maxVal);
            mAlpha->setValue(mDrmBo->alpha * (maxVal - minVal) + minVal);
        } else {
           // MESON_LOGE("No alpha supported in driver.");
        }

        if (bUpdate) {
            mFbId->apply(req);
            mCrtcId->apply(req);
            mSrcX->apply(req);
            mSrcY->apply(req);
            mSrcW->apply(req);
            mSrcH->apply(req);
            mCrtcX->apply(req);
            mCrtcY->apply(req);
            mCrtcW->apply(req);
            mCrtcH->apply(req);
            mZpos->apply(req);
            mInFence->setValue(mDrmBo->inFence);
            if (mBlendMode.get())
                mBlendMode->apply(req);
            if (mAlpha.get())
                mAlpha->apply(req);
        }
    }

    /*for drmplane it use the fence when atomic,
    *set it to -1, hwc will set it after atomic.
    */
    if (mFb)
        mFb->setPrevReleaseFence(-1);
    mFb = fb;
    mBlank = bBlank;
    return 0;
}

void DrmPlane::setDebugFlag(int dbgFlag) {
    UNUSED(dbgFlag);
    MESON_LOG_EMPTY_FUN();
}

void DrmPlane::resloveInFormats() {
    if (!mInFormats) {
        MESON_LOGI("No inFormats prop. ");
        return;
    }

    std::vector<uint8_t> blob;
    if (mInFormats->getBlobData(blob) != 0) {
        MESON_LOGE("Informats blob NULL.");
        return ;
    }

    struct drm_format_modifier_blob *header =
        (struct drm_format_modifier_blob *)blob.data();
    uint32_t *formats = (uint32_t *) ((char *) header + header->formats_offset);
    struct drm_format_modifier *modifiers =
        (struct drm_format_modifier *) ((char *) header + header->modifiers_offset);

    if (mFormats) {
        delete mFormats;
        mFormats = NULL;
    }
    mFormatCnt = header->count_formats;
    mFormats = new uint32_t[mFormatCnt];
    memcpy(mFormats, formats, sizeof(uint32_t) * mFormatCnt);

    mModifierCnt = header->count_modifiers;
    mModifiers = new struct drm_format_modifier[mModifierCnt];
    memcpy(mModifiers, modifiers, sizeof(struct drm_format_modifier) * mModifierCnt);
}

bool DrmPlane::validateFormat(uint32_t format, uint64_t modifier) {
    int formatIdx = -1;
    for (int i = 0; i < mFormatCnt; i ++) {
        if (format == mFormats[i]) {
            formatIdx = i;
            break;
        }
    }

    if (formatIdx == -1) {
     //   MESON_LOGD("Not Supported Format %x", format);
        return false;
    }

    if (modifier == 0)
        return true;

    uint64_t formatMask = 1ULL << formatIdx;
    for (int i = 0; i < mModifierCnt; i++) {
        if (mModifiers[i].modifier == modifier) {
            if (mModifiers[i].formats & formatMask) {
                return true;
            } else {
             //   MESON_LOGD("Not supported modifier-format (%lld-%d)", modifier, format);
                return false;
            }
        }
    }

    return false;
}

void DrmPlane::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Plane[%s-%d]\n", getName(), mId);
    dumpstr.appendFormat("\t Formats [%d]:", mFormatCnt);
    for (int i = 0;i < mFormatCnt; i++) {
        dumpstr.appendFormat("(%x),", mFormats[i]);
    }
    dumpstr.append("\n");

    dumpstr.appendFormat("\t Modifer [%d]:", mModifierCnt);
    for (int i = 0;i < mModifierCnt; i++) {
        dumpstr.appendFormat("(%llx-%llx),", mModifiers[i].formats, mModifiers[i].modifier);
    }
    dumpstr.append("\n");
}
