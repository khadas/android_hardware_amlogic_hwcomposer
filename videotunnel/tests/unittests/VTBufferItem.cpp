/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <VTBufferItem.h>

VTBufferItem::VTBufferItem() : mFd(-1) {

}

VTBufferItem::VTBufferItem(std::string path) : mFd(-1), mFilePath(path) {
}

int VTBufferItem::allocateBuffer() {
    if (mFilePath.empty()) {
        char nameBuffer[64] = "/data/vt-XXXXXX";
        mFd = mkstemp(nameBuffer);
        mFilePath = nameBuffer;
    } else {
        mFd = open(mFilePath.c_str(), O_WRONLY | O_CREAT , 0664);
    }

    return mFd;
}

int VTBufferItem::releaseBuffer(bool del) {
    if (mFd >= 0) {
        close(mFd);
        if (del)
            unlink(mFilePath.c_str());

        mFd = -1;
    }

    return 0;
}

int VTBufferItem::setBufferFd(int fd) {
    releaseBuffer(false);
    mFd = fd;
    return 0;
}

VTBufferItem::~VTBufferItem() {
    releaseBuffer(true);
}
