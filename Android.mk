# Copyright (C) 2011 Amlogic
#
#

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# /system/lib/hw/hwcomposer.amlogic.so
include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libutils libcutils libhardware libsync libfbcnf
LOCAL_STATIC_LIBRARIES := libomxutil
LOCAL_SRC_FILES := hwcomposer.cpp

ifeq ($(GPU_TYPE),t83x)
LOCAL_CFLAGS += -DGRALLOC_T83X
WITH_LIBPLAYER_MODULE := false
endif

MESON_GRALLOC_DIR ?= hardware/amlogic/gralloc

LOCAL_C_INCLUDES += \
    $(MESON_GRALLOC_DIR)

LOCAL_C_INCLUDES += system/core/libion/include/ \
                system/core/libion/kernel-headers

ifneq ($(WITH_LIBPLAYER_MODULE),false)
LOCAL_SHARED_LIBRARIES += libamavutils_alsa
AMPLAYER_APK_DIR=$(TOP)/vendor/amlogic/frameworks/av/LibPlayer/
LOCAL_C_INCLUDES += \
    $(AMPLAYER_APK_DIR)/amavutils/include
LOCAL_CFLAGS += -DWITH_LIBPLAYER_MODULE=1
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
LOCAL_CFLAGS += -DWITH_EXTERNAL_DISPLAY
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_MODULE := hwcomposer.amlogic
LOCAL_CFLAGS += -DLOG_TAG=\"hwcomposer\"
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
