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

ifndef HWC_ENFORCES_MAX_REFRESH_RATE
HWC_ENFORCES_MAX_REFRESH_RATE := 0
endif

ifndef HWC_VIDEO_AISR
HWC_VIDEO_AISR := false
endif

ifndef HWC_VIDEO_AIPQ
HWC_VIDEO_AIPQ := false
endif

ifndef HWC_VT_HW_VSYNC
HWC_VT_HW_VSYNC := false
endif

ifndef HWC_UVM_DETTACH
HWC_UVM_DETTACH := false
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
$(call soong_config_set,meson_hwc,hwc_release,$(HWC_RELEASE))
$(call soong_config_set,meson_hwc,hwc_backend,$(HWC_BACKEND))
$(call soong_config_set,meson_hwc,display_num,$(HWC_DISPLAY_NUM))
$(call soong_config_set,meson_hwc,primary_fb_width,$(HWC_PRIMARY_FRAMEBUFFER_WIDTH))
$(call soong_config_set,meson_hwc,primary_fb_height,$(HWC_PRIMARY_FRAMEBUFFER_HEIGHT))
$(call soong_config_set,meson_hwc,enforces_max_refresh_rate,$(HWC_ENFORCES_MAX_REFRESH_RATE))
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
$(call soong_config_set,meson_hwc,vdin_fbprocessor,$(HWC_VDIN_FBPROCESSOR))
$(call soong_config_set,meson_hwc,enable_active_mode,$(HWC_ENABLE_ACTIVE_MODE))
$(call soong_config_set,meson_hwc,enable_real_mode,$(HWC_ENABLE_REAL_MODE))
$(call soong_config_set,meson_hwc,enable_pre_display_calibrate,$(HWC_ENABLE_PRE_DISPLAY_CALIBRATE))
$(call soong_config_set,meson_hwc,target_use_default_hdr_property,$(HWC_ENABLE_DEFAULT_HDR_CAPABILITIES))
$(call soong_config_set,meson_hwc,target_app_layer_use_continuous_buffer,$(TARGET_APP_LAYER_USE_CONTINUOUS_BUFFER))
$(call soong_config_set,meson_hwc,pipe_viu1vdinviu2_always_loopback,$(HWC_PIPE_VIU1VDINVIU2_ALWAYS_LOOPBACK))
$(call soong_config_set,meson_hwc,dynamic_switch_connector,$(HWC_DYNAMIC_SWITCH_CONNECTOR))
$(call soong_config_set,meson_hwc,dynamic_swich_viu,$(HWC_DYNAMIC_SWITCH_VIU))
$(call soong_config_set,meson_hwc,android_version_s,$(HWC_ANDROID_S))
$(call soong_config_set,meson_hwc,enable_video_aisr,$(HWC_VIDEO_AISR))
$(call soong_config_set,meson_hwc,enable_vt_hwVsync,$(HWC_VT_HW_VSYNC))
$(call soong_config_set,meson_hwc,enable_video_aipq,$(HWC_VIDEO_AIPQ))
$(call soong_config_set,meson_hwc,enable_uvm_dettach,$(HWC_UVM_DETTACH))

#$(warning "the value of version_s: $(HWC_ANDROID_S)")
$(warning "the value of uvm_dettach: $(HWC_UVM_DETTACH)")
