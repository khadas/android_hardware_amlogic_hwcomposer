/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <MesonLog.h>
#include "misc.h"
#include "FrcDev.h"

#define FRC_DEV_PATH "/dev/frc"
#define FRC_DEV_CONTRL _IOW('F', 0x06, unsigned int)

#define FRC_DEV_SYSFS "/sys/class/frc/debug"

ANDROID_SINGLETON_STATIC_INSTANCE(FrcDev)

FrcDev::FrcDev() {
    mDrvFd = open(FRC_DEV_PATH, O_RDWR);
    mEnable = true;
}

FrcDev::~FrcDev() {
    if (mDrvFd > 0)
        close(mDrvFd);
}

int FrcDev::enableFrc(bool enable) {
    // device has no frc
    if (mDrvFd < 0) {
        return 0;
    }

    if (mEnable == enable) {
        return 0;
    }

    // currently write frc sysfs to enable/disable memc
    MESON_LOGD("%s %d", __func__, enable);
    if (enable) {
        sysfs_set_string(FRC_DEV_SYSFS, "auto_ctrl 1");
        sysfs_set_string(FRC_DEV_SYSFS, "dbg_mode 1");
    } else {
        sysfs_set_string(FRC_DEV_SYSFS, "auto_ctrl 0");
        sysfs_set_string(FRC_DEV_SYSFS, "dbg_mode 2");
    }

#if 0
    int value = enable ? 1 : 0;
    if (ioctl(mDrvFd, FRC_DEV_CONTRL, &value) != 0) {
        MESON_LOGE("set FRC enable %d ioctl error %s", value, strerror(errno));
        return -1;
    }
#endif

    return 0;
}
