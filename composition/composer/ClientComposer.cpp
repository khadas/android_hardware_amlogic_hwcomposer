/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "ClientComposer.h"
#include <DrmTypes.h>
#include <MesonLog.h>

ClientComposer::ClientComposer() {
}

ClientComposer::~ClientComposer() {
}

bool ClientComposer::isFbsSupport(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused,
    int composeIdx __unused) {
    return true;
}

int32_t ClientComposer::prepare() {
    mOverlayFbs.clear();
    return 0;
}

int32_t ClientComposer::addInputs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs,
    int composeIdx __unused) {
    mOverlayFbs = overlayfbs;
    return 0;
}

int32_t ClientComposer::getOverlyFbs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlays) {
    overlays = mOverlayFbs;
    return 0;
}

int32_t ClientComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> & fb,
    hwc_region_t damage __unused,
    int composeIdx __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    mClientTarget.reset();
    if (fb.get() != nullptr)
        mClientTarget = fb;
    return 0;
}

int32_t ClientComposer::start(int composeIdx __unused) {
    return 0;
}

std::shared_ptr<DrmFramebuffer> ClientComposer::getOutput(
    int composeIdx __unused) {
    std::lock_guard<std::mutex> lock(mMutex);
    return mClientTarget;
}

