/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 * Display screen test
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>

#include <ui/Rect.h>
#include <ui/GraphicTypes.h>
#include <ui/PixelFormat.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBufferAllocator.h>

#include <cutils/properties.h>

#include <system/graphics.h>

#include "DisplayAdapter.h"
#include "am_gralloc_ext.h"
//#include "misc.h"

// TODO: Fix Skia.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <SkImageEncoder.h>
#include <SkData.h>
#include <SkColorSpace.h>
#pragma GCC diagnostic pop

using namespace android;

static void usage(const char* pname) {
    fprintf(stderr,
            "usage: %s [-h] [-r] [-l layer-id] [FILENAME]\n"
            "    -h: print this message\n"
            "    -r: save as raw data or not.\n"
            "    -l: specify the hwc layer id to capture(not implement now)\n"
            "        see \"dumpsys SurfaceFlinger\" for valid layer IDS.\n"
            "if FILENAME not given, the results will be printed to sdtout.\n",
            pname);
}

static SkColorType flinger2skia(PixelFormat f) {
    switch (f) {
        case PIXEL_FORMAT_RGB_565:
            return kRGB_565_SkColorType;
        default:
            return kN32_SkColorType;
    }
}

static int32_t gralloc_unref_dma_buf(native_handle_t * hnd) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();

    bool bfreed = false;
    if (am_gralloc_is_valid_graphic_buffer(hnd)) {
        if (NO_ERROR == maper.freeBuffer(hnd)) {
            bfreed = true;
        }
    }

    if (bfreed == false) {
        /*may be we got handle not alloc by gralloc*/
        native_handle_close(hnd);
        native_handle_delete(hnd);
    }

    return 0;
}

static int32_t gralloc_lock_dma_buf(
    native_handle_t * handle, void** vaddr) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();
    uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
    int w = am_gralloc_get_width(handle);
    int h = am_gralloc_get_height(handle);

    Rect r(w, h);
    if (NO_ERROR == maper.lock(handle, usage, r, vaddr))
        return 0;

    fprintf(stderr, "lock buffer failed\n");
    return -EINVAL;
}

static int32_t gralloc_unlock_dma_buf(native_handle_t * handle) {
    static GraphicBufferMapper & maper = GraphicBufferMapper::get();
    if (NO_ERROR == maper.unlock(handle))
        return 0;
    return -EINVAL;
}

int main(int argc, char **argv) {
    const char* pname = argv[0];
    int layerId = -1;
    int c;
    bool rawData = false;

    while ((c = getopt(argc, argv, "rhl:")) != -1) {
        switch (c) {
            case 'l':
                layerId = atoi(optarg);
                break;
            case 'r':
                rawData = true;
                break;
            case '?':
            case 'h':
                usage(pname);
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    int fd = -1;
    const char* fn = NULL;
    if (argc == 0) {
        fd = dup(STDOUT_FILENO);
    } else if (argc == 1) {
        fn = argv[0];
        fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fd == -1) {
            fprintf(stderr, "Error opening file: %s (%s)\n", fn, strerror(errno));
            return 1;
        }
    }

    if (fd == -1) {
        usage(pname);
        return 1;
    }

    const native_handle_t *outBufferHandle = nullptr;
    std::unique_ptr<meson::DisplayAdapter> displayAdapter = meson::DisplayAdapterCreateRemote();

    if (!displayAdapter) {
        fprintf(stderr, "DisplayAdapter init failed\n");
        return 1;
    }
    displayAdapter->captureDisplayScreen(&outBufferHandle);

    if (outBufferHandle == nullptr) {
        fprintf(stderr, "capture display screen got a null buffhandle\n");
        close(fd);
        return 1;
    }

    fprintf(stderr, "\npassh handle nubFds %d, numInts %d\n",
            outBufferHandle->numFds, outBufferHandle->numInts);

    for (int i = 0; i < outBufferHandle->numFds; i++) {
        fprintf(stderr, "Get naitve handle fd[%d]= %d\n", i, outBufferHandle->data[i]);
    }

    native_handle_t *bufferHandle = const_cast<native_handle_t*> (outBufferHandle);

    int width = am_gralloc_get_width(bufferHandle);
    int height = am_gralloc_get_height(bufferHandle);
    int stride = am_gralloc_get_stride_in_pixel(bufferHandle);
    int format = am_gralloc_get_format(bufferHandle);

    size_t size = stride * height * bytesPerPixel(format);

    fprintf(stderr, "format %d, (%d, %d) stride %d\n",
                format, width, height, stride);
    void* mapBase = nullptr;

    if (gralloc_lock_dma_buf(bufferHandle, &mapBase) != 0) {
        fprintf(stderr, "lock dma buff failed\n");
        close(fd);
        return 1;
    }

    if (property_get_bool("vendor.meson.display.debug", false)) {
        sleep(30);
    }
    fprintf(stderr, "start screencap size=%u\n", size);

    if (rawData == false) {
        const SkImageInfo info =
           SkImageInfo::Make(width, height, flinger2skia(format), kPremul_SkAlphaType, nullptr);

        SkPixmap pixmap(info, mapBase, stride * bytesPerPixel(format));
        struct FDWStream final : public SkWStream {
            size_t fBytesWritten = 0;
            int fFd;
            FDWStream(int f) : fFd(f) {}
            size_t bytesWritten() const override {
                return fBytesWritten;
            }
            bool write(const void* buffer, size_t size) override {
                fBytesWritten += size;
                return size == 0 || ::write(fFd, buffer, size) > 0;
            }
        } fdStream(fd);
        (void)SkEncodeImage(&fdStream, pixmap, SkEncodedImageFormat::kPNG, 100);
    } else {
        size_t Bpp = bytesPerPixel(format);
        for (size_t y = 0 ; y < height ; y++) {
            write(fd, mapBase, width*Bpp);
            mapBase = (void *)((char *)mapBase + stride*Bpp);
        }
    }

    close(fd);
    // after use, need unlock and free native handle
    gralloc_unlock_dma_buf(bufferHandle);
    gralloc_unref_dma_buf(bufferHandle);

    fprintf(stderr, "display screen cap finish!\n");

    if (property_get_bool("vendor.meson.display.debug", false)) {
        sleep(30);
    }

    return 0;
}
