/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef SIMPLE_STRATEGY_H
#define SIMPLE_STRATEGY_H

#include <BasicTypes.h>
#include "ICompositionStrategy.h"

typedef enum {
    VIDEO_AT_BOTTOM                       = (1 << 0),
    VIDEO_AT_FIRST_POSITION               = (1 << 1),
    VIDEO_AT_FIRST_POSITION_V2            = ((1 << 1) | (1 << 2)),
    VIDEO_AT_SECOND_POSITION              = (1 << 2),
    VIDEO_AT_SECOND_POSITION_V2           = ((1 << 2) | (1 << 3)),
    VIDEO_AT_FIRST_AND_SECOND_POSITION    = ((1 << 1) | (1 << 3)),
} video_layer_position_mask;

enum {
    OSD_VIDEO_CONFLICTED                  = ((1 << 1) | (1 << 2)),
};

enum {
    OSD_LAYER_MASK                        = 0x00000001,
    Z_CONTINUOUS_PLANES_MASK              = 0x00000003,
};

class SimpleStrategy : public ICompositionStrategy {
public:
    SimpleStrategy();
    ~SimpleStrategy();

    void setUp(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposeDevice>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
        uint32_t flags);

    int32_t decideComposition();

    int32_t commit();

    void dump(String8 & dumpstr);

protected:
    void classifyLayers(std::vector<std::shared_ptr<DrmFramebuffer>> & layers);
    void classifyComposers(std::vector<std::shared_ptr<IComposeDevice>> & composers);
    void classifyPlanes(std::vector<std::shared_ptr<HwDisplayPlane>> & planes);

    void setUiComposer();
    void preProcessLayers();
    bool isPlaneSupported(std::shared_ptr<DrmFramebuffer> & fb);
    void sortLayersByZ(std::vector<std::shared_ptr<DrmFramebuffer>> &layers);
    void sortLayersByZReversed(std::vector<std::shared_ptr<DrmFramebuffer>> &layers);
    void changeDeviceToClientByZ(uint32_t from, uint32_t to);

    int32_t makeCurrentOsdPlanes(int32_t &numConflictPlanes);
    bool isVideoLayer(std::shared_ptr<DrmFramebuffer> &layer);
    bool expandComposedLayers(std::vector<std::shared_ptr<DrmFramebuffer>> &layers,
            std::vector<std::shared_ptr<DrmFramebuffer>> &composedLayers);
    void makeFinalDecision(std::vector<std::shared_ptr<DrmFramebuffer>> &composedLayers);

    void addCompositionInfo(std::shared_ptr<DrmFramebuffer>  layer,
        std::shared_ptr<IComposeDevice>  composer,
        std::shared_ptr<HwDisplayPlane>  plane,
        int planeBlank);

protected:
    std::list<std::pair<std::shared_ptr<DrmFramebuffer>,
        std::shared_ptr<HwDisplayPlane>>> mAssignedPlaneLayers;

    std::list<std::shared_ptr<HwDisplayPlane>> mCursorPlanes;

    std::list<std::shared_ptr<HwDisplayPlane>> mAmVideoPlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mHwcVideoPlanes;

    std::list<std::shared_ptr<HwDisplayPlane>> mOsdPlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mPresentOsdPlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mOsdDiscretePlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mOsdContinuousPlanes;

    std::shared_ptr<IComposeDevice> mDummyComposer;
    std::shared_ptr<IComposeDevice> mClientComposer;
    std::list<std::shared_ptr<IComposeDevice>> mComposers;
    std::shared_ptr<IComposeDevice> mUiComposer;

    std::vector<std::shared_ptr<DrmFramebuffer>> mLayers;

    std::vector<std::shared_ptr<DrmFramebuffer>> mVideoLayers;
    std::vector<std::shared_ptr<DrmFramebuffer>> mUiLayers;

    std::vector<std::shared_ptr<DrmFramebuffer>> mPreLayers;
    bool mHaveClientLayer;
    bool mSetComposerPlane;
    bool mOsdPlaneAssignedManually;

    bool mForceClientComposer;
    bool mHideSecureLayer;

    String8 mDumpStr;
};

#endif/*SIMPLE_STRATEGY_H*/
