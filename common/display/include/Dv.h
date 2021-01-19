/*
 ** Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 **
 ** This source code is subject to the terms and conditions defined in the
 ** file 'LICENSE' which is part of this source code package.
 **
 ** Description:
 **/

#ifndef DV_H
#define DV_H

#include <DrmTypes.h>
#include <BasicTypes.h>
#include <utils/threads.h>

static bool getDvSupportStatus() {
    //return dv support info from current platform device, not display device.
    const char *DV_PATH_INFO = "/sys/class/amdolby_vision/support_info";
    char buf[1024+1] = {0};
    int fd, len;

    /*bit0: 0-> efuse, 1->no efuse; */
    /*bit1: 1->ko loaded*/
    /*bit2: 1-> value updated*/
    int supportInfo;

    constexpr int dvDriverEnabled = (1 << 2);
    constexpr int dvSupported = ((1 << 0) | (1 << 1) | (1 <<2));

    if ((fd = open(DV_PATH_INFO, O_RDONLY)) < 0) {
        MESON_LOGE("open %s fail.\n", DV_PATH_INFO);
        return false;
    } else {
        if ((len = read(fd, buf, 1024)) < 0) {
            MESON_LOGE("read %s error: %s\n", DV_PATH_INFO, strerror(errno));
            close(fd);
            return false;
        } else {
            sscanf(buf, "%d", &supportInfo);
            if ((supportInfo & dvDriverEnabled) == 0)
                MESON_LOGE("dolby vision driver is not ready\n");

            close(fd);
            return ((supportInfo & dvSupported) == dvSupported) ? true : false;
        }
    }
}

#endif

