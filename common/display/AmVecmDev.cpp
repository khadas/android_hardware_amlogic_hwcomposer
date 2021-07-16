/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 1

#include <fcntl.h>
#include <sys/ioctl.h>
#include <MesonLog.h>
#include "AmVecmDev.h"

#define AMVECM_DEV_PATH "/dev/amvecm"

#define _VE_CM  'C'
#define AMVECM_IOC_S_EYE_PROT   _IOW(_VE_CM, 0x78, struct eye_protect_s)

ANDROID_SINGLETON_STATIC_INSTANCE(AmVecmDev)

AmVecmDev::AmVecmDev() {
    mDrvFd = open(AMVECM_DEV_PATH, O_RDWR);
    mEnable = false;
    MESON_ASSERT(mDrvFd > 0, "Amvecm dev open failed %s", strerror(errno));
}

AmVecmDev::~AmVecmDev() {
    if (mDrvFd > 0)
        close(mDrvFd);
}

int AmVecmDev::setColorTransform(const float *matrix,  const bool on) {
    memcpy(mColorMatrix, matrix, sizeof(float) * 16);
    int r, g, b;

    /* transform to 10 bit */
    r = (int)(1024 * matrix[0]);
    g = (int)(1024 * matrix[5]);
    b = (int)(1024 * matrix[10]);

    mEnable = on;
    struct eye_protect_s  data = {
        .en = on,
        .rgb = {r, g, b},
    };

    MESON_LOGD("%s to amvecm:%d,%d,%d  enable:%d", __func__, r, g, b, on);

    if (ioctl(mDrvFd, AMVECM_IOC_S_EYE_PROT, &data) != 0) {
        MESON_LOGD("set AmVECM eye prot failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}
