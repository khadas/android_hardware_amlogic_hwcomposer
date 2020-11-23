/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#ifndef _MESON_VIDEO_TUNNEL_BUFFERITEM_H
#define _MESON_VIDEO_TUNNEL_BUFFERITEM_H

#include <stdint.h>
#include <string>

#define VT_TIME_STAMP_TEST  310785169

class VTBufferItem {
public:
    VTBufferItem();
    VTBufferItem(std::string path);
    ~VTBufferItem();

    int allocateBuffer();
    int releaseBuffer(bool del);

    int getBufferFd() { return mFd; }
    int setBufferFd(int fd);

    int64_t getTimeStamp() { return mTimeStamp; }
    int setTimeStamp(int64_t time);

    std::string getBufferPath() { return mFilePath; }

    int getReleaseFenceFd() { return mReleaseFenceFd; }
    int setReleaseFenceFd(int fd);

private:
    int mFd;
    int mReleaseFenceFd;
    int64_t mTimeStamp;
    std::string mFilePath;
};

#endif //_MESON_VIDEO_TUNNEL_BUFFERITEM_H
