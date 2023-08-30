/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <sys/mman.h>
#include <ge2d_port.h>
#include <ge2d_com.h>
#include <aml_ge2d.h>
#include <cutils/native_handle.h>
#include <misc.h>
#include <system/graphics-base.h>
#include <am_gralloc_ext.h>


//Scaling and cropping configuration
static int SRC1_CANVAS_W = 1920;
static int SRC1_CANVAS_H = 1080;
static int DST_CANVAS_W = 1920;
static int DST_CANVAS_H = 1080;

static int SRC1_PLANE_MEM_CNT = 1;
static int DST_PLANE_MEM_CNT = 1;

static int SRC1_RECT_X = 0;
static int SRC1_RECT_Y = 0;
static int SRC1_RECT_W = 1920;
static int SRC1_RECT_H = 1080;

static int DST_RECT_X = 0;
static int DST_RECT_Y = 0;
static int DST_RECT_W = 1920;
static int DST_RECT_H = 1080;

static int MEM_ALLOC_TYPE = 0;

static void clearGe2DInfo(aml_ge2d_t *amlge2d) {
    if (!amlge2d) {
        fprintf(stderr, "clear ge2d info error\n");
        return;
    }

    memset(&(amlge2d->ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d->ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(amlge2d->ge2dinfo.dst_info), 0, sizeof(buffer_info_t));
}

static int aml_write_file(int shared_fd, const char *file_name, int write_bytes) {
    int fd = -1;
    int write_num = 0;
    char *vaddr = NULL;

    if (shared_fd < 0 || !file_name || !write_bytes) {
        fprintf(stderr, "wrong params, read failed");
        return -1;
    }

    vaddr = (char *) mmap(NULL, write_bytes,
                          PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0);
    if (!vaddr) {
        fprintf(stderr, "%s,%d,mmap failed,Not enough memory\n", __func__, __LINE__);
        return -1;
    }

    fd = open(file_name, O_RDWR | O_CREAT, 0660);
    if (fd < 0) {
        fprintf(stderr, "write file:%s open error\n", file_name);
        return -1;
    }

    write_num = write(fd, vaddr, write_bytes);
    if (write_num <= 0) {
        fprintf(stderr, "write file write_num=%d error\n", write_num);
        close(fd);
    }

    fprintf(stderr, "save image 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
            ((char *) vaddr)[0], ((char *) vaddr)[1], ((char *) vaddr)[2], ((char *) vaddr)[3],
            ((char *) vaddr)[4], ((char *) vaddr)[5], ((char *) vaddr)[6], ((char *) vaddr)[7]);
    close(fd);
    munmap(vaddr, write_bytes);

    return 0;
}

static int ge2d_strechblit(aml_ge2d_t *amlge2d, unsigned int src1_dma_fd, unsigned int dst_dma_fd,
                           unsigned int src1_format, unsigned int dst_format, unsigned int rotate) {
    int ret = -1;
    aml_ge2d_info_t *pge2dinfo = &amlge2d->ge2dinfo;

    clearGe2DInfo(amlge2d);
    pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;    /* use allocated memory  */
    pge2dinfo->src_info[0].mem_alloc_type = MEM_ALLOC_TYPE; /* use ION memory to test, dmabuf can also be supported */
    pge2dinfo->src_info[0].shared_fd[0] = src1_dma_fd;        /* src buf shared fd */
    pge2dinfo->src_info[0].plane_number = SRC1_PLANE_MEM_CNT;/* src plane_number */
    pge2dinfo->src_info[0].canvas_w = SRC1_CANVAS_W;        /* src canvas width */
    pge2dinfo->src_info[0].canvas_h = SRC1_CANVAS_H;        /* src canvas height */
    pge2dinfo->src_info[0].rect.x = SRC1_RECT_X;            /* src rect x, input data area */
    pge2dinfo->src_info[0].rect.y = SRC1_RECT_Y;            /* src rect y, input data area */
    pge2dinfo->src_info[0].rect.w = SRC1_RECT_W;            /* src rect w, input data area */
    pge2dinfo->src_info[0].rect.h = SRC1_RECT_H;            /* src rect h, input data area */
    pge2dinfo->src_info[0].format = src1_format;        /* src format */
    pge2dinfo->src_info[0].plane_alpha = 0xFF;                /* global plane alpha*/

    pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;        /* use allocated memory  */
    pge2dinfo->dst_info.mem_alloc_type = MEM_ALLOC_TYPE;    /* use ION memory to test, dmabuf can also be supported */
    pge2dinfo->dst_info.shared_fd[0] = dst_dma_fd;            /* dst buf shared fd */
    pge2dinfo->dst_info.plane_number = DST_PLANE_MEM_CNT;    /* dst plane_number */


    pge2dinfo->dst_info.canvas_w = DST_CANVAS_W;            /* dst canvas width */
    pge2dinfo->dst_info.canvas_h = DST_CANVAS_H;            /* dst canvas height */

    pge2dinfo->dst_info.rect.x = DST_RECT_X;                /* dst rect x, input data area */
    pge2dinfo->dst_info.rect.y = DST_RECT_Y;                /* dst rect y, input data area */
    pge2dinfo->dst_info.rect.w = DST_RECT_W;                /* dst rect w, input data area */
    pge2dinfo->dst_info.rect.h = DST_RECT_H;                /* dst rect h, input data area */

    pge2dinfo->dst_info.format = dst_format;            /* src format */
    pge2dinfo->dst_info.plane_alpha = 0xFF;                /* global plane alpha*/
    pge2dinfo->dst_info.rotation = rotate;                  /* dst rotation option*/

    pge2dinfo->ge2d_op = AML_GE2D_STRETCHBLIT;

    ret = aml_ge2d_process(pge2dinfo);
    if (ret < 0) {
        fprintf(stderr, "ge2d process failed, %s (%d)\n", __func__, __LINE__);
    }

    return ret;
}

int main() {
    fprintf(stderr, "ge2d init\n");
    int rotate = 0;
    int ret = 0;
    aml_ge2d_t amlge2d;

    memset(&amlge2d, 0, sizeof(aml_ge2d_t));

    ret = aml_ge2d_init(&amlge2d);
    if (ret < 0) {
        fprintf(stderr, "ge2d init failed\n");
    }

    fprintf(stderr, "Load the src data \n");
    buffer_handle_t srchnd = gralloc_alloc_dma_buf(1920, 1080, 3, true, false);
    if (srchnd == NULL) {
        fprintf(stderr, "VirtualLayer allocate buf failed. \n");
        return -1;
    }

    FILE *fp = NULL;
    fp = fopen("/sdcard/source-fb.raw", "r");
    if (fp == NULL) {
        fprintf(stderr, "open fail\n");
        exit(0);
    }

    int srcFd = am_gralloc_get_buffer_fd(srchnd);
    char *base = (char *) mmap(NULL, 1920 * 1080 * 3, PROT_WRITE, MAP_SHARED, srcFd, 0);

    int i = fread(base, 1, 1920 * 1080 * 3, fp);
    fprintf(stderr, "read %d \n", i);
    fclose(fp);
    munmap(base, 1920 * 1080 * 3);

    fprintf(stderr, "malloc the dest data\n");
    buffer_handle_t hnd = gralloc_alloc_dma_buf(1920, 1080, 1, true, false);
    if (hnd == NULL) {
        fprintf(stderr, "VirtualLayer allocate buf failed. \n");
        return -1;
    }

    int dstFd = am_gralloc_get_buffer_fd(hnd);

    fprintf(stderr, "start testing\n");

    /* 2. ge2d execute */
    ret = ge2d_strechblit(&amlge2d, srcFd, dstFd, PIXEL_FORMAT_RGB_888, PIXEL_FORMAT_RGBA_8888, rotate);

    ret = aml_write_file(dstFd, "/data/dst.raw", 1920 * 1080 * 4);

    aml_ge2d_exit(&amlge2d);
    fprintf(stderr, "ge2d_transform finished\n");
    return 0;
}
