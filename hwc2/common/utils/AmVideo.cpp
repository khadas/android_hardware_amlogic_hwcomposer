/*
// Copyright (c) 2017 Amlogic
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
*/

#define LOG_TAG "AmVideo"
//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include <AmVideo.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

//#define AMVIDEO_DEBUG

using namespace android;

#define AM_VIDEO_DEV "/dev/amvideo"

#define AMSTREAM_IOC_MAGIC  'S'
#define AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT  _IOR(AMSTREAM_IOC_MAGIC, 0x21, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT  _IOW(AMSTREAM_IOC_MAGIC, 0x22, int)

AmVideo* AmVideo::mInstance = NULL;
Mutex AmVideo::mLock;

AmVideo::AmVideo() {
    mDevFd = open(AM_VIDEO_DEV, O_RDWR | O_NONBLOCK);
    if (mDevFd < 0) {
        ALOGE("Open %s Failed. ", AM_VIDEO_DEV);
    }

    if (getVideoPresent(mVideoPresent) != 0) {
        ALOGE("Get video mute failed.");
        mVideoPresent = true;
    }
}

AmVideo::~AmVideo() {
    if (mDevFd >= 0) {
        close(mDevFd);
        mDevFd = -1;
    }
}

AmVideo* AmVideo::getInstance() {
    if (mInstance == NULL) {
        Mutex::Autolock _l(mLock);
        if (mInstance == NULL) {
            mInstance = new AmVideo();
        }
    }

    return mInstance;
}

int AmVideo::presentVideo(bool bPresent) {
    if (mDevFd < 0)
        return -EBADF;

    if (mVideoPresent != bPresent) {
        ALOGD("muteVideo to %d", bPresent);
        uint32_t val = bPresent ? 1 : 0;
        if (ioctl(mDevFd, AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT, val) != 0) {
            ALOGE("AMSTREAM_SET_VIDEO_OUTPUT ioctl (%d) return(%d)", bPresent, errno);
            return -EINVAL;
        }
        mVideoPresent = bPresent;
    } else {
        #ifdef AMVIDEO_DEBUG
        bool val = true;
        getVideoPresent(val);
        if (mVideoPresent != val) {
            ALOGE("presentVideo (%d) vs (%d)", mVideoPresent, val);
        }
        #endif
        ALOGD("Already set video to (%d)", bPresent);
    }

    return 0;
}

int AmVideo::getVideoPresent(bool& output) {
    if (mDevFd < 0)
        return -EBADF;

    uint32_t val = 1;
    if (ioctl(mDevFd, AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT, &val) != 0) {
        ALOGE("AMSTREAM_GET_VIDEO_OUTPUT ioctl fail(%d)", errno);
        return -EINVAL;
    }

    output = (val ==0) ? false : true;
    return 0;
}

