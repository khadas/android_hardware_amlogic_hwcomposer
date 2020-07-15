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
    int processVideoFbs();
    int processGfxFbs();

    void init();
    int applyCompositionFlags();
    int pickoutOsdFbs();
    int countComposerFbs(int &belowClientNum, int &upClientNum, int &insideClientNum);
    int confirmComposerRange();
    int setOsdFbs2PlanePairs();
    int selectComposer();
    int fillComposerFbs();
    void handleOverlayVideoZorder();
    int checkCommitZorder();
    void handleVPUScaleLimit();
    void handleVPULimit(bool video);
    void handleDispayLayerZorder();
    int handleOsdComposition();
    int handleOsdCompostionWithVideo();
    int32_t compareFbScale(drm_rect_t & aSrc, drm_rect_t & aDst, drm_rect_t & bSrc, drm_rect_t & bDst);
    int handleUVM();
    int allocateDiOutputFb(
        std::shared_ptr<DrmFramebuffer> & fb, uint32_t z);

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
    std::shared_ptr<IComposer> mDiComposer;
    std::vector<std::shared_ptr<IComposer>> mOtherComposers;

    /* Get display planes from DispalyManager */
    std::vector<std::shared_ptr<HwDisplayPlane>> mOsdPlanes;

    std::vector<std::shared_ptr<HwDisplayPlane>> mHwcVideoPlanes;             // Future  VIDEO support : 2 HwcVideoPlane
    std::vector<std::shared_ptr<HwDisplayPlane>> mOtherPlanes;

    /* Use for composer */
    std::shared_ptr<IComposer> mComposer;                       // Handle composer Fbs
    std::vector<std::shared_ptr<DrmFramebuffer>> mOverlayFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> mComposerFbs;  // Save Fbs that should be composered
    std::vector<std::shared_ptr<DrmFramebuffer>> mDIComposerFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> mHwcVideoInputFbs[2];

    std::list<DisplayPair> mDisplayPairs;

    bool mHaveClient;
    bool mInsideVideoFbsFlag;      // Has VIDEO between different OSD ui layers.
    uint32_t mMinComposerZorder;
    uint32_t mMaxComposerZorder;
    uint32_t mMinVideoZorder;
    uint32_t mMaxVideoZorder;

    /* Use for UVM */
    int mUVMFd;
};


#endif/*MULTIPLANESWITHDI_COMPOSITION_H*/
