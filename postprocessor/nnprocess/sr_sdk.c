/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_TAG "sr_sdk"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sr_sdk.h"
#include <nn_sdk.h>
#include <dlfcn.h>
#include <log/log.h>

static void *mHandle;
static int (*func_switchInputBuffer)(void *, void *,unsigned int);
static int (*func_switchOutputBuffer)(void *, void *, unsigned int);
static void* (*func_outputGet)(void *, aml_output_config_t);
static void* (*func_create)(aml_config*);
static int (*func_destory)(void*);

int nn_process_network(void *qcontext,
                             unsigned char *in_addr,
                             unsigned char *out_addr) {
    //ALOGD("enter %s.\n", __FUNCTION__);
    int ret = -1;
    if ((func_switchInputBuffer != NULL)
        && (func_switchOutputBuffer != NULL)
        && (func_outputGet != NULL)) {
        aml_output_config_t outconfig;
        outconfig.mdType = CUSTOM_NETWORK;
        outconfig.format = AML_OUTDATA_DMA;
        outconfig.perfMode = AML_PERF_INFERRENCE;

        ret = func_switchInputBuffer(qcontext, (void*)in_addr, 0);
        if (ret != 0) {
            //ALOGE("aml_util_swapInputBuffer error\n");
        } else {
            ret = func_switchOutputBuffer(qcontext, (void*)out_addr,  0);
            if (ret != 0) {
                //ALOGE("aml_util_swapOutputBuffer error\n");
            } else {
                func_outputGet(qcontext,  outconfig);
            }
        }
    }else {
        ALOGD("%s: interface don't implement.\n", __FUNCTION__);
    }

    return ret;
}

void* nn_init(const char *path) {
    ALOGD("enter %s.\n", __FUNCTION__);
    void *qcontext = NULL;
    if (func_create != NULL) {
        aml_config config;
        memset(&config,0,sizeof(aml_config));
        config.path = path;
        config.nbgType = NN_NBG_FILE;
        config.modelType = PYTORCH;

        qcontext = func_create(&config);
    } else {
        ALOGD("%s: interface don't implement.\n", __FUNCTION__);
    }
    return qcontext;
}

int nn_uninit(void* context) {
    ALOGD("enter %s.\n", __FUNCTION__);

    int ret = -1;
    if (func_destory != NULL) {
        ret = func_destory(context);
    } else {
        ALOGD("%s: interface don't implement.\n", __FUNCTION__);
    }

    if (mHandle != NULL) {
        dlclose(mHandle);
        mHandle = NULL;
    }

    return ret;
}

int isInterfaceImplement() {
    ALOGD("enter %s.\n", __FUNCTION__);

    int ret = 0;
    if (mHandle == NULL)
        mHandle = dlopen("libnnsdk.so", RTLD_NOW);

    if (mHandle == NULL) {
        ALOGE("open libnnsdk.so fail: %s.\n", dlerror());
    } else {
        func_switchInputBuffer = (int (*)(void *, void *, unsigned int))
            dlsym(mHandle, "aml_util_switchInputBuffer");
        if (func_switchInputBuffer == NULL)
            ALOGD("func_switchInputBuffer don't implement.\n");

        func_switchOutputBuffer = (int (*)(void *, void *, unsigned int))
            dlsym(mHandle, "aml_util_switchOutputBuffer");
        if (func_switchOutputBuffer == NULL)
            ALOGD("func_switchOutputBuffer don't implement.\n");

        func_outputGet = (void *(*)(void *, aml_output_config_t))
            dlsym(mHandle, "aml_module_output_get");
        if (func_outputGet == NULL)
            ALOGD("func_outputGet don't implement.\n");

        func_create = (void *(*)(aml_config*))
            dlsym(mHandle, "aml_module_create");
        if (func_create == NULL)
            ALOGD("func_create don't implement.\n");

        func_destory = (int(*)(void*))dlsym(mHandle, "aml_module_destroy");
        if (func_destory == NULL)
            ALOGD("func_destory don't implement.\n");

        if ((func_switchInputBuffer == NULL)
            || (func_switchOutputBuffer == NULL)
            || (func_outputGet == NULL)
            || (func_create == NULL)
            || (func_destory == NULL)) {
            ALOGE("NN interface don't implement in libnnsdk.so.\n");
        } else {
            ALOGD("NN interface is implement in libnnsdk.so.\n");
            ret = 1;
        }

    }

    return ret;
}
