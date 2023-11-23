/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AICOLOR_PROCESSOR_H
#define AICOLOR_PROCESSOR_H

#include <FbProcessor.h>
#include <queue>
#include <linux/ion.h>
#include <ion/ion.h>
#include <UvmDev.h>
#include "color_sdk.h"

#define AICOLOR_INPUT_WIDTH        256
#define AICOLOR_INPUT_HEIGH        256
#define MAX_AICOLOR_COUNT          120
#define AICOLOR_MAX_CACHE_COUNT     5

struct aicolor_buffer_t {
    int fd;
    void *fd_ptr; //only for non-nativebuffer!
    int size;
    buffer_handle_t buffer_handle;
};

enum aicolor_get_info_type_e {
        AICOLOR_GET_INVALID = 0,
        AICOLOR_GET_RGB_DATA = 1,
        AICOLOR_GET_INDEX_INFO = 2,
        AICOLOR_GET_BASIC_INFO = 3,
};

struct aicolor_time_info_t {
    int64_t count;
    uint64_t max_time;
    uint64_t min_time;
    uint64_t total_time;
    uint64_t avg_time;
};

/*hwc attach aicolor info*/
struct uvm_aicolor_info {
    int32_t shared_fd;
    int32_t aicolor_fd;
    unsigned char nn_value[MAX_AICOLOR_COUNT];
    int32_t aicolor_buf_index;
    int32_t aicolor_value_index;
    int32_t get_info_type;
    int32_t need_do_aicolor;
    int32_t repeat_frame;
    int32_t dw_width;
    int32_t dw_height;
    int32_t nn_input_frame_width;
    int32_t nn_input_frame_height;
    int32_t nn_status;
    int32_t omx_index;
};

struct uvm_aicolor_info_t {
    enum uvm_hook_mod_type mode_type;
    int shared_fd;
    struct uvm_aicolor_info aicolor_info;
};

union uvm_aicolor_ioctl_arg {
    struct uvm_hook_data hook_data;
    struct uvm_aicolor_info uvm_info;
};

struct aicolor_index_value_t {
    int buf_index;
    int shared_fd;
};

class AiColorProcessor : public FbProcessor {
public:
    AiColorProcessor();
    ~AiColorProcessor();

    int32_t setup();
    int32_t process(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb);
    int32_t asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence);
    int32_t onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb,
        int releaseFence);
    int32_t teardown();
    meson_fb_processor_t getFbProcessorType() {return FB_AICOLOR_PROCESSOR;};
    int allocDmaBuffer();
    int freeDmaBuffers();
    void triggerEvent();
    void threadProcess();
    int32_t waitEvent(int microseconds);
    static void *mNn_qcontext;
    static bool mModelLoaded;
    mutable std::mutex mMutex;
    mutable std::mutex mMutex_index;
    std::queue<int> mBuf_fd_q;
    static void * threadMain(void * data);
    int LoadNNModel();
    pthread_t mThread;
    bool mExitThread;
    bool mInited;
    int32_t ai_color_process(int input_fd);
    void dump_nn_info(int num);
    int PropGetInt(const char* str, int def);
    int check_D();
    pthread_mutex_t m_waitMutex;
    pthread_cond_t m_waitCond;
    int mUvmHandler;
    int mNn_Index;
    static struct aicolor_time_info_t mTime;
    static int mInstanceID;
    static int mLogLevel;
    bool mBuf_Alloced;
    bool mNnDoing;
    aicolor_buffer_t mAiColor_Buf;
    int mDumpIndex;
    int64_t mDupCount;
    int64_t mCloseCount;
    static int64_t mTotalDupCount;
    static int64_t mTotalCloseCount;
    struct aicolor_index_value_t mAiColorIndex[AICOLOR_MAX_CACHE_COUNT];
    int mCacheIndex;
    int mBuf_index;
    int mVInfo_width;
    int mVInfo_height;
    int mNnInputWidth;
    int mNnInputHeight;
};

#endif
