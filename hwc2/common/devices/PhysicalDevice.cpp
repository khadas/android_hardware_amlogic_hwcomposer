/*
// Copyright(c) 2016 Amlogic Corporation
*/
#include <HwcTrace.h>
#include <PhysicalDevice.h>
#include <Hwcomposer.h>
#include <sys/ioctl.h>
#include <sync/sync.h>
#include <Utils.h>

#include <tvp/OmxUtil.h>

static int Amvideo_Handle = 0;

namespace android {
namespace amlogic {

PhysicalDevice::PhysicalDevice(hwc2_display_t id, Hwcomposer& hwc)
    : mId(id),
      mHwc(hwc),
      mActiveDisplayConfig(-1),
      mVsyncObserver(NULL),
      mIsConnected(false),
      mFramebufferHnd(NULL),
      mFramebufferInfo(NULL),
      mPriorFrameRetireFence(-1),
      mTargetAcquireFence(-1),
      mIsValidated(false),
      mInitialized(false)
{
    CTRACE();

    switch (id) {
    case DEVICE_PRIMARY:
        mName = "Primary";
        break;
    case DEVICE_EXTERNAL:
        mName = "External";
        break;
    default:
        mName = "Unknown";
    }

    // init Display here.
    initDisplay();

    // set capacity of layers, layer's changed type, layer's changed request.
    mHwcLayersChangeType.setCapacity(LAYER_MAX_NUM_CHANGE_TYPE);
    mHwcLayersChangeRequest.setCapacity(LAYER_MAX_NUM_CHANGE_REQUEST);
    mHwcLayers.setCapacity(LAYER_MAX_NUM_SUPPORT);

    // set capacity of mDisplayConfigs
    mDisplayConfigs.setCapacity(DEVICE_COUNT);
}

PhysicalDevice::~PhysicalDevice()
{
    WARN_IF_NOT_DEINIT();
}

bool PhysicalDevice::initialize() {
    CTRACE();

    if (mId != DEVICE_PRIMARY && mId != DEVICE_EXTERNAL) {
        ETRACE("invalid device type");
        return false;
    }

    // create vsync event observer, we only have soft vsync now...
    mVsyncObserver = new SoftVsyncObserver(*this);
    if (!mVsyncObserver || !mVsyncObserver->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync observer");
    }

    mInitialized = true;
    return true;
}

void PhysicalDevice::deinitialize() {
    Mutex::Autolock _l(mLock);

    DEINIT_AND_DELETE_OBJ(mVsyncObserver);

    mInitialized = false;
}

HwcLayer* PhysicalDevice::getLayerById(hwc2_layer_t layerId) {
    HwcLayer* layer = NULL;

    layer = mHwcLayers.valueFor(layerId);
    if (!layer) ETRACE("getLayerById %lld error!", layerId);

    return layer;
}

int32_t PhysicalDevice::acceptDisplayChanges() {
    HwcLayer* layer = NULL;

    for (uint32_t i=0; i<mHwcLayersChangeType.size(); i++) {
        hwc2_layer_t layerId = mHwcLayersChangeType.keyAt(i);
        layer = mHwcLayersChangeType.valueAt(i);
        if (layer) {
            if (layer->getCompositionType() == HWC2_COMPOSITION_DEVICE
                || layer->getCompositionType() == HWC2_COMPOSITION_SOLID_COLOR) {
                layer->setCompositionType(HWC2_COMPOSITION_CLIENT);
            } else if (layer->getCompositionType() == HWC2_COMPOSITION_SIDEBAND) {
                layer->setCompositionType(HWC2_COMPOSITION_DEVICE);
            }
        }
    }
    // reset layer changed or requested to zero.
    mHwcLayersChangeType.clear();
    mHwcLayersChangeRequest.clear();

    return HWC2_ERROR_NONE;
}

bool PhysicalDevice::createLayer(hwc2_layer_t* outLayer) {
    HwcLayer* layer = new HwcLayer(mId);

    if (layer == NULL || !layer->initialize()) {
        ETRACE("createLayer: failed !");
        return false;
    }

    hwc2_layer_t layerId = reinterpret_cast<hwc2_layer_t>(layer);
    mHwcLayers.add(layerId, layer);
    *outLayer = layerId;

    return true;
}

bool PhysicalDevice::destroyLayer(hwc2_layer_t layerId) {
    HwcLayer* layer = mHwcLayers.valueFor(layerId);

    if (layer == NULL) {
        ETRACE("destroyLayer: no Hwclayer found (%d)", layerId);
        return false;
    }

    mHwcLayers.removeItem(layerId);
    DEINIT_AND_DELETE_OBJ(layer);
    return true;
}

int32_t PhysicalDevice::getActiveConfig(
    hwc2_config_t* outConfig) {

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getChangedCompositionTypes(
    uint32_t* outNumElements,
    hwc2_layer_t* outLayers,
    int32_t* /*hwc2_composition_t*/ outTypes) {
    HwcLayer* layer = NULL;

    // if outLayers or outTypes were NULL, the number of layers and types which would have been returned.
    if (NULL == outLayers || NULL == outTypes) {
        *outNumElements = mHwcLayersChangeType.size();
    } else {
        for (uint32_t i=0; i<mHwcLayersChangeType.size(); i++) {
            hwc2_layer_t layerId = mHwcLayersChangeType.keyAt(i);
            layer = mHwcLayersChangeType.valueAt(i);
            if (layer) {
                if (layer->getCompositionType() == HWC2_COMPOSITION_DEVICE
                    || layer->getCompositionType() == HWC2_COMPOSITION_SOLID_COLOR) {
                    // change all other device type to client.
                    outLayers[i] = layerId;
                    outTypes[i] = HWC2_COMPOSITION_CLIENT;
                    continue;
                }

                // sideband stream.
                if (layer->getCompositionType() == HWC2_COMPOSITION_SIDEBAND
                    && layer->getSidebandStream()) {
                    // TODO: we just transact SIDEBAND to OVERLAY for now;
                    DTRACE("get HWC_SIDEBAND layer, just change to overlay");
                    outLayers[i] = layerId;
                    outTypes[i] = HWC2_COMPOSITION_DEVICE;
                    continue;
                }
            }
        }

        if (mHwcLayersChangeType.size() > 0) {
            DTRACE("There are %d layers type has changed.", mHwcLayersChangeType.size());
            *outNumElements = mHwcLayersChangeType.size();
        } else {
            DTRACE("No layers compositon type changed.");
        }
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getClientTargetSupport(
    uint32_t width,
    uint32_t height,
    int32_t /*android_pixel_format_t*/ format,
    int32_t /*android_dataspace_t*/ dataspace) {

    if (width == mFramebufferInfo->info.xres
        && height == mFramebufferInfo->info.yres
        && format == HAL_PIXEL_FORMAT_RGBA_8888
        && dataspace == HAL_DATASPACE_UNKNOWN) {
        return HWC2_ERROR_NONE;
    }

    DTRACE("fbinfo: [%d x %d], client: [%d x %d]"
        "format: %d, dataspace: %d",
        mFramebufferInfo->info.xres,
        mFramebufferInfo->info.yres,
        width, height, format, dataspace);

    // TODO: ?
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t PhysicalDevice::getColorModes(
    uint32_t* outNumModes,
    int32_t* /*android_color_mode_t*/ outModes) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDisplayAttribute(
        hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute,
        int32_t* outValue) {
    Mutex::Autolock _l(mLock);

    if (!mIsConnected) {
        ETRACE("display %d is not connected.", mId);
    }

    DisplayConfig *configChosen = mDisplayConfigs.itemAt(config);
    if  (!configChosen) {
        ETRACE("failed to get display config: %ld", config);
        return HWC2_ERROR_BAD_CONFIG;
    }

    // TODO: HWC2_ERROR_BAD_CONFIG?
    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            //*outValue = (int32_t)mVsyncObserver->getRefreshPeriod();
            if (configChosen->getRefreshRate()) {
                *outValue = 1e9 / configChosen->getRefreshRate();
            } else {
                ETRACE("refresh rate is 0, default to 60fps!!!");
                *outValue = 1e9 / 60;
            }

            ETRACE("refresh period: %d", *outValue);
        break;
        case HWC2_ATTRIBUTE_WIDTH:
            //*outValue = mFramebufferInfo->info.xres;
            *outValue = configChosen->getWidth();
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
            //*outValue = mFramebufferInfo->info.yres;
            *outValue = configChosen->getHeight();
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            //*outValue = mFramebufferInfo->xdpi*1000;
            *outValue = configChosen->getDpiX() * 1000.0f;
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            //*outValue = mFramebufferInfo->ydpi*1000;
            *outValue = configChosen->getDpiY() * 1000.0f;
        break;
        default:
            ETRACE("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDisplayConfigs(
        uint32_t* outNumConfigs,
        hwc2_config_t* outConfigs) {
    Mutex::Autolock _l(mLock);

    if (!mIsConnected) {
        ETRACE("display %d is not connected.", mId);
    }

    /* if (NULL != outConfigs) outConfigs[0] = 0;
    *outNumConfigs = 1; */

    // fill in all config handles
    if (NULL != outConfigs) {
        for (int i = 0; i < static_cast<int>(*outNumConfigs); i++) {
            outConfigs[i] = i;
        }
    }
    *outNumConfigs = mDisplayConfigs.size();

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDisplayName(
    uint32_t* outSize,
    char* outName) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDisplayRequests(
    int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
    uint32_t* outNumElements,
    hwc2_layer_t* outLayers,
    int32_t* /*hwc2_layer_request_t*/ outLayerRequests) {

    // if outLayers or outTypes were NULL, the number of layers and types which would have been returned.
    if (NULL == outLayers || NULL == outLayerRequests) {
        *outNumElements = mHwcLayersChangeRequest.size();
    } else {
        for (uint32_t i=0; i<mHwcLayersChangeRequest.size(); i++) {
            hwc2_layer_t layerId = mHwcLayersChangeRequest.keyAt(i);
            HwcLayer *layer = mHwcLayersChangeRequest.valueAt(i);
            if (layer->getCompositionType() == HWC2_COMPOSITION_DEVICE) {
                // video overlay.
                if (layer->getBufferHandle()) {
                    private_handle_t const* hnd =
                        reinterpret_cast<private_handle_t const*>(layer->getBufferHandle());
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY) {
                        outLayers[i] = layerId;
                        outLayerRequests[i] = HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET;
                        continue;
                    }
                }
            }

            // sideband stream.
            if ((layer->getCompositionType() == HWC2_COMPOSITION_SIDEBAND && layer->getSidebandStream())
                //|| layer->getCompositionType() == HWC2_COMPOSITION_SOLID_COLOR
                || layer->getCompositionType() == HWC2_COMPOSITION_CURSOR) {
                // TODO: we just transact SIDEBAND to OVERLAY for now;
                DTRACE("get HWC_SIDEBAND layer, just change to overlay");
                outLayers[i] = layerId;
                outLayerRequests[i] = HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET;
                continue;
            }
        }

        if (mHwcLayersChangeRequest.size() > 0) {
            DTRACE("There are %d layer requests.", mHwcLayersChangeRequest.size());
            *outNumElements = mHwcLayersChangeRequest.size();
        } else {
            DTRACE("No layer requests.");
        }
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDisplayType(
    int32_t* /*hwc2_display_type_t*/ outType) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getDozeSupport(
    int32_t* outSupport) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getHdrCapabilities(
        uint32_t* outNumTypes,
        int32_t* /*android_hdr_t*/ outTypes,
        float* outMaxLuminance,
        float* outMaxAverageLuminance,
        float* outMinLuminance) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::getReleaseFences(
        uint32_t* outNumElements,
        hwc2_layer_t* outLayers,
        int32_t* outFences) {
    HwcLayer* layer = NULL;
    uint32_t num_layer = 0;

    if (NULL == outLayers || NULL == outFences) {
        for (uint32_t i=0; i<mHwcLayers.size(); i++) {
            hwc2_layer_t layerId = mHwcLayers.keyAt(i);
            layer = mHwcLayers.valueAt(i);
            if (layer) num_layer++;
        }
    } else {
        for (uint32_t i=0; i<mHwcLayers.size(); i++) {
            hwc2_layer_t layerId = mHwcLayers.keyAt(i);
            layer = mHwcLayers.valueAt(i);
            if (layer) {
                DTRACE("outFences: %d", layer->getAcquireFence());
                /*if (layer->getAcquireFence() > -1) {
                    close(layer->getAcquireFence());
                }*/
                if (layer->getAcquireFence() > -1) {
                    outFences[num_layer] = layer->getAcquireFence();
                } else {
                    outFences[num_layer] = -1;
                }
                outLayers[num_layer++] = layerId;
                layer->resetAcquireFence();
                // TODO: ?
            }
        }
    }

    if (num_layer > 0) {
        DTRACE("There are %d layer requests.", num_layer);
        *outNumElements = num_layer;
    } else {
        DTRACE("No layer have set buffer yet.");
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::postFramebuffer(int32_t* outRetireFence) {
    HwcLayer* layer = NULL;
    int32_t err = 0;
    void *cbuffer;

    // deal physical display's client target layer
    framebuffer_info_t* cbinfo = mCursorContext->getCursorInfo();
    bool cursorShow = false;
    for (uint32_t i=0; i<mHwcLayers.size(); i++) {
        hwc2_layer_t layerId = mHwcLayers.keyAt(i);
        layer = mHwcLayers.valueAt(i);
        if (layer && layer->getCompositionType()== HWC2_COMPOSITION_CURSOR) {
            if (private_handle_t::validate(layer->getBufferHandle()) < 0) {
                ETRACE("invalid cursor layer handle.");
                break;
            }
            private_handle_t *hnd = (private_handle_t *)(layer->getBufferHandle());
            DTRACE("This is a Sprite, hnd->stride is %d, hnd->height is %d", hnd->stride, hnd->height);
            if (cbinfo->info.xres != (uint32_t)hnd->stride || cbinfo->info.yres != (uint32_t)hnd->height) {
                ETRACE("disp: %d cursor need to redrew", mId);
                update_cursor_buffer_locked(cbinfo, hnd->stride, hnd->height);
                cbuffer = mmap(NULL, hnd->size, PROT_READ|PROT_WRITE, MAP_SHARED, cbinfo->fd, 0);
                if (cbuffer != MAP_FAILED) {
                    memcpy(cbuffer, hnd->base, hnd->size);
                    munmap(cbuffer, hnd->size);
                    DTRACE("setCursor ok");
                } else {
                    ETRACE("buffer mmap fail");
                }
            }
            cursorShow = true;
            break;
        }
    }

    if (!mClientTargetHnd || private_handle_t::validate(mClientTargetHnd) < 0 || mPowerMode == HWC2_POWER_MODE_OFF) {
        DTRACE("mClientTargetHnd is null or Enter suspend state, mTargetAcquireFence: %d", mTargetAcquireFence);
        if (mTargetAcquireFence > -1) {
            sync_wait(mTargetAcquireFence, 5000);
            close(mTargetAcquireFence);
            mTargetAcquireFence = -1;
        }
        *outRetireFence = -1;
        if (private_handle_t::validate(mClientTargetHnd) < 0) {
            ETRACE("mClientTargetHnd is not validate!");
            return HWC2_ERROR_NONE;
        }
    }

    *outRetireFence = mPriorFrameRetireFence;

    if (*outRetireFence >= 0) {
        DTRACE("Get prior frame's retire fence %d", *outRetireFence);
    } else {
        ETRACE("No valid prior frame's retire returned. %d ", *outRetireFence);
        // -1 means no fence, less than -1 is some error
        *outRetireFence = -1;
    }

    mPriorFrameRetireFence = fb_post_with_fence_locked(mFramebufferInfo, mClientTargetHnd, mTargetAcquireFence);
    mTargetAcquireFence = -1;

    // finally we need to update cursor's blank status
    if (cbinfo->fd > 0 && cursorShow != mCursorContext->getCursorStatus()) {
        mCursorContext->setCursorStatus(cursorShow);
        DTRACE("UPDATE FB1 status to %d", !cursorShow);
        ioctl(cbinfo->fd, FBIOBLANK, !cursorShow);
    }

    return err;
}


int32_t PhysicalDevice::presentDisplay(
        int32_t* outRetireFence) {
    int32_t err = HWC2_ERROR_NONE;
    HwcLayer* layer = NULL;

    if (mIsValidated) {
        // TODO: need improve the way to set video axis.
#if WITH_LIBPLAYER_MODULE
        for (uint32_t i=0; i<mHwcLayers.size(); i++) {
            hwc2_layer_t layerId = mHwcLayers.keyAt(i);
            layer = mHwcLayers.valueAt(i);

            if (layer && layer->getCompositionType()== HWC2_COMPOSITION_DEVICE) {
                layer->presentOverlay();
                break;
            }
        }
#endif
        err = postFramebuffer(outRetireFence);
        mIsValidated = false;
    } else { // display not validate yet.
        err = HWC2_ERROR_NOT_VALIDATED;
    }

    return err;
}

int32_t PhysicalDevice::setActiveConfig(
    hwc2_config_t config) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setClientTarget(
        buffer_handle_t target,
        int32_t acquireFence,
        int32_t /*android_dataspace_t*/ dataspace,
        hwc_region_t damage) {

	if (target && private_handle_t::validate(target) < 0) {
		return HWC2_ERROR_BAD_PARAMETER;
	}

    if (NULL != target) {
        mClientTargetHnd = target;
        mClientTargetDamageRegion = damage;
        if (-1 != acquireFence) {
            mTargetAcquireFence = acquireFence;
            //sync_wait(mTargetAcquireFence, 3000);
        }
        // TODO: HWC2_ERROR_BAD_PARAMETER && dataspace && damage.
    } else {
        DTRACE("client target is null!, no need to update this frame.");
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setColorMode(
    int32_t /*android_color_mode_t*/ mode) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setColorTransform(
    const float* matrix,
    int32_t /*android_color_transform_t*/ hint) {
    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setPowerMode(
    int32_t /*hwc2_power_mode_t*/ mode){

    mPowerMode = mode;
    return HWC2_ERROR_NONE;
}

bool PhysicalDevice::vsyncControl(bool enabled) {
    RETURN_FALSE_IF_NOT_INIT();

    ATRACE("disp = %d, enabled = %d", mId, enabled);
    return mVsyncObserver->control(enabled);
}

int32_t PhysicalDevice::validateDisplay(uint32_t* outNumTypes,
    uint32_t* outNumRequests) {
    HwcLayer* layer = NULL;
    bool istvp = false;

    for (uint32_t i=0; i<mHwcLayers.size(); i++) {
        hwc2_layer_t layerId = mHwcLayers.keyAt(i);
        layer = mHwcLayers.valueAt(i);
        if (layer) {
            // Physical Display.
            if (layer->getCompositionType() == HWC2_COMPOSITION_DEVICE) {
                // video overlay.
                if (layer->getBufferHandle()) {
                    private_handle_t const* hnd =
                        reinterpret_cast<private_handle_t const*>(layer->getBufferHandle());
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OMX) {
                        set_omx_pts((char*)hnd->base, &Amvideo_Handle);
                        istvp = true;
                    }
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY) {
                        mHwcLayersChangeRequest.add(layerId, layer);
                        continue;
                    }
                }

                // change all other device type to client.
                mHwcLayersChangeType.add(layerId, layer);
                continue;
            }

            // cursor layer.
            if (layer->getCompositionType() == HWC2_COMPOSITION_CURSOR) {
                DTRACE("This is a Cursor layer!");
                mHwcLayersChangeRequest.add(layerId, layer);
                continue;
            }

            // sideband stream.
            if (layer->getCompositionType() == HWC2_COMPOSITION_SIDEBAND
                && layer->getSidebandStream()) {
                // TODO: we just transact SIDEBAND to OVERLAY for now;
                DTRACE("get HWC_SIDEBAND layer, just change to overlay");
                mHwcLayersChangeRequest.add(layerId, layer);
                mHwcLayersChangeType.add(layerId, layer);
                continue;
            }

            // TODO: solid color.
            if (layer->getCompositionType() == HWC2_COMPOSITION_SOLID_COLOR) {
                DTRACE("This is a Solid Color layer!");
                //mHwcLayersChangeRequest.add(layerId, layer);
                mHwcLayersChangeType.add(layerId, layer);
                continue;
            }
        }
    }

    if (istvp == false && Amvideo_Handle!=0) {
        closeamvideo();
        Amvideo_Handle = 0;
    }

    if (mHwcLayersChangeRequest.size() > 0) {
        DTRACE("There are %d layer requests.", mHwcLayersChangeRequest.size());
        *outNumRequests = mHwcLayersChangeRequest.size();
    }

    // mark the validate function is called.(???)
    mIsValidated = true;
    if (mHwcLayersChangeType.size() > 0) {
        DTRACE("there are %d layer types has changed.", mHwcLayersChangeType.size());
        *outNumTypes = mHwcLayersChangeType.size();
        return HWC2_ERROR_HAS_CHANGES;
    }

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setCursorPosition(hwc2_layer_t layerId, int32_t x, int32_t y) {
    HwcLayer* layer = getLayerById(layerId);
    if (layer && HWC2_COMPOSITION_CURSOR == layer->getCompositionType()) {
        framebuffer_info_t* cbinfo = mCursorContext->getCursorInfo();
        fb_cursor cinfo;
        if (cbinfo->fd < 0) {
            ETRACE("setCursorPosition fd=%d", cbinfo->fd );
        }else {
            cinfo.hot.x = x;
            cinfo.hot.y = y;
            DTRACE("setCursorPosition x_pos=%d, y_pos=%d", cinfo.hot.x, cinfo.hot.y);
            ioctl(cbinfo->fd, FBIO_CURSOR, &cinfo);
        }
    } else {
        ETRACE("setCursorPosition bad layer.");
        return HWC2_ERROR_BAD_LAYER;
    }

    return HWC2_ERROR_NONE;
}


/*
Operater of framebuffer
*/
int32_t PhysicalDevice::initDisplay() {
    if (mIsConnected) return 0;

    Mutex::Autolock _l(mLock);

    if (!mFramebufferHnd) {
        mFramebufferInfo = new framebuffer_info_t();
        //init information from osd.
        mFramebufferInfo->displayType = mId;
        mFramebufferInfo->fbIdx = getOsdIdx(mId);
        int32_t err = init_frame_buffer_locked(mFramebufferInfo);
        int32_t bufferSize = mFramebufferInfo->finfo.line_length
            * mFramebufferInfo->info.yres;
        DTRACE("init_frame_buffer get fbinfo->fbIdx (%d) "
            "fbinfo->info.xres (%d) fbinfo->info.yres (%d)",
            mFramebufferInfo->fbIdx, mFramebufferInfo->info.xres,
            mFramebufferInfo->info.yres);
        int32_t usage = 0;
        private_module_t *grallocModule = Hwcomposer::getInstance().getGrallocModule();
        if (mId == HWC_DISPLAY_PRIMARY) {
            grallocModule->fb_primary.fb_info = *(mFramebufferInfo);
        } else if (mId == HWC_DISPLAY_EXTERNAL) {
            grallocModule->fb_external.fb_info = *(mFramebufferInfo);
            usage |= GRALLOC_USAGE_EXTERNAL_DISP;
        }

        //Register the framebuffer to gralloc module
        mFramebufferHnd = new private_handle_t(
                        private_handle_t::PRIV_FLAGS_FRAMEBUFFER,
                        usage, mFramebufferInfo->fbSize, 0,
                        0, mFramebufferInfo->fd, bufferSize, 0);
        grallocModule->base.registerBuffer(&(grallocModule->base), mFramebufferHnd);
        DTRACE("init_frame_buffer get frame size %d usage %d",
            bufferSize,usage);
    }

    mIsConnected = true;

    // init cursor framebuffer
    mCursorContext = new CursorContext();
    framebuffer_info_t* cbinfo = mCursorContext->getCursorInfo();
    cbinfo->fd = -1;

    //init information from cursor framebuffer.
    cbinfo->fbIdx = mId*2+1;
    if (1 != cbinfo->fbIdx && 3 != cbinfo->fbIdx) {
        ETRACE("invalid fb index: %d, need to check!",
            cbinfo->fbIdx);
        return 0;
    }
    int32_t err = init_cursor_buffer_locked(cbinfo);
    if (err != 0) {
        ETRACE("init_cursor_buffer_locked failed, need to check!");
        return 0;
    }
    ITRACE("init_cursor_buffer get cbinfo->fbIdx (%d) "
        "cbinfo->info.xres (%d) cbinfo->info.yres (%d)",
                        cbinfo->fbIdx,
                        cbinfo->info.xres,
                        cbinfo->info.yres);

    if ( cbinfo->fd >= 0) {
        DTRACE("init_cursor_buffer success!");
    }else{
        DTRACE("init_cursor_buffer fail!");
    }

    return 0;
}

void PhysicalDevice::removeDisplayConfigs()
{
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        delete config;
    }

    mDisplayConfigs.clear();
    mActiveDisplayConfig = -1;
}

bool PhysicalDevice::updateDisplayConfigs()
{
    Mutex::Autolock _l(mLock);

    bool ret;
    int32_t rate;

    if (!mIsConnected) {
        ETRACE("disp: %llu is not connected", mId);
        return true;
    }

    ret = Utils::checkOutputMode(mDisplayMode, &rate);
    if (ret) {
        mVsyncObserver->setRefreshRate(rate);
    }
    ETRACE("output mode refresh rate: %d", rate);

    ret |= Utils::checkVinfo(mFramebufferInfo);

    if (ret) {
        // reset display configs
        removeDisplayConfigs();
        // reset the number of display configs
        mDisplayConfigs.setCapacity(1);

        // use active fb dimension as config width/height
        DisplayConfig *config = new DisplayConfig(rate,
                                          mFramebufferInfo->info.xres,
                                          mFramebufferInfo->info.yres,
                                          mFramebufferInfo->xdpi,
                                          mFramebufferInfo->ydpi);
        // add it to the front of other configs
        mDisplayConfigs.push_front(config);

        // init the active display config
        mActiveDisplayConfig = 0;
    }
    return true;
}

void PhysicalDevice::onVsync(int64_t timestamp) {
    RETURN_VOID_IF_NOT_INIT();
    ATRACE("timestamp = %lld", timestamp);

    if (!mIsConnected)
        return;

    // notify hwc
    mHwc.vsync(mId, timestamp);
}

int32_t PhysicalDevice::createVirtualDisplay(
        uint32_t width,
        uint32_t height,
        int32_t* /*android_pixel_format_t*/ format,
        hwc2_display_t* outDisplay) {

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::destroyVirtualDisplay(
        hwc2_display_t display) {

    return HWC2_ERROR_NONE;
}

int32_t PhysicalDevice::setOutputBuffer(
        buffer_handle_t buffer, int32_t releaseFence) {
    // Virtual Display Only.
    return HWC2_ERROR_NONE;
}

void PhysicalDevice::dump(Dump& d) {
    Mutex::Autolock _l(mLock);
    d.append("-------------------------------------------------------------"
        "------------------------------------------------------------\n");
    d.append("Device Name: %s (%s)\n", mName,
            mIsConnected ? "connected" : "disconnected");
    d.append("   CONFIG   |   VSYNC_PERIOD   |   WIDTH   |   HEIGHT   |"
        "   DPI_X   |   DPI_Y   \n");
    d.append("------------+------------------+-----------+------------+"
        "-----------+-----------\n");
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        if (config) {
            d.append("%s %2d     |       %4d       |   %5d   |    %4d    |"
                "    %3d    |    %3d    \n",
                     (i == (size_t)mActiveDisplayConfig) ? "*   " : "    ",
                     i,
                     config->getRefreshRate(),
                     config->getWidth(),
                     config->getHeight(),
                     config->getDpiX(),
                     config->getDpiY());
        }
    }

    // dump layer list
    d.append("  Layers state:\n");
    d.append("    numLayers=%zu\n", mHwcLayers.size());
    d.append("    numChangedTypeLayers=%zu\n", mHwcLayersChangeType.size());
    d.append("    numChangedRequestLayers=%zu\n", mHwcLayersChangeRequest.size());

    if (mHwcLayers.size() > 0) {
        d.append(
            "       type    |  handle  |   zorder   | ds | alpa | tr | blnd |"
            "     source crop (l,t,r,b)      |          frame         \n"
            "  -------------+----------+------------+----+------+----+------+"
            "--------------------------------+------------------------\n");
        for (uint32_t i=0; i<mHwcLayers.size(); i++) {
            hwc2_layer_t layerId = mHwcLayers.keyAt(i);
            HwcLayer *layer = mHwcLayers.valueAt(i);
            if (layer) layer->dump(d);
        }
    }
}

} // namespace amlogic
} // namespace android
