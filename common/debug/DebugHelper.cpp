/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <misc.h>
#include <DebugHelper.h>
#include <MesonLog.h>

ANDROID_SINGLETON_STATIC_INSTANCE(DebugHelper)

#define DEBUG_HELPER_ENABLE_PROP "sys.hwc.debug.mode"
#define DEBUG_HELPER_COMMAND "sys.hwc.debug.command"

DebugHelper::DebugHelper() {
    clearDebugFlags();
}

DebugHelper::~DebugHelper() {
}

void DebugHelper::clearDebugFlags() {
    mDumpUsage = false;
    mDumpDetail = false;

    mLogFps = false;
    mLogCompositionFlow = false;
    mLogLayerStatistic = false;

    mSaveLayer = false;
    mHideLayer = false;
    mDiscardInFence = false;
    mDiscardOutFence = false;
}

void DebugHelper::resolveCmd() {
    if (isEnabled()) {
        char debugCmd[128];
        sys_get_string_prop(DEBUG_HELPER_COMMAND, debugCmd);
        if (debugCmd[0] == 0)
            mDumpUsage = true;

        MESON_LOG_EMPTY_FUN();

#if 0
        bool mDumpLayers;
        bool mDumpPlanes;
        bool mDumpComposition;
        bool mDumpDisplayMgr;

        bool mDumpUsage;

        bool mDebugFps;
        /*print layer statistic for collect layer attribute.*/
        bool mDebugLayerStatistic;

        /*abandon specific layers to dummy composer*/
        bool mHideLayer;

        /*handle osd in/out fence in hwc.*/
        bool mIgnorDisplayInFence;
        bool mIgnorDisplayOutFence;
#endif
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
    if (isEnabled() == false)
        return;

    if (mDumpUsage) {
        static const char * usage = "Supported command list in MesonHwc:\n"
            "--clear: clear all debug flags.\n"
            "--enable_detail/--disable_detail: enable/dislabe dump detail internal info.\n"
            "--enable_fps/--disable_fps: start/stop log fps.\n"
            "--enable_comps/--disable_comps: enable/disable compostion flow info.\n"
            "--enable_statistic/--disable_statistic:  enable/disable log layer statistic for hw analysis¡\n"
            "--enable_hide/--disable_hide [zorder]: enable/disable hide specific layers by zorder. \n"
            "--enable_infence/--disable_infence: pass in fence to display, or handle it in hwc.\n"
            "--enable_outfence/--disable_outfence: return display out fence, or handle it in hwc.\n"
            "save [zorder]: save specific layer's raw data by zorder. \n";

        dumpstr.append(usage);
    }

    MESON_LOG_EMPTY_FUN();
    /*DUMP OPEND*/
}

