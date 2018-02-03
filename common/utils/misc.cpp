/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <misc.h>

#include <cutils/properties.h>

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <MesonLog.h>

#define MAX_STR_LEN         512

bool sys_get_bool_prop(const char* prop, bool defVal) {
    return property_get_bool(prop, defVal);
}

int32_t sys_get_string_prop(const char* prop, char* val) {
    return property_get(prop, val, NULL);
}

int32_t sysfs_get_string_ex(const char* path, char *str, int32_t size,
    bool needOriginalData) {

    int32_t fd, len;

    if ( NULL == str ) {
        MESON_LOGE("buf is NULL");
        return -1;
    }

    if ((fd = open(path, O_RDONLY)) < 0) {
        MESON_LOGE("readSysFs, open %s fail.", path);
        return -1;
    }

    len = read(fd, str, size);
    if (len < 0) {
        MESON_LOGE("read error: %s, %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (!needOriginalData) {
        int32_t i , j;
        for (i = 0, j = 0; i <= len -1; i++) {
            /*change '\0' to 0x20(spacing), otherwise the string buffer will be cut off
             * if the last char is '\0' should not replace it
             */
            if (0x0 == str[i] && i < len - 1) {
                str[i] = 0x20;
            }

            /* delete all the character of '\n' */
            if (0x0a != str[i]) {
                str[j++] = str[i];
            }
        }

        str[j] = 0x0;
    }

    close(fd);
    return 0;

}

int32_t sysfs_get_string(const char* path, char *str) {
    char buf[MAX_STR_LEN+1] = {0};
    sysfs_get_string_ex(path, (char*)buf, MAX_STR_LEN, false);
    strcpy(str, buf);
    return 0;
}

int32_t sysfs_set_string(const char *path, const char *val) {
    int32_t bytes;
    int32_t fd = open(path, O_RDWR);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        //DTRACE("setSysfsStr %s= %s\n", path,val);
        close(fd);
        return 0;
    } else {
        MESON_LOGE(" open file error: [%s]", path);
        return -1;
    }
}

int32_t sysfs_get_int(const char* path, int32_t def) {
    int32_t val = def;
    char str[64];
    if (sysfs_get_string(path, str) == 0) {
        val = atoi(str);
        MESON_LOGE("sysfs(%s) read int32_t (%d)", path, val);
    }
    return val;
}

