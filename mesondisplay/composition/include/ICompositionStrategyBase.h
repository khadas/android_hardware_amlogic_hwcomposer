/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef ICOMPOSITION_STRATEGY_BASE_H
#define ICOMPOSITION_STRATEGY_BASE_H

#include <stdlib.h>

#include <BasicTypes.h>
#include <DrmFramebufferBase.h>
#include <IComposer.h>
#include <HwDisplayPlaneBase.h>
#include <HwDisplayCrtcBase.h>

struct DisplayPair {
    uint32_t din;                               // 0: din0, 1: din1, 2:din2, 3:video1, 4:video2
    uint32_t presentZorder;
    std::shared_ptr<DrmFramebufferBase> fb;     // UI or Video from SF
    std::shared_ptr<HwDisplayPlaneBase> plane;  // osdPlane <= 3, videoPlane <= 2
};

/*Base composition strategy class.*/
class ICompositionStrategyBase {
public:
    ICompositionStrategy();
    virtual ~ICompositionStrategy() { }

    virtual const char* getName() = 0;

    /*before call decideComb, setup will clean last data.*/
    virtual void setup(std::vector<std::shared_ptr<DrmFramebufferBase>> & layers,
        std::vector<std::shared_ptr<IComposer>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlaneBase>> & planes,
        std::shared_ptr<HwDisplayCrtcBase> & crtc,
        uint32_t reqFlag, float scaleValue,
        drm_mode_info_t mode) = 0;

    virtual void updateComposition() = 0;

    /*if have no valid combs, result will < 0.*/
    virtual int decideComposition() = 0;

    /*start composition, should set release fence to each Framebuffer.*/
    virtual int commit() = 0;

    virtual int getDisplayPair(std::list<DisplayPair> mDisplayPairs) = 0;

    virtual void dump(std::string & dumpstr) = 0;
};

#endif/*ICOMPOSITION_STRATEGY_BASE_H*/
