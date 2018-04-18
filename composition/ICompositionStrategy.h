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

#include <BasicTypes.h>
#include <DrmFramebuffer.h>
#include <IComposeDevice.h>
#include <HwDisplayPlane.h>

typedef enum {
    COMPOSE_FORCE_CLIENT = 1 << 0,
    COMPOSE_HIDE_SECURE_FB = 1 << 1,
} COMPOSE_OP_MASK;

class ICompositionStrategy {
public:

public:
    ICompositionStrategy() { }
    virtual ~ICompositionStrategy() { }

    /*before call decideComb, setup will clean last datas.*/
    virtual void setUp(std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
        std::vector<std::shared_ptr<IComposeDevice>> & composers,
        std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
        uint32_t flags) = 0;

    /*if have no valid combs, result will < 0.*/
    virtual int32_t decideComposition() = 0;

    /*start composition, should set release fence to each Framebuffer.*/
    virtual int32_t commit() = 0;

    virtual void dump(String8 & dumpstr) = 0;
};

#endif/*ICOMPOSITION_STRATEGY_H*/

