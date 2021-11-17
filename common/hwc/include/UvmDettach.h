/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_UVM_DETTACH_H
#define HWC_UVM_DETTACH_H

#include <deque>
#include <hardware/hwcomposer2.h>

#include <DrmSync.h>

class UvmDettach {
public:
    UvmDettach(hwc2_layer_t layerId);
    UvmDettach(int tunnelId);
    ~UvmDettach();

    int32_t attachUvmBuffer(int bufferFd);
    int32_t dettachUvmBuffer();
    int32_t collectUvmBuffer(const int fd, const int fence);
    int32_t releaseUvmResource();

    char *getDebugName() {return mName;};

protected:
    char mName[20];

    struct UvmBuffer {
        int bufferFd;
        std::shared_ptr<DrmFence> releaseFence;
    };

    std::deque<UvmBuffer> mUvmBufferQueue;
};
#endif /* HWC_UVM_DETTACH_H */
