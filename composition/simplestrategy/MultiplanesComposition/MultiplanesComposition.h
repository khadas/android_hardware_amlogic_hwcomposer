/*
* Copyright (c) 2018 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description:
*/

#ifndef MULTIPLANES_COMPOSITION_H
#define MULTIPLANES_COMPOSITION_H

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


class MultiplanesComposition : public ICompositionStrategy {
public:
    MultiplanesComposition();
    ~MultiplanesComposition();

    const char* getName() {return "MultiplanesComposition";}

    void setup(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposer>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
        uint32_t flags);

    int decideComposition();
    int commit();

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
    int handleOsdComposition();
    int handleOsdCompostionWithVideo();


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

    /* Composer */
    std::shared_ptr<IComposer> mDummyComposer;
    std::shared_ptr<IComposer> mClientComposer;
    std::vector<std::shared_ptr<IComposer>> mOtherComposers;

    /* Get display planes from DispalyManager */
    std::vector<std::shared_ptr<HwDisplayPlane>> mOsdPlanes;
    std::shared_ptr<HwDisplayPlane> mHwcVideoPlane;             // Future  VIDEO support : 2 HwcVideoPlane
    std::shared_ptr<HwDisplayPlane> mLegacyVideoPlane;          // Current VIDEO support : 1 LegacyVideoPlane + 1 HwcVideoPlane
    std::vector<std::shared_ptr<HwDisplayPlane>> mOtherPlanes;

    /* Use for composer */
    std::shared_ptr<IComposer> mComposer;                       // Handle composer Fbs
    std::vector<std::shared_ptr<DrmFramebuffer>> mOverlayFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> mComposerFbs;  // Save Fbs that should be composered
    std::list<DisplayPair> mDisplayPairs;

    bool mHaveClient;
    bool mInsideVideoFbsFlag;      // Has VIDEO between different OSD ui layers.
    uint32_t mMinComposerZorder;
    uint32_t mMaxComposerZorder;
    uint32_t mMinVideoZorder;
    uint32_t mMaxVideoZorder;

};


#endif/*MULTIPLANES_COMPOSITION_H*/
