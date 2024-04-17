# the following configs need to be defined in BoardConfig.mk
# HWC_DISPLY_NUM
# HWC_PRIMARY_FRAMEBUFFER_WIDTH
# HWC_PRIMARY_FRAMEBUFFER_HEIGHT
# HWC_PRIMARY_CONNECTOR_TYPE

ifeq ($(TARGET_BUILD_VARIANT), user)
HWC_RELEASE := true
else
HWC_RELEASE := false
endif

ifeq ($(HWC_DISPLAY_NUM), 1)
HWC_EXTEND_FRAMEBUFFER_WIDTH := 0
HWC_EXTEND_FRAMEBUFFER_HEIGHT := 0
HWC_EXTEND_CONNECTOR_TYPE := invalid
endif

ifndef HWC_ENABLE_HEADLESS_MODE
HWC_ENABLE_HEADLESS_MODE := false
endif

ifndef HWC_ENABLE_SOFTWARE_VSYNC
HWC_ENABLE_SOFTWARE_VSYNC := false
endif

ifndef HWC_ENABLE_PRIMARY_HOTPLUG
HWC_ENABLE_PRIMARY_HOTPLUG := false
endif

ifndef HWC_ENABLE_SECURE_LAYER_PROCESS
HWC_ENABLE_SECURE_LAYER_PROCESS := false
endif

ifndef HWC_DISABLE_CURSOR_PLANE
HWC_DISABLE_CURSOR_PLANE := true
endif

ifndef HWC_ENABLE_KEYSTONE_CORRECTION
HWC_ENABLE_KEYSTONE_CORRECTION := false
endif

ifndef HWC_ENABLE_GE2D_COMPOSITION
HWC_ENABLE_GE2D_COMPOSITION := false
endif

ifndef HWC_PIPELINE
HWC_PIPELINE := default
endif

ifndef HWC_HDMI_FRAC_MODE
HWC_HDMI_FRAC_MODE := 0
endif

ifndef HWC_ENFORCES_MAX_REFRESH_RATE
HWC_ENFORCES_MAX_REFRESH_RATE := 0
endif

ifndef HWC_VSYNC_PERIOD_SCALE_FACTOR
HWC_VSYNC_PERIOD_SCALE_FACTOR := 0
endif

ifndef HWC_ENABLE_FULL_ACTIVE_MODE
HWC_ENABLE_FULL_ACTIVE_MODE := false
endif

ifndef HWC_ENABLE_ACTIVE_MODE
HWC_ENABLE_ACTIVE_MODE := false
endif

ifndef HWC_ENABLE_REAL_MODE
HWC_ENABLE_REAL_MODE := false
endif

ifndef HWC_ENABLE_PRE_DISPLAY_CALIBRATE
HWC_ENABLE_PRE_DISPLAY_CALIBRATE := false
endif

ifndef HWC_ENABLE_DEFAULT_HDR_CAPABILITIES
HWC_ENABLE_DEFAULT_HDR_CAPABILITIES := false
endif

ifndef HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK
HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK := false
endif

ifndef HWC_VIDEO_MOSAIC
HWC_VIDEO_MOSAIC := false
endif

ifndef HWC_DYNAMIC_SWITCH_CONNECTOR
HWC_DYNAMIC_SWITCH_CONNECTOR := false
endif

ifndef HWC_DYNAMIC_SWITCH_VIU
HWC_DYNAMIC_SWITCH_VIU := false
endif

ifndef HWC_ENABLE_SEAMLESS_MODE_SWITCH
HWC_ENABLE_SEAMLESS_MODE_SWITCH := false
endif

ifndef HWC_ODD_CROP_WIDTH_NEED_CLIENT_COMPOSITE
HWC_ODD_CROP_WIDTH_NEED_CLIENT_COMPOSITE := false
endif

ifndef HWC_VIDEO_AISR
HWC_VIDEO_AISR := false
endif

ifndef HWC_VIDEO_AIPQ
HWC_VIDEO_AIPQ := false
endif

ifndef HWC_VIDEO_AIPQ_GPU
HWC_VIDEO_AIPQ_GPU := false
endif

ifndef HWC_VIDEO_AIFACE
HWC_VIDEO_AIFACE := false
endif

ifndef HWC_VIDEO_AICOLOR
HWC_VIDEO_AICOLOR := false
endif

ifndef HWC_VIDEO_DI
HWC_VIDEO_DI := false
endif

ifndef HWC_VT_HW_VSYNC
HWC_VT_HW_VSYNC := false
endif

ifndef HWC_UVM_DETTACH
HWC_UVM_DETACH := false
else
HWC_UVM_DETACH := $(HWC_UVM_DETTACH)
endif

ifndef HWC_FILTER_16_9MODE
HWC_FILTER_16_9MODE := false
endif

ifndef HWC_ENABLE_AIDL
HWC_ENABLE_AIDL := false
endif

ifndef HWC_ENABLE_VIRTUAL_LAYER
HWC_ENABLE_VIRTUAL_LAYER := false
endif

ifndef HWC_VIDEO_AIPROCESS_120
HWC_VIDEO_AIPROCESS_120 := false
endif

ifndef HWC_ENABLE_HAL_VIRTUALDISPLAY
HWC_ENABLE_HAL_VIRTUALDISPLAY := false
endif

ifndef HWC_LEGACY_VIDEO
HWC_LEGACY_VIDEO := false
endif

ifndef CONFIG_DEVICE_LOW_RAM
HWC_NON_LOW_RAM := true
else
HWC_NON_LOW_RAM := false
endif


# Setup configuration in Soong namespace
#
$(call soong_config_set,meson_hwc,hwc_release,$(HWC_RELEASE))
$(call soong_config_set,meson_hwc,display_num,$(HWC_DISPLAY_NUM))
$(call soong_config_set,meson_hwc,primary_fb_width,$(HWC_PRIMARY_FRAMEBUFFER_WIDTH))
$(call soong_config_set,meson_hwc,primary_fb_height,$(HWC_PRIMARY_FRAMEBUFFER_HEIGHT))
$(call soong_config_set,meson_hwc,enforces_max_refresh_rate,$(HWC_ENFORCES_MAX_REFRESH_RATE))
$(call soong_config_set,meson_hwc,vsync_period_scale_factor,$(HWC_VSYNC_PERIOD_SCALE_FACTOR))
$(call soong_config_set,meson_hwc,extend_fb_width,$(HWC_EXTEND_FRAMEBUFFER_WIDTH))
$(call soong_config_set,meson_hwc,extend_fb_height,$(HWC_EXTEND_FRAMEBUFFER_HEIGHT))
$(call soong_config_set,meson_hwc,primary_connector_type,$(HWC_PRIMARY_CONNECTOR_TYPE))
$(call soong_config_set,meson_hwc,extend_connector_type,$(HWC_EXTEND_CONNECTOR_TYPE))
$(call soong_config_set,meson_hwc,enable_headless_mode,$(HWC_ENABLE_HEADLESS_MODE))
$(call soong_config_set,meson_hwc,enable_software_vsync,$(HWC_ENABLE_SOFTWARE_VSYNC))
$(call soong_config_set,meson_hwc,enable_primary_hotplug,$(HWC_ENABLE_PRIMARY_HOTPLUG))
$(call soong_config_set,meson_hwc,enable_secure_layer_process,$(HWC_ENABLE_SECURE_LAYER_PROCESS))
$(call soong_config_set,meson_hwc,disable_cursor_plane,$(HWC_DISABLE_CURSOR_PLANE))
$(call soong_config_set,meson_hwc,enable_keystone_correction,$(HWC_ENABLE_KEYSTONE_CORRECTION))
$(call soong_config_set,meson_hwc,enable_ge2d_composition,$(HWC_ENABLE_GE2D_COMPOSITION))
$(call soong_config_set,meson_hwc,enable_display_mode_management,$(HWC_ENABLE_DISPLAY_MODE_MANAGEMENT))
$(call soong_config_set,meson_hwc,hdmi_frac_mode,$(HWC_HDMI_FRAC_MODE))
$(call soong_config_set,meson_hwc,hwc_pipeline,$(HWC_PIPELINE))
$(call soong_config_set,meson_hwc,enable_full_active_mode,$(HWC_ENABLE_FULL_ACTIVE_MODE))
$(call soong_config_set,meson_hwc,enable_active_mode,$(HWC_ENABLE_ACTIVE_MODE))
$(call soong_config_set,meson_hwc,enable_real_mode,$(HWC_ENABLE_REAL_MODE))
$(call soong_config_set,meson_hwc,enable_pre_display_calibrate,$(HWC_ENABLE_PRE_DISPLAY_CALIBRATE))
$(call soong_config_set,meson_hwc,target_use_default_hdr_property,$(HWC_ENABLE_DEFAULT_HDR_CAPABILITIES))
$(call soong_config_set,meson_hwc,pipe_viu1vdinviu2_always_loopback,$(HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK))
$(call soong_config_set,meson_hwc,dynamic_switch_connector,$(HWC_DYNAMIC_SWITCH_CONNECTOR))
$(call soong_config_set,meson_hwc,dynamic_switch_viu,$(HWC_DYNAMIC_SWITCH_VIU))
$(call soong_config_set,meson_hwc,enable_seamless_mode_switch,$(HWC_ENABLE_SEAMLESS_MODE_SWITCH))
$(call soong_config_set,meson_hwc,odd_crop_width_need_client_composite,$(HWC_ODD_CROP_WIDTH_NEED_CLIENT_COMPOSITE))
$(call soong_config_set,meson_hwc,enable_aidl,$(HWC_ENABLE_AIDL))
$(call soong_config_set,meson_hwc,enable_vt_hwVsync,$(HWC_VT_HW_VSYNC))
$(call soong_config_set,meson_hwc,enable_video_aisr,$(HWC_VIDEO_AISR))
$(call soong_config_set,meson_hwc,enable_video_aipq,$(HWC_VIDEO_AIPQ))
$(call soong_config_set,meson_hwc,enable_video_aipq_gpu,$(HWC_VIDEO_AIPQ_GPU))
$(call soong_config_set,meson_hwc,enable_video_aiface,$(HWC_VIDEO_AIFACE))
$(call soong_config_set,meson_hwc,enable_video_aicolor,$(HWC_VIDEO_AICOLOR))
$(call soong_config_set,meson_hwc,enable_video_di,$(HWC_VIDEO_DI))
$(call soong_config_set,meson_hwc,enable_uvm_detach,$(HWC_UVM_DETACH))
$(call soong_config_set,meson_hwc,filter_16_9mode,$(HWC_FILTER_16_9MODE))
$(call soong_config_set,meson_hwc,android_platform_sdk_extension_version,$(PLATFORM_SDK_EXTENSION_VERSION))
$(call soong_config_set,meson_hwc,enable_virtual_layer,$(HWC_ENABLE_VIRTUAL_LAYER))
$(call soong_config_set,meson_hwc,enable_video_mosaic,$(HWC_VIDEO_MOSAIC))
$(call soong_config_set,meson_hwc,enable_ai_process_120,$(HWC_VIDEO_AIPROCESS_120))
$(call soong_config_set,meson_hwc,enable_hal_virtualdisplay,$(HWC_ENABLE_HAL_VIRTUALDISPLAY))
$(call soong_config_set,meson_hwc,enable_legacy_video,$(HWC_LEGACY_VIDEO))
$(call soong_config_set,meson_hwc,non_low_ram,$(HWC_NON_LOW_RAM))

#$(warning "the value of aidl: $(HWC_ENABLE_AIDL)")
