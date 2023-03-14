/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2016/09/06
 *  @par function description:
 *  - 1 process HDCP TX authenticate
 */

#ifndef _HDCP_TX_AUTHENTICATION_H
#define _HDCP_TX_AUTHENTICATION_H

#include <map>
#include <cmath>
#include <string>
#include <pthread.h>
#include <semaphore.h>

#include <mutex>
#include <condition_variable>

#define HDCP_TX_FW_PATH_1         "/odm/etc/firmware/firmware.le"
#define HDCP_TX_FW_PATH_2         "/vendor/etc/firmware/firmware.le"

#define HDCP_TX_AUTH_FAIL         "persist.vendor.hdcptx.auth.fail"

#define DISPLAY_HDMI_HDCP_MODE      "/sys/class/amhdmitx/amhdmitx0/hdcp_mode"
#define DISPLAY_HDMI_AUDIO_MUTE     "/sys/class/amhdmitx/amhdmitx0/aud_mute"
#define DISPLAY_MEDIA_VIDEO_MUTE    "/sys/module/aml_media/parameters/video_mute_on"

enum {
    REPEATER_RX_VERSION_NONE        = 0,
    REPEATER_RX_VERSION_14          = 1,
    REPEATER_RX_VERSION_22          = 2
};

class HDCPTxAuth {
public:
    class HDCPTxAuthCallback {
    public:
        HDCPTxAuthCallback() {};
        virtual ~HDCPTxAuthCallback() {};
        virtual void onHdcpTxAuthEvent (const char* status) = 0;
    };
    HDCPTxAuth();

    ~HDCPTxAuth();

    void setHDCPCallback(HDCPTxAuthCallback *cb);
    void setBootAnimFinished(bool finished);
    void setRepeaterRxVersion(int ver);
    int start();
    int stop();
    void stopVerAll();
    void isAuthSuccess(int *status);
    void AuthResult(bool result);

private:
    bool authInit(bool *pHdcp22, bool *pHdcp14);
    bool authLoop(bool useHdcp22, bool useHdcp14);
    void startVer22();
    void startVer14();
    void mute(bool mute);

    static void* authThread(void* data);

    HDCPTxAuthCallback *pmHDCPTxAuthCallback = NULL;
    int mRepeaterRxVer;

    bool mMute;
    bool mBootAnimFinished;

    pthread_mutex_t pthreadTxMutex;
    sem_t pthreadTxSem;
    pthread_t pthreadIdHdcpTx;
    bool mExitHdcpTxThread;
    bool mFallbackDefault;

    std::mutex mMutex;
    std::condition_variable mCv;
};

#endif // _HDCP_TX_AUTHENTICATION_H
