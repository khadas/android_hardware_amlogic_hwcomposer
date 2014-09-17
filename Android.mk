# Copyright (C) 2011 Amlogic
#
#

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# /system/lib/hw/hwcomposer.amlogic.so
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libutils libcutils libhardware libsync libfbcnf
LOCAL_STATIC_LIBRARIES := libomxutil
LOCAL_SRC_FILES := hwcomposer.cpp

ifneq (,$(wildcard hardware/amlogic/gralloc))
GRALLOC_DIR := hardware/amlogic/gralloc
else
GRALLOC_DIR := hardware/libhardware/modules/gralloc
endif

LOCAL_C_INCLUDES += \
    $(GRALLOC_DIR)

ifneq ($(WITH_LIBPLAYER_MODULE),false)
LOCAL_SHARED_LIBRARIES += libamavutils
AMPLAYER_APK_DIR=$(TOP)/packages/amlogic/LibPlayer/
LOCAL_C_INCLUDES += \
    $(AMPLAYER_APK_DIR)/amavutils/include
LOCAL_CFLAGS += -DWITH_LIBPLAYER_MODULE=1
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
#LOCAL_CFLAGS += -DDEBUG_EXTERNAL_DISPLAY_ON_PANEL
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
