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

#include <utils/Singleton.h>
#include <utils/String8.h>
using namespace android;

#include <map>
#include <vector>

class DebugHelper : public Singleton<DebugHelper> {
public:
    DebugHelper();
    ~DebugHelper();

    void dump(String8 & dumpstr);

    void resolveCmd();

    /*dump internal info in hal dumpsys.*/
    inline bool dumpDetailInfo() {return mDumpDetail;}
    inline bool enableVsyncDetail() {return mEnableVsyncDetail;}

    /*log informations.*/
    inline bool logCompositionDetail() {return mLogCompositionDetail;}
    inline bool monitorDeviceComposition() {return mMonitorDeviceComposition;}
    inline uint32_t deviceCompositionThreshold() {return mDeviceCompositionThreshold;}
    inline bool logFps() {return mLogFps;}
    inline bool enableLogVerboser() {return mLogVerbose;}

    /*check if UI/osd hwcomposer disabled.*/
    bool disableUiHwc() {return mDisableUiHwc;}

    /*for fence debug*/
    inline bool discardInFence() {return mDiscardInFence;}
    inline bool discardOutFence() {return mDiscardOutFence;}

    /*for layer compostion debug*/
    inline void getHideLayers(std::vector<int> & layers) {layers = mHideLayers;}
    inline bool debugHideLayers() {return mDebugHideLayer;}

    inline void getPlaneDebugFlags(std::map<int, int> & planeFlags) {planeFlags = mPlanesDebugFlag;}
    inline bool debugPlanes() {return mDebugPlane;}

    /*save layer's raw data by layerid.*/
    inline void getSavedLayers(std::vector<int> & layers) {layers = mSaveLayers;}

    /*remove debug layer*/
    void removeDebugLayer(int id);

    /* for vpu scale limit */
    inline float getScaleLimit() {return mScaleLimit;}

    inline bool enableDrmBlockMode() { return mDrmBlockMode; }

protected:
    bool isEnabled();
    void clearOnePassCmd();
    void clearPersistCmd();

    void addHideLayer(int id);
    void removeHideLayer(int id);

    void addPlaneDebugFlag(int id, int flag);
    void removePlaneDebugFlag(int id, int flag);

protected:
    bool mEnabled;
    bool mDumpUsage;
    bool mDisableUiHwc;
    bool mDumpDetail;
    bool mEnableVsyncDetail;

    bool mLogCompositionDetail;
    bool mLogFps;
    bool mLogVerbose;

    bool mMonitorDeviceComposition;
    uint32_t mDeviceCompositionThreshold;
    float mScaleLimit;

    std::vector<int> mSaveLayers;
    std::vector<int> mHideLayers;
    bool mDebugHideLayer;

    std::map<int, int> mPlanesDebugFlag;
    bool mDebugPlane;

    /*handle osd in/out fence in hwc.*/
    bool mDiscardInFence;
    bool mDiscardOutFence;
    bool mDrmBlockMode;
};
#endif
