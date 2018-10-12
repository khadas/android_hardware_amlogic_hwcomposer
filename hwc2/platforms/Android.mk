# Copyright (C) 2016 Amlogic
#
#

LOCAL_PATH := $(call my-dir)
#include $(TOP)/hardware/amlogic/media/media_base_config.mk
# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
ifeq ($(TARGET_COMPOSOR2.2_SUPPORT), true)
LOCAL_CFLAGS += -DHDR_SUPPORT
endif
endif

LOCAL_SRC_FILES := \
    ../common/base/HwcLayer.cpp \
    ../common/base/HwcFenceControl.cpp \
    ../common/base/Hwcomposer.cpp \
    ../common/base/HwcModule.cpp \
    ../common/base/VsyncManager.cpp \
    ../common/devices/PhysicalDevice.cpp \
    ../common/devices/framebuffer.cpp \
    ../common/devices/PrimaryDevice.cpp \
    ../common/devices/VirtualDevice.cpp \
    ../common/hdmi/DisplayHdmi.cpp \
    ../common/observers/SoftVsyncObserver.cpp \
    ../common/observers/VsyncEventObserver.cpp \
    ../common/observers/UeventObserver.cpp \
    ../common/composers/IComposeDevice.cpp \
    ../common/utils/Utils.cpp \
    ../common/utils/Dump.cpp \
    ../common/utils/AmVinfo.cpp \
    ../common/utils/AmVideo.cpp \
    ../common/utils/SysTokenizer.cpp \

LOCAL_SRC_FILES += \
    PlatFactory.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libEGL \
    libdl \
    libhardware \
    libutils \
    libsync \
    libge2d \
    libbinder \
    libsystemcontrolservice \
    libui \
    libnativewindow

# added for treble
LOCAL_SHARED_LIBRARIES += \
    vendor.amlogic.hardware.systemcontrol@1.0 \
    libbase \
    libhidlbase \
    libhidltransport

LOCAL_STATIC_LIBRARIES := \
	libomxutil

LOCAL_C_INCLUDES := \
    system/core \
    system/core/libsync \
    system/core/libsync/include \
    system/core/include \
    $(BOARD_AML_VENDOR_PATH)/system/libge2d/inlcude \
    $(BOARD_AML_VENDOR_PATH)/frameworks/services

LOCAL_C_INCLUDES += $(LOCAL_PATH) \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../common/base \
    $(LOCAL_PATH)/../common/devices \
    $(LOCAL_PATH)/../common/hdmi \
    $(LOCAL_PATH)/../common/observers \
    $(LOCAL_PATH)/../common/utils \
    $(LOCAL_PATH)/../common/composers \
    $(LOCAL_PATH)/../.. \
    $(TOP)/hardware/amlogic/media/amavutils/include \
    $(LOCAL_PATH)/ \
    $(TOP)/$(BOARD_AML_VENDOR_PATH)/frameworks/services/systemcontrol \
    frameworks/native/libs/gui/include \
    frameworks/native/libs/ui/include \
    frameworks/native/libs/nativewindow/include \
    system/libhidl/transport/token/1.0/utils/include

LOCAL_KK=0
ifeq ($(GPU_TYPE),t83x)
LOCAL_KK:=1
endif
ifeq ($(GPU_ARCH),midgard)
LOCAL_KK:=1
endif
ifeq ($(LOCAL_KK),1)
	LOCAL_CFLAGS += -DMALI_AFBC_GRALLOC=1
else
	LOCAL_CFLAGS += -DMALI_AFBC_GRALLOC=0
endif

LOCAL_CPPFLAGS += -std=c++14

MESON_GRALLOC_DIR ?= hardware/amlogic/gralloc

LOCAL_C_INCLUDES += $(MESON_GRALLOC_DIR)

LOCAL_C_INCLUDES += system/core/libion/include/ \
                system/core/libion/kernel-headers

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq ($(TARGET_APP_LAYER_USE_CONTINUOUS_BUFFER),true)
LOCAL_CFLAGS += -DUSE_CONTINOUS_BUFFER_COMPOSER
# LOCAL_CFLAGS += -DENABLE_AML_GE2D_COMPOSER
# LOCAL_SRC_FILES += ../common/composers/GE2DComposer.cpp
# LOCAL_SHARED_LIBRARIES += libion
endif
ifeq ($(TARGET_USE_SOFTWARE_CURSOR),true)
LOCAL_CFLAGS += -DENABLE_SOFT_CURSOR
endif

ifeq ($(TARGET_HEADLESS),true)
LOCAL_CFLAGS += -DHWC_HEADLESS
LOCAL_CFLAGS += -DHWC_HEADLESS_REFRESHRATE=5
endif

ifeq ($(TARGET_SUPPORT_SECURE_LAYER),true)
LOCAL_CFLAGS += -DHWC_SUPPORT_SECURE_LAYER
endif

LOCAL_CFLAGS += -Werror -Wignored-qualifiers

LOCAL_SHARED_LIBRARIES += libamavutils_alsa
#LOCAL_C_INCLUDES += $(AMAVUTILS_PATH)/include

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.amlogic
# LOCAL_CFLAGS += -DLINUX

ifneq ($(TARGET_BUILD_VARIANT),user)
   LOCAL_CFLAGS += -DHWC_TRACE_FPS
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

include $(BUILD_SHARED_LIBRARY)
