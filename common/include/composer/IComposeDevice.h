/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef ICOMPOSE_DEVICE_H
#define ICOMPOSE_DEVICE_H

#include <stdlib.h>

#include <BasicTypes.h>
#include <Composition.h>
#include <DrmFramebuffer.h>

class IComposeDevice {
public:
    IComposeDevice() { }
    virtual ~IComposeDevice() { }

    virtual const char* getName() = 0;

    /*return mask of meson_compositon_t */
    virtual bool isCompositionSupport(meson_compositon_t type) = 0;

    /*check if input framebuffer can be consumed */
    virtual bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb) = 0;

    /* preapre for new composition pass.*/
    virtual int32_t prepare() = 0;

    /*return meson_compositon_t type */
    virtual meson_compositon_t getCompostionType(
        std::shared_ptr<DrmFramebuffer> & fb) = 0;

    /* add input framebuffers to this composer.*/
    virtual int32_t addInput(std::shared_ptr<DrmFramebuffer> & fb) = 0;

    virtual int32_t setOutput(std::shared_ptr<DrmFramebuffer> & fb,
        hwc_region_t damage) = 0;

    /* Start composition. When this function exit, input
   * should be able to get its relese fence.*/
    virtual int32_t start() = 0;

    virtual std::shared_ptr<DrmFramebuffer> getOutput();

};

#endif/*ICOMPOSER_DEVICE_H*/
