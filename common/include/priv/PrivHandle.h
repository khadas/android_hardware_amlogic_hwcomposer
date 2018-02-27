/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef PRIVATE_HANDLE_H
#define PRIVATE_HANDLE_H

#include <utils/NativeHandle.h>
#include <gralloc_priv.h>

class PrivHandle {
public:
    PrivHandle();

    virtual ~PrivHandle();

    static int getFormat(const native_handle_t *bufferhnd);
    static int getFd(const native_handle_t *bufferhnd);
    static int getBStride(const native_handle_t *bufferhnd);
    static int getPStride(const native_handle_t *bufferhnd);

    static uint64_t getInternalFormat(const native_handle_t *nativeHnd);

    static bool isContinuous(const native_handle_t *nativeHnd);
    static bool isOverlayVideo(const native_handle_t *nativeHnd);
    static bool isOmxVideo(const native_handle_t *nativeHnd);
    static bool isSecure(const native_handle_t *nativeHnd);
};

#endif/*PRIVATE_HANDLE_H*/
