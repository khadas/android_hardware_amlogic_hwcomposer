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

    static int getHndFormat(const native_handle_t *bufferhnd);
    static int getHndSharedFd(const native_handle_t *bufferhnd);
    static int getHndByteStride(const native_handle_t *bufferhnd);
    static int getHndPixelStride(const native_handle_t *bufferhnd);
};

#endif/*PRIVATE_HANDLE_H*/
