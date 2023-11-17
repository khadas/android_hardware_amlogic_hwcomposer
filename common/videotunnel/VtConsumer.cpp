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
    mContentListener.reset();
}

int32_t VtConsumer::setReleaseListener(VtReleaseListener *listener) {
    if (!listener)
        MESON_LOGD("[%s] [%s] set release listener is null",
                __func__, mName);

    mReleaseListener = listener;

    return 0;
}

int32_t VtConsumer::setVtContentListener(
        std::shared_ptr<VtContentListener> listener) {
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
            {
                vt_video_status_t statusType = (vt_video_status_t)cmdData.data;
                MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_VIDEO_STATUS %s",
                        __func__, mName, VtVideoStatusToString(statusType));
                mContentListener->onVideoStatus(statusType);
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
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_SOURCE_CROP(%d %d %d %d)",
                    __func__, mName, cmdData.rect.left, cmdData.rect.top,
                    cmdData.rect.right, cmdData.rect.bottom);
            mContentListener->onSourceCropChange(cmdData.rect);
            break;
        case VT_CMD_SET_DISPLAY_FRAME:
            MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_DISPLAY_FRAME(%d %d %d %d)",
                    __func__, mName, cmdData.rect.left, cmdData.rect.top,
                    cmdData.rect.right, cmdData.rect.bottom);
            mContentListener->onDisplayFrameChange(cmdData.rect);
            break;
        case VT_CMD_SET_SHOW_SOLID_COLOR:
            {
                vt_video_color_t colorType = (vt_video_color_t)cmdData.data;
                MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_SHOW_SOLID_COLOR %s",
                        __func__, mName, VtSolidColorToString(colorType));
                mContentListener->onNeedShowTempBuffer(colorType);
            }
            break;
        case VT_CMD_SET_COLOR_BLACK:
            {
                vt_video_status_t status = (vt_video_status_t)cmdData.data;
                MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_COLOR_BLACK %s",
                        __func__, mName, VtVideoStatusToString(status));
                mContentListener->onNeedShowTempBufferWithStatus(
                        SOLID_COLOR_FOR_BLACK, status);
            }
            break;
        case VT_CMD_SET_COLOR_BLUE:
            {
                vt_video_status_t status = (vt_video_status_t)cmdData.data;
                MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_COLOR_BLUE %s",
                        __func__, mName, VtVideoStatusToString(status));
                mContentListener->onNeedShowTempBufferWithStatus(
                        SOLID_COLOR_FOR_BLUE, status);
            }
            break;
        case VT_CMD_SET_COLOR_GREEN:
            {
                vt_video_status_t status = (vt_video_status_t)cmdData.data;
                MESON_LOGD("[%s] [%s] received cmd VT_CMD_SET_COLOR_GREEN %s",
                        __func__, mName, VtVideoStatusToString(status));
                mContentListener->onNeedShowTempBufferWithStatus(
                        SOLID_COLOR_FOR_GREEN, status);
            }
            break;
        default:
            MESON_LOGW("[%s] [%s] received an invalid CMD %d",
                    __func__, mName, (int)cmd);
            return -1;
    }

    return 0;
}

int32_t VtConsumer::onVtFrameDisplayed(int bufferFd, int fenceFd) {
    if (mReleaseListener) {
        return mReleaseListener->onFrameDisplayed(bufferFd, fenceFd);
    } else {
        return -1;
    }
}

int32_t VtConsumer::onFrameAvailable(
        std::vector<std::shared_ptr<VtBufferItem>> & items) {
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

const char * VtConsumer::VtSolidColorToString(vt_video_color_t type) {
    const char * typeStr;
    switch (type) {
        case SOLID_COLOR_FOR_BLACK :
            typeStr = "black";
            break;
        case SOLID_COLOR_FOR_GREEN:
            typeStr = "green";
            break;
        case SOLID_COLOR_FOR_BLUE:
            typeStr = "blue";
            break;
        default:
            typeStr = "unknown";
            break;
    }

    return typeStr;
}

const char * VtConsumer::VtVideoStatusToString(vt_video_status_t type) {
    const char * typeStr;
    switch (type) {
        case VT_VIDEO_STATUS_BLANK:
            typeStr = "blank";
            break;
        case VT_VIDEO_STATUS_HIDE:
            typeStr = "hide";
            break;
        case VT_VIDEO_STATUS_SHOW:
            typeStr = "show";
            break;
        case VT_VIDEO_STATUS_COLOR_ONCE:
            typeStr = "once";
            break;
        case VT_VIDEO_STATUS_COLOR_ALWAYS:
            typeStr = "always";
            break;
        case VT_VIDEO_STATUS_COLOR_DISABLE:
            typeStr = "disable";
            break;
        case VT_VIDEO_STATUS_HOLD_FRAME:
            typeStr = "freeze";
            break;
        default:
            typeStr = "unknown";
            break;
    }

    return typeStr;
}

