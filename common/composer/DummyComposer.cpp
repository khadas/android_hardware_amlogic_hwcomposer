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

bool DummyComposer::isCompositionSupport(
    meson_compositon_t type) {
    if (MESON_COMPOSITION_DUMMY == type)
        return true;
    else
        return false;
}

bool DummyComposer::isFbSupport(
    std::shared_ptr<DrmFramebuffer> & fb) {
    return true;
}

int32_t DummyComposer::prepare() {
    return 0;
}

meson_compositon_t DummyComposer::getCompostionType(
    std::shared_ptr<DrmFramebuffer> & fb) {
    return MESON_COMPOSITION_DUMMY;
}

int32_t DummyComposer::addInput(
    std::shared_ptr<DrmFramebuffer> & fb) {
    return 0;
}

int32_t DummyComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> & fb,
    hwc_region_t damage) {
    return 0;
}

int32_t DummyComposer::start() {
    return 0;
}

std::shared_ptr<DrmFramebuffer> DummyComposer::getOutput() {
    return NULL;
}

