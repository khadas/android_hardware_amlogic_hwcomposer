/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aiface"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nn_util.h>
#include <nn_sdk.h>
#include <dlfcn.h>
#include <utils/Log.h>
#include "nn_demo.h"

//static face_landmark5_out_t face_landmark5_result;

static void *mSdkHandle;
static void *mDemoHandle;
static int (*func_inputSet)(void *, nn_input *);
static void* (*func_outputGet)(void* , aml_output_config_t);
static void* (*func_create)(aml_config*);
static int (*func_destroy)(void*);
static void* (*func_facedet_outputGet)(void *, aml_output_config_t, face_landmark5_out_t *);
static int (*func_switchInputBuffer)(void *, void *,unsigned int);

void* face_process_network(void *qcontext, unsigned char *in_addr, face_landmark5_out_t *nnOut) {
    int ret = 0;

    memset(nnOut, 0, sizeof(face_landmark5_out_t));

    aml_output_config_t outconfig;
    outconfig.typeSize = sizeof(aml_output_config_t);
    outconfig.mdType = CUSTOM_NETWORK;

    if (func_switchInputBuffer != NULL) {
        ret = func_switchInputBuffer(qcontext, (void*)in_addr, 0);
        if (ret != 0) {
            ALOGE("aml_util_swapInputBuffer error\n");
            return NULL;
        }

        func_facedet_outputGet(qcontext, outconfig, nnOut);
        return (void*)nnOut;
    } else {
        ALOGE("%s:interface don't implement.\n", __FUNCTION__);
        return NULL;
    }
}

void* face_init(const char *path, int inputWidth __unused, int inputHeight __unused, int inputChannel __unused) {
    if (func_create != NULL) {
        void *qcontext = NULL;
        aml_config config;

        memset(&config, 0, sizeof(aml_config));
        config.path = path;
        config.nbgType = NN_NBG_FILE;
        config.modelType = CAFFE;
        qcontext = func_create(&config);

        if (qcontext == NULL) {
            ALOGE("amlnn_init is fail\n");
            return NULL;
        }

        return qcontext;
    }else {
        ALOGE("%s:interface don't implement.\n", __FUNCTION__);
        return NULL;
    }
}

void* face_uninit(void* context) {
    int ret = 0;

    if (func_destroy != NULL)
        ret = func_destroy(context);
    else
        ALOGE("%s:interface don't implement.\n", __FUNCTION__);

    if (mSdkHandle != NULL) {
        dlclose(mSdkHandle);
        mSdkHandle = NULL;
    }
    if (mDemoHandle != NULL) {
        dlclose(mDemoHandle);
        mDemoHandle = NULL;
    }

    return NULL;
}

int isFaceInterfaceImplement() {
    ALOGD("enter %s.\n", __FUNCTION__);

    int ret = 0;
    if (mSdkHandle == NULL)
        mSdkHandle = dlopen("libnnsdk.so", RTLD_NOW);
    if (mDemoHandle == NULL)
        mDemoHandle = dlopen("libnndemo.so", RTLD_NOW);

    if (mSdkHandle == NULL) {
        ALOGE("open libnnsdk.so fail: %s.\n", dlerror());
    } else {
        func_outputGet = (void *(*)(void *, aml_output_config_t))
            dlsym(mSdkHandle, "aml_module_output_get");
        if (func_outputGet == NULL)
            ALOGD("func_outputGet don't implement.\n");

        func_create = (void *(*)(aml_config*))
            dlsym(mSdkHandle, "aml_module_create");
        if (func_create == NULL)
            ALOGD("func_create don't implement.\n");

        func_destroy = (int(*)(void*))dlsym(mSdkHandle, "aml_module_destroy");
        if (func_destroy == NULL)
            ALOGD("func_destroy don't implement.\n");

        func_inputSet = (int(*)(void*, nn_input *))
            dlsym(mSdkHandle, "aml_module_input_set");
        if (func_inputSet == NULL)
            ALOGD("func_inputSet don't implement.\n");

        func_switchInputBuffer = (int (*)(void *, void *, unsigned int))
            dlsym(mSdkHandle, "aml_util_switchInputBuffer");
        if (func_switchInputBuffer == NULL)
            ALOGD("func_switchInputBuffer don't implement.\n");

        if ((func_create == NULL)
            || (func_inputSet == NULL)
            || (func_outputGet == NULL)
            || (func_destroy == NULL)
            || (func_switchInputBuffer == NULL)) {
            ALOGE("NN interface don't implement.\n");
        } else {
            ALOGD("NN interface is implement in.\n");
            ret = 1;
        }
    }

    if (mDemoHandle == NULL) {
        ALOGE("open libnndemo.so fail: %s.\n", dlerror());
    } else {
        func_facedet_outputGet = (void *(*)(void *, aml_output_config_t, face_landmark5_out_t *))
            dlsym(mDemoHandle, "aml_face_detection_output_get");
        if (func_facedet_outputGet == NULL)
            ALOGD("func_facedet_outputGet don't implement.\n");

        if (func_facedet_outputGet == NULL) {
            ALOGE("NN interface don't implement.\n");
        } else {
            ALOGD("NN interface is implement in.\n");
            ret = 1;
        }
    }

    return ret;
}