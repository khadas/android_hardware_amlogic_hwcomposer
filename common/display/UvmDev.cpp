/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <MesonLog.h>
#include "UvmDev.h"

#define UVM_DEV_PATH "/dev/uvm"
#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, struct uvm_fd_data)

struct uvm_fd_data {
    int fd;
    int commit_display;
};

ANDROID_SINGLETON_STATIC_INSTANCE(UvmDev)

UvmDev::UvmDev() {
    mDrvFd = open(UVM_DEV_PATH, O_RDWR);
    MESON_ASSERT(mDrvFd > 0, "UVM dev open failed");
}

UvmDev::~UvmDev() {
    if (mDrvFd > 0)
        close(mDrvFd);
}

int UvmDev::commitDisplay(const int fd, const int commit) {
    struct uvm_fd_data fd_data;

    fd_data.fd = fd;
    fd_data.commit_display = commit;

    if (ioctl(mDrvFd, UVM_IOC_SET_FD, &fd_data) != 0)
        return -1;

    return 0;
}
