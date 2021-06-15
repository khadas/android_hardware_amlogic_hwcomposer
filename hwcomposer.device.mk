# which backend to use
ifeq ($(HWC_ENABLE_DRM_BACKEND), true)
HWC_BACKEND := drm
else
HWC_BACKEND := fbdev
endif

ifeq ($(TARGET_BUILD_VARIANT), user)
HWC_RELEASE := true
endif

ifeq ($(HWC_DISPLAY_NUM), 1)
HWC_EXTEND_FRAMEBUFFER_WIDTH := 0
HWC_EXTEND_FRAMEBUFFER_HEIGHT := 0
HWC_EXTEND_CONNECTOR_TYPE := invalid
endif

ifndef HWC_PIPELINE
HWC_PIPELINE := default
endif

ifndef HWC_HDMI_FRAC_MODE
HWC_HDMI_FRAC_MODE := 0
endif

# TODO remove it when android S sdk released
#ifeq ($(PLATFORM_VERSION_CODENAME), S)
#ifeq ($(filter S, $(PLATFORM_VERSION_CODENAME)), S)
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 31 && echo OK),OK)
HWC_ANDROID_S := true
else
HWC_ANDROID_S := false
endif

# Setup configuration in Soong namespace
#
SOONG_CONFIG_NAMESPACES += meson_hwc
SOONG_CONFIG_meson_hwc += \
    hwc_release \
    hwc_backend \
    display_num \
    primary_fb_width \
    primary_fb_height \
    extend_fb_width \
    extend_fb_height \
    primary_connector_type \
    extend_connector_type \
    enable_headless_mode \
    enable_software_vsync \
    enable_primary_hotplug \
    enable_secure_layer_process \
    disable_cursor_plane \
    enable_keystone_correction \
    enable_ge2d_composition \
    hdmi_frac_mode \
    hwc_pipeline \
    android_version_s \
    vdin_fbprocessor \
    enable_active_mode \
    enable_real_mode \
    enable_pre_display_calibrate \
    target_use_default_hdr_property \
    target_app_layer_use_continuous_buffer \
    pipe_viu1vdinviu2_always_loopback \
    dynamic_switch_connector \
    dynamic_swich_viu

SOONG_CONFIG_meson_hwc_hwc_release := $(HWC_RELEASE)
SOONG_CONFIG_meson_hwc_hwc_backend := $(HWC_BACKEND)
SOONG_CONFIG_meson_hwc_display_num := $(HWC_DISPLAY_NUM)
SOONG_CONFIG_meson_hwc_primary_fb_width := $(HWC_PRIMARY_FRAMEBUFFER_WIDTH)
SOONG_CONFIG_meson_hwc_primary_fb_height := $(HWC_PRIMARY_FRAMEBUFFER_HEIGHT)
SOONG_CONFIG_meson_hwc_extend_fb_width := $(HWC_EXTEND_FRAMEBUFFER_WIDTH)
SOONG_CONFIG_meson_hwc_extend_fb_height := $(HWC_EXTEND_FRAMEBUFFER_HEIGHT)
SOONG_CONFIG_meson_hwc_primary_connector_type := $(HWC_PRIMARY_CONNECTOR_TYPE)
SOONG_CONFIG_meson_hwc_extend_connector_type := $(HWC_EXTEND_CONNECTOR_TYPE)
SOONG_CONFIG_meson_hwc_enable_headless_mode := $(HWC_ENABLE_HEADLESS_MODE)
SOONG_CONFIG_meson_hwc_enable_software_vsync := $(HWC_ENABLE_SOFTWARE_VSYNC)
SOONG_CONFIG_meson_hwc_enable_primary_hotplug := $(HWC_ENABLE_PRIMARY_HOTPLUG)
SOONG_CONFIG_meson_hwc_enable_secure_layer_process := $(HWC_ENABLE_SECURE_LAYER_PROCESS)
SOONG_CONFIG_meson_hwc_disable_cursor_plane := $(HWC_DISABLE_CURSOR_PLANE)
SOONG_CONFIG_meson_hwc_enable_keystone_correction := $(HWC_ENABLE_KEYSTONE_CORRECTION)
SOONG_CONFIG_meson_hwc_enable_ge2d_composition := $(HWC_ENABLE_GE2D_COMPOSITION)
SOONG_CONFIG_meson_hwc_enable_display_mode_management := $(HWC_ENABLE_DISPLAY_MODE_MANAGEMENT)
SOONG_CONFIG_meson_hwc_hdmi_frac_mode := $(HWC_HDMI_FRAC_MODE)
SOONG_CONFIG_meson_hwc_hwc_pipeline := $(HWC_PIPELINE)
SOONG_CONFIG_meson_hwc_vdin_fbprocessor := $(HWC_VDIN_FBPROCESSOR)
SOONG_CONFIG_meson_hwc_enable_active_mode := $(HWC_ENABLE_ACTIVE_MODE)
SOONG_CONFIG_meson_hwc_enable_real_mode := $(HWC_ENABLE_REAL_MODE)
SOONG_CONFIG_meson_hwc_enable_pre_display_calibrate := $(HWC_ENABLE_PRE_DISPLAY_CALIBRATE)
SOONG_CONFIG_meson_hwc_target_use_default_hdr_property := $(HWC_ENABLE_DEFAULT_HDR_CAPABILITIES)
SOONG_CONFIG_meson_hwc_target_app_layer_use_continuous_buffer := $(TARGET_APP_LAYER_USE_CONTINUOUS_BUFFER)
SOONG_CONFIG_meson_hwc_pipe_viu1vdinviu2_always_loopback := $(HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK)
SOONG_CONFIG_meson_hwc_dynamic_switch_connector := $(HWC_DYNAMIC_SWITCH_CONNECTOR)
SOONG_CONFIG_meson_hwc_dynamic_swich_viu := $(HWC_DYNAMIC_SWITCH_VIU)
SOONG_CONFIG_meson_hwc_android_version_s := $(HWC_ANDROID_S)

#$(warning "the value of version_s: $(HWC_ANDROID_S)")
