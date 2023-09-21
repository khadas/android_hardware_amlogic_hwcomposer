/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "mode_util.h"

int32_t meson_mode_write_sys(const char *path, const char *val) {
    int fd;

    if ((fd = open(path, O_RDWR)) < 0) {
        SYS_LOGE("open %s fail. Error info [%s]", path, strerror(errno));
        return -errno;
    }

    SYS_LOGD("write %s, val:%s\n", path, val);
    if (write(fd, val, strlen(val)) != strlen(val))
        SYS_LOGE("write %s failed!\n", path);

    close(fd);

    return 0;
}

int32_t meson_mode_read_sys(const char *path, char *val, bool original, int valSize) {
    char buf[MAX_BUF_LEN] = {0};

    int fd, len;
    if ((fd = open(path, O_RDONLY)) < 0) {
        SYS_LOGE("open %s fail. Error info [%s]", path, strerror(errno));
        return -errno;
    }

    len = read(fd, buf, MAX_BUF_LEN-1);
    close(fd);
    if (len < 0) {
        SYS_LOGE("read error: %s, %s\n", path, strerror(errno));
        return -errno;
    }

    if (!original) {
        int i , j;
        for (i = 0, j = 0; i <= len -1; i++) {
            /*
             * change '\0' to 0x20(spacing), otherwise the string buffer will be cut off
             * if the last char is '\0' should not replace it
             */
            if (0x0 == buf[i] && i < len - 1) {
                buf[i] = 0x20;
                SYS_LOGD("read buffer index:%d is a 0x0, replace to spacing \n", i);
            }

            /* delete all the character of '\n' and '\r'*/
            if (0x0a != buf[i] && 0x0d != buf[i]) {
                buf[j++] = buf[i];
            }
        }

        buf[j] = 0x0;
    }

    SYS_LOGD("read %s, result length:%d, val:%s\n", path, len, buf);

    strncpy(val, buf, valSize);

    return 0;
}

bool meson_write_valid_mode_sys(const char* path, const char *outputmode) {
    int fd;

    SYS_LOGD("meson_write %s, outputmode:%s\n", path, outputmode);

    if ((fd = open(path, O_WRONLY)) < 0) {
        SYS_LOGE("meson_write, open %s fail.", path);
        return false;
    }

    if (write(fd, outputmode, strlen(outputmode)) != strlen(outputmode)) {
        SYS_LOGD("valid mode is false!\n");
        close(fd);
        return false;
    }

    SYS_LOGD("valid mode is true!\n");
    close(fd);
    return true;
}
