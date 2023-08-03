/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <errno.h>

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <misc.h>
#include <system/graphics-base.h>
#include <am_gralloc_ext.h>

#define VDIN1_DEV "/dev/vdin1"
enum tvin_color_fmt_e {
    TVIN_RGB444 = 0,
    TVIN_YUV422,		/* 1 */
    TVIN_YUV444,		/* 2 */
    TVIN_YUYV422,		/* 3 */
    TVIN_YVYU422,		/* 4 */
    TVIN_UYVY422,		/* 5 */
    TVIN_VYUY422,		/* 6 */
    TVIN_NV12,		/* 7 */
    TVIN_NV21,		/* 8 */
    TVIN_BGGR,		/* 9  raw data */
    TVIN_RGGB,		/* 10 raw data */
    TVIN_GBRG,		/* 11 raw data */
    TVIN_GRBG,		/* 12 raw data */
    TVIN_COLOR_FMT_MAX,
};
struct vdin_vf_info {
    int index;
    unsigned int crc;

    /*
    * [0]:vdin get frame time,
    * [1]:vdin put frame time
    * [2]:vdin read return time
    */
    long long ready_clock[3];/* ns */
};

enum port_mode {
    capture_osd_plus_video = 0,
    capture_only_video,
};

struct vdin_v4l2_param_s {
    int width;
    int height;
    int fps;
    enum tvin_color_fmt_e dst_fmt;
    int dst_width; /* H scaling down */
    int dst_height; /* V scaling down */
    unsigned int bitorder; /* raw data bit order (0: none std, 1: std)*/
    enum port_mode mode;   /* 0: osd+video 1: video only*/
    int bit_dep;
    bool secure_memory_en; /* 0:not secure memory 1:secure memory */
};

struct vdin_set_canvas_s {
    int fd;
    int index;
};

#define _TM_T 'T'
#define TVIN_IOC_S_VDIN_V4L2START  _IOW(_TM_T, 0x25, struct vdin_v4l2_param_s)
#define TVIN_IOC_S_CANVAS_ADDR  _IOW(_TM_T, 0x4f, struct vdin_set_canvas_s)
#define TVIN_IOC_S_VDIN_V4L2STOP   _IO(_TM_T, 0x26)
#define TVIN_IOC_S_CANVAS_RECOVERY  _IO(_TM_T, 0x0a)

int main() {
    int dev = open(VDIN1_DEV, O_RDONLY);
    if (dev < 0) {
        return 0;
    }

    vdin_set_canvas_s canvas[6];
    std::vector<buffer_handle_t> mVdinHnds;

    for (int i = 0;i < 6; i++) {
        canvas[i].index = i;
        canvas[i].fd = -1;

        buffer_handle_t hnd = gralloc_alloc_dma_buf(1920, 1080, HAL_PIXEL_FORMAT_RGB_888, true, false,1);
        if (!hnd) {
            printf("gralloc alloc buffer failed");
            exit(-1);
        }
        mVdinHnds.push_back(hnd);

        int bufFd = am_gralloc_get_buffer_fd(hnd);
        if (bufFd >= 0) {
            canvas[i].fd = ::dup(bufFd);
        } else {
            printf("cannot get buffer fd(%d) i:%d", bufFd, i);
            exit(-1);
        }
    }

    ioctl(dev, TVIN_IOC_S_CANVAS_ADDR, canvas);
    printf("TVIN_IOC_S_CANVAS_ADDR \n");

    vdin_v4l2_param_s param;
    memset(&param, 0, sizeof(param));
    param.width = 3840;
    param.height = 2160;
    param.dst_width = 1920;
    param.dst_height = 1080;
    param.fps = 60;
    ioctl(dev, TVIN_IOC_S_VDIN_V4L2START, &param);
    printf("TVIN_IOC_S_VDIN_V4L2START \n");

    int savefd[6];
    savefd[0] = open("/sdcard/0.bin", O_CREAT | O_RDWR , 0644);
    savefd[1] = open("/sdcard/1.bin", O_CREAT | O_RDWR , 0644);
    savefd[2] = open("/sdcard/2.bin", O_CREAT | O_RDWR , 0644);
    savefd[3] = open("/sdcard/3.bin", O_CREAT | O_RDWR , 0644);
    savefd[4] = open("/sdcard/4.bin", O_CREAT | O_RDWR , 0644);
    savefd[5] = open("/sdcard/5.bin", O_CREAT | O_RDWR , 0644);

    //char idxstr[32];
    struct vdin_vf_info id;

    struct pollfd fds[1];
    fds[0].fd = dev;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    int k = 6;
    while (k--) {
        int rtn = poll(fds, 1, 10000);
        if (rtn > 0 && fds[0].revents == POLLIN) {
            printf("poll ok\n");
            //memset(idxstr, 0, 32);
            if (read(dev, &id, sizeof(struct vdin_vf_info)) >= 0) {
                int idx = id.index;//atoi(idxstr);
                printf("read ok, idx=%d\n", idx);
                //#if 0
                if (idx < 6) {
                    //HANDLE BUFFER.
                    char * img = (char *)mmap(NULL, 1920*1080*3, PROT_READ, MAP_SHARED, canvas[idx].fd, 0);
                    printf("mmap buf %p\n", img);

                    if (write(savefd[idx], img, 1920*1080*3) <= 0) {
                        printf("write error return %d \n",-errno);
                    }
                }
                //#endif
                printf("TVIN_IOC_S_CANVAS_RECOVERY \n");

                ioctl(dev, TVIN_IOC_S_CANVAS_RECOVERY, &idx);
            } else {
                printf("read fail \n");
            }
        } else {
            printf("poll fail \n");
        }
    }

    sleep(5);
    for (int j = 0;j < 6; j++) {
        close(canvas[j].fd);
        close(savefd[j]);
    }

    for (auto it = mVdinHnds.begin(); it != mVdinHnds.end(); it ++) {
        gralloc_free_dma_buf((native_handle_t * )*it);
    }
    ioctl(dev, TVIN_IOC_S_VDIN_V4L2STOP);
    printf("TVIN_IOC_S_VDIN_V4L2STOP  \n");
    return 0;
}
