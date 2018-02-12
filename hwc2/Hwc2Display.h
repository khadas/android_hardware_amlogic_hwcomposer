/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HWC2_DISPLAY_H
#define HWC2_DISPLAY_H

#include <map>
#include <unordered_map>
#include <hardware/hwcomposer2.h>

#include <HwDisplayManager.h>
#include <HwDisplayPlane.h>
#include <HwDisplayConnector.h>

#include <ComposerFactory.h>
#include <IComposeDevice.h>
#include <ICompositionStrategy.h>
#include "Hwc2Layer.h"
#include "MesonHwc2Defs.h"
#include "HwcModeMgr.h"


class Hwc2DisplayObserver  {
public:
    Hwc2DisplayObserver(){};
    virtual ~Hwc2DisplayObserver(){};
    virtual void refresh() = 0;
    virtual void onVsync(int64_t timestamp) = 0;
    virtual void onHotplug(bool connected) = 0;
};


class Hwc2Display : public HwDisplayObserver {
/*HWC2 interfaces.*/
public:
    /*Connector releated.*/
    virtual const char * getName();
    virtual const hdr_capabilities_t * getHdrCapabilities();

    /*Vsync*/
    virtual hwc2_error_t setVsyncEnable(hwc2_vsync_t enabled);
    virtual hwc2_error_t setVsyncPeriod(int32_t period);

    /*Layer releated.*/
    virtual std::shared_ptr<Hwc2Layer> getLayerById(hwc2_layer_t id);
    virtual hwc2_error_t createLayer(hwc2_layer_t * outLayer);
    virtual hwc2_error_t destroyLayer(hwc2_layer_t  inLayer);
    virtual hwc2_error_t setCursorPosition(hwc2_layer_t layer,
        int32_t x, int32_t y);

    virtual hwc2_error_t setColorTransform(const float* matrix,
        android_color_transform_t hint);
    virtual hwc2_error_t setPowerMode(hwc2_power_mode_t mode);

    /*Compose flow*/
    virtual hwc2_error_t validateDisplay(uint32_t* outNumTypes,
        uint32_t* outNumRequests);
    virtual hwc2_error_t presentDisplay(int32_t* outPresentFence);
    virtual hwc2_error_t acceptDisplayChanges();
    virtual hwc2_error_t getChangedCompositionTypes(
        uint32_t* outNumElements, hwc2_layer_t* outLayers,
        int32_t*  outTypes);
    virtual hwc2_error_t getDisplayRequests(
        int32_t* outDisplayRequests, uint32_t* outNumElements,
        hwc2_layer_t* outLayers,int32_t* outLayerRequests);
    virtual hwc2_error_t setClientTarget( buffer_handle_t target,
        int32_t acquireFence, int32_t dataspace, hwc_region_t damage);
    virtual hwc2_error_t getReleaseFences(uint32_t* outNumElements,
        hwc2_layer_t* outLayers, int32_t* outFences);

    /*display attrbuites*/
    virtual hwc2_error_t  getDisplayConfigs(
        uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    virtual hwc2_error_t  getDisplayAttribute(
        hwc2_config_t config, int32_t attribute, int32_t* outValue);
    virtual hwc2_error_t getActiveConfig(hwc2_config_t* outConfig);
    virtual hwc2_error_t setActiveConfig(hwc2_config_t config);
    virtual hwc2_error_t updateDisplayAttribute();

/*Additional interfaces.*/
public:
    Hwc2Display(hw_display_id dspId, std::shared_ptr<Hwc2DisplayObserver> observer);
    virtual ~Hwc2Display();
    virtual int32_t initialize();
    virtual void dump(String8 & dumpstr);

/*Additional interfaces for hw display.*/
public:
    void onVsync(int64_t timestamp);
    void onHotplug(bool connected);
    void onModeChanged();


protected:
    /* For compose. */
    hwc2_error_t collectLayersForPresent();
    hwc2_error_t collectComposersForPresent();
    hwc2_error_t collectPlanesForPresent();
    hwc2_error_t collectCompositionRequest(
            uint32_t* outNumTypes, uint32_t* outNumRequests);

    void dumpPresentComponents();

protected:
    std::unordered_map<hwc2_layer_t, std::shared_ptr<Hwc2Layer>> mLayers;
    std::shared_ptr<Hwc2DisplayObserver> mObserver;

    /*hw releated components*/
    hw_display_id mHwId;
    std::shared_ptr<HwDisplayCrtc> mCrtc;
    std::shared_ptr<HwDisplayConnector> mConnector;
    std::vector<std::shared_ptr<HwDisplayPlane>> mPlanes;

    /*composition releated components*/
    std::map<meson_composer_t, std::shared_ptr<IComposeDevice>> mComposers;
    std::shared_ptr<ICompositionStrategy> mCompositionStrategy;

    /*display configs*/
    std::shared_ptr<HwcModeMgr> mModeMgr;

    /* members used in present.*/
    std::vector<std::shared_ptr<DrmFramebuffer>> mPresentLayers;
    std::vector<std::shared_ptr<IComposeDevice>> mPresentComposers;
    std::vector<std::shared_ptr<HwDisplayPlane>> mPresentPlanes;

    std::vector<hwc2_layer_t> mChangedLayers;
    std::vector<hwc2_layer_t> mOverlayLayers;
};

#endif/*HWC2_DISPLAY_H*/
