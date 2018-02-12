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

include $(LOCAL_PATH)/common/priv/Android.mk

include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CPPFLAGS += -std=c++14
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

USE_HWC2 := true

#HWC API Version Config
ifeq ($(USE_HWC1),true)
LOCAL_CFLAGS += -DENABLE_MESON_HWC1
endif

ifeq ($(USE_HWC2),true)
LOCAL_CFLAGS += -DENABLE_MESON_HWC2
endif

#HWC Feature Config
ifeq ($(TARGET_SUPPORT_SECURE_LAYER),true)
LOCAL_CFLAGS += -DHWC_ENABLE_SECURE_LAYER
endif

ifeq ($(TARGET_SUPPORT_GE2D_COMPOSITION),true)
LOCAL_CFLAGS += -DHWC_ENABLE_GE2D_COMPOSITION
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/common/include/display \
	$(LOCAL_PATH)/common/include/composer \
	$(LOCAL_PATH)/common/include/base \
	$(LOCAL_PATH)/common/include/utils \
	$(LOCAL_PATH)/common/include/debug \
	$(LOCAL_PATH)/common/include/priv \
	$(LOCAL_PATH)/composition \
	system/core/libsync \
	system/core/libsync/include \
	system/core/include \
	system/core/include/system \
	vendor/amlogic/system/libge2d/inlcude \
	system/core/libion/include/ \
	system/core/libion/kernel-headers \
	hardware/amlogic/gralloc \
	$(TOP)/hardware/amlogic/media/amavutils/include \
	$(TOP)/vendor/amlogic/frameworks/services/systemcontrol

LOCAL_COMMON_BASE_FILES := \
	common/base/DrmFramebuffer.cpp \
	common/base/DrmSync.cpp \
	common/base/Composition.cpp

LOCAL_COMMON_COMPOSER_FILES := \
	common/composer/ComposerFactory.cpp \
	common/composer/ClientComposer.cpp \
	common/composer/DummyComposer.cpp

ifeq ($(TARGET_SUPPORT_GE2D_COMPOSITION),true)
LOCAL_COMMON_COMPOSER_FILES += \
	common/composer/GE2DComposer.cpp
endif

LOCAL_COMMON_DISPLAY_FILES  := \
	common/display/HwDisplayManager.cpp \
	common/display/HwDisplayVsync.cpp \
	common/display/HwDisplayCrtc.cpp \
	common/display/HwDisplayPlane.cpp \
	common/display/OsdPlane.cpp \
	common/display/VideoPlane.cpp \
	common/display/HwConnectorFactory.cpp \
	common/display/ConnectorHdmi.cpp \
	common/display/ConnectorPanel.cpp \
	common/display/AmVideo.cpp \
	common/display/AmVinfo.cpp

LOCAL_COMMON_UTILS_FILES  := \
	common/utils/misc.cpp \
	common/debug/DebugHelper.cpp \
	common/utils/SysTokenizer.cpp

LOCAL_COMPOSITION_FILES := \
	composition/CompositionStrategyFactory.cpp \
	composition/SimpleStrategy.cpp

ifeq ($(USE_HWC1),true)
#LOCAL_HWC_FILES :=
endif

ifeq ($(USE_HWC2),true)
LOCAL_HWC_FILES := \
	hwc2/Hwc2Base.cpp \
	hwc2/Hwc2Display.cpp \
	hwc2/Hwc2Layer.cpp \
	hwc2/Hwc2Module.cpp \
	hwc2/VirtualDisplay.cpp \
	hwc2/MesonHwc2.cpp
endif

LOCAL_SHARED_LIBRARIES := \
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
	libbhnd

#For Android O,and later
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
# added for treble
LOCAL_SHARED_LIBRARIES += \
    vendor.amlogic.hardware.systemcontrol@1.0_vendor \
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
LOCAL_C_INCLUDES += $(TOP)/hardware/amlogic/media/amavutils/include

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.amlogic

include $(BUILD_SHARED_LIBRARY)
