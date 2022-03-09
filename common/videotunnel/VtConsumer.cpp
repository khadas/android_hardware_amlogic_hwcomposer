/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1

#include "VtConsumer.h"

VtConsumer::VtConsumer(int tunnelId, uint32_t dispId, uint32_t layerId) {
    mTunnelId = tunnelId;
    mReleaseListener = nullptr;
    mContentListener = nullptr;
    mFlags = false;
    snprintf(mName, 64, "VtConsumer-%u-%d-%u",
            dispId, tunnelId, layerId);
}

VtConsumer::~VtConsumer() {
}

int32_t VtConsumer::setReleaseListener(VtReleaseListener *listener) {
    if (!listener) {
        MESON_LOGE("[%s] [%s] set release listener is null",
                __func__, mName);
        return -1;
    }

    mReleaseListener = listener;

    return 0;
}

int32_t VtConsumer::setVtContentListener(
        std::shared_ptr<VtContentListener> &listener) {
    if (!listener || !listener.get()) {
        MESON_LOGE("[%s] [%s] set content listener is null",
                __func__, mName);
        return -1;
    }

    if (mContentListener && mContentListener.get()) {
        mContentListener.reset();
    }

    mContentListener = listener;

    return 0;
}

int32_t VtConsumer::onVtCmds(vt_cmd_t & cmd, vt_cmd_data_t & cmdData) {
    if (!mContentListener || !mContentListener.get()) {
        MESON_LOGE("[%s] [%s] not found content listener", __func__, mName);
        return -1;
    }

    switch (cmd) {
        case VT_CMD_SET_VIDEO_STATUS:
            MESON_LOGD("[%s] [%s] received VT_CMD_SET_VIDEO_STATUS %d",
                    __func__, mName, cmdData.data);
            switch (cmdData.data) {
                case 0:
                    mContentListener->onVideoHide();
                    break;
                case 1:
                    mContentListener->onVideoBlank();
                    break;
                case 2:
                    mContentListener->onVideoShow();
                    break;
                default:
                    MESON_LOGW("[%s] [%s] get an invalid parameter(%d) for "
                            "cmd VT_CMD_SET_VIDEO_STATUS",
                            __func__, mName, cmdData.data);
            }
            break;
        case VT_CMD_GET_VIDEO_STATUS:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_GET_VIDEO_STATUS",
                    __func__, mName);
            break;
        case VT_CMD_SET_GAME_MODE:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_GAME_MODE %d",
                    __func__, mName, cmdData.data);
            mContentListener->onVideoGameMode(cmdData.data);
            break;
        case VT_CMD_SET_SOURCE_CROP:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_VIDEO_STATUS(%d %d %d %d)",
                    __func__, mName, cmdData.crop.left, cmdData.crop.top,
                    cmdData.crop.right, cmdData.crop.bottom);
            mContentListener->onSourceCropChange(cmdData.crop);
            break;
        case VT_CMD_SET_SHOW_SOLID_COLOR:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_SHOW_SOLID_COLOR %d",
                    __func__, mName, cmdData.data);
            mContentListener->onNeedShowTempBuffer(cmdData.data);
            break;
        case VT_CMD_SET_VIDEO_TYPE:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_VIDEO_TYPE",
                    __func__, mName);
            mContentListener->setVideoType(cmdData.data);
            break;
        default:
            MESON_LOGW("[%s] [%s] reveived an invalid CMD %d",
                    __func__, mName, (int)cmd);
            return -1;
    }

    return 0;
}

int32_t VtConsumer::onVtFrameDisplayed(int bufferFd, int fenceFd) {
    MESON_LOGV("[%s] [%s] bufferFd=%d, fenceFd=%d",
            __func__, mName, bufferFd, fenceFd);
    if (mReleaseListener) {
        return mReleaseListener->onFrameDisplayed(bufferFd, fenceFd);
    } else {
        return -1;
    }
}

int32_t VtConsumer::onFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
    MESON_LOGV("[%s] [%s] items.size=%d",
            __func__, mName, items.size());
    if (mContentListener && mContentListener.get()) {
        return mContentListener->onFrameAvailable(items);
    } else {
        return -1;
    }
}

void VtConsumer::setDestroyFlag() {
    std::lock_guard<std::mutex> lock(mMutex);
    /* need destroy this consumer */
    mFlags = true;
}

bool VtConsumer::getDestroyFlag() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mFlags;
}
