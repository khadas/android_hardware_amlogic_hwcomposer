/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <hardware/hwcomposer2.h>
#include <HwcPowerMode.h>
#include <MesonLog.h>

HwcPowerMode::HwcPowerMode() {
    mConnectorMode = MESON_POWER_BOOT;
    mPowerMode = HWC2_POWER_MODE_ON;

    mScreenBlank = true;
    mConnectorPresent = false;
}

HwcPowerMode::~HwcPowerMode() {
}

int32_t HwcPowerMode::setScreenStatus(bool bBlank) {
    mScreenBlank = bBlank;
    if (mConnectorMode == MESON_POWER_BOOT && !mScreenBlank) {
        mConnectorMode = mConnectorPresent ?
            MESON_POWER_CONNECTOR_IN : MESON_POWER_CONNECTOR_OUT;
    }

    return 0;
}

bool HwcPowerMode::getScreenStatus() {
    return mScreenBlank;
}

int32_t HwcPowerMode::setConnectorStatus(bool bConnected) {
    if (mConnectorMode != MESON_POWER_BOOT && mConnectorPresent != bConnected) {
        mConnectorMode = bConnected ?
            MESON_POWER_CONNECTOR_IN : MESON_POWER_CONNECTOR_OUT;
        MESON_LOGD("[%s]: power mode %d", __func__, mConnectorMode);
    }
    mConnectorPresent = bConnected;
    return 0;
}

int32_t HwcPowerMode::setPowerMode(int32_t mode) {
    int32_t ret = HWC2_ERROR_BAD_PARAMETER;
    mPowerMode = mode;

    switch (mode) {
        case HWC2_POWER_MODE_ON:
        case HWC2_POWER_MODE_OFF:
            ret = HWC2_ERROR_NONE;
            break;
        case HWC2_POWER_MODE_DOZE:
        case HWC2_POWER_MODE_DOZE_SUSPEND:
            ret = HWC2_ERROR_UNSUPPORTED;
            break;
        default:
            ret = HWC2_ERROR_BAD_PARAMETER;
    }

    return ret;
}

bool HwcPowerMode::needBlankScreen(bool bLayerPresent) {
    bool powerOn = (mPowerMode == HWC2_POWER_MODE_ON);

    switch (mConnectorMode) {
        case MESON_POWER_BOOT:
            return !(mConnectorPresent && bLayerPresent);
        case MESON_POWER_CONNECTOR_IN:
            return false || !powerOn;
        case MESON_POWER_CONNECTOR_OUT:
            return true;
        default:
            MESON_LOGE("Unknown power mode (%d).", mConnectorMode);
    };

    return false;
}

meson_power_mode_t HwcPowerMode::getConnectorMode() {
    return mConnectorMode;
}

