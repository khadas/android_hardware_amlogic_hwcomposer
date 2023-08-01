/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_FRCDEV_H
#define HWC_FRCDEV_H

#include <utils/Singleton.h>

class FrcDev : public android::Singleton<FrcDev> {
public:
    FrcDev();
    ~FrcDev();

    int enableFrc(bool enable);

private:
    int mDrvFd;
    bool mEnable;
};

#endif /* HWC_UVMDEV_H */
