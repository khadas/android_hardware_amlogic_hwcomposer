/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <BasicTypes.h>
#include "DiComposer.h"
#include <MesonLog.h>

struct DiComposerPair {
    uint32_t zorder;
    uint32_t num_composefbs;
    std::vector<std::shared_ptr<DrmFramebuffer>> composefbs;
};

DiComposer::DiComposer(int displayId)
    : IComposer() {

    mDisplayID = displayId;
    std::map<int,std::shared_ptr<VideoComposerDev>> devs = getVideoComposerDevByDisplayId(mDisplayID);

    for (auto it = devs.begin(); it != devs.end(); it++) {
        std::shared_ptr<ComposerImpl> impl = std::make_shared<ComposerImpl>();
        impl->composeDev = it->second;
        mComposerImpl.push_back(std::move(impl));
    }

    mImplNum = mComposerImpl.size();
    MESON_ASSERT(mImplNum > 0, "DiComposer init fail.");
}

DiComposer::~DiComposer() {
}

bool DiComposer::isFbsSupport(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused,
    int composeIdx __unused) {
    return true;
}

int32_t DiComposer::prepare() {
    /*clear impl data*/
    for (auto it = mComposerImpl.begin(); it != mComposerImpl.end(); it ++) {
        std::shared_ptr<ComposerImpl> impl = *it;
        impl->inputFbs.clear();
        impl->overlayFbs.clear();
        impl->outputFb.reset();
    }

    return 0;
}

int32_t DiComposer::addInputs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs,
    int composeIdx) {
    MESON_ASSERT(composeIdx < mImplNum, "DiComposer composeidx %d err .", composeIdx);

    std::shared_ptr<ComposerImpl> impl = mComposerImpl[composeIdx];
    impl->inputFbs = fbs;
    impl->overlayFbs = overlayfbs;
    return 0;
}

int32_t DiComposer::getOverlyFbs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlays __unused) {
    return 0;
}

int32_t DiComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> & fb,
    hwc_region_t damage __unused,
    int composeIdx) {
    MESON_ASSERT(composeIdx < mImplNum, "DiComposer composeidx %d err .", composeIdx);

    /*use fake fb, and read zorder from fb.*/
    MESON_ASSERT(fb->mFbType == DRM_FB_DI_COMPOSE_OUTPUT,
            "Di Composer only use fake output.");
    std::shared_ptr<ComposerImpl> impl = mComposerImpl[composeIdx];
    impl->outputFb = fb;
    return 0;
}

int32_t DiComposer::start(int composeIdx) {
    MESON_ASSERT(composeIdx < mImplNum, "DiComposer composeidx %d err .", composeIdx);

    int fenceFd = -1;
    std::shared_ptr<ComposerImpl> impl = mComposerImpl[composeIdx];

    MESON_ASSERT(impl->outputFb.get(), "DiComposer (%d) no output set!", composeIdx);
    impl->composeDev->enable(true);
    impl->composeDev->setFrames(impl->inputFbs, fenceFd, impl->outputFb->mZorder);

    /*set release fence*/
    for (auto it = impl->inputFbs.begin(); it != impl->inputFbs.end(); it++) {
       (*it)->setCurReleaseFence((fenceFd >= 0) ? ::dup(fenceFd) : -1);
    }

    if (fenceFd >= 0)
        close(fenceFd);
    return 0;
}

std::shared_ptr<DrmFramebuffer> DiComposer::getOutput(int composeIdx) {
    MESON_ASSERT(composeIdx < mImplNum, "DiComposer composeidx %d err .", composeIdx);

    std::shared_ptr<ComposerImpl> impl = mComposerImpl[composeIdx];
    return impl->outputFb;
}

