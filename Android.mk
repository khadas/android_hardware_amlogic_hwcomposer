# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/tvp/Android.mk

include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CPPFLAGS += -std=c++14
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq ($(TARGET_BUILD_VARIANT), user)
LOCAL_CFLAGS += -DHWC_RELEASE=1
endif

#*********************************HWC CONFIGS************************
#HWC API Version Config
ifeq ($(USE_HWC1), true)
LOCAL_CFLAGS += -DENABLE_MESON_HWC1
else ifeq ($(USE_HWC2), true)
LOCAL_CFLAGS += -DENABLE_MESON_HWC2

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_CFLAGS += -DHWC_HDR_METADATA_SUPPORT
endif

else
$(error "need config hwc api version")
endif

#HWC DISPLAY Config
ifneq ($(HWC_DISPLAY_NUM),)
    LOCAL_CFLAGS += -DHWC_DISPLAY_NUM=$(HWC_DISPLAY_NUM)
else
$(error "need config hwc crtc num")
endif

#FRAMEBUFFER CONFIG
#Primary
ifneq ($(HWC_PRIMARY_FRAMEBUFFER_WIDTH)$(HWC_PRIMARY_FRAMEBUFFER_HEIGHT),)
    LOCAL_CFLAGS += -DHWC_PRIMARY_FRAMEBUFFER_WIDTH=$(HWC_PRIMARY_FRAMEBUFFER_WIDTH)
    LOCAL_CFLAGS += -DHWC_PRIMARY_FRAMEBUFFER_HEIGHT=$(HWC_PRIMARY_FRAMEBUFFER_HEIGHT)
endif
#Extend, if needed.
ifneq ($(HWC_EXTEND_FRAMEBUFFER_WIDTH)$(HWC_EXTEND_FRAMEBUFFER_HEIGHT),)
    LOCAL_CFLAGS += -DHWC_EXTEND_FRAMEBUFFER_WIDTH=$(HWC_EXTEND_FRAMEBUFFER_WIDTH)
    LOCAL_CFLAGS += -DHWC_EXTEND_FRAMEBUFFER_HEIGHT=$(HWC_EXTEND_FRAMEBUFFER_HEIGHT)
endif

#CONNECTOR
#Primary
ifneq ($(HWC_PRIMARY_CONNECTOR_TYPE),)
    LOCAL_CFLAGS += -DHWC_PRIMARY_CONNECTOR_TYPE=\"$(HWC_PRIMARY_CONNECTOR_TYPE)\"
else
$(error "need config hwc primary connector type")
endif
#Extend, if needed.
ifneq ($(HWC_EXTEND_CONNECTOR_TYPE),)
    LOCAL_CFLAGS += -DHWC_EXTEND_CONNECTOR_TYPE=\"$(HWC_EXTEND_CONNECTOR_TYPE)\"
endif

#HEADLESS MODE
ifeq ($(HWC_ENABLE_HEADLESS_MODE), true)
LOCAL_CFLAGS += -DHWC_ENABLE_HEADLESS_MODE
LOCAL_CFLAGS += -DHWC_HEADLESS_REFRESHRATE=5
endif

#Active Mode
ifeq ($(HWC_ENABLE_ACTIVE_MODE), true)
LOCAL_CFLAGS += -DHWC_ENABLE_ACTIVE_MODE
endif
ifeq ($(HWC_ENABLE_FRACTIONAL_REFRESH_RATE), true)
LOCAL_CFLAGS += -DENABLE_FRACTIONAL_REFRESH_RATE
endif

#Display Calibrate
ifeq ($(HWC_ENABLE_PRE_DISPLAY_CALIBRATE), true)
#pre display calibrate means calibrate in surfacefligner,
#all the coordinates got by hwc already calibrated.
LOCAL_CFLAGS += -DHWC_ENABLE_PRE_DISPLAY_CALIBRATE
endif

#HWC Feature Config
ifeq ($(HWC_ENABLE_SOFTWARE_VSYNC), true)
LOCAL_CFLAGS += -DHWC_ENABLE_SOFTWARE_VSYNC
endif
ifeq ($(ENABLE_PRIMARY_DISPLAY_HOTPLUG), true) #Used for NTS test
HWC_ENABLE_PRIMARY_HOTPLUG := true
endif
ifeq ($(HWC_ENABLE_PRIMARY_HOTPLUG), true) #need surfaceflinger modifications
LOCAL_CFLAGS += -DHWC_ENABLE_PRIMARY_HOTPLUG
endif
ifeq ($(HWC_ENABLE_SECURE_LAYER_PROCESS), true)
LOCAL_CFLAGS += -DHWC_ENABLE_SECURE_LAYER_PROCESS
endif
ifeq ($(HWC_DISABLE_CURSOR_PLANE), true)
LOCAL_CFLAGS += -DHWC_DISABLE_CURSOR_PLANE
endif
ifeq ($(TARGET_USE_DEFAULT_HDR_PROPERTY),true)
LOCAL_CFLAGS += -DHWC_ENABLE_DEFAULT_HDR_CAPABILITIES
endif
ifeq ($(TARGET_APP_LAYER_USE_CONTINUOUS_BUFFER),false)
LOCAL_CFLAGS += -DHWC_FORCE_CLIENT_COMPOSITION
endif

#the following feature havenot finish.
ifeq ($(HWC_ENABLE_GE2D_COMPOSITION), true)
LOCAL_CFLAGS += -DHWC_ENABLE_GE2D_COMPOSITION
endif
ifeq ($(HWC_ENABLE_DISPLAY_MODE_MANAGEMENT), true)
LOCAL_CFLAGS += -DHWC_ENABLE_DISPLAY_MODE_MANAGEMENT
endif
#*********************************HWC CONFIGS END************************


LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/common/include/display \
    $(LOCAL_PATH)/common/include/composer \
    $(LOCAL_PATH)/common/include/base \
    $(LOCAL_PATH)/common/include/utils \
    $(LOCAL_PATH)/common/include/debug \
    $(LOCAL_PATH)/composition/include \
    system/core/libsync \
    system/core/libsync/include \
    system/core/include \
    system/core/include/system \
    vendor/amlogic/system/libge2d/inlcude \
    system/core/libion/include/ \
    system/core/libion/kernel-headers

LOCAL_C_INCLUDES += \
    hardware/amlogic/gralloc/amlogic \
    hardware/amlogic/gralloc \
    $(TOP)/hardware/amlogic/media/amavutils/include \
    $(TOP)/vendor/amlogic/frameworks/services/systemcontrol

LOCAL_COMMON_BASE_FILES := \
    common/base/DrmFramebuffer.cpp \
    common/base/DrmSync.cpp \
    common/base/DrmTypes.cpp \
    common/base/HwcConfig.cpp \
    common/base/HwcPowerMode.cpp

LOCAL_COMMON_DISPLAY_FILES  := \
    common/display/HwDisplayManager.cpp \
    common/display/HwDisplayVsync.cpp \
    common/display/HwDisplayCrtc.cpp \
    common/display/HwDisplayPlane.cpp \
    common/display/DummyPlane.cpp \
    common/display/OsdPlane.cpp \
    common/display/CursorPlane.cpp \
    common/display/LegacyVideoPlane.cpp \
    common/display/HwcVideoPlane.cpp \
    common/display/HwConnectorFactory.cpp \
    common/display/HwDisplayConnector.cpp \
    common/display/HwDisplayEventListener.cpp \
    common/display/ConnectorHdmi.cpp \
    common/display/ConnectorPanel.cpp \
    common/display/AmVinfo.cpp

LOCAL_COMMON_UTILS_FILES  := \
    common/utils/misc.cpp \
    common/utils/systemcontrol.cpp \
    common/utils/BitsMap.cpp \
    common/utils/EventThread.cpp \
    common/debug/DebugHelper.cpp

LOCAL_COMPOSITION_FILES := \
    composition/Composition.cpp \
    composition/CompositionStrategyFactory.cpp \
    composition/composer/ComposerFactory.cpp \
    composition/composer/ClientComposer.cpp \
    composition/composer/DummyComposer.cpp \
    composition/simplestrategy/SingleplaneComposition/SingleplaneComposition.cpp \
    composition/simplestrategy/MultiplanesComposition/MultiplanesComposition.cpp
ifeq ($(TARGET_SUPPORT_GE2D_COMPOSITION),true)
LOCAL_COMPOSITION_FILES += \
    common/composer/GE2DComposer.cpp
endif

ifeq ($(USE_HWC1),true)
#LOCAL_HWC_FILES :=
endif

ifeq ($(USE_HWC2),true)
LOCAL_HWC_FILES := \
    hwc2/Hwc2Base.cpp \
    hwc2/Hwc2Display.cpp \
    hwc2/Hwc2Layer.cpp \
    hwc2/Hwc2Module.cpp \
    hwc2/HwcModeMgr.cpp \
    hwc2/FixedSizeModeMgr.cpp \
    hwc2/VariableModeMgr.cpp \
    hwc2/VirtualDisplay.cpp \
    hwc2/MesonHwc2.cpp
endif

LOCAL_SHARED_LIBRARIES := \
    libamgralloc_ext \
    libcutils \
    liblog \
    libdl \
    libhardware \
    libutils \
    libsync \
    libion \
    libge2d \
    libbinder\
    libsystemcontrolservice

LOCAL_STATIC_LIBRARIES := \
    libomxutil

#For Android p,and later
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
# added for treble
LOCAL_SHARED_LIBRARIES += \
    vendor.amlogic.hardware.systemcontrol@1.0 \
    libbase \
    libhidlbase \
    libhidltransport

LOCAL_C_INCLUDES += \
    $(TOP)/vendor/amlogic/frameworks/services/systemcontrol
endif

LOCAL_SRC_FILES := \
    $(LOCAL_COMMON_BASE_FILES) \
    $(LOCAL_COMMON_COMPOSER_FILES) \
    $(LOCAL_COMMON_DISPLAY_FILES) \
    $(LOCAL_COMMON_UTILS_FILES) \
    $(LOCAL_COMPOSITION_FILES) \
    $(LOCAL_HWC_FILES)
#LOCAL_ALLOW_UNDEFINED_SYMBOLS:=true;

LOCAL_SHARED_LIBRARIES += libamavutils_alsa

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.amlogic

include $(BUILD_SHARED_LIBRARY)
