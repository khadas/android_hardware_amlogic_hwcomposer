/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <poll.h>
#include <string.h>
#include <cutils/uevent.h>
#include <MesonLog.h>

#include "HwDisplayEventListener.h"


ANDROID_SINGLETON_STATIC_INSTANCE(HwDisplayEventListener)


#define HDMITX_HOTPLUG_EVENT \
    "change@/devices/virtual/amhdmitx/amhdmitx0/hdmi"
#define HDMITX_HDCP_EVENT \
    "change@/devices/virtual/amhdmitx/amhdmitx0/hdcp"
#define VOUT_MODE_EVENT \
    "change@/devices/platform/vout/extcon/setmode"

#define UEVENT_MAX_LEN (4096)

HwDisplayEventListener::HwDisplayEventListener()
    :   mUeventMsg(NULL),
        mCtlInFd(-1),
        mCtlOutFd(-1) {
    /*init uevent socket.*/
    mEventSocket = uevent_open_socket(64*1024, true);
    if (mEventSocket < 0) {
        MESON_LOGE("uevent_init: uevent_open_socket failed\n");
        return;
    }

    /*make pipe fd for exit uevent thread.*/
    int ctlPipe[2];
    if (pipe(ctlPipe) < 0) {
        MESON_LOGE("make control pipe fail.\n");
        return;
    }
    mCtlInFd = ctlPipe[0];
    mCtlOutFd = ctlPipe[1];

    mUeventMsg = new char[UEVENT_MAX_LEN];
    memset(mUeventMsg, 0, UEVENT_MAX_LEN);

    /*load uevent parser*/
    mUeventParser.emplace(drm_event_hdmitx_hotplug, std::string(HDMITX_HOTPLUG_EVENT));
    mUeventParser.emplace(drm_event_hdmitx_hdcp, std::string(HDMITX_HDCP_EVENT));
    mUeventParser.emplace(drm_event_mode_changed, std::string(VOUT_MODE_EVENT));

    createThread();
}

HwDisplayEventListener::~HwDisplayEventListener() {
    if (mCtlInFd >= 0) {
        close(mCtlInFd);
        mCtlInFd = -1;
    }

//    requestExitAndWait();

    if (mEventSocket >= 0) {
        close(mEventSocket);
        mEventSocket = -1;
    }

    if (mCtlOutFd >= 0) {
        close(mCtlOutFd);
        mCtlOutFd = -1;
    }

    delete mUeventMsg;

    mUeventParser.clear();
    mEventHandler.clear();
}

void HwDisplayEventListener::createThread() {
    int ret;
    ret = pthread_create(&hw_event_thread, NULL, ueventThread, this);
    if (ret) {
        MESON_LOGE("failed to start uevent thread: %s", strerror(ret));
        return;
    }

    ret = pthread_create(&hw_virtual_hotplug_thread, NULL, virtualHotplugThread, this);
    if (ret) {
        MESON_LOGE("failed to start virtual hotplug thread: %s", strerror(ret));
        return;
    }
}

void HwDisplayEventListener::handleUevent() {
    std::map<drm_display_event, std::string>::iterator it = mUeventParser.begin();
    for (; it!= mUeventParser.end(); ++it) {
        if (memcmp(mUeventMsg, it->second.c_str(), it->second.length()) == 0) {
            String8 key;
            int val;
            char *msg = mUeventMsg;
            while (*msg) {
                key = String8(msg);
                MESON_LOGD("received Uevent: %s", msg);
                if (key.contains("STATE=ACA=1")) {
                     val =  1;
                } else if (key.contains("STATE=ACA=0")) {
                     val =  0;
                } else if (key.contains("STATE=HDMI=1")) {
                     val =  1;
                } else if (key.contains("STATE=HDMI=0")) {
                     val =  0;
                }
                msg += strlen(msg) + 1;
            }
            drm_display_event event = it->first;
            MESON_LOGD("parse event %d, val %d", event, val);

            std::multimap<drm_display_event, HwDisplayEventHandler *>::iterator it;
            for (it = mEventHandler.begin(); it != mEventHandler.end(); it++) {
                if (it->first == event || it->first == drm_event_any)
                    it->second->handle(event, val);
            }
        }
    }

}

void * HwDisplayEventListener::ueventThread(void * data) {
    HwDisplayEventListener* pThis = (HwDisplayEventListener*)data;
    while (true) {
        pthread_mutex_lock(&hwc_mutex);

        int rtn;
        struct pollfd fds[2];

        fds[0].fd = pThis->mEventSocket;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = pThis->mCtlOutFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        rtn = poll(fds, 2, -1);

        if (rtn > 0 && fds[0].revents == POLLIN) {
            ssize_t len = uevent_kernel_multicast_recv(pThis->mEventSocket,
                pThis->mUeventMsg, UEVENT_MAX_LEN - 2);
            if (len > 0)
                pThis->handleUevent();
        } else if (fds[1].revents) {
            MESON_LOGE("exit display event thread.");
            return NULL;
        }

        pthread_mutex_unlock(&hwc_mutex);
    }
    return NULL;
}

void * HwDisplayEventListener::virtualHotplugThread(void * data) {
    HwDisplayEventListener* pThis = (HwDisplayEventListener*)data;
    MESON_LOGV("Virtual hotplug thread start.");

    int val = 0;
    drm_display_event event = drm_event_hdmitx_hotplug;
    std::multimap<drm_display_event, HwDisplayEventHandler *>::iterator it;
    for (it = pThis->mEventHandler.begin(); it != pThis->mEventHandler.end(); it++) {
        if (it->first == event || it->first == drm_event_any)
            it->second->handle(event, val);
    }

    event = drm_event_mode_changed;
    val = 0;
    for (it = pThis->mEventHandler.begin(); it != pThis->mEventHandler.end(); it++) {
        if (it->first == event || it->first == drm_event_any)
            it->second->handle(event, val);
    }
    return NULL;
}

int32_t HwDisplayEventListener::registerHandler(
    drm_display_event event, HwDisplayEventHandler * handler) {
    std::multimap<drm_display_event, HwDisplayEventHandler* >::iterator it;
    switch (event) {
        case drm_event_hdmitx_hotplug:
        case drm_event_hdmitx_hdcp:
        case drm_event_mode_changed:
        case drm_event_any:
            mEventHandler.insert(std::make_pair(event, handler));
            return 0;
        default:
            return -ENOENT ;
    };
}

