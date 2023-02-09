/*
 *Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 *This source code is subject to the terms and conditions defined in the
 *file 'LICENSE' which is part of this source code package.
 *
 *Description:
 */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include "DisplayAdapter.h"
#include "whiteboard.h"

static std::unique_ptr<meson::DisplayAdapter> displayAdapter = meson::DisplayAdapterCreateRemote();

int WhiteBoard::getWhiteBoardFd() {
    const native_handle_t *outBufferHandle = nullptr;
    int fd = -1;

    if (!displayAdapter) {
        return -1;
    }
    displayAdapter->getWhiteBoardHanle(&outBufferHandle,fd);
    return fd;
}

bool WhiteBoard::setWhiteBoardMode(bool mode) {

    if (!displayAdapter) {
        return false;
    }

    return displayAdapter->setWriteBoardMode(mode);
}

bool WhiteBoard::setWBDisplayFrame(int x, int y) {

    if (!displayAdapter) {
        return false;
    }

    displayAdapter->setWBDisplayFrame(x, y);
    return true;
}

bool WhiteBoard::getWhiteBoardMode(bool& mode) {

    if (!displayAdapter) {
        return false;
    }

    displayAdapter->getWriteBoardMode(mode);
    return true;
}

bool WhiteBoard::hideVideoLayer(bool mode) {

    if (!displayAdapter) {
        return false;
    }

    displayAdapter->hideVideoLayer(mode);
    return true;
}
