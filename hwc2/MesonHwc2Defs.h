/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

 #ifndef MESON_HWC2_DEFS_H
#define MESON_HWC2_DEFS_H
#include <graphics.h>

#define MESON_HWC_HANDLE_PRIMARY_HOTPLUG 1

#define MESON_VIRTUAL_DISPLAY_ID_START (3)
#define MESON_VIRTUAL_DISPLAY_MAX_COUNT (1)
#define MESON_VIRTUAL_DISPLAY_MAX_DIMENSION (1920)
#define MESON_PHYSCIAL_DISPLAY_MAX_DIMENSION (1920)

typedef struct hdr_capabilities {
    android_hdr_t * hdrTypes;
    int hdrTypesNum;
    int maxLuminance;
    int avgLuminance;
    int minLuminance;
} hdr_capabilities_t;

 #endif/*MESON_HWC2_DEFS_H*/
