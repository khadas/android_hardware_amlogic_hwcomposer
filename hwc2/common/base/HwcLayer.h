/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef HWC_LAYER_H
#define HWC_LAYER_H

#include <hardware/hwcomposer2.h>
#include <utils/threads.h>
#include <Dump.h>
#include <utils/Vector.h>


namespace android {
namespace amlogic {

class HwcLayer {
    public:
        HwcLayer(hwc2_display_t& dpy);
        virtual ~HwcLayer();

        // HWC2 Layer functions
        int32_t setBuffer(buffer_handle_t buffer, int32_t acquireFence);
        int32_t setSurfaceDamage(hwc_region_t damage);

        // HWC2 Layer state functions
        int32_t setBlendMode(int32_t mode);
        int32_t setColor(hwc_color_t color);
        int32_t setDataspace(int32_t dataspace);
        int32_t setDisplayFrame(hwc_rect_t frame);
        int32_t setPlaneAlpha(float alpha);
        int32_t setSidebandStream(const native_handle_t* stream);
        int32_t setSourceCrop(hwc_frect_t crop);
        int32_t setTransform(int32_t transform);
        int32_t setVisibleRegion(hwc_region_t visible);
        int32_t setZ(uint32_t z);
        uint32_t getZ() const { return mZ; }

        int32_t setCompositionType(int32_t type);
        int32_t getCompositionType() const { return mCompositionType; }

        buffer_handle_t getBufferHandle() const { return mBufferHnd; }
        const native_handle_t* getSidebandStream() const { return mSidebandStream; }
        int32_t getAcquireFence() const { return mAcquireFence; }

        bool initialize();
        void deinitialize();
        void dump(Dump& d);

        void resetAcquireFence();

#if WITH_LIBPLAYER_MODULE
        void presentOverlay();
#endif

    private:
        hwc2_display_t mDisplayId;
        int32_t mBlendMode;
        hwc_color_t mColor;
        int32_t mCompositionType;
        int32_t mAcquireFence;
        int32_t mDataSpace;
        float mPlaneAlpha;
        int32_t mTransform;
        uint32_t mZ;
        hwc_frect_t mSourceCrop;
        hwc_rect_t mDisplayFrame;
        hwc_region_t mDamageRegion;
        hwc_region_t mVisibleRegion;


        union {
            buffer_handle_t mBufferHnd;

            /* When compositionType is HWC_SIDEBAND, this is the handle
             * of the sideband video stream to compose. */
            const native_handle_t* mSidebandStream;
        };

#if WITH_LIBPLAYER_MODULE
        // for store overlay layer's state.
        char mLastVal[32];
        char mLastAxis[32];
        char mLastMode[32];
        char mLastFreescale[32];
        char mLastWindowaxis[50];
#endif
        // lock
        Mutex mLock;
        bool mInitialized;
};

} // namespace amlogic
} // namespace android


#endif /* HWC_LAYER_H */
