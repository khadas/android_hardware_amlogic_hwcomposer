/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DummyComposer.h"

DummyComposer::DummyComposer() {
}

DummyComposer::~DummyComposer() {
}

bool DummyComposer::isFbsSupport(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused,
    int composeIdx __unused) {
    return true;
}

int32_t DummyComposer::prepare() {
    return 0;
}

int32_t DummyComposer::addInputs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused,
    int composeIdx __unused) {
    return 0;
}

int32_t DummyComposer::getOverlyFbs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlays __unused) {
    return 0;
}

int32_t DummyComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> & fb __unused,
    hwc_region_t damage __unused,
    int composeIdx __unused) {
    return 0;
}

int32_t DummyComposer::start(int composeIdx __unused) {
    return 0;
}

std::shared_ptr<DrmFramebuffer> DummyComposer::getOutput(
    int composeIdx __unused) {
    return NULL;
}

