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

bool Utils::get_bool_prop(const char* prop) {
    char val[PROPERTY_VALUE_MAX];
    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "false") && strcmp(val, "true") == 0) {
        //VTRACE("prop: %s is %s",prop, val);
        return true;
    }

    return false;
}

int Utils::get_int_prop(const char* prop) {
    char val[PROPERTY_VALUE_MAX];
    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "2")) {
        //VTRACE("prop: %s is %s",prop, val);
        return atoi(val);
    }
    return 0;
}

int Utils::getSysfsInt(const char* syspath, int def) {
    int val = def;
    char valstr[64];
    if (getSysfsStr(syspath, valstr, sizeof(valstr)) == 0) {
        val = atoi(valstr);
        //DTRACE("sysfs(%s) read int (%s)=(%d)", syspath, valstr, val);
    }
    return val;
}

int Utils::getSysfsStr(const char* syspath, char *valstr, int size,
    bool needOriginalData) {

    int fd, len;

    if ( NULL == valstr ) {
        ETRACE("buf is NULL");
        return -1;
    }

    if ((fd = open(syspath, O_RDONLY)) < 0) {
        ETRACE("readSysFs, open %s fail.", syspath);
        return -1;
    }

    len = read(fd, valstr, size);
    if (len < 0) {
        ETRACE("read error: %s, %s\n", syspath, strerror(errno));
        close(fd);
        return -1;
    }

    if (!needOriginalData) {
        int i , j;
        for (i = 0, j = 0; i <= len -1; i++) {
            /*change '\0' to 0x20(spacing), otherwise the string buffer will be cut off
             * if the last char is '\0' should not replace it
             */
            if (0x0 == valstr[i] && i < len - 1) {
                valstr[i] = 0x20;
            }
            //DTRACE("read buffer index:%d is a 0x0, replace to spacing \n", i);


            /* delete all the character of '\n' */
            if (0x0a != valstr[i]) {
                valstr[j++] = valstr[i];
            }
        }

        valstr[j] = 0x0;
    }

    //DTRACE("read %s, result length:%d, val:%s\n", syspath, len, valstr);

    close(fd);
    return 0;

}

int Utils::setSysfsStr(const char *path, const char *val) {
    int bytes;
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        //DTRACE("setSysfsStr %s= %s\n", path,val);
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

bool Utils::checkBoolProp(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "false") && strcmp(val, "true") == 0) {
        DTRACE("prop: %s is %s",prop, val);
        return true;
    }

    return false;
}

int32_t Utils::checkIntProp(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "2")) {
        VTRACE("prop: %s is %s",prop, val);
        return atoi(val);
    }
    return 0;
}

int32_t Utils::checkAndDupFd(int32_t fd) {
    if (fd < 0) {
        ETRACE("not a vliad fd %d", fd);
        return -1;
    }

    int32_t dup_fd = ::dup(fd);
    if (dup_fd < 0) {
        ETRACE("fd dup failed: %s", strerror(errno));
    }

    return dup_fd;
}

#if WITH_LIBPLAYER_MODULE
bool Utils::checkSysfsStatus(const char* sysfstr, char* lastr, int32_t size) {
    char val[32];
    char *p = lastr;

    memset(val, 0, sizeof(val));
    if (getSysfsStr(sysfstr, val, sizeof(val)) == 0) {
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

bool Utils::checkVinfo(framebuffer_info_t *fbInfo) {
    if (fbInfo != NULL && fbInfo->fd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fbInfo->fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
        {
            ETRACE("FBIOGET_VSCREENINFO error!!!");
            return -errno;
        }

        if (vinfo.xres != fbInfo->info.xres
            || vinfo.yres != fbInfo->info.yres
            || vinfo.width != fbInfo->info.width
            || vinfo.height != fbInfo->info.height) {
            if (int32_t(vinfo.width) <= 16 || int32_t(vinfo.height) <= 9) {
                // the driver doesn't return that information
                // default to 160 dpi
                vinfo.width  = ((vinfo.xres * 25.4f)/160.0f + 0.5f);
                vinfo.height = ((vinfo.yres * 25.4f)/160.0f + 0.5f);
            }
            fbInfo->xdpi = (vinfo.xres * 25.4f) / vinfo.width;
            fbInfo->ydpi = (vinfo.yres * 25.4f) / vinfo.height;

            fbInfo->info.xres = vinfo.xres;
            fbInfo->info.yres = vinfo.yres;
            fbInfo->info.width = vinfo.width;
            fbInfo->info.height = vinfo.height;

            return true;
        }
    }

    return false;
}

const char* Utils::getHotplugUeventEnvelope()
{
    return "change@/devices/virtual/switch/hdmi_audio";
}

const char* Utils::getHdcpUeventEnvelope()
{
    return "change@/devices/virtual/switch/hdcp";
}

const char* Utils::getSwitchState0()
{
    return "SWITCH_STATE=0";
}

const char* Utils::getSwitchState1()
{
    return "SWITCH_STATE=1";
}

} // namespace amlogic
} // namespace android
