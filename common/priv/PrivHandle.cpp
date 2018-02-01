/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <PrivHandle.h>

PrivHandle::PrivHandle() {
}

PrivHandle::~PrivHandle() {
}

int PrivHandle::getHndFormat(const native_handle_t *nativeHnd) {
    int format = HAL_PIXEL_FORMAT_RGBA_8888;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) format = buffer->format;
    return format;
}

int PrivHandle::getHndSharedFd(const native_handle_t *nativeHnd) {
    int fd = -1;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) fd = buffer->share_fd;
    return fd;
}

int PrivHandle::getHndByteStride(const native_handle_t *nativeHnd) {
    int byteStride = 64;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) byteStride = buffer->byte_stride;
    return byteStride;
}

int PrivHandle::getHndPixelStride(const native_handle_t *nativeHnd) {
    int pixelStride = 4;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) pixelStride = buffer->stride;
    return pixelStride;
}
