/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef MESON_VT_CONSUMER_H
#define MESON_VT_CONSUMER_H

#include <memory>
#include <vector>

#include <video_tunnel.h>

struct VtBufferItem {
    VtBufferItem()
    : mVtBufferFd(-1),
      mTimeStamp(-1) {
    }

    int32_t mVtBufferFd;
    // the buffer expected present time
    int64_t mTimeStamp;
};

class VtConsumer {
public:
    VtConsumer();
    virtual ~VtConsumer();

    class VtReleaseListener {
        virtual ~VtReleaseListener() {};
        virtual int32_t onFrameRelease(VtBufferItem &item, int fenceFd);
    };

    class VtContentListener {
        virtual ~VtContentListener() {};
        // buffer interfaces
        virtual int32_t onFrameAvailable(std::vector<VtBufferItem> & items);

        // cmd interfaces
        virtual int32_t onSourceCropChange(vt_rect & crop);
        virtual int32_t onVtCmds(vt_cmd & cmd, int data);
    };

    int32_t SetReleaseListener(std::shared_ptr<VtReleaseListener> &listener);

private:
    std::shared_ptr<VtReleaseListener> mReleaseListener;
};

#endif  /* MESON_VT_CONSUMER_H */
