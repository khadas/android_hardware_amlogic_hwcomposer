/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "DrmEncoder.h"

DrmEncoder::DrmEncoder(int drmFd, drmModeEncoderPtr p) {
    mDrmFd = drmFd;
    mId = p->encoder_id;
    mCrtcId = p->crtc_id;
    mCrtcMask = p->possible_crtcs;
}

DrmEncoder::~DrmEncoder() {
}

uint32_t DrmEncoder::getId() {
    return mId;
}

uint32_t DrmEncoder::getCrtcId() {
    return mCrtcId;
}

int32_t DrmEncoder::setCrtcId(uint32_t crtc) {
    mCrtcId = crtc;
    return 0;
}

uint32_t DrmEncoder::getPossibleCrtcs() {
    return mCrtcMask;
}

