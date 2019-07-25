/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DiComposer.h"

DiComposer::DiComposer() {
}

DiComposer::~DiComposer() {
}

bool DiComposer::isFbsSupport(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused) {
    return true;
}

int32_t DiComposer::prepare() {
    return 0;
}

int32_t DiComposer::addInput(
    std::shared_ptr<DrmFramebuffer> & fb __unused,
    bool bOverlay __unused) {
    return 0;
}

int32_t DiComposer::addInputs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & fbs __unused,
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlayfbs __unused) {
    return 0;
}

int32_t DiComposer::getOverlyFbs(
    std::vector<std::shared_ptr<DrmFramebuffer>> & overlays __unused) {
    return 0;
}

int32_t DiComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> & fb __unused,
    hwc_region_t damage __unused) {
    return 0;
}

int32_t DiComposer::start() {
    return 0;
}

std::shared_ptr<DrmFramebuffer> DiComposer::getOutput() {
    return NULL;
}


