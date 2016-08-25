/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "HWComposer2"
#include <hardware/hardware.h>

#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/resource.h>

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <cutils/compiler.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <utils/String8.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer2.h>
#include <hardware_legacy/uevent.h>
#include <utils/String8.h>

#include <EGL/egl.h>
#include <utils/Vector.h>
#include <utils/Timers.h>
#include <system/graphics.h>
#include <sync/sync.h>
// for private_handle_t
#include <gralloc_priv.h>
#include <gralloc_helper.h>

#if WITH_LIBPLAYER_MODULE
#include <Amavutils.h>
#endif
#include "tvp/OmxUtil.h"

///Defines for debug statements - Macro LOG_TAG needs to be defined in the respective files
#define HWC_LOGVA(str)              ALOGV_IF(chk_int_prop("sys.hwc.debuglevel") >=6,"%5d %s - " str, __LINE__,__FUNCTION__);
#define HWC_LOGVB(str,...)          ALOGV_IF(chk_int_prop("sys.hwc.debuglevel") >=6,"%5d %s - " str, __LINE__, __FUNCTION__, __VA_ARGS__);
#define HWC_LOGDA(str)              ALOGD_IF(chk_int_prop("sys.hwc.debuglevel") >=5,"%5d %s - " str, __LINE__,__FUNCTION__);
#define HWC_LOGDB(str, ...)         ALOGD_IF(chk_int_prop("sys.hwc.debuglevel") >=5,"%5d %s - " str, __LINE__, __FUNCTION__, __VA_ARGS__);
#define HWC_LOGIA(str)               ALOGI_IF(chk_int_prop("sys.hwc.debuglevel") >=4,"%5d %s - " str, __LINE__, __FUNCTION__);
#define HWC_LOGIB(str, ...)          ALOGI_IF(chk_int_prop("sys.hwc.debuglevel") >=4,"%5d %s - " str, __LINE__,__FUNCTION__, __VA_ARGS__);
#define HWC_LOGWA(str)             ALOGW_IF(chk_int_prop("sys.hwc.debuglevel") >=3,"%5d %s - " str, __LINE__, __FUNCTION__);
#define HWC_LOGWB(str, ...)        ALOGW_IF(chk_int_prop("sys.hwc.debuglevel") >=3,"%5d %s - " str, __LINE__,__FUNCTION__, __VA_ARGS__);
#define HWC_LOGEA(str)              ALOGE_IF(chk_int_prop("sys.hwc.debuglevel") >=2,"%5d %s - " str, __LINE__, __FUNCTION__);
#define HWC_LOGEB(str, ...)         ALOGE_IF(chk_int_prop("sys.hwc.debuglevel") >=2,"%5d %s - " str, __LINE__,__FUNCTION__, __VA_ARGS__);

#define LOG_FUNCTION_NAME         HWC_LOGVA("ENTER");
#define LOG_FUNCTION_NAME_EXIT    HWC_LOGVA("EXIT");
#define DBG_LOGA(str)             ALOGI_IF(chk_int_prop("sys.hwc.debuglevel")  >=4,"%10s-%5d %s - " str, HWC_BUILD_NAME, __LINE__,__FUNCTION__)
#define DBG_LOGB(str, ...)        ALOGI_IF(chk_int_prop("sys.hwc.debuglevel")  >=4,"%10s-%5d %s - " str, HWC_BUILD_NAME, __LINE__,__FUNCTION__, __VA_ARGS__);

#define SYSFS_AMVIDEO_CURIDX      "/sys/module/amvideo/parameters/cur_dev_idx"
#define SYSFS_DISPLAY_MODE          "/sys/class/display/mode"
#define SYSFS_FB0_FREE_SCALE        "/sys/class/graphics/fb0/free_scale"
#define SYSFS_FB1_FREE_SCALE        "/sys/class/graphics/fb0/free_scale"
#define SYSFS_VIDEO_AXIS               "/sys/class/video/axis"
#define SYSFS_VIDEOBUFUSED          "/sys/class/amstream/videobufused"
#define SYSFS_WINDOW_AXIS           "/sys/class/graphics/fb0/window_axis"

#define MAX_SUPPORT_DISPLAYS HWC_NUM_DISPLAY_TYPES

#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
#undef ENABLE_CURSOR_LAYER
#define CHK_SKIP_DISPLAY_FB0(dispIdx) \
        if (pdev->displays[HWC_DISPLAY_EXTERNAL].connected\
            && dispIdx == HWC_DISPLAY_PRIMARY) {\
            continue;\
        }
#else
#define ENABLE_CURSOR_LAYER           1 //cursor layer supported in hwc 1.4
#define CHK_SKIP_DISPLAY_FB0(dispIdx)   //nothing to do
#endif

// hwc2
#define isValidDisplay(dpyContext, displayId) \
    if (displayId > HWC_DISPLAY_VIRTUAL || displayId < HWC_DISPLAY_PRIMARY) \
    { \
        HWC_LOGEB("Bad Display: %d", static_cast<int32_t>(displayId)); \
        return HWC2_ERROR_BAD_DISPLAY; \
    } else { \
        if (!dpyContext || (dpyContext && !dpyContext->connected)) return HWC2_ERROR_BAD_DISPLAY; \
    }

typedef struct hwc2_module {
    /**
     * Common methods of the hardware composer module.  This *must* be the first member of
     * hwc_module as users of this structure will cast a hw_module_t to
     * hwc_module pointer in contexts where it's known the hw_module_t references a
     * hwc_module.
     */
    struct hw_module_t common;
} hwc2_module_t;

typedef struct cursor_context_t{
    bool blank;
    struct framebuffer_info_t cb_info;
    void *cbuffer;
    bool show;
}cursor_context_t;


typedef struct hwc2_hotplug_cb {
    hwc2_callback_data_t callbackData;
    void (*hotplug)(hwc2_callback_data_t callbackData, hwc2_display_t displayId, int32_t intConnected);
} hwc2_hotplug_cb_t;


typedef struct hwc2_refresh_cb {
    hwc2_callback_data_t callbackData;
    void (*refresh)(hwc2_callback_data_t callbackData, hwc2_display_t displayId);
} hwc2_refresh_cb_t;


typedef struct hwc2_vsync_cb {
    hwc2_callback_data_t callbackData;
    void (*vsync)(hwc2_callback_data_t callbackData, hwc2_display_t displayId, int64_t timestamp);
} hwc2_vsync_cb_t;


// layer's number.
#define HWC2_MAX_LAYERS 32
#define HWC2_MAX_OVERLAY_LAYERS 5

typedef struct hwc_layer {
    // layer contents
    union {
        buffer_handle_t buf_hnd;

        /* When compositionType is HWC_SIDEBAND, this is the handle
         * of the sideband video stream to compose. */
        const native_handle_t* sideband_stream;
    };
    int32_t layer_acquirefence;
    hwc_region_t demage_region;

    // layer state
    hwc2_blend_mode_t blend_mode;
    hwc2_composition_t clt_cmptype;
    hwc2_composition_t dev_cmptype;
    int32_t /*android_dataspace_t*/ dataspace;
    hwc_color_t color;
    hwc_rect_t display_frame;
    float alpha;
    //native_handle_t *sideband_stream;
    hwc_frect_t source_crop;
    hwc_transform_t transform;
    hwc_region_t visible_region;
    uint32_t zorder;
} hwc_layer_t;


typedef struct display_context {
    struct framebuffer_info_t fb_info;
    struct private_handle_t*  fb_hnd;
#ifdef ENABLE_CURSOR_LAYER
    struct cursor_context_t cursor_ctx;
#endif

    bool connected;

    int32_t /*android_color_mode_t*/ color_mode;

    // client target layer.
    buffer_handle_t clnt_tgrhnd;
    hwc_region_t clnt_tgrdmge;
    int32_t tgr_acqfence;

    // num of composition type changed layer.
    uint32_t num_chgtyps;
    uint32_t num_lyrreqs;

    // bool validated;

    // vsync
    bool vsync_enable;
    int32_t vsync_period;

    // layer
    hwc2_layer_t types_layer[HWC2_MAX_OVERLAY_LAYERS];
    hwc2_layer_t requests_layer[HWC2_MAX_OVERLAY_LAYERS];
    hwc_layer_t* hwc_layer[HWC2_MAX_LAYERS];

    // virtual display.
    buffer_handle_t virhnd;
    int32_t vir_relfence;
    uint32_t width;
    uint32_t height;
    android_pixel_format_t format;
} display_context_t;


typedef struct hwc2_context {
    hwc2_device base;

    /* our private state goes below here */
    hwc_layer_t const* saved_layer;
    unsigned saved_transform;
    int32_t saved_left;
    int32_t saved_top;
    int32_t saved_right;
    int32_t saved_bottom;

    bool blank_status;

    // video buf is used flag
    char video_buf_used[32];
    // hdmi output mode
    char mode[32];

    // vsync
    pthread_t primary_vsync_thread;
    pthread_t external_vsync_thread;
    const hwc2_vsync_cb_t vsync_cb;

    const hwc2_hotplug_cb_t hotplug_cb;
    const hwc2_refresh_cb_t refresh_cb;
    pthread_t hotplug_thread;

    private_module_t *gralloc_module;
    display_context_t displays[MAX_SUPPORT_DISPLAYS];
}hwc2_context_t;


typedef struct hwc_uevent_data {
    int32_t len;
    char buf[1024];
    char name[128];
    char state[128];
} hwc_uevent_data_t;


static pthread_cond_t hwc_cond[HWC_NUM_PHYSICAL_DISPLAY_TYPES] = {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};
static pthread_mutex_t hwc_mutex[HWC_NUM_PHYSICAL_DISPLAY_TYPES] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

extern "C" int32_t clock_nanosleep(clockid_t clock_id, int32_t flags,
                           const struct timespec *request, struct timespec *remain);

static bool chk_bool_prop(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "false") && strcmp(val, "true") == 0) {
        ALOGD("prop: %s is %s",prop, val);
        return true;
    }

    return false;
}

static int32_t chk_int_prop(const char* prop) {
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "2")) {
        //ALOGV("prop: %s is %s",prop, val);
        return atoi(val);
    }
    return 0;
}

static int32_t chk_and_dup(int32_t fence) {
    if (fence < 0) {
        HWC_LOGWB("not a vliad fence %d",fence);
        return -1;
    }

    int32_t dup_fence = dup(fence);
    if (dup_fence < 0) {
        HWC_LOGWB("fence dup failed: %s", strerror(errno));
    }

    return dup_fence;
}

#if WITH_LIBPLAYER_MODULE
static bool chk_sysfs_status(const char* sysfstr, char* lastr, int32_t size) {
    char val[32];
    char *p = lastr;

    memset(val, 0, sizeof(val));
    if (amsysfs_get_sysfs_str(sysfstr, val, sizeof(val)) == 0) {
        HWC_LOGVB("val: %s, lastr: %s",val, p);
        if ((strcmp(val, p) != 0)) {
            memset(p, 0, size);
            strcpy(p, val);
            return true;
        }
    }

    return false;
}
#endif

static int32_t chk_output_mode(char* curmode) {
    int32_t modefd = open(SYSFS_DISPLAY_MODE, O_RDONLY);
    if (modefd < 0) {
        HWC_LOGEB("open (%s) fail", SYSFS_DISPLAY_MODE);
        return -1;
    }

    char outputmode[32] = {0};
    read(modefd, outputmode, 31);
    close(modefd);
    modefd = -1;

    //check if need update vsync.
    if (strcmp(outputmode, curmode) == 0) {
        HWC_LOGVB("outputmode didn't change %s", curmode);
        return 0;
    }

    strcpy(curmode, outputmode);

    int32_t period = 16666666;
    if (strstr(outputmode, "50hz") != NULL) {
        period = (int32_t)(1e9 / 50);
    } else if (strstr(outputmode, "30hz") != NULL) {
        period = (int32_t)(1e9 / 30);
    } else if (strstr(outputmode, "25hz") != NULL) {
        period = (int32_t)(1e9 / 25);
    } else if ((strstr(outputmode, "24hz") != NULL) || (strstr(outputmode, "smpte") != NULL)) {
        period = (int32_t)(1e9 / 24);
    } else
        HWC_LOGDB("displaymode (%s) doesn't  specify HZ", curmode);

    HWC_LOGVB("get new outputmode (%s) new period (%ld)", curmode, period);
    return period;
}

static bool chk_vinfo(hwc2_context_t* context, int32_t disp) {
    display_context_t *dctx = &(context->displays[disp]);
    isValidDisplay(dctx, disp);

    struct framebuffer_info_t *fbinfo = &(dctx->fb_info);

    if (fbinfo != NULL && fbinfo->fd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fbinfo->fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
        {
            ALOGE("FBIOGET_VSCREENINFO error!!!");
            return -errno;
        }

        if (vinfo.xres != fbinfo->info.xres
            || vinfo.yres != fbinfo->info.yres
            || vinfo.width != fbinfo->info.width
            || vinfo.height != fbinfo->info.height) {
            if (int32_t(vinfo.width) <= 16 || int32_t(vinfo.height) <= 9) {
                // the driver doesn't return that information
                // default to 160 dpi
                vinfo.width  = ((vinfo.xres * 25.4f)/160.0f + 0.5f);
                vinfo.height = ((vinfo.yres * 25.4f)/160.0f + 0.5f);
            }
            fbinfo->xdpi = (vinfo.xres * 25.4f) / vinfo.width;
            fbinfo->ydpi = (vinfo.yres * 25.4f) / vinfo.height;

            fbinfo->info.xres = vinfo.xres;
            fbinfo->info.yres = vinfo.yres;
            fbinfo->info.width = vinfo.width;
            fbinfo->info.height = vinfo.height;

            return true;
        }
    }
    return false;
}

/*
Operater of framebuffer
*/
int32_t init_display(hwc2_context_t* context, int32_t displayId) {
    display_context_t *dctx = &(context->displays[displayId]);

    struct framebuffer_info_t *fbinfo = &(dctx->fb_info);

    if (dctx->connected) return 0;

    pthread_mutex_lock(&hwc_mutex[displayId]);

    if ( !dctx->fb_hnd ) {
        //init information from osd.
        fbinfo->displayType = displayId;
        fbinfo->fbIdx = getOsdIdx(displayId);
        int32_t err = init_frame_buffer_locked(fbinfo);
        int32_t bufferSize = fbinfo->finfo.line_length
            * fbinfo->info.yres;
        HWC_LOGDB("init_frame_buffer get fbinfo->fbIdx (%d) "
            "fbinfo->info.xres (%d) fbinfo->info.yres (%d)",
            fbinfo->fbIdx, fbinfo->info.xres,
            fbinfo->info.yres);
        int32_t usage = 0;
        if (displayId == HWC_DISPLAY_PRIMARY) {
            context->gralloc_module->fb_primary.fb_info = *(fbinfo);
        } else if (displayId == HWC_DISPLAY_EXTERNAL) {
            context->gralloc_module->fb_external.fb_info = *(fbinfo);
            usage |= GRALLOC_USAGE_EXTERNAL_DISP;
        }

        //Register the framebuffer to gralloc module
        dctx->fb_hnd = new private_handle_t(
                        private_handle_t::PRIV_FLAGS_FRAMEBUFFER,
                        usage, fbinfo->fbSize, 0,
                        0, fbinfo->fd, bufferSize, 0);
        context->gralloc_module->base.registerBuffer(
            &(context->gralloc_module->base),
            dctx->fb_hnd);
        HWC_LOGDB("init_frame_buffer get frame size %d usage %d",
            bufferSize,usage);

    }

    dctx->connected = true;
    pthread_mutex_unlock(&hwc_mutex[displayId]);

#ifdef ENABLE_CURSOR_LAYER
    // init cursor framebuffer
    cursor_context_t* cursor_ctx = &(dctx->cursor_ctx);
    cursor_ctx->show = false;
    framebuffer_info_t* cbinfo = &(cursor_ctx->cb_info);
    cbinfo->fd = -1;

    //init information from cursor framebuffer.
    cbinfo->fbIdx = displayId*2+1;
    if (1 != cbinfo->fbIdx && 3 != cbinfo->fbIdx) {
        HWC_LOGEB("invalid fb index: %d, need to check!",
            cbinfo->fbIdx);
        return 0;
    }
    int32_t err = init_cursor_buffer_locked(cbinfo);
    if (err != 0) {
        HWC_LOGEA("init_cursor_buffer_locked failed, need to check!");
        return 0;
    }
    HWC_LOGDB("init_cursor_buffer get cbinfo->fbIdx (%d) "
        "cbinfo->info.xres (%d) cbinfo->info.yres (%d)",
                        cbinfo->fbIdx,
                        cbinfo->info.xres,
                        cbinfo->info.yres);

    if ( cbinfo->fd >= 0) {
        HWC_LOGDA("init_cursor_buffer success!");
    }else{
        HWC_LOGEA("init_cursor_buffer fail!");
    }
#endif

    return 0;
}

int32_t uninit_display(hwc2_context_t* context, int32_t displayId) {
    hwc2_context_t *ctx = (hwc2_context_t*)context;
    display_context_t *dctx = &(ctx->displays[displayId]);

    if (!dctx->connected) return 0;

    pthread_mutex_lock(&hwc_mutex[displayId]);
    dctx->connected = false;
    pthread_mutex_unlock(&hwc_mutex[displayId]);

    return 0;
}

static void hwc2_overlay_compose(
        hwc2_context_t* context, hwc2_display_t displayId,
        hwc_layer_t const* l) {
    display_context_t *dctx = &(context->displays[displayId]);
    int32_t angle = 0;

#if WITH_LIBPLAYER_MODULE
    static char last_val[32] = "0";
    static char last_axis[32] = "0";
    static char last_mode[32] = {0};
    static char last_free_scale[32] = {0};
    static char last_window_axis[50] = {0};
    bool vpp_changed = false;
    bool axis_changed = false;
    bool mode_changed = false;
    bool free_scale_changed = false;
    bool window_axis_changed =false;

    if (chk_bool_prop("ro.vout.dualdisplay4")) {
        vpp_changed = chk_sysfs_status(
            SYSFS_AMVIDEO_CURIDX,last_val, 32);
    }

    mode_changed = chk_sysfs_status(SYSFS_DISPLAY_MODE, last_mode, 32);

    free_scale_changed = chk_sysfs_status(SYSFS_FB0_FREE_SCALE, last_free_scale, 32);
#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    if (dctx->connected)
        free_scale_changed = chk_sysfs_status(SYSFS_FB1_FREE_SCALE, last_free_scale, 32);
#endif

    axis_changed = chk_sysfs_status(SYSFS_VIDEO_AXIS, last_axis, 32);
    window_axis_changed = chk_sysfs_status(SYSFS_WINDOW_AXIS, last_window_axis, 50);

    if ((context->saved_layer == l)
        && (context->saved_transform == l->transform)
        && (context->saved_left == l->display_frame.left)
        && (context->saved_top == l->display_frame.top)
        && (context->saved_right == l->display_frame.right)
        && (context->saved_bottom == l->display_frame.bottom)
        && !vpp_changed
        && !mode_changed
        && !axis_changed
        && !free_scale_changed
        && !window_axis_changed) {
        return;
    }

    switch (l->transform) {
        case 0:
            angle = 0;
        break;
        case HAL_TRANSFORM_ROT_90:
            angle = 90;
        break;
        case HAL_TRANSFORM_ROT_180:
            angle = 180;
        break;
        case HAL_TRANSFORM_ROT_270:
            angle = 270;
        break;
        default:
        return;
    }

    amvideo_utils_set_virtual_position(
        l->display_frame.left,
        l->display_frame.top,
        l->display_frame.right - l->display_frame.left,
        l->display_frame.bottom - l->display_frame.top,
        angle);

    /* the screen mode from Android framework should always be set to normal mode
    * to match the relationship between the UI and video overlay window position.
    */
    /*set screen_mode in amvideo_utils_set_virtual_position(),pls check in libplayer*/
    //amvideo_utils_set_screen_mode(0);
#endif

    context->saved_layer =      l;
    context->saved_transform =  l->transform;
    context->saved_left =       l->display_frame.left;
    context->saved_top =        l->display_frame.top;
    context->saved_right =      l->display_frame.right;
    context->saved_bottom =     l->display_frame.bottom;

#if WITH_LIBPLAYER_MODULE
    memset(last_axis, 0, sizeof(last_axis));

    if (amsysfs_get_sysfs_str(SYSFS_VIDEO_AXIS, last_axis, sizeof(last_axis)) == 0) {
        HWC_LOGDB("****last video axis is: %s",last_axis);
    }
#endif
}

static int32_t hwc2_fb_post(
        hwc2_context_t *context,
        hwc2_display_t displayId,
        int32_t* outRetireFence) {
    display_context_t *dctx = &(context->displays[displayId]);
    hwc_layer_t *hwclayer = NULL;
    int32_t err = 0, i = 0;

    // deal physical display's client target layer
#if ENABLE_CURSOR_LAYER
    cursor_context_t * cursor_ctx = &(dctx->cursor_ctx);
    framebuffer_info_t* cbinfo = &(cursor_ctx->cb_info);
    bool cursor_show = false;

    for (uint32_t i=0; i<dctx->num_lyrreqs; i++) {
        hwc2_layer_t layer = dctx->requests_layer[i];
        hwclayer = dctx->hwc_layer[layer];
        if (hwclayer && hwclayer->dev_cmptype == HWC2_COMPOSITION_CURSOR) {
            if (private_handle_t::validate(hwclayer->buf_hnd) < 0) {
                HWC_LOGEA("invalid cursor layer handle.");
                break;
            }
            private_handle_t *hnd = (private_handle_t *)hwclayer->buf_hnd;
            HWC_LOGDB("This is a Sprite, hnd->stride is %d, hnd->height is %d", hnd->stride, hnd->height);
            if (cbinfo->info.xres != (uint32_t)hnd->stride || cbinfo->info.yres != (uint32_t)hnd->height) {
                HWC_LOGEB("disp: %d cursor need to redrew", (int32_t)displayId);
                update_cursor_buffer_locked(cbinfo, hnd->stride, hnd->height);
                cursor_ctx->cbuffer = mmap(NULL, hnd->size, PROT_READ|PROT_WRITE, MAP_SHARED, cbinfo->fd, 0);
                if (cursor_ctx->cbuffer != MAP_FAILED) {
                    memcpy(cursor_ctx->cbuffer, hnd->base, hnd->size);
                    munmap(cursor_ctx->cbuffer, hnd->size);
                    HWC_LOGDA("setCursor ok");
                } else {
                    HWC_LOGEA("buffer mmap fail");
                }
            }
            cursor_show = true;
        }
    }
#endif

    if (!dctx->clnt_tgrhnd) {
        HWC_LOGEA("target handle is null.");
        *outRetireFence = -1;
        return HWC2_ERROR_NONE;
    }
    if (private_handle_t::validate(dctx->clnt_tgrhnd) < 0) {
        return HWC2_ERROR_NOT_VALIDATED;
    }

    *outRetireFence = fb_post_with_fence_locked(&(dctx->fb_info), dctx->clnt_tgrhnd, dctx->tgr_acqfence);

    if (*outRetireFence >= 0) {
        HWC_LOGDB("Get retire fence %d", *outRetireFence);
    } else {
        HWC_LOGEB("No valid retire returned. %d ", *outRetireFence);
        //-1 means no fence, less than -1 is some error
        if (*outRetireFence < -1) err = HWC2_ERROR_NOT_VALIDATED;
        *outRetireFence -1;
    }


#if ENABLE_CURSOR_LAYER
    // finally we need to update cursor's blank status
    if (cbinfo->fd > 0 && (cursor_show != cursor_ctx->show) ) {
        cursor_ctx->show = cursor_show;
        HWC_LOGDB("UPDATE FB1 status to %d ",cursor_show);
        ioctl(cbinfo->fd, FBIOBLANK, !cursor_ctx->show);
    }
#endif

    return err;
}

static int32_t hwc2_close(hw_device_t *device) {
    hwc2_context_t *dev = (hwc2_context_t *)device;

    LOG_FUNCTION_NAME

    pthread_kill(dev->primary_vsync_thread, SIGTERM);
    pthread_join(dev->primary_vsync_thread, NULL);

    pthread_kill(dev->external_vsync_thread, SIGTERM);
    pthread_join(dev->external_vsync_thread, NULL);

    uninit_display(dev, HWC_DISPLAY_PRIMARY);
    uninit_display(dev, HWC_DISPLAY_EXTERNAL);

    if (dev) free(dev);

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

//#define USE_HW_VSYNC
#ifdef USE_HW_VSYNC
/*
Still have bugs, don't use it.
*/
int32_t wait_next_vsync(hwc2_context_t* ctx, nsecs_t* vsync_timestamp) {
    static nsecs_t previewTime = 0;
    nsecs_t vsyncDiff=0;
    const nsecs_t period = ctx->vsync_period;
    //we will delay hw vsync if missing one vsync interrupt isr.
    int32_t ret = 0;

    if (ioctl(ctx->displays[0].fb_info.fd, FBIO_WAITFORVSYNC, &ret) == -1) {
        HWC_LOGEB("ioctrl error %d",ctx->displays[0].fb_info.fd);
        ret=-1;
    } else {
        if (ret == 1) {
            *vsync_timestamp = systemTime(CLOCK_MONOTONIC);
            vsyncDiff=*vsync_timestamp - previewTime;
            if (previewTime != 0) HWC_LOGEB("wait for vsync success %lld",vsyncDiff);
            vsyncDiff%=period;
            if (vsyncDiff > 500000) {
                nsecs_t sleep ;
                sleep = (period - vsyncDiff);
                *vsync_timestamp+=sleep;
                struct timespec spec;
                spec.tv_sec  = *vsync_timestamp / 1000000000;
                spec.tv_nsec = *vsync_timestamp % 1000000000;
                clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
            }
            previewTime = *vsync_timestamp;
            ret=0;
        } else {
            HWC_LOGEA("wait for vsync fail");
            ret=-1;
        }
    }
    return ret;
}
#else
//software
int32_t wait_next_primary_vsync(hwc2_context_t* ctx, nsecs_t* vsync_timestamp) {
    display_context_t displayctx = ctx->displays[HWC_DISPLAY_PRIMARY];
    static nsecs_t vsync_time = 0;
    static nsecs_t old_vsync_period = 0;
    nsecs_t sleep;
    nsecs_t now = systemTime(CLOCK_MONOTONIC);

    //cal the last vsync time with old period
    if (displayctx.vsync_period != old_vsync_period) {
        if (old_vsync_period > 0) {
            vsync_time = vsync_time +
                    ((now - vsync_time) / old_vsync_period) * old_vsync_period;
        }
        old_vsync_period = displayctx.vsync_period;
    }

    //set to next vsync time
    vsync_time += displayctx.vsync_period;

    // we missed, find where the next vsync should be
    if (vsync_time - now < 0) {
        vsync_time = now + (displayctx.vsync_period -
                 ((now - vsync_time) % displayctx.vsync_period));
    }

    struct timespec spec;
    spec.tv_sec  = vsync_time / 1000000000;
    spec.tv_nsec = vsync_time % 1000000000;

    int32_t err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);
    *vsync_timestamp = vsync_time;

    return err;
}

int32_t wait_next_external_vsync(hwc2_context_t* ctx, nsecs_t* vsync_timestamp) {
    display_context_t displayctx = ctx->displays[HWC_DISPLAY_EXTERNAL];
    static nsecs_t vsync_time = 0;
    static nsecs_t old_vsync_period = 0;
    nsecs_t sleep;
    nsecs_t now = systemTime(CLOCK_MONOTONIC);

    //cal the last vsync time with old period
    if (displayctx.vsync_period != old_vsync_period) {
        if (old_vsync_period > 0) {
            vsync_time = vsync_time +
                    ((now - vsync_time) / old_vsync_period) * old_vsync_period;
        }
        old_vsync_period = displayctx.vsync_period;
    }

    //set to next vsync time
    vsync_time += displayctx.vsync_period;

    // we missed, find where the next vsync should be
    if (vsync_time - now < 0) {
        vsync_time = now + (displayctx.vsync_period -
                 ((now - vsync_time) % displayctx.vsync_period));
    }

    struct timespec spec;
    spec.tv_sec  = vsync_time / 1000000000;
    spec.tv_nsec = vsync_time % 1000000000;

    int32_t err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);
    *vsync_timestamp = vsync_time;

    return err;
}

#endif

static void *hwc2_primary_vsync_thread(void *data) {
    uint32_t displayId = HWC_DISPLAY_PRIMARY;
    hwc2_context_t* ctx = (hwc2_context_t*)data;
    display_context_t *dctx = &(ctx->displays[displayId]);
    nsecs_t timestamp;

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY - 1);
    sleep(2);

    while (true) {
        pthread_mutex_lock(&hwc_mutex[displayId]);
        while (!dctx->vsync_enable) {
            pthread_cond_wait(&hwc_cond[displayId],
                &hwc_mutex[displayId]);
        }
        pthread_mutex_unlock(&hwc_mutex[displayId]);

        if (wait_next_primary_vsync(ctx, &timestamp) == 0) {
            if (ctx->vsync_cb.vsync)
                ctx->vsync_cb.vsync(ctx->vsync_cb.callbackData,
                    displayId, timestamp);
        }
    }

    return NULL;
}

static void *hwc2_external_vsync_thread(void *data) {
    uint32_t displayId = HWC_DISPLAY_EXTERNAL;
    hwc2_context_t* ctx = (hwc2_context_t*)data;
    display_context_t *dctx = &(ctx->displays[displayId]);
    int64_t timestamp;

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY-1);
    sleep(2);

    while (true) {
        pthread_mutex_lock(&hwc_mutex[displayId]);
        while (!dctx->vsync_enable) {
            pthread_cond_wait(&hwc_cond[displayId],
                &hwc_mutex[displayId]);
        }
        pthread_mutex_unlock(&hwc_mutex[displayId]);

        if (wait_next_external_vsync(ctx, &timestamp) == 0) {
            if (dctx->connected && ctx->vsync_cb.vsync)
                ctx->vsync_cb.vsync(ctx->vsync_cb.callbackData,
                    displayId, timestamp);
        }
    }

    return NULL;
}

//#define SIMULATE_HOT_PLUG 1
#define HDMI_UEVENT                     "DEVPATH=/devices/virtual/switch/hdmi_audio"
#define HDMI_POWER_UEVENT               "DEVPATH=/devices/virtual/switch/hdmi_power"

static bool isMatch(hwc_uevent_data_t* ueventData, const char* matchName) {
    bool matched = false;
    // Consider all zero-delimited fields of the buffer.
    const char* field = ueventData->buf;
    const char* end = ueventData->buf + ueventData->len + 1;
    do {
        if (!strcmp(field, matchName)) {
            HWC_LOGEB("Matched uevent message with pattern: %s", matchName);
            matched = true;
        }
        //SWITCH_STATE=1, SWITCH_NAME=hdmi
        else if (strstr(field, "SWITCH_STATE=")) {
            strcpy(ueventData->state, field + strlen("SWITCH_STATE="));
        }
        else if (strstr(field, "SWITCH_NAME=")) {
            strcpy(ueventData->name, field + strlen("SWITCH_NAME="));
        }
        field += strlen(field) + 1;
    } while (field != end);

    return matched;
}

#define HOTPLUG_UEVENT_DEBUG
static void *hwc2_hotplug_thread(void *data) {
    hwc2_context_t* ctx = (hwc2_context_t*)data;
    display_context_t *dctx = &(ctx->displays[HWC_DISPLAY_PRIMARY]);
    bool fpsChanged = false, sizeChanged = false;
    //use uevent instead of usleep, because it has some delay
    hwc_uevent_data_t u_data;
    memset(&u_data, 0, sizeof(hwc_uevent_data_t));
    int32_t fd = uevent_init();

    while (fd > 0) {
        fpsChanged = false;
        sizeChanged = false;
        u_data.len= uevent_next_event(u_data.buf, sizeof(u_data.buf) - 1);
        if (u_data.len <= 0)
            continue;

        u_data.buf[u_data.len] = '\0';

#ifdef HOTPLUG_UEVENT_DEBUG
        //change@/devices/virtual/switch/hdmi ACTION=change DEVPATH=/devices/virtual/switch/hdmi
        //SUBSYSTEM=switch SWITCH_NAME=hdmi SWITCH_STATE=0 SEQNUM=2791
        char printBuf[1024] = {0};
        memcpy(printBuf, u_data.buf, u_data.len);
        for (int32_t i = 0; i < u_data.len; i++) {
            if (printBuf[i] == 0x0)
                printBuf[i] = ' ';
        }
        HWC_LOGEB("Received uevent message: %s", printBuf);
#endif
        if (isMatch(&u_data, HDMI_UEVENT)) {
            HWC_LOGEB("HDMI switch_state: %s switch_name: %s\n", u_data.state, u_data.name);
            if ((!strcmp(u_data.name, "hdmi_audio")) &&
                (!strcmp(u_data.state, "1"))) {
                // update vsync period if neccessry
                nsecs_t newperiod = chk_output_mode(ctx->mode);
                // check if vsync period is changed
                if (newperiod > 0 && newperiod != dctx->vsync_period) {
                    dctx->vsync_period = newperiod;
                    fpsChanged = true;
                }
                sizeChanged = chk_vinfo(ctx, HWC_DISPLAY_PRIMARY);
                if (fpsChanged || sizeChanged) {
                    if (ctx->hotplug_cb.hotplug) {
                        ctx->hotplug_cb.hotplug(
                            ctx->hotplug_cb.callbackData,
                            HWC_DISPLAY_PRIMARY, 1);
                    }
                }
            }
        }
    }

    return NULL;
}

/*
 * Device Functions
 *
 * All of these functions take as their first parameter a device pointer, so
 * this parameter is omitted from the described parameter lists.
 */

/* createVirtualDisplay(..., width, height, format, outDisplay)
 * Descriptor: HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY
 * Must be provided by all HWC2 devices
 *
 * Creates a new virtual display with the given width and height. The format
 * passed into this function is the default format requested by the consumer of
 * the virtual display output buffers. If a different format will be returned by
 * the device, it should be returned in this parameter so it can be set properly
 * when handing the buffers to the consumer.
 *
 * The display will be assumed to be on from the time the first frame is
 * presented until the display is destroyed.
 *
 * Parameters:
 *   width - width in pixels
 *   height - height in pixels
 *   format - prior to the call, the default output buffer format selected by
 *       the consumer; after the call, the format the device will produce
 *   outDisplay - the newly-created virtual display; pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_UNSUPPORTED - the width or height is too large for the device to
 *       be able to create a virtual display
 *   HWC2_ERROR_NO_RESOURCES - the device is unable to create a new virtual
 *       display at this time
 */
hwc2_error_t createVirtualDisplay(
        hwc2_device_t* device, uint32_t width,
        uint32_t height, android_pixel_format_t* format,
        hwc2_display_t* outDisplay) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *vctx = &(ctx->displays[HWC_DISPLAY_VIRTUAL]);

    if (width > 1920 && height > 1080) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    vctx->vir_relfence = -1;
    vctx->width = width;
    vctx->height = height;
    vctx->format = *format;
    vctx->connected = true;

    *outDisplay = HWC_DISPLAY_VIRTUAL;

    // TODO:
    return HWC2_ERROR_NONE;
}

/* destroyVirtualDisplay(..., display)
 * Descriptor: HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY
 * Must be provided by all HWC2 devices
 *
 * Destroys a virtual display. After this call all resources consumed by this
 * display may be freed by the device and any operations performed on this
 * display should fail.
 *
 * Parameters:
 *   display - the virtual display to destroy
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - the display handle which was passed in does not
 *       refer to a virtual display
 */
hwc2_error_t destroyVirtualDisplay(
        hwc2_device_t* device, hwc2_display_t display) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *vctx = &(ctx->displays[HWC_DISPLAY_VIRTUAL]);

    if (display != HWC_DISPLAY_VIRTUAL) {
        return HWC2_ERROR_BAD_PARAMETER;
    }
    vctx->connected = false;
    vctx->vir_relfence = -1;
    vctx->width = 0;
    vctx->height = 0;
    vctx->format = (android_pixel_format)0;

    // TODO:
    return HWC2_ERROR_NONE;
}

/* dump(..., outSize, outBuffer)
 * Descriptor: HWC2_FUNCTION_DUMP
 * Must be provided by all HWC2 devices
 *
 * Retrieves implementation-defined debug information, which will be displayed
 * during, for example, `dumpsys SurfaceFlinger`.
 *
 * If called with outBuffer == NULL, the device should store a copy of the
 * desired output and return its length in bytes in outSize. If the device
 * already has a stored copy, that copy should be purged and replaced with a
 * fresh copy.
 *
 * If called with outBuffer != NULL, the device should copy its stored version
 * of the output into outBuffer and store how many bytes of data it copied into
 * outSize. Prior to this call, the client will have populated outSize with the
 * maximum number of bytes outBuffer can hold. The device must not write more
 * than this amount into outBuffer. If the device does not currently have a
 * stored copy, then it should return 0 in outSize.
 *
 * Any data written into outBuffer need not be null-terminated.
 *
 * Parameters:
 *   outSize - if outBuffer was NULL, the number of bytes needed to copy the
 *       device's stored output; if outBuffer was not NULL, the number of bytes
 *       written into it, which must not exceed the value stored in outSize
 *       prior to the call; pointer will be non-NULL
 *   outBuffer - the buffer to write the dump output into; may be NULL as
 *       described above; data written into this buffer need not be
 *       null-terminated
 */
void dump(hwc2_device_t* device,
        uint32_t* outSize,
        char* outBuffer) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    int32_t numHwLayers;

    android::String8 result;
    if (NULL == outBuffer) {
        *outSize = 4096;
    } else {
        using namespace android;
        result.appendFormat("Hardware Composer state (version major: %d, minor: %d):\n",
            ctx->base.common.module->version_major, ctx->base.common.module->version_minor);
        for (size_t i=0 ; i<HWC_NUM_PHYSICAL_DISPLAY_TYPES ; i++) {
            display_context_t *dctx = &(ctx->displays[i]);
            framebuffer_info_t *fbinfo = &(dctx->fb_info);
            if (!dctx->connected)
                continue;

            result.appendFormat("  Display[%zd] configurations (* current):\n", i);
            result.appendFormat("    %ux%u, xdpi=%f, ydpi=%f, refresh=%" PRId64 "\n",
                    fbinfo->info.xres, fbinfo->info.yres,
                    fbinfo->xdpi, fbinfo->ydpi, dctx->vsync_period);

            for (i=0; i<HWC2_MAX_LAYERS; i++) {
                if (NULL != dctx->hwc_layer[i])
                    numHwLayers++;
            }

            result.appendFormat(
                    "  numHwLayers=%zu\n",numHwLayers);
            result.append(
                    "    type   |  handle  | tr | blnd |     source crop (l,t,r,b)      |          frame         \n"
                    "-----------+----------+----+------+--------------------------------+------------------------\n");
            for (i=0; i<HWC2_MAX_LAYERS; i++) {
                const hwc_layer_t *hwclayer = dctx->hwc_layer[i];
                if (hwclayer) {
                    int32_t type = (int32_t)hwclayer->dev_cmptype;

                    static char const* compositionTypeName[] = {
                            "UNKNOWN",
                            "GLES",
                            "HWC",
                            "SOLID",
                            "HWC_CURSOR",
                            "SIDEBAND"};
                    result.appendFormat(
                            " %9s | %08" PRIxPTR " | %02x | %04x |%7.1f,%7.1f,%7.1f,%7.1f |%5d,%5d,%5d,%5d \n",
                                    compositionTypeName[type],
                                    intptr_t(hwclayer->buf_hnd), hwclayer->transform, hwclayer->blend_mode,
                                    hwclayer->source_crop.left, hwclayer->source_crop.top,
                                    hwclayer->source_crop.right, hwclayer->source_crop.bottom,
                                    hwclayer->display_frame.left, hwclayer->display_frame.top,
                                    hwclayer->display_frame.right, hwclayer->display_frame.bottom);
                }
            }
        }
        *outSize = 4096;
        strcpy(outBuffer, result.string());
    }
}

/* getMaxVirtualDisplayCount(...)
 * Descriptor: HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT
 * Must be provided by all HWC2 devices
 *
 * Returns the maximum number of virtual displays supported by this device
 * (which may be 0). The client will not attempt to create more than this many
 * virtual displays on this device. This number must not change for the lifetime
 * of the device.
 */
uint32_t getMaxVirtualDisplayCount(
        hwc2_device_t* device) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;

    // TODO:
    // we only support one virtual display for now.
    return 1;
}

/* registerCallback(..., descriptor, callbackData, pointer)
 * Descriptor: HWC2_FUNCTION_REGISTER_CALLBACK
 * Must be provided by all HWC2 devices
 *
 * Provides a callback for the device to call. All callbacks take a callbackData
 * item as the first parameter, so this value should be stored with the callback
 * for later use. The callbackData may differ from one callback to another. If
 * this function is called multiple times with the same descriptor, later
 * callbacks replace earlier ones.
 *
 * Parameters:
 *   descriptor - which callback should be set
 *   callBackdata - opaque data which must be passed back through the callback
 *   pointer - a non-NULL function pointer corresponding to the descriptor
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_PARAMETER - descriptor was invalid
 */
hwc2_error_t registerCallback(
        hwc2_device_t* device,
        hwc2_callback_descriptor_t descriptor,
        hwc2_callback_data_t callbackData,
        hwc2_function_pointer_t pointer) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;

    switch (descriptor) {
        // callback functions
        case HWC2_CALLBACK_HOTPLUG:
        {
            hwc2_hotplug_cb_t cb = ctx->hotplug_cb;
            cb.callbackData = callbackData;
            cb.hotplug = reinterpret_cast<HWC2_PFN_HOTPLUG>(pointer);

            if (cb.hotplug) {
                HWC_LOGEA("First primary hotplug!");
                cb.hotplug(cb.callbackData,
                    HWC_DISPLAY_PRIMARY, 1);
            }
            break;
        }
        case HWC2_CALLBACK_REFRESH:
        {
            hwc2_refresh_cb_t cb = ctx->refresh_cb;
            cb.callbackData = callbackData;
            cb.refresh = reinterpret_cast<HWC2_PFN_REFRESH>(pointer);
            break;
        }
        case HWC2_CALLBACK_VSYNC:
        {
            hwc2_vsync_cb_t cb = ctx->vsync_cb;
            cb.callbackData = callbackData;
            cb.vsync = reinterpret_cast<HWC2_PFN_VSYNC>(pointer);
            break;
        }
        case HWC2_CALLBACK_INVALID:
        default:
            ALOGE("registerCallback bad parameter: %d", descriptor);
            return HWC2_ERROR_BAD_PARAMETER;
    }

    return HWC2_ERROR_NONE;
}

/*
 * Display Functions
 *
 * All of these functions take as their first two parameters a device pointer
 * and a display handle, so these parameters are omitted from the described
 * parameter lists.
 */

/* acceptDisplayChanges(...)
 * Descriptor: HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES
 * Must be provided by all HWC2 devices
 *
 * Accepts the changes required by the device from the previous validateDisplay
 * call (which may be queried using getChangedCompositionTypes) and revalidates
 * the display. This function is equivalent to requesting the changed types from
 * getChangedCompositionTypes, setting those types on the corresponding layers,
 * and then calling validateDisplay again.
 *
 * After this call it must be valid to present this display. Calling this after
 * validateDisplay returns 0 changes must succeed with HWC2_ERROR_NONE, but
 * should have no other effect.
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called
 */
hwc2_error_t acceptDisplayChanges(
        hwc2_device_t* device, hwc2_display_t display) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // HWC2_ERROR_NOT_VALIDATED

    return HWC2_ERROR_NONE;

}

/* createLayer(..., outLayer)
 * Descriptor: HWC2_FUNCTION_CREATE_LAYER
 * Must be provided by all HWC2 devices
 *
 * Creates a new layer on the given display.
 *
 * Parameters:
 *   outLayer - the handle of the new layer; pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_NO_RESOURCES - the device was unable to create this layer
 */
hwc2_error_t createLayer(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t* outLayer) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = NULL;
    uint32_t i = 0;

    hwclayer = (hwc_layer_t *)malloc(sizeof(hwc_layer_t));
    if (NULL == hwclayer) return HWC2_ERROR_NO_RESOURCES;
    memset(hwclayer, 0, sizeof(hwc_layer_t));
    hwclayer->layer_acquirefence = -1;

    for (i=0; i<HWC2_MAX_LAYERS; i++) {
        if (NULL == dctx->hwc_layer[i]) {
            dctx->hwc_layer[i] = hwclayer;
            break;
        }
    }
    if (i > HWC2_MAX_LAYERS) return HWC2_ERROR_NO_RESOURCES;

    *outLayer = (hwc2_layer_t)i;
    return HWC2_ERROR_NONE;
}

/* destroyLayer(..., layer)
 * Descriptor: HWC2_FUNCTION_DESTROY_LAYER
 * Must be provided by all HWC2 devices
 *
 * Destroys the given layer.
 *
 * Parameters:
 *   layer - the handle of the layer to destroy
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t destroyLayer(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    if (NULL != dctx->hwc_layer[layer]) {
        free(dctx->hwc_layer[layer]);
        dctx->hwc_layer[layer] = NULL;
        return HWC2_ERROR_NONE;
    } else {
        return HWC2_ERROR_BAD_LAYER;
    }
}

/* getActiveConfig(..., outConfig)
 * Descriptor: HWC2_FUNCTION_GET_ACTIVE_CONFIG
 * Must be provided by all HWC2 devices
 *
 * Retrieves which display configuration is currently active.
 *
 * If no display configuration is currently active, this function must return
 * HWC2_ERROR_BAD_CONFIG and place no configuration handle in outConfig. It is
 * the responsibility of the client to call setActiveConfig with a valid
 * configuration before attempting to present anything on the display.
 *
 * Parameters:
 *   outConfig - the currently active display configuration; pointer will be
 *       non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_CONFIG - no configuration is currently active
 */
hwc2_error_t getActiveConfig(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_config_t* outConfig) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    return HWC2_ERROR_NONE;
}

/* getChangedCompositionTypes(..., outNumElements, outLayers, outTypes)
 * Descriptor: HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES
 * Must be provided by all HWC2 devices
 *
 * Retrieves the layers for which the device requires a different composition
 * type than had been set prior to the last call to validateDisplay. The client
 * will either update its state with these types and call acceptDisplayChanges,
 * or will set new types and attempt to validate the display again.
 *
 * outLayers and outTypes may be NULL to retrieve the number of elements which
 * will be returned. The number of elements returned must be the same as the
 * value returned in outNumTypes from the last call to validateDisplay.
 *
 * Parameters:
 *   outNumElements - if outLayers or outTypes were NULL, the number of layers
 *       and types which would have been returned; if both were non-NULL, the
 *       number of elements returned in outLayers and outTypes, which must not
 *       exceed the value stored in outNumElements prior to the call; pointer
 *       will be non-NULL
 *   outLayers - an array of layer handles
 *   outTypes - an array of composition types, each corresponding to an element
 *       of outLayers
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called for this
 *       display
 */
hwc2_error_t getChangedCompositionTypes(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumElements, hwc2_layer_t* outLayers,
        hwc2_composition_t* outTypes) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = NULL;
    hwc2_layer_t layer;
    uint32_t num_type = 0;
    // if (!dctx->validated) return HWC2_ERROR_NOT_VALIDATED;

    // if outLayers or outTypes were NULL, the number of layers and types which would have been returned.
    if (NULL == outLayers || NULL == outTypes) {
        /* for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
            hwclayer = dctx->hwc_layer[i];
            if (hwclayer && hwclayer->dev_cmptype
                && hwclayer->dev_cmptype != hwclayer->clt_cmptype) {
                num_type++;
                continue;
            }
        } */
        *outNumElements = dctx->num_chgtyps;
    } else {
        for (uint32_t i=0; i<dctx->num_chgtyps; i++) {
            hwc2_layer_t layer = dctx->types_layer[i];
            hwclayer = dctx->hwc_layer[layer];

            if (hwclayer && hwclayer->dev_cmptype
                && hwclayer->dev_cmptype != hwclayer->clt_cmptype) {
                HWC_LOGDB("composition type is changed: %d -> %d", hwclayer->clt_cmptype, hwclayer->dev_cmptype);
                outLayers[num_type] = layer;
                outTypes[num_type] = hwclayer->dev_cmptype;
                num_type++;
                continue;
            }
        }

        if (num_type > 0) {
            if (dctx->num_chgtyps == num_type) {
                HWC_LOGDB("There are %d layers type has changed.", num_type);
                *outNumElements = num_type;
            } else {
                HWC_LOGEB("outNumElements:%d is not same as outNumTypes: %d.",
                    num_type, dctx->num_chgtyps);
            }
        } else {
            HWC_LOGDA("No layers compositon type changed.");
        }
    }

    return HWC2_ERROR_NONE;
}

/* getClientTargetSupport(..., width, height, format, dataspace)
 * Descriptor: HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT
 * Must be provided by all HWC2 devices
 *
 * Returns whether a client target with the given properties can be handled by
 * the device.
 *
 * The valid formats can be found in android_pixel_format_t in
 * <system/graphics.h>.
 *
 * For more about dataspaces, see setLayerDataspace.
 *
 * This function must return true for a client target with width and height
 * equal to the active display configuration dimensions,
 * HAL_PIXEL_FORMAT_RGBA_8888, and HAL_DATASPACE_UNKNOWN. It is not required to
 * return true for any other configuration.
 *
 * Parameters:
 *   width - client target width in pixels
 *   height - client target height in pixels
 *   format - client target format
 *   dataspace - client target dataspace, as described in setLayerDataspace
 *
 * Returns HWC2_ERROR_NONE if the given configuration is supported or one of the
 * following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_UNSUPPORTED - the given configuration is not supported
 */
hwc2_error_t getClientTargetSupport(
        hwc2_device_t* device, hwc2_display_t display, uint32_t width,
        uint32_t height, android_pixel_format_t format,
        android_dataspace_t dataspace) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    struct framebuffer_info_t *fbinfo = &(dctx->fb_info);

    if (width == fbinfo->info.xres
        && height == fbinfo->info.yres
        && format == HAL_PIXEL_FORMAT_RGBA_8888
        && dataspace == HAL_DATASPACE_UNKNOWN) {
        return HWC2_ERROR_NONE;
    }

    HWC_LOGEB("fbinfo: [%d x %d], client: [%d x %d]"
        "format: %d, dataspace: %d",
        fbinfo->info.xres,
        fbinfo->info.yres,
        width, height, format, dataspace);

    // TODO: ?
    return HWC2_ERROR_UNSUPPORTED;
}

/* getColorModes(..., outNumModes, outModes)
 * Descriptor: HWC2_FUNCTION_GET_COLOR_MODES
 * Must be provided by all HWC2 devices
 *
 * Returns the color modes supported on this display.
 *
 * The valid color modes can be found in android_color_mode_t in
 * <system/graphics.h>. All HWC2 devices must support at least
 * HAL_COLOR_MODE_NATIVE.
 *
 * outNumModes may be NULL to retrieve the number of modes which will be
 * returned.
 *
 * Parameters:
 *   outNumModes - if outModes was NULL, the number of modes which would have
 *       been returned; if outModes was not NULL, the number of modes returned,
 *       which must not exceed the value stored in outNumModes prior to the
 *       call; pointer will be non-NULL
 *   outModes - an array of color modes
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getColorModes(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumModes, int32_t* /*android_color_mode_t*/ outModes) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO: ?
    return HWC2_ERROR_NONE;
}

/* getDisplayAttribute(..., config, attribute, outValue)
 * Descriptor: HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE
 * Must be provided by all HWC2 devices
 *
 * Returns a display attribute value for a particular display configuration.
 *
 * Any attribute which is not supported or for which the value is unknown by the
 * device must return a value of -1.
 *
 * Parameters:
 *   config - the display configuration for which to return attribute values
 *   attribute - the attribute to query
 *   outValue - the value of the attribute; the pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_CONFIG - config does not name a valid configuration for this
 *       display
 */
hwc2_error_t getDisplayAttribute(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_config_t config, hwc2_attribute_t attribute, int32_t* outValue) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    struct framebuffer_info_t *fbinfo = &(dctx->fb_info);

    // TODO: HWC2_ERROR_BAD_CONFIG?
    switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            *outValue = dctx->vsync_period;
        break;
        case HWC2_ATTRIBUTE_WIDTH:
            *outValue = fbinfo->info.xres;
        break;
        case HWC2_ATTRIBUTE_HEIGHT:
            *outValue = fbinfo->info.yres;
        break;
        case HWC2_ATTRIBUTE_DPI_X:
            *outValue = fbinfo->xdpi*1000;
        break;
        case HWC2_ATTRIBUTE_DPI_Y:
            *outValue = fbinfo->ydpi*1000;
        break;
        default:
            HWC_LOGEB("unknown display attribute %u", attribute);
            *outValue = -1;
        break;
    }

    return HWC2_ERROR_NONE;
}

/* getDisplayConfigs(..., outNumConfigs, outConfigs)
 * Descriptor: HWC2_FUNCTION_GET_DISPLAY_CONFIGS
 * Must be provided by all HWC2 devices
 *
 * Returns handles for all of the valid display configurations on this display.
 *
 * outConfigs may be NULL to retrieve the number of elements which will be
 * returned.
 *
 * Parameters:
 *   outNumConfigs - if outConfigs was NULL, the number of configurations which
 *       would have been returned; if outConfigs was not NULL, the number of
 *       configurations returned, which must not exceed the value stored in
 *       outNumConfigs prior to the call; pointer will be non-NULL
 *   outConfigs - an array of configuration handles
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getDisplayConfigs(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumConfigs, hwc2_config_t* outConfigs) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    if (display == HWC_DISPLAY_PRIMARY) {
        if (NULL != outConfigs) outConfigs[0] = 0;
        *outNumConfigs = 1;
    } else if (display == HWC_DISPLAY_EXTERNAL) {
        HWC_LOGEB("getDisplayConfigs:connect =  %d", dctx->connected);
        //if (!dctx->connected) return HWC2_ERROR_BAD_DISPLAY;
        if (NULL != outConfigs) outConfigs[0] = 0;
        *outNumConfigs = 1;
    }

    return HWC2_ERROR_NONE;
}

/* getDisplayName(..., outSize, outName)
 * Descriptor: HWC2_FUNCTION_GET_DISPLAY_NAME
 * Must be provided by all HWC2 devices
 *
 * Returns a human-readable version of the display's name.
 *
 * outName may be NULL to retrieve the length of the name.
 *
 * Parameters:
 *   outSize - if outName was NULL, the number of bytes needed to return the
 *       name if outName was not NULL, the number of bytes written into it,
 *       which must not exceed the value stored in outSize prior to the call;
 *       pointer will be non-NULL
 *   outName - the display's name
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getDisplayName(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outSize, char* outName) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    android::String8 pmyName = android::String8("Primary Display");
    android::String8 extName = android::String8("EXTERNAL Display");
    android::String8 virName = android::String8("Virtual Display");

    switch (display) {
        case HWC_DISPLAY_PRIMARY:
            if (NULL != outName) strcpy(outName, pmyName.string());
            *outSize = pmyName.size()+1;
        break;
        case HWC_DISPLAY_EXTERNAL:
            if (NULL != outName) strcpy(outName, extName.string());
            *outSize = extName.size()+1;
        break;
        case HWC_DISPLAY_VIRTUAL:
            if (NULL != outName) strcpy(outName, virName.string());
            *outSize = virName.size()+1;
        break;
        default:
            HWC_LOGEB("invalidate display %d", (int32_t)display);
        break;
    }

    return HWC2_ERROR_NONE;
}

/* getDisplayRequests(..., outDisplayRequests, outNumElements, outLayers,
 *     outLayerRequests)
 * Descriptor: HWC2_FUNCTION_GET_DISPLAY_REQUESTS
 * Must be provided by all HWC2 devices
 *
 * Returns the display requests and the layer requests required for the last
 * validated configuration.
 *
 * Display requests provide information about how the client should handle the
 * client target. Layer requests provide information about how the client
 * should handle an individual layer.
 *
 * If outLayers or outLayerRequests is NULL, the required number of layers and
 * requests must be returned in outNumElements, but this number may also be
 * obtained from validateDisplay as outNumRequests (outNumElements must be equal
 * to the value returned in outNumRequests from the last call to
 * validateDisplay).
 *
 * Parameters:
 *   outDisplayRequests - the display requests for the current validated state
 *   outNumElements - if outLayers or outLayerRequests were NULL, the number of
 *       elements which would have been returned, which must be equal to the
 *       value returned in outNumRequests from the last validateDisplay call on
 *       this display; if both were not NULL, the number of elements in
 *       outLayers and outLayerRequests, which must not exceed the value stored
 *       in outNumElements prior to the call; pointer will be non-NULL
 *   outLayers - an array of layers which all have at least one request
 *   outLayerRequests - the requests corresponding to each element of outLayers
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called for this
 *       display
 */
hwc2_error_t getDisplayRequests(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_display_request_t* outDisplayRequests,
        uint32_t* outNumElements,
        hwc2_layer_t* outLayers,
        hwc2_layer_request_t* outLayerRequests) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = NULL;
    hwc2_layer_t layer;
    uint32_t num_requests = 0;

    //if (!dctx->validated) return HWC2_ERROR_NOT_VALIDATED;

    // if outLayers or outTypes were NULL, the number of layers and types which would have been returned.
    if (NULL == outLayers || NULL == outLayerRequests) {
        /*for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
            hwclayer = dctx->hwc_layer[i];
            if (hwclayer && hwclayer->dev_cmptype && hwclayer->dev_cmptype != HWC2_COMPOSITION_CLIENT) {
                num_requests++;
            }
        }*/
        *outNumElements = dctx->num_lyrreqs;
    } else {
        for (uint32_t i=0; i<dctx->num_lyrreqs; i++) {
            hwc2_layer_t layer = dctx->requests_layer[i];
            hwc_layer_t *hwclayer = dctx->hwc_layer[layer];
            outLayers[num_requests] = layer;
            if (hwclayer && hwclayer->dev_cmptype != HWC2_COMPOSITION_CLIENT) {
                outLayerRequests[num_requests] =
                    (hwc2_layer_request_t)HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET;
            }
            num_requests++;
        }

        if (num_requests > 0) {
            if (dctx->num_lyrreqs == num_requests) {
                HWC_LOGDB("There are %d layer requests.", num_requests);
                *outNumElements = num_requests;
            } else {
                HWC_LOGEB("outNumElements:%d is not same as outNumTypes: %d.",
                    num_requests, dctx->num_lyrreqs);
            }
        } else {
            HWC_LOGDA("No layer requests.");
        }
        // dctx->num_lyrreqs = 0;
    }

    return HWC2_ERROR_NONE;
}

/* getDisplayType(..., outType)
 * Descriptor: HWC2_FUNCTION_GET_DISPLAY_TYPE
 * Must be provided by all HWC2 devices
 *
 * Returns whether the given display is a physical or virtual display.
 *
 * Parameters:
 *   outType - the type of the display; pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getDisplayType(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_display_type_t* outType) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    switch (display) {
        case HWC_DISPLAY_PRIMARY:
            *outType = HWC2_DISPLAY_TYPE_PHYSICAL;
        break;
        case HWC_DISPLAY_EXTERNAL:
            *outType = HWC2_DISPLAY_TYPE_PHYSICAL;
        break;
        case HWC_DISPLAY_VIRTUAL:
            *outType = HWC2_DISPLAY_TYPE_VIRTUAL;
        break;
        default:
            *outType = HWC2_DISPLAY_TYPE_INVALID;
            HWC_LOGEB("invalidate display %d", (int32_t)display);
        break;
    }

    return HWC2_ERROR_NONE;
}

/* getDozeSupport(..., outSupport)
 * Descriptor: HWC2_FUNCTION_GET_DOZE_SUPPORT
 * Must be provided by all HWC2 devices
 *
 * Returns whether the given display supports HWC2_POWER_MODE_DOZE and
 * HWC2_POWER_MODE_DOZE_SUSPEND. DOZE_SUSPEND may not provide any benefit over
 * DOZE (see the definition of hwc2_power_mode_t for more information), but if
 * both DOZE and DOZE_SUSPEND are no different from HWC2_POWER_MODE_ON, the
 * device should not claim support.
 *
 * Parameters:
 *   outSupport - whether the display supports doze modes (1 for yes, 0 for no);
 *       pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getDozeSupport(
        hwc2_device_t* device, hwc2_display_t display,
        int32_t* outSupport) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO: we do not support Doze for now.
    *outSupport = 0;

    return HWC2_ERROR_NONE;
}

/* getHdrCapabilities(..., outNumTypes, outTypes, outMaxLuminance,
 *     outMaxAverageLuminance, outMinLuminance)
 * Descriptor: HWC2_FUNCTION_GET_HDR_CAPABILITIES
 * Must be provided by all HWC2 devices
 *
 * Returns the high dynamic range (HDR) capabilities of the given display, which
 * are invariant with regard to the active configuration.
 *
 * Displays which are not HDR-capable must return no types in outTypes and set
 * outNumTypes to 0.
 *
 * If outTypes is NULL, the required number of HDR types must be returned in
 * outNumTypes.
 *
 * Parameters:
 *   outNumTypes - if outTypes was NULL, the number of types which would have
 *       been returned; if it was not NULL, the number of types stored in
 *       outTypes, which must not exceed the value stored in outNumTypes prior
 *       to the call; pointer will be non-NULL
 *   outTypes - an array of HDR types, may have 0 elements if the display is not
 *       HDR-capable
 *   outMaxLuminance - the desired content maximum luminance for this display in
 *       cd/m^2; pointer will be non-NULL
 *   outMaxAverageLuminance - the desired content maximum frame-average
 *       luminance for this display in cd/m^2; pointer will be non-NULL
 *   outMinLuminance - the desired content minimum luminance for this display in
 *       cd/m^2; pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getHdrCapabilities(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumTypes, int32_t* /*android_hdr_t*/ outTypes,
        float* outMaxLuminance, float* outMaxAverageLuminance,
        float* outMinLuminance) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO: currently not support Hdr Capabilityies on aml platform???
    *outNumTypes = 0;

    return HWC2_ERROR_NONE;

}

/* getReleaseFences(..., outNumElements, outLayers, outFences)
 * Descriptor: HWC2_FUNCTION_GET_RELEASE_FENCES
 * Must be provided by all HWC2 devices
 *
 * Retrieves the release fences for device layers on this display which will
 * receive new buffer contents this frame.
 *
 * A release fence is a file descriptor referring to a sync fence object which
 * will be signaled after the device has finished reading from the buffer
 * presented in the prior frame. This indicates that it is safe to start writing
 * to the buffer again. If a given layer's fence is not returned from this
 * function, it will be assumed that the buffer presented on the previous frame
 * is ready to be written.
 *
 * The fences returned by this function should be unique for each layer (even if
 * they point to the same underlying sync object), and ownership of the fences
 * is transferred to the client, which is responsible for closing them.
 *
 * If outLayers or outFences is NULL, the required number of layers and fences
 * must be returned in outNumElements.
 *
 * Parameters:
 *   outNumElements - if outLayers or outFences were NULL, the number of
 *       elements which would have been returned; if both were not NULL, the
 *       number of elements in outLayers and outFences, which must not exceed
 *       the value stored in outNumElements prior to the call; pointer will be
 *       non-NULL
 *   outLayers - an array of layer handles
 *   outFences - an array of sync fence file descriptors as described above,
 *       each corresponding to an element of outLayers
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 */
hwc2_error_t getReleaseFences(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumElements, hwc2_layer_t* outLayers,
        int32_t* outFences) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = NULL;
    hwc2_layer_t layer;
    uint32_t num_layer = 0;

    /*for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
        hwclayer = dctx->hwc_layer[i];
        if (hwclayer->layer_acquirefence > -1) {
            if (NULL == outLayers || NULL == outFences) {
                num_layer++;
            } else {
                outLayers[num_layer] = i;
                outFences[num_layer++] = hwclayer->layer_acquirefence;
            }
        }
    }*/

    /*if (display == HWC_DISPLAY_VIRTUAL) {
        if (NULL == outLayers || NULL == outFences) {
            HWC_LOGEA("No layer have set buffer yet.", dctx->tgr_acqfence);
        } else {
            HWC_LOGEA("No layer have set buffer yet.");
        }
    }*/

    if (NULL == outLayers || NULL == outFences) {
        for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
            hwclayer = dctx->hwc_layer[i];
            if (hwclayer && hwclayer->layer_acquirefence > -1) num_layer++;
        }
    } else {
        for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
            hwclayer = dctx->hwc_layer[i];
            if (hwclayer && hwclayer->layer_acquirefence > -1) {
                outLayers[num_layer] = i;
                outFences[num_layer++] = hwclayer->layer_acquirefence;
                hwclayer->layer_acquirefence = -1;
            }
        }
    }

    if (num_layer > 0) {
        HWC_LOGDB("There are %d layer requests.", num_layer);
        *outNumElements = num_layer;
    } else {
        HWC_LOGDA("No layer have set buffer yet.");
    }

    return HWC2_ERROR_NONE;
}

/* presentDisplay(..., outRetireFence)
 * Descriptor: HWC2_FUNCTION_PRESENT_DISPLAY
 * Must be provided by all HWC2 devices
 *
 * Presents the current display contents on the screen (or in the case of
 * virtual displays, into the output buffer).
 *
 * Prior to calling this function, the display must be successfully validated
 * with validateDisplay. Note that setLayerBuffer and setLayerSurfaceDamage
 * specifically do not count as layer state, so if there are no other changes
 * to the layer state (or to the buffer's properties as described in
 * setLayerBuffer), then it is safe to call this function without first
 * validating the display.
 *
 * If this call succeeds, outRetireFence will be populated with a file
 * descriptor referring to a retire sync fence object. For physical displays,
 * this fence will be signaled when the result of composition of the prior frame
 * is no longer necessary (because it has been copied or replaced by this
 * frame). For virtual displays, this fence will be signaled when writes to the
 * output buffer have completed and it is safe to read from it.
 *
 * Parameters:
 *   outRetireFence - a sync fence file descriptor as described above; pointer
 *       will be non-NULL
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_NO_RESOURCES - no valid output buffer has been set for a virtual
 *       display
 *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not successfully been called
 *       for this display
 */
int32_t presentDisplay(
        hwc2_device_t* device, hwc2_display_t display,
        int32_t* outRetireFence) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    int32_t err = HWC2_ERROR_NONE;

    if (display == HWC_DISPLAY_VIRTUAL) {
        // deal virtual display.
        if (dctx->connected) {
            if (!dctx->virhnd) {
                HWC_LOGEA("virtual display handle is null.");
                *outRetireFence = -1;
                return HWC2_ERROR_NO_RESOURCES;
            }
            if (private_handle_t::validate(dctx->virhnd) < 0)
                return HWC2_ERROR_NO_RESOURCES;

            if (dctx->tgr_acqfence) {
                sync_wait(dctx->tgr_acqfence, 500);
                close(dctx->tgr_acqfence);
                dctx->tgr_acqfence = -1;
            }
            *outRetireFence = dctx->vir_relfence;
        }
    } else {
        // deal with physical display.
        hwc_layer_t *hwclayer = NULL;

        // TODO: need improve the way to set video axis.
#if WITH_LIBPLAYER_MODULE
        for (uint32_t i=0; i<dctx->num_lyrreqs; i++) {
            hwc2_layer_t layer = dctx->requests_layer[i];
            hwclayer = dctx->hwc_layer[layer];
            if (hwclayer && hwclayer->dev_cmptype == HWC2_COMPOSITION_DEVICE) {
                hwc2_overlay_compose(ctx, display, hwclayer);
            }
        }
#endif
        err = hwc2_fb_post(ctx, display, outRetireFence);
    }
    // reset num_chgtyps & num_lyrreqs to 0.
    dctx->num_chgtyps = 0;
    dctx->num_lyrreqs = 0;

    // reset validated to false.
    // dctx->validated = false;
    return err;
}

/* setActiveConfig(..., config)
 * Descriptor: HWC2_FUNCTION_SET_ACTIVE_CONFIG
 * Must be provided by all HWC2 devices
 *
 * Sets the active configuration for this display. Upon returning, the given
 * display configuration should be active and remain so until either this
 * function is called again or the display is disconnected.
 *
 * Parameters:
 *   config - the new display configuration
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_CONFIG - the configuration handle passed in is not valid for
 *       this display
 */
hwc2_error_t setActiveConfig(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_config_t config) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO:
    return HWC2_ERROR_NONE;
}

/* setClientTarget(..., target, acquireFence, dataspace, damage)
 * Descriptor: HWC2_FUNCTION_SET_CLIENT_TARGET
 * Must be provided by all HWC2 devices
 *
 * Sets the buffer handle which will receive the output of client composition.
 * Layers marked as HWC2_COMPOSITION_CLIENT will be composited into this buffer
 * prior to the call to presentDisplay, and layers not marked as
 * HWC2_COMPOSITION_CLIENT should be composited with this buffer by the device.
 *
 * The buffer handle provided may be null if no layers are being composited by
 * the client. This must not result in an error (unless an invalid display
 * handle is also provided).
 *
 * Also provides a file descriptor referring to an acquire sync fence object,
 * which will be signaled when it is safe to read from the client target buffer.
 * If it is already safe to read from this buffer, -1 may be passed instead.
 * The device must ensure that it is safe for the client to close this file
 * descriptor at any point after this function is called.
 *
 * For more about dataspaces, see setLayerDataspace.
 *
 * The damage parameter describes a surface damage region as defined in the
 * description of setLayerSurfaceDamage.
 *
 * Will be called before presentDisplay if any of the layers are marked as
 * HWC2_COMPOSITION_CLIENT. If no layers are so marked, then it is not
 * necessary to call this function. It is not necessary to call validateDisplay
 * after changing the target through this function.
 *
 * Parameters:
 *   target - the new target buffer
 *   acquireFence - a sync fence file descriptor as described above
 *   dataspace - the dataspace of the buffer, as described in setLayerDataspace
 *   damage - the surface damage region
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - the new target handle was invalid
 */
hwc2_error_t setClientTarget(
        hwc2_device_t* device, hwc2_display_t display,
        buffer_handle_t target, int32_t acquireFence,
        android_dataspace_t dataspace, hwc_region_t damage) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

	if (target && private_handle_t::validate(target) < 0) {
		return HWC2_ERROR_BAD_PARAMETER;
	}

    if (NULL != target) {
        dctx->clnt_tgrhnd = target;
        dctx->clnt_tgrdmge = damage;
        if (-1 != acquireFence) {
            dctx->tgr_acqfence = acquireFence;
            // wait acquireFence to be signaled, wait 5s.
            // sync_wait(acquireFence, 5000);
        }
        // TODO: HWC2_ERROR_BAD_PARAMETER && dataspace && damage.
    } else {
        HWC_LOGDA("client target is null!, no need to update this frame.");
    }

    return HWC2_ERROR_NONE;
}

/* setColorMode(..., mode)
 * Descriptor: HWC2_FUNCTION_SET_COLOR_MODE
 * Must be provided by all HWC2 devices
 *
 * Sets the color mode of the given display.
 *
 * Upon returning from this function, the color mode change must have fully
 * taken effect.
 *
 * The valid color modes can be found in android_color_mode_t in
 * <system/graphics.h>. All HWC2 devices must support at least
 * HAL_COLOR_MODE_NATIVE, and displays are assumed to be in this mode upon
 * hotplug.
 *
 * Parameters:
 *   mode - the mode to set
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - mode is not a valid color mode
 *   HWC2_ERROR_UNSUPPORTED - mode is not supported on this display
 */
hwc2_error_t setColorMode(
        hwc2_device_t* device, hwc2_display_t display,
        int32_t /*android_color_mode_t*/ mode) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO: return HWC2_ERROR_BAD_PARAMETER && HWC2_ERROR_UNSUPPORTED;
    dctx->color_mode = mode;
    return HWC2_ERROR_NONE;
}

/* setColorTransform(..., matrix, hint)
 * Descriptor: HWC2_FUNCTION_SET_COLOR_TRANSFORM
 * Must be provided by all HWC2 devices
 *
 * Sets a color transform which will be applied after composition.
 *
 * If hint is not HAL_COLOR_TRANSFORM_ARBITRARY, then the device may use the
 * hint to apply the desired color transform instead of using the color matrix
 * directly.
 *
 * If the device is not capable of either using the hint or the matrix to apply
 * the desired color transform, it should force all layers to client composition
 * during validateDisplay.
 *
 * The matrix provided is an affine color transformation of the following form:
 *
 * |r.r r.g r.b 0|
 * |g.r g.g g.b 0|
 * |b.r b.g b.b 0|
 * |Tr  Tg  Tb  1|
 *
 * This matrix will be provided in row-major form: {r.r, r.g, r.b, 0, g.r, ...}.
 *
 * Given a matrix of this form and an input color [R_in, G_in, B_in], the output
 * color [R_out, G_out, B_out] will be:
 *
 * R_out = R_in * r.r + G_in * g.r + B_in * b.r + Tr
 * G_out = R_in * r.g + G_in * g.g + B_in * b.g + Tg
 * B_out = R_in * r.b + G_in * g.b + B_in * b.b + Tb
 *
 * Parameters:
 *   matrix - a 4x4 transform matrix (16 floats) as described above
 *   hint - a hint value which may be used instead of the given matrix unless it
 *       is HAL_COLOR_TRANSFORM_ARBITRARY
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - hint is not a valid color transform hint
 */
hwc2_error_t setColorTransform(
        hwc2_device_t* device, hwc2_display_t display,
        const float* matrix, int32_t /*android_color_transform_t*/ hint) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    // TODO: return HWC2_ERROR_BAD_PARAMETER;

    return HWC2_ERROR_NONE;
}

/* setOutputBuffer(..., buffer, releaseFence)
 * Descriptor: HWC2_FUNCTION_SET_OUTPUT_BUFFER
 * Must be provided by all HWC2 devices
 *
 * Sets the output buffer for a virtual display. That is, the buffer to which
 * the composition result will be written.
 *
 * Also provides a file descriptor referring to a release sync fence object,
 * which will be signaled when it is safe to write to the output buffer. If it
 * is already safe to write to the output buffer, -1 may be passed instead. The
 * device must ensure that it is safe for the client to close this file
 * descriptor at any point after this function is called.
 *
 * Must be called at least once before presentDisplay, but does not have any
 * interaction with layer state or display validation.
 *
 * Parameters:
 *   buffer - the new output buffer
 *   releaseFence - a sync fence file descriptor as described above
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - the new output buffer handle was invalid
 *   HWC2_ERROR_UNSUPPORTED - display does not refer to a virtual display
 */
hwc2_error_t setOutputBuffer(
        hwc2_device_t* device, hwc2_display_t display,
        buffer_handle_t buffer, int32_t releaseFence) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *vctx = &(ctx->displays[HWC_DISPLAY_VIRTUAL]);

    if (display != HWC_DISPLAY_VIRTUAL) {
        HWC_LOGEB("Should be a virtual display: %d", (int32_t)display);
        return HWC2_ERROR_UNSUPPORTED;
    }

    if (vctx->connected) {
        if (buffer && private_handle_t::validate(buffer) < 0) {
            HWC_LOGEA("buffer handle is invalid");
            return HWC2_ERROR_BAD_PARAMETER;
        }

        if (NULL != buffer) {
            vctx->virhnd = buffer;
            vctx->vir_relfence = releaseFence;
        } else {
            HWC_LOGDA("Virtual Display output buffer target is null!, no need to update this frame.");
        }
    }
    // TODO: do something?
    return HWC2_ERROR_NONE;
}

/* setPowerMode(..., mode)
 * Descriptor: HWC2_FUNCTION_SET_POWER_MODE
 * Must be provided by all HWC2 devices
 *
 * Sets the power mode of the given display. The transition must be complete
 * when this function returns. It is valid to call this function multiple times
 * with the same power mode.
 *
 * All displays must support HWC2_POWER_MODE_ON and HWC2_POWER_MODE_OFF. Whether
 * a display supports HWC2_POWER_MODE_DOZE or HWC2_POWER_MODE_DOZE_SUSPEND may
 * be queried using getDozeSupport.
 *
 * Parameters:
 *   mode - the new power mode
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - mode was not a valid power mode
 *   HWC2_ERROR_UNSUPPORTED - mode was a valid power mode, but is not supported
 *       on this display
 */
hwc2_error_t setPowerMode(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_power_mode_t mode) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    if (mode < HWC2_POWER_MODE_OFF || mode > HWC2_POWER_MODE_ON) {
        HWC_LOGEB("setPowerMode bad parameter: %d", mode);
        return HWC2_ERROR_BAD_PARAMETER;
    }

    return HWC2_ERROR_NONE;
}

/* setVsyncEnabled(..., enabled)
 * Descriptor: HWC2_FUNCTION_SET_VSYNC_ENABLED
 * Must be provided by all HWC2 devices
 *
 * Enables or disables the vsync signal for the given display. Virtual displays
 * never generate vsync callbacks, and any attempt to enable vsync for a virtual
 * display though this function must return HWC2_ERROR_NONE and have no other
 * effect.
 *
 * Parameters:
 *   enabled - whether to enable or disable vsync
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - enabled was an invalid value
 */
hwc2_error_t setVsyncEnabled(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_vsync_t enabled) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    if (HWC_DISPLAY_VIRTUAL == display)
        return HWC2_ERROR_NONE;

    switch (enabled) {
        case HWC2_VSYNC_ENABLE:
            dctx->vsync_enable = true;
            break;
        case HWC2_VSYNC_DISABLE:
            dctx->vsync_enable = false;
            break;
        case HWC2_VSYNC_INVALID:
        default:
            ALOGE("setVsyncEnabled bad parameter: %d", enabled);
            return HWC2_ERROR_BAD_PARAMETER;
    }

    pthread_mutex_lock(&hwc_mutex[display]);
    pthread_cond_signal(&hwc_cond[display]);
    pthread_mutex_unlock(&hwc_mutex[display]);

    return HWC2_ERROR_NONE;
}

/* validateDisplay(..., outNumTypes, outNumRequests)
 * Descriptor: HWC2_FUNCTION_VALIDATE_DISPLAY
 * Must be provided by all HWC2 devices
 *
 * Instructs the device to inspect all of the layer state and determine if
 * there are any composition type changes necessary before presenting the
 * display. Permitted changes are described in the definition of
 * hwc2_composition_t above.
 *
 * Also returns the number of layer requests required
 * by the given layer configuration.
 *
 * Parameters:
 *   outNumTypes - the number of composition type changes required by the
 *       device; if greater than 0, the client must either set and validate new
 *       types, or call acceptDisplayChanges to accept the changes returned by
 *       getChangedCompositionTypes; must be the same as the number of changes
 *       returned by getChangedCompositionTypes (see the declaration of that
 *       function for more information); pointer will be non-NULL
 *   outNumRequests - the number of layer requests required by this layer
 *       configuration; must be equal to the number of layer requests returned
 *       by getDisplayRequests (see the declaration of that function for
 *       more information); pointer will be non-NULL
 *
 * Returns HWC2_ERROR_NONE if no changes are necessary and it is safe to present
 * the display using the current layer state. Otherwise returns one of the
 * following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_HAS_CHANGES - outNumTypes was greater than 0 (see parameter list
 *       for more information)
 */
hwc2_error_t validateDisplay(
        hwc2_device_t* device, hwc2_display_t display,
        uint32_t* outNumTypes, uint32_t* outNumRequests) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = NULL;
    uint32_t num_type = 0, num_requests = 0;

    for (uint32_t i=0; i<HWC2_MAX_LAYERS; i++) {
        hwclayer = dctx->hwc_layer[i];
        if (hwclayer) {
            if (display != HWC_DISPLAY_VIRTUAL) {
                // Physical Display.
                if (hwclayer->clt_cmptype == HWC2_COMPOSITION_DEVICE) {
                    // video overlay.
                    if (hwclayer->buf_hnd) {
                        private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(hwclayer->buf_hnd);
                        if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY) {
                            hwclayer->dev_cmptype = HWC2_COMPOSITION_DEVICE;
                            //dctx->types_layer[num_type] = i;
                            dctx->requests_layer[num_requests] = i;
                            //num_type++; num_requests++;
                            num_requests++;
                            continue;
                        }
                    }

                    // change all other device type to client.
                    hwclayer->dev_cmptype = HWC2_COMPOSITION_CLIENT;
                    dctx->types_layer[num_type] = i;
                    num_type++;
                    continue;
                }

                // cursor layer.
#if ENABLE_CURSOR_LAYER
                if (hwclayer->clt_cmptype == HWC2_COMPOSITION_CURSOR) {
                    HWC_LOGDA("This is a Cursor layer!");
                    if (display == HWC_DISPLAY_VIRTUAL && dctx->virhnd
                        && private_handle_t::validate(dctx->virhnd) >=0) {
                        hwclayer->dev_cmptype = HWC2_COMPOSITION_CLIENT;
                        dctx->types_layer[num_type] = i;
                        num_type++;
                    } else {
                        hwclayer->dev_cmptype = HWC2_COMPOSITION_CURSOR;
                        dctx->requests_layer[num_requests] = i;
                        num_requests++;
                    }
                    continue;
                }
#endif

                // sideband stream.
                if (hwclayer->clt_cmptype == HWC2_COMPOSITION_SIDEBAND && hwclayer->sideband_stream) {
                    // TODO: we just transact SIDEBAND to OVERLAY for now;
                    HWC_LOGDA("get HWC_SIDEBAND layer, just change to overlay");
                    hwclayer->dev_cmptype = HWC2_COMPOSITION_DEVICE;
                    dctx->types_layer[num_type] = i;
                    dctx->requests_layer[num_requests] = i;
                    num_type++; num_requests++;
                    continue;
                }

                // TODO: solid color.
                if (hwclayer->clt_cmptype == HWC2_COMPOSITION_SOLID_COLOR) {
                    HWC_LOGDA("This is a Solid Color layer!");
                    hwclayer->dev_cmptype = HWC2_COMPOSITION_SOLID_COLOR;
                    dctx->requests_layer[num_requests] = i;
                    num_requests++;
                    continue;
                }
            } else {
                // Virtual Display.
                if (dctx->virhnd && private_handle_t::validate(dctx->virhnd) >=0) {
                    if (hwclayer->clt_cmptype != HWC2_COMPOSITION_CLIENT) {
                        // change all other device type to client.
                        hwclayer->dev_cmptype = HWC2_COMPOSITION_CLIENT;
                        dctx->types_layer[num_type] = i;
                        num_type++;
                        continue;
                    }
                }
            }
        }
    }

    if (num_requests > 0) {
        HWC_LOGDB("There are %d layer requests.", num_requests);
        *outNumRequests = dctx->num_lyrreqs = num_requests;
    }

    // mark the validate function is called.(???)
    // dctx->validated = true;
    if (num_type > 0) {
        HWC_LOGDB("there are %d layer types has changed.", num_type);
        *outNumTypes = dctx->num_chgtyps= num_type;
        return HWC2_ERROR_HAS_CHANGES;
    }

    return HWC2_ERROR_NONE;
}

/*
 * Layer Functions
 *
 * These are functions which operate on layers, but which do not modify state
 * that must be validated before use. See also 'Layer State Functions' below.
 *
 * All of these functions take as their first three parameters a device pointer,
 * a display handle for the display which contains the layer, and a layer
 * handle, so these parameters are omitted from the described parameter lists.
 */

/* setCursorPosition(..., x, y)
 * Descriptor: HWC2_FUNCTION_SET_CURSOR_POSITION
 * Must be provided by all HWC2 devices
 *
 * Asynchonously sets the position of a cursor layer.
 *
 * Prior to validateDisplay, a layer may be marked as HWC2_COMPOSITION_CURSOR.
 * If validation succeeds (i.e., the device does not request a composition
 * change for that layer), then once a buffer has been set for the layer and it
 * has been presented, its position may be set by this function at any time
 * between presentDisplay and any subsequent validateDisplay calls for this
 * display.
 *
 * Once validateDisplay is called, this function will not be called again until
 * the validate/present sequence is completed.
 *
 * May be called from any thread so long as it is not interleaved with the
 * validate/present sequence as described above.
 *
 * Parameters:
 *   x - the new x coordinate (in pixels from the left of the screen)
 *   y - the new y coordinate (in pixels from the top of the screen)
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
 *   HWC2_ERROR_BAD_LAYER - the layer is invalid or is not currently marked as
 *       HWC2_COMPOSITION_CURSOR
 *   HWC2_ERROR_NOT_VALIDATED - the device is currently in the middle of the
 *       validate/present sequence
 */
hwc2_error_t setCursorPosition(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, int32_t x, int32_t y) {
#if ENABLE_CURSOR_LAYER
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];
    struct cursor_context_t *cursor_ctx = &(dctx->cursor_ctx);
    struct framebuffer_info_t *cbinfo = &(cursor_ctx->cb_info);
    struct fb_cursor cinfo;

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    if (HWC2_COMPOSITION_CURSOR != hwclayer->clt_cmptype)
        return HWC2_ERROR_BAD_LAYER;

    // TODO: ...HWC2_ERROR_NOT_VALIDATED?

    if (cbinfo->fd < 0) {
        HWC_LOGEB("hwc_setCursorPositionAsync fd=%d", cbinfo->fd );
    }else {
        cinfo.hot.x = x;
        cinfo.hot.y = y;
        if (display == HWC_DISPLAY_PRIMARY) {
            HWC_LOGDB("hwc_setCursorPositionAsync x_pos=%d, y_pos=%d", cinfo.hot.x, cinfo.hot.y);
            ioctl(cbinfo->fd, FBIO_CURSOR, &cinfo);
        } else if(display == HWC_DISPLAY_EXTERNAL) {
            // TODO:
            HWC_LOGDA("hwc_setCursorPositionAsync external display need show cursor too! ");
            //ioctl(cbinfo->fd, FBIO_CURSOR, &cinfo);
        }
    }
#endif
    return HWC2_ERROR_NONE;
}

/* setLayerBuffer(..., buffer, acquireFence)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_BUFFER
 * Must be provided by all HWC2 devices
 *
 * Sets the buffer handle to be displayed for this layer. If the buffer
 * properties set at allocation time (width, height, format, and usage) have not
 * changed since the previous frame, it is not necessary to call validateDisplay
 * before calling presentDisplay unless new state needs to be validated in the
 * interim.
 *
 * Also provides a file descriptor referring to an acquire sync fence object,
 * which will be signaled when it is safe to read from the given buffer. If it
 * is already safe to read from the buffer, -1 may be passed instead. The
 * device must ensure that it is safe for the client to close this file
 * descriptor at any point after this function is called.
 *
 * This function must return HWC2_ERROR_NONE and have no other effect if called
 * for a layer with a composition type of HWC2_COMPOSITION_SOLID_COLOR (because
 * it has no buffer) or HWC2_COMPOSITION_SIDEBAND or HWC2_COMPOSITION_CLIENT
 * (because synchronization and buffer updates for these layers are handled
 * elsewhere).
 *
 * Parameters:
 *   buffer - the buffer handle to set
 *   acquireFence - a sync fence file descriptor as described above
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - the buffer handle passed in was invalid
 */
hwc2_error_t setLayerBuffer(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, buffer_handle_t buffer,
        int32_t acquireFence) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // Bad parameter
	if (buffer && private_handle_t::validate(buffer) < 0)
		return HWC2_ERROR_BAD_PARAMETER;

    if (NULL != buffer) {
        hwclayer->buf_hnd = buffer;
        if (-1 != acquireFence) {
            hwclayer->layer_acquirefence = acquireFence;
            // wait acquireFence to be signaled, wait 5s.
            // sync_wait(acquireFence, 5000);
        }
    } else {
        HWC_LOGDA("Layer buffer is null! no need to update this layer.");
    }

    return HWC2_ERROR_NONE;
}

/* setLayerSurfaceDamage(..., damage)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE
 * Must be provided by all HWC2 devices
 *
 * Provides the region of the source buffer which has been modified since the
 * last frame. This region does not need to be validated before calling
 * presentDisplay.
 *
 * Once set through this function, the damage region remains the same until a
 * subsequent call to this function.
 *
 * If damage.numRects > 0, then it may be assumed that any portion of the source
 * buffer not covered by one of the rects has not been modified this frame. If
 * damage.numRects == 0, then the whole source buffer must be treated as if it
 * has been modified.
 *
 * If the layer's contents are not modified relative to the prior frame, damage
 * will contain exactly one empty rect([0, 0, 0, 0]).
 *
 * The damage rects are relative to the pre-transformed buffer, and their origin
 * is the top-left corner. They will not exceed the dimensions of the latched
 * buffer.
 *
 * Parameters:
 *   damage - the new surface damage region
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerSurfaceDamage(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_region_t damage) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // TODO: still have some work to do here.
    hwclayer->demage_region = damage;
    return HWC2_ERROR_NONE;
}

/*
 * Layer State Functions
 *
 * These functions modify the state of a given layer. They do not take effect
 * until the display configuration is successfully validated with
 * validateDisplay and the display contents are presented with presentDisplay.
 *
 * All of these functions take as their first three parameters a device pointer,
 * a display handle for the display which contains the layer, and a layer
 * handle, so these parameters are omitted from the described parameter lists.
 */

/* setLayerBlendMode(..., mode)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_BLEND_MODE
 * Must be provided by all HWC2 devices
 *
 * Sets the blend mode of the given layer.
 *
 * Parameters:
 *   mode - the new blend mode
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - an invalid blend mode was passed in
 */
hwc2_error_t setLayerBlendMode(
        hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
        hwc2_blend_mode_t mode) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    if (mode < HWC2_BLEND_MODE_INVALID
        || mode > HWC2_BLEND_MODE_COVERAGE)
        return HWC2_ERROR_BAD_PARAMETER;

    hwclayer->blend_mode= mode;
    return HWC2_ERROR_NONE;
}

/* setLayerColor(..., color)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_COLOR
 * Must be provided by all HWC2 devices
 *
 * Sets the color of the given layer. If the composition type of the layer is
 * not HWC2_COMPOSITION_SOLID_COLOR, this call must return HWC2_ERROR_NONE and
 * have no other effect.
 *
 * Parameters:
 *   color - the new color
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerColor(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_color_t color) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->color = color;
    return HWC2_ERROR_NONE;
}

/* setLayerCompositionType(..., type)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE
 * Must be provided by all HWC2 devices
 *
 * Sets the desired composition type of the given layer. During validateDisplay,
 * the device may request changes to the composition types of any of the layers
 * as described in the definition of hwc2_composition_t above.
 *
 * Parameters:
 *   type - the new composition type
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - an invalid composition type was passed in
 *   HWC2_ERROR_UNSUPPORTED - a valid composition type was passed in, but it is
 *       not supported by this device
 */
hwc2_error_t setLayerCompositionType(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc2_composition_t type) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // Bad parameter.
    if (type < HWC2_COMPOSITION_INVALID
        || type > HWC2_COMPOSITION_SIDEBAND)
        return HWC2_ERROR_BAD_PARAMETER;

    // Do not support solid color for now.
    if (type == HWC2_COMPOSITION_SOLID_COLOR)
        return HWC2_ERROR_UNSUPPORTED;

    hwclayer->clt_cmptype = type;
    return HWC2_ERROR_NONE;
}

/* setLayerDataspace(..., dataspace)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_DATASPACE
 * Must be provided by all HWC2 devices
 *
 * Sets the dataspace that the current buffer on this layer is in.
 *
 * The dataspace provides more information about how to interpret the buffer
 * contents, such as the encoding standard and color transform.
 *
 * See the values of android_dataspace_t in <system/graphics.h> for more
 * information.
 *
 * Parameters:
 *   dataspace - the new dataspace
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerDataspace(
        hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
        int32_t /*android_dataspace_t*/ dataspace) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->dataspace = dataspace;
    return HWC2_ERROR_NONE;
}

/* setLayerDisplayFrame(..., frame)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME
 * Must be provided by all HWC2 devices
 *
 * Sets the display frame (the portion of the display covered by a layer) of the
 * given layer. This frame will not exceed the display dimensions.
 *
 * Parameters:
 *   frame - the new display frame
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerDisplayFrame(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_rect_t frame) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->display_frame = frame;
    return HWC2_ERROR_NONE;
}

/* setLayerPlaneAlpha(..., alpha)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA
 * Must be provided by all HWC2 devices
 *
 * Sets an alpha value (a floating point value in the range [0.0, 1.0]) which
 * will be applied to the whole layer. It can be conceptualized as a
 * preprocessing step which applies the following function:
 *   if (blendMode == HWC2_BLEND_MODE_PREMULTIPLIED)
 *       out.rgb = in.rgb * planeAlpha
 *   out.a = in.a * planeAlpha
 *
 * If the device does not support this operation on a layer which is marked
 * HWC2_COMPOSITION_DEVICE, it must request a composition type change to
 * HWC2_COMPOSITION_CLIENT upon the next validateDisplay call.
 *
 * Parameters:
 *   alpha - the plane alpha value to apply
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerPlaneAlpha(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, float alpha) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->alpha = alpha;
    return HWC2_ERROR_NONE;
}

/* setLayerSidebandStream(..., stream)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM
 * Provided by HWC2 devices which support HWC2_CAPABILITY_SIDEBAND_STREAM
 *
 * Sets the sideband stream for this layer. If the composition type of the given
 * layer is not HWC2_COMPOSITION_SIDEBAND, this call must return HWC2_ERROR_NONE
 * and have no other effect.
 *
 * Parameters:
 *   stream - the new sideband stream
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - an invalid sideband stream was passed in
 */
hwc2_error_t setLayerSidebandStream(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, const native_handle_t* stream) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // Bad parameter.
    if (NULL == stream)
        return HWC2_ERROR_BAD_PARAMETER;

    hwclayer->sideband_stream = stream;
    return HWC2_ERROR_NONE;
}

/* setLayerSourceCrop(..., crop)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_SOURCE_CROP
 * Must be provided by all HWC2 devices
 *
 * Sets the source crop (the portion of the source buffer which will fill the
 * display frame) of the given layer. This crop rectangle will not exceed the
 * dimensions of the latched buffer.
 *
 * If the device is not capable of supporting a true float source crop (i.e., it
 * will truncate or round the floats to integers), it should set this layer to
 * HWC2_COMPOSITION_CLIENT when crop is non-integral for the most accurate
 * rendering.
 *
 * If the device cannot support float source crops, but still wants to handle
 * the layer, it should use the following code (or similar) to convert to
 * an integer crop:
 *   intCrop.left = (int) ceilf(crop.left);
 *   intCrop.top = (int) ceilf(crop.top);
 *   intCrop.right = (int) floorf(crop.right);
 *   intCrop.bottom = (int) floorf(crop.bottom);
 *
 * Parameters:
 *   crop - the new source crop
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerSourceCrop(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_frect_t crop) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // TODO: still have some work to do.
    hwclayer->source_crop = crop;
    return HWC2_ERROR_NONE;
}

/* setLayerTransform(..., transform)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_TRANSFORM
 * Must be provided by all HWC2 devices
 *
 * Sets the transform (rotation/flip) of the given layer.
 *
 * Parameters:
 *   transform - the new transform
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 *   HWC2_ERROR_BAD_PARAMETER - an invalid transform was passed in
 */
hwc2_error_t setLayerTransform(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_transform_t transform) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    // Bad parameter.
    if (transform < 0 || transform > HWC_TRANSFORM_ROT_270) {
        return HWC2_ERROR_BAD_PARAMETER;
    }

    hwclayer->transform = transform;
    return HWC2_ERROR_NONE;

}

/* setLayerVisibleRegion(..., visible)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION
 * Must be provided by all HWC2 devices
 *
 * Specifies the portion of the layer that is visible, including portions under
 * translucent areas of other layers. The region is in screen space, and will
 * not exceed the dimensions of the screen.
 *
 * Parameters:
 *   visible - the new visible region, in screen space
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerVisibleRegion(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, hwc_region_t visible) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->visible_region = visible;
    return HWC2_ERROR_NONE;
}

/* setLayerZOrder(..., z)
 * Descriptor: HWC2_FUNCTION_SET_LAYER_Z_ORDER
 * Must be provided by all HWC2 devices
 *
 * Sets the desired Z order (height) of the given layer. A layer with a greater
 * Z value occludes a layer with a lesser Z value.
 *
 * Parameters:
 *   z - the new Z order
 *
 * Returns HWC2_ERROR_NONE or one of the following errors:
 *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
 */
hwc2_error_t setLayerZOrder(
        hwc2_device_t* device, hwc2_display_t display,
        hwc2_layer_t layer, uint32_t z) {
    hwc2_context_t *ctx = (hwc2_context_t*)device;
    display_context_t *dctx = &(ctx->displays[display]);
    isValidDisplay(dctx, display);

    hwc_layer_t *hwclayer = dctx->hwc_layer[layer];

    // Bad layer.
    if (NULL == hwclayer)
        return HWC2_ERROR_BAD_LAYER;

    hwclayer->zorder = z;
    return HWC2_ERROR_NONE;
}

/* getCapabilities(..., outCount, outCapabilities)
 *
 * Provides a list of capabilities (described in the definition of
 * hwc2_capability_t above) supported by this device. This list must
 * not change after the device has been loaded.
 *
 * Parameters:
 *   outCount - if outCapabilities was NULL, the number of capabilities
 *       which would have been returned; if outCapabilities was not NULL,
 *       the number of capabilities returned, which must not exceed the
 *       value stored in outCount prior to the call
 *   outCapabilities - a list of capabilities supported by this device; may
 *       be NULL, in which case this function must write into outCount the
 *       number of capabilities which would have been written into
 *       outCapabilities
 */
void hwc2_getCapabilities(struct hwc2_device* device, uint32_t* outCount,
        int32_t* /*hwc2_capability_t*/ outCapabilities) {
    if (NULL == outCapabilities) {
        *outCount = 1;
    } else {
        *outCount = 1;
        outCapabilities[0] = HWC2_CAPABILITY_SIDEBAND_STREAM;
    }
}

hwc2_function_pointer_t hwc2_getFunction(struct hwc2_device* device,
        int32_t /*hwc2_function_descriptor_t*/ descriptor) {
    switch (descriptor) {
        // Device functions
        case HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(createVirtualDisplay);
        case HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(destroyVirtualDisplay);
        case HWC2_FUNCTION_DUMP:
            return reinterpret_cast<hwc2_function_pointer_t>(dump);
        case HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT:
            return reinterpret_cast<hwc2_function_pointer_t>(getMaxVirtualDisplayCount);
        case HWC2_FUNCTION_REGISTER_CALLBACK:
            return reinterpret_cast<hwc2_function_pointer_t>(registerCallback);

        // Display functions
        case HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES:
            return reinterpret_cast<hwc2_function_pointer_t>(acceptDisplayChanges);
        case HWC2_FUNCTION_CREATE_LAYER:
            return reinterpret_cast<hwc2_function_pointer_t>(createLayer);
        case HWC2_FUNCTION_DESTROY_LAYER:
            return reinterpret_cast<hwc2_function_pointer_t>(destroyLayer);
        case HWC2_FUNCTION_GET_ACTIVE_CONFIG:
            return reinterpret_cast<hwc2_function_pointer_t>(getActiveConfig);
        case HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES:
            return reinterpret_cast<hwc2_function_pointer_t>(getChangedCompositionTypes);
        case HWC2_FUNCTION_GET_COLOR_MODES:
            return reinterpret_cast<hwc2_function_pointer_t>(getColorModes);
        case HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE:
            return reinterpret_cast<hwc2_function_pointer_t>(getDisplayAttribute);
        case HWC2_FUNCTION_GET_DISPLAY_CONFIGS:
            return reinterpret_cast<hwc2_function_pointer_t>(getDisplayConfigs);
        case HWC2_FUNCTION_GET_DISPLAY_NAME:
            return reinterpret_cast<hwc2_function_pointer_t>(getDisplayName);
        case HWC2_FUNCTION_GET_DISPLAY_REQUESTS:
            return reinterpret_cast<hwc2_function_pointer_t>(getDisplayRequests);
        case HWC2_FUNCTION_GET_DISPLAY_TYPE:
            return reinterpret_cast<hwc2_function_pointer_t>(getDisplayType);
        case HWC2_FUNCTION_GET_DOZE_SUPPORT:
            return reinterpret_cast<hwc2_function_pointer_t>(getDozeSupport);
        case HWC2_FUNCTION_GET_HDR_CAPABILITIES:
            return reinterpret_cast<hwc2_function_pointer_t>(getHdrCapabilities);
        case HWC2_FUNCTION_GET_RELEASE_FENCES:
            return reinterpret_cast<hwc2_function_pointer_t>(getReleaseFences);
        case HWC2_FUNCTION_PRESENT_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(presentDisplay);
        case HWC2_FUNCTION_SET_ACTIVE_CONFIG:
            return reinterpret_cast<hwc2_function_pointer_t>(setActiveConfig);
        case HWC2_FUNCTION_SET_CLIENT_TARGET:
            return reinterpret_cast<hwc2_function_pointer_t>(setClientTarget);
        case HWC2_FUNCTION_SET_COLOR_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(setColorMode);
        case HWC2_FUNCTION_SET_COLOR_TRANSFORM:
            return reinterpret_cast<hwc2_function_pointer_t>(setColorTransform);
        case HWC2_FUNCTION_SET_OUTPUT_BUFFER:
            return reinterpret_cast<hwc2_function_pointer_t>(setOutputBuffer);
        case HWC2_FUNCTION_SET_POWER_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(setPowerMode);
        case HWC2_FUNCTION_SET_VSYNC_ENABLED:
            return reinterpret_cast<hwc2_function_pointer_t>(setVsyncEnabled);
        case HWC2_FUNCTION_VALIDATE_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(validateDisplay);

        // Layer functions
        case HWC2_FUNCTION_SET_CURSOR_POSITION:
            return reinterpret_cast<hwc2_function_pointer_t>(setCursorPosition);
        case HWC2_FUNCTION_SET_LAYER_BUFFER:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerBuffer);
        case HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerSurfaceDamage);

        // Layer state functions
        case HWC2_FUNCTION_SET_LAYER_BLEND_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerBlendMode);
        case HWC2_FUNCTION_SET_LAYER_COLOR:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerColor);
        case HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerCompositionType);
        case HWC2_FUNCTION_SET_LAYER_DATASPACE:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerDataspace);
        case HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerDisplayFrame);
        case HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerPlaneAlpha);
        case HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerSidebandStream);
        case HWC2_FUNCTION_SET_LAYER_SOURCE_CROP:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerSourceCrop);
        case HWC2_FUNCTION_SET_LAYER_TRANSFORM:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerTransform);
        case HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerVisibleRegion);
        case HWC2_FUNCTION_SET_LAYER_Z_ORDER:
            return reinterpret_cast<hwc2_function_pointer_t>(setLayerZOrder);
        default:
            ALOGE("getFunction: Unknown function descriptor: %d", descriptor);
            return NULL;
    }
}

static int32_t hwc2_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device) {
    int32_t ret;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) return -EINVAL;

    hwc2_context_t *dev;
    dev = (hwc2_context_t *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
        (const struct hw_module_t **)&dev->gralloc_module)) {
        HWC_LOGEA("failed to get gralloc hw module");
        ret = -EINVAL;
        goto err_get_module;
    }

    //init primiary display
    //default is alwasy false,will check it in hot plug.
    init_display(dev, HWC_DISPLAY_PRIMARY);

    // willchanged to use hw vsync.
    dev->displays[HWC_DISPLAY_PRIMARY].vsync_period = chk_output_mode(dev->mode);

    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = HWC_DEVICE_API_VERSION_2_0;
    dev->base.common.module = const_cast<hw_module_t *>(module);
    dev->base.common.close = hwc2_close;

    dev->base.getFunction = hwc2_getFunction;
    dev->base.getCapabilities = hwc2_getCapabilities;

    dev->displays[HWC_DISPLAY_PRIMARY].vsync_enable = false;
    dev->displays[HWC_DISPLAY_EXTERNAL].vsync_enable = false;
    dev->blank_status = false;
    *device = &dev->base.common;

    ret = pthread_create(&dev->primary_vsync_thread, NULL, hwc2_primary_vsync_thread, dev);
    if (ret) {
        HWC_LOGEB("failed to start primary vsync thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }

    ret = pthread_create(&dev->external_vsync_thread, NULL, hwc2_external_vsync_thread, dev);
    if (ret) {
        HWC_LOGEB("failed to start external vsync thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }

    //temp solution, will change to use uevnet from kernel
    ret = pthread_create(&dev->hotplug_thread, NULL, hwc2_hotplug_thread, dev);
    if (ret) {
        HWC_LOGEB("failed to start hotplug thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }
    return 0;

err_vsync:
    uninit_display(dev,HWC_DISPLAY_PRIMARY);
err_get_module:
    if (dev) free(dev);
    return ret;
}

static struct hw_module_methods_t hwc2_module_methods = {
    .open = hwc2_device_open
};

hwc2_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 2,
        .version_minor = 0,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "hwcomposer2 module",
        .author = "Amlogic",
        .methods = &hwc2_module_methods,
        .dso = NULL,
        .reserved = {0},
    }
 };

