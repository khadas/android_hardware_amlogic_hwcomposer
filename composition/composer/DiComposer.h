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
#include <VideoComposerDev.h>

#define DI_COMPOSER_NAME "DiComposer"

class DiComposer : public IComposer {
public:
    DiComposer();
    ~DiComposer();

    const char* getName() { return DI_COMPOSER_NAME; }
    meson_compositon_t getType() { return MESON_COMPOSITION_DI; }

    bool isFbsSupport(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
        std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs,
        int composeIdx = 0);

    int32_t prepare();

    int32_t addInputs(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
        std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs,
        int composeIdx = 0);

    int32_t getOverlyFbs(std::vector<std::shared_ptr<DrmFramebuffer>> & overlays);

    int32_t setOutput(
        std::shared_ptr<DrmFramebuffer> & fb,
        hwc_region_t damage,
        int composeIdx = 0);

    int32_t start(int composeIdx = 0);

    std::shared_ptr<DrmFramebuffer> getOutput(int composeIdx = 0);

protected:
    class ComposerImpl{
        public:
            std::vector<std::shared_ptr<DrmFramebuffer>> inputFbs;
            std::vector<std::shared_ptr<DrmFramebuffer>> overlayFbs;
            std::shared_ptr<DrmFramebuffer> outputFb;
            std::shared_ptr<VideoComposerDev> composeDev;
    };

    std::vector<std::shared_ptr<ComposerImpl>> mComposerImpl;
    int mImplNum;
};

#endif/*DUMMY_COMPOSER_H*/

