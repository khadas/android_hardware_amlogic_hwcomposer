/*
* Copyright (c) 2022 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: support 8K video and 4k120hz video
*/

#define LOG_NDEBUG 1
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utils/Trace.h>
#include <hardware/hwcomposer2.h>
#include "MultiplanesWithHRComposition.h"
#include <DrmTypes.h>
#include <MesonLog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "HwcVideoPlane.h"
#include "am_gralloc_ext.h"
#include <DebugHelper.h>
#include <HwcConfig.h>
#include "UvmDev.h"
#include "FrcDev.h"

#define OSD_OUTPUT_ONE_CHANNEL         1

#define OSD_FB_BEGIN_ZORDER            65   // osd zorder: 65 - 128
#define TOP_VIDEO_FB_BEGIN_ZORDER      129  // top video zorder: 129 - 192
#define BOTTOM_VIDEO_FB_BEGIN_ZORDER   1    // bottom video zorder: 1 - 64

#define OSD_SCALER_INPUT_MAX_WIDTH (3840)
#define OSD_SCALER_INPUT_MAX_HEIGHT (2160)

#define OSD_SCALER_INPUT_FACTOR (3.0)
#define OSD_SCALER_INPUT_MARGIN (1.1)

#define DEFAULT_REFRESH_RATE 60

#define IS_FB_COMPOSED(fb) \
    (fb->mZorder >= mMinComposerZorder && fb->mZorder <= mMaxComposerZorder)

/* Constructor function */
MultiplanesWithHRComposition::MultiplanesWithHRComposition() {
    init();
    mOsdPlaneNum = 0;
    mVideoPlaneNum = 0;
    mScaleValue = 0;
    memset(&mDisplayMode, 0, sizeof(mDisplayMode));
    mIsSideBandDisable = false;
    mVideoProcessorsMgr = nullptr;
}

/* Deconstructor function */
MultiplanesWithHRComposition::~MultiplanesWithHRComposition() {
}

/* Clean FrameBuffer, Composer and Plane. */
void MultiplanesWithHRComposition::init() {
    /* Reset Flags */
    mHDRMode             = false;
    mHideSecureLayer     = false;
    mForceClientComposer = false;
    mHaveClient          = false;
    mInsideVideoFbsFlag  = false;
    mVsyncOverDefault = false;
    m4Mosaic = false;
    mIsDisablePostProcessor = false;

    /*crtc scale info.*/
    mDisplayRefFb.reset();
    memset(&mOsdDisplayFrame, 0, sizeof(mOsdDisplayFrame));
    mCrtc.reset();

    /* Clean FrameBuffer */
    mFramebuffers.clear();

    /* Clean Composer */
    mDummyComposer.reset();
    mClientComposer.reset();
    mDiComposer.reset();
    mOtherComposers.clear();

    /* Clean Plane */
    mOsdPlanes.clear();
    mHwcVideoPlanes.clear();
    mOtherPlanes.clear();

    /* Clean Composition members */
    mComposer.reset();
    mOverlayFbs.clear();
    mComposerFbs.clear();
    mDIComposerFbs.clear();
    mHwcVideoInputFbs.clear();
    mDisplayPairs.clear();

    mMinComposerZorder = INVALID_ZORDER;
    mMaxComposerZorder = INVALID_ZORDER;
    mMinVideoZorder    = INVALID_ZORDER;
    mMaxVideoZorder    = INVALID_ZORDER;

    mDumpStr.clear();
}

int MultiplanesWithHRComposition::allocateDiOutputFb(
    std::shared_ptr<DrmFramebuffer> & fb,
    uint32_t z) {
    /*now output fb is fake, allocate once create.*/
    fb = std::make_shared<DrmFramebuffer>();
    fb->mFbType = DRM_FB_DI_COMPOSE_OUTPUT;
    fb->mCompositionType = MESON_COMPOSITION_DI;
    fb->mZorder = z;
    return 0;
}

int MultiplanesWithHRComposition::deleteFbFromeVectorFbs(
        std::vector<std::shared_ptr<DrmFramebuffer>> & inputFbs,
        std::shared_ptr<DrmFramebuffer> & fb) {
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator itErase;

    if (!fb.get())
        return 0;

    /* remove from list */
    itErase = std::find(inputFbs.begin(), inputFbs.end(), fb);
    if (itErase != inputFbs.end()) {
        inputFbs.erase(itErase);
        return 0;
    }
    return 1;
}

int MultiplanesWithHRComposition::chooseVideoFbsBaseNoOverlap(
        std::vector<std::shared_ptr<DrmFramebuffer>> & inputVideoFbs,
        std::vector<std::shared_ptr<DrmFramebuffer>> & outputVideoFbs) {
    drm_rect_t dispFrame, dispFrame1;
    bool is_overlap;
    std::shared_ptr<DrmFramebuffer> fb, fb1;

    for (auto it = inputVideoFbs.begin(); it != inputVideoFbs.end(); it++) {
        fb = *it;
        is_overlap = false;

        /* find one that did not overlap with others */
        dispFrame = fb->getDisplayFrame();
        for (auto it1 = it; it1 != inputVideoFbs.end(); it1++) {
            fb1 = *it1;
            if (fb == fb1)
                continue;

            dispFrame1 = fb1->getDisplayFrame();
            if (std::max(0, std::min(dispFrame.right, dispFrame1.right) -
                std::max(dispFrame.left, dispFrame1.left)) *
                std::max(0, std::min(dispFrame.bottom, dispFrame1.bottom) -
                std::max(dispFrame.top, dispFrame1.top))) {
                is_overlap = true;
                break;
            }
        }

        if (!is_overlap) {
            MESON_LOGV("[%s] LayerId:%" PRIu64 " buffer is not overlap with others",
                    __func__, fb->getUniqueId());
            outputVideoFbs.push_back(fb);
        }
    }

    return 0;
}

int MultiplanesWithHRComposition::chooseOneVideoFbBaseBiggestWindow(
        std::vector<std::shared_ptr<DrmFramebuffer>> & inputVideoFbs,
        std::shared_ptr<DrmFramebuffer> & outputVideoFb) {
    drm_rect_t dispFrame;
    std::shared_ptr<DrmFramebuffer> fb = nullptr;
    std::shared_ptr<DrmFramebuffer> large_fb = nullptr;
    int region = 0, max_region = 0;

    outputVideoFb.reset();

    if (inputVideoFbs.empty())
        return 0;

    for (auto it = inputVideoFbs.begin(); it != inputVideoFbs.end(); it++) {
        fb = *it;

        dispFrame = fb->getDisplayFrame();
        region = (dispFrame.right - dispFrame.left) * (dispFrame.bottom - dispFrame.top);

        if (!large_fb) {
            large_fb = fb;
            max_region = region;
            continue;
        }

        /* find the largest the frame */
        if (region > max_region) {
            large_fb = fb;
            max_region = region;
        } else if (region == max_region) {
            if (fb->mZorder > large_fb->mZorder)
                large_fb = fb;
        }
    }

    outputVideoFb = large_fb;
    dispFrame = large_fb->getDisplayFrame();
    MESON_LOGV("[%s] LayerId:%" PRIu64 " return large region buffer (%d %d %d %d)",
            __func__, large_fb->getUniqueId(),
            dispFrame.left, dispFrame.top, dispFrame.right, dispFrame.bottom);

    return 0;
}

int MultiplanesWithHRComposition::chooseOneVideoFbBaseVideoType(
        std::vector<std::shared_ptr<DrmFramebuffer>> & inputVideoFbs,
        std::shared_ptr<DrmFramebuffer> & outputVideoFb) {
    int video_type;
    std::vector<std::shared_ptr<DrmFramebuffer>> fbs;

    std::vector<int> video_types {AM_VIDEO_DI_POST, AM_VIDEO_SECURE,
        AM_VIDEO_DV, AM_VIDEO_4K, AM_VIDEO_AFBC, AM_VIDEO_HDR,
        AM_VIDEO_HDR10_PLUS, AM_VIDEO_HLG};

    for (auto it = inputVideoFbs.begin(); it != inputVideoFbs.end(); it++) {
        video_type = (*it)->getVideoType();
        if (video_type > 0) fbs.push_back(*it);

        MESON_LOGV("[%s] LayerId:%" PRIu64 " videoType:%x",
                __func__, (*it)->getUniqueId(), video_type);
    }

    if (!fbs.empty()) {
        auto type_it = video_types.begin();
        for (; type_it != video_types.end(); type_it++) {
            bool have_type = false;
            for (auto fbIt = fbs.begin(); fbIt != fbs.end(); fbIt++) {
                video_type = (*fbIt)->getVideoType();
                if ((video_type & *type_it) == *type_it) {
                    have_type = true;
                    break;
                }
            }

            if (have_type) {
                for (auto fbIt = fbs.begin(); fbIt != fbs.end();) {
                    video_type = (*fbIt)->getVideoType();
                    if ((video_type & *type_it) == *type_it)
                        fbIt++;
                    else
                        fbs.erase(fbIt);
                }
            }

            if (fbs.size() == 1)
                break;
        }
    }

    outputVideoFb.reset();

    if (!fbs.empty()) {
        auto it = fbs.begin();
        outputVideoFb = *it;
        for (; it != fbs.end(); it++) {
            if (outputVideoFb->mZorder <= (*it)->mZorder)
                outputVideoFb = *it;
        }
    }

    if (outputVideoFb.get())
        MESON_LOGV("[%s] choose the fb, LayerId:%" PRIu64 " videoType:0x%x",
                __func__, outputVideoFb->getUniqueId(), outputVideoFb->getVideoType());

    return 0;
}

int MultiplanesWithHRComposition::chooseOneVideoFb(
        std::shared_ptr<DrmFramebuffer> & outputVideoFb,
        bool bVideoCompose) {
    std::vector<std::shared_ptr<DrmFramebuffer>> noOverlayFbs;
    outputVideoFb.reset();

    // no Video Fbs
    if (mDIComposerFbs.empty())
        return -EINVAL;

    if (!bVideoCompose) {
        chooseOneVideoFbBaseVideoType(mDIComposerFbs, outputVideoFb);
        if (!outputVideoFb.get())
            chooseOneVideoFbBaseBiggestWindow(mDIComposerFbs, outputVideoFb);
    } else {
        chooseVideoFbsBaseNoOverlap(mDIComposerFbs, noOverlayFbs);
        /* at least one buffer in noOverlayFbs vector */

        if (noOverlayFbs.size() == 1) {
            outputVideoFb = *noOverlayFbs.begin();
        } else {
            chooseOneVideoFbBaseVideoType(noOverlayFbs, outputVideoFb);
            if (!outputVideoFb.get()) {
                chooseOneVideoFbBaseBiggestWindow(noOverlayFbs, outputVideoFb);
            }
        }
    }

    if (outputVideoFb.get()) {
        MESON_LOGV("[%s]: choose the fb, LayerId:%" PRIu64, __func__, outputVideoFb->getUniqueId());
        deleteFbFromeVectorFbs(mDIComposerFbs, outputVideoFb);
    }

    return 0;
}

/*
 * If all the 4 video meet the following requirements, then all of then can send to vd1.
 * 1. each video is within its own 1/4 window
 * 2. The horizontal border distance from each video to its 1/4 window must be equal
 * 3. 4 video must be all afbc
 * 4. The distance to the border must be a multiple of 8
 * 5. only support when refresh rate <= 60hz
 */
bool MultiplanesWithHRComposition::checkAndHandle4Mosaic(uint32_t videoZorder) {
    if (HwcConfig::mosaicEnabled() == false)
        return false;

    // first check if hwc video plane (VD1) support mosaic feature
    HwcVideoPlane *plane = (HwcVideoPlane *)(mHwcVideoPlanes[0].get());
    if (!plane)
        return false;
    if (plane->isSupportMosaic() == false)
        return false;

    // mosaic need disable when output refresh rate > 60hz
    if (mDisplayMode.refreshRate > DEFAULT_REFRESH_RATE)
        return false;

    // check 4 video playback
    if (mDIComposerFbs.empty() || mDIComposerFbs.size() != 4)
        return false;

    std::shared_ptr<DrmFramebuffer> fb;
    uint32_t halfWidth = mDisplayMode.pixelW/2;
    uint32_t halfHeight = mDisplayMode.pixelH/2;

    // 2*2 mosaic check
    int leftWindow = 0;
    int topWindow = 0;
    for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); it++) {
        fb = *it;
        drm_rect_t displayFrame = fb->getDisplayFrame();
        uint32_t frameWidth = displayFrame.right - displayFrame.left;
        uint32_t frameHeight = displayFrame.bottom - displayFrame.top;

        // check all afbc
        if (!(fb->getVideoType() & AM_VIDEO_AFBC))
            return false;

        // check if the width and height <= 1/2 vinfo */
        if (frameWidth > halfWidth || frameHeight > halfHeight)
            return false;

        // check the border distance
        if (displayFrame.left < halfWidth) {
            leftWindow++;
            if (displayFrame.left != halfWidth - displayFrame.right)
                return false;

            // check border distance
            if (displayFrame.left % 8 != 0)
                return false;
        } else {
            if (displayFrame.left - halfWidth != mDisplayMode.pixelW - displayFrame.right)
                return false;

            // check border distance
            if ((displayFrame.left - halfWidth) % 8 != 0)
                return false;
        }

        if (displayFrame.top < halfHeight) {
            topWindow++;
        }
    }

    // check each video within its own 1/4 window
    if (topWindow != leftWindow)
        return false;

    mHwcVideoInputFbs = mDIComposerFbs;
    m4Mosaic = true;

    for (auto it = mHwcVideoInputFbs.begin(); it != mHwcVideoInputFbs.end(); it++) {
        (*it)->mCompositionType = MESON_COMPOSITION_DI;
    }

    /*set dicomposer and get output video.*/
    std::vector<std::shared_ptr<DrmFramebuffer>> nofbs;
    mDiComposer->prepare();
    mDiComposer->addInputs(mDIComposerFbs, nofbs, 0);
    /*TODO: workaround to pass zorder to composer.*/
    hwc_region_t damage = {0, 0};
    allocateDiOutputFb(fb, (*mHwcVideoInputFbs.begin())->mZorder);
    mDiComposer->setOutput(fb, damage, 0);
    mDiComposer->setProcessorsManager(mVideoProcessorsMgr, 0);

    /*-----set buffer to displaypair------*/
    mDisplayPairs.push_back(DisplayPair{
            (uint32_t)mOsdPlaneNum,  videoZorder, fb, mHwcVideoPlanes[0]});

    mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());

    return true;
}

bool MultiplanesWithHRComposition::handleLLM(const bool enable) {
    // LLM need disable FRC
    FrcDev::getInstance().enableFrc(!enable);
    return true;
}

void MultiplanesWithHRComposition::handleLegacySidebandVideoFbs(
        std::vector<std::shared_ptr<DrmFramebuffer>> & sidebandFbs,
        uint32_t maxVideoZ) {
    /*TODO: only one legacy sideband(hwc cannot get fb info) supported now. */
    std::shared_ptr<DrmFramebuffer> fb;
    uint32_t videoZ;

    if (sidebandFbs.size() > 0) {
        /*sideband always push to video plane 0*/
        auto it = sidebandFbs.begin();
        fb = *it;
        it ++;

        for (; it != sidebandFbs.end(); it ++) {
            MESON_LOGD("%s too many sideband, skip.", __func__);
            (*it)->mCompositionType = MESON_COMPOSITION_DUMMY;
        }
        sidebandFbs.clear();
    }

    if (fb) {
        videoZ = (maxVideoZ != INVALID_ZORDER) ? maxVideoZ : fb->mZorder;

        mDisplayPairs.push_back(DisplayPair{
                (uint32_t)mOsdPlaneNum, maxVideoZ, fb, mHwcVideoPlanes[0]});
        mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
    }
}

void MultiplanesWithHRComposition::handleVideoWithoutVideoPlane(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs) {
    auto it = fbs.begin();
    for (; it != fbs.end(); it++)
        (*it)->mCompositionType = MESON_COMPOSITION_CLIENT;
}

void MultiplanesWithHRComposition::handleNonLegacySidebandVideoFbs(
        uint32_t minVideoZ, uint32_t maxVideoZ __unused) {
    bool bVideoCompose = false;
    bool have8K60HZ = false;
    std::shared_ptr<DrmFramebuffer> fb;
    uint32_t videoZ, usedPlanes = 0;
#ifdef MESON_HWC_RESOLUTION_AND_REFRESH_RATE_LIMIT
    int videoType;
#endif
    int videoFbNum = mDIComposerFbs.size();

    if (videoFbNum == 0)
        return;

    if (!mHwcVideoPlanes.size() ||
        mHwcVideoPlanes.size() != mVideoPlaneNum) {
        /* 1, just one video plane to display legacy sideband video
         * 2, not support legacy sideband + non-sideband video */
        handleVideoWithoutVideoPlane(mDIComposerFbs);
        mDIComposerFbs.clear();
        return;
    }

    if (videoFbNum > mVideoPlaneNum)
        bVideoCompose = true;

#ifdef MESON_HWC_RESOLUTION_AND_REFRESH_RATE_LIMIT
    if (mVsyncOverDefault) {
        while (true) {
            if (!chooseOneVideoFb(fb, bVideoCompose)) {
                videoType = fb->getVideoType();

                if ((videoType & AM_VIDEO_8K) == AM_VIDEO_8K) {
                    MESON_LOGV("%s, VideoPlane can not support 8K video "
                        "when refresh rate greater then DEFAULT",
                        __func__);
                    fb->mCompositionType = MESON_COMPOSITION_CLIENT;
                    continue;
                } else {
                    fb->mCompositionType = MESON_COMPOSITION_PLANE_HWCVIDEO;
                    if (bVideoCompose)
                        videoZ = maxVideoZ;
                    else
                        videoZ = fb->mZorder;

                    mDisplayPairs.push_back(DisplayPair{
                            (uint32_t)mOsdPlaneNum, videoZ, fb, mHwcVideoPlanes[0]});
                    mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
                    /* currently, hwc only supports one channel
                     * when refresh rate greater than DEFAULT */
                    handleVideoWithoutVideoPlane(mDIComposerFbs);
                    mDIComposerFbs.clear();
                    return;
                }
            } else {
                /* no find available FB */
                return;
            }
        }
    } else {
#endif
        if (checkAndHandle4Mosaic(minVideoZ)) {
            return;
        }

        for (int i = 0; i < videoFbNum; i++) {
            usedPlanes++;
            /* Fbs set to the last hwcVideoPlane */
            if (i >= mVideoPlaneNum -1) {
                mHwcVideoInputFbs = mDIComposerFbs;
                if (mHwcVideoInputFbs.size() > 0) {
                    if (have8K60HZ) {
                        /* currently, hwc only supports one channel for 8K60Hz */
                        handleVideoWithoutVideoPlane(mDIComposerFbs);
                        mDIComposerFbs.clear();
                        break;
                    }

                    if (bVideoCompose) {
                        videoZ = minVideoZ;
                        auto it = mHwcVideoInputFbs.begin();
                        for (; it != mHwcVideoInputFbs.end(); it++)
                            (*it)->mCompositionType = MESON_COMPOSITION_DI;

                        /*set dicomposer and get output video.*/
                        std::vector<std::shared_ptr<DrmFramebuffer>> nofbs;
                        mDiComposer->prepare();
                        mDiComposer->addInputs(mDIComposerFbs, nofbs, i);
                        /*TODO: workaround to pass zorder to composer.*/
                        hwc_region_t damage = {0, 0};
                        allocateDiOutputFb(fb, (*mHwcVideoInputFbs.begin())->mZorder);
                        mDiComposer->setOutput(fb, damage, i);
                        mDiComposer->setProcessorsManager(mVideoProcessorsMgr, i);
                    } else {
                        fb = *mHwcVideoInputFbs.begin();
                        videoZ = fb->mZorder;
                        fb->mCompositionType = MESON_COMPOSITION_PLANE_HWCVIDEO;
                    }

                    /*-----set buffer to displaypair------*/
                    mDisplayPairs.push_back(DisplayPair{
                            (uint32_t)mOsdPlaneNum + i,  videoZ, fb, mHwcVideoPlanes[i]});

                }
                break;
            }

            if (!chooseOneVideoFb(fb, bVideoCompose)) {
#ifdef MESON_HWC_RESOLUTION_AND_REFRESH_RATE_LIMIT
                // need remove limitations of 8K resolution.
                videoType = fb->getVideoType();
                if ((videoType & AM_VIDEO_8K) == AM_VIDEO_8K) {
                    if ((videoType & AM_VIDEO_DV) == AM_VIDEO_DV) {
                        MESON_LOGV("%s, VideoPlane can not support 8K+DV video ", __func__);
                        fb->mCompositionType = MESON_COMPOSITION_CLIENT;
                        continue;
                    } else {
                        have8K60HZ = true;
                    }
                }
#endif

                MESON_LOGV("[%s] id:%" PRIu64 ", setTo video %d ", __func__, fb->getUniqueId(), i);
                fb->mCompositionType = MESON_COMPOSITION_PLANE_HWCVIDEO;

                if (bVideoCompose) {
                    /* make sure Z-Order of the last VD is the minimum */
                    videoZ = fb->mZorder + 1;;
                } else {
                    videoZ = fb->mZorder;
                }

                mDisplayPairs.push_back(DisplayPair{
                        (uint32_t)mOsdPlaneNum + i, videoZ, fb, mHwcVideoPlanes[i]});
            }
        }

        /* removed used planes from mHwcVideoPlanes */
        for (uint32_t i = 0; i < usedPlanes; i++)
            mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
#ifdef MESON_HWC_RESOLUTION_AND_REFRESH_RATE_LIMIT
    }
#endif
}

int MultiplanesWithHRComposition::processVideoFbs() {
    std::vector<std::shared_ptr<DrmFramebuffer>> sidebandFbs;
    std::shared_ptr<DrmFramebuffer> fb;

    uint32_t minVideoZ = INVALID_ZORDER;
    uint32_t maxVideoZ = INVALID_ZORDER;
    int videoFbNum = 0;
    bool isAiProcess120Enable = false;
    for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
        bool bSideband = false;
        bool bForceClientCompostion = false;
        fb = fbIt->second;

        /* skip dummy */
        if (fb->mCompositionType == MESON_COMPOSITION_DUMMY)
            continue;

        if (fb->mCompositionType == MESON_COMPOSITION_CLIENT)
            bForceClientCompostion = true;

        drm_fb_type_t fbType = fb->getFbType();
        switch (fbType) {
            case DRM_FB_VIDEO_OVERLAY:
            case DRM_FB_VIDEO_SIDEBAND:
            case DRM_FB_VIDEO_SIDEBAND_TV:
                bSideband = true;
                [[clang::fallthrough]];
            case DRM_FB_VIDEO_TUNNEL_SIDEBAND:
                /*sideband type cannot support CLIENT Composition*/
                bForceClientCompostion = false;
                [[clang::fallthrough]];
            case DRM_FB_VIDEO_DMABUF:
            case DRM_FB_VIDEO_UVM_DMA:
                if (bSideband) {
                    sidebandFbs.push_back(fb);
                } else if (fb->haveValidBuffer()) {
                    if (bForceClientCompostion)
                        continue;
                    mDIComposerFbs.push_back(fb);
                } else {
                    fb->mCompositionType = MESON_COMPOSITION_DUMMY;
                    continue;
                }
                videoFbNum++;
                if (minVideoZ == INVALID_ZORDER || fb->mZorder < minVideoZ)
                    minVideoZ = fb->mZorder;

                if (maxVideoZ == INVALID_ZORDER || fb->mZorder > maxVideoZ)
                    maxVideoZ = fb->mZorder;
                break;
            default:
                break;
        };
    }

    if ((videoFbNum <= mVideoPlaneNum) && (mDiComposer != NULL))
        mDiComposer->prepare();

#ifdef ENABLE_AIPROCESS_120
    isAiProcess120Enable = true;
#else
    if (mVsyncOverDefault)
        isAiProcess120Enable = false;
    else
        isAiProcess120Enable = true;
#endif

    if (videoFbNum == 0 ||
        !isAiProcess120Enable || mIsDisablePostProcessor) {
        /* Video Processor: only support one video now,
         * not support legacy sideband ,
         * not support aipq&aisr when refresh rate is greater than 60,
         */
        if (mVideoProcessorsMgr.get()) {
            mVideoProcessorsMgr->tearDownAllProcessors();
            mVideoProcessorsMgr.reset();
        }

        if (videoFbNum == 0)
            return 0;
    } else {
        if (!mVideoProcessorsMgr.get())
            mVideoProcessorsMgr = std::make_shared<VideoProcessorsManager>();

        mVideoProcessorsMgr->setup(mDIComposerFbs);
    }

    /*
    * Composition: only 1 + .. + N mode now
    * For vd1:
    * 1. support on channel legacy sideband.
    * 2. special video types: 8K/AFBC/DV/HDR/HDR10/HLG/SECURE/DIPOST.
    * 3. no overlap & biggest one.
    * For the last vd:
    * 1. compose always happened on the last vd.
    * 2. the last vd compose always output fullscreen buffer.
    * 3. vd1 should on top of vd2 when have video composed.
    */

    handleLegacySidebandVideoFbs(sidebandFbs, maxVideoZ);
    handleNonLegacySidebandVideoFbs(minVideoZ, maxVideoZ);

    return 0;
}

/* Apply flag with secure and forceClient Fbs. */
int MultiplanesWithHRComposition::applyCompositionFlags() {
    if (!mHideSecureLayer && !mForceClientComposer) {
        return 0;
    }

    std::shared_ptr<DrmFramebuffer> fb;
    auto fbIt = mFramebuffers.begin();
    for (; fbIt != mFramebuffers.end(); ++fbIt) {
        fb = fbIt->second;
        if (fb->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
            if (mHideSecureLayer && fb->mSecure) {
                fb->mCompositionType = MESON_COMPOSITION_DUMMY;
            } else if (mForceClientComposer) {
                fb->mCompositionType = MESON_COMPOSITION_CLIENT;
            }
        }
    }

    return 0;
}

/* handle uvm, */
int MultiplanesWithHRComposition::handleUVM() {
    std::shared_ptr<DrmFramebuffer> fb;
    auto fbIt = mDisplayPairs.begin();

    for (; fbIt != mDisplayPairs.end(); ++fbIt) {
        fb = fbIt->fb;
        if (fb->mFbType == DRM_FB_VIDEO_UVM_DMA) {
            int uFd = am_gralloc_get_buffer_fd(fb->mBufferHandle);
            if (UvmDev::getInstance().commitDisplay(uFd, 1)) {
                MESON_LOGE("UVM set fd data ioctl error %s", strerror(errno));
                continue;
            }
        }
    }

    return 0;
}

int MultiplanesWithHRComposition::processGfxFbs() {
    /* Remove dummy and video Fbs for later osd composition.
     * Pickout OSD Fbs.
     * Save client flag.
     */
    pickoutOsdFbs();

    if (!mInsideVideoFbsFlag) {
        handleOsdComposition();
    } else {
        handleOsdCompositionWithVideo();
    }

    /* record overlayFbs and start to compose */
    if (mComposer.get()) {
        mComposer->prepare();
        mComposer->addInputs(mComposerFbs, mOverlayFbs);
    }

    return 0;
}

/* Delete dummy and video Fbs, then pickout OSD Fbs. */
int MultiplanesWithHRComposition::pickoutOsdFbs() {
    std::shared_ptr<DrmFramebuffer> fb;
    std::vector<std::shared_ptr<DrmFramebuffer>> dummyFbs;
    bool bRemove = false;
    bool bClientLayer = false;
    auto fbIt = mFramebuffers.begin();
    for (; fbIt != mFramebuffers.end(); ) {
        fb = fbIt->second;
        bRemove = false;
        bClientLayer = false;
        switch (fb->mCompositionType) {
            case MESON_COMPOSITION_DUMMY:
                dummyFbs.push_back(fb);
                bRemove = true;
                break;

            case MESON_COMPOSITION_PLANE_HWCVIDEO:
            case MESON_COMPOSITION_DI:
                MESON_LOGV("remove overlay fb (%d)", fb->mZorder);
                mOverlayFbs.push_back(fb);
                bRemove = true;
                break;

            case MESON_COMPOSITION_CLIENT:
                mHaveClient  = true;
                bClientLayer = true;
                [[clang::fallthrough]];
            case MESON_COMPOSITION_UNDETERMINED:
                {
                    /* we thought plane with same type have some scanout capacity.
                     * so just check with first osd plane.
                     */
                    auto plane = mOsdPlanes.begin();
                    if (bClientLayer || (*plane)->isFbSupport(fb) == false) {
                        if (mMinComposerZorder == INVALID_ZORDER ||
                            mMaxComposerZorder == INVALID_ZORDER) {
                            mMinComposerZorder = fb->mZorder;
                            mMaxComposerZorder = fb->mZorder;
                        } else {
                            if (mMinComposerZorder > fb->mZorder)
                                mMinComposerZorder = fb->mZorder;
                            if (mMaxComposerZorder < fb->mZorder)
                                mMaxComposerZorder = fb->mZorder;
                        }
                    }
                }
                break;

            default:
                MESON_LOGE("Unknown composition type(%d)", fb->mCompositionType);
                bRemove = true;
                break;
        }

        if (bRemove)
            fbIt = mFramebuffers.erase(fbIt);
        else
            ++ fbIt;
    }

    if (dummyFbs.size() > 0) {
        std::vector<std::shared_ptr<DrmFramebuffer>> dummyOverlayFbs;
        mDummyComposer->addInputs(dummyFbs, dummyOverlayFbs);
    }

    /* 1. check mInsideVideoFbsFlag = false
     * 2. for HDR mode, adjust compose range.
     */
    if (!mOverlayFbs.empty() && !mFramebuffers.empty()) {
        bool bVideoCompose = mOverlayFbs.size() > 2 ?  true : false;
        auto uiFbIt = mFramebuffers.begin();
        uint32_t uiFbMinZorder = uiFbIt->second->mZorder;
        uiFbIt = mFramebuffers.end();
        uiFbIt --;
        uint32_t uiFbMaxZorder = uiFbIt->second->mZorder;
        int videoOnTop = 0;

        for (auto videoFbIt = mOverlayFbs.begin(); videoFbIt != mOverlayFbs.end(); ) {
            std::shared_ptr<DrmFramebuffer> videoFb = *videoFbIt;
             if (videoFb->mZorder >= uiFbMinZorder && videoFb->mZorder <= uiFbMaxZorder) {
                if (mHDRMode || bVideoCompose) {
                    /* For hdr composition: it's hw limit.
                     * video should top or bottom.
                     * when video layer is not top, make all the lower framebuffers composed.
                     * For fixed planemode, video is always bottom.
                     */
                    mMinComposerZorder = mFramebuffers.begin()->second->mZorder;
                    if (mMaxComposerZorder == INVALID_ZORDER || videoFb->mZorder > mMaxComposerZorder) {
                        mMaxComposerZorder = videoFb->mZorder;
                    }
                } else {
                    mInsideVideoFbsFlag = true;
                }
                ++videoFbIt;
            } else {
                /*only one video on top of ui */
                if (videoFb->mZorder > uiFbMaxZorder) {
                    if (videoOnTop >= 1 || bVideoCompose) {
                        ++videoFbIt;
                        continue;
                    }
                    videoOnTop ++;
                }

                videoFbIt = mOverlayFbs.erase(videoFbIt);
            }
        }
    }

    return 0;
}

/*Count below, inside and up client Fbs number.
 ********************************************************************
 **                                                                 *
 **                    |-----------------------                     *
 **       upClientNum--{-----------------------                     *
 **                    |-----------------------                     *
 **                     ----maxClientZorder----|                    *
 **                     -----------------------} --insideClientNum  *
 **                     ----minClientZorder----|                    *
 **                    |-----------------------                     *
 **    belowClientNum--{-----------------------                     *
 **                    |-----------------------                     *
 **                                                                 *
 ********************************************************************
 */
int MultiplanesWithHRComposition::countComposerFbs(int &belowClientNum, int &upClientNum, int &insideClientNum) {
    if (mMinComposerZorder == INVALID_ZORDER) {
        belowClientNum = 0;
        upClientNum = 0;
        insideClientNum = 0;
        return 0;
    }

    std::shared_ptr<DrmFramebuffer> fb;
    auto fbIt = mFramebuffers.begin();
    for (; fbIt != mFramebuffers.end(); ++fbIt) {
        fb = fbIt->second;
        if (fb->mZorder < mMinComposerZorder)
            belowClientNum++;
        else if (fb->mZorder > mMaxComposerZorder)
            upClientNum++;
        else
            insideClientNum++;
    }

    return 1;
}

int MultiplanesWithHRComposition::confirmComposerRange() {
    std::shared_ptr<DrmFramebuffer> fb;
    uint32_t osdFbsNum    = mFramebuffers.size();
    uint32_t osdPlanesNum = mOsdPlanes.size();
    int belowClientNum  = 0;
    int upClientNum     = 0;
    int insideClientNum = 0;
    if (osdFbsNum == 0 || osdPlanesNum == 0) {
        return 0;
    }
    int ret = countComposerFbs(belowClientNum, upClientNum, insideClientNum);
    UNUSED(ret);
    if (osdFbsNum > osdPlanesNum) { // CASE 1: osdFbsNum > osdPlanesNum , need compose more fbs.*/
        /* CASE 1_1: mMinComposerZorder != INVALID_ZORDER, need do compose. */
        int minNeedComposedFbs = osdFbsNum - insideClientNum - osdPlanesNum + 1;  // minimum OSD Fbs need to be composered
        if (mMinComposerZorder != INVALID_ZORDER) {
            /* If minNeedComposedFbs = 0, use client range to compose */
            if (minNeedComposedFbs > 0) {
                /* compose more fbs from minimum zorder first. */
                auto fbIt = mFramebuffers.begin();
                if (belowClientNum > 0 && minNeedComposedFbs <= belowClientNum) {
                    int noComposedFbs = belowClientNum - minNeedComposedFbs;
                    if (noComposedFbs > 0) {
                        for (; fbIt != mFramebuffers.end(); ++fbIt) {
                            noComposedFbs -- ;
                            if (noComposedFbs < 0)
                                break;
                        }
                    }
                    minNeedComposedFbs = 0;
                } else {
                    minNeedComposedFbs -= belowClientNum;
                }
                /* confirm the minimum zorder value. */
                if (fbIt != mFramebuffers.end())
                    mMinComposerZorder = fbIt->second->mZorder;

                /* compose fb from maximum zorder. */
                if (minNeedComposedFbs > 0) {
                    MESON_ASSERT(upClientNum > 0, "upClientNum should > 0.");
                    fbIt = mFramebuffers.upper_bound(mMaxComposerZorder);
                    for (; fbIt != mFramebuffers.end(); ++ fbIt) {
                        minNeedComposedFbs --;
                        if (minNeedComposedFbs <= 0)
                            break;
                    }

                    /* we can confirm the maximum zorder value. */
                    if (fbIt != mFramebuffers.end())
                        mMaxComposerZorder = fbIt->second->mZorder;
                }
            }
        }  else {
        /* CASE 1_2: no fb set to compose before, free to choose new fbs to compose */
            int countComposerFbs = 0;
            for (auto it = mFramebuffers.begin(); it != mFramebuffers.end(); ++it) {
                fb = it->second;
                countComposerFbs++;
                if (countComposerFbs == minNeedComposedFbs) {
                    mMaxComposerZorder = fb->mZorder;
                    mMinComposerZorder = (mFramebuffers.begin()->second)->mZorder;
                    break;
                }
            }
        }
    }

    return 0;
}

int32_t MultiplanesWithHRComposition::compareFbScale(
    drm_rect_t & aSrc,
    drm_rect_t & aDst,
    drm_rect_t & bSrc,
    drm_rect_t & bDst) {
    int32_t aDisplayWidth = aDst.right - aDst.left;
    int32_t aDisplayHeight = aDst.bottom - aDst.top;
    int32_t aSrcWidth = aSrc.right - aSrc.left;
    int32_t aSrcHeight = aSrc.bottom - aSrc.top;
    int32_t bDisplayWidth = bDst.right - bDst.left;
    int32_t bDisplayHeight = bDst.bottom - bDst.top;
    int32_t bSrcWidth = bSrc.right - bSrc.left;
    int32_t bSrcHeight = bSrc.bottom - bSrc.top;

    int widthCompare = aDisplayWidth*bSrcWidth - bDisplayWidth*aSrcWidth;
    int heightCompare = aDisplayHeight *bSrcHeight - bDisplayHeight * aSrcHeight;
    if (widthCompare == 0 && heightCompare == 0)
        return 0;
    else if (widthCompare > 0 && heightCompare > 0)
        return 1;
    else {
        //MESON_LOGW("compareFbScale failed %d ,%d, %d, %d",
            //widthCompare, heightCompare,
            //bDisplayWidth, bDisplayHeight);
        return -1;
    }
}

/* Set DisplayPairs between UI(OSD) Fbs with plane. */
int MultiplanesWithHRComposition::setOsdFbs2PlanePairs() {
    if (mFramebuffers.size() == 0)
        return 0;

    std::shared_ptr<HwDisplayPlane> osdTemp;
    for (uint32_t i = 1; i < mOsdPlanes.size(); i++) {
        if (mOsdPlanes[i]->getCapabilities() & PLANE_PRIMARY) {
            osdTemp = mOsdPlanes[0];
            mOsdPlanes[0] = mOsdPlanes[i];
            mOsdPlanes[i] = osdTemp;
            break;
        }
    }

    uint32_t usedPlanes = 0;

    if (mDisplayRefFb.get()) {
        /*baseFb always post to din0*/
        mDisplayPairs.push_back(
            DisplayPair{usedPlanes, mDisplayRefFb->mZorder, mDisplayRefFb, mOsdPlanes[usedPlanes]});
        /* Not composed fb, set to osd composition. */
        if (mDisplayRefFb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
            mDisplayRefFb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
    }

    for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
        std::shared_ptr<DrmFramebuffer> fb = fbIt->second;
        if (fb == mDisplayRefFb)
            continue;
        /* fb is unscaled or is represent of composer output,
         * we pick primary display.
         */
        /* Pick any available plane */
        if (usedPlanes != mOsdPlanes.size() - 1) {
            usedPlanes++;
        }
        mDisplayPairs.push_back(
                DisplayPair{usedPlanes, fb->mZorder, fb, mOsdPlanes[usedPlanes]});

        /* Not composed fb, set to osd composition. */
        if (fb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
            fb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
    }

    /* removed used planes from mOsdPlanes */
    for (uint32_t i = 0; i <= usedPlanes; i++) {
        mOsdPlanes.erase(mOsdPlanes.begin());
    }

    return 0;
}

/* Select composer */
int MultiplanesWithHRComposition::selectComposer() {
    if (mComposerFbs.size() == 0) {
        mClientComposer->prepare();
        return 0;
    }

    if (!mHaveClient) {
        auto composerIt = mOtherComposers.begin();
        for (; composerIt != mOtherComposers.end(); ++composerIt) {
            if ((*composerIt)->isFbsSupport(mComposerFbs, mOverlayFbs)) {
                mComposer = *composerIt;
                break;
            }
        }
    }
    if (mComposer.get() == NULL)
        mComposer = mClientComposer;

    /* set composed fb composition type */
    for (auto it = mComposerFbs.begin(); it != mComposerFbs.end(); ++it) {
        (*it)->mCompositionType = mComposer->getType();
    }

    return 0;
}

/* Find out which fb need to compose and push DisplayPair */
int MultiplanesWithHRComposition::fillComposerFbs() {
    std::shared_ptr<DrmFramebuffer> fb;
    if (mMaxComposerZorder != INVALID_ZORDER &&
        mMinComposerZorder != INVALID_ZORDER) {
        auto fbIt = mFramebuffers.begin();
        auto lastComposeFbIt = mFramebuffers.end();
        while (fbIt != mFramebuffers.end()) {
            fb = fbIt->second;
            if (IS_FB_COMPOSED(fb)) {
                mComposerFbs.push_back(fb);
                if (lastComposeFbIt != mFramebuffers.end()) {
                    mFramebuffers.erase(lastComposeFbIt);
                }
                lastComposeFbIt = fbIt;
            }
            ++ fbIt;
        }

        MESON_ASSERT(lastComposeFbIt != mFramebuffers.end(), "no last composer fb !");
        MESON_ASSERT(mDisplayRefFb.get() == NULL, "reffb should be NULL.");
        mDisplayRefFb = lastComposeFbIt->second;
    }

    for (auto videoFbIt = mOverlayFbs.begin(); videoFbIt != mOverlayFbs.end();) {
        std::shared_ptr<DrmFramebuffer> videoFb = *videoFbIt;
        if (IS_FB_COMPOSED(videoFb)) {
            ++videoFbIt;
        } else {
            videoFbIt = mOverlayFbs.erase(videoFbIt);
        }
    }

    return 0;
}

/* Update present zorder.
 * If videoZ ~ (mMinComposerZorder, mMaxComposerZorder), set maxVideoZ = maxVideoZ - 1
 */
void MultiplanesWithHRComposition::handleOverlayVideoZorder() {
    auto it = mDisplayPairs.begin();
    it = mDisplayPairs.begin();
    for (; it != mDisplayPairs.end(); ++it) {
        if (IS_FB_COMPOSED(it->fb)) {
            it->presentZorder = it->presentZorder - 1;
        }
    }
}

/*
 * Scale Limitation:
 * 1. VPU only support composed 2 non afbc osd layers
 * 2. For afbc layers,  exp_h = SourceFrame_height * 1/3 * 1.1;
 *     when DisplayFrame_height < exp_h then only support two layers.
 */
void MultiplanesWithHRComposition::handleVPUScaleLimit() {
    uint32_t nonAfbcFbsNumb = 0;
    uint32_t limitCount = 0;

    int belowClientNum  = 0;
    int upClientNum     = 0;
    int insideClientNum = 0;

    std::shared_ptr<DrmFramebuffer> fb;
    for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
        fb = fbIt->second;
        float scaleLimit = DebugHelper::getInstance().getScaleLimit();
        if (scaleLimit == 0)
            scaleLimit = OSD_SCALER_INPUT_MARGIN / OSD_SCALER_INPUT_FACTOR;

        drm_rect_t dispFrame = fb->getDisplayFrame();
        float expHeight = (fb->mSourceCrop.bottom - fb->mSourceCrop.top) * scaleLimit;
        float dispHeight = (dispFrame.bottom - dispFrame.top) * mScaleValue;

        /* osdComposed layers */
        if (fb->mZorder < mMinComposerZorder || fb->mZorder > mMaxComposerZorder) {
            /* it is non afbc layer */
            if (am_gralloc_get_vpu_afbc_mask(fb->mBufferHandle) == 0) {
                nonAfbcFbsNumb++;
            }

            if (expHeight > dispHeight) {
                limitCount++;

               if (fb->mZorder > mMaxComposerZorder)
                   upClientNum++;
               else
                   belowClientNum++;
            }
        } else  {
            insideClientNum++;
        }
    }

    /* minus one layer for VPU composer */
    if (limitCount >= 1 || nonAfbcFbsNumb > 2) {
        if (upClientNum > 0) {
            auto fbIt = mFramebuffers.upper_bound(mMaxComposerZorder);
            if (fbIt != mFramebuffers.end())
                mMaxComposerZorder = fbIt->second->mZorder;
        } else if (belowClientNum > 0) {
            if (mMinComposerZorder == INVALID_ZORDER) {
                /* free to select one */
                mMinComposerZorder = mFramebuffers.begin()->second->mZorder;
                mMaxComposerZorder = mMinComposerZorder;
            } else {
                auto fbIt = mFramebuffers.lower_bound(mMinComposerZorder);
                if (fbIt != mFramebuffers.begin() && fbIt != mFramebuffers.end()) {
                    fbIt--;
                    mMinComposerZorder = fbIt->second->mZorder;
                }
            }
        }
    }
}

/*
Limitation:
1. scale input should smaller than 1080P.
2. din0 should input the base fb.
*/
void MultiplanesWithHRComposition::handleVPULimit(bool video) {
    UNUSED(video);
    //MESON_ASSERT(video == false, "handleVPULimit havenot support video");

    if (mFramebuffers.size() == 0)
        return ;

    handleVPUScaleLimit();

    if (mMaxComposerZorder != INVALID_ZORDER &&
        mMinComposerZorder != INVALID_ZORDER) {
        /*will use compose fb as reffb.*/
        return ;
    }

    /*select base fb and set base scale info.*/
    int32_t minXOffset = -1, minYOffset = -1;
    int32_t compositionTargetW = 0, compositionTargetH = 0;
    for (auto it = mFramebuffers.begin(); it != mFramebuffers.end(); it++) {
        drm_rect_t dispFrame = it->second->getDisplayFrame();
        if (minXOffset == -1 || minXOffset > dispFrame.left)
            minXOffset = dispFrame.left;
        if (minYOffset == -1 || minYOffset > dispFrame.top)
            minYOffset = dispFrame.top;

        if (dispFrame.right > compositionTargetW)
            compositionTargetW = dispFrame.right;
        if (dispFrame.bottom > compositionTargetH)
            compositionTargetH = dispFrame.bottom;
    }

    /*choose base fb, the scale is smallest bigger.*/
    compositionTargetW = compositionTargetW - minXOffset;
    compositionTargetH = compositionTargetH - minYOffset;

    drm_rect_t scaleInput = {0, 0,
        OSD_SCALER_INPUT_MAX_WIDTH, OSD_SCALER_INPUT_MAX_HEIGHT};
    drm_rect_t scaleOutput = {0, 0, compositionTargetW, compositionTargetH};

    /*choose the scale > targetW/MAX_INPUT*/
    for (auto it = mFramebuffers.begin(); it != mFramebuffers.end(); it++) {
        std::shared_ptr<DrmFramebuffer> fb = it->second;
        drm_rect_t dispFrame = fb->getDisplayFrame();
        int32_t ret = compareFbScale(fb->mSourceCrop, dispFrame, scaleInput, scaleOutput);
        if (0 == ret) {
            mDisplayRefFb = fb;
            break;
        } else if (1 == ret) {
            if (!mDisplayRefFb)
                mDisplayRefFb = fb;
            else {
                drm_rect_t refDispFrame = mDisplayRefFb->getDisplayFrame();
                if (-1 == compareFbScale(fb->mSourceCrop, dispFrame,
                    mDisplayRefFb->mSourceCrop, refDispFrame)) {
                    mDisplayRefFb = fb;
                }
            }
        }
    }

    /*no suitable fb, we need do composition*/
    if (!mDisplayRefFb) {
        /* All Scale Fbs and no default composer range, select begin Fbs to composer. */
        mMinComposerZorder = mFramebuffers.begin()->second->mZorder;
        mMaxComposerZorder = mMinComposerZorder;
    } else {
        /*set display offset, the offset will be updated when commit() if reffb is composed*/
        mOsdDisplayFrame.crtc_display_x = minXOffset;
        mOsdDisplayFrame.crtc_display_y = minYOffset;
    }
}

void MultiplanesWithHRComposition::handleDisplayLayerZorder() {
    int topVideoNum = 0;
    uint32_t maxOsdZorder = INVALID_ZORDER;
    for (auto it = mDisplayPairs.begin(); it != mDisplayPairs.end(); ++it) {
        std::shared_ptr<DrmFramebuffer> fb = it->fb;
        std::shared_ptr<HwDisplayPlane> plane = it->plane;
        if (OSD_PLANE == plane->getType()) {
            if (maxOsdZorder == INVALID_ZORDER) {
                maxOsdZorder = it->presentZorder;
            } else {
                if (maxOsdZorder < it->presentZorder)
                    maxOsdZorder = it->presentZorder;
            }
            it->presentZorder = it->presentZorder + OSD_FB_BEGIN_ZORDER; // osd zorder: 65 - 128
        }
    }

    for (auto it = mDisplayPairs.begin(); it != mDisplayPairs.end(); ++it) {
        std::shared_ptr<DrmFramebuffer> fb = it->fb;
        std::shared_ptr<HwDisplayPlane> plane = it->plane;
        if (HWC_VIDEO_PLANE == plane->getType()) {
            if (fb->mZorder > maxOsdZorder && topVideoNum != 1) {
                it->presentZorder = it->presentZorder + TOP_VIDEO_FB_BEGIN_ZORDER; // top video zorder: 129 - 192
                topVideoNum++;
            } else {
                it->presentZorder = it->presentZorder + BOTTOM_VIDEO_FB_BEGIN_ZORDER; // bottom video zorder: 1 - 64
            }
        }
    }
}

/* Handle OSD Fbs and set OsdFbs2Plane pairs. */
int MultiplanesWithHRComposition::handleOsdComposition() {
    /* Step 1:
     * Judge whether compose or not.
     * If need to compose, confirm max/min client zorder.
     */
    confirmComposerRange();
    handleVPULimit(false);

    /* Step 2:
     * Push composers to cache mComposerFbs to build OSD2Plane pair.
     */
    fillComposerFbs();

    /* Step 3:
     * Select composer.
     */
    selectComposer();

    /* Step 4:
     * Set DisplayPairs between OSD Fbs with plane.
     */
    setOsdFbs2PlanePairs();

    return 0;
}

int MultiplanesWithHRComposition::handleOsdCompositionWithVideo() {
    std::shared_ptr<DrmFramebuffer> fb;

    /* STEP 1: handle two video fbs. */
    if (mOverlayFbs.size() > 1) {
        MESON_ASSERT(mOverlayFbs.size() <= 3, "Only support 3 video layers now.");
        /* CASE VIDEO_1: have two video fbs between ui fbs. */
        /* Calculate max/min video zorder. */
        mMinVideoZorder = INVALID_ZORDER;
        mMaxVideoZorder = INVALID_ZORDER;
        auto videoIt = mOverlayFbs.begin();
        for (; videoIt != mOverlayFbs.end(); ++ videoIt) {
            fb = *videoIt;
            if (mMinVideoZorder == INVALID_ZORDER) {
                mMinVideoZorder = fb->mZorder;
                mMaxVideoZorder = fb->mZorder;
            } else {
                if (mMinVideoZorder > fb->mZorder)
                    mMinVideoZorder = fb->mZorder;
                if (mMaxVideoZorder < fb->mZorder)
                    mMaxVideoZorder = fb->mZorder;
            }
        }

        /* Judge whether double video are neighbor or not */
        bool bNeighborVideo = true;
        auto fbIt = mFramebuffers.lower_bound(mMinVideoZorder);
        for (; fbIt != mFramebuffers.end(); ++ fbIt) {
            if (fbIt->second->mZorder > mMinVideoZorder && fbIt->second->mZorder < mMaxVideoZorder) {
                bNeighborVideo = false;
                break;
            }
        }

        if (!bNeighborVideo) {
            /* CASE VIDEO_1_1: two video fbs are not neighbor. */
            if (mMinComposerZorder != INVALID_ZORDER) {
                /* CASE VIDEO_1_1_1: have default compose range */
                /* Change compose range to cover two video fbs. */
/*
case 1: Both up and below client has video      |case 2_1: Only up client has video        |case 2_2: Only below client has video
        maxClientZorder from 5-->7              | maxClientZorder from 5-->7               | maxClientZorder not change
        minClientZorder from 4-->3              | minClientZorder not change               | minClientZorder from 4-->3
zorder: 8 -- osd ---------                      | 8 -- osd ---------                       | 8 -- osd ---------
        7 -- maxVideo ---- maxClient(composer)  | 7 -- maxVideo ----  maxClient(composer)  | 7 -- client osd -- maxClient(composer)
        6 -- osd ---------                      | 6 -- osd ---------                       | 6 -- maxVideo ----
        5 -- client osd --                      | 5 -- client osd --                       | 5 -- client osd --
        4 -- client osd --                      | 4 -- client osd --                       | 4 -- client osd --
        3 -- osd --------- minClient(composer)  | 3 -- minVideo ----                       | 3 -- osd ---------  minClient(composer)
        2 -- minVideo ----                      | 2 -- client osd --  minClient(composer)  | 2 -- minVideo ----
        1 -- osd ---------                      | 1 -- osd ---------                       | 1 -- osd ---------
*/
                if (mMinComposerZorder > mMinVideoZorder) {
                    mMinComposerZorder = mMinVideoZorder + 1; // +1 to not compose the video
                }
                if (mMaxComposerZorder < mMaxVideoZorder) {
                    mMaxComposerZorder = mMaxVideoZorder;
                }
            } else {
                /* CASE VIDEO_1_1_2:no default compose range
                 * Compose the fbs below minVideoZ, so the minVideoZ will be bottom.
                 * CASE: two video inside osd ui without client(default) range
                 * set maxClientZorder = 2
                 * set minClientZorder = 1
                 * zorder: 5 -- osd -------
                 *         4 -- maxVideo --
                 *         3 -- osd -------
                 *         2 -- minVideo -- maxClient(composer)
                 *         1 -- osd ------- minClient(composer)
                 */
                mMaxComposerZorder = mMinVideoZorder;
                mMinComposerZorder = mFramebuffers.begin()->first;
            }
            /* goto case VIDEO_2 */
        } else {
            /* CASE VIDEO_1_2: two video fbs are neighbor, treat as one video. */
            /* goto case VIDEO_2 */
        }
    }

    /* STEP 2: now only one video fb or two neighbor video fbs.
     * pick ui fbs to compose.
     */
    confirmComposerRange();
    handleVPULimit(true);

    /* Push composers to cache mComposerFbs to build OSD2Plane pair. */
    fillComposerFbs();

    /* Step 3:
     * Select composer.
     */
    selectComposer();

    setOsdFbs2PlanePairs();

    return 0;
}

/* The public setup interface.
 * layers: UI(include OSD and VIDEO) layer from SurfaceFlinger.
 * composers: Composer style.
 * planes: Get OSD and VIDEO planes from HwDisplayManager.
 */
void MultiplanesWithHRComposition::setup(
    std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
    std::vector<std::shared_ptr<IComposer>> & composers,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
    std::shared_ptr<HwDisplayCrtc> & crtc,
    uint32_t reqFlag,
    float scaleValue,
    drm_mode_info_t mode) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    init();

    mCompositionFlag = reqFlag;
    mScaleValue = scaleValue;
    mCrtc = crtc;
    mDisplayMode = mode;

    // need remove limitation of refrash rate
    if (mode.refreshRate > DEFAULT_REFRESH_RATE) {
        mVsyncOverDefault = true;
    }

    /* add layers */
    auto layerIt = layers.begin();
    for (; layerIt != layers.end(); ++layerIt) {
        std::shared_ptr<DrmFramebuffer> layer = *layerIt;
        mFramebuffers.insert(make_pair(layer->mZorder, layer));
    }

    /* collect composers */
    auto composerIt = composers.begin();
    for (; composerIt != composers.end(); ++composerIt) {
        std::shared_ptr<IComposer> composer = *composerIt;
        switch (composer->getType()) {
            case MESON_COMPOSITION_DUMMY:
                if (mDummyComposer == NULL)
                    mDummyComposer = composer;
                break;

            case MESON_COMPOSITION_CLIENT:
                if (mClientComposer == NULL)
                    mClientComposer = composer;
                break;

            case MESON_COMPOSITION_DI:
                if (mDiComposer == NULL)
                    mDiComposer = composer;
                break;

            default:
                mOtherComposers.push_back(composer);
                break;
        }
    }

    /* collect planes */
    auto planeIt = planes.begin();
    for (; planeIt != planes.end(); ++planeIt) {
        std::shared_ptr<HwDisplayPlane> plane = *planeIt;
        switch (plane->getType()) {
            case OSD_PLANE:
                    mOsdPlanes.push_back(plane);
                    break;

            case HWC_VIDEO_PLANE:
                mHwcVideoPlanes.push_back(plane);
                break;

            default:
                mOtherPlanes.push_back(plane);
                break;
        }
    }

    mVideoPlaneNum = mHwcVideoPlanes.size();
    mOsdPlaneNum = mOsdPlanes.size();

#if OSD_OUTPUT_ONE_CHANNEL
    mHDRMode = true;
#else
    if (reqFlag & COMPOSE_WITH_HDR_VIDEO) {
        mHDRMode = true;
    }
#endif
    if (reqFlag & COMPOSE_HIDE_SECURE_FB) {
        mHideSecureLayer = true;
    }
    if (reqFlag & COMPOSE_FORCE_CLIENT) {
        mForceClientComposer = true;
    }
}

//for present skip validate need update composition
void MultiplanesWithHRComposition::updateComposition() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);

    mOtherPlanes.clear();
    mDumpStr.clear();
}

/* Decide to choose which Fbs and how to build OsdFbs2Plane pairs. */
int MultiplanesWithHRComposition::decideComposition() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    int ret = 0;
    if (mFramebuffers.empty()) {
        MESON_LOGV("No layers to compose, exit.");
        if (mClientComposer != NULL) {
            hwc_region_t damage = {0, 0};
            std::shared_ptr<DrmFramebuffer> fb = nullptr;

            mClientComposer->prepare();
            mClientComposer->setOutput(fb, damage, 1);
        }

        return ret;
    }

    /* handle VIDEO fbs. */
    processVideoFbs();

    /* handle HDR mode, hide secure layer, and force client. */
    applyCompositionFlags();
    /* handle graphic fbs.*/
    processGfxFbs();

    handleOverlayVideoZorder();
    handleDisplayLayerZorder();

    return ret;
}

/* Commit DisplayPair to display. */
int MultiplanesWithHRComposition::commit() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    bool setPlaneSuccess = true;
    bool bHaveVTBuffer = false;
    int ret = -1;

    if (mCrtc == nullptr) {
        MESON_LOGE("MultiplanesWithHRComposition::%s commit but no crtc!",
                __func__);
        return HWC2_ERROR_NO_RESOURCES;
    }

    /* replace composer output with din0 Pair. */
    std::shared_ptr<DrmFramebuffer> composerOutput;

    if (mComposer.get()) {
        mComposer->start();
        composerOutput = mComposer->getOutput();
    }

    /* handle uvm */
    handleUVM();

    for (auto displayIt = mDisplayPairs.begin(); displayIt != mDisplayPairs.end(); ++displayIt) {
        uint32_t presentZorder = displayIt->presentZorder;
        std::shared_ptr<DrmFramebuffer> fb = displayIt->fb;
        std::shared_ptr<HwDisplayPlane> plane = displayIt->plane;
        drm_plane_blank_t blankFlag = (mHideSecureLayer && fb->mSecure) ?
            BLANK_FOR_SECURE_CONTENT : UNBLANK;

        if (composerOutput.get() &&
            fb->mCompositionType == mComposer->getType()) {
            presentZorder = mMaxComposerZorder + OSD_FB_BEGIN_ZORDER;
            bool bDumpPlane = true;
            for (auto it = mComposerFbs.begin(); it != mComposerFbs.end(); ++it) {
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, plane, presentZorder, blankFlag);
                    bDumpPlane = false;
                } else {
                    dumpComposedFb(*it);
                }
            }
            /* Set fb instead of composer output. */
            fb = composerOutput;
        } else  if (fb->mCompositionType == MESON_COMPOSITION_DI) {
            bool bDumpPlane = true;
            bool bDoDiCompose = false;
            for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); ++it) {
                if ((*it)->isVtBuffer())
                    bHaveVTBuffer = bDoDiCompose = true;

                MESON_LOGV("meet composer pair %d", (*it)->mCompositionType);
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, plane, presentZorder, blankFlag);
                    bDumpPlane = false;
                } else {
                    dumpComposedFb(*it);
                }
            }
            /* make sure SF donot refresh VtLayer and VT only refresh VtLayer*/
            if (!bDoDiCompose)
                mDiComposer->start(m4Mosaic ? 0 : mVideoPlaneNum - 1);
        } else {
            dumpFbAndPlane(fb, plane, presentZorder, blankFlag);
        }

        if (fb->isVtBuffer()) {
            bHaveVTBuffer = true;

            if (fb->interruptVtProcess(blankFlag)) {
                if (mVideoProcessorsMgr.get())
                    mVideoProcessorsMgr->resetProcessors(fb);

                if (blankFlag == BLANK_FOR_NO_CONTENT)
                    MESON_LOGV("%s, layerId(%" PRIu64 ") will blank plane",
                            __func__, fb->mId);

                plane->setPlane(fb, presentZorder, blankFlag);

                if (blankFlag != BLANK_FOR_NO_CONTENT)
                    fb->freeSolidColorBuffer();

                continue;
            }
        } else {
            bool videoPcrMgr = false;
            if (mVideoProcessorsMgr.get() && (fb->mCompositionType != MESON_COMPOSITION_DI))
                videoPcrMgr = mVideoProcessorsMgr->runProcessors(fb, plane,
                        presentZorder, blankFlag);

            /* make sure SF donot refresh VtLayer
             * and VT thread only refresh VtLayer when layer have a valid buffer
             */
            if (!videoPcrMgr) {
                ret = plane->setPlane(fb, presentZorder, blankFlag);
            } else {
                // have processors and return success
                ret = 0;
            }

            fb->clearFbHandleFlag();
            if (ret != 0) {
                MESON_LOGE("%s setPlane failed", plane->getName());
                setPlaneSuccess = false;
            }
        }
    }

    /* Blank un-used plane. */
    auto planeIt = mHwcVideoPlanes.begin();
    for (; planeIt != mHwcVideoPlanes.end(); ++planeIt) {
        mOtherPlanes.push_back(*planeIt);
    }
    planeIt = mOsdPlanes.begin();
    for (; planeIt != mOsdPlanes.end(); ++planeIt) {
        mOtherPlanes.push_back(*planeIt);
    }

    planeIt = mOtherPlanes.begin();
    for (; planeIt != mOtherPlanes.end(); ++planeIt) {
        dumpUnusedPlane(*planeIt, BLANK_FOR_NO_CONTENT);

        if (bHaveVTBuffer && ((*planeIt)->getType() == HWC_VIDEO_PLANE)) {
            /* need disable Video Plane in VideoTunnel Thread
             * when have videotunnel buffers */
            continue;
        }
        (*planeIt)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
    }

    if (!setPlaneSuccess)
        return HWC2_ERROR_NO_RESOURCES;

    if (mDisplayRefFb.get()) {
        drm_rect_t dispFrame = mDisplayRefFb->getDisplayFrame();
        if (IS_FB_COMPOSED(mDisplayRefFb)) {
            if (composerOutput.get()) {
                mDisplayRefFb = composerOutput;
                dispFrame = mDisplayRefFb->getDisplayFrame();
            } else {
                MESON_LOGE("Output of client composer is NULL!");
            }
            mOsdDisplayFrame.crtc_display_x = dispFrame.left;
            mOsdDisplayFrame.crtc_display_y = dispFrame.top;
        }

        mOsdDisplayFrame.framebuffer_w = mDisplayRefFb->mSourceCrop.right -
            mDisplayRefFb->mSourceCrop.left;
        mOsdDisplayFrame.framebuffer_h = mDisplayRefFb->mSourceCrop.bottom -
            mDisplayRefFb->mSourceCrop.top;
        mOsdDisplayFrame.crtc_display_w = dispFrame.right - dispFrame.left;
        mOsdDisplayFrame.crtc_display_h = dispFrame.bottom - dispFrame.top;
    }

    mCrtc->setDisplayFrame(mOsdDisplayFrame);
    return 0;
}

/* Commit DisplayPair to display. */
int MultiplanesWithHRComposition::commitTunnelVideo() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> lock(mMutex);
    int ret = -1;

    if (mCrtc == nullptr) {
        MESON_LOGE("MultiplanesWithHRComposition::%s commit but no crtc!",
                __func__);
        return HWC2_ERROR_NO_RESOURCES;
    }

    for (auto displayIt = mDisplayPairs.begin(); displayIt != mDisplayPairs.end(); ++displayIt) {
        uint32_t presentZorder = displayIt->presentZorder;
        std::shared_ptr<DrmFramebuffer> fb = displayIt->fb;
        std::shared_ptr<HwDisplayPlane> plane = displayIt->plane;
        drm_plane_blank_t blankFlag = (mHideSecureLayer && fb->mSecure) ?
            BLANK_FOR_SECURE_CONTENT : UNBLANK;

        if (!fb->isVtBuffer() && fb->mCompositionType != MESON_COMPOSITION_DI)
            continue;

        if (fb->mCompositionType == MESON_COMPOSITION_DI) {
            bool doDiCompose = false;
            auto it = mDIComposerFbs.begin();
            for (; it != mDIComposerFbs.end(); ++it) {
                if ((*it)->isVtBuffer())
                    doDiCompose = true;
            }

            if (doDiCompose)
                mDiComposer->start(m4Mosaic ? 0 : mVideoPlaneNum - 1);
            continue;
        }

        if (fb->interruptVtProcess(blankFlag)) {
            if (mVideoProcessorsMgr.get())
                mVideoProcessorsMgr->resetProcessors(fb);

            if (blankFlag == BLANK_FOR_NO_CONTENT)
                MESON_LOGV("%s, layerId(%" PRIu64 ") will blank plane",
                        __func__, fb->mId);

            plane->setPlane(fb, presentZorder, blankFlag);

            if (blankFlag != BLANK_FOR_NO_CONTENT)
                fb->freeSolidColorBuffer();

            continue;
        }

        bool videoPcrMgr = false;
        if (mVideoProcessorsMgr.get())
            videoPcrMgr = mVideoProcessorsMgr->runProcessors(fb, plane,
                    presentZorder, blankFlag);

        if (!videoPcrMgr) {
            ret = plane->setPlane(fb, presentZorder, blankFlag);
            fb->clearFbHandleFlag();

            if (ret != 0) {
                MESON_LOGE("%s setPlane failed", plane->getName());
                return ret;
            }
        }
    }

    /* disable no used video plane*/
    auto planeIt = mHwcVideoPlanes.begin();
    for (; planeIt != mHwcVideoPlanes.end(); ++planeIt) {
        (*planeIt)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
    }

    return 0;
}
void MultiplanesWithHRComposition::dump(String8 & dumpstr) {
    std::lock_guard<std::mutex> lock(mMutex);
    ICompositionStrategy::dump(dumpstr);
    dumpstr.appendFormat("BaseScaleInfo (%dx%d->%dx%d, %dx%d) \n",
        mOsdDisplayFrame.framebuffer_w, mOsdDisplayFrame.framebuffer_h,
        mOsdDisplayFrame.crtc_display_x, mOsdDisplayFrame.crtc_display_y,
        mOsdDisplayFrame.crtc_display_w, mOsdDisplayFrame.crtc_display_h);
}
