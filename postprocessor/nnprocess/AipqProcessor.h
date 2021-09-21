/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AIPQ_PROCESSOR_H
#define AIPQ_PROCESSOR_H

#include <FbProcessor.h>
#include <queue>
#include <linux/ion.h>
#include <ion/ion.h>
#include <nn_demo.h>
#include <UvmDev.h>

#define META_DATA_SIZE      (256)

#define MAX_SCENE 7
#define AI_OUT_SCENE 5
#define AI_PQ_TOP AI_OUT_SCENE
#define AIPQ_NB_PATH            "/vendor/bin/nn/PQNet.nb"
#define AIPQ_SCENE_DATA_PATH    "/vendor/etc/scenes_data.txt"

struct aipq_buffer_t {
    int fd;
    void *fd_ptr; //only for non-nativebuffer!
    ion_user_handle_t ion_hnd; //only for non-nativebuffer!
    int size;
};

enum aipq_get_info_type_e {
        AIPQ_GET_INVALID = 0,
        AIPQ_GET_224_DATA = 1,
        AIPQ_GET_INDEX_INFO = 2,
        AIPQ_GET_BASIC_INFO = 3,
};

struct aipq_time_info_t {
    int64_t count;
    uint64_t max_time;
    uint64_t min_time;
    uint64_t total_time;
    uint64_t avg_time;
};

struct nn_value_t {
    int maxclass;
    int maxprob;
};

/*hwc attach aipq info*/
struct uvm_aipq_info {
    int32_t shared_fd;
    int32_t aipq_fd;
    struct nn_value_t nn_value[AI_PQ_TOP];
    int32_t aipq_buf_index;
    int32_t aipq_value_index;
    int32_t get_info_type;
    int32_t need_do_aipq;
    int32_t repert_frame;
};

struct uvm_aipq_info_t {
    enum uvm_hook_mod_type mode_type;
    int shared_fd;
    struct uvm_aipq_info aipq_info;
};

union uvm_aipq_ioctl_arg {
    struct uvm_hook_data hook_data;
    struct uvm_aipq_info uvm_info;
};

class AipqProcessor : public FbProcessor {
public:
    AipqProcessor();
    ~AipqProcessor();

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
    static void *mNn_qcontext;
    static bool mModelLoaded;
    mutable std::mutex mMutex;
    std::queue<int> mBuf_fd_q;
    static void * threadMain(void * data);
    int LoadNNModel();
    pthread_t mThread;
    bool mExitThread;
    bool mInited;
    int32_t ai_pq_process(int input_fd);
    void nn_value_reorder(img_classify_out_t *nn_out, struct nn_value_t *scenes);
    void dump_nn_info();
    int PropGetInt(const char* str, int def);
    int check_D();
    pthread_mutex_t m_waitMutex;
    pthread_cond_t m_waitCond;
    int mIonFd;
    int mUvmHander;
    int mNn_Index;
    static struct aipq_time_info_t mTime;
    static int mInstanceID;
    static int mLogLevel;
    bool mBuf_Alloced;
    aipq_buffer_t mAipq_Buf;
    int mDumpIndex;
    static int mSkin_index_class1;
    static int mSkin_index_class2;
    int64_t mDupCount;
    int64_t mCloseCount;
    static int64_t mTotalDupCount;
    static int64_t mTotalCloseCount;
    struct nn_value_t mLastNnValue[AI_PQ_TOP];
};

#endif
