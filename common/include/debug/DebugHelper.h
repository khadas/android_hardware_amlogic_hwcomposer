/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <BasicTypes.h>

class DebugHelper : public Singleton<DebugHelper> {
public:
    DebugHelper();
    ~DebugHelper();

    void dump(String8 & dumpstr);

    void resolveCmd();

    /*dump internal info in hal dumpsys.*/
    inline bool dumpDetailInfo() {return mDumpDetail;}

    /*log informations.*/
    inline bool logCompositionFlow() {return mLogCompositionFlow;}
    inline bool logFps() {return mLogFps;}
    inline bool logLayerStatistic() {return mLogLayerStatistic;}

    /*save layer's raw data by zorder.*/
    inline bool saveLayer2File() {return mSaveLayer;}
    inline uint32_t getSaveLayer();

    /*for fence debug*/
    inline bool discardInFence() {return mDiscardInFence;}
    inline bool discardOutFence() {return mDiscardOutFence;}

    /*for layer compostion debug*/
    inline bool hideLayer() {return mHideLayer;}
    inline uint32_t getHideLayer();

protected:
    bool isEnabled();
    void clearOnePassCmd();
    void clearPersistCmd();

protected:

    bool mEnabled;
    bool mDumpUsage;
    bool mDumpDetail;

    bool mLogCompositionFlow;
    bool mLogFps;
    bool mLogLayerStatistic;

    bool mSaveLayer;
    bool mHideLayer;

    /*handle osd in/out fence in hwc.*/
    bool mDiscardInFence;
    bool mDiscardOutFence;
};

#endif
