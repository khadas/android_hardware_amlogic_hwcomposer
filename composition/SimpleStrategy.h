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

#include "ICompositionStrategy.h"
#include <list>

class SimpleStrategy : public ICompositionStrategy {
public:
    SimpleStrategy();
    ~SimpleStrategy();

    void setUp(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposeDevice>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes);

    int32_t decideComposition();

    int32_t commit();

protected:
    void classifyLayers(std::vector<std::shared_ptr<DrmFramebuffer>> & layers);
    void classifyComposers(std::vector<std::shared_ptr<IComposeDevice>> & composers);
    void classifyPlanes(std::vector<std::shared_ptr<HwDisplayPlane>> & planes);

    void setUiComposer();
    void preProcessLayers();
    bool isPlaneSupported(std::shared_ptr<DrmFramebuffer> & fb);

protected:
    std::list<std::shared_ptr<HwDisplayPlane>> mVideoPlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mOsdPlanes;
    std::list<std::shared_ptr<HwDisplayPlane>> mCursorPlanes;

    std::shared_ptr<IComposeDevice> mDummyComposer;
    std::shared_ptr<IComposeDevice> mClientComposer;
    std::list<std::shared_ptr<IComposeDevice>> mComposers;
    std::shared_ptr<IComposeDevice> mUiComposer;

    std::vector<std::shared_ptr<DrmFramebuffer>> mLayers;
    std::vector<std::shared_ptr<DrmFramebuffer>> mUiLayers;

    bool mHaveClientLayer;

};

#endif/*SIMPLE_STRATEGY_H*/
