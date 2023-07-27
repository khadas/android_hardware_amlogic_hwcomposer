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
#include "UvmDev.h"

#define UVM_DEV_PATH "/dev/uvm"
#define UVM_IOC_MAGIC 'U'

#define UVM_IOC_SET_PID _IOWR(UVM_IOC_MAGIC, 2, struct uvm_pid_data)
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, struct uvm_fd_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, struct uvm_hook_data)
#define UVM_IOC_DETACH _IOWR(UVM_IOC_MAGIC, 8, struct uvm_hook_data)
#define UVM_IOC_GET_VIDEO_INFO _IOWR(UVM_IOC_MAGIC, 11, struct uvm_fd_info)

ANDROID_SINGLETON_STATIC_INSTANCE(UvmDev)

UvmDev::UvmDev() {
    mDrvFd = open(UVM_DEV_PATH, O_RDWR);
    MESON_ASSERT(mDrvFd > 0, "UVM dev open failed");
}

UvmDev::~UvmDev() {
    if (mDrvFd > 0)
        close(mDrvFd);
}

int UvmDev::setPid(int pid) {
    struct uvm_pid_data {
        int pid;
    } pid_data;

    pid_data.pid = pid;

    MESON_LOGD("setUVM with pid %d", pid);
    if (ioctl(mDrvFd, UVM_IOC_SET_PID, &pid_data) != 0) {
        MESON_LOGE("setUVM pid %d ioctl error %s", pid, strerror(errno));
        return -1;
    }
    return 0;
}

int UvmDev::commitDisplay(const int fd, const int commit) {
    struct uvm_fd_data fd_data;

    fd_data.fd = fd;
    fd_data.data = commit;

    if (ioctl(mDrvFd, UVM_IOC_SET_FD, &fd_data) != 0)
        return -1;

    return 0;
}

// dettach uvm hooked buffer
int UvmDev::dettachBuffer(int fd) {
    struct uvm_hook_data hook_data = {
        1 << VF_PROCESS_DI,
        fd,
        "dettach",
    };

    if (ioctl(mDrvFd, UVM_IOC_DETACH, &hook_data) != 0)
        return -1;

    return 0;
}

int UvmDev::attachBuffer(const int fd) {
    struct uvm_hook_data hook_data = {
        PROCESS_HWC,
        fd,
        "detach_flag=1",
    };

    if (ioctl(mDrvFd, UVM_IOC_SET_INFO, &hook_data) != 0) {
        MESON_LOGV("set uvm %d detach flag failed: %s", fd, strerror(errno));
        return -1;
    }

    return 0;
}

/* fd is must be video buffer fd */
int UvmDev::getVideoInfo(struct uvm_fd_info & videoInfo) {
    if (ioctl(mDrvFd, UVM_IOC_GET_VIDEO_INFO, &videoInfo) != 0) {
        MESON_LOGV("get %d video info failed: %s", videoInfo.fd, strerror(errno));
        return -1;
    }
    return 0;
}
