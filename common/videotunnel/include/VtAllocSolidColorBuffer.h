/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MESON_VT_ALLOC_SOLID_COLOR_BUFFER
#define MESON_VT_ALLOC_SOLID_COLOR_BUFFER

#include <video_tunnel.h>

#define VIDEO_BUFFER_W 192
#define VIDEO_BUFFER_H 90

class VtAllocSolidColorBuffer{
public:
    VtAllocSolidColorBuffer();
    virtual ~VtAllocSolidColorBuffer();

    int allocBuffer(vt_video_color_t colorType);
    int getPreBuffer();
private:
    void freeBuffer();

   int mBufferFd;
   vt_video_color_t mColorType;
   buffer_handle_t mSolidColorHandle;
};
#endif
