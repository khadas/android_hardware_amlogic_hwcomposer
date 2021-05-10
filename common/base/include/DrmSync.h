/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DRM_SYNC_H
#define DRM_SYNC_H

#include <stdlib.h>
#include <BasicTypes.h>

class DrmFence {
public:
    DrmFence(int32_t fencefd);
    ~DrmFence();

    int32_t wait(int timeout);
    int32_t waitForever(const char* logname);

    bool isValid() const { return mFenceFd != -1; }

    int32_t getFd() {return mFenceFd;};
    int32_t dup() const;
    static std::shared_ptr<DrmFence> merge(const char * name,
        const std::shared_ptr<DrmFence> &f1,
        const std::shared_ptr<DrmFence> &f2);

    enum class Status {
        Invalid,     // Fence is invalid
        Unsignaled,  // Fence is valid but has not yet signaled
        Signaled,    // Fence is valid and has signaled
    };

    // getStatus() returns whether the fence has signaled yet. Prefer this to
    // getSignalTime() or wait() if all you care about is whether the fence has
    // signaled.
    inline Status getStatus() {
        switch (wait(0)) {
            case 0:
                return Status::Signaled;
            case -ETIME:
                return Status::Unsignaled;
            default:
                return Status::Invalid;
        }
    }

public:
    static const std::shared_ptr<DrmFence> NO_FENCE;

protected:
    int32_t mFenceFd;
};

class DrmTimeLine {
public:
};

#endif/*DRM_SYNC_H*/
