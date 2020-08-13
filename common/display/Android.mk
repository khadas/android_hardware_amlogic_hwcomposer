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
include $(CLEAR_VARS)

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_CPPFLAGS := $(HWC_CPP_FLAGS)
LOCAL_CFLAGS := $(HWC_C_FLAGS)

ifneq ($(HWC_HDMI_FRAC_MODE),)
    LOCAL_CFLAGS += -DHWC_HDMI_FRAC_MODE=$(HWC_HDMI_FRAC_MODE)
else
    LOCAL_CFLAGS += -DHWC_HDMI_FRAC_MODE=0
    $(warning "HDMI default frac mode = 0, use fraction refresh rate.")
endif

LOCAL_SHARED_LIBRARIES := $(HWC_SHARED_LIBS)
ifeq ($(HWC_ENABLE_DRM_BACKEND), true)
LOCAL_CFLAGS += -DHWC_ENABLE_DRM_BACKEND
LOCAL_SRC_FILES := \
    drm/HwDisplayManagerDrm.cpp \
    drm/HwDisplayCrtcDrm.cpp \
    drm/OsdPlaneDrm.cpp
else
LOCAL_SRC_FILES := \
    fbdev/HwDisplayManagerFbdev.cpp \
    fbdev/HwDisplayCrtcFbdev.cpp \
    fbdev/OsdPlane.cpp \
    fbdev/HwDisplayConnectorFbdev.cpp \
    fbdev/ConnectorHdmi.cpp \
    fbdev/ConnectorCvbs.cpp \
    fbdev/ConnectorPanel.cpp \
    fbdev/HwConnectorFactory.cpp \
    fbdev/AmVinfo.cpp \
    fbdev/HwDisplayPlaneFbdev.cpp \
    fbdev/LegacyVideoPlane.cpp \
    fbdev/LegacyExtVideoPlane.cpp \
    fbdev/DummyPlane.cpp \
    fbdev/CursorPlane.cpp \
    fbdev/ConnectorDummy.cpp
endif

LOCAL_SRC_FILES += \
    HwcVideoPlane.cpp \
    HwDisplayEventListener.cpp \
    Vdin.cpp \
    VideoComposerDev.cpp \
    VideoTunnelThread.cpp

LOCAL_C_INCLUDES := \
    system/core/include \
    $(LOCAL_PATH)/../../videotunnel/include \
    $(LOCAL_PATH)/include \
    external/libdrm/include/drm \
    external/libdrm/include

LOCAL_STATIC_LIBRARIES := \
    hwc.utils_static \
    hwc.base_static \
    hwc.debug_static \
    libomxutil

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include \
    external/libdrm/include/drm

LOCAL_MODULE := hwc.display_static

include $(BUILD_STATIC_LIBRARY)
