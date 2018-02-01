/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "ClientComposer.h"
#include <MesonLog.h>

ClientComposer::ClientComposer() {
}

ClientComposer::~ClientComposer() {
}

bool ClientComposer::isCompositionSupport(
    meson_compositon_t type) {
    if (MESON_COMPOSITION_CLIENT == type)
        return true;
    else
        return false;
}

bool ClientComposer::isFbSupport(
    std::shared_ptr<DrmFramebuffer> &fb) {
    return true;
}

int32_t ClientComposer::prepare() {
    return 0;
}

meson_compositon_t ClientComposer::getCompostionType(
    std::shared_ptr<DrmFramebuffer> &fb) {
    return MESON_COMPOSITION_CLIENT;
}

int32_t ClientComposer::addInput(
    std::shared_ptr<DrmFramebuffer> &fb) {
    return 0;
}

int32_t ClientComposer::setOutput(
    std::shared_ptr<DrmFramebuffer> &fb,
    hwc_region_t damage) {

    mClientTarget = fb;
    return 0;
}

int32_t ClientComposer::start() {
    return 0;
}

std::shared_ptr<DrmFramebuffer> ClientComposer::getOutput() {
    return mClientTarget;
}

