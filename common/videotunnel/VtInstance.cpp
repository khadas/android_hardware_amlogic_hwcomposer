/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1

#include "VtInstance.h"
#include "VideoTunnelDev.h"

VtInstance::VtInstance(int tunnelId)
    : mTunnelId(tunnelId) {
    mConsumers.clear();
    mBufferQueue.clear();
    snprintf(mName, 64, "VtInstance-%d", tunnelId);
}

VtInstance::~VtInstance() {
    mConsumers.clear();
    releaseInstanceBuffers();
    disconnect();
}

int32_t VtInstance::onFrameDisplayed(int bufferFd, int fenceFd) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::shared_ptr<VtBufferItem> ptrBufferItem;
    if (mBufferQueue.empty()) {
        MESON_LOGE("[%s] [%s] buffer queue is empty (%d)",
            __func__, mName, bufferFd);
        return -1;
    }

    MESON_LOGV("[%s] [%s] buffer=%d, fenceFd=%d",
        __func__, mName, bufferFd, fenceFd);

    for (auto it = mBufferQueue.begin(); it != mBufferQueue.end();) {
        ptrBufferItem = *it;
        if (ptrBufferItem->mVtBufferFd != bufferFd) {
            ++it;
            continue;
        }

        std::shared_ptr<DrmFence> releaseFence = ptrBufferItem->mReleaseFence;
        if (fenceFd < -1) {
            MESON_LOGV("[%s] [%s] buffer=%d have an invalid fenceFd",
                __func__, mName, bufferFd);
        } else if (releaseFence == DrmFence::NO_FENCE ||
            releaseFence->getStatus() == DrmFence::Status::Invalid ||
            releaseFence->getStatus() == DrmFence::Status::Signaled) {
            ptrBufferItem->mReleaseFence.reset(new DrmFence(fenceFd));
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

    MESON_LOGE("[%s] [%s] cannot found %d in buffer queue",
            __func__, mName, bufferFd);
    return -1;
}

int32_t VtInstance::registerVtConsumer(
        std::shared_ptr<VtConsumer> & consumer) {
    /* only Hwc2Layer will call this api when have
     * an videotunnel layer create
     */
    if (!consumer) {
        MESON_LOGE("[%s] [%s] consumer is null", __func__, mName);
        return -1;
    }
    consumer->setReleaseListener(this);
    mConsumers.push_back(consumer);
    return 0;
}

int32_t VtInstance::unregisterVtConsumer(
        std::shared_ptr<VtConsumer> & consumer) {
    /* only Hwc2Layer will call this api when videotunnel layer destroy.
     * and Hwc2Layer will release all buffer Fd before call this api
     */
    int32_t ret = -1;

    if (!consumer) {
        MESON_LOGE("[%s] [%s] consumer is null", __func__, mName);
        return ret;
    }

    auto it = mConsumers.begin();
    for (; it != mConsumers.end(); it++) {
        std::shared_ptr<VtConsumer> item = (*it);
        if (item.get() == consumer.get()) {
            MESON_LOGV("[%s] [%s] remove consumer %p", __func__,
                    mName, consumer.get());
            item->setDestroyFlag();
            ret = 0;
            break;
        }
    }

    if (ret)
        MESON_LOGW("[%s] [%s] cannot found VT consumer %p",
        __func__, mName, consumer.get());

    return ret;
}

int32_t VtInstance::connect() {
    int32_t ret = -1;
    if (mTunnelId >= 0) {
        ret = VideoTunnelDev::getInstance().connect(mTunnelId);
        if (ret >= 0) {
            MESON_LOGD("[%s] [%s] connect to videotunnel successed",
                    __func__, mName);
        } else {
                MESON_LOGE("[%s] [%s] connect to videotunnel filed",
                        __func__, mName);
        }
    } else {
        MESON_LOGE("[%s] [%s] tunnel id is invalid", __func__, mName);
    }
    return ret;
}

void VtInstance::disconnect() {
    MESON_LOGD("[%s] [%s] disconnect videotunnel",
            __func__, mName);
    if (mTunnelId >= 0)
        VideoTunnelDev::getInstance().disconnect(mTunnelId);
    mTunnelId = -1;
}

int32_t VtInstance::acquireBuffer() {
    int32_t ret = 0;
    int bufFd = -1;
    int64_t timeStamp = -1;
    std::vector<std::shared_ptr<VtBufferItem>> items;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        while (true) {
            ret = VideoTunnelDev::getInstance().acquireBuffer(mTunnelId,
                                                              bufFd,
                                                              timeStamp);

            if (ret == 0) {
                MESON_LOGV("[%s] [%s] acquire buffer %d timeStamp %lld",
                        __func__, mName, bufFd, timeStamp);
                std::shared_ptr<VtBufferItem> item =
                    std::make_shared<VtBufferItem> (bufFd, timeStamp);
                items.push_back(item);
                mBufferQueue.push_back(item);
            } else if (ret == -EAGAIN) {
                /* no buffer available */
                break;
            } else {
                MESON_LOGE("[%s] [%s] acquire buffer error %d",
                        __func__, mName, ret);
                return ret;
            }
        }
    }

    if (!items.empty()) {
        for (auto it = mConsumers.begin(); it != mConsumers.end(); it++) {
            std::shared_ptr<VtConsumer> consumer = *it;
            std::vector<std::shared_ptr<VtBufferItem>>::iterator item;

            for (item = items.begin(); item != items.end(); item++)
                (*item)->refHandle();

            ret = consumer->onFrameAvailable(items);
            if (ret < 0 && ret != -EAGAIN) {
                MESON_LOGE("[%s] [%s] call consumer(%p) onFrameAvailable failed",
                    __func__, mName, consumer.get());
                consumer->setDestroyFlag();
                for (item = items.begin(); item != items.end(); item++)
                    (*item)->unrefHandle();
                ret = -EAGAIN;
            }
        }
    }

    return ret;
}

void VtInstance::releaseBufferLocked(std::shared_ptr<VtBufferItem> & item) {
    if (item.get()) {
        MESON_LOGV("[%s] [%s] release buffer %d",
            __func__, mName, item->mVtBufferFd);
        VideoTunnelDev::getInstance().releaseBuffer(
            mTunnelId, item->mVtBufferFd, item->mReleaseFence->dup());
        item->mReleaseFence.reset();
    }
}

void VtInstance::releaseInstanceBuffers() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mBufferQueue.empty()) {
        MESON_LOGE("[%s] [%s] buffer queue is empty",
            __func__, mName);
        return;
    }

    for (auto it = mBufferQueue.begin(); it != mBufferQueue.end(); ++it) {
        std::shared_ptr<VtBufferItem> item = *it;
        MESON_LOGV("[%s] [%s] release buffer %d",
                 __func__, mName, item->mVtBufferFd);
        VideoTunnelDev::getInstance().releaseBuffer(
                mTunnelId, item->mVtBufferFd, -1);
        item->mReleaseFence.reset();
    }
    mBufferQueue.clear();
}

int32_t VtInstance::recieveCmds() {
    int32_t ret;
    enum vt_cmd cmd;
    struct vt_cmd_data cmdData;

    ret = VideoTunnelDev::getInstance().recieveCmd(mTunnelId, cmd, cmdData);
    if (ret < 0)
        return ret;

    for (auto it = mConsumers.begin(); it != mConsumers.end(); it++) {
        ret = (*it)->onVtCmds(cmd, cmdData);
    }

    return ret;
}

bool VtInstance::needDestroyThisInstance() {
    auto it = mConsumers.begin();
    for (; it != mConsumers.end(); ) {
        if (!(*it).get()) {
            it = mConsumers.erase(it);
        } else {
            if ((*it)->getDestroyFlag())
                it = mConsumers.erase(it);
            else
                it++;
        }
    }

    if (mConsumers.empty())
        return true;
    else
        return false;
}
