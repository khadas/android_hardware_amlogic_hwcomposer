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
#include <MesonLog.h>

struct VtBufferItem {
    VtBufferItem()
    : mVtBufferFd(-1),
      mTimeStamp(-1) {
    }
    VtBufferItem(int bufferFd, int64_t timeStamp)
    : mVtBufferFd(bufferFd),
      mTimeStamp(timeStamp) {
    }

    int mVtBufferFd;
    // the buffer expected present time
    int64_t mTimeStamp;
};

class VtConsumer {
public:
    VtConsumer(int tunnelId);
    virtual ~VtConsumer();

    class VtReleaseListener {
    public:
        virtual ~VtReleaseListener() {};
        virtual int32_t onFrameDisplayed(int bufferFd, int fenceFd);
    };

    class VtContentListener {
    public:
        virtual ~VtContentListener() {};
        // buffer interfaces
        virtual int32_t onFrameAvailable(
                std::vector<VtBufferItem> & items __unused) {return 0;};

        // cmd interfaces
        virtual void onVideoHide() {};
        virtual void onVideoBlank() {};
        virtual void onVideoShow() {};
        virtual void onVideoGameMode(int data __unused) {};
        virtual int32_t getVideoStatus() {return 0;};
        virtual void onSourceCropChange(vt_rect_t & crop __unused) {};
        // need show black or blue frame when video starts playing
        virtual void onNeedShowTempBuffer(int colorType __unused) {};
        virtual void setVideoType(int videoType __unused) {}; // AM_VIDEO_TYPE
    };

    /*register VtInstance callback */
    int32_t setReleaseListener(std::shared_ptr<VtReleaseListener> &listener);
    /* register Hwc2Layer callback */
    int32_t setVtContentListener(std::shared_ptr<VtContentListener> &listener);

    int32_t onVtCmds(vt_cmd & cmd, vt_cmd_data_t & cmdData);
    int32_t onVtFrameDisplayed(int bufferFd, int fenceFd);

private:
    int mTunnelId;
    /* VtInstance callback */
    std::shared_ptr<VtReleaseListener> mReleaseListener;
    /* Hwc2Layer callback */
    std::shared_ptr<VtContentListener> mContentListener;
};

#endif  /* MESON_VT_CONSUMER_H */
