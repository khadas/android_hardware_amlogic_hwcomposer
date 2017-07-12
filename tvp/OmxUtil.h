/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *     AMLOGIC OMX IOCTL WRAPPER
 */


int openamvideo();
void closeamvideo();
int setomxdisplaymode();
int setomxpts(int time_video);
void set_omx_pts(char* data, int* handle);



