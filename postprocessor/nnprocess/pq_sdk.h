/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _PQ_SDK_H
#define _PQ_SDK_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

void* init(const char *path, int model_type);
void* process_network(void *context, unsigned char *rawdata);
void* uninit(void* context);
int isPqInterfaceImplement();
#ifdef __cplusplus
} //extern "C"
#endif
