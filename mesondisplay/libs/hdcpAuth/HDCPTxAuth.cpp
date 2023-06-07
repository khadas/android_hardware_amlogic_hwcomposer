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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <chrono>
#include <cutils/properties.h>

#include "HDCPTxAuth.h"
#include "mode_util.h"

#define DISPLAY_HDMI_HDCP_KEY       "/sys/class/amhdmitx/amhdmitx0/hdcp_lstore"
#define DISPLAY_HDMI_HDCP_CONF      "/sys/class/amhdmitx/amhdmitx0/hdcp_ctrl"
#define DISPLAY_HDMI_HDCP_POWER     "/sys/class/amhdmitx/amhdmitx0/hdcp_pwr"
#define DISPLAY_HDMI_VIDEO_MUTE     "/sys/class/amhdmitx/amhdmitx0/vid_mute"
#define DISPLAY_HDMI_AVMUTE_SYSFS   "/sys/devices/virtual/amhdmitx/amhdmitx0/avmute"
#define DISPLAY_HDMI_HDCP_AUTH      "/sys/module/aml_media/parameters/hdmi_authenticated"
#define DISPLAY_HDMI_HDCP_VER       "/sys/class/amhdmitx/amhdmitx0/hdcp_ver"

#define HDMI_TX_AUTH_FAIL               "0"
#define HDMI_TX_AUTH_SUCCESS            "1"

#define DISPLAY_HDMI_HDCP14_STOP        "stop14" //stop HDCP1.4 authenticate
#define DISPLAY_HDMI_HDCP22_STOP        "stop22" //stop HDCP2.2 authenticate
#define DISPLAY_HDMI_HDCP_14            "1"
#define DISPLAY_HDMI_HDCP_22            "2"

HDCPTxAuth::HDCPTxAuth() :
    mRepeaterRxVer(REPEATER_RX_VERSION_NONE),
    mMute(false),
    mBootAnimFinished(false),
    pthreadIdHdcpTx(0),
    mFallbackDefault(false) {

    mExitHdcpTxThread = false;
    int ret;/* initialize an attribute to default value */
    ret = pthread_mutex_init(&pthreadTxMutex, NULL);
    if (ret != 0) {
        SYS_LOGE("pthreadTxMutex init failed\n");
    }

    if (sem_init(&pthreadTxSem, 0, 0) < 0) {
        SYS_LOGE("HDCPTxAuth, sem_init failed\n");
        exit(0);
    }
}

HDCPTxAuth::~HDCPTxAuth() {
    sem_destroy(&pthreadTxSem);
    pthread_mutex_destroy(&pthreadTxMutex);
}

void HDCPTxAuth::setBootAnimFinished(bool finished) {
    mBootAnimFinished = finished;
}

void HDCPTxAuth::setRepeaterRxVersion(int ver) {
    mRepeaterRxVer = ver;
}

void HDCPTxAuth::setHDCPCallback(HDCPTxAuthCallback *cb) {
    pmHDCPTxAuthCallback = cb;
}

//start HDCP TX authenticate
int HDCPTxAuth::start() {
    int ret;
    pthread_t thread_id;

    if (pthread_mutex_trylock(&pthreadTxMutex) != 0) {
        SYS_LOGE("hdcp_tx create thread, Mutex is deadlock\n");
        return -1;
    }

    mExitHdcpTxThread = false;
    ret = pthread_create(&thread_id, NULL, authThread, this);
    if (ret != 0) SYS_LOGE("hdcp_tx, thread create failed\n");

    ret = sem_wait(&pthreadTxSem);
    if (ret < 0) SYS_LOGE("hdcp_tx, sem_wait failed\n");

    pthreadIdHdcpTx = thread_id;
    pthread_mutex_unlock(&pthreadTxMutex);
    SYS_LOGI("hdcp_tx, create hdcp thread id = %lu done\n", thread_id);
    return 0;
}

//stop HDCP TX authenticate
int HDCPTxAuth::stop() {
    void *threadResult;
    int ret = -1;

    stopVerAll();
#ifndef RECOVERY_MODE
    mFallbackDefault = false;
#endif
    if (0 != pthreadIdHdcpTx) {
        mExitHdcpTxThread = true;
        mCv.notify_all();
        if (pthread_mutex_trylock(&pthreadTxMutex) != 0) {
            SYS_LOGE("hdcp_tx exit thread, Mutex is deadlock\n");
            return ret;
        }

        if (0 != pthread_join(pthreadIdHdcpTx, &threadResult)) {
            pthread_mutex_unlock(&pthreadTxMutex);
            SYS_LOGE("hdcp_tx exit thread failed\n");
            return ret;
        }

        pthread_mutex_unlock(&pthreadTxMutex);
        SYS_LOGI("hdcp_tx pthread exit id = %lu, %s  done\n", pthreadIdHdcpTx, (char *)threadResult);
        pthreadIdHdcpTx = 0;
        ret = 0;
    }

    return ret;
}

//Define to force authentication regardless of keys presence
//#define HDCP_AUTHENTICATION_NO_KEYS

void HDCPTxAuth::mute(bool mute __unused) {

#if !defined(HDCP_AUTHENTICATION_NO_KEYS)
    char hdcpTxKey[MESON_MODE_LEN] = {0};
    meson_mode_read_sys(DISPLAY_HDMI_HDCP_KEY, hdcpTxKey, false, sizeof(hdcpTxKey));

    if ((strlen(hdcpTxKey) == 0) || !(strcmp(hdcpTxKey, "00")))
        return;
#endif

#ifdef HDCP_AUTHENTICATION
    if (mute != mMute) {
        //meson_mode_write_sys(DISPLAY_HDMI_AUDIO_MUTE, mute ? "1" : "0");
        meson_mode_write_sys(DISPLAY_HDMI_VIDEO_MUTE, mute ? "1" : "0");
        mMute = mute;
        SYS_LOGI("hdcp_tx mute %s\n", mute ? "on" : "off");
    }
#endif
}

void* HDCPTxAuth::authThread(void* data) {
    bool hdcp22 = false;
    bool hdcp14 = false;
    HDCPTxAuth *pThiz = (HDCPTxAuth*)data;

    SYS_LOGI("hdcp_tx thread loop entry\n");
    sem_post(&pThiz->pthreadTxSem);

    //while (!pThiz->mBootAnimFinished) {
    //    usleep(50*1000);//sleep 50ms
    //}

    int count = 0;
    while (!pThiz->mExitHdcpTxThread) {
        char hdcpTxKey[MESON_MODE_LEN] = {0};

        //actually, every product need provision HDCP key, if can not read it, we will always pend on
        meson_mode_read_sys(DISPLAY_HDMI_HDCP_KEY, hdcpTxKey, false, sizeof(hdcpTxKey));
        //we already read the HDCP key now, if we support 2.2, we must support 1.4
        if (((strstr(hdcpTxKey, (char *)"22") != NULL) && (strstr(hdcpTxKey, (char *)"14") != NULL)) ||
            // only have HDCP 1.4 key
            (!strcmp(hdcpTxKey, "14"))
            //wait 10s tee_hdcp update key
            || (count > 10))
            break;

        count++;

        std::unique_lock<std::mutex> autolock(pThiz->mMutex);
        pThiz->mCv.wait_for(autolock, std::chrono::milliseconds(1000));
    }

    if (pThiz->mExitHdcpTxThread)
        return NULL;

    if (pThiz->authInit(&hdcp22, &hdcp14)) {
        if (!pThiz->authLoop(hdcp22, hdcp14)) {
            SYS_LOGE("HDCP authenticate fail\n");
            pThiz->AuthResult(false);
        }
    }
    return NULL;
}

bool HDCPTxAuth::authInit(bool *pHdcp22, bool *pHdcp14) {
    bool useHdcp22 = false;
    bool useHdcp14 = false;
#ifdef HDCP_AUTHENTICATION
    char hdcpRxVer[MESON_MODE_LEN] = {0};
    char hdcpTxKey[MESON_MODE_LEN] = {0};

    //in general, MBOX is TX device, need to detect its TX keys.
    //            TV   is RX device, need to detect its RX keys.
    //HDCP TX: get current MBOX[TX] device contains which TX keys. Values:[14/22, 00 is no key]
    meson_mode_read_sys(DISPLAY_HDMI_HDCP_KEY, hdcpTxKey, false, sizeof(hdcpTxKey));
    SYS_LOGI("hdcp_tx key:%s\n", hdcpTxKey);
    if ((strlen(hdcpTxKey) == 0) || !(strcmp(hdcpTxKey, "00"))) {
        AuthResult(false);
        return false;
    }

    //HDCP RX: get current TV[RX] device contains which RX key. Values:[14/22, 00 is no key]
    //Values is the hightest key. if value is 22, means the devices supports 22 and 14.
    meson_mode_read_sys(DISPLAY_HDMI_HDCP_VER, hdcpRxVer, false, sizeof(hdcpRxVer));
    SYS_LOGI("hdcp_tx remote version:%s\n", hdcpRxVer);
    if ((strlen(hdcpRxVer) == 0) || !(strcmp(hdcpRxVer, "00"))) {
        AuthResult(false);
        return false;
    }

    //stop hdcp_tx
    stopVerAll();

    if (REPEATER_RX_VERSION_22 == mRepeaterRxVer) {
        SYS_LOGI("hdcp_tx 2.2 supported for RxSupportHdcp2.2Auth\n");
        useHdcp22 = true;
    } else if (/*(strstr(cap, (char *)"2160p") != NULL) && */(strstr(hdcpRxVer, (char *)"22") != NULL) &&
        (strstr(hdcpTxKey, (char *)"22") != NULL)) {
        if ((!access(HDCP_TX_FW_PATH_1, 0)) || (!access(HDCP_TX_FW_PATH_2, 0))) {
            SYS_LOGI("hdcp_tx 2.2 supported\n");
            useHdcp22 = true;
        } else {
            SYS_LOGE("!!!hdcp_tx 2.2 key is burned in This device, But %s and %s is not exist\n", HDCP_TX_FW_PATH_1, HDCP_TX_FW_PATH_2);
        }
    }

    if (REPEATER_RX_VERSION_14 == mRepeaterRxVer) {
        SYS_LOGI("hdcp_tx 1.4 supported for RxSupportHdcp1.4Auth\n");
        useHdcp14 = true;
    } else if (!useHdcp22 && (strstr(hdcpRxVer, (char *)"14") != NULL) &&
        (strstr(hdcpTxKey, (char *)"14") != NULL)) {
        useHdcp14 = true;
        SYS_LOGI("hdcp_tx 1.4 supported\n");
    }

    if (!useHdcp22 && !useHdcp14) {
        //do not support hdcp1.4 and hdcp2.2
        SYS_LOGE("device do not support hdcp1.4 or hdcp2.2\n");
        AuthResult(false);
        return false;
    }

    //start hdcp_tx
    if (useHdcp22) {
        startVer22();
    }
    else if (useHdcp14) {
        startVer14();
    }
#endif
    *pHdcp22 = useHdcp22;
    *pHdcp14 = useHdcp14;
    return true;
}

bool HDCPTxAuth::authLoop(bool useHdcp22, bool useHdcp14) {
    SYS_LOGI("hdcp_tx begin to authenticate hdcp22:%d, hdcp14:%d\n", useHdcp22, useHdcp14);

    bool success = false;
#ifdef HDCP_AUTHENTICATION
    int count = 0;
    while (!mExitHdcpTxThread) {
        std::unique_lock<std::mutex> autolock(mMutex);
        mCv.wait_for(autolock, std::chrono::milliseconds(200));

        char auth[MESON_MODE_LEN] = {0};
        meson_mode_read_sys(DISPLAY_HDMI_HDCP_AUTH, auth, false, sizeof(auth));
        if (strstr(auth, (char *)"1")) {//Authenticate is OK
            success = true;
            AuthResult(true);
            break;
        }

        count++;
        if (count > 40) { //max 200msx40 = 8s it will authenticate completely
            if (useHdcp22) {
                SYS_LOGE("hdcp_tx 2.2 authenticate fail for 8s timeout, change to hdcp_tx 1.4 authenticate\n");

                count = 0;
                useHdcp22 = false;
                useHdcp14 = true;
                stopVerAll();
                //if support hdcp22, must support hdcp14
                startVer14();
                continue;
            }
            else if (useHdcp14) {
                SYS_LOGE("hdcp_tx 1.4 authenticate fail, 8s timeout\n");
                startVer14();
            }
            break;
        }
    }
#endif
    SYS_LOGI("hdcp_tx authenticate success: %d\n", success?1:0);
    return success;
}

void HDCPTxAuth::startVer22() {
    //start hdcp_tx 2.2
    SYS_LOGI("start hdcp_tx 2.2\n");
    meson_mode_write_sys(DISPLAY_HDMI_HDCP_MODE, DISPLAY_HDMI_HDCP_22);
    usleep(50*1000);

    property_set("ctl.start", "hdcp_tx22");
}

void HDCPTxAuth::startVer14() {
    //start hdcp_tx 1.4
    //SYS_LOGI("hdcp_tx 1.4 start\n");
    meson_mode_write_sys(DISPLAY_HDMI_HDCP_MODE, DISPLAY_HDMI_HDCP_14);
}

void HDCPTxAuth::stopVerAll() {
    char hdcpRxVer[MESON_MODE_LEN] = {0};
    //stop hdcp_tx 2.2 & 1.4
    SYS_LOGI("hdcp_tx 2.2 & 1.4 stop hdcp pwr\n");
    meson_mode_write_sys(DISPLAY_HDMI_HDCP_POWER, "1");
    usleep(20000);
    property_set("ctl.stop", "hdcp_tx22");
    usleep(20000);
    meson_mode_read_sys(DISPLAY_HDMI_HDCP_POWER, hdcpRxVer, false, sizeof(hdcpRxVer));

    meson_mode_write_sys(DISPLAY_HDMI_HDCP_CONF, DISPLAY_HDMI_HDCP14_STOP);
    meson_mode_write_sys(DISPLAY_HDMI_HDCP_CONF, DISPLAY_HDMI_HDCP22_STOP);
    usleep(2000);
}

void HDCPTxAuth::AuthResult(bool result) {
    char fail_case[PROPERTY_VALUE_MAX] = {0};
    property_get(HDCP_TX_AUTH_FAIL, fail_case, "4");
    if (result) {
        meson_mode_write_sys(DISPLAY_HDMI_AVMUTE_SYSFS, "-1");
        if (!strcmp(fail_case, "1") || !strcmp(fail_case, "2")) {
            meson_mode_write_sys(DISPLAY_HDMI_VIDEO_MUTE, "0");
            meson_mode_write_sys(DISPLAY_HDMI_AUDIO_MUTE, "0");
            meson_mode_write_sys(DISPLAY_MEDIA_VIDEO_MUTE, "0");
        } else if (!strcmp(fail_case, "3") && mFallbackDefault) {
#ifndef RECOVERY_MODE
            SYS_LOGD("send hdcp auth success event.\n");
            if (pmHDCPTxAuthCallback)
                pmHDCPTxAuthCallback->onHdcpTxAuthEvent(HDMI_TX_AUTH_SUCCESS);
            mFallbackDefault = false;
#endif
        }
    } else {
        if (!strcmp(fail_case, "1")) {
            meson_mode_write_sys(DISPLAY_HDMI_VIDEO_MUTE, "1");
            meson_mode_write_sys(DISPLAY_HDMI_AUDIO_MUTE, "1");
        } else if (!strcmp(fail_case, "2")) {
            meson_mode_write_sys(DISPLAY_HDMI_AUDIO_MUTE, "1");
            meson_mode_write_sys(DISPLAY_MEDIA_VIDEO_MUTE, "1");
       } else if (!strcmp(fail_case, "3") && !mFallbackDefault) {
#ifndef RECOVERY_MODE
            SYS_LOGD("send hdcp auth fail event.\n");
            if (pmHDCPTxAuthCallback)
                pmHDCPTxAuthCallback->onHdcpTxAuthEvent(HDMI_TX_AUTH_FAIL);
            mFallbackDefault = true;
#endif
        }
    }
}

void HDCPTxAuth::isAuthSuccess(int *status) {
    int ret = pthread_mutex_trylock(&pthreadTxMutex);
    if (ret != 0) {
         SYS_LOGE("try lock pthreadTxMutex return status: %d\n",ret);
         *status = -1;
    }
    char auth[MESON_MODE_LEN] = {0};
    meson_mode_read_sys(DISPLAY_HDMI_HDCP_AUTH, auth, false, sizeof(auth));
    if (strstr(auth, (char *)"1")) {//Authenticate is OK
        *status = 1;
    }
    pthread_mutex_unlock(&pthreadTxMutex);
    SYS_LOGI("HDCPTx Auth status is: %d",*status);
}
