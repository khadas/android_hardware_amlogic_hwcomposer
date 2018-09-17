include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ./tvp/OmxUtil.cpp

LOCAL_C_INCLUDES := \
        ./tvp \
        system/core/include

LOCAL_MODULE:= libomxutil

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

include $(BUILD_STATIC_LIBRARY)
