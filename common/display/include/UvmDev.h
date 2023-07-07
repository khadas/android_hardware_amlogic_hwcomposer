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

#define META_DATA_SIZE       (512)

/**
 * please sync the file in kernel UVM module
 * if you change uvm_hook_mod_type.
 * include/linux/amlogic/meson_uvm_core.h
 */
enum uvm_hook_mod_type {
    VF_SRC_DECODER,
    VF_SRC_VDIN,
    VF_PROCESS_V4LVIDEO,
    VF_PROCESS_DI,
    VF_PROCESS_VIDEOCOMPOSER,
    VF_PROCESS_DECODER,
    PROCESS_NN,
    PROCESS_GRALLOC,
    PROCESS_AIPQ,
    PROCESS_DALTON,
    PROCESS_AIFACE,
    PROCESS_AICOLOR,
    PROCESS_HWC,
    PROCESS_INVALID,
};

struct uvm_fd_data {
    int fd;
    int data;
};

struct uvm_hook_data {
    int mode_type;
    int shared_fd;
    char data_buf[META_DATA_SIZE + 1];
};

class UvmDev : public android::Singleton<UvmDev> {
public:
    UvmDev();
    ~UvmDev();

    // set SF fd to uvm
    int setPid(int pid);
    // set UVM buffer fd to driver
    int commitDisplay(const int fd, const int commit);
    int dettachBuffer(const int fd);
    int attachBuffer(const int fd);
    int getVideoType(const int fd);

private:
    int mDrvFd;
};

#endif /* HWC_UVMDEV_H */
