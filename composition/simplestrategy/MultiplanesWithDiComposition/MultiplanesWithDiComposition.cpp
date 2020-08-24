/*
* Copyright (c) 2018 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description:
*/

#define LOG_NDEBUG 1

#include "MultiplanesWithDiComposition.h"
#include <DrmTypes.h>
#include <MesonLog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "HwcVideoPlane.h"
#include "am_gralloc_ext.h"
#include <DebugHelper.h>

#define OSD_PLANE_NUM_MAX         3  // Maximum osd planes of support
#define OSD_OUTPUT_ONE_CHANNEL         1
#define OSD_PLANE_DIN_ZERO             0    // din0: osd fb input
#define OSD_PLANE_DIN_ONE              1    // din1: osd fb input
#define OSD_PLANE_DIN_TWO              2    // din2: osd fb input
#define VIDEO_PLANE_DIN_ONE            3    // video1: video fb input
#define VIDEO_PLANE_DIN_TWO            4    // video2: video fb input

#define OSD_FB_BEGIN_ZORDER            65   // osd zorder: 65 - 128
#define TOP_VIDEO_FB_BEGIN_ZORDER      129  // top video zorder: 129 - 192
#define BOTTOM_VIDEO_FB_BEGIN_ZORDER   1    // bottom video zorder: 1 - 64

#define OSD_SCALER_INPUT_MAX_WIDTH (1920)
#define OSD_SCALER_INPUT_MAX_HEIGH (1080)
#define OSD_SCALER_INPUT_FACTOR (3.0)
#define OSD_SCALER_INPUT_MARGIN (1.1)

#define IS_FB_COMPOSED(fb) \
    (fb->mZorder >= mMinComposerZorder && fb->mZorder <= mMaxComposerZorder)

#define UVM_DEV_PATH "/dev/uvm"
#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, struct uvm_fd_data)

struct uvm_fd_data {
    int fd;
    int commit_display;
};

/* Constructor function */
MultiplanesWithDiComposition::MultiplanesWithDiComposition() {
    mUVMFd = open(UVM_DEV_PATH, O_RDONLY | O_CLOEXEC);
    if (mUVMFd < 0) {
        MESON_LOGE("open uvm device error");
    }
}

/* Deconstructor function */
MultiplanesWithDiComposition::~MultiplanesWithDiComposition() {
    if (mUVMFd >= 0)
        close(mUVMFd);
}

/* Clean FrameBuffer, Composer and Plane. */
void MultiplanesWithDiComposition::init() {
    /* Reset Flags */
    mHDRMode             = false;
    mHideSecureLayer     = false;
    mForceClientComposer = false;
    mHaveClient          = false;
    mInsideVideoFbsFlag  = false;

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
    mHwcVideoInputFbs[0].clear();
    mHwcVideoInputFbs[1].clear();
    mDisplayPairs.clear();

    mMinComposerZorder = INVALID_ZORDER;
    mMaxComposerZorder = INVALID_ZORDER;
    mMinVideoZorder    = INVALID_ZORDER;
    mMaxVideoZorder    = INVALID_ZORDER;

    mDumpStr.clear();
}

int MultiplanesWithDiComposition::allocateDiOutputFb(
    std::shared_ptr<DrmFramebuffer> & fb,
    uint32_t z) {
    /*now output fb is fake, allocate once create.*/
    fb = std::make_shared<DrmFramebuffer>();
    fb->mFbType = DRM_FB_DI_COMPOSE_OUTPUT;
    fb->mCompositionType = MESON_COMPOSITION_DI;
    fb->mZorder = z;
    return 0;
}

int MultiplanesWithDiComposition::processVideoFbs() {
    std::vector<std::shared_ptr<DrmFramebuffer>> sidebandFbs;
    std::shared_ptr<DrmFramebuffer> fb;
    std::shared_ptr<DrmFramebuffer> topFb, bottomFb;

    uint32_t minVideoZ = -1, maxVideoZ = -1;
    int videoFbNum = 0;
    for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
        bool bSideband = false;
        fb = fbIt->second;
        switch (fb->mFbType) {
            case DRM_FB_VIDEO_OVERLAY:
            case DRM_FB_VIDEO_SIDEBAND:
            case DRM_FB_VIDEO_SIDEBAND_TV:
            case DRM_FB_VIDEO_TUNNEL_SIDEBAND:
                bSideband = true;
                [[clang::fallthrough]];
            case DRM_FB_VIDEO_OMX_V4L:
            case DRM_FB_VIDEO_OMX2_V4L2:
            case DRM_FB_VIDEO_DMABUF:
            case DRM_FB_VIDEO_TUNNEL_SIDEBAND_SECOND:
                if (bSideband) {
                    sidebandFbs.push_back(fb);
                } else {
                    mDIComposerFbs.push_back(fb);
                }
                videoFbNum++;
                if (minVideoZ == INVALID_ZORDER || fb->mZorder < minVideoZ) {
                    minVideoZ = fb->mZorder;
                    bottomFb = fb;
                }
                if (maxVideoZ == INVALID_ZORDER || fb->mZorder > maxVideoZ) {
                    maxVideoZ = fb->mZorder;
                    topFb = fb;
                }
                break;
            default:
                break;
        };
    }

    if (videoFbNum == 0)
        return 0;

    /*
    * Composition: only 1 + N mode now, and most check are hardcode.
    * For vd1:
    * 1. sideband.
    * 2. special video types: AFBC/DV/HDR/HDR10/HLG/SECURE/DIPOST.
    * 3. no overlap & biggest one.
    * For vd2:
    * 1. compose always happened on vd2.
    * 2. vd2 compose always output fullscreen buffer.
    * 3. vd1 should on top of vd2 when have video composed.
    */

    /*------pick fb to vd1------*/
    /*TODO: only one sideband supported now. */
    std::shared_ptr<DrmFramebuffer> vd1Fb;
    if (sidebandFbs.size() > 0) {
        auto it = sidebandFbs.begin();
        /*sideband always push to video plane 0*/
        vd1Fb = *it;
        it ++;
        for (; it != sidebandFbs.end(); it ++) {
            MESON_LOGD("too many sideband, skip.");
            (*it)->mCompositionType = MESON_COMPOSITION_DUMMY;
            videoFbNum --;
        }
        MESON_LOGV("get vd1fb from sideband %d - %d",
            vd1Fb->mZorder, vd1Fb->mFbType);
    }

    if (!vd1Fb) {
        buffer_handle_t buf;
        int video_type;
        int videoTypes2Vd1 = AM_VIDEO_DI_POST | AM_VIDEO_SECURE |
                AM_VIDEO_DV | AM_VIDEO_4K | AM_VIDEO_AFBC |
                AM_VIDEO_HDR | AM_VIDEO_HDR10_PLUS | AM_VIDEO_HLG;
        for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); it++) {
            buf = (*it)->mBufferHandle;
            if (am_gralloc_get_omx_video_type(buf, &video_type) == 0 &&
                (video_type & videoTypes2Vd1) != 0) {
                vd1Fb = *it;
                mDIComposerFbs.erase(it);
                MESON_LOGV("get vd1fb from special video %d-%d",
                    vd1Fb->mZorder, vd1Fb->mFbType);
                break;
            }
        }
    }

    /* find the biggest window and it can't overlap with other window.
    * if no success, find one that can't overlap to others and post it
    * to video_composer.0, post others to video_composer.1
    */
    if (!vd1Fb) {
        drm_rect_t dispFrame, dispFrame1;
        bool is_overlap;
        std::shared_ptr<DrmFramebuffer> fb, fb1;
        std::vector<std::shared_ptr<DrmFramebuffer>> no_overlap_fbs;

        for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); it++) {
            fb = *it;
            is_overlap = false;

            /* post DRM_FB_VIDEO_TUNNEL_SIDEBAND_SECOND type buf to video_composer.1 */
            if (fb->mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND_SECOND)
                continue;

            /* find one that did not overlap with others */
            dispFrame = fb->mDisplayFrame;
            for (auto it1 = mDIComposerFbs.begin(); it1 != mDIComposerFbs.end(); it1++) {
                fb1 = *it1;
                dispFrame1 = fb1->mDisplayFrame;
                if (fb == fb1)
                    continue;
                if (std::max(0, std::min(dispFrame.right, dispFrame1.right) -
                    std::max(dispFrame.left, dispFrame1.left)) *
                    std::max(0, std::min(dispFrame.bottom, dispFrame1.bottom) -
                    std::max(dispFrame.top, dispFrame1.top))) {
                    is_overlap = true;
                    break;
                }
            }

            if (!is_overlap)
                no_overlap_fbs.push_back(fb);
        }

        if (no_overlap_fbs.size() > 0) {
            vd1Fb = *(no_overlap_fbs.begin());
        } else {
            /*all layer have overlap, pick from top/bottom bigger layer. */
            MESON_ASSERT(topFb != NULL && bottomFb != NULL && topFb != bottomFb,
                    "top or bottom (%p vs %p) is not valid.", topFb.get(), bottomFb.get());
            int topregion = 0, bottomregion = 0;
            topregion = (topFb->mDisplayFrame.right - topFb->mDisplayFrame.left) *
                (topFb->mDisplayFrame.bottom - topFb->mDisplayFrame.top);
            bottomregion = (bottomFb->mDisplayFrame.right - bottomFb->mDisplayFrame.left) *
                (bottomFb->mDisplayFrame.bottom - bottomFb->mDisplayFrame.top);
            if (topregion > bottomregion)
                vd1Fb = topFb;
            else
                vd1Fb = bottomFb;
        }

        /*remove vd1Fb from list*/
        for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); it++) {
            if (*it == vd1Fb) {
                mDIComposerFbs.erase(it);
                break;
            }
        }

        MESON_LOGV("get vd1fb from large/nooverlap %d", vd1Fb->mFbType);
    }

    MESON_ASSERT(vd1Fb != NULL, "vd1 should always have buf.");
    mHwcVideoInputFbs[0].push_back(vd1Fb);

    /*------push remaining fbs to vd2------*/
    if (mDIComposerFbs.size() > 0) {
        mHwcVideoInputFbs[1] = mDIComposerFbs;
    }

    /*-----set buffer to displaypair------*/
    bool bVideoCompose = mHwcVideoInputFbs[1].size() > 1 ? true : false;

    bool bFixVideoZ = false;
    if (mHwcVideoInputFbs[1].size() > 1 &&
        (mHwcVideoPlanes[1]->getCapabilities() & PLANE_SUPPORT_ALPHA) == 0) {
        /*for no alpha video layer, force vd1 top, vd2 bottom.*/
        bFixVideoZ = true;
    }

    if (mHwcVideoInputFbs[1].size() > 0) {
        uint32_t videoZ = -1;

        if (bFixVideoZ) {
            videoZ = minVideoZ;
        } else {
            fb = *mHwcVideoInputFbs[1].begin();
            videoZ = fb->mZorder;
        }

        if (bVideoCompose) {
            for (auto it = mHwcVideoInputFbs[1].begin(); it != mHwcVideoInputFbs[1].end(); it++) {
                (*it)->mCompositionType = MESON_COMPOSITION_DI;
            }
            /*set dicomposer and get output video.*/
            std::vector<std::shared_ptr<DrmFramebuffer>> nofbs;
            mDiComposer->prepare();
            mDiComposer->addInputs(mDIComposerFbs, nofbs, 1);
            /*TODO: workaround to pass zorder to composer.*/
            hwc_region_t damage;
            allocateDiOutputFb(fb, (*mHwcVideoInputFbs[1].begin())->mZorder);
            mDiComposer->setOutput(fb, damage, 1);
        } else {
            fb = *mHwcVideoInputFbs[1].begin();
            fb->mCompositionType = MESON_COMPOSITION_PLANE_HWCVIDEO;
        }

        //MESON_LOGD("set vd2 %d, %d", mHwcVideoInputFbs[1].size(), fb->mFbType);
        mDisplayPairs.push_back(
            DisplayPair{VIDEO_PLANE_DIN_TWO,  videoZ, fb, mHwcVideoPlanes[1]});
        mHwcVideoPlanes.erase(mHwcVideoPlanes.begin() + 1);
    }

    if (mHwcVideoInputFbs[0].size() > 0) {
        uint32_t videoZ = -1;
        fb = *mHwcVideoInputFbs[0].begin();
        fb->mCompositionType = MESON_COMPOSITION_PLANE_HWCVIDEO;

        if (bVideoCompose || bFixVideoZ) {
            videoZ = maxVideoZ;
        } else {
            videoZ = fb->mZorder;
        }

        //MESON_LOGD("set vd1 %d", fb->mFbType);
        mDisplayPairs.push_back(
            DisplayPair{VIDEO_PLANE_DIN_ONE, videoZ, fb, mHwcVideoPlanes[0]});
        mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
    }

    return 0;
}

/* Apply flag with secure and forceClient Fbs. */
int MultiplanesWithDiComposition::applyCompositionFlags() {
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
int MultiplanesWithDiComposition::handleUVM() {
    if (mUVMFd > 0) {
        std::shared_ptr<DrmFramebuffer> fb;
        auto fbIt = mFramebuffers.begin();
        struct uvm_fd_data fd_data;

        for (; fbIt != mFramebuffers.end(); ++fbIt) {
            fb = fbIt->second;

            if (am_gralloc_is_uvm_dma_buffer(fb->mBufferHandle)) {
                fd_data.fd = am_gralloc_get_buffer_fd(fb->mBufferHandle);
                fd_data.commit_display = 1;

                if (ioctl(mUVMFd, UVM_IOC_SET_FD, &fd_data) != 0) {
                    MESON_LOGE("setUVM fd data ioctl error %s", strerror(errno));
                    return -1;
                }
            }
        }
    }
    return 0;
}

int MultiplanesWithDiComposition::processGfxFbs() {
    /* Remove dummy and video Fbs for later osd composition.
     * Pickout OSD Fbs.
     * Save client flag.
     */
    pickoutOsdFbs();

    if (!mInsideVideoFbsFlag) {
        handleOsdComposition();
    } else {
        handleOsdCompostionWithVideo();
    }

    /* record overlayFbs and start to compose */
    if (mComposer.get()) {
        mComposer->prepare();
        mComposer->addInputs(mComposerFbs, mOverlayFbs);
    }

    return 0;
}

/* Delete dummy and video Fbs, then pickout OSD Fbs. */
int MultiplanesWithDiComposition::pickoutOsdFbs() {
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
                MESON_LOGE("Unknown compostition type(%d)", fb->mCompositionType);
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
int MultiplanesWithDiComposition::countComposerFbs(int &belowClientNum, int &upClientNum, int &insideClientNum) {
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

int MultiplanesWithDiComposition::confirmComposerRange() {
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

int32_t MultiplanesWithDiComposition::compareFbScale(
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
    int heighCompare = aDisplayHeight *bSrcHeight - bDisplayHeight * aSrcHeight;
    if (widthCompare == 0 && heighCompare == 0)
        return 0;
    else if (widthCompare > 0 && heighCompare > 0)
        return 1;
    else {
        //MESON_LOGW("compareFbScale failed %d ,%d, %d, %d",
            //widthCompare, heighCompare,
            //bDisplayWidth, bDisplayHeight);
        return -1;
    }
}

/* Set DisplayPairs between UI(OSD) Fbs with plane. */
int MultiplanesWithDiComposition::setOsdFbs2PlanePairs() {
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

    bool bDin0 = false;
    bool bDin1 = false;
    bool bDin2 = false;

    /*baseFb always post to din0*/
    mDisplayPairs.push_back(
        DisplayPair{OSD_PLANE_DIN_ZERO, mDisplayRefFb->mZorder, mDisplayRefFb, mOsdPlanes[OSD_PLANE_DIN_ZERO]});
    bDin0 = true;
    /* Not composed fb, set to osd composition. */
    if (mDisplayRefFb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
        mDisplayRefFb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;

    for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
        std::shared_ptr<DrmFramebuffer> fb = fbIt->second;
        if (fb == mDisplayRefFb)
            continue;
        /* fb is unscaled or is represent of composer output,
         * we pick primary display.
         */
        /* Pick any available plane */
        if (bDin1 == false) {
            mDisplayPairs.push_back(
                DisplayPair{OSD_PLANE_DIN_ONE, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_ONE]});
            bDin1 = true;
        } else {
            mDisplayPairs.push_back(
                DisplayPair{OSD_PLANE_DIN_TWO, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_TWO]});
            bDin2 = true;
        }

        /* Not composed fb, set to osd composition. */
        if (fb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
            fb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
    }

    /* removed used planes from mOsdPlanes */
    uint32_t removePlanes = 0;
    if (bDin0)
        removePlanes ++;
    if (bDin1)
        removePlanes ++;
    if (bDin2)
        removePlanes ++;

    for (uint32_t i = 0;i < removePlanes; i++) {
        mOsdPlanes.erase(mOsdPlanes.begin());
    }

    return 0;
}

/* Select composer */
int MultiplanesWithDiComposition::selectComposer() {
    if (mComposerFbs.size() == 0)
        return 0;

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

/* Find out which fb need to compse and push DisplayPair */
int MultiplanesWithDiComposition::fillComposerFbs() {
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
void MultiplanesWithDiComposition::handleOverlayVideoZorder() {
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
void MultiplanesWithDiComposition::handleVPUScaleLimit() {
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

        float expHeight = (fb->mSourceCrop.bottom - fb->mSourceCrop.top) * scaleLimit;
        float dispHeight = (fb->mDisplayFrame.bottom -fb->mDisplayFrame.top) * mScaleValue;

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
void MultiplanesWithDiComposition::handleVPULimit(bool video) {
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
    int32_t compostionTargetW = 0, compostionTargetH = 0;
    for (auto it = mFramebuffers.begin(); it != mFramebuffers.end(); it++) {
        std::shared_ptr<DrmFramebuffer> fb = it->second;
        if (minXOffset == -1 || minXOffset > fb->mDisplayFrame.left)
            minXOffset = fb->mDisplayFrame.left;
        if (minYOffset == -1 || minYOffset > fb->mDisplayFrame.top)
            minYOffset = fb->mDisplayFrame.top;

        if (fb->mDisplayFrame.right > compostionTargetW)
            compostionTargetW = fb->mDisplayFrame.right;
        if (fb->mDisplayFrame.bottom > compostionTargetH)
            compostionTargetH = fb->mDisplayFrame.bottom;
    }

    /*choose base fb, the scale is smallest bigger.*/
    compostionTargetW = compostionTargetW - minXOffset;
    compostionTargetH = compostionTargetH - minYOffset;

    drm_rect_t scaleInput = {0, 0,
        OSD_SCALER_INPUT_MAX_WIDTH, OSD_SCALER_INPUT_MAX_HEIGH};
    drm_rect_t scaleOutput = {0, 0, compostionTargetW, compostionTargetH};

    /*choose the scale > targetW/MAX_INPUT*/
    for (auto it = mFramebuffers.begin(); it != mFramebuffers.end(); it++) {
        std::shared_ptr<DrmFramebuffer> fb = it->second;
        int32_t ret = compareFbScale(fb->mSourceCrop, fb->mDisplayFrame, scaleInput, scaleOutput);
        if (0 == ret) {
            mDisplayRefFb = fb;
            break;
        } else if (1 == ret) {
            if (!mDisplayRefFb)
                mDisplayRefFb = fb;
            else {
                if (-1 == compareFbScale(fb->mSourceCrop, fb->mDisplayFrame,
                    mDisplayRefFb->mSourceCrop, mDisplayRefFb->mDisplayFrame) ) {
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

void MultiplanesWithDiComposition::handleDispayLayerZorder() {
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
int MultiplanesWithDiComposition::handleOsdComposition() {
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

int MultiplanesWithDiComposition::handleOsdCompostionWithVideo() {
    std::shared_ptr<DrmFramebuffer> fb;

    /* STEP 1: handle two video fbs. */
    if (mOverlayFbs.size() > 1) {
        MESON_ASSERT(mOverlayFbs.size() <= 2, "Only support 2 video layers now.");
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

        /* Judge whether double video are neighbour or not */
        bool bNeighbourVideo = true;
        auto fbIt = mFramebuffers.lower_bound(mMinVideoZorder);
        for (; fbIt != mFramebuffers.end(); ++ fbIt) {
            if (fbIt->second->mZorder > mMinVideoZorder && fbIt->second->mZorder < mMaxVideoZorder) {
                bNeighbourVideo = false;
                break;
            }
        }

        if (!bNeighbourVideo) {
            /* CASE VIDEO_1_1: two video fbs are not neighbour. */
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
            /* CASE VIDEO_1_2: two video fbs are neighbour, treat as one video. */
            /* goto case VIDEO_2 */
        }
    }

    /* STEP 2: now only one video fb or two neighbour video fbs.
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
void MultiplanesWithDiComposition::setup(
    std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
    std::vector<std::shared_ptr<IComposer>> & composers,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
    std::shared_ptr<HwDisplayCrtc> & crtc,
    uint32_t reqFlag,
    float scaleValue) {
    init();

    mCompositionFlag = reqFlag;
    mScaleValue = scaleValue;
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

    mCrtc = crtc;

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
                MESON_ASSERT(mOsdPlanes.size() <= OSD_PLANE_NUM_MAX,
                    "More than three osd planes !!");
                break;

            case HWC_VIDEO_PLANE:
                mHwcVideoPlanes.push_back(plane);
                break;

            default:
                mOtherPlanes.push_back(plane);
                break;
        }
    }
}

/* Decide to choose whcih Fbs and how to build OsdFbs2Plane pairs. */
int MultiplanesWithDiComposition::decideComposition() {
    int ret = 0;
    if (mFramebuffers.empty()) {
        MESON_LOGV("No layers to compose, exit.");
        return ret;
    }

    /* handle uvm */
    handleUVM();

    /* handle VIDEO fbs. */
    processVideoFbs();

    /* handle HDR mode, hide secure layer, and force client. */
    applyCompositionFlags();
    /* handle graphic fbs.*/
    processGfxFbs();

    return ret;
}

/* Commit DisplayPair to display. */
int MultiplanesWithDiComposition::commit() {
    /* replace composer output with din0 Pair. */
    std::shared_ptr<DrmFramebuffer> composerOutput;
    if (mComposer.get()) {
        mComposer->start();
        composerOutput = mComposer->getOutput();
    }

    handleOverlayVideoZorder();
    handleDispayLayerZorder();

    for (auto displayIt = mDisplayPairs.begin(); displayIt != mDisplayPairs.end(); ++displayIt) {
        uint32_t presentZorder = displayIt->presentZorder;
        std::shared_ptr<DrmFramebuffer> fb = displayIt->fb;
        std::shared_ptr<HwDisplayPlane> plane = displayIt->plane;
        int blankFlag = (mHideSecureLayer && fb->mSecure) ?
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
            mDiComposer->start(1);
            bool bDumpPlane = true;
            for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); ++it) {
                MESON_LOGE("meet composer pair %d", (*it)->mCompositionType);
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, plane, presentZorder, blankFlag);
                    bDumpPlane = false;
                } else {
                    dumpComposedFb(*it);
                }
            }
        } else {
            dumpFbAndPlane(fb, plane, presentZorder, blankFlag);
        }

        /* Set display info. */
        plane->setPlane(fb, presentZorder, blankFlag);
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
        (*planeIt)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
        dumpUnusedPlane(*planeIt, BLANK_FOR_NO_CONTENT);
    }

    /*set crtc info.*/
    if (mHDRMode)
        mCrtc->setOsdChannels(1);

    if (mDisplayRefFb.get()) {
        if (IS_FB_COMPOSED(mDisplayRefFb)) {
            if (composerOutput.get()) {
                mDisplayRefFb = composerOutput;
            } else {
                MESON_LOGE("Output of cient composer is NULL!");
            }
            mOsdDisplayFrame.crtc_display_x = mDisplayRefFb->mDisplayFrame.left;
            mOsdDisplayFrame.crtc_display_y = mDisplayRefFb->mDisplayFrame.top;
        }

        mOsdDisplayFrame.framebuffer_w = mDisplayRefFb->mSourceCrop.right -
            mDisplayRefFb->mSourceCrop.left;
        mOsdDisplayFrame.framebuffer_h = mDisplayRefFb->mSourceCrop.bottom -
            mDisplayRefFb->mSourceCrop.top;
        mOsdDisplayFrame.crtc_display_w = mDisplayRefFb->mDisplayFrame.right -
            mDisplayRefFb->mDisplayFrame.left;
        mOsdDisplayFrame.crtc_display_h = mDisplayRefFb->mDisplayFrame.bottom -
            mDisplayRefFb->mDisplayFrame.top;
    }

    mCrtc->setDisplayFrame(mOsdDisplayFrame);
    return 0;
}

