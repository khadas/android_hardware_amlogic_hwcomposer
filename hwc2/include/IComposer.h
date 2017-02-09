/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef ICOMPOSER_H
#define ICOMPOSER_H

#include <HwcLayer.h>

namespace android {
namespace amlogic {

//  aml composer interface
class IComposer {
public:
    IComposer() {}
    virtual ~IComposer() {}
public:
    virtual bool initialize(framebuffer_info_t* fbInfo) = 0;
    virtual void deinitialize() = 0;
    virtual int32_t startCompose(Vector< hwc2_layer_t > hwcLayers, int32_t *offset = 0, int32_t frameCount = 0) = 0;
    virtual const char* getName() const = 0;
    // virtual void setCurGlesFbSlot(uint32_t slot) = 0;
    virtual const buffer_handle_t getBufHnd() = 0;
    virtual void mergeRetireFence(int32_t slot, int32_t retireFence) = 0;
    virtual void removeRetireFence(int32_t slot) = 0;
    virtual void setVideoOverlayLayerId(hwc2_layer_t layerId) = 0;
    virtual void fillRectangle(hwc_rect_t clipRect, uint32_t color, uint32_t offset, int shared_fd) = 0;
};

}
}

#endif /* IDISPLAY_DEVICE_H */
