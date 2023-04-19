/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AIFACE_PROCESSOR_H
#define AIFACE_PROCESSOR_H

#include <FbProcessor.h>
#include <queue>
#include <linux/ion.h>
#include <ion/ion.h>
#include <UvmDev.h>
#include "face_sdk.h"

#define AIFACE_INPUT_WIDTH    512
#define AIFACE_INPUT_HEIGH    288
#define AIFACE_INPUT_CHANNEL  1
#define AIFACE_INPUT_ROTMAT   1  //1:RGB data  2:YUV gray data

#define MAX_FACE_COUNT 10
#define AIFACE_MAX_CACHE_COUNT 5

struct aiface_buffer_t {
    int fd;
    void *fd_ptr; //only for non-nativebuffer!
    int size;
    buffer_handle_t buffer_handle;
};

enum aiface_get_info_type_e {
        AIFACE_GET_INVALID = 0,
        AIFACE_GET_RGB_DATA = 1,
        AIFACE_GET_INDEX_INFO = 2,
        AIFACE_GET_BASIC_INFO = 3,
};

struct aiface_time_info_t {
    int64_t count;
    uint64_t max_time;
    uint64_t min_time;
    uint64_t total_time;
    uint64_t avg_time;
};

struct face_value_t
{
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t score;
};

/*hwc attach aiface info*/
struct uvm_aiface_info {
    int32_t shared_fd;
    int32_t aiface_fd;
    int64_t buf_phy_addr;
    struct face_value_t nn_value[MAX_FACE_COUNT];
    int32_t aiface_buf_index;
    int32_t aiface_value_index;
    int32_t get_info_type;
    int32_t need_do_aiface;
    int32_t repeat_frame;
    int32_t dw_width;
    int32_t dw_height;
    int32_t nn_input_frame_width;
    int32_t nn_input_frame_height;
    int32_t nn_input_frame_format;
    int32_t nn_status;
    int32_t omx_index;
    void *dma_buf_addr;
};

struct uvm_aiface_info_t {
    enum uvm_hook_mod_type mode_type;
    int shared_fd;
    struct uvm_aiface_info aiface_info;
};

union uvm_aiface_ioctl_arg {
    struct uvm_hook_data hook_data;
    struct uvm_aiface_info uvm_info;
};

struct aiface_index_value_t {
    int buf_index;
    int shared_fd;
};

class AiFaceProcessor : public FbProcessor {
public:
    AiFaceProcessor();
    ~AiFaceProcessor();

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
    int allocDmaBuffer();
    int freeDmaBuffers();
    void triggerEvent();
    void threadProcess();
    int32_t waitEvent(int microseconds);
    void *mNn_qcontext;
    bool mModelLoaded;
    mutable std::mutex mMutex;
    mutable std::mutex mMutex_index;
    std::queue<int> mBuf_fd_q;
    static void * threadMain(void * data);
    int LoadNNModel();
    pthread_t mThread;
    bool mExitThread;
    bool mInited;
    int32_t ai_face_process(int input_fd);
    void dump_nn_info();
    int PropGetInt(const char* str, int def);
    int check_D();
    pthread_mutex_t m_waitMutex;
    pthread_cond_t m_waitCond;
    int mUvmHandler;
    int mNn_Index;
    static struct aiface_time_info_t mTime;
    static int mInstanceID;
    static int mLogLevel;
    static std::mutex mMutexAi;
    bool mBuf_Alloced;
    bool mNnDoing;
    aiface_buffer_t mAiFace_Buf;
    int mDumpIndex;
    int64_t mDupCount;
    int64_t mCloseCount;
    static int64_t mTotalDupCount;
    static int64_t mTotalCloseCount;
    struct aiface_index_value_t mAiFaceIndex[AIFACE_MAX_CACHE_COUNT];
    int mCacheIndex;
    int mBuf_index;
    int mVInfo_width;
    int mVInfo_height;
    int mNnInputWidth;
    int mNnInputHeight;
    int mNnInputChannel;
    int mNnInputFormat;
    void *mDmaBufAddr;
    face_landmark5_out_t nn_out_temp;
};

#endif

