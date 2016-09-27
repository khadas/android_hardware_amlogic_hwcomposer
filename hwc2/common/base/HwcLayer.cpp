/*
// Copyright(c) 2016 Amlogic Corporation
*/

#include <inttypes.h>
#include <HwcTrace.h>
#include <HwcLayer.h>
#include <Hwcomposer.h>
#include <IDisplayDevice.h>
#include <cutils/properties.h>
#include <Utils.h>

namespace android {
namespace amlogic {

HwcLayer::HwcLayer(hwc2_display_t& dpy)
    : mDisplayId(dpy),
      mBlendMode(0),
      mCompositionType(0),
      mAcquireFence(-1),
      mDataSpace(HAL_DATASPACE_UNKNOWN),
      mPlaneAlpha(0.0f),
      mTransform(HAL_TRANSFORM_RESERVED),
      mZ(0),
      mInitialized(false)
{

}

HwcLayer::~HwcLayer()
{

}

bool HwcLayer::initialize() {
    Mutex::Autolock _l(mLock);

    mInitialized = true;
    return true;
}

void HwcLayer::deinitialize() {
    Mutex::Autolock _l(mLock);

    mInitialized = false;
}

void HwcLayer::resetAcquireFence() {
    Mutex::Autolock _l(mLock);

    mAcquireFence = -1;
}


int32_t HwcLayer::setBuffer(buffer_handle_t buffer, int32_t acquireFence) {
    Mutex::Autolock _l(mLock);

    // Bad parameter
	if (buffer && private_handle_t::validate(buffer) < 0)
		return HWC2_ERROR_BAD_PARAMETER;

    if (NULL == buffer) {
        //mBufferHnd = buffer;
        // if (-1 != acquireFence) mAcquireFence = acquireFence;
    //} else {
        DTRACE("Layer buffer is null! no need to update this layer.");
    }
    mBufferHnd = buffer;
    mAcquireFence =acquireFence ;

    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setSurfaceDamage(hwc_region_t damage) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do here.
    mDamageRegion = damage;
    return HWC2_ERROR_NONE;
}

// HWC2 Layer state functions
int32_t HwcLayer::setBlendMode(int32_t mode) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mBlendMode = mode;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setColor(hwc_color_t color) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mColor = color;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setCompositionType(int32_t type) {
    Mutex::Autolock _l(mLock);

    mCompositionType = type;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setDataspace(int32_t dataspace) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mDataSpace = dataspace;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setDisplayFrame(hwc_rect_t frame) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mDisplayFrame = frame;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setPlaneAlpha(float alpha) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mPlaneAlpha = alpha;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setSidebandStream(const native_handle_t* stream) {
    Mutex::Autolock _l(mLock);

    // Bad parameter.
    if (NULL == stream) {
        return HWC2_ERROR_BAD_PARAMETER;
    }
    mSidebandStream = stream;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setSourceCrop(hwc_frect_t crop) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mSourceCrop = crop;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setTransform(int32_t transform) {
    Mutex::Autolock _l(mLock);

    mTransform = transform;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setVisibleRegion(hwc_region_t visible) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mVisibleRegion = visible;
    return HWC2_ERROR_NONE;
}

int32_t HwcLayer::setZ(uint32_t z) {
    Mutex::Autolock _l(mLock);

    // TODO: still have some work to do.
    mZ = z;
    return HWC2_ERROR_NONE;
}

#if WITH_LIBPLAYER_MODULE
void HwcLayer::presentOverlay() {
    int32_t angle = 0;
    bool vpp_changed = false;
    bool axis_changed = false;
    bool mode_changed = false;
    bool free_scale_changed = false;
    bool window_axis_changed =false;

    if (Utils::checkBoolProp("ro.vout.dualdisplay4")) {
        vpp_changed = Utils::checkSysfsStatus(
            SYSFS_AMVIDEO_CURIDX, mLastVal, 32);
    }

    mode_changed            = Utils::checkSysfsStatus(SYSFS_DISPLAY_MODE, mLastMode, 32);
    free_scale_changed      = Utils::checkSysfsStatus(SYSFS_FB0_FREE_SCALE, mLastFreescale, 32);
    axis_changed            = Utils::checkSysfsStatus(SYSFS_VIDEO_AXIS, mLastAxis, 32);
    window_axis_changed     = Utils::checkSysfsStatus(SYSFS_WINDOW_AXIS, mLastWindowaxis, 50);

    if (!vpp_changed && !mode_changed && !axis_changed && !free_scale_changed
        && !window_axis_changed) {
        return;
    }

    switch (mTransform) {
        case 0:
            angle = 0;
        break;
        case HAL_TRANSFORM_ROT_90:
            angle = 90;
        break;
        case HAL_TRANSFORM_ROT_180:
            angle = 180;
        break;
        case HAL_TRANSFORM_ROT_270:
            angle = 270;
        break;
        default:
        return;
    }

    amvideo_utils_set_virtual_position(mDisplayFrame.left, mDisplayFrame.top,
        mDisplayFrame.right - mDisplayFrame.left,
        mDisplayFrame.bottom - mDisplayFrame.top,
        angle);

    /* the screen mode from Android framework should always be set to normal mode
    * to match the relationship between the UI and video overlay window position.
    */
    /*set screen_mode in amvideo_utils_set_virtual_position(),pls check in libplayer*/
    //amvideo_utils_set_screen_mode(0);

    memset(mLastAxis, 0, sizeof(mLastAxis));
    if (amsysfs_get_sysfs_str(SYSFS_VIDEO_AXIS, mLastAxis, sizeof(mLastAxis)) == 0) {
        DTRACE("****last video axis is: %s", mLastAxis);
    }

}
#endif

void HwcLayer::dump(Dump& d) {
    Mutex::Autolock _l(mLock);

    static char const* compositionTypeName[] = {
                    "UNKNOWN",
                    "GLES",
                    "HWC",
                    "SOLID",
                    "HWC_CURSOR",
                    "SIDEBAND"};

    d.append(
            "   %11s | %08" PRIxPTR " | %10d | %02x | %1.2f | %02x | %04x |%7.1f,%7.1f,%7.1f,%7.1f |%5d,%5d,%5d,%5d \n",
                    compositionTypeName[mCompositionType], intptr_t(mBufferHnd),
                    mZ, mDataSpace, mPlaneAlpha, mTransform, mBlendMode,
                    mSourceCrop.left, mSourceCrop.top, mSourceCrop.right, mSourceCrop.bottom,
                    mDisplayFrame.left, mDisplayFrame.top, mDisplayFrame.right, mDisplayFrame.bottom);
}


} // namespace amlogic
} // namespace android
