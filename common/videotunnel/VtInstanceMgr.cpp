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

/* call from consumer, need hold mInstanceMutex before call*/
int32_t VtInstanceMgr::connectInstanceLocked(int tunnelId,
        std::shared_ptr<VtConsumer> consumer) {
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
            MESON_LOGE("%s, [%d] connect video tunnel failed: %d",
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
        MESON_LOGD("%s, [%d] connect video tunnel succeeded",
            __func__, tunnelId);

    if (!mVtHandleEventsThread)
        mVtHandleEventsThread = std::make_shared<VtHandleEventsThread>();
    else
        mVtHandleEventsThread->startThread();

    return ret;
}

/* call from consumer, need hold mInstanceMutex before call */
int32_t VtInstanceMgr::disconnectInstanceLocked(int tunnelId,
        std::shared_ptr<VtConsumer> consumer) {
    int ret = -1;
    std::shared_ptr<VtInstance> ptrInstance;

    if (tunnelId < 0 || !consumer.get()) {
        MESON_LOGE("%s, [%d] parameters invalid",
            __func__, tunnelId);
        return ret;
    }

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
    }

    clearUpInstancesLocked();
    return ret;
}

void VtInstanceMgr::clearUpInstances() {
    std::lock_guard<std::mutex> lock(mInstanceMutex);
    clearUpInstancesLocked();
}

void VtInstanceMgr::clearUpInstancesLocked() {
    bool bRemove = false;
    int id;
    std::shared_ptr<VtInstance> ptrInstance;

    if (mInstances.empty())
        return;

    auto it = mInstances.begin();
    for (; it != mInstances.end();) {
        id = it->first;
        ptrInstance = it->second;
        bRemove = ptrInstance->needDestroyThisInstance();
        if (bRemove) {
            ptrInstance->disconnect();
            it = mInstances.erase(it);
            MESON_LOGD("%s, destroy instance %d succeeded",
                    __func__, id);
        } else {
            ++it;
        }
    }
}

const char * VtInstanceMgr::vtPollStatusToString(VideoTunnelDev::VtPollStatus status) {
    const char * str;
    switch (status) {
        case VideoTunnelDev::VtPollStatus::eBufferReady:
            str = "VT Buffer Ready";
            break;
        case VideoTunnelDev::VtPollStatus::eCmdReady:
            str = "VT Cmd Ready";
            break;
        case VideoTunnelDev::VtPollStatus::eNotReady:
            str = "VT Not Ready";
            break;
        default:
            str = "Status is not found";
    }
    return str;
}

VideoTunnelDev::VtPollStatus VtInstanceMgr::pollVtEvents() {
    VideoTunnelDev::VtPollStatus ret;
    ret = VideoTunnelDev::VtPollStatus::eInvalidStatus;

    clearUpInstances();
    if (mInstances.empty()) {
        /* will exit VtHandleEventsThread */
        return ret;
    }

    ret = VideoTunnelDev::getInstance().pollBufferAndCmds();
    MESON_LOGV("VtInstanceMgr::%s %s",
            __func__, vtPollStatusToString(ret));

    return ret;
}

int32_t VtInstanceMgr::handleBuffers() {
    int32_t ret = -1;
    std::shared_ptr<VtInstance> ptrInstance;

    std::lock_guard<std::mutex> lock(mInstanceMutex);
    clearUpInstancesLocked();
    if (mInstances.empty()) {
        /* will exit VtHandleEventsThread */
        return ret;
    }

    auto instanceIt = mInstances.begin();
    for (; instanceIt != mInstances.end(); instanceIt++) {
        ptrInstance = instanceIt->second;
        ret = ptrInstance->acquireBuffer();
    }

    return ret;
}

int32_t VtInstanceMgr::handleCmds() {
    int32_t ret = -1;
    std::shared_ptr<VtInstance> ptrInstance;

    std::lock_guard<std::mutex> lock(mInstanceMutex);
    clearUpInstancesLocked();
    if (mInstances.empty()) {
        /* will exit VtHandleEventsThread */
        return ret;
    }

    auto instanceIt = mInstances.begin();
    for (; instanceIt != mInstances.end(); instanceIt++) {
        ptrInstance = instanceIt->second;
        ret = ptrInstance->receiveCmds();
    }

    return ret;
}

void VtInstanceMgr::lockInstancesMutex() {
    mInstanceMutex.lock();
}

void VtInstanceMgr::unlockInstancesMutex() {
    mInstanceMutex.unlock();
}
