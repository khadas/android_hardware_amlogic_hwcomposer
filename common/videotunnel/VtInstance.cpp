/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "VtInstance.h"
#include "VideoTunnelDev.h"

VtInstance::VtInstance(int tunnelId)
    : mTunnelId(tunnelId) {
    mConsumers.clear();
    mBufferQueue.clear();
}

VtInstance::~VtInstance() {
    mConsumers.clear();
    disconnect();
}

int32_t VtInstance::onFrameDisplayed(int bufferFd, int fenceFd) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::shared_ptr<VtBufferItem> ptrBufferItem;
    if (mBufferQueue.empty()) {
        MESON_LOGE("%s, [%d] buffer queue is empty (%d)",
            __func__, mTunnelId, bufferFd);
        return -1;
    }

    MESON_LOGV("%s, [%d] buffer=%d, fenceFd=%d",
        __func__, mTunnelId, bufferFd, fenceFd);

    for (auto it = mBufferQueue.begin(); it != mBufferQueue.end();) {
        ptrBufferItem = *it;
        if (ptrBufferItem->mVtBufferFd != bufferFd) {
            ++it;
            continue;
        }

        std::shared_ptr<DrmFence> releaseFence = ptrBufferItem->mReleaseFence;
        if (fenceFd < -1) {
            MESON_LOGV("%s, [%d] buffer(%d) have an invalid fenceFd",
                __func__, mTunnelId, bufferFd);
        } else if (releaseFence == DrmFence::NO_FENCE ||
            releaseFence->getStatus() == DrmFence::Status::Invalid ||
            releaseFence->getStatus() == DrmFence::Status::Signaled) {
            ptrBufferItem->mReleaseFence.reset();
            ptrBufferItem->mReleaseFence = std::make_shared<DrmFence>(fenceFd);
        } else if (releaseFence->getStatus() == DrmFence::Status::Unsignaled) {
            ptrBufferItem->mReleaseFence = DrmFence::merge("VtBufferFd",
                releaseFence, std::make_shared<DrmFence>(fenceFd));
        } else {
            // nothing to do;
        }

        if (ptrBufferItem->unrefHandle()) {
            it = mBufferQueue.erase(it);
            releaseBufferLocked(ptrBufferItem);
        }

        return 0;
    }

    MESON_LOGE("%s, [%d] cannot found %d in buffer queue",
            __func__, mTunnelId, bufferFd);
    return -1;
}

int32_t VtInstance::registerVtConsumer(std::shared_ptr<VtConsumer> & consumer) {
    /* only Hwc2Layer will call this api when have an videotunnel layer create */
    if (!consumer) {
        MESON_LOGE("%s, [%d] consumer is null", __func__, mTunnelId);
        return -1;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    mConsumers.push_back(consumer);
    return 0;
}

int32_t VtInstance::unregisterVtConsumer(std::shared_ptr<VtConsumer> & consumer) {
    /* only Hwc2Layer will call this api when videotunnel layer destroy.
     * and Hwc2Layer will release all buffer Fd before call this api
     */
    int32_t ret = -1;

    if (!consumer) {
        MESON_LOGE("%s, [%d] consumer is null", __func__, mTunnelId);
        return ret;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mConsumers.begin();
    for (; it != mConsumers.end(); it++) {
        std::shared_ptr<VtConsumer> item = (*it);
        if (item.get() == consumer.get()) {
            MESON_LOGV("%s, [%d] remove consumer %p", __func__, mTunnelId, consumer.get());
            mConsumers.erase(it);
            ret = 0;
            break;
        }
    }

    if (it == mConsumers.end())
        MESON_LOGW("%s, [%d] cannot found VT consumer %p",
        __func__, mTunnelId, consumer.get());

    return ret;
}

int32_t VtInstance::connect() {
    int32_t ret = -1;
    if (mTunnelId >= 0) {
        ret = VideoTunnelDev::getInstance().connect(mTunnelId);
        if (ret >= 0) {
            MESON_LOGD("%s [%d] connect to videotunnel successed", __func__, mTunnelId);
        } else {
                MESON_LOGE("%s [%d] connect to videotunnel filed", __func__, mTunnelId);
        }
    } else {
        MESON_LOGE("%s, [%d] tunnel id is invalid", __func__, mTunnelId);
    }
    return ret;
}

void VtInstance::disconnect() {
    if (mTunnelId >= 0)
        VideoTunnelDev::getInstance().disconnect(mTunnelId);
    mTunnelId = -1;
}

int32_t VtInstance::acquireBuffer() {
    std::lock_guard<std::mutex> lock(mMutex);
    int32_t ret = 0;
    int bufFd = -1;
    int64_t timeStamp = -1;
    std::vector<std::shared_ptr<VtBufferItem>> items;

    while (true) {
        ret = VideoTunnelDev::getInstance().acquireBuffer(mTunnelId, bufFd, timeStamp);

        if (ret == 0) {
            std::shared_ptr<VtBufferItem> item =
                std::make_shared<VtBufferItem> (bufFd, timeStamp);
            items.push_back(item);
            mBufferQueue.push_back(item);
        } else if (ret == -EAGAIN) {
            /* no buffer available */
            break;
        } else {
            MESON_LOGE("%s, [%d] acquire buffer error %d", __func__, mTunnelId, ret);
            return ret;
        }
    }

    if (!items.empty()) {
        for (auto it = mConsumers.begin(); it != mConsumers.end(); it++) {
            std::shared_ptr<VtConsumer> consumer = *it;
            ret = consumer->onFrameAvailable(items);
            if (ret >= 0) {
                for (auto item = items.begin(); item != items.end(); item++) {
                    (*item)->refHandle();
                }
            } else {
                MESON_LOGE("%s, [%d] call consumer(%p) onFrameAvailable failed",
                    __func__, mTunnelId, consumer.get());
            }
        }
    }

    return ret;
}

void VtInstance::releaseBufferLocked(std::shared_ptr<VtBufferItem> & item) {
    if (item.get()) {
        MESON_LOGV("%s, [%d] release buffer %d",
            __func__, mTunnelId, item->mVtBufferFd);
        VideoTunnelDev::getInstance().releaseBuffer(
            mTunnelId, item->mVtBufferFd, item->mReleaseFence->dup());
        item->mReleaseFence.reset();
    }
}

void VtInstance::releaseBuffers() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mBufferQueue.empty()) {
        MESON_LOGE("%s, [%d] buffer queue is empty",
            __func__, mTunnelId);
        return;
    }

    for (auto it = mBufferQueue.begin(); it != mBufferQueue.end();) {
        std::shared_ptr<VtBufferItem> item = *it;
        if (item->needReleaseBufferFd()) {
            releaseBufferLocked(item);
            it = mBufferQueue.erase(it);
        } else {
            ++it;
        }
    }
}

int32_t VtInstance::recieveCmds() {
    int32_t ret;
    enum vt_cmd cmd;
    struct vt_cmd_data cmdData;

    ret = VideoTunnelDev::getInstance().recieveCmd(mTunnelId, cmd, cmdData);
    if (ret < 0)
        return ret;

    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mConsumers.begin(); it != mConsumers.end(); it++) {
        ret = (*it)->onVtCmds(cmd, cmdData);
    }

    return ret;
}

bool VtInstance::needDestroyThisInstance() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mConsumers.empty())
        return true;
    else
        return false;
}
