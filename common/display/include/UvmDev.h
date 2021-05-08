/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC_UVMDEV_H
#define HWC_UVMDEV_H

#include <utils/Singleton.h>

class UvmDev : public android::Singleton<UvmDev> {
public:
    UvmDev();
    ~UvmDev();

    // set UVM buffer fd to driver
    int commitDisplay(const int fd, const int commit);

private:
    int mDrvFd;
};

#endif /* HWC_UVMDEV_H */
