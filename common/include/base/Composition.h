/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef COMPOSITION_H
#define COMPOSITION_H

typedef enum {
    MESON_COMPOSITION_UNDETERMINED = 0,

    /*Compostion type of composer*/
    MESON_COMPOSITION_DUMMY = 1,
    MESON_COMPOSITION_CLIENT,
    MESON_COMPOSITION_GE2D,
    /*MESON_COMPOSITION_GPU,*/

    /*Compostion type of plane*/
    MESON_COMPOSITION_PLANE_AMVIDEO,
    MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND,
    MESON_COMPOSITION_PLANE_OSD,
    MESON_COMPOSITION_PLANE_CURSOR,

    /*New video plane.*/
    MESON_COMPOSITION_PLANE_HWCVIDEO,
} meson_compositon_t;

typedef enum meson_compose_to {
    /*default is no special feature*/
    MESON_COMPOSE_TO_ANY_PLANE = 0,
    /*special composition dest need to be special plane*/
    MESON_COMPOSE_TO_CONTINUOUS_PLANE = 1 << 0,
} meson_compose_to_t;

bool isOverlayComposition(meson_compositon_t type);
bool isComposerComposition(meson_compositon_t type);
const char* compositionTypeToString(meson_compositon_t compType);

#endif/*COMPOSITION_H*/
