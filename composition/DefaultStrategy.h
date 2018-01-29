/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef DEFAULT_STRATEGY_H
#define DEFAULT_STRATEGY_H

#include <ICompositionStrategy.h>


enum {
    LAYER_NODE = 0,
    COMPOSER_NODE = 1,
    PLANE_NODE = 2,
};

class StrategyNode {
public:
    int type; //0: layer 1: composer 2: plane
    int index;
    std::list<std::shared_ptr<StrategyNode>> child;

public:
    int32_t addChild(std::shared_ptr<StrategyNode> node);
};


class DefaultStrategy : public ICompositionStrategy {
public:
    DefaultStrategy();
    ~DefaultStrategy();

    void setUp();

    int32_t decideComposition(
        std::vector<std::shared_ptr<Hwc2Layer>> layers,
        std::vector<std::shared_ptr<IComposeDevice>> composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> planes);

protected:
    int32_t buildStrategyNodes();
    std::shared_ptr<StrategyNode> findByCompositionType(
        uint32_t compositionType, std::vector<std::shared_ptr<StrategyNode>> & nodes);
    std::shared_ptr<StrategyNode> findByFrameBufferType(
        uint32_t fbType, std::vector<std::shared_ptr<StrategyNode>> & nodes);


protected:
    /*Layers & Composer list*/
    std::list<std::shared_ptr<StrategyNode>> mLayerNodes;
    std::list<std::shared_ptr<StrategyNode>> mComposerNodes;

    std::list<std::shared_ptr<StrategyNode>> mOutputNodes;

    std::list<std::shared_ptr<StrategyNode>> mVideoNodes;
    std::list<std::shared_ptr<StrategyNode>> mOsdNodes;
    std::list<std::shared_ptr<StrategyNode>> mCursorNodes;



    std::vector<std::shared_ptr<Hwc2Layer>> mLayers;
    std::vector<std::shared_ptr<IComposeDevice>> mComposers;
    std::vector<std::shared_ptr<HwDisplayPlane>> mPlanes;

    std::shared_ptr<StrategyNode> mDummyConsumer;
    std::shared_ptr<StrategyNode> mClientConsumer;

    std::vector<std::shared_ptr<StrategyNode>> mVideoConsumers;



};


#endif/*DEFAULT_STRATEGY_H*/
