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

int PrivHandle::getFormat(const native_handle_t *nativeHnd) {
    int format = HAL_PIXEL_FORMAT_RGBA_8888;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) format = buffer->format;
    return format;
}

int PrivHandle::getFd(const native_handle_t *nativeHnd) {
    int fd = -1;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) fd = buffer->share_fd;
    return fd;
}

int PrivHandle::getBStride(const native_handle_t *nativeHnd) {
    int byteStride = 64;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) byteStride = buffer->byte_stride;
    return byteStride;
}

int PrivHandle::getPStride(const native_handle_t *nativeHnd) {
    int pixelStride = 4;
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (buffer) pixelStride = buffer->stride;
    return pixelStride;
}

bool PrivHandle::isSecure(const native_handle_t *nativeHnd) {
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (NULL == buffer) return true;

    if (buffer->flags & private_handle_t::PRIV_FLAGS_SECURE_PROTECTED)
        return true;

    return false;
}

bool PrivHandle::isContinuous(const native_handle_t *nativeHnd) {
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (NULL == buffer) return true;

    if (buffer->flags & private_handle_t::PRIV_FLAGS_CONTINUOUS_BUF
            || buffer->flags & private_handle_t::PRIV_FLAGS_USES_ION_DMA_HEAP)
        return true;

    return false;
}

bool PrivHandle::isOverlayVideo(const native_handle_t *nativeHnd) {
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (NULL == buffer) return false;

    if (buffer->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY)
        return true;

    return false;
}

bool PrivHandle::isOmxVideo(const native_handle_t *nativeHnd) {
    private_handle_t const* buffer = private_handle_t::dynamicCast(nativeHnd);

    if (NULL == buffer) return false;

    if (buffer->flags & private_handle_t::PRIV_FLAGS_VIDEO_OMX)
        return true;

    return false;
}
