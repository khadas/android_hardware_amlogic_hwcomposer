/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC2_DISPLAY_PIPE_MGR_H
#define HWC2_DISPLAY_PIPE_MGR_H

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>

#include <BasicTypes.h>
#include <HwcDisplay.h>
#include <HwcVsync.h>
#include <HwcConfig.h>
#include <HwDisplayEventListener.h>
#include <VdinPostProcessor.h>

/*
 * manage  <Hwc2Display, HwDisplayPipe> according to
 * hwcconfig.
 */

/*requests*/
enum {
    rPostProcessorStart = 1 << 0,
    rPostProcessorStop = 1 << 1,
    rPostProcessorStartExt = 1 << 2,
    rPostProcessorStopExt  = 1 << 3,

    rDisplayModeSet = 1 << 4,
    rDisplayModeSetExt = 1 << 5,

    rCalibrationSet = 1 << 6,
    rCalibrationSetExt = 1 << 7,

    rKeystoneEnable = 1 << 8,
    rKeystoneDisable = 1 << 9,
};

/*events*/
enum {
    eHdmiPlugIn = 1 << 0,
    eHdmiPlugOut = 1<< 1,

    eDisplayModeChange = 1 << 2,
    eDisplayModeExtChange = 1 << 3,
};


class HwcDisplayPipeMgr
    :   public android::Singleton<HwcDisplayPipeMgr>,
        public HwDisplayEventHandler {

public:
    HwcDisplayPipeMgr();
    ~HwcDisplayPipeMgr();

    int32_t setHwcDisplay(
        uint32_t disp, std::shared_ptr<HwcDisplay> & hwcDisp);

    int32_t initDisplays();
    int32_t update(uint32_t flags);

    void handle(drm_display_event event, int val);

protected:
    class PipeCfg {
        public:
            int32_t hwcCrtcId;
            drm_connector_type_t hwcConnectorType;
            hwc_post_processor_t hwcPostprocessorType;

            int32_t modeCrtcId;
            drm_connector_type_t modeConnectorType;
    };

    class PipeStat {
    public:
        PipeStat();
        ~PipeStat();

        PipeCfg cfg;

        std::shared_ptr<HwcDisplay> hwcDisplay;
        std::shared_ptr<HwDisplayCrtc> hwcCrtc;
        std::vector<std::shared_ptr<HwDisplayPlane>> hwcPlanes;
        std::shared_ptr<HwDisplayConnector> hwcConnector;
        std::shared_ptr<HwcVsync> hwcVsync;
        std::shared_ptr<HwcPostProcessor> hwcPostProcessor;

        std::shared_ptr<HwcModeMgr> modeMgr;
        std::shared_ptr<HwDisplayCrtc> modeCrtc;
        std::shared_ptr<HwDisplayConnector> modeConnector;
    };

protected:
    int32_t updatePipe();

    int32_t getDisplayPipe(
        uint32_t hwcdisp, PipeCfg & cfgor);
    int32_t getCrtc(
        int32_t crtcid, std::shared_ptr<HwDisplayCrtc> & crtc);
    int32_t getPlanes(
        int32_t crtcid, std::vector<std::shared_ptr<HwDisplayPlane>> & planes);
    int32_t getConnector(
        drm_connector_type_t type, std::shared_ptr<HwDisplayConnector> & connector);
    int32_t getPostProcessor(
        hwc_post_processor_t type, std::shared_ptr<HwcPostProcessor> & processor);
    drm_connector_type_t chooseConnector(hwc_connector_t config);

protected:
    std::vector<std::shared_ptr<HwDisplayPlane>> mPlanes;
    std::vector<std::shared_ptr<HwDisplayCrtc>> mCrtcs;
    std::map<drm_connector_type_t, std::shared_ptr<HwDisplayConnector>> mConnectors;
    std::map<hwc_post_processor_t, std::shared_ptr<HwcPostProcessor>> mProcessors;

    hwc_pipe_policy_t mPipePolicy;
    std::map<uint32_t, std::shared_ptr<PipeStat>> mPipeStats;

    std::mutex mMutex;

    bool mPostProcessor;
};

#endif
