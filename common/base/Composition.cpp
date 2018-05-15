/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <Composition.h>

bool isOverlayComposition(meson_compositon_t type) {
        if (type == MESON_COMPOSITION_PLANE_AMVIDEO
            || type == MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND)
            return true;
        else
            return false;
}

bool isComposerComposition(meson_compositon_t type) {
    if (type == MESON_COMPOSITION_CLIENT
        || type == MESON_COMPOSITION_GE2D
        || type == MESON_COMPOSITION_DUMMY)
        return true;
    else
        return false;
}

const char* compositionTypeToString(
    meson_compositon_t compType) {
    const char * compStr = "NONE";
    switch (compType) {
        case MESON_COMPOSITION_UNDETERMINED:
            compStr = "NONE";
            break;
        case MESON_COMPOSITION_DUMMY:
            compStr = "DUMMY";
            break;
        case MESON_COMPOSITION_CLIENT:
            compStr = "CLIENT";
            break;
        case MESON_COMPOSITION_GE2D:
            compStr = "GE2D";
            break;
        case MESON_COMPOSITION_PLANE_AMVIDEO:
            compStr = "AMVIDEO";
            break;
        case MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND:
            compStr = "SIDEBAND";
            break;
        case MESON_COMPOSITION_PLANE_OSD:
            compStr = "OSD";
            break;
        case MESON_COMPOSITION_PLANE_CURSOR:
            compStr = "CURSOR";
            break;
        case MESON_COMPOSITION_PLANE_HWCVIDEO:
            compStr = "HWCVIDEO";
            break;
        default:
            compStr = "UNKNOWN";
            break;
    }

    return compStr;
}


