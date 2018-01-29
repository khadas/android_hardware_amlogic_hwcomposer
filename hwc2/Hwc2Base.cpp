/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "Hwc2Base.h"

hwc2_composition_t translateCompositionType(
    meson_compositon_t type) {
    hwc2_composition_t hwcCompostion;
    switch (type) {
        case MESON_COMPOSITION_DUMMY:
        case MESON_COMPOSITION_PLANE_VIDEO:
        case MESON_COMPOSITION_PLANE_OSD:
        case MESON_COMPOSITION_GE2D:
                hwcCompostion = HWC2_COMPOSITION_DEVICE;
                break;
        case MESON_COMPOSITION_PLANE_OSD_COLOR:
                hwcCompostion = HWC2_COMPOSITION_SOLID_COLOR;
                break;
        case MESON_COMPOSITION_PLANE_CURSOR:
                hwcCompostion = HWC2_COMPOSITION_CURSOR;
                break;
         case MESON_COMPOSITION_PLANE_VIDEO_SIDEBAND:
                hwcCompostion = HWC2_COMPOSITION_SIDEBAND;
                break;
          case MESON_COMPOSITION_CLIENT:
                hwcCompostion = HWC2_COMPOSITION_CLIENT;
                break;
          default:
                hwcCompostion = HWC2_COMPOSITION_INVALID;
                break;
    }

    return hwcCompostion;
}

