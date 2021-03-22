/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef _SR_SDK_H
#define _SR_SDK_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

void* nn_init(const char *path);
int nn_process_network(void *qcontext,
                             unsigned char *in_addr,
                             unsigned char *out_addr);
int nn_uninit(void* context);
int isInterfaceImplement();

#ifdef __cplusplus
} //extern "C"
#endif
