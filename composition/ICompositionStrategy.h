/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef ICOMPOSITION_STRATEGY_H
#define ICOMPOSITION_STRATEGY_H

#include <stdlib.h>
#include <memory>
#include <vector>

#include <DrmFramebuffer.h>
#include <IComposeDevice.h>
#include <HwDisplayPlane.h>

class ICompositionStrategy {
public:
    ICompositionStrategy() { }
    virtual ~ICompositionStrategy() { }

    /*before call decideComb, setup will clean last datas.*/
    virtual void setUp(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposeDevice>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes) = 0;

    /*if have no valid combs, result will < 0.*/
    virtual int32_t decideComposition() = 0;

    /*start composition, should set release fence to each Framebuffer.*/
    virtual int32_t commit() = 0;
};

#endif/*ICOMPOSITION_STRATEGY_H*/

