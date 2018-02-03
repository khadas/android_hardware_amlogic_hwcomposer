/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <unistd.h>
#include <getopt.h>

#include <misc.h>
#include <DebugHelper.h>
#include <MesonLog.h>

ANDROID_SINGLETON_STATIC_INSTANCE(DebugHelper)

#define DEBUG_HELPER_ENABLE_PROP "sys.hwc.debug"
#define DEBUG_HELPER_COMMAND "sys.hwc.debug.command"
#define MAX_DEBUG_COMMANDS (5)

#define INT_PARAMERTER_TO_BOOL(param)  \
        atoi(param) > 0 ? true : false

#define CHECK_CMD_INT_PARAMETER() \
    if (i > paramNum) { \
        MESON_LOGE("param number is not correct.\n");   \
        break;  \
    }


DebugHelper::DebugHelper() {
    clearPersistCmd();
    clearOnePassCmd();
}

DebugHelper::~DebugHelper() {
}

void DebugHelper::clearOnePassCmd() {
    mSaveLayer = false;
    mDumpUsage = false;
}

void DebugHelper::clearPersistCmd() {
    mDumpDetail = false;

    mLogFps = false;
    mLogCompositionFlow = false;
    mLogLayerStatistic = false;

    mHideLayer = false;
    mDiscardInFence = false;
    mDiscardOutFence = false;
}

void DebugHelper::resolveCmd() {
    clearOnePassCmd();
    mEnabled = sys_get_bool_prop(DEBUG_HELPER_ENABLE_PROP, false);

    if (mEnabled) {
        char debugCmd[128] = {0};
        if (sys_get_string_prop(DEBUG_HELPER_COMMAND, debugCmd) > 0 && debugCmd[0]) {
            mDumpUsage = false;

            int paramNum = 0;
            char * paramArray [MAX_DEBUG_COMMANDS + 1] = {NULL};
            char * delimit = " ";
            char * param = strtok(debugCmd, delimit);

            while (param != NULL) {
                MESON_LOGE("param [%s]\n", param);
                paramArray[paramNum] = param;
                paramNum++;
                if (paramNum >= MAX_DEBUG_COMMANDS)
                    break;
                param = strtok(NULL, delimit);
            }

            for (int i = 0; i < paramNum; i++) {
                MESON_LOGE("Parse command [%s]", paramArray[i]);
                if (strcmp(paramArray[i], "--clear") == 0) {
                    clearPersistCmd();
                    clearOnePassCmd();
                    continue;
                }

                if (strcmp(paramArray[i], "--detail") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mDumpDetail = INT_PARAMERTER_TO_BOOL(paramArray[i]);
                    continue;
                }

                if (strcmp(paramArray[i], "--fps") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mLogFps = INT_PARAMERTER_TO_BOOL(paramArray[i]);
                    continue;
                }

                if (strcmp(paramArray[i], "--composition-info") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mLogCompositionFlow = INT_PARAMERTER_TO_BOOL(paramArray[i]);

                    continue;
                }

                if (strcmp(paramArray[i], "--layer-statistic") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mLogLayerStatistic = INT_PARAMERTER_TO_BOOL(paramArray[i]);
                    continue;
                }

                if (strcmp(paramArray[i], "--infence") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mDiscardInFence = INT_PARAMERTER_TO_BOOL(paramArray[i]);
                    continue;
                }

                if (strcmp(paramArray[i], "--outfence") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mDiscardOutFence = INT_PARAMERTER_TO_BOOL(paramArray[i]);
                    continue;
                }

                if (strcmp(paramArray[i], "--hide") == 0) {
                    mHideLayer = true;
                    continue;
                }

                if (strcmp(paramArray[i], "--show") == 0) {
                    mHideLayer = false;
                    continue;
                }

                if (strcmp(paramArray[i], "--save") == 0) {
                    mSaveLayer = true;
                    continue;
                }
            }

            /*Need permission to reset prop.*/
            sys_set_prop(DEBUG_HELPER_COMMAND, "");
        } else {
            mDumpUsage = true;
        }
    }
}

bool DebugHelper::isEnabled() {
    return sys_get_bool_prop(DEBUG_HELPER_ENABLE_PROP, false);
}

uint32_t DebugHelper::getSaveLayer() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

uint32_t DebugHelper::getHideLayer() {
    MESON_LOG_EMPTY_FUN();
    return 0;
}

void DebugHelper::dump(String8 & dumpstr) {
    if (!mEnabled)
        return;

    if (mDumpUsage) {
        static const char * usage =
            "Passed command string to prop " DEBUG_HELPER_COMMAND " to debug.\n"
            "Supported commands:\n"
            "\t --clear: clear all debug flags.\n"
            "\t --detail 0|1: enable/dislabe dump detail internal info.\n"
            "\t --fps 0|1: start/stop log fps.\n"
            "\t --composition-info 0|1: enable/disable composition detail info.\n"
            "\t --layer-statistic 0|1:  enable/disable log layer statistic for hw analysis. \n"
            "\t --infence 0 | 1: pass in fence to display, or handle it in hwc.\n"
            "\t --outfence 0 | 1: return display out fence, or handle it in hwc.\n"
            "\t --hide/--show [zorder]: hide/unhide specific layers by zorder. \n"
            "\t --save [zorder]: save specific layer's raw data by zorder. \n";

        dumpstr.append(usage);
    }

}

