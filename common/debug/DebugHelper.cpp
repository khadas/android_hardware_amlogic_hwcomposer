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

#define DEBUG_HELPER_ENABLE_PROP "vendor.hwc.debug"
#define DEBUG_HELPER_COMMAND "vendor.hwc.debug.command"

#define MAX_DEBUG_COMMANDS (20)

#define INT_PARAMERTER_TO_BOOL(param)  \
        atoi(param) > 0 ? true : false

#define CHECK_CMD_INT_PARAMETER() \
    if (i >= paramNum) { \
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
    mDumpUsage = false;
}

void DebugHelper::clearPersistCmd() {
    mDisableUiHwc = false;
    mDumpDetail = false;

    mLogFps = false;
    mLogCompositionInfo = false;
    mLogLayerStatistic = false;

    mDiscardInFence = false;
    mDiscardOutFence = false;

    mHideLayers.clear();
    mSaveLayers.clear();
    mHidePlanes.clear();

    mDebugHideLayer = false;
    mDebugHidePlane = false;
}

void DebugHelper::addHideLayer(int id) {
    bool bExist = false;
    std::vector<int>::iterator it;
    for (it = mHideLayers.begin(); it < mHideLayers.end(); it++) {
        if (*it == id) {
            bExist = true;
        }
    }

    if (!bExist) {
        mHideLayers.push_back(id);
    }
}

void DebugHelper::removeHideLayer(int id) {
    std::vector<int>::iterator it;
    for (it = mHideLayers.begin(); it < mHideLayers.end(); it++) {
        if (*it == id) {
            mHideLayers.erase(it);
            break;
        }
    }
}

void DebugHelper::addHidePlane(int id) {
    bool bExist = false;
    std::vector<int>::iterator it;
    for (it = mHidePlanes.begin(); it < mHidePlanes.end(); it++) {
        if (*it == id) {
            bExist = true;
        }
    }

    if (!bExist) {
        mHidePlanes.push_back(id);
    }
}

void DebugHelper::removeHidePlane(int id) {
    std::vector<int>::iterator it;
    for (it = mHidePlanes.begin(); it < mHidePlanes.end(); it++) {
        if (*it == id) {
            mHidePlanes.erase(it);
            break;
        }
    }
}


void DebugHelper::resolveCmd() {
    clearOnePassCmd();
    mEnabled = sys_get_bool_prop(DEBUG_HELPER_ENABLE_PROP, false);

    if (mEnabled) {
        char debugCmd[128] = {0};
        if (sys_get_string_prop(DEBUG_HELPER_COMMAND, debugCmd) > 0 && debugCmd[0]) {
            mDumpUsage = false;

            int paramNum = 0;
            const char * delimit = " ";
            char * paramArray [MAX_DEBUG_COMMANDS + 1] = {NULL};
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

                if (strcmp(paramArray[i], "--nohwc") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    mDisableUiHwc = INT_PARAMERTER_TO_BOOL(paramArray[i]);
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
                    mLogCompositionInfo = INT_PARAMERTER_TO_BOOL(paramArray[i]);
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
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    int layerId = atoi(paramArray[i]);
                    if (layerId < 0) {
                        MESON_LOGE("Show invalid layer (%d)", layerId);
                    } else {
                        addHideLayer(layerId);
                        mDebugHideLayer = true;
                    }
                    continue;
                }

                if (strcmp(paramArray[i], "--show") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    int layerId = atoi(paramArray[i]);
                    if (layerId < 0) {
                        MESON_LOGE("Show invalid layer (%d)", layerId);
                    } else {
                        removeHideLayer(layerId);
                        mDebugHideLayer = true;
                    }
                    continue;
                }

                if (strcmp(paramArray[i], "--hide-plane") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    int planeId = atoi(paramArray[i]);
                    if (planeId < 0) {
                        MESON_LOGE("Show invalid plane (%d)", planeId);
                    } else {
                        addHidePlane(planeId);
                        mDebugHidePlane = true;
                    }
                    continue;
                }

                if (strcmp(paramArray[i], "--show-plane") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    int planeId = atoi(paramArray[i]);
                    if (planeId < 0) {
                        MESON_LOGE("Show invalid plane (%d)", planeId);
                    } else {
                        removeHidePlane(planeId);
                        mDebugHidePlane = true;
                    }
                    continue;
                }

                if (strcmp(paramArray[i], "--save") == 0) {
                    i++;
                    CHECK_CMD_INT_PARAMETER();
                    int layerId = atoi(paramArray[i]);
                    if (layerId < 0) {
                        MESON_LOGE("Save layer (%d)", layerId);
                    } else {
                        mSaveLayers.push_back(layerId);
                    }
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

void DebugHelper::removeDebugLayer(int id __unused) {
    #if 0/*useless now.*/
    /*remove hide layer*/
    removeHideLayer(id);

    /*remove save layer*/
    std::vector<int>::iterator it;
    for (it = mSaveLayers.begin(); it < mSaveLayers.end(); it++) {
        if (*it == id) {
            mSaveLayers.erase(it);
            break;
        }
    }
    #endif
}

void DebugHelper::dump(String8 & dumpstr) {
    if (!mEnabled)
        return;

    if (mDumpUsage) {
        static const char * usage =
            "Pass command string to prop " DEBUG_HELPER_COMMAND " to debug.\n"
            "Supported commands:\n"
            "\t --clear: clear all debug flags.\n"
            "\t --nohwc 0|1:  choose osd/UI hwcomposer.\n"
            "\t --detail 0|1: enable/dislabe dump detail internal info.\n"
            "\t --infence 0 | 1: pass in fence to display, or handle it in hwc.\n"
            "\t --outfence 0 | 1: return display out fence, or handle it in hwc.\n"
            "\t --composition-info 0|1: enable/disable composition detail info.\n"
            "\t --hide/--show [layerId]: hide/unhide specific layers by zorder. \n"
            "\t --hide-plane/--show-plane [planeId]: hide/unhide specific plane by plane id. \n"
            "\t --fps 0|1: start/stop log fps.\n"
            "\t --layer-statistic 0|1:  enable/disable log layer statistic for hw analysis. \n"
            "\t --save [layerId]: save specific layer's raw data by layer id. \n";

        dumpstr.append("\nMesonHwc debug helper:\n");
        dumpstr.append(usage);
        dumpstr.append("\n");
    } else {
        std::vector<int>::iterator it;

        dumpstr.append("Debug Command:\n");
        dumpstr.appendFormat("--nohwc (%d)\n", mDisableUiHwc);
        dumpstr.appendFormat("--detail (%d)\n", mDumpDetail);
        dumpstr.appendFormat("--infence (%d)\n", mDiscardInFence);
        dumpstr.appendFormat("--outfence (%d)\n", mDiscardOutFence);
        dumpstr.appendFormat("--composition-info (%d)\n", mLogCompositionInfo);
        dumpstr.appendFormat("--fps (%d)\n", mLogFps);
        dumpstr.appendFormat("--layer-statistic (%d)\n", mLogLayerStatistic);

        dumpstr.append("--hide-plane (");
        for (it = mHidePlanes.begin(); it < mHidePlanes.end(); it++) {
            dumpstr.appendFormat("%d    ", *it);
        }
        dumpstr.append(")\n");

        dumpstr.append("--hide (");
        for (it = mHideLayers.begin(); it < mHideLayers.end(); it++) {
            dumpstr.appendFormat("%d    ", *it);
        }
        dumpstr.append(")\n");

        dumpstr.append("--save (");
        for (it = mSaveLayers.begin(); it < mSaveLayers.end(); it++) {
            dumpstr.appendFormat("%d    ", *it);
        }
        dumpstr.append(")\n");
    }

}

