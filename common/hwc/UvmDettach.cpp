/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1
#define UVM_DETTACH_HOLD_MAX_BUFFER (8)

#include <MesonLog.h>
#include "UvmDettach.h"
#include "HwcConfig.h"

UvmDettach::UvmDettach(hwc2_layer_t layerId) {
    snprintf(mName, 20, "layer-%d", (int)layerId);
    mUvmBufferQueue.clear();
}

UvmDettach::UvmDettach(int tunnelId) {
    snprintf(mName, 20, "layer-%d", tunnelId);
    mUvmBufferQueue.clear();
}

UvmDettach::~UvmDettach() {
    releaseUvmResource();
}

int32_t UvmDettach::getVideoInfo(struct uvm_fd_info & videoInfo) {
    return UvmDev::getInstance().getVideoInfo(videoInfo);
}

int32_t UvmDettach::attachUvmBuffer(int bufferFd) {
    if (HwcConfig::UvmDettachEnabled()) {
        return UvmDev::getInstance().attachBuffer(bufferFd);
    } else {
        return 0;
    }
}

int32_t UvmDettach::dettachUvmBuffer() {
    if (HwcConfig::UvmDettachEnabled()) {
        int signalCount = 0;
        if (mUvmBufferQueue.size() <= 0)
            return -EAGAIN;

        for (auto it = mUvmBufferQueue.begin(); it != mUvmBufferQueue.end(); it++) {
            auto currentStatus = it->releaseFence->getStatus();
            /* fence was signal */
            if (currentStatus == DrmFence::Status::Invalid ||
                currentStatus == DrmFence::Status::Signaled)
                signalCount ++;
        }

        MESON_LOGV("%s: %s UvmBufferQueue size:%zu, signalCount:%d",
                __func__, mName, mUvmBufferQueue.size(), signalCount);

        while (signalCount > 0) {
            auto item = mUvmBufferQueue.front();
            MESON_LOGV("%s: %s bufferFd:%d, fenceStatus:%d",
                    __func__, mName, item.bufferFd, item.releaseFence->getStatus());

            UvmDev::getInstance().dettachBuffer(item.bufferFd);
            if (item.bufferFd >= 0)
                close(item.bufferFd);

            mUvmBufferQueue.pop_front();

            signalCount --;
        }
    }

    return 0;
}

int32_t UvmDettach::collectUvmBuffer(const int fd, const int fenceFd) {
    if (HwcConfig::UvmDettachEnabled()) {
        if (fd < 0) {
            MESON_LOGV("%s: %s get an invalid fd", __func__, mName);
            if (fenceFd >=0 )
                close(fenceFd);

            return -EINVAL;
        }

        if (fenceFd < 0) {
            MESON_LOGV("%s: %s get an invalid fenceFd", __func__, mName);
            UvmDev::getInstance().dettachBuffer(fd);
            close(fd);
            return 0;
        }

        if (mUvmBufferQueue.size() >= UVM_DETTACH_HOLD_MAX_BUFFER) {
            int count = mUvmBufferQueue.size() - UVM_DETTACH_HOLD_MAX_BUFFER;
            for (int i = 0; i <= count; i++) {
                auto item = mUvmBufferQueue.front();
                if (item.bufferFd >= 0)
                    close(item.bufferFd);

                mUvmBufferQueue.pop_front();
            }
        }

        UvmBuffer item = {fd, std::move(std::make_shared<DrmFence>(fenceFd))};
        mUvmBufferQueue.push_back(item);
    } else {
        if (fd >= 0) {
            close(fd);
        }
        if (fenceFd >= 0) {
            close(fenceFd);
        }
    }

    return 0;
}

int32_t UvmDettach::releaseUvmResource() {
    if (HwcConfig::UvmDettachEnabled()) {
        for (auto it = mUvmBufferQueue.begin(); it != mUvmBufferQueue.end(); it++) {
            if (it->bufferFd >= 0)
                close(it->bufferFd);
        }
        mUvmBufferQueue.clear();
    }

    return 0;
}

