/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aipq"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dlfcn.h>
#include "face_sdk.h"

static float rprob8[10800] = {0};
static float rprob16[2760] = {0};
static float rprob32[720] = {0};
//output box
static detBox rbox8[45][80][3];
static detBox rbox16[23][40][3];
static detBox rbox32[12][20][3];

static float overlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1;
    float l2 = x2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1;
    float r2 = x2 + w2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

static float box_intersection(detBox a, detBox b)
{
    float area = 0;
    float w = overlap(a.x, a.w, b.x, b.w);
    float h = overlap(a.y, a.h, b.y, b.h);
    if (w < 0 || h < 0) {
        return 0;
    }
    area = w * h;
    return area;
}

static float box_union(detBox a, detBox b)
{
    float i = box_intersection(a, b);
    float u = a.w * a.h + b.w * b.h - i;
    return u;
}

static float box_iou(detBox a, detBox b)
{
    return box_intersection(a, b) / box_union(a, b);
}

static int nms_comparator(const void *pa, const void *pb)
{
    face_sortable_bbox a = *(face_sortable_bbox *)pa;
    face_sortable_bbox b = *(face_sortable_bbox *)pb;
    float diff = a.probs - b.probs;
    if (diff < 0)
        return 1;
    else if (diff > 0)
        return -1;

    return 0;
}

static void do_intersection(detBox *box_a, detBox *box_b)
{
    float ul_x = fmax(box_a->x, box_b->x);
    float ul_y = fmax(box_a->y, box_b->y);
    float lr_x = fmin(box_a->x + box_a->w, box_b->x + box_b->w);
    float lr_y = fmin(box_a->y + box_a->h, box_b->y + box_b->h);
    box_a->x = ul_x;
    box_a->y = ul_y;
    box_a->w = lr_x - ul_x;
    box_a->h = lr_y - ul_y;
}

static void do_nms_sort(detBox *boxes, float probs[], int total)
{
    int i = 0, j = 0;
    face_sortable_bbox *s = (face_sortable_bbox *)calloc(total, sizeof(face_sortable_bbox));
    for (i = 0; i < total; ++i) {
        s[i].index = i;
        s[i].classId = 0;
        s[i].probs = probs[i];
    }

    qsort(s, total, sizeof(face_sortable_bbox), nms_comparator);

    for (i = 0; i < total; ++i) {
        if (probs[s[i].index] >= CONFIDENCE_THRESHOLD) {
            for (j = i+1; j < total; j++) {
                if (probs[s[j].index] >= CONFIDENCE_THRESHOLD) {
                    detBox b = boxes[s[j].index];
                    if (box_iou(boxes[s[i].index], b) > NMS_THRESHOLD) {
                        if (probs[s[i].index] == probs[s[j].index])
                            do_intersection(&boxes[s[i].index], &boxes[s[j].index]);
                        probs[s[j].index] = 0;
                    }
                }
            }
        }
    }
    free(s);
}


static void face_set_result(int num, detBox *boxes, float probs[], face_landmark5_out_t* face_out)
{

    int i;
    int detect_num = face_out->detNum;
    for (i = 0; i < num; i++) {
        float prob = probs[i];
        if (detect_num < MAX_FACE_DETECT_NUM) {
            if (prob > CONFIDENCE_THRESHOLD) {
                if (detect_num >= MAX_FACE_DETECT_NUM)
                    break;
                face_out->facebox[detect_num].score = prob;
                face_out->facebox[detect_num].x = boxes[i].x;
                face_out->facebox[detect_num].y = boxes[i].y;
                face_out->facebox[detect_num].w = boxes[i].w;
                face_out->facebox[detect_num].h = boxes[i].h;
                detect_num++;
            }
        }
    }
    face_out->detNum = detect_num;
}

static void do_global_sort(detBox *boxe1, detBox *boxe2, float prob1[], float prob2[], int len1, int len2)
{
    int i,j;
    for (i = 0; i < len1; ++i) {
        if (prob1[i] > GLOBAL_NMS_THRESHOLD) {
            for (j = 0; j < len2; j++) {
                if (prob2[j] > GLOBAL_NMS_THRESHOLD) {
                    if (box_iou(boxe1[i], boxe2[j]) > 0.1) {
                        if (prob2[j] > prob1[i])
                            prob1[i] = 0;
                        else
                            prob2[j] = 0;
                    }
                }
            }
        }
    }
}

void process_face_detect(float *bbox_buf, float *prob_buf, face_landmark5_out_t* face_out)
{
    int i = 0, k = 0, m = 0, x = 0, y = 0;
    int h32 = 0, w32 = 0, h16 = 0, w16 = 0, h8 = 0, w8 = 0;
    float pred_ctrx = 0, pred_ctry = 0, predw = 0, predh = 0;
    int valid_8 = 0, valid_16 = 0, valid_32 = 0;

    detBox *rpbox8  = (detBox *)rbox8;
    detBox *rpbox16 = (detBox *)rbox16;
    detBox *rpbox32 = (detBox *)rbox32;

    for (i = 0; i < OUTPUT_SIZE; i++) {
        if (i < 10800) {
            rprob8[m] = prob_buf[2 * i + 1];
            if (rprob8[m] < CONFIDENCE_THRESHOLD)
                rprob8[m] = 0;
            else
                valid_8 = 1;
            m++;
        }
        else if (i < 13560) {
            rprob16[x] = prob_buf[2 * i + 1];
            if (rprob16[x] < CONFIDENCE_THRESHOLD)
                rprob16[x] = 0;
            else
                valid_16 = 1;
            x++;
        }
        else {
            rprob32[y] = prob_buf[2 * i + 1];
            if (rprob32[y] < CONFIDENCE_THRESHOLD)
                rprob32[y] = 0;
            else
                valid_32 = 1;
            y++;
        }
    }

    if (valid_8 == 1) {
        for (y = 0, k = 0; y < 45; y++) {
            for (x = 0; x < 80; x++) {
                for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h8 = w8 = 6;
                    else if (i == 1)
                        h8 = w8 = 10;
                    else
                        h8 = w8 = 16;

                    float s_kx = w8;
                    float s_ky = h8 ;
                    float cx = (x + 0.5) * 8;
                    float cy = (y + 0.5) * 8;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox8[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox8[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox8[y][x][i].w = predw;
                    rbox8[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_16 == 1) {
        for (y=0, k = 43200; y < 23; y++) {
            for (x = 0; x < 40; x++) {
               for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h16 = w16 = 24;
                    else if (i == 1)
                        h16 = w16 = 40;
                    else
                        h16 = w16 = 64;

                    float s_kx = w16;
                    float s_ky = h16;
                    float cx = (x + 0.5) * 16;
                    float cy = (y + 0.5) * 16;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox16[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox16[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox16[y][x][i].w = predw;
                    rbox16[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_32 == 1) {
        for (y = 0, k = 54240; y < 12; y++) {
            for (x = 0; x < 20; x++) {
                for (i = 0; i < 3; i++) {
                    if (i == 0)
                        h32=w32=96;
                    else if (i == 1)
                        h32=w32=160;
                    else
                        h32=w32=256;

                    float s_kx = w32;
                    float s_ky = h32;
                    float cx = (x + 0.5) * 32;
                    float cy = (y + 0.5) * 32;

                    pred_ctrx = cx + bbox_buf[k] * 0.1 * s_kx;
                    pred_ctry = cy + bbox_buf[k+1]* 0.1 * s_ky;
                    predw = exp((bbox_buf[k+2])*0.2) * s_kx;
                    predh = exp((bbox_buf[k+3])*0.2) * s_ky;

                    rbox32[y][x][i].x = (pred_ctrx-0.5*(predw));
                    rbox32[y][x][i].y = (pred_ctry-0.5*(predh));
                    rbox32[y][x][i].w = predw;
                    rbox32[y][x][i].h = predh;
                    k += 4;
                }
            }
        }
    }

    if (valid_32 == 1) {
        do_nms_sort(rpbox32, rprob32, 720);
        if (valid_16 == 1) {
            do_nms_sort(rpbox16, rprob16, 2760);
            do_global_sort(rpbox32,rpbox16,rprob32,rprob16,720,2760);
            if (valid_8 == 1) {
                do_nms_sort(rpbox8, rprob8, 10800);
                do_global_sort(rpbox16, rpbox8, rprob16, rprob8, 2760,10800);
                face_set_result(720, rpbox32, rprob32, face_out);
                face_set_result(2760, rpbox16, rprob16, face_out);
                face_set_result(10800, rpbox8, rprob8, face_out);
            }
            else {
                face_set_result(720, rpbox32, rprob32, face_out);
                face_set_result(2760, rpbox16, rprob16, face_out);
            }
        }
        else if (valid_8 == 1 && valid_16 == 0) {
            do_nms_sort(rpbox8, rprob8, 10800);
            face_set_result(720, rpbox32, rprob32, face_out);
            face_set_result(10800, rpbox8, rprob8, face_out);
        }
        else
            face_set_result(720, rpbox32, rprob32, face_out);
    }

    if (valid_16 == 1 && valid_32 == 0) {
        do_nms_sort(rpbox16, rprob16, 2760);
        if (valid_8 == 1) {
            do_nms_sort(rpbox8, rprob8, 10800);
            do_global_sort(rpbox16, rpbox8, rprob16, rprob8, 2760,10800);
            face_set_result(2760, rpbox16, rprob16, face_out);
            face_set_result(10800, rpbox8, rprob8, face_out);
        }
        else
            face_set_result(2760, rpbox16, rprob16, face_out);
    }

    if (valid_8 == 1 && valid_16 == 0 && valid_32 == 0) {
        do_nms_sort(rpbox8, rprob8, 10800);
        face_set_result(10800, rpbox8, rprob8, face_out);
    }
}