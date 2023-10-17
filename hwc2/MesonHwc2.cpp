/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <unistd.h>
#include <thread>
#include <mutex>
#include <signal.h>

#include <condition_variable>

#include <utils/Trace.h>

#include <BasicTypes.h>
#include <MesonLog.h>
#include <DebugHelper.h>
#include <HwcConfig.h>
#include <HwcVsync.h>
#include <HwcDisplayPipe.h>
#include <misc.h>
#include <systemcontrol.h>

#include "MesonHwc2Defs.h"
#include "MesonHwc2.h"
#include "VirtualDisplay.h"
#include "hwcomposer3.h"

#define GET_REQUEST_FROM_PROP 1

#define CHECK_DISPLAY_VALID(display)    \
    if (isDisplayValid(display) == false) { \
        MESON_LOGE("(%s) met invalid display id (%d)", \
            __func__, (int)display); \
        return HWC2_ERROR_BAD_DISPLAY; \
    }

#define GET_HWC_DISPLAY(display)    \
    std::shared_ptr<Hwc2Display> hwcDisplay; \
    std::map<hwc2_display_t, std::shared_ptr<Hwc2Display>>::iterator it = \
            mDisplays.find(display); \
    if (it != mDisplays.end()) { \
        hwcDisplay = it->second; \
    }  else {\
        MESON_LOGE("(%s) met invalid display id (%d)",\
            __func__, (int)display); \
        return HWC2_ERROR_BAD_DISPLAY; \
    }

#define GET_HWC_LAYER(display, id)    \
    std::shared_ptr<Hwc2Layer> hwcLayer = display->getLayerById(id); \
    if (hwcLayer.get() == NULL) { \
        MESON_LOGE("(%s) met invalid layer id (%d) in display (%p)",\
            __func__, (int)id, display.get()); \
        return HWC2_ERROR_BAD_LAYER; \
    }


#ifdef GET_REQUEST_FROM_PROP
static bool m3DMode = false;
static bool mVdinPostMode = false;
static bool mKeyStoneMode = false;
#define DEFAULT_CROD(keystoneW, keystoneH)    \
    std::string w = std::to_string(keystoneW);\
    std::string h = std::to_string(keystoneH);\
    std::string defaultCord = "0.0,0.0,"+ w +".0,0.0,"+ w +".0," + h + ".0,0.0,"+ h +".0";\
    char keystoneProp[PROP_VALUE_LEN_MAX];\
    strcpy(keystoneProp, defaultCord.c_str());\

#endif

ANDROID_SINGLETON_STATIC_INSTANCE(MesonHwc2)

/************************************************************
*                        Hal Interface
************************************************************/
#define DUMP_STR_LEN (8192*2 - 1)
void MesonHwc2::dump(uint32_t* outSize, char* outBuffer) {
    if (outBuffer == NULL) {
        *outSize = DUMP_STR_LEN;
        return ;
    }

    String8 dumpstr;
    DebugHelper::getInstance().resolveCmd();

#ifdef HWC_RELEASE
    dumpstr.append("\nMesonHwc2 state(RELEASE):\n");
#else
    dumpstr.append("\nMesonHwc2 state(DEBUG):\n");
#endif

    if (DebugHelper::getInstance().dumpDetailInfo()) {
        mDisplayPipe->dump(dumpstr);
        HwcConfig::dump(dumpstr);
    }

    // dump composer status
    std::map<hwc2_display_t, std::shared_ptr<Hwc2Display>>::iterator it;
    for (it = mDisplays.begin(); it != mDisplays.end(); it++) {
        it->second->dump(dumpstr);
    }

    if (HwcConfig::getPipeline() == HWC_PIPE_LOOPBACK) {
        dumpstr.appendFormat("m3DMode:%d\n", m3DMode);
        dumpstr.appendFormat("mVdinPostMode:%d, mKeyStoneMode:%d\n",
                mVdinPostMode, mKeyStoneMode);
    }

    DebugHelper::getInstance().dump(dumpstr);
    dumpstr.append("\n");

    strncpy(outBuffer, dumpstr.c_str(), dumpstr.size() > DUMP_STR_LEN ? DUMP_STR_LEN : dumpstr.size());
}

// TODO: refactor when android t branch out
void MesonHwc2::getCapabilities(uint32_t* outCount, int32_t* outCapabilities) {
    *outCount = 2;

#ifdef ENABLE_AIDL
#if (PLATFORM_SDK_VERSION >= 34)
    auto connectorType = HwcConfig::getConnectorType(0);
    //TODO: enable boot display config when G support it for dual display
    if (HwcConfig::getDisplayNum() == 1) {
        // panel does not support hdr output control
        if (connectorType == HWC_HDMI_CVBS || connectorType == DRM_MODE_CONNECTOR_HDMIA) {
            *outCount = 3;
        }
    } else {
        *outCount = 1;
    }
#endif
#endif

    if (outCapabilities) {
        outCapabilities[0] = HWC2_CAPABILITY_SIDEBAND_STREAM;
        if (*outCount >= 2)
            outCapabilities[1] = HWC2_CAPABILITY_SKIP_VALIDATE;
#ifdef ENABLE_AIDL
        if (HwcConfig::getDisplayNum() == 1) {
            outCapabilities[1] = HWC3_CAPABILITY_BOOT_DISPLAY_CONFIG;
#if (PLATFORM_SDK_VERSION >= 34)
            if (connectorType == HWC_HDMI_CVBS || connectorType == DRM_MODE_CONNECTOR_HDMIA) {
                outCapabilities[2] = HWC3_HDR_OUTPUT_CONVERSION_CONFIG;
            }
#endif
        }
#endif
    }
}

int32_t MesonHwc2::getClientTargetSupport(hwc2_display_t display,
    uint32_t width __unused, uint32_t height __unused, int32_t format, int32_t dataspace) {
    GET_HWC_DISPLAY(display);
    if (format == HAL_PIXEL_FORMAT_RGBA_8888 &&
        dataspace == HAL_DATASPACE_UNKNOWN) {
        return HWC2_ERROR_NONE;
    }

    MESON_LOGE("getClientTargetSupport failed: format (%d), dataspace (%d)",
        format, dataspace);
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t MesonHwc2::registerCallback(int32_t descriptor,
    hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer) {
    hwc2_error_t ret = HWC2_ERROR_NONE;

    switch (descriptor) {
        case HWC2_CALLBACK_HOTPLUG:
            /* For android:
            When primary display is hdmi, we should always return connected event
            to surfaceflinger, or surfaceflinger will not boot and wait
            connected event.
            */
            mHotplugFn = reinterpret_cast<HWC2_PFN_HOTPLUG>(pointer);
            mHotplugData = callbackData;

            /*always to call hotplug for primary display,
            * for android always think primary is always connected.
            */
            if (mHotplugFn) {
                mHotplugFn(mHotplugData, HWC_DISPLAY_PRIMARY, HWC2_CONNECTION_CONNECTED);
                for (int i = 1; i < HwcConfig::getDisplayNum(); i ++) {
                    GET_HWC_DISPLAY(i);
                    if (hwcDisplay->isDisplayConnected()) {
                        MESON_LOGD("%s sent hotplug display:%d", __func__, i);
                        mHotplugFn(mHotplugData, i, HWC2_CONNECTION_CONNECTED);
                    }
                }
            }
            break;
        case HWC2_CALLBACK_REFRESH:
            mRefreshFn = reinterpret_cast<HWC2_PFN_REFRESH>(pointer);
            mRefreshData = callbackData;
            break;
        case HWC2_CALLBACK_VSYNC:
            mVsyncFn = reinterpret_cast<HWC2_PFN_VSYNC>(pointer);
            mVsyncData = callbackData;
            break;
        case HWC2_CALLBACK_VSYNC_2_4:
            mVsync24Fn = reinterpret_cast<HWC2_PFN_VSYNC_2_4>(pointer);
            mVsync24Data = callbackData;
            break;

        case HWC2_CALLBACK_VSYNC_PERIOD_TIMING_CHANGED:
            mVsyncPeriodTimingChangedFn = reinterpret_cast<HWC2_PFN_VSYNC_PERIOD_TIMING_CHANGED>(pointer);
            mVsyncPeriodData = callbackData;
            break;

        default:
            MESON_LOGE("register unknown callback (%d)", descriptor);
            ret = HWC2_ERROR_UNSUPPORTED;
            break;
    }

    return ret;
}

int32_t MesonHwc2::getDisplayName(hwc2_display_t display,
    uint32_t* outSize, char* outName) {
    GET_HWC_DISPLAY(display);
    const char * name = hwcDisplay->getName();
    if (name == NULL) {
        MESON_LOGE("getDisplayName (%d) failed", (int)display);
    } else {
        *outSize = strlen(name) + 1;
        if (outName) {
            strcpy(outName, name);
        }
    }
    return HWC2_ERROR_NONE;
}

int32_t  MesonHwc2::getDisplayType(hwc2_display_t display,
    int32_t* outType) {
    GET_HWC_DISPLAY(display);

    /*only physical device supported.*/
    *outType = HWC2_DISPLAY_TYPE_PHYSICAL;

    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::getDozeSupport(hwc2_display_t display,
    int32_t* outSupport) {
    /*No doze support now.*/
    CHECK_DISPLAY_VALID(display);
    *outSupport = 0;
    return HWC2_ERROR_NONE;
}

int32_t  MesonHwc2::getColorModes(hwc2_display_t display,
    uint32_t* outNumModes, int32_t*  outModes) {
    CHECK_DISPLAY_VALID(display);
    /*Only support native color mode.*/
    *outNumModes = 1;
    if (outModes) {
         outModes[0] = HAL_COLOR_MODE_NATIVE;
     }
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setColorMode(hwc2_display_t display, int32_t mode __unused) {
    CHECK_DISPLAY_VALID(display);
     if (mode < HAL_COLOR_MODE_NATIVE)
        return HWC2_ERROR_BAD_PARAMETER;

    if (mode == HAL_COLOR_MODE_NATIVE)
        return HWC2_ERROR_NONE;
    else
        return HWC2_ERROR_UNSUPPORTED;
}

int32_t MesonHwc2::setColorTransform(hwc2_display_t display,
    const float* matrix, int32_t hint) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setColorTransform(matrix, (android_color_transform_t)hint);
}

int32_t MesonHwc2::getHdrCapabilities(hwc2_display_t display,
    uint32_t* outNumTypes, int32_t* outTypes, float* outMaxLuminance,
    float* outMaxAverageLuminance, float* outMinLuminance) {
    GET_HWC_DISPLAY(display);
    const drm_hdr_capabilities_t * caps = hwcDisplay->getHdrCapabilities();
    if (caps) {
        bool getInfo = false;
        if (outTypes)
            getInfo = true;

        if (caps->DolbyVisionSupported) {
            *outNumTypes = *outNumTypes + 1;
            if (getInfo) {
                *outTypes++ = HAL_HDR_DOLBY_VISION;
            }
        }
        if (caps->HLGSupported) {
            *outNumTypes = *outNumTypes + 1;
            if (getInfo) {
                *outTypes++ = HAL_HDR_HLG;
            }
        }
        if (caps->HDR10Supported) {
            *outNumTypes = *outNumTypes + 1;
            if (getInfo) {
                *outTypes++ = HAL_HDR_HDR10;
            }
        }

        if (caps->HDR10PlusSupported) {
            *outNumTypes = *outNumTypes + 1;
            if (getInfo) {
                *outTypes++ = HAL_HDR_HDR10_PLUS;
            }
        }

#if (PLATFORM_SDK_VERSION >= 34)
        if (caps->DOLBY_VISION_4K30_Supported) {
            *outNumTypes = *outNumTypes + 1;
            if (getInfo) {
                *outTypes++ = DOLBY_VISION_4K30;
            }
        }
#endif
        if (getInfo) {
            *outMaxLuminance = caps->maxLuminance;
            *outMaxAverageLuminance = caps->avgLuminance;
            *outMinLuminance = caps->minLuminance;
        }
    } else {
        *outNumTypes = 0;
    }

    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setPowerMode(hwc2_display_t display,
    int32_t mode) {
    GET_HWC_DISPLAY(display);
    int32_t ret = hwcDisplay->setPowerMode((hwc2_power_mode_t)mode);

    if (hwcDisplay->needSwitchConnector()) {
        /*
         * need switch connector
         */
        if (mode == HWC2_POWER_MODE_OFF && mAidlCilentIsSF) {
            mDisplayPipe->handleEvent(DRM_EVENT_HDMITX_HOTPLUG, DRM_EVENT_SUSPEND);
        }
    }

    return ret;
}

int32_t MesonHwc2::setVsyncEnable(hwc2_display_t display,
    int32_t enabled) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setVsyncEnable((hwc2_vsync_t)enabled);
}

int32_t MesonHwc2::getActiveConfig(hwc2_display_t display,
    hwc2_config_t* outConfig) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getActiveConfig(outConfig);
}

int32_t  MesonHwc2::getDisplayConfigs(hwc2_display_t display,
            uint32_t* outNumConfigs, hwc2_config_t* outConfigs) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getDisplayConfigs(outNumConfigs, outConfigs);
}

int32_t MesonHwc2::setActiveConfig(hwc2_display_t display,
        hwc2_config_t config) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setActiveConfig(config);
}

int32_t  MesonHwc2::getDisplayAttribute(hwc2_display_t display,
    hwc2_config_t config, int32_t attribute, int32_t* outValue) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getDisplayAttribute(config, attribute, outValue);
}

/*************Virtual display api below*************/
int32_t MesonHwc2::createVirtualDisplay(uint32_t width, uint32_t height,
    int32_t* format, hwc2_display_t* outDisplay) {
    hwc2_display_t id = getVirtualDisplayId();
    std::shared_ptr<VirtualDisplay> disp = std::make_shared<VirtualDisplay>(width, height, id);
    disp->initialize();
    disp->setFormat(format);
    mDisplays.emplace(id, disp);
    mDisplayPipe->addVirtualDisplay(disp);
    *outDisplay = id;
    GET_HWC_DISPLAY(0);
    hwcDisplay->createCallbackThread(true);
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::destroyVirtualDisplay(hwc2_display_t display) {
    GET_HWC_DISPLAY(display);
    CHECK_DISPLAY_VALID(display);
    VirtualDisplay * disp = (VirtualDisplay *)hwcDisplay.get();
    disp->stopVdin();
    mDisplays.erase(display);
    freeVirtualDisplayId(display);
    {
        GET_HWC_DISPLAY(0);
        hwcDisplay->createCallbackThread(false);
    }
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setOutputBuffer(hwc2_display_t display,
    buffer_handle_t buffer, int32_t releaseFence) {
    GET_HWC_DISPLAY(display);
    VirtualDisplay * disp = (VirtualDisplay *)hwcDisplay.get();
    return disp->setOutputBuffer(buffer, releaseFence);
}

uint32_t MesonHwc2::getMaxVirtualDisplayCount() {
    return MESON_VIRTUAL_DISPLAY_MAX_COUNT;
}

/*************Compose api below*************/
int32_t  MesonHwc2::acceptDisplayChanges(hwc2_display_t display) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->acceptDisplayChanges();
}

int32_t MesonHwc2::getChangedCompositionTypes( hwc2_display_t display,
    uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t*  outTypes) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getChangedCompositionTypes(outNumElements,
        outLayers, outTypes);
}

int32_t MesonHwc2::getDisplayRequests(hwc2_display_t display,
    int32_t* outDisplayRequests, uint32_t* outNumElements,
    hwc2_layer_t* outLayers, int32_t* outLayerRequests) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getDisplayRequests(outDisplayRequests, outNumElements,
        outLayers, outLayerRequests);
}

int32_t MesonHwc2::setClientTarget(hwc2_display_t display,
    buffer_handle_t target, int32_t acquireFence,
    int32_t dataspace, hwc_region_t damage) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setClientTarget(target, acquireFence,
        dataspace, damage);
}

int32_t MesonHwc2::getReleaseFences(hwc2_display_t display,
    uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* outFences) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getReleaseFences(outNumElements,
        outLayers, outFences);
}

int32_t MesonHwc2::validateDisplay(hwc2_display_t display,
    uint32_t* outNumTypes, uint32_t* outNumRequests) {
    GET_HWC_DISPLAY(display);
    setCalibrateInfo(display);

    return hwcDisplay->validateDisplay(outNumTypes,
        outNumRequests);
}

int32_t MesonHwc2::presentDisplay(hwc2_display_t display,
    int32_t* outPresentFence) {
    GET_HWC_DISPLAY(display);
    /*handle display request*/
    uint32_t request = getDisplayRequest();
    handleDisplayRequest(request);
    if (request != 0) {
        hwcDisplay->outsideChanged();
    }
    return hwcDisplay->presentDisplay(outPresentFence);
}

/*************Layer api below*************/
int32_t MesonHwc2::createLayer(hwc2_display_t display, hwc2_layer_t* outLayer) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->createLayer(outLayer);
}

int32_t MesonHwc2::destroyLayer(hwc2_display_t display, hwc2_layer_t layer) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->destroyLayer(layer);
}

int32_t MesonHwc2::setCursorPosition(hwc2_display_t display,
    hwc2_layer_t layer, int32_t x, int32_t y) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setCursorPosition(layer, x, y);
}

int32_t MesonHwc2::setLayerBuffer(hwc2_display_t display, hwc2_layer_t layer,
    buffer_handle_t buffer, int32_t acquireFence) {
    int32_t ret;
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    ret = hwcLayer->setBuffer(buffer, acquireFence);
    if (ret == HWC2_ERROR_NONE)
        hwcDisplay->handleVtThread();

    return ret;
}

int32_t MesonHwc2::setLayerSurfaceDamage(hwc2_display_t display,
    hwc2_layer_t layer, hwc_region_t damage) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setSurfaceDamage(damage);
}

int32_t MesonHwc2::setLayerBlendMode(hwc2_display_t display,
    hwc2_layer_t layer, int32_t mode) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setBlendMode((hwc2_blend_mode_t)mode);
}

int32_t MesonHwc2::setLayerColor(hwc2_display_t display,
    hwc2_layer_t layer, hwc_color_t color) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setColor(color);
}

int32_t MesonHwc2::setLayerCompositionType(hwc2_display_t display,
    hwc2_layer_t layer, int32_t type) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setCompositionType(type);
}

int32_t MesonHwc2::setLayerDataspace(hwc2_display_t display,
    hwc2_layer_t layer, int32_t dataspace) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setDataspace((android_dataspace_t)dataspace);
}

int32_t MesonHwc2::setLayerDisplayFrame(hwc2_display_t display,
    hwc2_layer_t layer, hwc_rect_t frame) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setDisplayFrame(frame);
}

int32_t MesonHwc2::setLayerPlaneAlpha(hwc2_display_t display,
    hwc2_layer_t layer, float alpha) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setPlaneAlpha(alpha);
}

int32_t MesonHwc2::setLayerSidebandStream(hwc2_display_t display,
    hwc2_layer_t layer, const native_handle_t* stream) {
    int32_t ret;
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    if (display == MESON_VIRTUAL_DISPLAY_ID_START) {
        return HWC2_ERROR_NONE;
    }
    ret = hwcLayer->setSidebandStream(stream, hwcDisplay);
    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>> bLayers = hwcDisplay->getAllLayers();
    for (auto it = bLayers.begin(); it != bLayers.end(); it++) {
        std::shared_ptr<Hwc2Layer> layer = it->second;
        if (layer != hwcLayer && layer->getVideoTunnelId() == hwcLayer->getVideoTunnelId()) {
            MESON_LOGE("layer id is %" PRIu64",tunnel ID %d exist",layer->getUniqueId() ,layer->getVideoTunnelId());
            break;
        }
    }
    if (ret == HWC2_ERROR_NONE)
        hwcDisplay->handleVtThread();

    return ret;
}

int32_t MesonHwc2::setLayerSourceCrop(hwc2_display_t display,
    hwc2_layer_t layer, hwc_frect_t crop) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setSourceCrop(crop);
}

int32_t MesonHwc2::setLayerTransform(hwc2_display_t display,
    hwc2_layer_t layer, int32_t transform) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setTransform((hwc_transform_t)transform);
}

int32_t MesonHwc2::setLayerVisibleRegion(hwc2_display_t display,
    hwc2_layer_t layer, hwc_region_t visible) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setVisibleRegion(visible);
}

int32_t  MesonHwc2::setLayerZorder(hwc2_display_t display,
    hwc2_layer_t layer, uint32_t z) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setZorder(z);
}

int32_t MesonHwc2::getRenderIntents(hwc2_display_t display,
    int32_t mode, uint32_t* outNumIntents, int32_t* outIntents) {
    GET_HWC_DISPLAY(display);

    if (mode < HAL_COLOR_MODE_NATIVE)
        return HWC2_ERROR_BAD_PARAMETER;
    if (mode != HAL_COLOR_MODE_NATIVE)
        return HWC2_ERROR_UNSUPPORTED;

    *outNumIntents = 1;
    if (outIntents) {
        *outIntents = HAL_RENDER_INTENT_COLORIMETRIC;
    }

    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setColorModeWithRenderIntent(
    hwc2_display_t display, int32_t  mode, int32_t  intent) {
    GET_HWC_DISPLAY(display);

    if (mode < HAL_COLOR_MODE_NATIVE ||
        intent < HAL_RENDER_INTENT_COLORIMETRIC)
        return HWC2_ERROR_BAD_PARAMETER;
    if (mode != HAL_COLOR_MODE_NATIVE ||
        intent != HAL_RENDER_INTENT_COLORIMETRIC)
        return HWC2_ERROR_UNSUPPORTED;

    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setLayerPerFrameMetadata(
        hwc2_display_t display, hwc2_layer_t layer,
        uint32_t numElements, const int32_t* /*hw2_per_frame_metadata_key_t*/ keys,
        const float* metadata) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return hwcLayer->setPerFrameMetadata(numElements, keys, metadata);
}

int32_t MesonHwc2::getPerFrameMetadataKeys(
        hwc2_display_t display,
        uint32_t* outNumKeys,
        int32_t* /*hwc2_per_frame_metadata_key_t*/ outKeys) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getFrameMetadataKeys(outNumKeys, outKeys);
}

int32_t MesonHwc2::getDisplayIdentificationData(hwc2_display_t display, uint8_t* outPort,
            uint32_t* outDataSize, uint8_t* outData) {
    std::vector<uint8_t> edid;
    uint32_t port;
    GET_HWC_DISPLAY(display);
    if (0 != hwcDisplay->getDisplayIdentificationData(port, edid)) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    if (outData == nullptr) {
        *outDataSize = edid.size();
    } else {
        if (*outDataSize != edid.size()) {
            return HWC2_ERROR_BAD_PARAMETER;
        }
        memcpy(outData, edid.data(), sizeof(uint8_t) * edid.size());
    }
    *outPort = (uint8_t)port;
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::getDisplayCapabilities(hwc2_display_t display, uint32_t* outNumCapabilities, uint32_t* outCapabilities) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getDisplayCapabilities(outNumCapabilities, outCapabilities);
}

int32_t MesonHwc2::getDisplayBrightnessSupport(hwc2_display_t display, bool* outSupport) {
    GET_HWC_DISPLAY(display);
    *outSupport = false;
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::setDisplayBrightness(hwc2_display_t display, float brightness) {
    GET_HWC_DISPLAY(display);
    (void) brightness;
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t MesonHwc2::getDisplayConnectionType(hwc2_display_t display, uint32_t* outType) {
    GET_HWC_DISPLAY(display);
    *outType = (it->first == 0 ? DISPLAY_TYPE_INTERNAL : DISPLAY_TYPE_EXTERNAL);
    return HWC2_ERROR_NONE;
}

int32_t MesonHwc2::getDisplayVsyncPeriod(hwc2_display_t display, hwc2_vsync_period_t* outVsyncPeriod) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getDisplayVsyncPeriod(outVsyncPeriod);
}

int32_t MesonHwc2::setActiveConfigWithConstraints(hwc2_display_t display, hwc2_config_t config __unused,
    hwc_vsync_period_change_constraints_t* vsyncPeriodChangeConstraints,
    hwc_vsync_period_change_timeline_t* outTimeline) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setActiveConfigWithConstraints(config, vsyncPeriodChangeConstraints, outTimeline);
}

int32_t MesonHwc2::setAutoLowLatencyMode(hwc2_display_t display, bool on) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setAutoLowLatencyMode(on);
}

int32_t MesonHwc2::getSupportedContentTypes (hwc2_display_t display, uint32_t* outNum, uint32_t* outSupportedContentTypes) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getSupportedContentTypes(outNum, outSupportedContentTypes);
}

int32_t MesonHwc2::setContentType(hwc2_display_t display, uint32_t contentType) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setContentType(contentType);
}

/* hwc3 */
int32_t MesonHwc2::setBootDisplayConfig(hwc2_display_t display, uint32_t config) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setBootConfig(config);
}

int32_t MesonHwc2::clearBootDisplayConfig(hwc2_display_t display) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->clearBootConfig();
}

int32_t MesonHwc2::getPreferredBootDisplayConfig(hwc2_display_t display,
        int32_t* config) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getPreferredBootConfig(config);
}

int32_t MesonHwc2::getDisplayPhysicalOrientation(hwc2_display_t display,
        int32_t* outOrientation) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->getPhysicalOrientation(outOrientation);
}

int32_t MesonHwc2::setExpectedPresentTime(hwc2_display_t display,
        int64_t expectedPresentTime) {
    GET_HWC_DISPLAY(display);
    return hwcDisplay->setExpectedPresentTime(expectedPresentTime);
}

int32_t MesonHwc2::setLayerBrightness(hwc2_display_t display,
        hwc2_layer_t layer, float brightness) {
    GET_HWC_DISPLAY(display);
    GET_HWC_LAYER(hwcDisplay, layer);
    return  hwcLayer->setBrightness(brightness);
}

int32_t MesonHwc2::setAidlClientPid(int32_t pid) {
    GET_HWC_DISPLAY(0);
    char path[32] = {0x0};
    char name[32] = {0x0};
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        mAidlCilentIsSF = false;
    } else {
        if (fgets(name, sizeof(name), f) != NULL && strstr(name, "surfaceflinger")) {
            mAidlCilentIsSF = true;
        } else {
            mAidlCilentIsSF = false;
        }
        fclose(f);
    }
    return hwcDisplay->setAidlClientPid(pid);
}

int32_t MesonHwc2::getHdrConversionCapabilities(uint32_t* outNumCapability,
            drm_hdr_conversion_capability_t* outConversionCapability) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->getHdrConversionCapabilities(outNumCapability,outConversionCapability);
}

int32_t MesonHwc2::setHdrConversionStrategy(bool passThrough, uint32_t numElements, bool isAuto,
            uint32_t* autoAllowedHdrTypes, uint32_t* preferredHdrOutputType) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setHdrConversionStrategy(passThrough, numElements, isAuto,
                                                    autoAllowedHdrTypes, preferredHdrOutputType);
}

int32_t MesonHwc2::getOverlaySupport(uint32_t* numElements,
            uint32_t* pixelFormats) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->getOverlaySupport(numElements, pixelFormats);
}

/**********************Amlogic ext display interface*******************/
int32_t MesonHwc2::setPostProcessor(bool bEnable) {
    mDisplayRequests |= bEnable ? rPostProcessorStart : rPostProcessorStop;
    return 0;
}

int32_t MesonHwc2::setCalibrateInfo(hwc2_display_t display){
    GET_HWC_DISPLAY(display);
    int32_t caliX,caliY,caliW,caliH;
    drm_mode_info_t mDispMode;
    hwcDisplay->getDispMode(mDispMode);

    /*default info*/
    caliX = caliY = 0;
    caliW = mDispMode.pixelW;
    caliH = mDispMode.pixelH;
    drm_rect_wh_t viewPort;
    getViewPort(viewPort, hwcDisplay->getConnectorType());

    if (!HwcConfig::preDisplayCalibrateEnabled() &&
            viewPort.x >= 0 &&viewPort.y >= 0 &&
            viewPort.w > 0 && viewPort.h > 0 &&
            viewPort.x + viewPort.w <= mDispMode.pixelW &&
            viewPort.y + viewPort.h <= mDispMode.pixelH) {
        caliX = viewPort.x;
        caliY = viewPort.y;
        caliW = viewPort.w;
        caliH = viewPort.h;
    }

#ifdef GET_REQUEST_FROM_PROP
    if (mKeyStoneMode) {
        caliX += 1;
        caliY += 1;
        caliW -= 2;
        caliH -= 2;
    }
#endif

    return hwcDisplay->setCalibrateInfo(caliX,caliY,caliW,caliH);
}

uint32_t MesonHwc2::getDisplayRequest() {
    /*read extend prop to update display request.*/
#ifdef GET_REQUEST_FROM_PROP
    if (HwcConfig::getPipeline() == HWC_PIPE_LOOPBACK) {
        char val[PROP_VALUE_LEN_MAX];
        bool bVal = false;

        /*get 3dmode status*/
        bVal = sys_get_bool_prop("vendor.hwc.3dmode", false);
        if (m3DMode != bVal) {
            mDisplayRequests |= bVal ? r3DModeEnable : r3DModeDisable;
            m3DMode = bVal;
        }

        if (HwcConfig::alwaysVdinLoopback()) {
            bVal = sys_get_bool_prop("vendor.hwc.postprocessor", true);
            if (mVdinPostMode != bVal) {
                mDisplayRequests |= bVal ? rPostProcessorStart : rPostProcessorStop;
                mVdinPostMode  = bVal;
            }

            if (mVdinPostMode) {
                /*get keystone status*/
                uint32_t width,height;
                HwcConfig::getFramebufferSize(0, width, height);
                DEFAULT_CROD(width, height);
                bVal = false;
                if ((sys_get_string_prop("persist.vendor.hwc.keystone", val) > 0 &&
                    strcmp(val, "0") != 0 && strcmp(val, keystoneProp) != 0) || strcmp(mKeystoneConfigs.c_str(), "0") != 0) {
                    bVal = true;
                }
                if (mKeyStoneMode != bVal) {
                    mDisplayRequests |= bVal ? rKeystoneEnable : rKeystoneDisable;
                    mKeyStoneMode = bVal;
                }
            } else {
                mKeyStoneMode = false;
            }
        } else {
            bVal = false;
            if ((sys_get_string_prop("persist.vendor.hwc.keystone", val) > 0 &&
                strcmp(val, "0") != 0) || strcmp(mKeystoneConfigs.c_str(), "0") != 0) {
                bVal = true;
            }
            if (mKeyStoneMode != bVal) {
                mDisplayRequests |= bVal ?
                    (rPostProcessorStart | rKeystoneEnable) : rPostProcessorStop;
                mKeyStoneMode = bVal;
            }
        }
    }
#endif
    /*record and reset requests.*/
    uint32_t request = mDisplayRequests;
    mDisplayRequests = 0;
    if (request > 0) {
        MESON_LOGD("getDisplayRequest %x", request);
    }
    return request;
}

int32_t MesonHwc2::handleDisplayRequest(uint32_t request) {
    mDisplayPipe->handleRequest(request);
    return 0;
}

/* get primary display vsync timestamp and vsync period */
bool MesonHwc2::getDisplayVsyncAndPeriod(int64_t& timestamp, int32_t& vsyncPeriodNanos) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->getDisplayVsyncAndPeriod(timestamp, vsyncPeriodNanos);
}

int32_t MesonHwc2::setPerferredMode(std::string mode) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setPerferredMode(mode);
}

int32_t MesonHwc2::setColorSpace(std::string colorspace) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setColorSpace(colorspace);
}

int32_t MesonHwc2::clearUserDisplayConfig() {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->clearUserDisplayConfig();
}

int32_t MesonHwc2::setDvMode(std::string dv_mode) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setDvMode(dv_mode);
}

/**********************Internal Implement********************/

class MesonHwc2Observer
    : public Hwc2DisplayObserver {
public:
    MesonHwc2Observer(hwc2_display_t display, MesonHwc2 * hwc) {
        mDispId = display;
        mHwc = hwc;
    }
    ~MesonHwc2Observer() {
        MESON_LOGD("MesonHwc2Observer disp(%d) destruct.", (int)mDispId);
    }

    void refresh() {
        MESON_LOGD("Display (%d) ask for refresh.", (int)mDispId);

        if (mHwc->mWhiteBoardMode) {
            if (kill(mHwc->mCallingPid, 0) == -1 && errno == ESRCH) {
                mHwc->setWriteBoardMode(false, mHwc->mCallingPid);
            }
        }

        mHwc->refresh(mDispId);
    }

    void onVsync(int64_t timestamp, uint32_t vsyncPeriodNanos) {
        mHwc->onVsync(mDispId, timestamp, vsyncPeriodNanos);
    }

    void onHotplug(bool connected) {
        mHwc->onHotplug(mDispId, connected);
    }

    void onVsyncPeriodTimingChanged(hwc_vsync_period_change_timeline_t* updatedTimeline) {
        MESON_LOGD("Display (%d) timeline changed.", (int)mDispId);
        mHwc->onVsyncPeriodTimingChanged(mDispId, updatedTimeline);
    }

protected:
    hwc2_display_t mDispId;
    MesonHwc2 * mHwc;
};

MesonHwc2::MesonHwc2() {
    mVirtualDisplayIds = 0;
    mHotplugFn = NULL;
    mHotplugData = NULL;
    mRefreshFn = NULL;
    mRefreshData = NULL;
    mVsyncFn = NULL;
    mVsyncData = NULL;
    mVsync24Fn = NULL;
    mVsync24Data = NULL;
    mVsyncPeriodTimingChangedFn = NULL;
    mVsyncPeriodData = NULL;
    mDisplayRequests = 0;
    mChangedViewPort = false;
    mAidlCilentIsSF = true;
    initialize();
}

MesonHwc2::~MesonHwc2() {
    mDisplays.clear();
}

void MesonHwc2::refresh(hwc2_display_t  display) {
    if (mRefreshFn) {
        mRefreshFn(mRefreshData, display);
        return;
    }

    MESON_LOGE("No refresh callback registered.");
}

void MesonHwc2::onVsync(hwc2_display_t display, int64_t timestamp, uint32_t vsyncPeriodNanos) {
    if (mVsync24Fn) {
        mVsync24Fn(mVsync24Data, display, timestamp, vsyncPeriodNanos);
        return;
    }

    if (mVsyncFn) {
        mVsyncFn(mVsyncData, display, timestamp);
        return;
    }

    MESON_LOGE("No vsync callback registered.");
}

void MesonHwc2::onHotplug(hwc2_display_t display, bool connected) {
    if (display == HWC_DISPLAY_PRIMARY && !HwcConfig::primaryHotplugEnabled()) {
        MESON_LOGD("Primary don't config to support hotplug.");
        return;
    }

    hwc2_connection_t connection =
        connected ? HWC2_CONNECTION_CONNECTED : HWC2_CONNECTION_DISCONNECTED;

    if (connection == HWC2_CONNECTION_DISCONNECTED) {
        if (display == HWC_DISPLAY_PRIMARY) {
            connection = HWC2_CONNECTION_CONNECTED;
        } else {
            /*will remove display, clear display resource now. */
            std::shared_ptr<Hwc2Display> hwcDisplay;
            auto it = mDisplays.find(display);
            if (it != mDisplays.end()) {
                it->second->cleanupBeforeDestroy();
            }  else {
                MESON_LOGE("(%s) met invalid display id (%d)",
                    __func__, (int)display);
            }
        }
    }

    if (mHotplugFn) {
        MESON_LOGD("On hotplug, Fn: %p, Data: %p, display: %d, connection: %d",
                mHotplugFn, mHotplugData, (int)display, connection);
        mHotplugFn(mHotplugData, display, connection);
    } else {
        MESON_LOGE("No hotplug callback registered.");
    }
}

void MesonHwc2::onVsyncPeriodTimingChanged(hwc2_display_t display,
        hwc_vsync_period_change_timeline_t* updatedTimeline) {
    if (mVsyncPeriodTimingChangedFn) {
        mVsyncPeriodTimingChangedFn(mVsyncPeriodData, display, updatedTimeline);
    }
}

int32_t MesonHwc2::captureDisplayScreen(buffer_handle_t hnd) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->captureDisplayScreen(hnd);
}

int32_t MesonHwc2::getWhiteBoardHanle(native_handle_t** hnd) {
    GET_HWC_DISPLAY(0);
    if (hwcDisplay->mVirtualLayer == nullptr) {
        MESON_LOGE("Please enable white board first");
        return -1;
    }
    *hnd = hwcDisplay->mVirtualLayer->mBufferHandle;
    return 0;
}

bool MesonHwc2::setWriteBoardMode(bool mode, int callingPid) {
    GET_HWC_DISPLAY(0);
    mCallingPid = callingPid;
    mWhiteBoardMode = mode;
    hwcDisplay->setWriteBoardMode(mode);
    return true;
}

bool MesonHwc2::enableSyncProtection(bool mode) {
    GET_HWC_DISPLAY(0);
    hwcDisplay->enableSyncProtection(mode);
    return true;
}

bool MesonHwc2::getWriteBoardMode(bool& mode) {
    GET_HWC_DISPLAY(0);
    hwcDisplay->getWriteBoardMode(mode);
    return true;
}

bool MesonHwc2::setWBDisplayFrame(int x, int y) {
    GET_HWC_DISPLAY(0);
    if (x < 0 || y < 0 || x > FB_SIZE_4K_W || y > FB_SIZE_4K_H) {
        MESON_LOGE("setWBDisplayFrame the position is invalid");
        return false;
    }
    hwcDisplay->setWBDisplayFrame(x, y);

    return true;
}

bool MesonHwc2::hideVideoLayer(bool hide) {
    GET_HWC_DISPLAY(0);
    hwcDisplay->hideVideoLayer(hide);
    return true;
}

bool MesonHwc2::setViewPort(const drm_rect_wh_t viewPort, drm_connector_type_t type) {
    std::lock_guard<std::mutex> lock(mViewPortMutex);
    GET_HWC_DISPLAY(0);
    mViewPorts.insert_or_assign(type, viewPort);
    hwcDisplay->outsideChanged();
    return true;
}

bool MesonHwc2::getViewPort(drm_rect_wh_t & viewPort, drm_connector_type_t type) {
    auto it = mViewPorts.find(type);
    if (it != mViewPorts.end()) {
        viewPort = it -> second;
        return true;
    }
    return false;
}

bool MesonHwc2::disableSideband(bool isDisable) {
    GET_HWC_DISPLAY(0);
    hwcDisplay->disableSideband(isDisable);
    return true;
}
bool MesonHwc2::setFrameRateHint(std::string value) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setFrameRateHint(value);
}

int32_t MesonHwc2::setFrameRate(float value) {
    GET_HWC_DISPLAY(0);
    return hwcDisplay->setFrameRate(value);
}

int32_t MesonHwc2::getDisplays(std::map<hwc2_display_t, shared_ptr<Hwc2Display>> & displays) {
    displays = mDisplays;
    return 0;
}

int32_t MesonHwc2::initialize() {
    std::map<uint32_t, std::shared_ptr<HwcDisplay>> mhwcDisps;
    mDisplayPipe = createDisplayPipe(HwcConfig::getPipeline());

    for (uint32_t i = 0; i < HwcConfig::getDisplayNum(); i ++) {
        /*create hwc2display*/
        auto displayObserver = std::make_shared<MesonHwc2Observer>(i, this);
        auto disp = std::make_shared<Hwc2Display>(displayObserver, i);
        disp->initialize();
        mDisplays.emplace(i, disp);
        auto baseDisp = std::dynamic_pointer_cast<HwcDisplay>(disp);
        mhwcDisps.emplace(i, baseDisp);
    }

    mDisplayPipe->init(mhwcDisps);
/*For round corner*/
#ifdef ENABLE_VIRTUAL_LAYER
    GET_HWC_DISPLAY(0);
    hwc2_layer_t outLayer  = 0;
    hwcDisplay->createVirtualLayer(&outLayer);
    if (outLayer == 0) {
        MESON_LOGD("createVirtualLayer failed");
    }
#endif
    return HWC2_ERROR_NONE;
}

bool MesonHwc2::isDisplayValid(hwc2_display_t display) {
    std::map<hwc2_display_t, std::shared_ptr<Hwc2Display>>::iterator it =
        mDisplays.find(display);

    if (it != mDisplays.end())
        return true;

    return false;
}

uint32_t MesonHwc2::getVirtualDisplayId() {
    uint32_t idx;
    for (idx = MESON_VIRTUAL_DISPLAY_ID_START; idx < 31; idx ++) {
        if ((mVirtualDisplayIds & (1 << idx)) == 0) {
            mVirtualDisplayIds |= (1 << idx);
            MESON_LOGD("getVirtualDisplayId (%d) to (%x)", idx, mVirtualDisplayIds);
            return idx;
        }
    }

    MESON_LOGE("All virtual display id consumed.\n");
    return 0;
}

void MesonHwc2::freeVirtualDisplayId(uint32_t id) {
    mVirtualDisplayIds &= ~(1 << id);
    MESON_LOGD("freeVirtualDisplayId (%d) to (%x)", id, mVirtualDisplayIds);
}

bool MesonHwc2::setKeystoneCorrection(std::string params) {
    mKeystoneConfigs = params;
    GET_HWC_DISPLAY(0);
    hwcDisplay->setKeystoneCorrection(params);
    return true;
}

bool MesonHwc2::setReverseMode(int type) {
    GET_HWC_DISPLAY(0);
    hwcDisplay->setReverseMode(type);
    return true;
}

