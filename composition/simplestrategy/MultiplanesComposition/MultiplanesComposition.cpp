/*
* Copyright (c) 2018 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description:
*/

#include "MultiplanesComposition.h"
#include <DrmTypes.h>
#include <MesonLog.h>

#define LEGACY_VIDEO_MODE_SWITCH  1  // Only use in current device (Only one leagcy video plane)
#define OSD_PLANE_DIN_ZERO        0  // din0: osd fb input
#define OSD_PLANE_DIN_ONE         1  // din1: osd fb input
#define OSD_PLANE_DIN_TWO         2  // din2: osd fb input
#define VIDEO_PLANE_DIN_ONE       3  // video1: video fb input
#define VIDEO_PLANE_DIN_TWO       4  // video2: video fb input
#define OSD_PLANE_NUM_MAX         3  // Maximum osd planes of support


/* Constructor function */
MultiplanesComposition::MultiplanesComposition() {}

/* Deconstructor function */
MultiplanesComposition::~MultiplanesComposition() {}

/* Clean FrameBuffer, Composer and Plane. */
void MultiplanesComposition::init() {
    /* Reset Flags */
    mHDRMode             = false;
    mHideSecureLayer     = false;
    mForceClientComposer = false;
    mHaveClient          = false;
    mInsideVideoFbsFlag  = false;

    /* Clean FrameBuffer */
    mFramebuffers.clear();

    /* Clean Composer */
    mDummyComposer.reset();
    mClientComposer.reset();
    mOtherComposers.clear();

    /* Clean Plane */
    mOsdPlanes.clear();
    mHwcVideoPlane.reset();
    mLegacyVideoPlane.reset();
    mOtherPlanes.clear();

    /* Clean Composition members */
    mComposer.reset();
    mOverlayFbs.clear();
    mComposerFbs.clear();
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
int MultiplanesComposition::handleVideoComposition() {
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
        {DRM_FB_VIDEO_SIDEBAND, LEGACY_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND},
        {DRM_FB_VIDEO_OMX_V4L, HWC_VIDEO_PLANE,
            MESON_COMPOSITION_PLANE_HWCVIDEO},
    };
    static int pairSize = sizeof(planeCompPairs) / sizeof(struct planeComp);

    std::shared_ptr<DrmFramebuffer> fb;
    auto fbIt = mFramebuffers.begin();
    for (; fbIt != mFramebuffers.end(); ++fbIt) {
        fb = fbIt->second;
        for (int i = 0; i < pairSize; i++) {
            if (fb->mFbType == planeCompPairs[i].srcFb) {
                uint32_t presentZorder = fb->mZorder;
                MESON_LOGD("video zorder: %d", fb->mZorder);
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
                } else if (planeCompPairs[i].destPlane == HWC_VIDEO_PLANE) {
                    if (mHwcVideoPlane.get()) {
                        mDisplayPairs.push_back(DisplayPair{VIDEO_PLANE_DIN_TWO, presentZorder, fb, mHwcVideoPlane});
                        mHwcVideoPlane.reset();
                    } else {
                        MESON_LOGE("too many layers need HWC_VIDEO_PLANE, discard.");
                        destComp = MESON_COMPOSITION_DUMMY;
                    }
                    fb->mCompositionType = destComp;
                    break;
                }
            }
        }
    }

    return 0;
}

/* Apply flag with secure and forceClient Fbs. */
int MultiplanesComposition::applyCompositionFlags() {
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
int MultiplanesComposition::pickoutOsdFbs() {
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
                mOverlayFbs.push_back(fb);
                bRemove = true;
                break;

            case MESON_COMPOSITION_CLIENT:
                mHaveClient  = true;
                bClientLayer = true;
            case MESON_COMPOSITION_UNDETERMINED:
                {
                    /* we thought plane with same type have some scanout capacity.
                     * so just check with first osd plane.
                     */
                    auto plane = mOsdPlanes.begin();
                    if (bClientLayer || (*plane)->isFbSupport(fb) == false) {
                        MESON_LOGD("client: %d, fb %d is not support, set to compose.", bClientLayer, fb->mZorder);
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

/* Only support one leagcy video in current times. */
#if LEGACY_VIDEO_MODE_SWITCH
    if (!mOverlayFbs.empty() && !mFramebuffers.empty()) {
        auto osdFbIt = mFramebuffers.begin();
        uint32_t minOsdFbZorder = osdFbIt->second->mZorder;
        osdFbIt = mFramebuffers.end();
        osdFbIt --;
        uint32_t maxOsdFbZorder = osdFbIt->second->mZorder;

        /* Current only input one Leagcy video fb. */
        std::shared_ptr<DrmFramebuffer> leagcyVideoFb = *(mOverlayFbs.begin());
        if (leagcyVideoFb->mZorder > minOsdFbZorder) {
            mMinComposerZorder = mFramebuffers.begin()->second->mZorder;
            /* Leagcy video is always on the bottom.
             * SO, all fbs below leagcyVideo zorder need to compose.
             * Set maxClientZorder = leagcyVideoZorder
             */
            if (mMaxComposerZorder == INVALID_ZORDER || leagcyVideoFb->mZorder > maxOsdFbZorder) {
                mMaxComposerZorder = leagcyVideoFb->mZorder;
            }
        }
    }
#else // If only one leagcy video in current times, don't need to check inside video flag.
    /* 1. check mInsideVideoFbsFlag = false
     * 2. for HDR mode, adjust compose range.
     */
    if (!mOverlayFbs.empty() && !mFramebuffers.empty()) {
        auto uiFbIt = mFramebuffers.begin();
        uint32_t uiFbMinZorder = uiFbIt->second->mZorder;
        uiFbIt = mFramebuffers.end();
        uiFbIt --;
        uint32_t uiFbMaxZorder = uiFbIt->second->mZorder;
        MESON_LOGD("%s : UI fb range (%d ~ %d).", __func__, uiFbMinZorder, uiFbMaxZorder);

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
                MESON_LOGD("Video (z=%d) is on top/bottom, not overlay candidate, remove.",
                    videoFb->mZorder);
                videoFbIt = mOverlayFbs.erase(videoFbIt);
            }
        }

        MESON_LOGD("%s : after adjust , UI fb range (%d ~ %d).",
            __func__, uiFbMinZorder, uiFbMaxZorder);
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
int MultiplanesComposition::countComposerFbs(int &belowClientNum, int &upClientNum, int &insideClientNum) {
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

int MultiplanesComposition::confirmComposerRange() {
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
    MESON_LOGD("default compose range: (%d ~ %d)",
        mMinComposerZorder, mMaxComposerZorder);
    MESON_LOGD("Fb default valid: %d, need compose num %d | %d | %d ",
        ret, belowClientNum, insideClientNum, upClientNum);

    MESON_LOGD("fb vs plane: %d | %d, client: %d",
        osdFbsNum, osdPlanesNum, mHaveClient);
    if (osdFbsNum > osdPlanesNum) { // CASE 1: osdFbsNum > osdPlanesNum , need compose more fbs.*/
        /* CASE 1_1: mMinComposerZorder != MAX_PLANE_ZORDER, need do compose. */
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
                    MESON_LOGD("Compose from begin, still need %d fbs.",
                        minNeedComposedFbs);
                }
                /* confirm the minimum zorder value. */
                mMinComposerZorder = fbIt->second->mZorder;

                /* compose fb from maximum zorder. */
                if (minNeedComposedFbs > 0) {
                    MESON_ASSERT(upClientNum > 0, "upClientNum should > 0.");
                    fbIt = mFramebuffers.find(mMaxComposerZorder);
                    fbIt ++;
                    for (; fbIt != mFramebuffers.end(); ++ fbIt) {
                        minNeedComposedFbs --;
                        if (minNeedComposedFbs <= 0)
                            break;
                    }
                    /* we can confirm the maximum zorder value. */
                    mMaxComposerZorder = fbIt->second->mZorder;
                    MESON_LOGD("osd fb > plane, has default range, confirm range(%d ~ %d)",
                        mMinComposerZorder, mMaxComposerZorder);
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
            MESON_LOGD("osd fb > plane, no default range, confirm range(%d ~ %d)",
                mMinComposerZorder, mMaxComposerZorder);
        }
    }

    return 0;
}

/* Set DisplayPairs between UI(OSD) Fbs with plane. */
int MultiplanesComposition::setOsdFbs2PlanePairs() {
    std::shared_ptr<HwDisplayPlane> osdTemp;
    for (uint32_t i = 1; i < mOsdPlanes.size(); i++) {
        if (mOsdPlanes[i]->getCapabilities() & PLANE_PRIMARY) {
            osdTemp = mOsdPlanes[0];
            mOsdPlanes[0] = mOsdPlanes[i];
            mOsdPlanes[i] = osdTemp;
            break;
        }
    }

    uint32_t uiFbsNum = mFramebuffers.size();

    bool bDin0 = false;
    bool bDin1 = false;
    bool bDin2 = false;
    /* only fb, set to primary plane directlly. */

    if (uiFbsNum == 1) {
        std::shared_ptr<DrmFramebuffer> fb = mFramebuffers.begin()->second;
        MESON_LOGD("only din0-zorder: %d", fb->mZorder);
        mDisplayPairs.push_back(
            DisplayPair{OSD_PLANE_DIN_ZERO, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_ZERO]});
        bDin0 = true;

        if (fb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
            fb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
    } else {
        for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end(); ++fbIt) {
            std::shared_ptr<DrmFramebuffer> fb = fbIt->second;
            /* fb is unscaled or is represent of composer output,
             * we pick primary display.
             */
            bool bFlag = false;
            if (bDin0 == false) {
                if ((fb->mZorder <= mMaxComposerZorder &&
                    fb->mZorder >= mMinComposerZorder) ||
                    fb->isScaled() == false) {
                    MESON_LOGD("din0-zorder: %d", fb->mZorder);
                    mDisplayPairs.push_back(
                        DisplayPair{OSD_PLANE_DIN_ZERO, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_ZERO]});
                    bDin0 = true;
                    bFlag = true;
                }
            }
            if ((bDin1 == false || bDin2 == false) && (bFlag == false)) {
                /* Pick any available plane */
                if (bDin1 == false) {
                    MESON_LOGD("din1-zorder: %d", fb->mZorder);
                    mDisplayPairs.push_back(
                        DisplayPair{OSD_PLANE_DIN_ONE, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_ONE]});
                    bDin1 = true;
                } else {
                    MESON_LOGD("din2-zorder: %d", fb->mZorder);
                    mDisplayPairs.push_back(
                        DisplayPair{OSD_PLANE_DIN_TWO, fb->mZorder, fb, mOsdPlanes[OSD_PLANE_DIN_TWO]});
                    bDin2 = true;
                }
            }

            /* Not composed fb, set to osd composition. */
            if (fb->mCompositionType == MESON_COMPOSITION_UNDETERMINED)
                fb->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
        }
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
int MultiplanesComposition::selectComposer() {
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
    MESON_LOGD("Use composer (%s)", mComposer->getName());
    for (auto it = mComposerFbs.begin(); it != mComposerFbs.end(); ++it) {
        (*it)->mCompositionType = mComposer->getType();
    }

    return 0;
}

/* Find out which fb need to compse and push DisplayPair */
int MultiplanesComposition::fillComposerFbs() {
    std::shared_ptr<DrmFramebuffer> fb;
    if (mMaxComposerZorder != INVALID_ZORDER &&
        mMinComposerZorder != INVALID_ZORDER) {
            MESON_LOGD("will compose fb (%d ~ %d)",
                mMinComposerZorder, mMaxComposerZorder);

        uint32_t maxOsdFbZorder = mFramebuffers.find(mMaxComposerZorder)->second->mZorder;
        for (auto fbIt = mFramebuffers.begin(); fbIt != mFramebuffers.end();) {
            fb = fbIt->second;
            if (fb->mZorder <= mMaxComposerZorder &&
                fb->mZorder >= mMinComposerZorder) {
                mComposerFbs.push_back(fb);
                MESON_LOGD("fill composer zorder: %d", fb->mZorder);
                if (fb->mZorder == maxOsdFbZorder)
                    ++ fbIt;
                else
                    fbIt = mFramebuffers.erase(fbIt);
            } else {
                MESON_LOGD("not composer zorder: %d", fb->mZorder);
                ++ fbIt;
            }
        }
    }

    for (auto videoFbIt = mOverlayFbs.begin(); videoFbIt != mOverlayFbs.end();) {
        std::shared_ptr<DrmFramebuffer> videoFb = *videoFbIt;
        if ((videoFb->mZorder > mMinComposerZorder) ||
            (videoFb->mZorder < mMaxComposerZorder)) {
            ++videoFbIt;
        } else {
            MESON_LOGD("Video (z=%d) is on top/bottom, remove.",
                videoFb->mZorder);
            videoFbIt = mOverlayFbs.erase(videoFbIt);
        }
    }

    return 0;
}

/* Update present zorder.
 * If videoZ ~ (mMinComposerZorder, mMaxComposerZorder), set maxVideoZ = maxVideoZ - 1
 */
void MultiplanesComposition::handleOverlayVideoZorder() {
    auto it = mDisplayPairs.begin();
    it = mDisplayPairs.begin();
    for (; it != mDisplayPairs.end(); ++it) {
        if (it->fb->mZorder >= mMinComposerZorder && it->fb->mZorder <= mMaxComposerZorder) {
            it->presentZorder = it->presentZorder - 1;
        }
    }
}

/* If osdFbsNum == 3, and has video inside osd fbs.
 * Step 1: Confirm middlle zorder
 * Step 2: Compare videoZ with middleZ
 *         If VideoZ > middleOsdZ, judge middleZ and minOsdZ Scaled.
 *         If VideoZ < middleOsdZ, judge middleZ and maxOsdZ Scaled.
 */
void MultiplanesComposition::handleVPULimit(bool video) {
    int belowClientNum  = 0;
    int upClientNum     = 0;
    int insideClientNum = 0;
    int ret = countComposerFbs(belowClientNum, upClientNum, insideClientNum);
    MESON_LOGD("video: %d, composer valid: %d, confirmed composer num %d | %d | %d ",
        video, ret, belowClientNum, insideClientNum, upClientNum);

    uint32_t osdFbsNum  = mFramebuffers.size();
    int remainOsdFbsNum = osdFbsNum - insideClientNum + 1;
    auto minFbIt = mFramebuffers.begin();
    uint32_t minOsdZ = minFbIt->second->mZorder;
    auto maxFbIt = mFramebuffers.end();
    maxFbIt --;
    uint32_t maxOsdZ = maxFbIt->second->mZorder;

    std::shared_ptr<DrmFramebuffer> fb;
    if (mHDRMode == false && osdFbsNum == 2) {
        return; // non-hdr mode, 2 osd fbs, nothing to do.
    } else if (video == true
               && remainOsdFbsNum == 3
               && (mMaxVideoZorder <= maxOsdZ && mMinVideoZorder > minOsdZ)) {
        uint32_t noscaleNum = 0;
        auto uiFbIt = mFramebuffers.begin();
        for (; uiFbIt != mFramebuffers.end(); ++ uiFbIt) {
            fb = uiFbIt->second;
            if (fb->isScaled() == false
                || (fb->mZorder >= mMinComposerZorder && fb->mZorder <= mMaxComposerZorder)) {
                noscaleNum++;
            }
            if (noscaleNum == 2) {
                break;
            }
        }
        if (noscaleNum < 2) {
            /* step 1: Confirm middlle zorder. */
            auto middleFbIt = mFramebuffers.begin(); // init midlle position
            uint32_t middleOsdZ = maxFbIt->second->mZorder;
            if (mMaxComposerZorder != INVALID_ZORDER && mMaxComposerZorder == maxOsdZ) {
                middleFbIt ++;
            } else {
                middleFbIt = mFramebuffers.end();
                middleFbIt --;
                middleFbIt --;
            }
            middleOsdZ = middleFbIt->second->mZorder;

            /* step 2: Compare videoZ with middleZ. */
            if (middleFbIt->second->isScaled() == true
                && (middleOsdZ < mMinComposerZorder || middleOsdZ > mMaxComposerZorder)) {
                if (mMaxVideoZorder > middleOsdZ
                    && minFbIt->second->isScaled() == true
                    && (minOsdZ < mMinComposerZorder || minOsdZ > mMaxComposerZorder)) {
                    mMinComposerZorder = middleOsdZ;
                    if (mMaxComposerZorder == INVALID_ZORDER) {
                        mMaxComposerZorder = middleOsdZ;
                    }
                } else if (mMaxVideoZorder < middleOsdZ
                           && maxFbIt->second->isScaled() == true
                           && (maxOsdZ < mMinComposerZorder || maxOsdZ > mMaxComposerZorder)) {
                    mMaxComposerZorder = mMaxVideoZorder;
                    if (mMinComposerZorder == INVALID_ZORDER) {
                        mMinComposerZorder = minOsdZ;
                    }
                }
            }
        }
    } else {
        bool needComposeForScale = true;
        for (auto iter = mFramebuffers.begin(); iter != mFramebuffers.end(); ++iter) {
            /* Has NoScale Fbs or default composer range. */
            if (iter->second->isScaled() == false
                || (iter->second->mZorder <= mMaxComposerZorder && iter->second->mZorder >= mMinComposerZorder)) {
                needComposeForScale = false;
                break;
            }
        }
        MESON_LOGD("needComposeForScale:%d", needComposeForScale);

        /* All Scale Fbs and no default composer range, select begin Fbs to composer. */
        if (needComposeForScale == true) {
            mMinComposerZorder = (mFramebuffers.begin()->second)->mZorder;
            mMaxComposerZorder = mMinComposerZorder;
            MESON_LOGD("All scale fbs and no default composer range, select minimum zorder:%d to compose.", mMinComposerZorder);
        }
    }

}

/* Handle OSD Fbs and set OsdFbs2Plane pairs. */
int MultiplanesComposition::handleOsdComposition() {
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

int MultiplanesComposition::handleOsdCompostionWithVideo() {
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
void MultiplanesComposition::setup(
    std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
    std::vector<std::shared_ptr<IComposer>> & composers,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
    uint32_t reqFlag) {
    init();

    mCompositionFlag = reqFlag;
    if (reqFlag & COMPOSE_WITH_HDR_VIDEO) {
        mHDRMode = true;
    }
    if (reqFlag & COMPOSE_HIDE_SECURE_FB) {
        mHideSecureLayer = true;
    }
    if (reqFlag & COMPOSE_FORCE_CLIENT) {
        mForceClientComposer = true;
    }

    /* add layers */
    auto layerIt = layers.begin();
    for (; layerIt != layers.end(); ++layerIt) {
        std::shared_ptr<DrmFramebuffer> layer = *layerIt;
        mFramebuffers.insert(make_pair(layer->mZorder, layer));
        MESON_LOGD("SF inject zorder: %d", layer->mZorder);
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
                MESON_ASSERT(mOsdPlanes.size() < OSD_PLANE_NUM_MAX,
                    "More than three osd planes !!");
                mOsdPlanes.push_back(plane);
                break;

            case HWC_VIDEO_PLANE:
                if (mHwcVideoPlane.get() == NULL) {
                    mHwcVideoPlane = plane;
                } else {
                    MESON_LOGE("More than one hwc_video osd plane, not support now.");
                    mOtherPlanes.push_back(plane);
                }
                break;

            case LEGACY_VIDEO_PLANE:
                if (mLegacyVideoPlane.get() == NULL) {
                    mLegacyVideoPlane = plane;
                } else {
                    MESON_LOGE("More than one legacy_video osd plane, discard.");
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
int MultiplanesComposition::decideComposition() {
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
    return ret;
}

/* Commit DisplayPair to display. */
int MultiplanesComposition::commit() {
    /* Start compose, and replace composer output with din0 Pair. */
    std::shared_ptr<DrmFramebuffer> composerOutput;
    if (mComposer.get()) {
        mComposer->addInputs(mComposerFbs, mOverlayFbs);
        mComposer->start();
        composerOutput = mComposer->getOutput();
    }
    handleOverlayVideoZorder();

    /* Commit display path. */
    auto displayIt = mDisplayPairs.begin();
    for (; displayIt != mDisplayPairs.end(); ++displayIt) {
        /* Maybe video or osd ui zorder = 0, set all ui zorder + 1 in the end. */
        uint32_t presentZorder = displayIt->presentZorder + 1;
        std::shared_ptr<DrmFramebuffer> fb = displayIt->fb;
        std::shared_ptr<HwDisplayPlane> plane = displayIt->plane;
        bool blankFlag = (mHideSecureLayer && fb->mSecure) ?
            BLANK_FOR_SECURE_CONTENT : UNBLANK;

        if (composerOutput.get() &&
            fb->mCompositionType == mComposer->getType()) {
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
            presentZorder = mMaxComposerZorder + 1;
            fb = composerOutput;
        } else {
            dumpFbAndPlane(fb, plane, presentZorder, blankFlag);
        }

        MESON_LOGD("setPlane presentZorder: %d", presentZorder);
        /* Set display info. */
        plane->setPlane(fb, presentZorder);
        plane->blank(blankFlag);
    }

    /* Blank un-used plane. */
    if (mLegacyVideoPlane.get())
        mOtherPlanes.push_back(mLegacyVideoPlane);
    if (mHwcVideoPlane.get())
        mOtherPlanes.push_back(mLegacyVideoPlane);
    auto osdIt = mOsdPlanes.begin();
    for (; osdIt != mOsdPlanes.end(); ++osdIt) {
        mOtherPlanes.push_back(*osdIt);
    }

    auto planeIt = mOtherPlanes.begin();
    for (; planeIt != mOtherPlanes.end(); ++planeIt) {
        (*planeIt)->blank(BLANK_FOR_NO_CONENT);
        dumpUnusedPlane(*planeIt, BLANK_FOR_NO_CONENT);
    }

    return 0;
}
