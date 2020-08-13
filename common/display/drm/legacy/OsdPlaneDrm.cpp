/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "OsdPlaneDrm.h"
#include <MesonLog.h>
#include <DebugHelper.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define OSD_PATTERN_SIZE (128)

OsdPlaneDrm::OsdPlaneDrm(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id),
      mBlank(false),
      mPossibleCrtcs(0),
      mDrmFb(NULL) {
    snprintf(mName, 64, "drm-osd-%d", id);
    mPlaneInfo.out_fen_fd = -1;
    getProperties();
}

OsdPlaneDrm::~OsdPlaneDrm() {
}

int32_t OsdPlaneDrm::getProperties() {
    int i;
    /*mode resources*/
    mRes_mode = drmModeGetResources(mDrvFd);
    /*plane resources*/
    mRes_plane = drmModeGetPlaneResources(mDrvFd);
    mPtr_plane = drmModeGetPlane(mDrvFd, mRes_plane->planes[mId]);
    /*plane properties*/
    mProperties_plane = drmModeObjectGetProperties(mDrvFd,
        mRes_plane->planes[mId], DRM_MODE_OBJECT_PLANE);
    mProperty_plane = (drmModePropertyRes **)calloc(mProperties_plane->count_props,
                        sizeof(*mProperty_plane));
    mCapability = 0;
    for (i = 0; i < mProperties_plane->count_props; i++) {
        mProperty_plane[i] = drmModeGetProperty(mDrvFd, mProperties_plane->props[i]);
        if (!strcmp(mProperty_plane[i]->name, "type") &&
            mProperties_plane->prop_values[i] == DRM_PLANE_TYPE_PRIMARY)
            mCapability |= PLANE_PRIMARY;
    }
    mCapability |= PLANE_SUPPORT_ZORDER;
    mCapability |= PLANE_SUPPORT_FREE_SCALE;
    mPossibleCrtcs |= CRTC_VOUT1;

    /*crtc properties*/
    mPtr_crtc = drmModeGetCrtc(mDrvFd, mRes_mode->crtcs[0]);
    mPtr_encoder = drmModeGetEncoder(mDrvFd, mRes_mode->encoders[0]);
    mPtr_connector = drmModeGetConnector(mDrvFd, mRes_mode->connectors[0]);
    //mPtr_fb = drmModeGetFB(mDrvFd, mRes_mode->fbs[0]);
    /*crtc properties*/
    mProperties_crtc = drmModeObjectGetProperties(mDrvFd,
        mRes_mode->crtcs[0], DRM_MODE_OBJECT_CRTC);
    mProperty_crtc = (drmModePropertyRes **)calloc(mProperties_crtc->count_props,
                        sizeof(*mProperty_crtc));
    for (i = 0; i < mProperties_crtc->count_props; i++) {
        mProperty_crtc[i] = drmModeGetProperty(mDrvFd, mProperties_crtc->props[i]);
    }
    /*connector properties*/
    mProperties_connector = drmModeObjectGetProperties(mDrvFd,
        mRes_mode->connectors[0], DRM_MODE_OBJECT_CONNECTOR);
    mProperty_connector = (drmModePropertyRes **)calloc(mProperties_connector->count_props,
                        sizeof(*mProperty_connector));
    for (i = 0; i < mProperties_connector->count_props; i++) {
        mProperty_connector[i] = drmModeGetProperty(mDrvFd, mProperties_connector->props[i]);
    }

    return 0;
}

const char * OsdPlaneDrm::getName() {
    return mName;
}

uint32_t OsdPlaneDrm::getPlaneType() {
    if (mDebugIdle) {
        return INVALID_PLANE;
    }

    return OSD_PLANE;
}

uint32_t OsdPlaneDrm::getCapabilities() {
    return mCapability;
}

int32_t OsdPlaneDrm::getFixedZorder() {
    if (mCapability & PLANE_SUPPORT_ZORDER) {
        return INVALID_ZORDER;
    }

    return OSD_PLANE_FIXED_ZORDER;
}

uint32_t OsdPlaneDrm::getPossibleCrtcs() {
    return mPossibleCrtcs;
}

bool OsdPlaneDrm::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->isRotated())
         return false;

    //if cursor fb, check if buffer is cont
    switch (fb->mFbType) {
        case DRM_FB_CURSOR:
            if (!am_gralloc_is_coherent_buffer(fb->mBufferHandle))
                return false;
            break;
        case DRM_FB_SCANOUT:
            break;
        //case DRM_FB_COLOR:
            //return true;
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

    int format = am_gralloc_get_format(fb->mBufferHandle);
    int afbc = am_gralloc_get_vpu_afbc_mask(fb->mBufferHandle);

    if (blendMode == DRM_BLEND_MODE_NONE && format == HAL_PIXEL_FORMAT_BGRA_8888) {
        MESON_LOGE("blend mode: %u, Layer format %d not support.", blendMode, format);
        return false;
    }
    if (afbc == 0) {
        switch (format) {
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
            case HAL_PIXEL_FORMAT_RGB_888:
            case HAL_PIXEL_FORMAT_RGB_565:
            case HAL_PIXEL_FORMAT_BGRA_8888:
                break;
            default:
                MESON_LOGE("afbc: %d, Layer format %d not support.", afbc, format);
                return false;
        }
    } else {
        if ((mCapability & PLANE_SUPPORT_AFBC) == PLANE_SUPPORT_AFBC) {
            switch (format) {
                case HAL_PIXEL_FORMAT_RGBA_8888:
                case HAL_PIXEL_FORMAT_RGBX_8888:
                    break;
                default:
                    MESON_LOGE("afbc: %d, Layer format %d not support.", afbc, format);
                    return false;
            }
        } else {
            MESON_LOGI("AFBC buffer && unsupported AFBC plane, turn to GPU composition");
            return false;
        }
    }

    uint32_t sourceWidth = fb->mSourceCrop.bottom - fb->mSourceCrop.top;
    uint32_t sourceHeight = fb->mSourceCrop.right - fb->mSourceCrop.left;
    if (sourceWidth > OSD_INPUT_MAX_HEIGHT ||sourceHeight > OSD_INPUT_MAX_WIDTH)
        return false;
    if (sourceWidth < OSD_INPUT_MIN_HEIGHT ||sourceHeight < OSD_INPUT_MIN_WIDTH)
        return false;
    return true;
}

uint32_t OsdPlaneDrm::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_BGR565;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    default:
      MESON_LOGI("Cannot convert hal format to drm format %u", hal_format);
      return -EINVAL;
  }
}

int32_t OsdPlaneDrm::setPlane(std::shared_ptr<DrmFramebuffer> fb, uint32_t zorder, int blankOp) {
    MESON_ASSERT(mDrvFd >= 0, "osd plane fd is not valiable!");
    MESON_ASSERT(zorder > 0, "osd driver request zorder > 0");// driver request zorder > 0
    drmModeAtomicReqPtr req;

    req = drmModeAtomicAlloc();
    memset(&mPlaneInfo, 0, sizeof(mPlaneInfo));
    mPlaneInfo.magic         = OSD_SYNC_REQUEST_RENDER_MAGIC_V2;
    mPlaneInfo.len           = sizeof(osd_plane_info_t);
    mPlaneInfo.type          = DIRECT_COMPOSE_MODE;
    mPlaneInfo.zorder        = zorder;
    mPlaneInfo.shared_fd     = -1;
    mPlaneInfo.in_fen_fd     = -1;
    mPlaneInfo.out_fen_fd    = -1;

    bool bBlank = blankOp == UNBLANK ? false : true;
    if (!bBlank) {
        if (!fb) {
            MESON_LOGE("For osd plane unblank, the fb should not be null!");
            return 0;
        }

        std::shared_ptr<DrmFramebuffer> postFb = fb;
        if (mDebugPattern) {
            if (!mPatternFb.get())
                createPatternFb();
            if (mPatternFb.get())
                postFb = mPatternFb;
        }

        drm_rect_t srcCrop       = postFb->mSourceCrop;
        buffer_handle_t buf      = postFb->mBufferHandle;
        drm_rect_t disFrame      = fb->mDisplayFrame;

        mPlaneInfo.xoffset       = srcCrop.left;
        mPlaneInfo.yoffset       = srcCrop.top;
        mPlaneInfo.width         = srcCrop.right    - srcCrop.left;
        mPlaneInfo.height        = srcCrop.bottom   - srcCrop.top;
        mPlaneInfo.dst_x         = disFrame.left;
        mPlaneInfo.dst_y         = disFrame.top;
        mPlaneInfo.dst_w         = disFrame.right   - disFrame.left;
        mPlaneInfo.dst_h         = disFrame.bottom  - disFrame.top;
        mPlaneInfo.blend_mode    = fb->mBlendMode;
        mPlaneInfo.op           |= OSD_BLANK_OP_BIT;

        if (fb->mBufferHandle != NULL) {
            mPlaneInfo.fb_width  = am_gralloc_get_width(buf);
            mPlaneInfo.fb_height = am_gralloc_get_height(buf);
        } else {
            mPlaneInfo.fb_width  = -1;
            mPlaneInfo.fb_height = -1;
        }

        if (fb->mFbType == DRM_FB_COLOR) {
            /*reset buffer layer info*/
            mPlaneInfo.shared_fd = -1;

            mPlaneInfo.dim_layer = 1;
              /*osd canot support plane alpha when ouput dim layer.
            *so we handle the plane on color here.
            */
            mPlaneInfo.dim_color = (((unsigned char)(fb->mColor.r * fb->mPlaneAlpha) << 24) |
                                                    ((unsigned char)(fb->mColor.g * fb->mPlaneAlpha) << 16) |
                                                    ((unsigned char)(fb->mColor.b * fb->mPlaneAlpha) << 8) |
                                                    ((unsigned char)(fb->mColor.a * fb->mPlaneAlpha)));
            mPlaneInfo.plane_alpha = 255;
            mPlaneInfo.afbc_inter_format = 0;
        } else  {
            //reset dim layer info.
            mPlaneInfo.dim_layer = 0;
            mPlaneInfo.dim_color = 0;

            mPlaneInfo.shared_fd     = ::dup(am_gralloc_get_buffer_fd(buf));
            mPlaneInfo.format        = am_gralloc_get_format(buf);
            mPlaneInfo.byte_stride   = am_gralloc_get_stride_in_byte(buf);
            mPlaneInfo.pixel_stride  = am_gralloc_get_stride_in_pixel(buf);
            mPlaneInfo.afbc_inter_format = am_gralloc_get_vpu_afbc_mask(buf);
            mPlaneInfo.plane_alpha   = (unsigned char)255 * fb->mPlaneAlpha; //kenrel need alpha 0 ~ 255

            /*
              OSD only handle premultiplied and coverage,
              So HWC set format to RGBX when blend mode is NONE.
            */
            if (mPlaneInfo.blend_mode == DRM_BLEND_MODE_NONE
                && mPlaneInfo.format == HAL_PIXEL_FORMAT_RGBA_8888) {
                mPlaneInfo.format = HAL_PIXEL_FORMAT_RGBX_8888;
            }
        }

        if (DebugHelper::getInstance().discardInFence()) {
            fb->getAcquireFence()->waitForever("osd-input");
            mPlaneInfo.in_fen_fd = -1;
        } else {
            mPlaneInfo.in_fen_fd     = fb->getAcquireFence()->dup();
        }

    } else {
        /*For nothing to display, post blank to osd which will signal the last retire fence.*/

        //Already set blank, return.
        if (mBlank == bBlank)
            return 0;

        mPlaneInfo.op &= ~(OSD_BLANK_OP_BIT);
    }
    mBlank = bBlank;
    /*drm plane property*/
    uint32_t gem_handle[4], pitches[4], offsets[4];
    uint64_t modifier[4];
    int i;
    int ret = drmPrimeFDToHandle(mDrvFd, mPlaneInfo.shared_fd, &gem_handle[0]);
    if (ret) {
        MESON_LOGE("failed to import prime fd %d ret=%d", mPlaneInfo.shared_fd, ret);
        return ret;
    }

    int32_t drm_format = ConvertHalFormatToDrm(mPlaneInfo.format);
    if (drm_format < 0)
        return -1;
    pitches[0] = mPlaneInfo.byte_stride;
    offsets[0] = 0;
    modifier[0] = 0;
    /*temp work around used to blank/disable osd plane*/
    if (mBlank)
        mPlaneInfo.dst_x = mPlaneInfo.dst_w + 1;

    uint32_t old_fb_id;
    if (mPtr_plane->fb_id)
        old_fb_id = mPtr_plane->fb_id;
    else
        old_fb_id = 0;
    ret = drmModeAddFB2WithModifiers(mDrvFd, mPlaneInfo.fb_width, mPlaneInfo.fb_height,
                                    drm_format, gem_handle, pitches,
                                    offsets, modifier, &mPtr_plane->fb_id,
                                    modifier[0] ? DRM_MODE_FB_MODIFIERS : 0);
    for (i = 0; i < mProperties_plane->count_props; i++) {
        if (!strcmp(mProperty_plane[i]->name, "FB_ID"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPtr_plane->fb_id);
        else if (!strcmp(mProperty_plane[i]->name, "CRTC_ID"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPtr_plane->crtc_id);
        else if (!strcmp(mProperty_plane[i]->name, "SRC_X"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.xoffset);
        else if (!strcmp(mProperty_plane[i]->name, "SRC_Y"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.yoffset);
        else if (!strcmp(mProperty_plane[i]->name, "SRC_W"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.width << 16);
        else if (!strcmp(mProperty_plane[i]->name, "SRC_H"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.height << 16);
        else if (!strcmp(mProperty_plane[i]->name, "CRTC_X"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.dst_x);
        else if (!strcmp(mProperty_plane[i]->name, "CRTC_Y"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.dst_y);
        else if (!strcmp(mProperty_plane[i]->name, "CRTC_W"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.dst_w);
        else if (!strcmp(mProperty_plane[i]->name, "CRTC_H"))
            drmModeAtomicAddProperty(req, mRes_plane->planes[mId],
                mProperty_plane[i]->prop_id, mPlaneInfo.dst_h);
        else
            MESON_LOGD("skip plane property[%s] config", mProperty_plane[i]->name);
    }
    /*connector property*/
    for (i = 0; i < mProperties_connector->count_props; i++) {
        if (!strcmp(mProperty_connector[i]->name, "CRTC_ID"))
            drmModeAtomicAddProperty(req, mPtr_connector->connector_id,
                mProperty_connector[i]->prop_id, mPtr_crtc->crtc_id);
        else
            MESON_LOGD("skip connector property[%s] config", mProperty_connector[i]->name);
    }
    /*find the 1080p60hz as the default display mode*/
    if (0) {
    drmModeModeInfo *mode = NULL;
    for (i = 0; i < mPtr_connector->count_modes; i++) {
        mode = &mPtr_connector->modes[i];
        if (mode->hdisplay == 1920 && mode->vdisplay == 1080 && mode->vrefresh == 60) {
            MESON_LOGD("find the match mode property[%s]", mode->name);
            break;
        }
    }
    /*crtc property*/
    for (i = 0; i < mProperties_crtc->count_props; i++) {
        uint32_t blob_id;
        if (!strcmp(mProperty_crtc[i]->name, "MODE_ID")) {
            drmModeCreatePropertyBlob(mDrvFd, mode, sizeof(*mode), &blob_id);
            drmModeAtomicAddProperty(req, mPtr_crtc->crtc_id,
                mProperty_crtc[i]->prop_id, blob_id);
        } else if (!strcmp(mProperty_crtc[i]->name, "ACTIVE")) {
            drmModeAtomicAddProperty(req, mPtr_crtc->crtc_id,
                mProperty_crtc[i]->prop_id, 1);
        } else
            MESON_LOGD("skip crtc property[%s] config", mProperty_crtc[i]->name);
    }
    }
    ret = drmModeAtomicCommit(mDrvFd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    if (ret) {
        MESON_LOGE("osd plane atomic commit failed");
        MESON_LOGD("drmModeAtomicCommit failed");
        return -EINVAL;
    }
    drmModeAtomicFree(req);
    if (old_fb_id)
        ret = drmModeRmFB(mDrvFd, old_fb_id);
    if (ret)
        MESON_LOGE("drmModeRmFB old_fb_id %d fail", old_fb_id);

    if (mDrmFb.get()) {
    /* dup a out fence fd for layer's release fence, we can't close this fd
    * now, cause display retire fence will also use this fd. will be closed
    * on SF side*/
        if (DebugHelper::getInstance().discardOutFence()) {
            mDrmFb->setPrevReleaseFence(-1);
        } else {
            mDrmFb->setPrevReleaseFence((mPlaneInfo.out_fen_fd >= 0) ? ::dup(mPlaneInfo.out_fen_fd) : -1);
        }
    }

    // update drm fb.
    if (bBlank)
        mDrmFb.reset();
    else
        mDrmFb = fb;
    return 0;
}

void OsdPlaneDrm::createPatternFb() {
    buffer_handle_t hnd = gralloc_alloc_dma_buf(
        OSD_PATTERN_SIZE, OSD_PATTERN_SIZE,
        HAL_PIXEL_FORMAT_RGBA_8888,
        true, false);
    mPatternFb = std::make_shared<DrmFramebuffer>(hnd, -1);

    void * fbmem = NULL;
    if (mPatternFb->lock(&fbmem) == 0) {
        native_handle_t * handle = mPatternFb->mBufferHandle;
        int w = am_gralloc_get_width(handle);
        int h = am_gralloc_get_height(handle);
        int type = mId % 3;
        char r = 0, g = 0, b = 0;
        switch (type) {
            case 0:
                 r = g = b = 255;
                 break;
            case 1:
                r = 255;
                break;
            case 2:
                g = 255;
                break;
             case 3:
                b = 255;
                break;
            default:
                r = g = b = 128;
                break;
        };
        MESON_LOGD("Plane setpattern (%d-%d,%d,%d)", mId, r, g, b);

        char * colorbuf = (char *) fbmem;
        for (int ir = 0; ir < h; ir++) {
            for (int ic = 0; ic < w; ic++) {
                colorbuf[0] = r;
                colorbuf[1] = g;
                colorbuf[2] = b;
                colorbuf[3] = 0xff;
                colorbuf += 4;
            }
        }
        mPatternFb->unlock();
    }
}

void OsdPlaneDrm::dump(String8 & dumpstr) {
    if (!mBlank) {
        dumpstr.appendFormat("| osd%2d |"
                " %4d | %4d | %4d %4d %4d %4d | %4d %4d %4d %4d | %2d | %2d | %4d |"
                " %4d | %5d | %5d | %4x |%8x  |\n",
                 mId,
                 mPlaneInfo.zorder,
                 mPlaneInfo.type,
                 mPlaneInfo.xoffset, mPlaneInfo.yoffset, mPlaneInfo.width, mPlaneInfo.height,
                 mPlaneInfo.dst_x, mPlaneInfo.dst_y, mPlaneInfo.dst_w, mPlaneInfo.dst_h,
                 mPlaneInfo.shared_fd,
                 mPlaneInfo.format,
                 mPlaneInfo.byte_stride,
                 mPlaneInfo.pixel_stride,
                 mPlaneInfo.blend_mode,
                 mPlaneInfo.plane_alpha,
                 mPlaneInfo.op,
                 mPlaneInfo.afbc_inter_format);
    }
}

