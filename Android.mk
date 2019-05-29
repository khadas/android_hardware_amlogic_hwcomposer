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

#common hwc make configs.
HWC_SHARED_LIBS := \
    libamgralloc_ext \
    libcutils \
    liblog \
    libdl \
    libhardware \
    libutils \
    libsync \
    libion \
    libge2d \
    libui \
    libbinder

# for android p systemcontrol service
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
HWC_SHARED_LIBS +=\
    vendor.amlogic.hardware.systemcontrol@1.0 \
    vendor.amlogic.hardware.systemcontrol@1.1 \
    libbase \
    libhidlbase \
    libhidltransport
else
HWC_SHARED_LIBS += libsystemcontrolservice
endif

HWC_C_FLAGS := -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq ($(TARGET_BUILD_VARIANT), user)
HWC_C_FLAGS += -DHWC_RELEASE=1
endif

HWC_CPP_FLAGS := -std=c++14

include $(call all-makefiles-under, $(LOCAL_PATH))
