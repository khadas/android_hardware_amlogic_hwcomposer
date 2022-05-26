/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <hardware/hwcomposer2.h>

/**
 * setBootDisplayConfig(..., config)
 * Descriptor: HWC3_FUNCTION_SET_BOOT_DISPLAY_CONFIG
 *
 * Sets the display config in which the device boots.
 *
 * If the device is unable to boot in this config for any reason (example HDMI display changed),
 * the implementation should try to find a mode which matches the resolution and refresh-rate of
 * this mode. If no such config exists, the implementation's preferred config should be used. The
 * boot config should be persisted across such events.
 *
 * @param config is the new boot config for the display.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 * @exception EX_BAD_CONFIG when an invalid config id was passed in.
 *
 *
 * @see clearBootDisplayConfig
 * @see getPreferredDisplayConfig
 */
typedef int32_t (*HWC3_PFN_SET_BOOT_DISPLAY_CONFIG)(hwc2_device_t* device,
        hwc2_display_t display, int32_t config);

/**
 * clearBootDisplayConfig(...)
 * Descriptor: HWC3_FUNCTION_CLEAR_BOOT_DISPLAY_CONFIG
 *
 * Clears the boot display config.
 *
 * The device should boot in implementation's preferred display config.
 *
 * @param display is the display for which the persisted boot display config is cleared.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 *
 * See also:
 * @see setBootDisplayConfig
 * @see getSystemPreferredDisplayConfig
 */
typedef int32_t (*HWC3_PFN_CLEAR_BOOT_DISPLAY_CONFIG)(hwc2_device_t* device,
        hwc2_display_t display);

/**
 * getPreferredBootDisplayConfig(...)
 * Descriptor: HWC3_FUNCTION_GET_PREFERRED_BOOT_DISPLAY_CONFIG
 *
 * Returns the implementation's preferred display config.
 *
 * This is the display config that should be used at boot, if no boot config has been requested.
 *
 * @param display is the display of which has the preferred config.
 * @return the implementation's preferred display config.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 *
 * @see setBootDisplayConfig
 * @see clearBootDisplayConfig
 */
typedef int32_t (*HWC3_PFN_GET_PREFERRED_BOOT_DISPLAY_CONFIG)(hwc2_device_t* device,
        hwc2_display_t display, int32_t* config);

/*
 * getDisplayPhysicalOrientation(..., outOrientation)
 * Descriptor: HWC3_FUNCTION_GET_DISPLAY_PHYSICAL_ORIENTATION
 *
 * Returns the implementation's display orientation.
 *
 * @param display is the display of which has the preferred config.
 * @return the implementation's display orientation.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 *
 */
typedef int32_t (*HWC3_PFN_GET_DISPLAY_PHYSICAL_ORIENTATION)(hwc2_device_t* device,
        hwc2_display_t display, int32_t* config);

/*
 * setExpectedPresentTime(..., time)
 * Descriptor: HWC3_FUNCTION_SET_EXPECTED_PRESENT_TIME
 *
 * @param display is the display of which has the preferred config.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 *
 */
typedef int32_t (*HWC3_PFN_SET_EXPECTED_PRESENT_TIME)(hwc2_device_t* device,
        hwc2_display_t display, int64_t expectedPresentTime);

/*
 * setLayerBrightness(..., brightness)
 * Descriptor: HWC3_FUNCTION_SET_LAYER_BRIGHTNESS
 *
 * @param display is the display of which has the preferred config.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 */
typedef int32_t (*HWC3_PFN_SET_LAYER_BRIGHTNESS)(hwc2_device_t* device,
        hwc2_display_t display, hwc2_layer_t layer, float brightness);


/*
 * setAidlOn(..., flag)
 * Descriptor: HWC3_FUNCTION_SET_AIDL_CLIENT_PID
 *
 * Extern interface to tell hwc whether we are on Aidl service or not
 * @param display is the display of which has the preferred config.
 *
 * @exception EX_BAD_DISPLAY when an invalid display handle was passed in.
 *
 */
typedef int32_t (*HWC3_PFN_SET_AIDL_CLIENT_PID)(hwc2_device_t* device, int32_t pid);

typedef enum {
    HWC3_FUNCTION_INVALID =  HWC2_FUNCTION_GET_LAYER_GENERIC_METADATA_KEY + 8,
    HWC3_FUNCTION_SET_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_CLEAR_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_GET_PREFERRED_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_GET_DISPLAY_PHYSICAL_ORIENTATION,
    HWC3_FUNCTION_SET_EXPECTED_PRESENT_TIME,
    HWC3_FUNCTION_SET_LAYER_BRIGHTNESS,
    HWC3_FUNCTION_SET_AIDL_CLIENT_PID,
} hwc3_function_descriptor_t;

typedef enum {
    HWC3_POWER_MODE_SUSPEND = 4,
} hwc3_power_mode_t;

/*
 * extern of composition type
 */
typedef enum {
    HWC3_COMPOSITION_DECORATION = 6,
} hwc3_composition_t;

typedef enum {
    HWC3_TRANSFORM_NONE = 0,
    HWC3_TRANSFORM_ROT_90 = 4,
    HWC3_TRANSFORM_ROT_180 = 3,
    HWC3_TRANSFORM_ROT_270 = 7,
} hwc3_transform_t;
