/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include "HwcVideoPlane.h"
#include "AmFramebuffer.h"
#include <DebugHelper.h>
#include <OmxUtil.h>
#include <misc.h>


HwcVideoPlane::HwcVideoPlane(int32_t drvFd, uint32_t id)
    : HwDisplayPlane(drvFd, id) {
    snprintf(mName, 64, "HwcVideo-%d", id);
}

HwcVideoPlane::~HwcVideoPlane() {
}

const char * HwcVideoPlane::getName() {
    return mName;
}

uint32_t HwcVideoPlane::getPlaneType() {
    return HWC_VIDEO_PLANE;
}

uint32_t HwcVideoPlane::getCapabilities() {
    /*HWCVideoplane always support zorder.*/
    return PLANE_SUPPORT_ZORDER;
}

int32_t HwcVideoPlane::getFixedZorder() {
    return INVALID_ZORDER;
}

uint32_t HwcVideoPlane::getPossibleCrtcs() {
    return CRTC_VOUT1;
}

bool HwcVideoPlane::isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->mFbType == DRM_FB_VIDEO_OMX_V4L)
        return true;

    return false;
}

int32_t HwcVideoPlane::setComposePlane(
    DiComposerPair *difbs, int blankOp) {
    u32 i;
	video_frame_info_t *vFrameInfo;
    int video_composer_enable;
	std::shared_ptr<DrmFramebuffer> fb;
    native_handle_t * buf;
    char *base = NULL;

	if (mDrvFd < 0) {
        MESON_LOGE("hwcvideo plane fd is not valiable!");
        return -EBADF;
    }

	bool bBlank = blankOp == UNBLANK ? false : true;

	if (!bBlank) {

		memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));
		mFramesCount = difbs->composefbs.size();
		for (i = 0; i < mFramesCount; i++) {
			vFrameInfo = &mVideoFramesInfo.frame_info[i];
			fb = difbs->composefbs[i];

			buffer_handle_t buf = fb->mBufferHandle;
			drm_rect_t dispFrame = fb->mDisplayFrame;
			drm_rect_t srcCrop = fb->mSourceCrop;
			if (fb->mFbType == DRM_FB_VIDEO_OMX_V4L) {
				vFrameInfo->fd = am_gralloc_get_omx_v4l_file(buf);
				vFrameInfo->type = 0;
			} else if (fb->mFbType == DRM_FB_VIDEO_DMABUF) {
				vFrameInfo->fd = am_gralloc_get_video_dma_buf_fd(buf);
				vFrameInfo->type = 1;
			} else if (fb->mFbType == DRM_FB_VIDEO_SIDEBAND ||
				fb->mFbType == DRM_FB_VIDEO_SIDEBAND_SECOND) {
				vFrameInfo->type = 2;
			}

			vFrameInfo->dst_x = dispFrame.left;
			vFrameInfo->dst_y = dispFrame.top;
			vFrameInfo->dst_w = dispFrame.right - dispFrame.left;
			vFrameInfo->dst_h = dispFrame.bottom - dispFrame.top;

			vFrameInfo->crop_x = srcCrop.left;
			vFrameInfo->crop_y = srcCrop.top;
			vFrameInfo->crop_w = srcCrop.right - srcCrop.left;
			vFrameInfo->crop_h = srcCrop.bottom - srcCrop.top;
			vFrameInfo->buffer_w = am_gralloc_get_width(buf);
			vFrameInfo->buffer_h = am_gralloc_get_height(buf);
			vFrameInfo->zorder = fb->mZorder;
			vFrameInfo->transform = fb->mTransform;
		}
		mVideoFramesInfo.frame_count = mFramesCount;
		mVideoFramesInfo.layer_index = mId;
		mVideoFramesInfo.disp_zorder = difbs->zorder;

		if (!mStatus) {
            video_composer_enable = 1;
			MESON_LOGE("di composer device %d set enable.\n", mDrvFd);
			if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_ENABLE, &video_composer_enable) != 0) {
				MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
					strerror(errno), errno, mDrvFd);
				return -1;
			}
			mStatus = 1;
		}

		MESON_LOGE("di composer device %d set frame.\n", mDrvFd);
		if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_FRAMES, &mVideoFramesInfo) != 0) {
			MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
				strerror(errno), errno, mDrvFd);
			return -1;
		}

		for (i = 0; i < mFramesCount; i++) {
			vFrameInfo = &mVideoFramesInfo.frame_info[i];
			fb = difbs->composefbs[i];

			if (fb) {
                buf = fb->mBufferHandle;
                if (0 == gralloc_lock_dma_buf(buf, (void **)&base)) {
                    set_v4lvideo_sync_info(base);
                    gralloc_unlock_dma_buf(buf);
                } else {
                    MESON_LOGE("set_v4lvideo_sync_info failed.");
                }
			/* dup a out fence fd for layer's release fence, we can't close this fd
			* now, cause display retire fence will also use this fd. will be closed
			* on SF side*/
				if (DebugHelper::getInstance().discardOutFence()) {
					MESON_LOGE("di composer set release fence: -1.\n");
					fb->setReleaseFence(-1);
				} else {
					MESON_LOGE("di composer set release fence: %u.\n", vFrameInfo->composer_fen_fd);
					if (i > 0)
						fb->setReleaseFence((vFrameInfo->composer_fen_fd >= 0) ? dup(vFrameInfo->composer_fen_fd) : -1);
					else if (i == 0)
						fb->setReleaseFence((vFrameInfo->composer_fen_fd >= 0) ? vFrameInfo->composer_fen_fd : -1);
				}
			}
		}
	}else {
		memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));

		if (mStatus) {
            video_composer_enable = 0;
			MESON_LOGE("di composer device %d set disable.\n", mDrvFd);
			if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_ENABLE, &video_composer_enable) != 0) {
				MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
					strerror(errno), errno, mDrvFd);
				return -1;
			}
			mStatus = 0;
		}
	}
	return 0;
}

int32_t HwcVideoPlane::setPlane(
    std::shared_ptr<DrmFramebuffer> fb,
    uint32_t zorder __unused, int blankOp) {
    u32 i;
	video_frame_info_t *vFrameInfo;
    int video_composer_enable;

	if (mDrvFd < 0) {
        MESON_LOGE("hwcvideo plane fd is not valiable!");
        return -EBADF;
    }

	bool bBlank = blankOp == UNBLANK ? false : true;

	if (!bBlank) {

		memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));
		mFramesCount = 1;
		for (i = 0; i < mFramesCount; i++) {
			vFrameInfo = &mVideoFramesInfo.frame_info[i];

			buffer_handle_t buf = fb->mBufferHandle;
			drm_rect_t dispFrame = fb->mDisplayFrame;
			drm_rect_t srcCrop = fb->mSourceCrop;
			vFrameInfo->fd = am_gralloc_get_omx_v4l_file(buf);
			
			vFrameInfo->dst_x = dispFrame.left;
			vFrameInfo->dst_y = dispFrame.top;
			vFrameInfo->dst_w = dispFrame.right - dispFrame.left;
			vFrameInfo->dst_h = dispFrame.bottom - dispFrame.top;

			vFrameInfo->crop_x = srcCrop.left;
			vFrameInfo->crop_y = srcCrop.top;
			vFrameInfo->crop_w = srcCrop.right - srcCrop.left;
			vFrameInfo->crop_h = srcCrop.bottom - srcCrop.top;
			vFrameInfo->zorder = fb->mZorder + 1;
			vFrameInfo->transform = fb->mTransform;
		}
		mVideoFramesInfo.frame_count = mFramesCount;
		mVideoFramesInfo.layer_index = mId;
		mVideoFramesInfo.disp_zorder = fb->mZorder + 1;

		if (!mStatus) {
            video_composer_enable = 1;
			MESON_LOGE("di composer device %d set enable.\n", mDrvFd);
			if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_ENABLE, &video_composer_enable) != 0) {
				MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
					strerror(errno), errno, mDrvFd);
				return -1;
			}
			mStatus = 1;
		}

		MESON_LOGE("di composer device %d set frame.\n", mDrvFd);
		if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_FRAMES, &mVideoFramesInfo) != 0) {
			MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
				strerror(errno), errno, mDrvFd);
			return -1;
		}
	}else {
		memset(&mVideoFramesInfo, 0, sizeof(mVideoFramesInfo));	

		if (mStatus) {
            video_composer_enable = 0;
			MESON_LOGE("di composer device %d set disable.\n", mDrvFd);
			if (ioctl(mDrvFd, VIDEO_COMPOSER_IOCTL_SET_ENABLE, &video_composer_enable) != 0) {
				MESON_LOGE("video composer: ioctl error, %s(%d), mDrvFd = %d",
					strerror(errno), errno, mDrvFd);
				return -1;
			}
			mStatus = 0;
		}	
	}
#if 0
    drm_rect_t srcCrop       = fb->mSourceCrop;
    drm_rect_t disFrame      = fb->mDisplayFrame;
    buffer_handle_t buf      = fb->mBufferHandle;

    mPlaneInfo.magic         = OSD_SYNC_REQUEST_RENDER_MAGIC_V2;
    mPlaneInfo.len           = sizeof(osd_plane_info_t);
    mPlaneInfo.type          = DIRECT_COMPOSE_MODE;

    mPlaneInfo.xoffset       = srcCrop.left;
    mPlaneInfo.yoffset       = srcCrop.top;
    mPlaneInfo.width         = srcCrop.right    - srcCrop.left;
    mPlaneInfo.height        = srcCrop.bottom   - srcCrop.top;
    mPlaneInfo.dst_x         = disFrame.left;
    mPlaneInfo.dst_y         = disFrame.top;
    mPlaneInfo.dst_w         = disFrame.right   - disFrame.left;
    mPlaneInfo.dst_h         = disFrame.bottom  - disFrame.top;

    if (DebugHelper::getInstance().discardInFence()) {
        fb->getAcquireFence()->waitForever("osd-input");
        mPlaneInfo.in_fen_fd = -1;
    } else {
        mPlaneInfo.in_fen_fd     = fb->getAcquireFence()->dup();
    }
    mPlaneInfo.format        = am_gralloc_get_format(buf);
    mPlaneInfo.shared_fd     = am_gralloc_get_buffer_fd(buf);
    mPlaneInfo.byte_stride   = am_gralloc_get_stride_in_byte(buf);
    mPlaneInfo.pixel_stride  = am_gralloc_get_stride_in_pixel(buf);
    /* osd request plane zorder > 0 */
    mPlaneInfo.zorder        = fb->mZorder + 1;
    mPlaneInfo.blend_mode    = fb->mBlendMode;
    mPlaneInfo.plane_alpha   = fb->mPlaneAlpha;
    mPlaneInfo.op            &= ~(OSD_BLANK_OP_BIT);
    mPlaneInfo.afbc_inter_format = am_gralloc_get_vpu_afbc_mask(buf);

    if (ioctl(mDrvFd, FBIOPUT_OSD_SYNC_RENDER_ADD, &mPlaneInfo) != 0) {
        MESON_LOGE("osd plane FBIOPUT_OSD_SYNC_RENDER_ADD return(%d)", errno);
        return -EINVAL;
    }

    if (mDrmFb) {
    /* dup a out fence fd for layer's release fence, we can't close this fd
    * now, cause display retire fence will also use this fd. will be closed
    * on SF side*/
        if (DebugHelper::getInstance().discardOutFence()) {
            mDrmFb->setReleaseFence(-1);
        } else {
            mDrmFb->setReleaseFence((mPlaneInfo.out_fen_fd >= 0) ? ::dup(mPlaneInfo.out_fen_fd) : -1);
        }
    }

    // update drm fb.
    mDrmFb = fb;

    mPlaneInfo.in_fen_fd  = -1;
    mPlaneInfo.out_fen_fd = -1;
#endif
    return 0;
}

void HwcVideoPlane::dump(String8 & dumpstr __unused) {
	u32 i;
	video_frame_info_t *vFrameInfo;
	for (i = 0; i < mFramesCount; i++) {
		vFrameInfo = &mVideoFramesInfo.frame_info[i];
        dumpstr.appendFormat("HwcVideo%2d "
                "     %3d | %1d | %4d, %4d, %4d, %4d |  %4d, %4d, %4d, %4d | %2d | %2d",
                 mId,
                 vFrameInfo->zorder,
                 vFrameInfo->fd,
                 vFrameInfo->crop_x, vFrameInfo->crop_y, vFrameInfo->crop_w, vFrameInfo->crop_h,
                 vFrameInfo->dst_x, vFrameInfo->dst_y, vFrameInfo->dst_w, vFrameInfo->dst_h,
                 vFrameInfo->composer_fen_fd,
                 vFrameInfo->disp_fen_fd);
    }
}

