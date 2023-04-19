/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _FACE_SDK_H
#define _FACE_SDK_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "nn_sdk.h"
#include "nn_util.h"

#define MAX_FACE_DETECT_NUM         100
#define CONFIDENCE_THRESHOLD        0.8
#define NMS_THRESHOLD               0.25
#define GLOBAL_NMS_THRESHOLD        0.7
#define TOP_K                       500
#define KEEP_TOP_K                  100
#define OUTPUT_SIZE                 14280

typedef struct
{
    int index;
    int classId;
    float probs;
} face_sortable_bbox;

typedef struct __nn_face_landmark_5
{
    unsigned int   detNum;
    detBox facebox[MAX_FACE_DETECT_NUM];
    point_t pos[MAX_FACE_DETECT_NUM][5];
}face_landmark5_out_t;

void* face_init(const char *path, int inputWidth, int inputHeight, int inputChannel);
void* face_process_network(void *qcontext, unsigned char *in_addr, face_landmark5_out_t *nnOut);
void* face_uninit(void* context);
int isFaceInterfaceImplement();
void process_face_detect(float *bbox_buf, float *prob_buf, face_landmark5_out_t* face_out);
#ifdef __cplusplus
} //extern "C"
#endif
