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
#define LOG_TAG "HWComposer"
#include <hardware/hardware.h>

#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/resource.h>

#include <EGL/egl.h>

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <cutils/compiler.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <utils/String8.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer.h>
#include <hardware_legacy/uevent.h>
#include <utils/String8.h>

#include <EGL/egl.h>
#include <utils/Vector.h>
#include <utils/Timers.h>
// for private_handle_t
#include <gralloc_priv.h>
#include <gralloc_helper.h>

#if WITH_LIBPLAYER_MODULE
#include <Amavutils.h>
#endif
#include "tvp/OmxUtil.h"

#include <system/graphics.h>

#include <sync/sync.h>

#ifndef LOGD
#define LOGD ALOGD
#endif

static bool chk_bool_prop(const char* prop){
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "false") && strcmp(val, "true") == 0) {
        ALOGD("prop: %s is %s",prop, val);
        return true;
    }

    return false;
}

static int chk_int_prop(const char* prop){
    char val[PROPERTY_VALUE_MAX];

    memset(val, 0, sizeof(val));
    if (property_get(prop, val, "2")) {
        //ALOGV("prop: %s is %s",prop, val);
        return atoi(val);
    }
    return 0;
}

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

static int Amvideo_Handle = 0;

#define SYSFS_AMVIDEO_CURIDX      "/sys/module/amvideo/parameters/cur_dev_idx"
#define SYSFS_DISPLAY_MODE          "/sys/class/display/mode"
#define SYSFS_FB0_FREE_SCALE        "/sys/class/graphics/fb0/free_scale"
#define SYSFS_FB1_FREE_SCALE        "/sys/class/graphics/fb0/free_scale"
#define SYSFS_VIDEO_AXIS               "/sys/class/video/axis"
#define SYSFS_VIDEOBUFUSED          "/sys/class/amstream/videobufused"

extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);
/*****************************************************************************/
static int chk_and_dup(int fence)
{
    if (fence < 0) {
        HWC_LOGWB("not a vliad fence %d",fence);
        return -1;
    }

    int dup_fence = dup(fence);
    if (dup_fence < 0) {
        HWC_LOGWB("fence dup failed: %s", strerror(errno));
    }

    return dup_fence;
}

#define MAX_SUPPORT_DISPLAYS HWC_NUM_PHYSICAL_DISPLAY_TYPES

    //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
typedef struct cursor_context_t{
    bool blank;
    struct framebuffer_info_t cb_info;
    void *cbuffer;
}cursor_context_t;
#endif
//--hwc 1.4

typedef struct display_context_t{
    bool connected;
    struct framebuffer_info_t fb_info;
    struct private_handle_t*  fb_hnd;

    //++hwc 1.4
#if 0 //ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    struct cursor_context_t cursor_ctx;
#endif
    //--hwc 1.4
}display_context_t;

struct hwc_context_1_t;

int init_display(hwc_context_1_t* context,int displayType);
int uninit_display(hwc_context_1_t* context,int displayType);

#define get_display_info(ctx,disp) \
    display_context_t * display_ctx = &(ctx->display_ctxs[disp]);\
    framebuffer_info_t* fbinfo = &(display_ctx->fb_info); 


struct hwc_context_1_t {
    hwc_composer_device_1_t base;

    /* our private state goes below here */
    hwc_layer_1_t const* saved_layer;
    unsigned saved_transform;
    int saved_left;
    int saved_top;
    int saved_right;
    int saved_bottom;

    //vsync.
    int32_t     vsync_period;
    int           vsync_enable;
    bool         blank_status;

    //video buf is used flag
    char         video_buf_used[32];

    const hwc_procs_t       *procs;
    pthread_t               vsync_thread;
    pthread_t            hotplug_thread;

    private_module_t        *gralloc_module;
    display_context_t display_ctxs[MAX_SUPPORT_DISPLAYS];
};

#if WITH_LIBPLAYER_MODULE
static bool chk_sysfs_status(const char* sysfstr, char* lastr, int size){
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

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "hwcomposer module",
        author: "Amlogic",
        methods: &hwc_module_methods,
        dso : NULL,
        reserved : {0},
    }
 };


static pthread_cond_t hwc_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t hwc_mutex = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************************/

static void dump_handle(private_handle_t *h)
{
    //ALOGV("\t\tformat = %d, width = %u, height = %u, stride = %u, vstride = %u",
        //      h->format, h->width, h->height, h->stride, h->vstride);
}

static void dump_layer(hwc_layer_1_t const *l)
{
    HWC_LOGVB("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
        "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
        l->compositionType, l->flags, l->handle, l->transform,
        l->blending,
        l->sourceCrop.left,
        l->sourceCrop.top,
        l->sourceCrop.right,
        l->sourceCrop.bottom,
        l->displayFrame.left,
        l->displayFrame.top,
        l->displayFrame.right,
        l->displayFrame.bottom);

    if (l->handle && !(l->flags & HWC_SKIP_LAYER)) dump_handle(private_handle_t::dynamicCast(l->handle));
}

/*****************************************************************************/

static void hwc_overlay_compose(hwc_context_1_t *dev, hwc_layer_1_t const* l)
{
    int angle;
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)dev;

#if WITH_LIBPLAYER_MODULE
    static char last_val[32] = "0";
    static char last_axis[32] = "0";
    static char last_mode[32] = {0};
    static char last_free_scale[32] = {0};
    bool vpp_changed = false;
    bool axis_changed = false;
    bool mode_changed = false;
    bool free_scale_changed = false;

    if (chk_bool_prop("ro.vout.dualdisplay4")) {
        vpp_changed = chk_sysfs_status(SYSFS_AMVIDEO_CURIDX, last_val, 32);
    }

    mode_changed = chk_sysfs_status(SYSFS_DISPLAY_MODE, last_mode, 32);

#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    if (!(ctx->display_ctxs[HWC_DISPLAY_EXTERNAL].connected/* && is_m8_single_display()*/)) {
#endif
        free_scale_changed = chk_sysfs_status(SYSFS_FB0_FREE_SCALE, last_free_scale, 32);
#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    }else{
        free_scale_changed = chk_sysfs_status(SYSFS_FB1_FREE_SCALE, last_free_scale, 32);
    }
#endif
    axis_changed = chk_sysfs_status(SYSFS_VIDEO_AXIS, last_axis, 32);

    if ((ctx->saved_layer == l) &&
        (ctx->saved_transform == l->transform) &&
        (ctx->saved_left == l->displayFrame.left) &&
        (ctx->saved_top == l->displayFrame.top) &&
        (ctx->saved_right == l->displayFrame.right) &&
        (ctx->saved_bottom == l->displayFrame.bottom) &&
        !vpp_changed && !mode_changed && !axis_changed && !free_scale_changed) {
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

    amvideo_utils_set_virtual_position(l->displayFrame.left,
                   l->displayFrame.top,
                   l->displayFrame.right - l->displayFrame.left + 1,
                   l->displayFrame.bottom - l->displayFrame.top + 1,
                   angle);

    /* the screen mode from Android framework should always be set to normal mode
    * to match the relationship between the UI and video overlay window position.
    */
    /*set screen_mode in amvideo_utils_set_virtual_position(),pls check in libplayer*/
    //amvideo_utils_set_screen_mode(0);
#endif

    ctx->saved_layer = l;
    ctx->saved_transform = l->transform;
    ctx->saved_left = l->displayFrame.left;
    ctx->saved_top = l->displayFrame.top;
    ctx->saved_right = l->displayFrame.right;
    ctx->saved_bottom = l->displayFrame.bottom;

#if WITH_LIBPLAYER_MODULE
    memset(last_axis, 0, sizeof(last_axis));

    if (amsysfs_get_sysfs_str(SYSFS_VIDEO_AXIS, last_axis, sizeof(last_axis)) == 0) {
        HWC_LOGDB("****last video axis is: %s",last_axis);
    }
#endif
}

static void hwc_dump(hwc_composer_device_1* dev, char *buff, int buff_len)
{
    if (buff_len <= 0) return;

    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    android::String8 result;

    for (int i = 0; i < MAX_SUPPORT_DISPLAYS; i++) {
        get_display_info(pdev,i);

        if (display_ctx->connected) {
            result.appendFormat("  %8s Display connected: %3s\n", 
                HWC_DISPLAY_EXTERNAL == i ? "External":"Primiary", display_ctx->connected ? "Yes" : "No");
                result.appendFormat("    w=%u, h=%u, xdpi=%f, ydpi=%f, osdIdx=%d\n", 
                fbinfo->info.xres, 
                fbinfo->info.yres, 
                fbinfo->xdpi, 
                fbinfo->ydpi,
                fbinfo->fbIdx);
        }
    }

    //result.append(
    //        "   type   |  handle  |  color   | blend | format |   position    |     size      | gsc \n"
    //        "----------+----------|----------+-------+--------+---------------+---------------------\n");
    //        8_______ | 8_______ | 8_______ | 5____ | 6_____ | [5____,5____] | [5____,5____] | 3__ \n"

    result.append("\n");

    strlcpy(buff, result.string(), buff_len);
}

static int hwc_blank(struct hwc_composer_device_1 *dev, int disp, int blank)
{
    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    //TODO: need impl
    if (disp == HWC_DISPLAY_PRIMARY) pdev->blank_status = ( blank ? true : false);

    return 0;
}

static int hwc_setPowerMode(struct hwc_composer_device_1* dev, int disp,
                int mode)
{
    LOG_FUNCTION_NAME

    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    //TODO:

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

static int hwc_query(struct hwc_composer_device_1* dev, int what, int *value)
{
    LOG_FUNCTION_NAME

    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    switch (what) {
        case HWC_BACKGROUND_LAYER_SUPPORTED:
            // we support the background layer
            value[0] = 1;
        break;
        case HWC_VSYNC_PERIOD:
            // vsync period in nanosecond
            value[0] = pdev->vsync_period;
        break;
        default:
            // unsupported query
        return -EINVAL;
    }

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

static int hwc_eventControl(struct hwc_composer_device_1* dev,
                            int disp,
                            int event,
                            int enabled)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t *)dev;
    switch (event)
    {
        case HWC_EVENT_VSYNC:
            ctx->vsync_enable = enabled;
            pthread_mutex_lock(&hwc_mutex);
            pthread_cond_signal(&hwc_cond);
            pthread_mutex_unlock(&hwc_mutex);
        return 0;
    }
    return -EINVAL;
}

static int hwc_prepare(struct hwc_composer_device_1 *dev,
                       size_t numDisplays,
                       hwc_display_contents_1_t** displays)
{
    int err = 0, i = 0;
    hwc_context_1_t *pdev =  (hwc_context_1_t *)dev;
    hwc_display_contents_1_t *display_content = NULL;

    if (!numDisplays || !displays) return 0;

    LOG_FUNCTION_NAME
    //retireFenceFd will close in surfaceflinger, just reset it.
    for (i=0;i<numDisplays;i++) {

#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
        if (pdev->display_ctxs[HWC_DISPLAY_EXTERNAL].connected
            && i==HWC_DISPLAY_PRIMARY /*&& is_m8_single_display()*/) {
            continue;
        }
#endif

        display_content = displays[i];
        if ( display_content ) {
            display_content->retireFenceFd = -1;
            for (size_t j=0 ; j< display_content->numHwLayers ; j++) {
                hwc_layer_1_t* l = &display_content->hwLayers[j];
                //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
                if (l->flags & HWC_IS_CURSOR_LAYER) {
                    l->hints = HWC_HINT_CLEAR_FB;
                    HWC_LOGDA("This is a Sprite layer");
                    l->compositionType = HWC_CURSOR_OVERLAY;
                    continue;
                }
#endif
                //--hwc 1.4
                if (l->handle) {
                    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(l->handle);
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY) {
                        l->hints = HWC_HINT_CLEAR_FB;
                        l->compositionType = HWC_OVERLAY;
                        continue;
                    }
                }
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

static int fb_post(hwc_context_1_t *pdev,
        hwc_display_contents_1_t* contents, int display_type){
    int err = 0;
    size_t i = 0;

    //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    cursor_context_t * cursor_ctx = &(pdev->display_ctxs[display_type].cursor_ctx);
    framebuffer_info_t* cbinfo = &(cursor_ctx->cb_info); 

    cursor_ctx->blank = false;
#endif
    //--hwc 1.4

    for (i = 0; i < contents->numHwLayers; i++) {

        //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
        //deal cursor layer
        if (contents->hwLayers[i].flags & HWC_IS_CURSOR_LAYER) {
            hwc_layer_1_t *layer = &(contents->hwLayers[i]);
            if (private_handle_t::validate(layer->handle) < 0) break;
            private_handle_t *hnd = (private_handle_t *)layer->handle;

            HWC_LOGDB("This is a Sprite, hnd->stride is %d, hnd->height is %d", hnd->stride, hnd->height);
            if (cbinfo->info.xres != hnd->stride || cbinfo->info.yres != hnd->height) {
                HWC_LOGDB("disp: %d cursor need to redrew", display_type);
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
            cursor_ctx->blank = true;
        }
#endif
        //--hwc 1.4

        //deal framebuffer target layer
        if (contents->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET) {
            hwc_layer_1_t *layer = &(contents->hwLayers[i]);
            if (private_handle_t::validate(layer->handle) < 0) break;

            //deal with virtural display fence
            if (display_type == HWC_DISPLAY_VIRTUAL) {
                layer->releaseFenceFd = layer->acquireFenceFd;
                contents->retireFenceFd = contents->outbufAcquireFenceFd;

                HWC_LOGVB("HWC_DISPLAY_VIRTUAL Get release fence %d, retire fence %d, outbufAcquireFenceFd %d",
                        layer->releaseFenceFd,
                        contents->retireFenceFd, contents->outbufAcquireFenceFd);
                continue;
            }

            get_display_info(pdev, display_type);

#ifdef DEBUG_EXTERNAL_DISPLAY_ON_PANEL
            int acquireFence = layer->acquireFenceFd;
            if (display_type != displayWho()) {
                close(acquireFence);
                acquireFence = -1;
                continue;
            }
#endif

            layer->releaseFenceFd = fb_post_with_fence_locked(fbinfo,layer->handle,layer->acquireFenceFd);

            if (layer->releaseFenceFd >= 0) {
                //layer->releaseFenceFd = releaseFence;
                contents->retireFenceFd = chk_and_dup(layer->releaseFenceFd);

                HWC_LOGVB("Get release fence %d, retire fence %d",
                        layer->releaseFenceFd,
                        contents->retireFenceFd);
            } else {
                HWC_LOGEB("No valid release_fence returned. %d ",layer->releaseFenceFd);
                //-1 means no fence, less than -1 is some error
                if (layer->releaseFenceFd < -1) err = layer->releaseFenceFd;
                contents->retireFenceFd = layer->releaseFenceFd = -1;
            }
        }
    }

    //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    //finally we need to update cursor's blank status
    if (cbinfo->fd > 0) ioctl(cbinfo->fd, FBIOBLANK, !cursor_ctx->blank);
#endif
    //--hwc 1.4
    return err;
}


#ifdef DEBUG_EXTERNAL_DISPLAY_ON_PANEL
int displayWho(){
    if (chk_bool_prop("debug.display_external")) {
        return 1;
    }else{
        return 0;
    }
}
#endif

static int hwc_set(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int err = 0, i = 0, j =0;
    hwc_context_1_t *pdev =  (hwc_context_1_t *)dev;
    hwc_display_contents_1_t *display_content = NULL;

    if (!numDisplays || !displays) return 0;

    LOG_FUNCTION_NAME
    //TODO: need improve the way to set video axis.
#if WITH_LIBPLAYER_MODULE
    bool istvp = false;
    for (i = 0;i<numDisplays;i++) {
#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
        if (pdev->display_ctxs[HWC_DISPLAY_EXTERNAL].connected
            && i==HWC_DISPLAY_PRIMARY /*&& is_m8_single_display()*/) {
            continue;
        }
#endif

        display_content = displays[i];
        if ( display_content ) {
            for (j=0 ; j<display_content->numHwLayers ; j++) {
                hwc_layer_1_t* l = &display_content->hwLayers[j];
                if (l->handle) {
                    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(l->handle);
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OMX) {
                        set_omx_pts((char*)hnd->base, &Amvideo_Handle);
                        istvp = true;
                    }
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_VIDEO_OVERLAY) {
                        hwc_overlay_compose(pdev, l);
                    }
                }
            }
            if (istvp == false && Amvideo_Handle!=0) {
                closeamvideo();
                Amvideo_Handle = 0;
            }
        }
    }

#endif

    for (i=0;i<numDisplays;i++) {

#ifdef SINGLE_EXTERNAL_DISPLAY_USE_FB1
        if (pdev->display_ctxs[HWC_DISPLAY_EXTERNAL].connected
                && i==HWC_DISPLAY_PRIMARY /*&& is_m8_single_display()*/) {
            continue;
        }
#endif
        display_content = displays[i];
        if (display_content) {
            if (i <= HWC_DISPLAY_VIRTUAL) {
                 //physic display
                 err = fb_post(pdev,display_content,i);
            } else {
                 HWC_LOGEB("display %d is not supported",i);
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT
    return err;
}

static int hwc_close(hw_device_t *device)
{
    struct hwc_context_1_t *dev = (struct hwc_context_1_t *)device;

    LOG_FUNCTION_NAME

    pthread_kill(dev->vsync_thread, SIGTERM);
    pthread_join(dev->vsync_thread, NULL);

    uninit_display(dev,HWC_DISPLAY_PRIMARY);
    uninit_display(dev,HWC_DISPLAY_EXTERNAL);

    if (dev) free(dev);

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

//#define USE_HW_VSYNC
#ifdef USE_HW_VSYNC
/*
Still have bugs, don't use it.
*/
int wait_next_vsync(struct hwc_context_1_t* ctx, nsecs_t* vsync_timestamp){
    static nsecs_t previewTime = 0;
    nsecs_t vsyncDiff=0;
    const nsecs_t period = ctx->vsync_period;
    //we will delay hw vsync if missing one vsync interrupt isr.
    int ret = 0;

    if (ioctl(ctx->display_ctxs[0].fb_info.fd, FBIO_WAITFORVSYNC, &ret) == -1) {
        HWC_LOGEB("ioctrl error %d",ctx->display_ctxs[0].fb_info.fd);
        ret=-1;
    } else {
        if (ret == 1) {
            *vsync_timestamp = systemTime(CLOCK_MONOTONIC);
            vsyncDiff=*vsync_timestamp - previewTime;   
            if (previewTime != 0) HWC_LOGEB("wait for vsync success %lld",vsyncDiff);
            vsyncDiff%=period;
            if(vsyncDiff > 500000) 
            {
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
int wait_next_vsync(struct hwc_context_1_t* ctx , nsecs_t* vsync_timestamp)
{
    static nsecs_t nextFakeVSync = 0;

    const nsecs_t period = ctx->vsync_period;
    const nsecs_t now = systemTime(CLOCK_MONOTONIC);
    nsecs_t next_vsync = nextFakeVSync;
    nsecs_t sleep = next_vsync - now;
    if (sleep < 0) {
        // we missed, find where the next vsync should be
        sleep = (period - ((now - next_vsync) % period));
        next_vsync = now + sleep;
    }
    nextFakeVSync = next_vsync + period;

    struct timespec spec;
    spec.tv_sec  = next_vsync / 1000000000;
    spec.tv_nsec = next_vsync % 1000000000;

    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);
    *vsync_timestamp = next_vsync;

    return err;
}
#endif

static void *hwc_vsync_thread(void *data)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)data;
    nsecs_t nextFakeVSync = 0;
    nsecs_t timestamp;

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY-1);
    sleep(2);

    while (true) {
        pthread_mutex_lock(&hwc_mutex);
        while(ctx->vsync_enable==false) {
            pthread_cond_wait(&hwc_cond, &hwc_mutex);
        }
        pthread_mutex_unlock(&hwc_mutex);

        if (wait_next_vsync(ctx,&timestamp) == 0) {
            if (ctx->procs) ctx->procs->vsync(ctx->procs, 0, timestamp);
        }
    }

    return NULL;
}

#ifdef WITH_EXTERNAL_DISPLAY

//#define SIMULATE_HOT_PLUG 1
static bool chk_external_conect()
{
#ifdef SIMULATE_HOT_PLUG
    HWC_LOGDA("Simulate hot plug");
    return chk_bool_prop("debug.connect_external");
#else
    //TODO:need to check hotplug callback for different device.
    //Should consider different device.
    return chk_bool_prop("sys.sf.hotplug");
#endif
}

static void *hwc_hotplug_thread(void *data)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)data;

    while (true) {
        usleep(500*1000);//sleep 200 ms.

        if (ctx->procs) {
            bool connect = chk_external_conect();
            get_display_info(ctx,HWC_DISPLAY_EXTERNAL);
            if (display_ctx->connected != connect ) {
                HWC_LOGDB("external new state : %d",connect);
                if (!display_ctx->connected) {
                    init_display(ctx,HWC_DISPLAY_EXTERNAL);
                } else {
                    uninit_display(ctx,HWC_DISPLAY_EXTERNAL);
                }
                ctx->procs->hotplug(ctx->procs, HWC_DISPLAY_EXTERNAL, connect);
            }
#if WITH_LIBPLAYER_MODULE
            //we should refresh screen when video buf used flag changed
            if (chk_sysfs_status(SYSFS_VIDEOBUFUSED, ctx->video_buf_used, 32)) {
                ctx->procs->invalidate(ctx->procs);
            }
#endif
        }
    }

    return NULL;
}
#endif

static void hwc_registerProcs(hwc_composer_device_1 *dev,
            hwc_procs_t const* procs)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)dev;

    if (ctx) ctx->procs = procs;
}

static int hwc_getDisplayConfigs(hwc_composer_device_1_t *dev,
            int disp ,uint32_t *config ,size_t *numConfigs)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)dev;

    if (*numConfigs == 0) return 0;

    LOG_FUNCTION_NAME

    get_display_info(ctx,disp);

    if (disp == HWC_DISPLAY_PRIMARY) {
        config[0] = 0;
        *numConfigs = 1;
        return 0;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
        HWC_LOGEB("hwc_getDisplayConfigs:connect =  %d",display_ctx->connected);
        if (!display_ctx->connected) return -EINVAL;

        config[0] = 0;
        *numConfigs = 1;
        return 0;
    }

    LOG_FUNCTION_NAME_EXIT
    return -EINVAL;
}

static int hwc_getDisplayAttributes(hwc_composer_device_1_t *dev,
            int disp, uint32_t config, const uint32_t *attributes, int32_t *values)
{
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)dev;

    LOG_FUNCTION_NAME

    get_display_info(ctx,disp);

    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        switch(attributes[i]) {	
            case HWC_DISPLAY_VSYNC_PERIOD:
                values[i] = ctx->vsync_period;
            break;
            case HWC_DISPLAY_WIDTH:
                values[i] = fbinfo->info.xres;
            break;
            case HWC_DISPLAY_HEIGHT:
                values[i] = fbinfo->info.yres;
            break;
            case HWC_DISPLAY_DPI_X:
                values[i] = fbinfo->xdpi*1000;
            break;
            case HWC_DISPLAY_DPI_Y:
                values[i] = fbinfo->ydpi*1000;
            break;
            default:
                HWC_LOGEB("unknown display attribute %u", attributes[i]);
                values[i] = -EINVAL;
            break;
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return 0;
}


//++hwc 1.4
static int hwc_getActiveConfig(struct hwc_composer_device_1* dev, int disp)
{
    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    //TODO:

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

static int hwc_setActiveConfig(struct hwc_composer_device_1* dev, int disp,
            int index)
{
    LOG_FUNCTION_NAME
    struct hwc_context_1_t *pdev = (struct hwc_context_1_t *)dev;

    //TODO:
    LOG_FUNCTION_NAME_EXIT
    return 0;
}

static int hwc_setCursorPositionAsync(struct hwc_composer_device_1 *dev, int disp, 
    int x_pos, int y_pos)
{
    LOG_FUNCTION_NAME

#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    int err = 0, i = 0;
    struct fb_cursor cinfo;
    struct hwc_context_1_t* ctx = (struct hwc_context_1_t*)dev;
    cursor_context_t * cursor_ctx = &(ctx->display_ctxs[disp].cursor_ctx);
    framebuffer_info_t* cbinfo = &(cursor_ctx->cb_info);

    if (cbinfo->fd < 0) {
        HWC_LOGEB("hwc_setCursorPositionAsync fd=%d", cbinfo->fd );
    }else {
        cinfo.hot.x = x_pos;
        cinfo.hot.y = y_pos;
        if (disp == HWC_DISPLAY_PRIMARY) {
            HWC_LOGDB("hwc_setCursorPositionAsync x_pos=%d, y_pos=%d", cinfo.hot.x, cinfo.hot.y);
            ioctl(cbinfo->fd, FBIO_CURSOR, &cinfo);
        } else if(disp == HWC_DISPLAY_EXTERNAL) {
            //TODO:
            HWC_LOGDA("hwc_setCursorPositionAsync external display need show cursor too! ");
            //ioctl(cbinfo->fd, FBIO_CURSOR, &cinfo);
        }
    }
#endif

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

//--hwc 1.4

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int ret;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) return -EINVAL;

    struct hwc_context_1_t *dev;
    dev = (struct hwc_context_1_t *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
        (const struct hw_module_t **)&dev->gralloc_module)) {
        HWC_LOGEA("failed to get gralloc hw module");
        ret = -EINVAL;
        goto err_get_module;
    }

    //init primiary display
    //default is alwasy false,will check it in hot plug.
    init_display(dev,HWC_DISPLAY_PRIMARY);

    //60HZ, willchanged to use hw vsync.
    dev->vsync_period  = 16666666;

    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = HWC_DEVICE_API_VERSION_1_4;
    dev->base.common.module = const_cast<hw_module_t *>(module);
    dev->base.common.close = hwc_close;

    dev->base.prepare = hwc_prepare;
    dev->base.set = hwc_set;
    dev->base.eventControl = hwc_eventControl;
    //++hwc 1.4 abandon api
    dev->base.blank = hwc_blank;
    //--hwc 1.4 abandon api
    dev->base.query = hwc_query;
    dev->base.registerProcs = hwc_registerProcs;

    dev->base.dump = hwc_dump;
    dev->base.getDisplayConfigs = hwc_getDisplayConfigs;
    dev->base.getDisplayAttributes = hwc_getDisplayAttributes;
    //++hwc 1.4 new apis
    dev->base.setPowerMode = hwc_setPowerMode;
    dev->base.getActiveConfig = hwc_getActiveConfig;
    dev->base.setActiveConfig = hwc_setActiveConfig;
    dev->base.setCursorPositionAsync = hwc_setCursorPositionAsync;
    //--hwc 1.4 new apis
    dev->vsync_enable = false;
    dev->blank_status = false;
    *device = &dev->base.common;

    ret = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (ret) {
        HWC_LOGEB("failed to start vsync thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }

#ifdef WITH_EXTERNAL_DISPLAY
    //temp solution, will change to use uevnet from kernel
    ret = pthread_create(&dev->hotplug_thread, NULL, hwc_hotplug_thread, dev);
    if (ret) {
        HWC_LOGEB("failed to start hotplug thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }
#endif

    return 0;

err_vsync:
    uninit_display(dev,HWC_DISPLAY_PRIMARY);
err_get_module:
    if (dev) free(dev);

    return ret;
}

/*
Operater of framebuffer
*/
int init_display(hwc_context_1_t* context,int displayType)
{
    get_display_info(context, displayType);

    if (display_ctx->connected) return 0;

    pthread_mutex_lock(&hwc_mutex);

    if ( !display_ctx->fb_hnd ) {
        //init information from osd.
        fbinfo->displayType = displayType;
        fbinfo->fbIdx = getOsdIdx(fbinfo->displayType);
        int err = init_frame_buffer_locked(fbinfo);
        int bufferSize = fbinfo->finfo.line_length * fbinfo->info.yres;
        HWC_LOGDB("init_frame_buffer get fbinfo->fbIdx (%d) fbinfo->info.xres (%d) fbinfo->info.yres (%d)",fbinfo->fbIdx, fbinfo->info.xres,fbinfo->info.yres);
        int usage = 0; 
        if (displayType > 0) usage |= GRALLOC_USAGE_EXTERNAL_DISP;

        //Register the framebuffer to gralloc module
        display_ctx->fb_hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER, usage, fbinfo->fbSize, 0,
                                                                    0, fbinfo->fd, bufferSize);
        context->gralloc_module->base.registerBuffer(&(context->gralloc_module->base),display_ctx->fb_hnd);
        HWC_LOGDB("init_frame_buffer get frame size %d usage %d",bufferSize,usage);
    }

    display_ctx->connected = true;
    pthread_mutex_unlock(&hwc_mutex);

    //++hwc 1.4
#if 0//ndef SINGLE_EXTERNAL_DISPLAY_USE_FB1
    // init cursor framebuffer
    cursor_context_t* cursor_ctx = &(display_ctx->cursor_ctx);
    framebuffer_info_t* cbinfo = &(cursor_ctx->cb_info);
    cbinfo->fd = -1;

    //init information from cursor framebuffer.
    cbinfo->fbIdx = displayType*2+1;
    if (1 != cbinfo->fbIdx && 3 != cbinfo->fbIdx) {
        HWC_LOGEB("invalid fb index: %d, need to check!",cbinfo->fbIdx);
        return 0;
    }
    int err = init_cursor_buffer_locked(cbinfo);
    if (err != 0) {
        HWC_LOGEA("init_cursor_buffer_locked failed, need to check!");
        return 0;
    }
    HWC_LOGDB("init_cursor_buffer get cbinfo->fbIdx (%d) cbinfo->info.xres (%d) cbinfo->info.yres (%d)",
                        cbinfo->fbIdx,
                        cbinfo->info.xres,
                        cbinfo->info.yres);

    if ( cbinfo->fd >= 0) {
        HWC_LOGDA("init_cursor_buffer success!");
    }else{
        HWC_LOGEA("init_cursor_buffer fail!");
    }
#endif
    //--hwc 1.4

    return 0;
}

int uninit_display(hwc_context_1_t* context,int displayType)
{
    get_display_info(context, displayType);

    if (!display_ctx->connected) {
        return 0;
    }

    pthread_mutex_lock(&hwc_mutex);

    display_ctx->connected = false;
    pthread_mutex_unlock(&hwc_mutex);

    return 0;
}

