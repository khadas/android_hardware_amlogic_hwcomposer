/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef COPY_PROCESSOR_H
#define COPY_PROCESSOR_H

#include <FbProcessor.h>
#include "common/Ge2dHelper.h"

class CopyProcessor : public FbProcessor {
public:
    CopyProcessor();
    ~CopyProcessor();

    int32_t setup();
    int32_t asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence);
    int32_t onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb,
        int releaseFence);

    int32_t process(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb);
    int32_t teardown();

    meson_fb_processor_t getFbProcessorType() {return FB_COPY_PROCESSOR;};

private:
    std::shared_ptr<Ge2dHelper> mGe2dHelper;
};

#endif

