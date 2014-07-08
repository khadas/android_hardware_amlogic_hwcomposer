/*
 * AMLOGIC IOCTL WRAPPER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#define LOG_NDEBUG 0
#define LOG_TAG "omxutil"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <media/stagefright/foundation/ADebug.h>

#define AMSTREAM_IOC_MAGIC  'S'

#define AMSTREAM_IOC_SET_OMX_VPTS  _IOW(AMSTREAM_IOC_MAGIC, 0xaf, unsigned long)
#define AMSTREAM_IOC_SET_VIDEO_DISABLE  _IOW(AMSTREAM_IOC_MAGIC, 0x49, unsigned long)

static int amvideo_handle = 0;
int openamvideo() {
    amvideo_handle = open("/dev/amvideo",O_RDWR | O_NONBLOCK);
    return amvideo_handle;
}

void closeamvideo() {
    if (amvideo_handle != 0) {
        int ret = close(amvideo_handle);
        amvideo_handle = 0;
        if (ret < 0)
            ALOGE("close Amvideo error");
    }
}

int setomxdisplaymode() {
    return ioctl(amvideo_handle, AMSTREAM_IOC_SET_VIDEO_DISABLE, 2);

}
int setomxpts(int time_video) {
    return ioctl(amvideo_handle, AMSTREAM_IOC_SET_OMX_VPTS, (unsigned long)&time_video);
}


