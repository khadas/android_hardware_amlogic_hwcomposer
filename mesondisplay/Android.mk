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

LOCAL_PATH:= $(call my-dir)

HIDL_DEP_SHARED_LIBRARIES := \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    vendor.amlogic.display.meson_display_ipc@1.0 \
    libui \
    liblog \
    libutils \
    libcutils

####################################################
####### shared :libmeson_display_service ############
####################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libmeson_display_service

LOCAL_CFLAGS := -DLOG_TAG=\"MesonDisplayServer\"

LOCAL_SRC_FILES := \
   service/DisplayService.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/service

LOCAL_SHARED_LIBRARIES := \
    libamgralloc_ext \
    libbase \
    $(HIDL_DEP_SHARED_LIBRARIES)

LOCAL_STATIC_LIBRARIES += \
    meson_display.adapter_common_static \
    hwc.utils_static \
    libjsoncpp

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES)

LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := \
    $(LOCAL_STATIC_LIBRARIES)

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    $(LOCAL_SHARED_LIBRARIES)

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)

###########################################################
####### static :meson_display.adapter_common_static  ##########
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE := meson_display.adapter_common_static

LOCAL_SRC_FILES := \
    adapter/DisplayAdapterCommon.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/adapter \
    $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES)

LOCAL_SHARED_LIBRARIES :=  \
    liblog \
    libcutils

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libjsoncpp

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_STATIC_LIBRARY)

###########################################################
####### static :meson_display.adapter_local_static ############
###########################################################
include $(CLEAR_VARS)
# need distinct with static lib "libmeson_display_adapter_local_static" for recovery on Android.bp
LOCAL_MODULE := meson_display.adapter_local_static
LOCAL_CFLAGS := -DLOG_TAG=\"MesonDisplayServer\"

LOCAL_SRC_FILES := \
    adapter/DisplayAdapterLocal.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/adapter

#copy frme hwc. because hwc.display_static depend it.
LOCAL_HWC_SHARED_LIBS := \
    $(HIDL_DEP_SHARED_LIBRARIES) \
    libamgralloc_ext \
    libdl \
    libhardware \
    libsync \
    libion \
    libge2d \
    vendor.amlogic.hardware.systemcontrol@1.0 \
    vendor.amlogic.hardware.systemcontrol@1.1 \
    libbase \
    libbinder

LOCAL_SHARED_LIBRARIES :=  \
    $(LOCAL_HWC_SHARED_LIBS) \
    liblog

LOCAL_STATIC_LIBRARIES += \
    meson_display.adapter_common_static \
    hwc.common_static \
    hwc.composition_static \
    hwc.postprocessor_static \
    hwc.display_static \
    hwc.utils_static \
    hwc.base_static \
    hwc.debug_static \
    hwc.composer_static \
    libomxutil

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    $(LOCAL_SHARED_LIBRARIES)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES)

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 28 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_STATIC_LIBRARY)

####################################################
####### shared :libmeson_display_adapter_remote ####
####################################################
include $(CLEAR_VARS)

LOCAL_MODULE:= libmeson_display_adapter_remote

LOCAL_CFLAGS := -DLOG_TAG=\"MesonDisplayClient\"

LOCAL_SRC_FILES:= \
    adapter/DisplayAdapterRemote.cpp \
    adapter/DisplayClient.cpp

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/include \
  $(LOCAL_PATH)/adapter

LOCAL_EXPORT_C_INCLUDE_DIRS := \
  $(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES := \
    $(HIDL_DEP_SHARED_LIBRARIES) \
    android.hardware.graphics.mapper@2.0 \
    android.hardware.graphics.mapper@3.0 \
    android.hardware.graphics.mapper@4.0

LOCAL_STATIC_LIBRARIES := \
    meson_display.adapter_common_static \
    hwc.utils_static \
    libjsoncpp

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    $(LOCAL_SHARED_LIBRARIES)

LOCAL_EXPORT_STATIC_LIBRARY_HEADERS := \
    $(LOCAL_STATIC_LIBRARIES)

LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

include $(BUILD_SHARED_LIBRARY)

#######################################
####### exec :meson_display_client ####
#######################################
include $(CLEAR_VARS)

LOCAL_MODULE:= meson_display_client

LOCAL_SRC_FILES:= \
    test/display_client.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libmeson_display_adapter_remote


LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

include $(BUILD_EXECUTABLE)


####################################
####### exec :test_ipc ##########
####################################
include $(CLEAR_VARS)

LOCAL_MODULE:= test_ipc

LOCAL_SRC_FILES := \
    test/ipc_test.cpp

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    $(HIDL_DEP_SHARED_LIBRARIES) \
    libmeson_display_adapter_remote \
    libmeson_display_service

LOCAL_STATIC_LIBRARIES := \
    meson_display.adapter_common_static \
    libjsoncpp

LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_EXECUTABLE)


#######################################
####### exec :test_display_recovery ####
#######################################
include $(CLEAR_VARS)

LOCAL_MODULE:= meson_display_client_recovery

LOCAL_CFLAGS += -DRECOVERY_MODE

LOCAL_SRC_FILES := \
    test/display_client.cpp

LOCAL_STATIC_LIBRARIES := \
    meson_display.adapter_common_static \
    libmeson_display_adapter_local_static

LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_EXECUTABLE)



#######################################
####### exec :test_watcher ####
#######################################
include $(CLEAR_VARS)

LOCAL_MODULE:= test_watcher

LOCAL_CFLAGS +=

LOCAL_SRC_FILES := \
    test/watcher.cpp

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES :=  \
    liblog

LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_EXECUTABLE)
include $(call all-makefiles-under,$(LOCAL_PATH))
