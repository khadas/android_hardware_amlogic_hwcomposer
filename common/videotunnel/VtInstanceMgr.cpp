/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include <unistd.h>

#include "VtInstanceMgr.h"
#include "VideoTunnelDev.h"

ANDROID_SINGLETON_STATIC_INSTANCE(VtInstanceMgr)

VtInstanceMgr::VtInstanceMgr() {
    mInstances.clear();
    mVtHandleEventsThread = nullptr;
}

VtInstanceMgr::~VtInstanceMgr() {
    mInstances.clear();
    if (mVtHandleEventsThread)
        mVtHandleEventsThread->stopThread();
}

/* call from consumer */
int32_t VtInstanceMgr::connectInstance(int tunnelId,
        std::shared_ptr<VtConsumer> & consumer) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::shared_ptr<VtInstance> ptrInstance;
    int32_t ret = -1;
    if (tunnelId < 0) {
        MESON_LOGD("%s, get an invalid parameter %d", __func__, tunnelId);
        return ret;
    }

    auto instanceIt = mInstances.find(tunnelId);
    if (instanceIt == mInstances.end()) {
         ptrInstance = std::make_shared<VtInstance> (tunnelId);
         ret = ptrInstance->connect();
         if (ret < 0) {
            ptrInstance.reset();
            MESON_LOGE("%s, [%d] connect video composer failed: %d",
                __func__, tunnelId, ret);
            return ret;
         }
         mInstances.emplace(tunnelId, ptrInstance);
    } else {
        ptrInstance = instanceIt->second;
    }

    if (ptrInstance.get()) {
        ret = ptrInstance->registerVtConsumer(consumer);
    }

    if (ret >= 0)
        MESON_LOGD("%s, [%d] connect video composer successed",
            __func__, tunnelId);

    if (!mVtHandleEventsThread) {
        mVtHandleEventsThread = std::make_shared<VtHandleEventsThread>();
    }
    return ret;
}

/* call from consumer */
int32_t VtInstanceMgr::disconnectInstance(int tunnelId,
        std::shared_ptr<VtConsumer> & consumer) {
    int ret = -1;
    bool need_distroy_thread = false;
    std::shared_ptr<VtInstance> ptrInstance;

    if (tunnelId < 0 || !consumer.get()) {
        MESON_LOGE("%s, [%d] paramaters invalid",
            __func__, tunnelId);
        return ret;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mInstances.empty()) {
            MESON_LOGW("%s, [%d] currently not found instance",
                __func__, tunnelId);
            return ret;
        }

        auto instanceIt = mInstances.find(tunnelId);
        if (instanceIt != mInstances.end()) {
            ptrInstance = instanceIt->second;
            ret = ptrInstance->unregisterVtConsumer(consumer);
            if (ret < 0)
                MESON_LOGE("%s, [%d] unregister consumer failed: %d",
                    __func__, tunnelId, ret);
            if (ptrInstance->needDestroyThisInstance()) {
                MESON_LOGD("%s, destory instance %d successed", __func__, tunnelId);
                mInstances.erase(tunnelId);
            }
        }

        if (mInstances.empty() && mVtHandleEventsThread)
            need_distroy_thread = true;
    }

    if (need_distroy_thread) {
        MESON_LOGD("%s, all the instances are released,  stop handEventsThread", __func__);
        mVtHandleEventsThread->stopThread();
        mVtHandleEventsThread.reset();
        mVtHandleEventsThread = nullptr;
    }

    return ret;
}

void VtInstanceMgr::clearUpInstances() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool bRemove = false;
    int id;
    std::shared_ptr<VtInstance> ptrInstance;
    auto it = mInstances.begin();

    for (; it != mInstances.end();) {
        id = it->first;
        ptrInstance = it->second;
        bRemove = ptrInstance->needDestroyThisInstance();
        if (bRemove) {
            it = mInstances.erase(it);
            MESON_LOGD("%s, destroy instance %d successed", __func__, id);
        } else {
            ++it;
        }
    }
}

VideoTunnelDev::VtPollStatus VtInstanceMgr::pollVtEvents() {
    VideoTunnelDev::VtPollStatus ret;

    ret = VideoTunnelDev::getInstance().pollBufferAndCmds();

    return ret;
}

int32_t VtInstanceMgr::handleBuffers() {
    std::lock_guard<std::mutex> lock(mMutex);
    int32_t ret = 0;
    std::shared_ptr<VtInstance> ptrInstance;
    auto instanceIt = mInstances.begin();

    for (; instanceIt != mInstances.end(); instanceIt++) {
        ptrInstance = instanceIt->second;
        ret = ptrInstance->acquireBuffer();
    }

    return ret;
}

int32_t VtInstanceMgr::handleCmds() {
    std::lock_guard<std::mutex> lock(mMutex);
    int32_t ret = 0;
    std::shared_ptr<VtInstance> ptrInstance;
    auto instanceIt = mInstances.begin();

    for (; instanceIt != mInstances.end(); instanceIt++) {
        ptrInstance = instanceIt->second;
        ret = ptrInstance->recieveCmds();
    }

    return ret;
}
