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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

//MesonDisplay
#include "Hwc2Layer.h"
#include "CompositionProcessor.h"
#include <png.h>
#include <zlib.h>
#include <misc.h>
#include <sys/mman.h>

int loadVirtualLayerData(FILE *file, std::shared_ptr<Hwc2Layer> targetLayer){

    //use libpng
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));
    png_init_io(png_ptr, file);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);

    int m_width = png_get_image_width(png_ptr, info_ptr);
    int m_height = png_get_image_height(png_ptr, info_ptr);

    hwc_frect_t mCrop = {0, 0, static_cast<float>(m_width), static_cast<float>(m_height)};
    targetLayer->setSourceCrop(mCrop);
    targetLayer->setBlendMode(HWC2_BLEND_MODE_PREMULTIPLIED);

    buffer_handle_t hnd = gralloc_alloc_dma_buf(m_width, m_height, HAL_PIXEL_FORMAT_RGBA_8888, true, false);
    if (hnd == NULL ) {
        fprintf(stderr, "VirtualLayer allocate buf failed. \n");
        return -1;
    }

    targetLayer->setBuffer(hnd,-1);
    int bufFd = am_gralloc_get_buffer_fd(hnd);
    char *base = (char *)mmap(NULL, m_width*m_height*4, PROT_WRITE , MAP_SHARED, bufFd, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "targetLayer mmap failed \n");
        return -1;
    }

    unsigned char ** row_pointers = png_get_rows(png_ptr, info_ptr);
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if (row_pointers)
        fprintf(stderr, "targetLayer size(%dx%d) row_bytes %d \n", m_width, m_height,row_bytes);

    for (int i = 0; i < m_height; i++) {
        memcpy(base + row_bytes*i, row_pointers[i], row_bytes);
    }
    munmap(base,m_width*m_height*4);

    return 0;

}

int main() {
    fprintf(stderr, "Start to test the Composition function\n");
    fprintf(stderr, "Please confirm that Whiteboard.png and UI.png exist under the/system/media directory \n");
    fprintf(stderr, "The two pictures should be 32bits \n");
    fprintf(stderr, "Results are saved in /sdcard/outputs.raw \n");
    //Create DrmFramebuffers and load fake data
    std::shared_ptr<Hwc2Layer> mFirstLayer = std::make_shared<Hwc2Layer>(1);
    std::shared_ptr<Hwc2Layer> mSecondLayer = std::make_shared<Hwc2Layer>(1);
    const char * uipngPath  = "/system/media/UI.png";
    const char * wbpngPath  = "/system/media/Whiteboard.png";

    FILE *file = fopen(uipngPath,"rb");
    if (file == NULL) {
        fprintf(stderr,"Unable to open PNG %s \n",uipngPath);
        return -1;
    }

    int ret = loadVirtualLayerData(file, mFirstLayer);
    if (ret == -1) {
        fprintf(stderr,"Load date failed\n");
        return -1;
    }
    fclose(file);

    FILE *wbfile = fopen(wbpngPath,"rb");
    if (wbfile == NULL) {
        fprintf(stderr,"Unable to open PNG %s \n",wbpngPath);
        return -1;
    }

    ret = loadVirtualLayerData(wbfile, mSecondLayer);
    if (ret == -1) {
        fprintf(stderr,"Load date failed\n");
        return -1;
    }
    fclose(wbfile);

    //setUp the render thread
    std::shared_ptr<CompositionProcessor> combProcessor = std::make_shared<CompositionProcessor>();
    combProcessor->setup();

    //Create the outputs Buffer
    buffer_handle_t hnd;
    hnd = gralloc_alloc_dma_buf(3840, 2160, HAL_PIXEL_FORMAT_RGBA_8888, true, false);
    auto outBuf = std::make_shared<DrmFramebuffer>(hnd, -1);
    //composition the two DrmFramebuffers
    std::shared_ptr<DrmFramebuffer> inUI = mFirstLayer;
    std::shared_ptr<DrmFramebuffer> inWB = mSecondLayer;
    combProcessor->composite(inUI, inWB, outBuf);
    //get Target DrmFramebuffer and store it

    int width = am_gralloc_get_width(outBuf->mBufferHandle);
    int height = am_gralloc_get_height(outBuf->mBufferHandle);
    int stride = am_gralloc_get_stride_in_pixel(outBuf->mBufferHandle);
    int format = am_gralloc_get_format(outBuf->mBufferHandle);
    fprintf(stderr, "outputs format %d, (%d, %d) stride %d\n",
        format, width, height, stride);

    void* mapBase = nullptr;
    if (gralloc_lock_dma_buf(outBuf->mBufferHandle, &mapBase) != 0) {
        fprintf(stderr, "lock dma buff failed\n");
        return -1;
    }
    int fd = -1;
    fd = open("/sdcard/outputs.raw", O_WRONLY | O_CREAT | O_TRUNC, 0664);

    size_t Bpp = bytesPerPixel(format);
    for (size_t y = 0 ; y < height ; y++) {
        write(fd, mapBase, width*Bpp);
        mapBase = (void *)((char *)mapBase + stride*Bpp);
    }

    close(fd);
    // after use, need unlock and free native handle
    gralloc_unlock_dma_buf(outBuf->mBufferHandle);
    gralloc_unref_dma_buf(outBuf->mBufferHandle);

    fprintf(stderr, "Composition two frames finish!\n");
    return 0;
}
