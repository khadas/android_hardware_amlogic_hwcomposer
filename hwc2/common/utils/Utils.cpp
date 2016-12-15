/*
// Copyright(c) 2016 Amlogic Corporation
*/

#include <hardware/hardware.h>

#include <HwcTrace.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <cutils/properties.h>
#include <hardware/hwcomposer2.h>

#include <Utils.h>

namespace android {
namespace amlogic {

Utils::Utils()
{

}

Utils::~Utils()
{

}

bool Utils::checkBoolProp(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "false") && strcmp(val, "true") == 0) {
        ALOGD("prop: %s is %s",prop, val);
        return true;
    }

    return false;
}

int32_t Utils::checkIntProp(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "2")) {
        //ALOGV("prop: %s is %s",prop, val);
        return atoi(val);
    }
    return 0;
}

int32_t Utils::checkAndDupFence(int32_t fence) {
    if (fence < 0) {
        ETRACE("not a vliad fence %d",fence);
        return -1;
    }

    int32_t dup_fence = dup(fence);
    if (dup_fence < 0) {
        ETRACE("fence dup failed: %s", strerror(errno));
    }

    return dup_fence;
}

#if WITH_LIBPLAYER_MODULE
bool Utils::checkSysfsStatus(const char* sysfstr, char* lastr, int32_t size) {
    char val[32];
    char *p = lastr;

    memset(val, 0, sizeof(val));
    if (amsysfs_get_sysfs_str(sysfstr, val, sizeof(val)) == 0) {
        DTRACE("val: %s, lastr: %s",val, p);
        if ((strcmp(val, p) != 0)) {
            memset(p, 0, size);
            strcpy(p, val);
            return true;
        }
    }

    return false;
}
#endif

bool Utils::checkOutputMode(char* curmode, int32_t* rate) {
    int32_t modefd = open(SYSFS_DISPLAY_MODE, O_RDONLY);
    if (modefd < 0) {
        ETRACE("open (%s) fail", SYSFS_DISPLAY_MODE);
        return -1;
    }

    char outputmode[32] = {0};
    read(modefd, outputmode, 31);
    close(modefd);
    modefd = -1;

    *rate = 60;
    if (strstr(outputmode, "50hz") != NULL) {
        *rate = 50;
    } else if (strstr(outputmode, "30hz") != NULL) {
        *rate = 30;
    } else if (strstr(outputmode, "25hz") != NULL) {
        *rate = 25;
    } else if ((strstr(outputmode, "24hz") != NULL) || (strstr(outputmode, "smpte") != NULL)) {
        *rate = 24;
    } else
        DTRACE("displaymode (%s) doesn't  specify HZ", curmode);

    //check if need update vsync.
    if (strcmp(outputmode, curmode) == 0) {
        ETRACE("outputmode didn't change %s", curmode);
        return false;
    }

    strcpy(curmode, outputmode);
    DTRACE("get new outputmode (%s) new period (%d)", curmode, rate);
    return true;
}

bool Utils::checkVinfo(framebuffer_info_t *fbinfo) {
    if (fbinfo != NULL && fbinfo->fd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fbinfo->fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
        {
            ALOGE("FBIOGET_VSCREENINFO error!!!");
            return -errno;
        }

        if (vinfo.xres != fbinfo->info.xres
            || vinfo.yres != fbinfo->info.yres
            || vinfo.width != fbinfo->info.width
            || vinfo.height != fbinfo->info.height) {
            if (int32_t(vinfo.width) <= 16 || int32_t(vinfo.height) <= 9) {
                // the driver doesn't return that information
                // default to 160 dpi
                vinfo.width  = ((vinfo.xres * 25.4f)/160.0f + 0.5f);
                vinfo.height = ((vinfo.yres * 25.4f)/160.0f + 0.5f);
            }
            fbinfo->xdpi = (vinfo.xres * 25.4f) / vinfo.width;
            fbinfo->ydpi = (vinfo.yres * 25.4f) / vinfo.height;

            fbinfo->info.xres = vinfo.xres;
            fbinfo->info.yres = vinfo.yres;
            fbinfo->info.width = vinfo.width;
            fbinfo->info.height = vinfo.height;

            return true;
        }
    }

    return false;
}

const char* Utils::getUeventEnvelope()
{
    return "change@/devices/virtual/switch/hdmi_audio";
}

const char* Utils::getHotplugOutString()
{
    return "SWITCH_STATE=0";
}

const char* Utils::getHotplugInString()
{
    return "SWITCH_STATE=1";
}

} // namespace amlogic
} // namespace android
