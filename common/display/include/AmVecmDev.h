/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_AMVECMDEV_H
#define HWC_AMVECMDEV_H

#include <utils/Singleton.h>

struct eye_protect_s {
    int en;
    int rgb[3];
};

class AmVecmDev : public android::Singleton<AmVecmDev> {
public:
    AmVecmDev();
    ~AmVecmDev();

    int setColorTransform(const float *matrix,  const bool on);

private:
    int mDrvFd;
    bool mEnable;
    float mColorMatrix[16];
};

#endif /* HWC_AMVECMDEV_H */
