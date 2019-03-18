/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_POSTPROCESSOR_H
#define HWC_POSTPROCESSOR_H

typedef enum {
    VDIN_POST_PROCESSOR = 0,
    INVALID_POST_PROCESSOR,
} hwc_post_processor_t;

enum {
    PRESENT_BLANK = 1<< 0,
    PRESENT_SIDEBAND = 1 << 1,
};

class HwcPostProcessor {
public:
    virtual ~HwcPostProcessor() { }

    virtual int32_t start() = 0;
    virtual int32_t stop() = 0;
    virtual bool running() = 0;

    virtual int32_t present(int32_t flags, int32_t fence) = 0;
};

#endif
