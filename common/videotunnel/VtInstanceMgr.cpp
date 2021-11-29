/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VtInstanceMgr.h"
#include "VideoTunnelDev.h"

ANDROID_SINGLETON_STATIC_INSTANCE(VtInstanceMgr)

VtInstanceMgr::VtInstanceMgr() {
    mInstances.clear();
}

VtInstanceMgr::~VtInstanceMgr() {
    mInstances.clear();
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
         mInstances.emplace(tunnelId, std::move(ptrInstance));
    } else {
        ptrInstance = instanceIt->second;
    }

    if (ptrInstance.get()) {
        ret = ptrInstance->registerVtConsumer(consumer);
    }

    if (ret >= 0)
        MESON_LOGD("%s, [%d] connect video composer successed",
            __func__, tunnelId);

    return ret;
}

/* call from consumer */
int32_t VtInstanceMgr::disconnectInstance(int tunnelId,
        std::shared_ptr<VtConsumer> & consumer) {
    std::lock_guard<std::mutex> lock(mMutex);
    int ret = -1;
    std::shared_ptr<VtInstance> ptrInstance;

    if (tunnelId < 0 || !consumer.get()) {
        MESON_LOGE("%s, [%d] paramaters invalid",
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

    tryDestroyInstanceLocked(tunnelId);

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

bool VtInstanceMgr::tryDestroyInstanceLocked(int tunnelId) {
    std::shared_ptr<VtInstance> ptrInstance;
    std::map<int, std::shared_ptr<VtInstance>>::iterator it;

    it = mInstances.find(tunnelId);
    if (it != mInstances.end()) {
        ptrInstance = it->second;
        if (ptrInstance->needDestroyThisInstance()) {
            mInstances.erase(tunnelId);
            MESON_LOGD("%s, destory instance %d successed", __func__, tunnelId);
            return true;
        } else {
            MESON_LOGV("%s, instance %d have valid consumer", __func__, tunnelId);
        }
    } else
        MESON_LOGW("%s, cannot find %d instance", __func__, tunnelId);

    return false;
}

int32_t VtInstanceMgr::pollVtCmds() {
    int32_t ret;

    ret = VideoTunnelDev::getInstance().pollCmds();

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
