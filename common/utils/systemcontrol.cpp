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
#include <misc.h>

#include <utils/String16.h>

#define MODE_LEN 64
#define MAX_BUFFER_LEN             4096
//RX support display mode
#define DISPLAY_HDMI_EDID               "/sys/class/amhdmitx/amhdmitx0/disp_cap"
#define DISPLAY_HDMI_SINK_TYPE          "/sys/class/amhdmitx/amhdmitx0/sink_type"
#define DISPLAY_EDID_STATUS             "/sys/class/amhdmitx/amhdmitx0/edid_parsing"
//HDCP Authentication
#define DISPLAY_HDMI_HDCP_AUTH          "/sys/module/aml_media/parameters/hdmi_authenticated"
#define PROP_DOLBY_VISION_ENABLE "persist.vendor.sys.dolbyvision.enable"
#define PROP_DOLBY_VISION_TV_ENABLE "persist.vendor.sys.tv.dolbyvision.enable"

#if PLATFORM_SDK_VERSION >=  26
#include <vendor/amlogic/hardware/systemcontrol/1.1/ISystemControl.h>
using ::vendor::amlogic::hardware::systemcontrol::V1_1::ISystemControl;
using ::vendor::amlogic::hardware::systemcontrol::V1_0::Result;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
#else
#include <ISystemControlService.h>
#include <binder/IServiceManager.h>
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

    gSC = ISystemControl::tryGetService();
    while (!gSC) {
        MESON_LOGE("tryGet system control daemon Service failed, sleep to wait.");
        usleep(200*1000);//sleep 200ms
        gSC = ISystemControl::tryGetService();
    };

    gSCDeathRecipient = new SystemControlDeathRecipient();
    Return<bool> linked = gSC->linkToDeath(gSCDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        MESON_LOGE("Transaction error in linking to system service death: %s",
            linked.description().c_str());
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
        MESON_LOGE("sc_get_display_mode FAIL.");
        return -EFAULT;
    }

    return 0;
}

int32_t sc_set_display_mode(std::string &dispmode) {
    CHK_SC_PROXY();

    Result ret = gSC->setActiveDispMode(dispmode);
    if (ret == Result::OK) {
        return 0;
    } else {
        MESON_LOGE("sc_set_display_mode FAIL.");
        return -EFAULT;
    }
}

int32_t sc_get_osd_position(std::string &dispmode, int *position) {
    CHK_SC_PROXY();

    auto out = gSC->getPosition(dispmode, [&position](const Result &ret,
                        int left, int top, int width, int height) {
        if (ret == Result::OK) {
            position[0] = left;
            position[1] = top;
            position[2] = width;
            position[3] = height;
        }
    });

    if (!out.isOk()) {
        MESON_LOGE("sc_get_osd_positionc fail.");
        return -EFAULT;
    }

    return 0;
}

int32_t sc_write_sysfs(const char * path, std::string & val) {
    CHK_SC_PROXY();

    Result ret = gSC->writeSysfs(path, val);
    if (ret == Result::OK) {
        return 0;
    } else {
        MESON_LOGE("sc_write_sysfs FAIL.");
        return -EFAULT;
    }
}

int32_t sc_read_sysfs(const char * path, std::string & val) {
    CHK_SC_PROXY();

    gSC->readSysfs(path, [&val](
        const Result &ret, const hidl_string & retval) {
        if (Result::OK == ret) {
            val = retval.c_str();
        } else {
            val.clear();
        }
    });

    if (val.empty()) {
        MESON_LOGE("sc_read_sysfs FAIL.");
        return -EFAULT;
    }
    return 0;
}

int32_t sc_read_bootenv(const char * key, std::string & val) {
    CHK_SC_PROXY();

    gSC->getBootEnv(key, [&val](
        const Result &ret, const hidl_string & retval) {
        if (Result::OK == ret) {
            val = retval.c_str();
        } else {
            val.clear();
        }
    });

    if (val.empty()) {
        MESON_LOGE("sc_read_bootenv FAIL.");
        return -EFAULT;
    }

    return 0;
}

bool sc_set_bootenv(const char *key, const std::string &val) {
    CHK_SC_PROXY();

    gSC->setBootEnv(key, val);
    return true;
}

bool sc_get_property_boolean(const char * prop, bool val) {
    CHK_SC_PROXY();

    if (!prop) {
        return -EINVAL;
    }

    bool result = val;
    gSC->getPropertyBoolean(prop, val, [&result](const Result &ret, const bool& retval) {
            if (Result::OK == ret) {
                result = retval;
            }
    });

    return result;
}

int32_t sc_get_property_string(const char * prop, std::string & val, const std::string & def) {
    CHK_SC_PROXY();

    if (!prop) {
        return -EINVAL;
    }

    gSC->getPropertyString(prop, def, [&val](const Result & ret, const hidl_string & retval) {
        if (Result::OK == ret) {
            val = retval.c_str();
        } else {
            val.clear();
        }
    });

    return 0;
}

int32_t sc_set_property(const char * prop, const char *val ) {
    CHK_SC_PROXY();

    if (!prop || !val) {
        return -EINVAL;
    }

    Result ret = gSC->setProperty(hidl_string(prop), hidl_string(val));
    if (ret == Result::OK) {
        return 0;
    } else {
        MESON_LOGE("sc_set_property prop:%s val:%s FAIL.", prop, val);
        return -EFAULT;
    }
}

int32_t sc_sink_support_dv(std::string &mode, bool &val) {
    CHK_SC_PROXY();

    gSC->sinkSupportDolbyVision([&mode, &val](
        const Result &ret, const hidl_string &sinkMode, const bool &support) {
            if (ret == Result::OK) {
                val = support;
                mode = sinkMode.c_str();
            } else {
                val = false;
                mode.clear();
            }
    });

    if (val == false)
        MESON_LOGD("[%s] sink not support DV", __func__);

    return 0;
}

int32_t sc_get_dolby_version_type() {
    CHK_SC_PROXY();

    int32_t result = 0;

    gSC->getDolbyVisionType([&result](const Result &ret, const int32_t& value) {
        if (ret == Result::OK) {
            result = value;
        }
    });

    return result;
 }

bool sc_is_dolby_version_enable() {
    std::string mode;
    bool unused, dv_enable = false;
    sc_sink_support_dv(mode, unused);

    if (!sc_get_property_boolean(PROP_DOLBY_VISION_TV_ENABLE, false)) {
        dv_enable = false;
    } else if (!mode.empty()) {
        dv_enable = true;
     }
 
    return dv_enable;
}

bool  sc_get_pref_display_mode(std::string & dispmode) {
    CHK_SC_PROXY();

    gSC->getPrefHdmiDispMode([&dispmode](
        const Result & ret, const hidl_string & supportDispModes) {
        if (Result::OK == ret) {
            dispmode = supportDispModes.c_str();
        } else {
            dispmode.clear();
        }
    });

    if (dispmode.empty()) {
        MESON_LOGE("sc_get_pref_display_mode FAIL.");
        return false;
    }

    return true;
}

static bool get_hdmi_edid_data(char *data, uint32_t len) {
    char sinkType[MODE_LEN] = {0};
    char edidParsing[MODE_LEN] = {0};

    //three sink types: sink, repeater, none
    sysfs_get_string_original(DISPLAY_HDMI_SINK_TYPE, sinkType, MODE_LEN);
    sysfs_get_string(DISPLAY_EDID_STATUS, edidParsing, MODE_LEN);

    if (strstr(sinkType, "sink") || strstr(sinkType, "repeater")) {
        int count = 0;
        while (true) {
            sysfs_get_string_original(DISPLAY_HDMI_EDID, data, len);
            if (strlen(data) > 0)
                break;

            if (count >= 5) {
                strcpy(data, "null edid");
                break;
            }
            count++;
            usleep(500000);
        }
    } else {
        strcpy(data, "null edid");
    }

    return true;
}

int32_t get_hdmitx_mode_list(std::vector<std::string>& edidlist) {
    const char *delim = "\n";
    char edid_buf[MAX_BUFFER_LEN] = {0};

    edidlist.clear();
    get_hdmi_edid_data(edid_buf, MAX_BUFFER_LEN);
    char *ptr = strtok(edid_buf, delim);
    while (ptr != NULL) {
        int len = strlen(ptr);
        if (ptr[len - 1] == '*')
            ptr[len - 1] = '\0';

            edidlist.push_back(std::string(ptr));
            ptr = strtok(NULL, delim);
    }

    return 0;
}

int32_t get_hdmitx_hdcp_state(bool & val) {
    char auth[MODE_LEN] = {0};
    sysfs_get_string(DISPLAY_HDMI_HDCP_AUTH, auth, MODE_LEN);
    if (strstr(auth, "1"))
        val = true;
    else
        val = false;

    return 0;
}

int32_t read_sysfs(const char * path, std::string & val) {
    char buf[MAX_BUFFER_LEN] = {0};
    if (sysfs_get_string(path, buf, MAX_BUFFER_LEN) == 0)
        val = buf;

    if (val.empty()) {
        MESON_LOGE("sc_read_sysfs %s FAIL.", path);
        return -EFAULT;
    }
    return 0;
}

int32_t sc_set_hdmi_allm(bool on) {
    int32_t value = on ? 1 : 0;

    CHK_SC_PROXY();
    MESON_LOGD("set auto low latency mode to %d", value);
    gSC->setALLMState(value);

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
    if (gSC == NULL)
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

int32_t sc_get_hdmitx_hdcp_state(bool & val) {
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
        MESON_LOGE("sc_get_display_mode FAIL.");
        return -EFAULT;
    }
}

int32_t sc_set_display_mode(std::string &dispmode) {
    CHK_SC_PROXY();

    if (gSC->setActiveDispMode(dispmode)) {
        return 0;
    } else {
        MESON_LOGE("sc_set_display_mode FAIL.");
        return -EFAULT;
    }
}

int32_t sc_get_osd_position(std::string &dispmode, int *position) {
    CHK_SC_PROXY();

    const char * mode = dispmode.c_str();
    int left, top, width, height;
    gSC->getPosition(String16(mode), left, top, width, height);
    position[0] = left;
    position[1] = top;
    position[2] = width;
    position[3] = height;
    return 0;
}

int32_t sc_write_sysfs(const char * path, std::string &dispmode) {
    CHK_SC_PROXY();

    Result ret = gSC->writeSysfs(dispmode);
    if (ret == Result::OK) {
        return 0;
    } else {
        MESON_LOGE("sc_write_sysfs FAIL.");
        return -EFAULT;
    }
}

int32_t sc_read_sysfs(const char * path, std::string &dispmode) {
    CHK_SC_PROXY();

    Result ret = gSC->readSysfs(dispmode);
    if (ret == Result::OK) {
        return 0;
    } else {
        MESON_LOGE("sc_read_sysfs FAIL.");
        return -EFAULT;
    }
}

int32_t sc_set_hdmi_allm(bool on) {
    int32_t value = on ? 1 : 0;

    CHK_SC_PROXY();
    MESON_LOGD("set auto low latency mode to %d", value);
    gSC->setALLMState(value);

    return 0;
}
#endif
