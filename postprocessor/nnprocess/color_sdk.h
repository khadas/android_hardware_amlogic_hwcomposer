/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _COLOR_SDK_H
#define _COLOR_SDK_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

void* color_init(const char *path, int model_type, int inputWidth, int inputHeight);
void* color_process_network(void *context, unsigned char *rawdata);
void* color_uninit(void* context);
int isColorInterfaceImplement();
#ifdef __cplusplus
} //extern "C"
#endif
