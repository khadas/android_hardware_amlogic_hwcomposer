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

typedef enum {
    HWC3_FUNCTION_INVALID =  HWC2_FUNCTION_GET_LAYER_GENERIC_METADATA_KEY + 8,
    HWC3_FUNCTION_SET_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_CLEAR_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_GET_PREFERRED_BOOT_DISPLAY_CONFIG,
    HWC3_FUNCTION_GET_DISPLAY_PHYSICAL_ORIENTATION,
    HWC3_FUNCTION_SET_EXPECTED_PRESENT_TIME,
    HWC3_FUNCTION_SET_LAYER_BRIGHTNESS,
    HWC3_FUNCTION_SET_AIDL_CLIENT_PID,
    HWC3_FUNCTION_GET_HDR_CONVERSION_CAPABILITIES,
    HWC3_FUNCTION_SET_HDR_CONVERSION_STRATEGY,
    HWC3_FUNCTION_GET_OVERLAY_SUPPORT,
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

typedef enum {
    HWC3_CAPABILITY_BOOT_DISPLAY_CONFIG = 5,
    HWC3_HDR_OUTPUT_CONVERSION_CONFIG = 6,
} hwc3_capability_t;

typedef enum {
    DOLBY_VISION_4K30 = 5,
} hwc3_hdr_t;

typedef struct {
    uint32_t sourceType;
    uint32_t outputType;
    bool addsLatency;
} hwc3_hdr_conversion_capability;

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

/*3.2 interface*/
/* getHdrConversionCapabilities(..., HdrConversionCapabilities)
 * Descriptor: HWC3_PFN_GET_HDR_CONVERSION_CAPABILITIES
 *
 * @param
 *   HdrConversionCapabilities - the array of HDR conversion capability.
 *
 *   OutNumHdrConversionCapabilities - if HdrConversionCapabilities was nullptr, returns the number
 *      of supported content types; if HdrConversionCapabilities was not nullptr, returns the
 *      number of capabilities stored in HdrConversionCapabilities
 *
 * @exception EX_UNSUPPORTED when not supported by the underlying HAL
 * */
typedef int32_t (*HWC3_PFN_GET_HDR_CONVERSION_CAPABILITIES)(hwc2_device_t* device,
        uint32_t* OutNumHdrConversionCapabilities,
        hwc3_hdr_conversion_capability* HdrConversionCapabilities);

/* setHdrConversionStrategy(..., passthrough, autoAllowedHdrTypes)
 * Descriptor: HWC3_PFN_SET_HDR_CONVERSION_STRATEGY
 *
 * @param
 *   passthrough - when this parameter is set to true, HDR conversion is disabled by the
 *       implementation. The output HDR type will change dynamically to match the content.
 *
 *   numElements - the number of autoAllowedHdrTypes
 *
 *   autoAllowedHdrTypes - The case that passthrough is false, when this parameter is set, the output HDR
 *       type is selected by the implementation. The implementation is only allowed to set the output HDR
 *       type to the HDR types present in this list. If conversion to any of the autoHdrTypes types is
 *       not possible, the implementation should do no conversion.
 *
 * @exception EX_UNSUPPORTED when autoAllowedHdrTypes not supported by the underlying HAL
 * */
typedef int32_t (*HWC3_PFN_SET_HDR_CONVERSION_STRATEGY)(hwc2_device_t* device,
        bool passthrough, uint32_t numElements, bool isAuto, uint32_t* autoAllowedHdrTypes,
        uint32_t* preferredHdrOutputType);

/* getOverlaySupport(..., combinations)
 * Descriptor: HWC3_PFN_GET_OVERLAY_SUPPORT
 * @param
 *   combinations List of pixelformats, standards, transfers and ranges dataspaces that can be used
 *       together If some dataspaces, e.g. scRGB (STANDARD_BT709 | TRANSFER_SRGB | RANGE_EXTENDED),
 *       only work with specific formats, then this list may contain more than 1 entry.
 *       If some ranges, e.g. RANGE_LIMITED, only work with specific
 *       Formats/standards/transfers, then this list may contain more than 1 entry.
 *       Currently, we only report pixelFormats from drivers
 *
 * @exception EX_UNSUPPORTED when not supported by the underlying HAL
 * */
typedef int32_t (*HWC3_PFN_GET_OVERLAY_SUPPORT)(hwc2_device_t* device,
        uint32_t* numElements, uint32_t* pixelFormats);
