/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1

#include <MesonLog.h>
#include <DebugHelper.h>
#include <HwcConfig.h>

#include "VideoProcessorsManager.h"

VideoProcessorsManager::VideoProcessorsManager() {
    mSrCount = 0;
    mPqCount = 0;
    mColorCount = 0;
    mDiCount = 0;
    mSrProcessors.clear();
    mPqProcessors.clear();
    mColorProcessors.clear();
    mDiProcessors.clear();
    mFbProcessorsPairs.clear();
    mResetFlagPairs.clear();
}

VideoProcessorsManager::~VideoProcessorsManager() {
    tearDownAllProcessors();
}

// need to be called before setup processor
void VideoProcessorsManager::prepare(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs) {
    mVideoFbsNum = fbs.size();
    mSrCount = 0;
    mPqCount = 0;
    mColorCount = 0;
    mDiCount = 0;

    mFbProcessorsPairs.clear();

    for (auto search = mResetFlagPairs.begin();
            search != mResetFlagPairs.end(); ) {
        bool find = false;

       for (auto it = fbs.begin(); it != fbs.end(); it++) {
            if (search->first == (*it)->getUniqueId()) {
                find = true;
                break;
            }
       }

       if (!find)
           search = mResetFlagPairs.erase(search);
       else
           search++;
    }

    for (auto it = fbs.begin(); it != fbs.end(); it++) {
        auto search = mResetFlagPairs.find((*it)->getUniqueId());
        if (search == mResetFlagPairs.end())
            mResetFlagPairs.emplace((*it)->getUniqueId(), true);
    }
}

int VideoProcessorsManager::setUpAiSrProcessor() {
    std::shared_ptr<FbProcessor> processor;
    int i, num, needMaxNum, supportChannelNum;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aisr.
         * donot enable aisr processor when there's multiple video channels
         * */
        tearDownSrProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiSrProcessorEnabled()) {
        // setup AiSrprocessor
        supportChannelNum = HwcConfig::getSupportAiSrChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AISR processor", __FUNCTION__, needMaxNum);
        if (mSrProcessors.size() < needMaxNum) {
            num = needMaxNum - mSrProcessors.size();
            for (i = 0; i < num; i++) {
                createFbProcessor(FB_AISR_PROCESSOR, processor);
                processor->setup();
                mSrProcessors.push_back(processor);
            }
        } else if (mSrProcessors.size() > needMaxNum) {
            num = mSrProcessors.size() - needMaxNum;
            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: teardown one AISR processor", __FUNCTION__);
                (*mSrProcessors.begin())->teardown();
                mSrProcessors.erase(mSrProcessors.begin());
            }
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAiPqProcessor() {
    std::shared_ptr<FbProcessor> processor;
    int i, num, needMaxNum, supportChannelNum;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aipq.
         * donot enable aipq processor when there's multiple video channels
         * */
        tearDownPqProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiPqProcessorEnabled()) {
        // setup AiPqprocessor
        supportChannelNum = HwcConfig::getSupportAiPqChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AIPQ processor", __FUNCTION__, needMaxNum);
        if (mPqProcessors.size() < needMaxNum) {
            num = needMaxNum - mPqProcessors.size();
            for (i = 0; i < num; i++) {
                createFbProcessor(FB_AIPQ_PROCESSOR, processor);
                processor->setup();
                mPqProcessors.push_back(processor);
            }
        } else if (mPqProcessors.size() > needMaxNum) {
            num = mPqProcessors.size() - needMaxNum;
            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: teardown one AIPQ processor", __FUNCTION__);
                (*mPqProcessors.begin())->teardown();
                mPqProcessors.erase(mPqProcessors.begin());
            }
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAiColorProcessor() {
    std::shared_ptr<FbProcessor> processor;
    int i, num, needMaxNum, supportChannelNum;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aisr.
         * donot enable aisr processor when there's multiple video channels
         * */
        tearDownColorProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiColorProcessorEnabled()) {
        // setup AiSrprocessor
        supportChannelNum = HwcConfig::getSupportAiColorChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AICOLOR processor", __FUNCTION__, needMaxNum);
        if (mColorProcessors.size() < needMaxNum) {
            num = needMaxNum - mSrProcessors.size();
            for (i = 0; i < num; i++) {
                createFbProcessor(FB_AICOLOR_PROCESSOR, processor);
                processor->setup();
                mColorProcessors.push_back(processor);
            }
        } else if (mColorProcessors.size() > needMaxNum) {
            num = mColorProcessors.size() - needMaxNum;
            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: teardown one AICOLOR processor", __FUNCTION__);
                (*mColorProcessors.begin())->teardown();
                mColorProcessors.erase(mColorProcessors.begin());
            }
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpDiProcessor() {
    std::shared_ptr<FbProcessor> processor;
    int i, num, needMaxNum, supportChannelNum;

    if (!DebugHelper::getInstance().disableDi() &&
        HwcConfig::DiProcessorEnabled()) {
        supportChannelNum = HwcConfig::getSupportDiChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel DI processor", __FUNCTION__, needMaxNum);
        if (mDiProcessors.size() < needMaxNum) {
            num = needMaxNum - mDiProcessors.size();
            for (i = 0; i < num; i++) {
                createFbProcessor(FB_DI_PROCESSOR, processor);
                processor->setup();
                mDiProcessors.push_back(processor);
                MESON_LOGD("%s: create one DI processor, DI processors count:%" PRIuFAST16,
                        __FUNCTION__, mDiProcessors.size());
            }
        } else if (mDiProcessors.size() > needMaxNum) {
            num = mDiProcessors.size() - needMaxNum;
            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: teardown one DI processor", __FUNCTION__);
                (*mDiProcessors.begin())->teardown();
                mDiProcessors.erase(mDiProcessors.begin());
            }
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAllProcessors() {
    setUpAiSrProcessor();
    setUpAiPqProcessor();
    setUpDiProcessor();

    return 0;
}

void VideoProcessorsManager::tearDownSrProcessors() {
    if (!mSrProcessors.empty()) {
        MESON_LOGV("%s: tear down all AISR processors", __FUNCTION__);
        auto it = mSrProcessors.begin();
        for (; it != mSrProcessors.begin(); it++)
            (*it)->teardown();

        mSrProcessors.clear();
    }
}

void VideoProcessorsManager::tearDownPqProcessors() {
    if (!mPqProcessors.empty()) {
        MESON_LOGV("%s: tear down all AIPQ processors", __FUNCTION__);
        auto it = mPqProcessors.begin();
        for (; it != mPqProcessors.end(); it++)
            (*it)->teardown();

        mPqProcessors.clear();
    }
}

void VideoProcessorsManager::tearDownColorProcessors() {
    if (!mColorProcessors.empty()) {
        MESON_LOGV("%s, tear down all AICOLOR processors", __FUNCTION__);
        auto it = mColorProcessors.begin();
        for (; it != mColorProcessors.end(); it++)
            (*it)->teardown();

        mColorProcessors.clear();
    }
}

void VideoProcessorsManager::tearDownDiProcessors() {
    if (!mDiProcessors.empty()) {
        MESON_LOGV("%s, tear down all DI processors", __FUNCTION__);
        auto it = mDiProcessors.begin();
        for (; it != mDiProcessors.end(); it++)
            (*it)->teardown();

        mDiProcessors.clear();
    }
}

void VideoProcessorsManager::tearDownAllProcessors() {
    tearDownSrProcessors();
    tearDownPqProcessors();
    tearDownColorProcessors();
    tearDownDiProcessors();
}

bool VideoProcessorsManager::resetProcessors (
        std::shared_ptr<DrmFramebuffer> & fb){
    //fb is not update
    if (!fb.get())
        return false;

    if (mFbProcessorsPairs.empty())
        return false;

    auto search = mFbProcessorsPairs.find(fb->getUniqueId());
    if (search == mFbProcessorsPairs.end())
        return false;

    auto resetIt = mResetFlagPairs.find(fb->getUniqueId());
    if (resetIt == mResetFlagPairs.end()) {
        mResetFlagPairs.emplace(fb->getUniqueId(), true);
        return true;
    } else {
        if (resetIt->second)
            return true;
    }

    MESON_LOGD("%s, reset processors for layerId:%" PRIu64,
            __FUNCTION__, fb->getUniqueId());
    for (auto it = search->second.begin(); it != search->second.end(); it++) {
        if ((*it).get()) {
            (*it)->teardown();
            if ((*it)->getFbProcessorType() == FB_DI_PROCESSOR) {
                fb->setDiProcessorFence(-1);
                fb->setDiProcessorFd(-1);
            }
            (*it)->setup();
        }
    }

    resetIt->second = true;

    return true;
}

int VideoProcessorsManager::resetAllProcessors() {
    MESON_LOGD("%s, reset all processors", __FUNCTION__);
    if (!mSrProcessors.empty()) {
        auto it = mSrProcessors.begin();
        for (; it != mSrProcessors.end(); it++) {
            (*it)->teardown();
            (*it)->setup();
        }
    }

    if (!mPqProcessors.empty()) {
        auto it = mPqProcessors.begin();
        for (; it != mPqProcessors.end(); it++) {
            (*it)->teardown();
            (*it)->setup();
        }
    }

    if (!mColorProcessors.empty()) {
        auto it = mColorProcessors.begin();
        for (; it != mColorProcessors.end(); it++) {
            (*it)->teardown();
            (*it)->setup();
        }
    }

    if (!mDiProcessors.empty()) {
        auto it = mDiProcessors.begin();
        for (; it != mDiProcessors.end(); it++) {
            (*it)->teardown();
            (*it)->setup();
        }
    }

    return 0;
}

void VideoProcessorsManager::setProcessors(
        std::shared_ptr<DrmFramebuffer>& fb) {

    std::vector<std::shared_ptr<FbProcessor>> processors;
    if (mDiCount < mDiProcessors.size()) {
        MESON_LOGV("%s: add DI processor(%d) for fbId: %" PRIu64,
                __FUNCTION__, mDiCount, fb->getUniqueId());
        /* DI Processor need top of the list */
        processors.push_back(mDiProcessors[mDiCount]);
        mDiCount++;
    }

    if (mSrCount < mSrProcessors.size()) {
        MESON_LOGV("%s: add AISR processor(%d) for fbId: %" PRIu64,
                __FUNCTION__, mSrCount, fb->getUniqueId());
        processors.push_back(mSrProcessors[mSrCount]);
        mSrCount++;
    }

    if (mPqCount < mPqProcessors.size()) {
        MESON_LOGV("%s: add AIPQ processor(%d) for fbId: %" PRIu64,
                __FUNCTION__, mPqCount, fb->getUniqueId());
        processors.push_back(mPqProcessors[mPqCount]);
        mPqCount++;
    }

    if (mColorCount < mColorProcessors.size()) {
        MESON_LOGV("%s: add AIPQ processor(%d) for fbId: %" PRIu64,
                __FUNCTION__, mColorCount, fb->getUniqueId());
        processors.push_back(mColorProcessors[mColorCount]);
        mColorCount++;
    }

    mFbProcessorsPairs.emplace(fb->getUniqueId(), processors);
}

void VideoProcessorsManager::setup(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs) {
    prepare(fbs);
    setUpAllProcessors();

    auto it = fbs.begin();
    for (; it != fbs.end(); it ++)
        setProcessors((*it));
}

bool VideoProcessorsManager::runProcessors(
        std::shared_ptr<DrmFramebuffer> & fb,
        std::shared_ptr<HwDisplayPlane> & plane,
        uint32_t presentZorder,
        int blankFlag) {
    bool hasProcessor = false;
    bool hasDiProcessor = false;
    int processFence = -1;
    int releaseFence = -1;
    int diFence = -1;
    std::shared_ptr<FbProcessor> processor;
    std::shared_ptr<DrmFramebuffer> inFb;
    std::shared_ptr<DrmFramebuffer> outFb;

    //fb is not update
    if (!fb.get() || !fb->isFbUpdated())
        return false;

    if (mFbProcessorsPairs.empty())
        return false;

    auto search = mFbProcessorsPairs.find(fb->getUniqueId());
    if (search == mFbProcessorsPairs.end())
        return false;

    auto resetIt = mResetFlagPairs.find(fb->getUniqueId());
    if (resetIt == mResetFlagPairs.end())
        mResetFlagPairs.emplace(fb->getUniqueId(), false);
    else
        resetIt->second = false;

    inFb = fb;
    for (auto it = search->second.begin(); it != search->second.end(); it++) {
        if ((*it).get()) {
            if (processFence >= 0) {
                close(processFence);
                processFence = -1;
            }

            (*it)->asyncProcess(inFb, outFb, processFence);
            MESON_LOGV("%s, processor:%d return fence:%d",
                    __func__, (*it)->getFbProcessorType(), processFence);

            outFb->setProcessFence((processFence >= 0) ? dup(processFence) : -1);
            inFb = outFb;
            hasProcessor = true;

            /* DI Processor will copy data from input fb to output fb.
             * so we can release input fb with processFence */
            if ((*it)->getFbProcessorType() == FB_DI_PROCESSOR) {
                hasDiProcessor = true;
                /* need pass this fence to VC */
                outFb->setDiProcessorFence((processFence >= 0) ? dup(processFence) : -1);
                /* At the moment, DI processFence cannot be used as release fence.
                 * will enable this code in the next version
                if (processFence >= 0) {
                    diFence = dup(processFence);
                }
                */
            }
        }
    }

    // has processor
    if (hasProcessor) {
        plane->setPlane(outFb, presentZorder, blankFlag);
        releaseFence = outFb->getCurReleaseFence();

        for (auto it = search->second.begin(); it != search->second.end(); it++) {
            if ((*it).get())
                (*it)->onBufferDisplayed(outFb, (releaseFence >= 0) ? dup(releaseFence) : -1);
        }

        if (hasDiProcessor && diFence >= 0) {
            /* need reset release fence after setPlane() */
            fb->setCurReleaseFence(diFence);

            if (releaseFence >= 0)
                close(releaseFence);

        } else
            fb->onLayerDisplayed(releaseFence, (processFence >= 0) ? dup(processFence) : -1);

        if (processFence >= 0)
            close(processFence);
    }

    return hasProcessor;
}

struct fb_pairs{
    std::shared_ptr<DrmFramebuffer> inFb;
    std::shared_ptr<DrmFramebuffer> outFb;
    int fenceFd;
    bool hasDiProcessor;
};

bool VideoProcessorsManager::runProcessors(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
        std::shared_ptr<VideoComposerDev> dev,
        uint32_t z) {
    bool hasProcessor = false;
    bool hasDiProcessor = false;
    int processFence = -1;
    int releaseFence = -1;
    std::shared_ptr<DrmFramebuffer> inFb;
    std::shared_ptr<DrmFramebuffer> outFb;
    std::vector<std::shared_ptr<DrmFramebuffer>> inFbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> outFbs;
    std::vector<struct fb_pairs> fbPairs;

    if (mFbProcessorsPairs.empty())
        return false;

    for (auto fbIt = fbs.begin(); fbIt != fbs.end(); fbIt++) {
        inFb = *fbIt;
        auto resetIt = mResetFlagPairs.find(inFb->getUniqueId());
        if (resetIt == mResetFlagPairs.end())
            mResetFlagPairs.emplace(inFb->getUniqueId(), false);
        else
            resetIt->second = false;

        hasProcessor = false;
        hasDiProcessor = false;
        outFb.reset();
        auto search = mFbProcessorsPairs.find(inFb->getUniqueId());
        if (search != mFbProcessorsPairs.end()) {
            auto processorIt = search->second.begin();
            for (; processorIt != search->second.end(); processorIt++) {
                if ((*processorIt).get()) {
                    if (processFence >= 0) {
                        close(processFence);
                        processFence = -1;
                    }

                    (*processorIt)->asyncProcess(inFb, outFb, processFence);
                    outFb->setProcessFence((processFence >= 0) ? dup(processFence) : -1);
                    inFb = outFb;
                    hasProcessor = true;

                    /* DI Processor will copy data from input fb to output fb.
                     * so we can release input fb with processFence.
                     * with the exception of bypass DI */
                    if ((*processorIt)->getFbProcessorType() == FB_DI_PROCESSOR) {
                        /* At the moment, DI processFence cannot be used as release fence.
                         * will enable this code in the next version
                        if (processFence >= 0) {
                            inFb->setCurReleaseFence(dup(processFence));
                            hasDiProcessor = true;
                        }
                        */

                        /* need pass this fence to VC */
                        outFb->setDiProcessorFence((processFence >= 0) ? dup(processFence) : -1);
                    }
                }
            }
        }

        if (hasProcessor)
            outFbs.push_back(outFb);
        else
            outFbs.push_back(*fbIt);

        fbPairs.push_back({*fbIt, outFb,
                (processFence >= 0) ? dup(processFence) : -1, hasDiProcessor});

        if (processFence >= 0) {
            close(processFence);
            processFence = -1;
        }
    }

    dev->enable(true);
    dev->setFrames(outFbs, releaseFence, z);

    auto fbPairIt = fbPairs.begin();
    for (; fbPairIt != fbPairs.end(); fbPairIt++) {
        auto search = mFbProcessorsPairs.find((*fbPairIt).inFb->getUniqueId());
        if (search != mFbProcessorsPairs.end()) {
            auto processorIt = search->second.begin();
            for (; processorIt != search->second.end(); processorIt++) {
                if ((*processorIt).get() && (*fbPairIt).outFb.get()) {
                    (*processorIt)->onBufferDisplayed((*fbPairIt).outFb,
                            (releaseFence >= 0) ? dup(releaseFence) : -1);
                }
            }
        }

        if (!(*fbPairIt).hasDiProcessor)
            (*fbPairIt).inFb->onLayerDisplayed(
                    (releaseFence >= 0) ? dup(releaseFence) : -1, (*fbPairIt).fenceFd);
    }

    if (releaseFence >= 0)
        close(releaseFence);

    return true;
}
