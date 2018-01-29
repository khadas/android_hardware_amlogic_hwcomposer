/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DUMMY_COMPOSER_H
#define DUMMY_COMPOSER_H

#include <IComposeDevice.h>

#define DUMMY_COMPOSER_NAME "Dummy"

class DummyComposer : public IComposeDevice {
public:
    DummyComposer();
    ~DummyComposer();

    const char* getName() { return DUMMY_COMPOSER_NAME; }

    bool isCompositionSupport(meson_compositon_t type);

    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t prepare();

    meson_compositon_t getCompostionType(
        std::shared_ptr<DrmFramebuffer> & fb);

    int32_t addInput(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setOutput(std::shared_ptr<DrmFramebuffer> & fb,
        hwc_region_t damage);

    int32_t start();

    std::shared_ptr<DrmFramebuffer> getOutput();

};

#endif/*DUMMY_COMPOSER_H*/

