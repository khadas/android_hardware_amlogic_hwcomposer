/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_NDEBUG 1

#include <utils/Trace.h>
#include <MesonLog.h>
#include <DebugHelper.h>
#include <HwcConfig.h>

#include "VideoProcessorsManager.h"

VideoProcessorsManager::VideoProcessorsManager() {
    mSrProcessors.clear();
    mPqProcessors.clear();
    mColorProcessors.clear();
    mDiProcessors.clear();
    mFbProcessorsPairs.clear();
    mResetFlagPairs.clear();
    mVideoFbsNum = -1;
}

VideoProcessorsManager::~VideoProcessorsManager() {
    tearDownAllProcessors();
}

void VideoProcessorsManager::destroyUnusedProcessor(
        std::vector<std::shared_ptr<FbProcessor>> & processors) {
    bool bRemove = true;

    if (processors.empty())
        return;

    for (auto prIt = processors.begin(); prIt != processors.end(); ) {
        bRemove = true;

        for (auto fbIt = mVideoFbs.begin(); fbIt != mVideoFbs.end(); fbIt++) {
            if ((*fbIt)->getUniqueId() == (*prIt)->getUseLayerId()) {
                bRemove = false;
                break;
            }
        }

        if (bRemove) {
            MESON_LOGV("%s, teardown processor(%d) for layerId %" PRIu64,
                    __FUNCTION__, (*prIt)->getFbProcessorType(),
                    (*prIt)->getUseLayerId());
            (*prIt)->teardown();
            prIt = processors.erase(prIt);
        } else {
            prIt++;
        }
    }
}

// need to be called before setup processor
void VideoProcessorsManager::prepare(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs) {
    mVideoFbs.clear();
    mVideoFbs = fbs;
    mVideoFbsNum = fbs.size();

    mFbProcessorsPairs.clear();

    // remove processor for destroy layer
    destroyUnusedProcessor(mDiProcessors);
    destroyUnusedProcessor(mSrProcessors);
    destroyUnusedProcessor(mPqProcessors);
    destroyUnusedProcessor(mColorProcessors);

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
    std::vector<hwc2_layer_t> layerIds;
    hwc2_layer_t id;
    int i, num, needMaxNum, supportChannelNum;
    bool bFlag = false;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aisr.
         * donot enable aisr processor when there's multiple video channels
         * */
        tearDownSrProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiSrProcessorEnabled()) {
        for (auto fbIt = mVideoFbs.begin(); fbIt != mVideoFbs.end(); fbIt++) {
            id = (*fbIt)->getUniqueId();
            bFlag = false;

            auto prIt = mSrProcessors.begin();
            for (; prIt != mSrProcessors.end(); prIt++) {
                if (id == (*prIt)->getUseLayerId()) {
                    bFlag = true;
                    break;
                }
            }

            if (!bFlag) layerIds.push_back(id);
        }

        // setup AiSrprocessor
        supportChannelNum = HwcConfig::getSupportAiSrChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AISR processor",
                __FUNCTION__, needMaxNum);
        if (mSrProcessors.size() < needMaxNum) {
            num = needMaxNum - mSrProcessors.size();
            if (num > layerIds.size()) {
                MESON_LOGE("%s, setup AISR processor failed", __FUNCTION__);
                return 0;
            }

            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: create AISR processor for layerID:%" PRIu64,
                        __FUNCTION__, (*layerIds.begin()));
                createFbProcessor(FB_AISR_PROCESSOR, processor);
                processor->setup();
                processor->setUseLayerId(*layerIds.begin());
                layerIds.erase(layerIds.begin());
                mSrProcessors.push_back(processor);
            }
        } else if (mSrProcessors.size() > needMaxNum) {
            MESON_LOGW("%s: AISR, that should be impossible", __FUNCTION__);
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAiPqProcessor() {
    std::shared_ptr<FbProcessor> processor;
    std::vector<hwc2_layer_t> layerIds;
    hwc2_layer_t id;
    int i, num, needMaxNum, supportChannelNum;
    bool bFlag = false;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aipq.
         * donot enable aipq processor when there's multiple video channels
         * */
        tearDownPqProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiPqProcessorEnabled()) {
        for (auto fbIt = mVideoFbs.begin(); fbIt != mVideoFbs.end(); fbIt++) {
            id = (*fbIt)->getUniqueId();
            bFlag = false;

            auto prIt = mPqProcessors.begin();
            for (; prIt != mPqProcessors.end(); prIt++) {
                if (id == (*prIt)->getUseLayerId()) {
                    bFlag = true;
                    break;
                }
            }

            if (!bFlag) layerIds.push_back(id);
        }

        // setup AiPqprocessor
        supportChannelNum = HwcConfig::getSupportAiPqChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AIPQ processor",
                __FUNCTION__, needMaxNum);
        if (mPqProcessors.size() < needMaxNum) {
            num = needMaxNum - mPqProcessors.size();
            if (num > layerIds.size()) {
                MESON_LOGW("%s, setup AIPQ processor failed", __FUNCTION__);
                return 0;
            }

            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: create AIPQ processor for layerID:%" PRIu64,
                        __FUNCTION__, (*layerIds.begin()));
                createFbProcessor(FB_AIPQ_PROCESSOR, processor);
                processor->setup();
                processor->setUseLayerId(*layerIds.begin());
                layerIds.erase(layerIds.begin());
                mPqProcessors.push_back(processor);
            }
        } else if (mPqProcessors.size() > needMaxNum) {
            MESON_LOGW("%s: AIPQ, that should be impossible", __FUNCTION__);
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAiColorProcessor() {
    std::shared_ptr<FbProcessor> processor;
    std::vector<hwc2_layer_t> layerIds;
    hwc2_layer_t id;
    int i, num, needMaxNum, supportChannelNum;
    bool bFlag = false;

    if (mVideoFbsNum != 1) {
        /* TODO: currently only support one channel video for Aisr.
         * donot enable aisr processor when there's multiple video channels
         * */
        tearDownColorProcessors();
        return 0;
    }

    if (!DebugHelper::getInstance().disableAISRAIPQ() &&
        HwcConfig::AiColorProcessorEnabled()) {
        for (auto fbIt = mVideoFbs.begin(); fbIt != mVideoFbs.end(); fbIt++) {
            id = (*fbIt)->getUniqueId();
            bFlag = false;

            auto prIt = mColorProcessors.begin();
            for (; prIt != mColorProcessors.end(); prIt++) {
                if (id == (*prIt)->getUseLayerId()) {
                    bFlag = true;
                    break;
                }
            }

            if (!bFlag) layerIds.push_back(id);
        }

        // setup AiSrprocessor
        supportChannelNum = HwcConfig::getSupportAiColorChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        MESON_LOGV("%s: create %d channel AICOLOR processor",
                __FUNCTION__, needMaxNum);
        if (mColorProcessors.size() < needMaxNum) {
            num = needMaxNum - mColorProcessors.size();
            if (num > layerIds.size()) {
                MESON_LOGE("%s, setup AICOLOR processor failed", __FUNCTION__);
                return 0;
            }

            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: create AICOLOR processor for layerID:%" PRIu64,
                        __FUNCTION__, (*layerIds.begin()));
                createFbProcessor(FB_AICOLOR_PROCESSOR, processor);
                processor->setup();
                processor->setUseLayerId(*layerIds.begin());
                layerIds.erase(layerIds.begin());
                mColorProcessors.push_back(processor);
            }
        } else if (mColorProcessors.size() > needMaxNum) {
            MESON_LOGW("%s: AICOLOR, that should be impossible", __FUNCTION__);
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpDiProcessor() {
    std::shared_ptr<FbProcessor> processor;
    std::vector<hwc2_layer_t> layerIds;
    hwc2_layer_t id;
    int i, num, needMaxNum, supportChannelNum;
    bool bFlag = false;

    if (!DebugHelper::getInstance().disableDi() &&
        HwcConfig::DiProcessorEnabled()) {
        for (auto fbIt = mVideoFbs.begin(); fbIt != mVideoFbs.end(); fbIt++) {
            id = (*fbIt)->getUniqueId();
            bFlag = false;

            auto prIt = mDiProcessors.begin();
            for (; prIt != mDiProcessors.end(); prIt++) {
                if (id == (*prIt)->getUseLayerId()) {
                    bFlag = true;
                    break;
                }
            }

            if (!bFlag) layerIds.push_back(id);
        }

        supportChannelNum = HwcConfig::getSupportDiChannelNumber();
        needMaxNum =
            mVideoFbsNum > supportChannelNum ? supportChannelNum : mVideoFbsNum;

        if (mDiProcessors.size() < needMaxNum) {
            num = needMaxNum - mDiProcessors.size();
            if (num > layerIds.size()) {
                MESON_LOGE("%s, setup DI processor failed", __FUNCTION__);
                return 0;
            }

            for (i = 0; i < num; i++) {
                MESON_LOGV("%s: create DI processor for layerID:%" PRIu64,
                        __FUNCTION__, (*layerIds.begin()));
                createFbProcessor(FB_DI_PROCESSOR, processor);
                processor->setup();
                processor->setUseLayerId(*layerIds.begin());
                layerIds.erase(layerIds.begin());
                mDiProcessors.push_back(processor);
            }
        } else if (mDiProcessors.size() > needMaxNum) {
            MESON_LOGW("%s: AISR, that should be impossible", __FUNCTION__);
        }
    }

    return 0;
}

int VideoProcessorsManager::setUpAllProcessors() {
    setUpAiSrProcessor();
    setUpAiPqProcessor();
    setUpAiColorProcessor();
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
    ATRACE_INT64("resetProcessors", fb->getUniqueId());
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

    MESON_LOGV("%s, reset processors for layerId:%" PRIu64,
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
    hwc2_layer_t layerId = fb->getUniqueId();

    std::vector<std::shared_ptr<FbProcessor>> vProcessors;

    std::vector<std::vector<std::shared_ptr<FbProcessor>>> allProcessors = {
        mSrProcessors, mPqProcessors, mColorProcessors, mDiProcessors,
    };

    // remove processor for destroy layer
    auto prIts = allProcessors.begin();
    for (; prIts != allProcessors.end(); prIts++) {
        std::vector<std::shared_ptr<FbProcessor>> prs = (*prIts);
        for (auto prIt = prs.begin(); prIt != prs.end(); prIt++) {
            if (layerId == (*prIt)->getUseLayerId()) {
                MESON_LOGV("%s: add processor(%d) for fbId: %" PRIu64,
                        __FUNCTION__, (*prIt)->getFbProcessorType(), layerId);
                vProcessors.push_back(*prIt);
                break;
            }
        }
    }

    mFbProcessorsPairs.emplace(layerId, vProcessors);
}

void VideoProcessorsManager::setup(
        std::vector<std::shared_ptr<DrmFramebuffer>> & fbs) {
    ATRACE_CALL();
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
    ATRACE_CALL();
    bool hasProcessor = false;
    bool hasDiProcessor = false;
    int processFence = -1;
    int releaseFence = -1;
    /*diFence removed*/
    // int diFence = -1;
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

        /*Logically dead code,the condition diFence >= 0 cannot be true
         *if (hasDiProcessor && diFence >= 0) {
         * need reset release fence after setPlane()
            fb->setCurReleaseFence(diFence);

            if (releaseFence >= 0)
                close(releaseFence);

            } else
        */
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
    ATRACE_CALL();
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
