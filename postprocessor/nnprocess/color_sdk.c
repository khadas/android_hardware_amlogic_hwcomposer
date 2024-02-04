/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aicolor"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nn_util.h>
#include <nn_sdk.h>
#include <dlfcn.h>
#include <utils/Log.h>
#include "color_sdk.h"

static void *mSdkHandle;
static int (*func_switchInputBuffer)(void *, void *,unsigned int);
static void* (*func_outputGet)(void* , aml_output_config_t);
static void* (*func_create)(aml_config*);
static int (*func_destroy)(void*);

void* color_process_network(void *qcontext, unsigned char *in_addr) {
    int ret = 0;
    nn_output *outdata = NULL;

    aml_output_config_t outconfig;
    outconfig.typeSize = sizeof(aml_output_config_t);
    outconfig.mdType = CUSTOM_NETWORK;
    outconfig.format = AML_OUTDATA_RAW;

    if ((func_switchInputBuffer != NULL)
        && (func_outputGet != NULL)) {
        ret = func_switchInputBuffer(qcontext, (void*)in_addr, 0);
        if (ret != 0) {
            ALOGE("func_switchInputBuffer error\n");
            return NULL;
        }
        outdata = (nn_output *)func_outputGet(qcontext, outconfig);
        if (outdata == NULL) {
            ALOGE("aml_module_output_get error\n");
            return NULL;
        }
        return (void*)(outdata->out[0].buf);
    } else {
        ALOGE("%s:interface don't implement.\n", __FUNCTION__);
        return NULL;
    }
}

void* color_init(const char *path) {
    ALOGD("enter %s.\n", __FUNCTION__);
    void *qcontext = NULL;
    if (func_create != NULL) {
        aml_config config;
        memset(&config,0,sizeof(aml_config));
        config.path = path;
        config.nbgType = NN_ADLA_FILE;
        config.modelType = ADLA_LOADABLE;

        qcontext = func_create(&config);
    } else {
        ALOGD("%s: interface don't implement.\n", __FUNCTION__);
    }
    return qcontext;
}

void* color_uninit(void* context) {
    int ret = 0;
    if (func_destroy != NULL)
        ret = func_destroy(context);
    else
        ALOGE("%s:interface don't implement.\n", __FUNCTION__);

    if (mSdkHandle != NULL) {
        dlclose(mSdkHandle);
        mSdkHandle = NULL;
    }

    return NULL;
}

int isColorInterfaceImplement() {
    ALOGD("enter %s.\n", __FUNCTION__);

    int ret = 0;
    if (mSdkHandle == NULL)
        mSdkHandle = dlopen("libnnsdk.so", RTLD_NOW);

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

        func_switchInputBuffer = (int (*)(void *, void *, unsigned int))
            dlsym(mSdkHandle, "aml_util_switchInputBuffer");
        if (func_switchInputBuffer == NULL)
            ALOGD("func_switchInputBuffer don't implement.\n");

        if ((func_create == NULL)
            || (func_switchInputBuffer == NULL)
            || (func_outputGet == NULL)
            || (func_destroy == NULL)) {
            ALOGE("NN interface don't implement.\n");
        } else {
            ALOGD("NN interface is implement in.\n");
            ret = 1;
        }
    }

    return ret;
}
