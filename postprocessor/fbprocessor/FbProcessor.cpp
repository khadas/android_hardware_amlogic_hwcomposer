/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <MesonLog.h>
#include <FbProcessor.h>
#include "DummyProcessor.h"
#include "CopyProcessor.h"

int32_t createFbProcessor(
    meson_fb_processor_t type,
    std::shared_ptr<FbProcessor> & processor) {
    int ret = 0;
    switch (type) {
        case FB_DUMMY_PROCESSOR:
            processor = std::make_shared<DummyProcessor>();
            break;
        case FB_COPY_PROCESSOR:
            processor = std::make_shared<CopyProcessor>();
            break;
        case FB_KEYSTONE_PROCESSOR:
            MESON_ASSERT(0, "NO IMEPLEMENT.");
            break;
        default:
            MESON_ASSERT(0, "unknown processor type %d", type);
            processor = NULL;
            ret = -ENODEV;
    };

    return ret;
}

