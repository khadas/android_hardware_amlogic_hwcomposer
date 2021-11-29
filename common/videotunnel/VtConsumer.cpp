/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "VtConsumer.h"

VtConsumer::VtConsumer(int tunnelId) {
    mTunnelId = tunnelId;
    mReleaseListener = NULL;
    mContentListener = NULL;
}

VtConsumer::~VtConsumer() {
    if (mReleaseListener.get())
        mReleaseListener.reset();

    if (mContentListener.get())
        mContentListener.reset();
}

int32_t VtConsumer::setReleaseListener(
        std::shared_ptr<VtReleaseListener> &listener) {
    if (!listener.get()) {
        MESON_LOGE("%s, [%d] set release listener is null",
                __func__, mTunnelId);
        return -1;
    }

    if (mReleaseListener.get())
        mReleaseListener.reset();

    mReleaseListener = listener;

    return 0;
}

int32_t VtConsumer::setVtContentListener(
        std::shared_ptr<VtContentListener> &listener) {
    if (!listener.get()) {
        MESON_LOGE("%s, [%d] set content listener is null",
                __func__, mTunnelId);
        return -1;
    }

    if (mContentListener.get())
        mContentListener.reset();

    mContentListener = listener;

    return 0;
}

int32_t VtConsumer::onVtCmds(vt_cmd_t & cmd, vt_cmd_data_t & cmdData) {
    if (!mContentListener.get()) {
        MESON_LOGE("%s, [%d] not found content listener",
                __func__, mTunnelId);
        return -1;
    }

    switch (cmd) {
        case VT_CMD_SET_VIDEO_STATUS:
            MESON_LOGV("%s, [%d] received VT_CMD_SET_VIDEO_STATUS",
                    __func__, mTunnelId);
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
                    MESON_LOGW("%s, [%d] get an invalid parameter(%d) for "
                            "cmd VT_CMD_SET_VIDEO_STATUS",
                            __func__, mTunnelId, cmdData.data);
            }
            break;
        case VT_CMD_GET_VIDEO_STATUS:
            MESON_LOGV("%s, [%d] received cmd VT_CMD_GET_VIDEO_STATUS",
                    __func__, mTunnelId);
            break;
        case VT_CMD_SET_GAME_MODE:
            MESON_LOGV("%s, [%d] received cmd VT_CMD_SET_GAME_MODE",
                    __func__, mTunnelId);
            mContentListener->onVideoGameMode(cmdData.data);
            break;
        case VT_CMD_SET_SOURCE_CROP:
            MESON_LOGV("%s, [%d] received cmd VT_CMD_SET_VIDEO_STATUS(%d %d %d %d)",
                    __func__, mTunnelId, cmdData.crop.left, cmdData.crop.top,
                    cmdData.crop.right, cmdData.crop.bottom);
            mContentListener->onSourceCropChange(cmdData.crop);
            break;
        case VT_CMD_SET_SOLID_COLOR_BUF:
            MESON_LOGV("%s, [%d] received cmd VT_CMD_SET_SOLID_COLOR_BUF %d",
                    __func__, mTunnelId, cmdData.data);
            mContentListener->onNeedShowTempBuffer(cmdData.data);
            break;
        case VT_CMD_SET_VIDEO_TYPE:
            MESON_LOGV("%s, [%d] received cmd VT_CMD_SET_VIDEO_TYPE",
                    __func__, mTunnelId);
            mContentListener->setVideoType(cmdData.data);
            break;
        default:
            MESON_LOGW("%s, [%d] reveived an invalid CMD %d",
                    __func__, mTunnelId, (int)cmd);
            return -1;
    }

    return 0;
}

int32_t VtConsumer::onVtFrameDisplayed(int bufferFd, int fenceFd) {
    if (mReleaseListener.get()) {
        return mReleaseListener->onFrameDisplayed(bufferFd, fenceFd);
    } else {
        return -1;
    }
}

int32_t VtConsumer::onFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
    if (mContentListener.get()) {
        return mContentListener->onFrameAvailable(items);
    } else {
        return -1;
    }
}
