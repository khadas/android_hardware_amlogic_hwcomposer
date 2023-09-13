/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef ICOMPOSER_H
#define ICOMPOSER_H

#include <stdlib.h>

#include <BasicTypes.h>
#include <Composition.h>
#include <DrmFramebufferBase.h>

class IComposer {
public:
    IComposer() { }
    virtual ~IComposer() { }

    virtual const char* getName() = 0;
    virtual meson_composition_t getType() = 0;

    /*check if input framebuffer can be consumed */
    virtual bool isFbsSupport(
        std::vector<std::shared_ptr<DrmFramebufferBase>> & fbs,
        std::vector<std::shared_ptr<DrmFramebufferBase>> & overlayfbs,
        int composeIdx = 0) = 0;

    /* prepare for new composition pass.*/
    virtual int32_t prepare() = 0;

    /* add input framebuffers to this composer.*/
    virtual int32_t addInputs(
        std::vector<std::shared_ptr<DrmFramebufferBase>> & fbs,
        std::vector<std::shared_ptr<DrmFramebufferBase>> & overlayfbs,
        int composeIdx = 0) = 0;

    virtual int32_t setOutput(
        std::shared_ptr<DrmFramebufferBase> & fb,
        int composeIdx = 0) = 0;

    /* Start composition. When this function exit, input
   * should be able to get its release fence.*/
    virtual int32_t start(int composeIdx = 0) = 0;

    /*test if inputs can handle by composer.*/
    virtual int32_t test(int composeIdx = 0) { UNUSED(composeIdx); return 0; };

    /* return overlay fbs*/
    virtual int32_t getOverlyFbs(std::vector<std::shared_ptr<DrmFramebufferBase>> & overlays) = 0;

    virtual std::shared_ptr<DrmFramebufferBase> getOutput(int composeIdx = 0);
};

#endif/*ICOMPOSER_H*/
