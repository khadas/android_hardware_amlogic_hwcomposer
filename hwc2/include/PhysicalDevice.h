/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef PHYSICAL_DEVICE_H
#define PHYSICAL_DEVICE_H

#include <utils/KeyedVector.h>
#include <SoftVsyncObserver.h>
#include <IDisplayDevice.h>
#include <HwcLayer.h>

namespace android {
namespace amlogic {

class IHdcpControl;

class DeviceControlFactory {
public:
    virtual ~DeviceControlFactory(){}
public:
    virtual IHdcpControl* createHdcpControl() = 0;
};

class CursorContext {
public:
    CursorContext()
    : mStatus(false)
    {
        mCursorInfo = new framebuffer_info_t();
    }
    virtual ~CursorContext(){}

    virtual framebuffer_info_t* getCursorInfo() { return mCursorInfo; }
    virtual bool getCursorStatus() { return mStatus; }
    virtual void setCursorStatus(bool status) { mStatus = status; }
private:
    framebuffer_info_t *mCursorInfo;
    bool mStatus;
};

class Hwcomposer;
class SoftVsyncObserver;

class PhysicalDevice : public IDisplayDevice {
    enum {
        LAYER_MAX_NUM_CHANGE_REQUEST = 8,
        LAYER_MAX_NUM_CHANGE_TYPE = 16,
        LAYER_MAX_NUM_SUPPORT = LAYER_MAX_NUM_CHANGE_TYPE,
    };
public:
    PhysicalDevice(hwc2_display_t id, Hwcomposer& hwc);
    ~PhysicalDevice();

    friend class Hwcomposer;
    friend class HwcLayer;

    // Required by HWC2
    virtual int32_t acceptDisplayChanges();
    virtual bool createLayer(hwc2_layer_t* outLayer);
    virtual bool destroyLayer(hwc2_layer_t layer);
    virtual int32_t getActiveConfig(hwc2_config_t* outConfig);
    virtual int32_t getChangedCompositionTypes(uint32_t* outNumElements, hwc2_layer_t* outLayers,
        int32_t* /*hwc2_composition_t*/ outTypes);
    virtual int32_t getClientTargetSupport(uint32_t width,
        uint32_t height, int32_t /*android_pixel_format_t*/ format,
        int32_t /*android_dataspace_t*/ dataspace);
    virtual int32_t getColorModes(uint32_t* outNumModes,
        int32_t* /*android_color_mode_t*/ outModes);
    virtual int32_t getDisplayAttribute(hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute, int32_t* outValue);
    virtual int32_t getDisplayConfigs(uint32_t* outNumConfigs, hwc2_config_t* outConfigs);
    virtual int32_t getDisplayName(uint32_t* outSize,char* outName);
    virtual int32_t getDisplayRequests(int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
        uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* /*hwc2_layer_request_t*/ outLayerRequests);
    virtual int32_t getDisplayType(int32_t* /*hwc2_display_type_t*/ outType);
    virtual int32_t getDozeSupport(int32_t* outSupport);
    virtual int32_t getHdrCapabilities(uint32_t* outNumTypes,
        int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
        float* outMaxAverageLuminance, float* outMinLuminance);
    virtual int32_t getReleaseFences(uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* outFences);
    virtual int32_t presentDisplay(int32_t* outRetireFence);
    virtual int32_t setActiveConfig(hwc2_config_t config);
    virtual int32_t setClientTarget(buffer_handle_t target,
        int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace, hwc_region_t damage);
    virtual int32_t setColorMode(int32_t /*android_color_mode_t*/ mode);
    virtual int32_t setColorTransform(const float* matrix, int32_t /*android_color_transform_t*/ hint);

    //virtual int32_t setOutputBuffer(buffer_handle_t buffer, int32_t releaseFence); // virtual display only

    virtual int32_t setPowerMode(int32_t /*hwc2_power_mode_t*/ mode);
    virtual bool vsyncControl(bool enabled); // virtual hwc2_error_t setVsyncEnabled(hwc2_display_t display, int32_t /*hwc2_vsync_t*/ enabled);
    virtual int32_t validateDisplay(uint32_t* outNumTypes, uint32_t* outNumRequests);
    virtual int32_t setCursorPosition(hwc2_layer_t layerId, int32_t x, int32_t y);

    virtual int32_t createVirtualDisplay(uint32_t width, uint32_t height, int32_t* format, hwc2_display_t* outDisplay);
    virtual int32_t destroyVirtualDisplay(hwc2_display_t display);
    virtual int32_t setOutputBuffer(buffer_handle_t buffer, int32_t releaseFence);

    // Other Display methods
    virtual Hwcomposer& getDevice() const { return mHwc; }
    virtual hwc2_display_t getId() const { return mId; }
    virtual bool isConnected() const { return mIsConnected; }

    // device related operations
    virtual bool initCheck() const { return mInitialized; }
    virtual bool initialize();
    virtual void deinitialize();
    virtual const char* getName() const { return mName; };
    virtual int getDisplayId() const { return mId; };
    virtual HwcLayer* getLayerById(hwc2_layer_t layerId);

    // display config operations
    virtual void removeDisplayConfigs();
    virtual bool updateDisplayConfigs();

    //events
    virtual void onVsync(int64_t timestamp);
    virtual void dump(Dump& d);

private:
    // For use by Device
    int32_t initDisplay();
    int32_t postFramebuffer(int32_t* outRetireFence);

    // Member variables
    hwc2_display_t mId;
    const char *mName;
    bool mIsConnected;
    Hwcomposer& mHwc;
    char mDisplayMode[32];

    // display configs
    Vector<DisplayConfig*> mDisplayConfigs;
    int mActiveDisplayConfig;

    SoftVsyncObserver *mVsyncObserver;

    // DeviceControlFactory *mControlFactory;

    framebuffer_info_t *mFramebufferInfo;
    private_handle_t  *mFramebufferHnd;
    CursorContext *mCursorContext;

    int32_t /*android_color_mode_t*/ mColorMode;

    // client target layer.
    buffer_handle_t mClientTargetHnd;
    hwc_region_t mClientTargetDamageRegion;
    int32_t mTargetAcquireFence;

    // num of composition type changed layer.
    uint32_t mNumLayersChangetype;
    uint32_t mNumLayerChangerequest;

    // layer
    KeyedVector<hwc2_layer_t, HwcLayer*> mHwcLayersChangeType;
    KeyedVector<hwc2_layer_t, HwcLayer*> mHwcLayersChangeRequest;
    KeyedVector<hwc2_layer_t, HwcLayer*> mHwcLayers;

    // lock
    Mutex mLock;
    bool mInitialized;
};


}
}

#endif /* PHYSICAL_DEVICE_H */
