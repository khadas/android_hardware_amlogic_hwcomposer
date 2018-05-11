/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <systemcontrol.h>
#include <MesonLog.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>

#if PLATFORM_SDK_VERSION >=  26
#include <vendor/amlogic/hardware/systemcontrol/1.0/ISystemControl.h>
using ::vendor::amlogic::hardware::systemcontrol::V1_0::ISystemControl;
using ::vendor::amlogic::hardware::systemcontrol::V1_0::Result;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
#else
#include <ISystemControlService.h>
#endif

#define CHK_SC_PROXY() \
    if (gSC == NULL) { \
        load_sc_proxy(); \
        if (gSC == NULL) { \
            MESON_LOGE("Load systemcontrol service failed."); \
            return -EFAULT;\
        } \
    }


/*HIDL BASED SYSTEMCONTROL SERVICE PROXY.*/
#if PLATFORM_SDK_VERSION >= 26

static sp<ISystemControl> gSC = NULL;

struct SystemControlDeathRecipient : public android::hardware::hidl_death_recipient  {
    virtual void serviceDied(uint64_t cookie,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override {
        UNUSED(cookie);
        UNUSED(who);
        gSC = NULL;
    };
};
sp<SystemControlDeathRecipient> gSCDeathRecipient = NULL;

static void load_sc_proxy() {
    if (gSC != NULL)
        return;

    gSC = ISystemControl::getService();
    gSCDeathRecipient = new SystemControlDeathRecipient();
    Return<bool> linked = gSC->linkToDeath(gSCDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        MESON_LOGE("Transaction error in linking to system service death: %s", linked.description().c_str());
    } else if (!linked) {
        MESON_LOGE("Unable to link to system service death notifications");
    } else {
        MESON_LOGV("Link to system service death notification successful");
    }
}

int32_t sc_get_hdmitx_mode_list(std::vector<std::string>& edidlist) {
    CHK_SC_PROXY();

    gSC->getSupportDispModeList([&edidlist](
        const Result & ret, const hidl_vec<hidl_string> supportDispModes) {
        if (Result::OK == ret) {
            for (size_t i = 0; i < supportDispModes.size(); i++) {
                edidlist.push_back(supportDispModes[i]);
            }
        } else {
            edidlist.clear();
        }
    });

    if (edidlist.empty()) {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
        return -EFAULT;
    }

    return 0;
}

int32_t sc_get_hdmitx_hdcp_state(bool & val) {
    CHK_SC_PROXY();
    Result rtn = gSC->isHDCPTxAuthSuccess();
    MESON_LOGD("hdcp status: %d", rtn);
    val = (rtn == Result::OK) ? true : false;
    return 0;
}

int32_t  sc_get_display_mode(std::string & dispmode) {
    CHK_SC_PROXY();

    gSC->getActiveDispMode([&dispmode](
        const Result & ret, const hidl_string & supportDispModes) {
        if (Result::OK == ret) {
            dispmode = supportDispModes.c_str();
        } else {
            dispmode.clear();
        }
    });

    if (dispmode.empty()) {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
        return -EFAULT;
    }

    return 0;
}

int32_t sc_get_osd_position(std::string &dispmode, int *position) {
    CHK_SC_PROXY();

    gSC->getPosition(dispmode, [&position](const Result &ret,
                        int left, int top, int width, int height) {
        if (ret == Result::OK) {
            position[0] = left;
            position[1] = top;
            position[2] = width;
            position[3] = height;

            MESON_LOGI("sc_get_osd_position x:%d y:%d w:%d h:%d",
                 position[0], position[1], position[2], position[3]);
        }
    });

    return 0;
}

#else

static sp<ISystemControlService> gSC = NULL;

static void load_sc_proxy() {
    if (gSC != NULL)
        return;

    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        MESON_LOGE("Couldn't get default ServiceManager\n");
        return;
    }

    gSC = interface_cast<ISystemControlService>(
        sm->getService(String16("system_control")));
    if (systemControl == NULL)
        MESON_LOGE("Couldn't get connection to SystemControlService\n");
}

int32_t sc_get_hdmitx_mode_list(std::vector<std::string>& edidlist) {
    CHK_SC_PROXY();

    if (gSC->getSupportDispModeList(&edidlist)) {
        return 0;
    } else {
        MESON_LOGE("syscontrol::readEdidList FAIL.");
        return -EFAULT;
    }
}

bool sc_get_hdmitx_hdcp_state(bool & val) {
    CHK_SC_PROXY();
    int status;
    gSC->isHDCPTxAuthSuccess(status);
    val = (status == 1) ?  true : false;
    MESON_LOGD("hdcp status: %d", status);
    return 0;
}

int32_t  sc_get_display_mode(std::string & dispmode) {
    CHK_SC_PROXY();

    if (gSC->getActiveDispMode(&dispmode)) {
        return 0;
    } else {
        MESON_LOGE("syscontrol::getActiveDispMode FAIL.");
        return -EFAULT;
    }
}

int32_t sc_get_osd_position(std::string &dispmode, int *position) {
    CHK_SC_PROXY();

    if (gSC->getPosition(dispmode,
                int left, int top, int width, int height)) {
        position[0] = left;
        position[1] = top;
        position[2] = width;
        position[3] = height;
        MESON_LOGI("sc_get_osd_position x:%d y:%d w:%d h:%d",
             position[0], position[1], position[2], position[3]);

        return 0;
    } else {
        MESON_LOGE("syscontrol::getPosition FAIL.");
        return -EFAULT;
    }
}

#endif