/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *     AMLOGIC OMX IOCTL WRAPPER
 */


#define LOG_NDEBUG 0
#define LOG_TAG "omxutil"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <media/stagefright/foundation/ADebug.h>

#define AMSTREAM_IOC_MAGIC  'S'

#define AMSTREAM_IOC_SET_OMX_VPTS  _IOW(AMSTREAM_IOC_MAGIC, 0xaf, int)
#define AMSTREAM_IOC_SET_VIDEO_DISABLE  _IOW(AMSTREAM_IOC_MAGIC, 0x49, int)

static int amvideo_handle = 0;

#define TVP_SECRET "amlogic_omx_decoder,pts="
#define TVP_SECRET_RENDER "is rendered = true"

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

void set_omx_pts(char* data, int* handle) {
    if (data == NULL) {
        ALOGE("hnd->base is NULL!!!!");
        return;
    }
    if (strncmp(data, TVP_SECRET, strlen(TVP_SECRET)) == 0) {
        if (*handle == 0 || amvideo_handle == 0) {
             *handle = openamvideo();
            if (*handle == 0)
                ALOGW("can not open amvideo");
        }
        if (strncmp(data+sizeof(TVP_SECRET)+sizeof(signed long long), TVP_SECRET_RENDER, strlen(TVP_SECRET_RENDER)) != 0) {
            signed long long time;
            memcpy(&time, (char*)data+sizeof(TVP_SECRET), sizeof(signed long long));
            int time_video = time * 9 / 100 + 1;
            //ALOGW("render____time=%lld,time_video=%d",time,time_video);
            int ret = setomxpts(time_video);
            if (ret < 0) {
                ALOGW("setomxpts error, ret =%d",ret);
            }
        }
        memcpy((char*)data + sizeof(TVP_SECRET) + sizeof(signed long long), TVP_SECRET_RENDER, sizeof(TVP_SECRET_RENDER));
    }
}

