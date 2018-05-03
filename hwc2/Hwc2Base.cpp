/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "Hwc2Base.h"

hwc2_composition_t mesonComp2Hwc2Comp(
    meson_compositon_t type) {
    hwc2_composition_t hwcCompostion;
    switch (type) {
        case MESON_COMPOSITION_CLIENT:
                hwcCompostion = HWC2_COMPOSITION_CLIENT;
                break;
        case MESON_COMPOSITION_PLANE_CURSOR:
                hwcCompostion = HWC2_COMPOSITION_CURSOR;
                break;
        case MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND:
                hwcCompostion = HWC2_COMPOSITION_SIDEBAND;
                break;
        case MESON_COMPOSITION_DUMMY:
        case MESON_COMPOSITION_PLANE_AMVIDEO:
        case MESON_COMPOSITION_PLANE_HWCVIDEO:
        case MESON_COMPOSITION_PLANE_OSD:
        case MESON_COMPOSITION_GE2D:
        default:
                hwcCompostion = HWC2_COMPOSITION_DEVICE;
                break;
    }

    return hwcCompostion;
}

