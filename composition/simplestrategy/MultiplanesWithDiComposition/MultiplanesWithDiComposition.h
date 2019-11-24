/*
* Copyright (c) 2018 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description:
*/

#ifndef MULTIPLANESWITHDI_COMPOSITION_H
#define MULTIPLANESWITHDI_COMPOSITION_H

#include <functional>
#include "ICompositionStrategy.h"


/*
------------------------------------------------------------------------------------------------------------
|  VPU on s905d2 Architecture, 2018                                                                        |
------------------------------------------------------------------------------------------------------------
|                                                                                                          |
|                                                                                            -------       |
|                                                                                            |     |       |
|                                                                                            |     |       |
|  -----------------------------------------------------------VIDEO1------------------------>|     |       |                                                                                     |
|                                                                                            |     |       |                                                                           |
|  -----------------------------------------------------------VIDEO1------------------------>|     |       |
|                                                                                            |     |       |
|                         -----                                                              |     |       |
|                         |   |                                                              |     |       |
|                         |   |         --------                                             |     |       |
|  ---------din0--------->|---|--din0-->|      |   --------                                  |     |       |
|                         |   |         |BLEND0|-->|      |   -------   -------------        |BLEND|---->  |
|                         |   |         |      |   |      |-->| HDR |-->| FreeScale |--OSD-->|     |       |
|                         |   |         --------   |BLEND2|   -------   -------------        |     |       |
|                         |   |                    |      |                                  |     |       |
|           -----------   |   |         --------   |      |                                  |     |       |
|  --din1-->|FreeScale|-->|OSD|-------->|      |-->|      |                                  |     |       |
|           -----------   |MUX|         |BLEND1|   --------                                  |     |       |
|                         |   |-------->|      |                                             |     |       |
|                         |   |         --------                                             |     |       |
|                         |   |                                                              |     |       |
|           -----------   |   |                                                              |     |       |
|  --din2-->|FreeScale|-->|   |-------------------------- OSD (4k) ------------------------->|     |       |
|           -----------   |   |                                                              |     |       |
|                         |   |                                                              |     |       |
|                         -----                                                              -------       |
|                                                                                                          |
------------------------------------------------------------------------------------------------------------
*/

#define OSD_PLANE_NUM_MAX         3  // Maximum osd planes of support

class MultiplanesWithDiComposition : public ICompositionStrategy {
public:
    MultiplanesWithDiComposition();
    ~MultiplanesWithDiComposition();

    const char* getName() {return "MultiplanesWithDiComposition";}

    void setup(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposer>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
        std::shared_ptr<HwDisplayCrtc> & crtc,
        uint32_t flags);

    int decideComposition();
    int commit();
    //void dump(String8 & dumpstr);

protected:
    void init();
    int handleVideoComposition();
    int applyCompositionFlags();
    int pickoutOsdFbs();
    int countComposerFbs(int &belowClientNum, int &upClientNum, int &insideClientNum);
    int confirmComposerRange();
    int setOsdFbs2PlanePairs();
    int selectComposer();
    int fillComposerFbs();
    void handleOverlayVideoZorder();
    int checkCommitZorder();
    void handleVPULimit(bool video);
    void handleDispayLayerZorder();
    int handleOsdComposition();
    int handleOsdCompostionWithVideo();
    void handleDiComposition();
    int32_t compareFbScale(drm_rect_t & aSrc, drm_rect_t & aDst, drm_rect_t & bSrc, drm_rect_t & bDst);

protected:
    struct DisplayPair {
        uint32_t din;                           // 0: din0, 1: din1, 2:din2, 3:video1, 4:video2
        uint32_t presentZorder;
        std::shared_ptr<DrmFramebuffer> fb;     // UI or Video from SF
        std::shared_ptr<HwDisplayPlane> plane;  // osdPlane <= 3, videoPlane <= 2
    };

    /* Input Flags from SF */
    bool mHDRMode;
    bool mHideSecureLayer;
    bool mForceClientComposer;

    /* Input Fbs from SF, min zorder at begin, max zorder at end. */
    std::map<uint32_t, std::shared_ptr<DrmFramebuffer>, std::less<uint32_t>> mFramebuffers;

    /*reffb is the fb used to setup the osddisplayframe.*/
    std::shared_ptr<DrmFramebuffer> mDisplayRefFb;
    display_zoom_info_t mOsdDisplayFrame;
    std::shared_ptr<HwDisplayCrtc> mCrtc;

    /* Composer */
    std::shared_ptr<IComposer> mDummyComposer;
    std::shared_ptr<IComposer> mClientComposer;
    std::vector<std::shared_ptr<IComposer>> mOtherComposers;

    /* Get display planes from DispalyManager */
    std::vector<std::shared_ptr<HwDisplayPlane>> mOsdPlanes;

    std::vector<std::shared_ptr<HwDisplayPlane>> mHwcVideoPlanes;             // Future  VIDEO support : 2 HwcVideoPlane
    std::shared_ptr<HwDisplayPlane> mLegacyVideoPlane;          // Current VIDEO support : 1 LegacyVideoPlane + 1 LegacyExtVideoPlane
    std::shared_ptr<HwDisplayPlane> mLegacyExtVideoPlane;
    std::vector<std::shared_ptr<HwDisplayPlane>> mOtherPlanes;

    /* Use for composer */
    std::shared_ptr<IComposer> mComposer;                       // Handle composer Fbs
    std::vector<std::shared_ptr<DrmFramebuffer>> mOverlayFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> mComposerFbs;  // Save Fbs that should be composered
    std::vector<std::shared_ptr<DrmFramebuffer>> mDIComposerFbs;
    std::list<DisplayPair> mDisplayPairs;

    bool mHaveClient;
    bool mInsideVideoFbsFlag;      // Has VIDEO between different OSD ui layers.
    uint32_t mMinComposerZorder;
    uint32_t mMaxComposerZorder;
    uint32_t mMinVideoZorder;
    uint32_t mMaxVideoZorder;

    std::shared_ptr<IComposer> mDiComposer;
    std::vector<std::shared_ptr<DrmFramebuffer>> mDiInFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> mDiOutFbs;
};


#endif/*MULTIPLANESWITHDI_COMPOSITION_H*/
