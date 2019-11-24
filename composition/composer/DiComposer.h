/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DI_COMPOSER_H
#define DI_COMPOSER_H

#include <IComposer.h>

#define DI_COMPOSER_NAME "DiComposer"

class DiComposer : public IComposer {
public:
    DiComposer();
    ~DiComposer();

    const char* getName() { return DI_COMPOSER_NAME; }
    meson_compositon_t getType() { return MESON_COMPOSITION_DI; }

    bool isFbsSupport(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
        std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs);

    int32_t prepare();

    int32_t addInput(std::shared_ptr<DrmFramebuffer> & fb, bool bOverlay = false);

    int32_t addInputs(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
        std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs);

    int32_t getOverlyFbs(std::vector<std::shared_ptr<DrmFramebuffer>> & overlays);

    int32_t setOutput(std::shared_ptr<DrmFramebuffer> & fb,
        hwc_region_t damage);

    int32_t start();

    std::shared_ptr<DrmFramebuffer> getOutput();

protected:
    std::vector<std::shared_ptr<DrmFramebuffer>> mVideoComposerIn;
    std::vector<std::shared_ptr<DrmFramebuffer>> mVideoComposerOut;


};

#endif/*DUMMY_COMPOSER_H*/

