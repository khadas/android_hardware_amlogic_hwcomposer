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
    mEnable = on;
    struct eye_protect_s  data;
    data.en = on;

    /* transform to 10 bit */
    for (int i = 0; i < 16; i++) {
        data.mtx_ep[i/4][i%4] = (int)(1024 * matrix[i]);
    }

    String8 matrixDump;
    matrixDump.append("\n-------------------------------------------------\n");
    for (int i = 0; i < 16; i ++ ) {
        matrixDump.appendFormat("%6f ", matrix[i]);
        if ((i+1) % 4 == 0)
            matrixDump.append("\n");
    }
    matrixDump.append("-------------------------------------------------\n");

    MESON_LOGD("%s matrix:%s  enable:%d", __func__,  matrixDump.string(), on);

    if (ioctl(mDrvFd, AMVECM_IOC_S_EYE_PROT, &data) != 0) {
        MESON_LOGD("set AmVECM eye prot failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}
