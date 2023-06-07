/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VIDEO_PROCESSORS_MANAGER_H
#define VIDEO_PROCESSORS_MANAGER_H
#include <FbProcessor.h>
#include <VideoComposerDev.h>
#include <HwcVideoPlane.h>

class VideoProcessorsManager {
public:
    VideoProcessorsManager();
    ~VideoProcessorsManager();

    void prepare(std::vector<std::shared_ptr<DrmFramebuffer>> & fbs);

    int setUpAiSrProcessor();
    int setUpAiPqProcessor();
    int setUpAiColorProcessor();
    int setUpDiProcessor();
    int setUpAllProcessors();

    void setProcessors(std::shared_ptr<DrmFramebuffer>& fb);
    void setup(std::vector<std::shared_ptr<DrmFramebuffer>> & fbs);

    void tearDownSrProcessors();
    void tearDownPqProcessors();
    void tearDownColorProcessors();
    void tearDownDiProcessors();
    void tearDownAllProcessors();

    bool resetProcessors(std::shared_ptr<DrmFramebuffer> & fb);
    int resetAllProcessors();
    bool runProcessors(std::shared_ptr<DrmFramebuffer> & fb,
            std::shared_ptr<HwDisplayPlane> & plane,
            uint32_t presentZorder,
            int blankFlag);
    bool runProcessors(std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
            std::shared_ptr<VideoComposerDev> dev,
            uint32_t z);

protected:
    int32_t mVideoFbsNum;
    int32_t mSrCount;
    int32_t mPqCount;
    int32_t mColorCount;
    int32_t mDiCount;

    std::vector<std::shared_ptr<FbProcessor>> mSrProcessors;
    std::vector<std::shared_ptr<FbProcessor>> mPqProcessors;
    std::vector<std::shared_ptr<FbProcessor>> mColorProcessors;
    std::vector<std::shared_ptr<FbProcessor>> mDiProcessors;
    std::vector<std::shared_ptr<FbProcessor>> mProcessors;

    std::map<hwc2_layer_t, std::vector<std::shared_ptr<FbProcessor>>> mFbProcessorsPairs;
    std::map<hwc2_layer_t, bool> mResetFlagPairs;
};

#endif
