/*
* Copyright (c) 2018 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description:
*/

#include "MultiplanesWithDiComposition.h"
#include <DrmTypes.h>
#include <MesonLog.h>
#include "HwcVideoPlane.h"



#define LEGACY_VIDEO_MODE_SWITCH       0    // Only use in current device (Only one legacy video plane)
#define OSD_OUTPUT_ONE_CHANNEL         1
#define OSD_PLANE_DIN_ZERO             0    // din0: osd fb input
#define OSD_PLANE_DIN_ONE              1    // din1: osd fb input
#define OSD_PLANE_DIN_TWO              2    // din2: osd fb input
#define VIDEO_PLANE_DIN_ONE            3    // video1: video fb input
#define VIDEO_PLANE_DIN_TWO            4    // video2: video fb input
#define VIDEO_DI_IN0                   5    // DI input 0
#define VIDEO_DI_IN1                   6    // DI input 1
#define VIDEO_DI_IN2                   7    // DI input 2
#define VIDEO_DI_IN3                   8    // DI input 3

#define OSD_FB_BEGIN_ZORDER            65   // osd zorder: 65 - 128
#define TOP_VIDEO_FB_BEGIN_ZORDER      129  // top video zorder: 129 - 192
#define BOTTOM_VIDEO_FB_BEGIN_ZORDER   1    // bottom video zorder: 1 - 64
#define PIP_VIDEO_DISPLAYFRAME_SIZE    16


#define OSD_SCALER_INPUT_MAX_WIDTH (1920)
#define OSD_SCALER_INPUT_MAX_HEIGH (1080)

#define DI_VIDEO_COMPOSE_MAX 2


#define IS_FB_COMPOSED(fb) \
    (fb->mZorder >= mMinComposerZorder && fb->mZorder <= mMaxComposerZorder)


/* Constructor function */
MultiplanesWithDiComposition::MultiplanesWithDiComposition() {}

/* Deconstructor function */
MultiplanesWithDiComposition::~MultiplanesWithDiComposition() {}

/* Clean FrameBuffer, Composer and Plane. */
void MultiplanesWithDiComposition::init() {
    /* Reset Flags */
    mDiComposer.reset();
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
    mOtherComposers.clear();

    /* Clean Plane */
    mOsdPlanes.clear();
    mHwcVideoPlanes.clear();
    mLegacyVideoPlane.reset();
    mLegacyExtVideoPlane.reset();
    mOtherPlanes.clear();

    /* Clean Composition members */
    mComposer.reset();
    mOverlayFbs.clear();
    mComposerFbs.clear();
    mDIComposerFbs.clear();
    mDisplayPairs.clear();

    mMinComposerZorder = INVALID_ZORDER;
    mMaxComposerZorder = INVALID_ZORDER;
    mMinVideoZorder    = INVALID_ZORDER;
    mMaxVideoZorder    = INVALID_ZORDER;

    mDumpStr.clear();
}

/* Handle VIDEO Fbs and set VideoFbs2Plane pairs.
 * Current VIDEO plane support : 1 LegacyVideoPlane + 1 HwcVideoPlane
 * Future  VIDEO plane support : 2 HwcVideoPlane
 */
int MultiplanesWithDiComposition::handleVideoComposition() {
    meson_compositon_t destComp;
    static struct planeComp {
        drm_fb_type_t srcFb;
        drm_plane_type_t destPlane;
        meson_compositon_t destComp;
    } planeCompPairs [] = {
        {DRM_FB_VIDEO_OVERLAY, LEGACY_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_AMVIDEO},
        {DRM_FB_VIDEO_OMX_PTS, LEGACY_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_AMVIDEO},
        {DRM_FB_VIDEO_SIDEBAND, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_DI_VIDEO},
        {DRM_FB_VIDEO_SIDEBAND_SECOND, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_DI_VIDEO},
        {DRM_FB_VIDEO_OMX_PTS_SECOND, LEGACY_EXT_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_AMVIDEO},
        {DRM_FB_VIDEO_OMX_V4L, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_DI_VIDEO},
        {DRM_FB_VIDEO_OMX2_V4L2, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_DI_VIDEO},
        {DRM_FB_VIDEO_DMABUF, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_DI_VIDEO},
    };
    static int pairSize = sizeof(planeCompPairs) / sizeof(struct planeComp);

    std::shared_ptr<DrmFramebuffer> fb;
    auto fbIt = mFramebuffers.begin();
    for (; fbIt != mFramebuffers.end(); ++fbIt) {
        fb = fbIt->second;
        for (int i = 0; i < pairSize; i++) {
            if (fb->mFbType == planeCompPairs[i].srcFb) {
                uint32_t presentZorder = fb->mZorder;
                destComp = planeCompPairs[i].destComp;
                if (planeCompPairs[i].destPlane == LEGACY_VIDEO_PLANE) {
                    if (mLegacyVideoPlane.get()) {
                        mDisplayPairs.push_back(DisplayPair{VIDEO_PLANE_DIN_ONE, presentZorder, fb, mLegacyVideoPlane});
                        mLegacyVideoPlane.reset();
                    } else {
                        MESON_LOGE("too many layers need LEGACY_VIDEO_PLANE, discard.");
                        destComp = MESON_COMPOSITION_DUMMY;
                    }
                    fb->mCompositionType = destComp;
                    break;
                } else if (planeCompPairs[i].destPlane == LEGACY_EXT_VIDEO_PLANE) {
                    int32_t width = 0, height = 0;
                    width = abs(fb->mDisplayFrame.right - fb->mDisplayFrame.left);
                    height = abs(fb->mDisplayFrame.bottom - fb->mDisplayFrame.top);
                    if (width <= PIP_VIDEO_DISPLAYFRAME_SIZE && height <= PIP_VIDEO_DISPLAYFRAME_SIZE) {
                        destComp = MESON_COMPOSITION_DUMMY;
                    } else if (mLegacyExtVideoPlane.get()) {
                        mDisplayPairs.push_back(DisplayPair{VIDEO_PLANE_DIN_TWO, presentZorder, fb, mLegacyExtVideoPlane});
                        mLegacyExtVideoPlane.reset();
                    } else {
                        MESON_LOGE("too many layers need LEGACY_EXT_VIDEO_PLANE, discard.");
                        destComp = MESON_COMPOSITION_DUMMY;
                    }
                    fb->mCompositionType = destComp;
                    break;
                } else if (planeCompPairs[i].destPlane == HWC_VIDEO_PLANE) {
                    mDIComposerFbs.push_back(fb);
                    MESON_ASSERT(mDIComposerFbs.size() <= MAX_LAYER_COUNT,
                        "More than four DI video layers !!!");
                    fb->mCompositionType = destComp;
                    break;
                } else {
                    MESON_LOGE("Not supported dest plane: %d", planeCompPairs[i].destPlane);
                    break;
                }
            }
        }
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

            case MESON_COMPOSITION_PLANE_AMVIDEO:
            case MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND:
            case MESON_COMPOSITION_PLANE_HWCVIDEO:
            case MESON_COMPOSITION_PLANE_DI_VIDEO:
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

    if (mDIComposerFbs.size() > 0) {
        std::vector<std::shared_ptr<DrmFramebuffer>> diOverlayFbs;
        mDiComposer->addInputs(mDIComposerFbs, diOverlayFbs);
    }

/* Only support one legacy video in current times. */
#if LEGACY_VIDEO_MODE_SWITCH
    if (!mOverlayFbs.empty() && !mFramebuffers.empty()) {
        auto osdFbIt = mFramebuffers.begin();
        uint32_t minOsdFbZorder = osdFbIt->second->mZorder;
        osdFbIt = mFramebuffers.end();
        osdFbIt --;

        /* Current only input one Legacy video fb. */
        std::shared_ptr<DrmFramebuffer> legacyVideoFb = *(mOverlayFbs.begin());
        if (legacyVideoFb->mZorder > minOsdFbZorder) {
            mMinComposerZorder = mFramebuffers.begin()->second->mZorder;
            /* Legacy video is always on the bottom.
             * SO, all fbs below legacyVideo zorder need to compose.
             * Set maxClientZorder = legacyVideoZorder
             */
            if (mMaxComposerZorder == INVALID_ZORDER || legacyVideoFb->mZorder > mMaxComposerZorder) {
                mMaxComposerZorder = legacyVideoFb->mZorder;
            }
        }
    }
#else // If only one legacy video in current times, don't need to check inside video flag.
    /* 1. check mInsideVideoFbsFlag = false
     * 2. for HDR mode, adjust compose range.
     */
    if (!mOverlayFbs.empty() && !mFramebuffers.empty()) {
        auto uiFbIt = mFramebuffers.begin();
        uint32_t uiFbMinZorder = uiFbIt->second->mZorder;
        uiFbIt = mFramebuffers.end();
        uiFbIt --;
        uint32_t uiFbMaxZorder = uiFbIt->second->mZorder;

        for (auto videoFbIt = mOverlayFbs.begin(); videoFbIt != mOverlayFbs.end(); ) {
            std::shared_ptr<DrmFramebuffer> videoFb = *videoFbIt;
             if (videoFb->mZorder >= uiFbMinZorder && videoFb->mZorder <= uiFbMaxZorder) {
                if (mHDRMode) {
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
                videoFbIt = mOverlayFbs.erase(videoFbIt);
            }
        }
    }
#endif

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
Limitation:
1. scale input should smaller than 1080P.
2. din0 should input the base fb.
*/
void MultiplanesWithDiComposition::handleVPULimit(bool video) {
    UNUSED(video);
    //MESON_ASSERT(video == false, "handleVPULimit havenot support video");

    if (mFramebuffers.size() == 0)
        return ;

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
        if (OSD_PLANE == plane->getPlaneType()) {
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
        if (LEGACY_VIDEO_PLANE == plane->getPlaneType() || LEGACY_EXT_VIDEO_PLANE == plane->getPlaneType()) {
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

void MultiplanesWithDiComposition::handleDiComposition()
{
    std::shared_ptr<DrmFramebuffer> fb, fb1, large_fb, sideband_fb;
    uint32_t zorder;
    std::multimap<int, std::shared_ptr<DrmFramebuffer>> video_type_maps;
    std::vector<std::shared_ptr<DrmFramebuffer>> no_overlap_fbs;
    static std::vector<int> video_types {AM_VIDEO_DI_POST, AM_VIDEO_SECURE, AM_VIDEO_DV,
        AM_VIDEO_4K, AM_VIDEO_AFBC, AM_VIDEO_HDR, AM_VIDEO_HDR10_PLUS, AM_VIDEO_HLG};
    buffer_handle_t buf;
    int video_type, ret, region = 0, max_region = 0;

    if (mDIComposerFbs.size() > 0) {
        //mDiComposer->start();
        std::shared_ptr<HwcVideoPlane>  hwcVideoPlane0, hwcVideoPlane1;
        std::shared_ptr<HwDisplayPlane> hwcPlane;
        std::vector<std::shared_ptr<DrmFramebuffer>> composefbs0, composefbs1, commitfbs;
        DiComposerPair diComposerPair;
        drm_rect_t dispFrame, dispFrame1;
        bool is_overlap;
        composefbs0.clear();
        composefbs1.clear();
        hwcVideoPlane0 = std::static_pointer_cast<HwcVideoPlane>(mHwcVideoPlanes[0]);
        hwcVideoPlane1 = std::static_pointer_cast<HwcVideoPlane>(mHwcVideoPlanes[1]);

        for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); it++) {
            fb = *it;
            buf = fb->mBufferHandle;
            is_overlap = false;

            if (!large_fb)
                large_fb = fb;

            if (fb->mFbType == DRM_FB_VIDEO_SIDEBAND ||
                fb->mFbType == DRM_FB_VIDEO_SIDEBAND_SECOND) {
                sideband_fb = fb;
                continue;
            }

            ret = am_gralloc_get_omx_video_type(buf, &video_type);
            if (ret)
                continue;

            for (auto type_it = video_types.begin(); type_it != video_types.end(); type_it++) {
                if ((video_type & *type_it) == *type_it) {
                    video_type_maps.insert(std::make_pair(*type_it, fb));
                }
            }

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

            /* find the largest the frame */
            region = (dispFrame.right - dispFrame.left) * (dispFrame.bottom - dispFrame.top);
            if ((dispFrame.right - dispFrame.left) * (dispFrame.bottom - dispFrame.top) > max_region) {
                large_fb = fb;
                max_region = region;
            }
        }

        for (auto type_it = video_types.begin(); type_it != video_types.end(); type_it++) {
            if (video_type_maps.count(*type_it) > 0) {
                auto map_it = video_type_maps.lower_bound(*type_it);
                composefbs0.push_back(map_it->second);
                break;
            }
        }

        if (sideband_fb)
            composefbs0.insert(composefbs0.begin(), sideband_fb);

        if (composefbs0.size() == 0) {
            /* find the biggest window and it can't overlap with other window.
             * if no success, find one that can't overlap to others and post it
             * to video_composer.0, post others to video_composer.1
             */
            if (no_overlap_fbs.size() > 0)
                composefbs0.push_back(*no_overlap_fbs.begin());
            else if (large_fb)
                composefbs0.push_back(large_fb);
        }
        for (auto it = mDIComposerFbs.begin(); it != mDIComposerFbs.end(); ) {
            if ((composefbs0.size() > 0) && (*it == *composefbs0.begin())) {
                mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
                commitfbs.push_back(*it);
                diComposerPair.num_composefbs = commitfbs.size();
                diComposerPair.composefbs = commitfbs;
                diComposerPair.plane = hwcVideoPlane0;

                fb = *composefbs0.begin();
                zorder = (fb->mZorder == 0 ? 1 : fb->mZorder);
            } else {
                composefbs1.push_back(*it);
            }
            it = mDIComposerFbs.erase(it);
        }

        bool bDumpPlane = true;
        if (commitfbs.size() > 0) {
            fb = *commitfbs.begin();
            if (composefbs1.size() <= 1)
                diComposerPair.zorder = fb->mZorder;
            else if (composefbs1.size() > 1)
                diComposerPair.zorder = zorder;

            MESON_LOGE("commit %d layers to video.composer0.\n", diComposerPair.num_composefbs);
            hwcVideoPlane0->setComposePlane(&diComposerPair, UNBLANK);

            hwcPlane = hwcVideoPlane0;
            for (auto it = commitfbs.begin(); it != commitfbs.end(); it++) {
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, hwcPlane, diComposerPair.zorder, UNBLANK);
                    bDumpPlane = false;
                } else
                    dumpComposedFb(*it);
            }
        }

        if (composefbs1.size() > 0 ) {
            mHwcVideoPlanes.erase(mHwcVideoPlanes.begin());
            diComposerPair.num_composefbs = composefbs1.size();
            diComposerPair.composefbs = composefbs1;
            diComposerPair.plane = hwcVideoPlane1;
            fb = *composefbs1.begin();

            if (composefbs1.size() == 1)
                diComposerPair.zorder = fb->mZorder;
            else if (composefbs1.size() > 1)
                diComposerPair.zorder = 0;

            MESON_LOGE("commit %d layers to video.composer1.\n", diComposerPair.num_composefbs);
            hwcVideoPlane1->setComposePlane(&diComposerPair, UNBLANK);

            hwcPlane = hwcVideoPlane1;
            bDumpPlane = true;
            for (auto it = composefbs1.begin(); it != composefbs1.end(); it++) {
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, hwcPlane, diComposerPair.zorder, UNBLANK);
                    bDumpPlane = false;
                } else
                    dumpComposedFb(*it);
            }
        }
    }
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
    uint32_t reqFlag) {
    init();

    mCompositionFlag = reqFlag;
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
        switch (plane->getPlaneType()) {
            case OSD_PLANE:
                mOsdPlanes.push_back(plane);
                MESON_ASSERT(mOsdPlanes.size() <= OSD_PLANE_NUM_MAX,
                    "More than three osd planes !!");
                break;

            case HWC_VIDEO_PLANE:
                mHwcVideoPlanes.push_back(plane);
                MESON_ASSERT(mHwcVideoPlanes.size() <= DI_VIDEO_COMPOSE_MAX,
                    "More than two DI video composer planes !!!");
                break;

            case LEGACY_VIDEO_PLANE:
                if (mLegacyVideoPlane.get() == NULL) {
                    mLegacyVideoPlane = plane;
                } else {
                    MESON_ASSERT(0, "More than one legacy_video osd plane, discard.");
                    mOtherPlanes.push_back(plane);
                }
                break;

            case LEGACY_EXT_VIDEO_PLANE:
                if (mLegacyExtVideoPlane.get() == NULL) {
                    mLegacyExtVideoPlane = plane;
                } else {
                    MESON_ASSERT(0, "More than one legacy_ext_video osd plane, discard.");
                    mOtherPlanes.push_back(plane);
                }
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

    /* handle VIDEO Fbs. */
    handleVideoComposition();

    /* handle HDR mode, hide secure layer, and force client. */
    applyCompositionFlags();

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

    handleDiComposition();
    handleOverlayVideoZorder();
    handleDispayLayerZorder();

    /* Commit display path. */
    for (auto displayIt = mDisplayPairs.begin(); displayIt != mDisplayPairs.end(); ++displayIt) {
        uint32_t presentZorder = displayIt->presentZorder;
        std::shared_ptr<DrmFramebuffer> fb = displayIt->fb;
        std::shared_ptr<HwDisplayPlane> plane = displayIt->plane;
        int blankFlag = (mHideSecureLayer && fb->mSecure) ?
            BLANK_FOR_SECURE_CONTENT : UNBLANK;

        if (composerOutput.get() &&
            fb->mCompositionType == mComposer->getType()) {
            presentZorder = mMaxComposerZorder + OSD_FB_BEGIN_ZORDER;
            /* Dump composer info. */
            bool bDumpPlane = true;
            auto it = mComposerFbs.begin();
            for (; it != mComposerFbs.end(); ++it) {
                if (bDumpPlane) {
                    dumpFbAndPlane(*it, plane, presentZorder, blankFlag);
                    bDumpPlane = false;
                } else {
                    dumpComposedFb(*it);
                }
            }

            /* Set fb instead of composer output. */
            fb = composerOutput;
        } else {
            dumpFbAndPlane(fb, plane, presentZorder, blankFlag);
        }

        /* Set display info. */
        plane->setPlane(fb, presentZorder, blankFlag);
    }

    /* Blank un-used plane. */
    if (mLegacyVideoPlane.get())
        mOtherPlanes.push_back(mLegacyVideoPlane);
    if (mLegacyExtVideoPlane.get())
        mOtherPlanes.push_back(mLegacyExtVideoPlane);

    auto osdIt = mOsdPlanes.begin();
    for (; osdIt != mOsdPlanes.end(); ++osdIt) {
        mOtherPlanes.push_back(*osdIt);
    }

    auto planeIt = mOtherPlanes.begin();
    for (; planeIt != mOtherPlanes.end(); ++planeIt) {
        (*planeIt)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
        dumpUnusedPlane(*planeIt, BLANK_FOR_NO_CONTENT);
    }

    auto diIt = mHwcVideoPlanes.begin();
    for (; diIt != mHwcVideoPlanes.end(); ++diIt) {
        if ((*diIt).get())
            (*diIt)->setPlane(NULL, HWC_PLANE_FAKE_ZORDER, BLANK_FOR_NO_CONTENT);
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

