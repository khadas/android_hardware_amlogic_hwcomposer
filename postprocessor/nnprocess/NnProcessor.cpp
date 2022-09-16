/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_nn"

#include "NnProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "sr_sdk.h"
#include <sched.h>
#include <cutils/properties.h>
#include <ion/ion.h>
#include <linux/ion_4.12.h>

#define NN_PB_2      "/vendor/bin/nn/SRNetx2_e8.nb"  /*1080p->4k*/
#define NN_PB_3      "/vendor/bin/nn/SRNetx3_e8.nb"
#define NN_PB_4      "/vendor/bin/nn/SRNetx4_960_e8.nb"  /*540p->4k*/
#define NN_PB_2_I    "/vendor/bin/nn/SRNetx2_i_e8.nb"  /*1080I->4k*/
#define NN_PB_3_I    "/vendor/bin/nn/SRNetx3_i_e8.nb"
#define NN_PB_4_I    "/vendor/bin/nn/SRNetx4_960_i_e8.nb"  /*540I->4k*/

#define FENCE_TIMEOUT_MS 1000

#define UVM_IOC_MAGIC 'U'

#define UVM_IOC_ATTATCH _IOWR(UVM_IOC_MAGIC, 5, \
                struct uvm_hook_data)
#define UVM_IOC_GET_INFO _IOWR(UVM_IOC_MAGIC, 6, \
                struct uvm_hook_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, \
                struct uvm_hook_data)

int NnProcessor::mInstanceID = 0;
int64_t NnProcessor::mTotalDupCount = 0;
int64_t NnProcessor::mTotalCloseCount = 0;
struct time_info_t NnProcessor::mTime[NN_MODE_COUNT];
void* NnProcessor::mNn_qcontext[NN_MODE_COUNT];
int NnProcessor::log_level = 0;
bool NnProcessor::mModelLoaded;

int NnProcessor::nn_check_D() {
    return (log_level > 0);
}

NnProcessor::NnProcessor() {
    ALOGD("NnProcessor");

    int i = 0;
    int interlaceCheckProp = 0;
    mBuf_Alloced = false;
    mExitThread = true;
    pthread_mutex_init(&m_waitMutex, NULL);
    pthread_cond_init(&m_waitCond, NULL);
    mIonFd = -1;
    while (i < SR_OUT_BUF_COUNT) {
        mSrBuf[i].index = i;
        mSrBuf[i].fd_ptr = NULL;
        mSrBuf[i].fd = -1;
        mSrBuf[i].ion_hnd = -1;
        mSrBuf[i].fence_fd = -1;
        mSrBuf[i].fence_fd_last = -1;
        mSrBuf[i].phy = 0;
        i++;
    }
    mInited = false;
    mIsModelInterfaceExist = true;
    mUvmHander = -1;
    mNn_Index = 0;
    mDumpHf = 0;
    mLast_buf = NULL;
    mVInfo_width = 0;
    mVInfo_height = 0;
    if (mInstanceID == 0) {
        for (i = 0; i < NN_MODE_COUNT; i++) {
            mTime[i].count = 0;
            mTime[i].max_time = 0;
            mTime[i].min_time = 0;
            mTime[i].total_time = 0;
            mTime[i].avg_time = 0;
            mNn_qcontext[i] = NULL;
        }
    }

    mUvmHander = open("/dev/uvm", O_RDWR | O_NONBLOCK);
    if (mUvmHander < 0) {
        ALOGE("can not open uvm");
    }

    interlaceCheckProp = PropGetInt("vendor.hwc.aisr_check_interlace", 1);
    if (interlaceCheckProp == 1) {
        ALOGD("need check I/P source.\n");
        mNeed_check_interlace = true;
    } else
        mNeed_check_interlace = false;

    mIsModelInterfaceExist = (isInterfaceImplement() == 1);

    if (!mModelLoaded && mIsModelInterfaceExist) {
        if (LoadNNModel())
            mModelLoaded = true;
    }

    mInstanceID++;
    mDupCount = 0;
    mCloseCount = 0;
}

NnProcessor::~NnProcessor() {
    int i;

    ALOGD("~NnProcessor: mDupCount =%lld, mCloseCount =%lld, total %lld %lld",
        mDupCount, mCloseCount, mTotalDupCount, mTotalCloseCount);

    if (mDupCount != mCloseCount)
        ALOGE("~NnProcessor:count err: %lld %lld", mDupCount, mCloseCount);

    if (mTotalDupCount != mTotalCloseCount)
        ALOGE("~NnProcessor:total count err: %lld %lld", mTotalDupCount, mTotalCloseCount);

    if (mInited)
        teardown();

    for (i = 0; i < NN_MODE_COUNT; i++) {
        if (!mNeed_check_interlace && i > 2)
            break;
        if (mTime[i].count > 0) {
            mTime[i].avg_time = mTime[i].total_time / mTime[i].count;
        }
        ALOGD("%s: time: i=%d, count=%lld, max=%lld, min=%lld, avg=%lld",
            __FUNCTION__,
            i,
            mTime[i].count,
            mTime[i].max_time,
            mTime[i].min_time,
            mTime[i].avg_time);
    }
    if (mUvmHander) {
        close(mUvmHander);
        mUvmHander = NULL;
    }
    ALOGD("%s: fence :r=%lld,wait=%lld, r-w=%lld",
          __FUNCTION__,
          mFence_receive_count,
          mFence_wait_count,
          mFence_receive_count - mFence_wait_count);

}

int NnProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    //ALOGD("%s is not set used def=%d\n", str, ret);
    return ret;
}

int32_t NnProcessor::setup() {
    ALOGD("%s", __FUNCTION__);
    if (!mUvmHander || !mIsModelInterfaceExist) {
        ALOGD("%s: init action is not ok.\n", __FUNCTION__);
        return -1;
    }

    if (mExitThread == true) {
            int ret = pthread_create(&mThread,
                                     NULL,
                                     NnProcessor::threadMain,
                                     (void *)this);
            if (ret != 0) {
                ALOGE("failed to start NnProcessor main thread: %s",
                      strerror(ret));
            } else
                mExitThread = false;
    }

    mNn_mode = -1;
    mNn_interlace_flag = 0;
    mInited = true;
    mBuf_index = 0;
    mBuf_index_cur = -1;
    mNeed_fence = false;
    mFence_receive_count = 0;
    mFence_wait_count = 0;

    return 0;
}

int32_t NnProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t NnProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    int ret;
    int ret_attatch = 0;
    int fence_fd = -1;
    buffer_handle_t buf = inputfb->mBufferHandle;
    int input_fd = -1;
    struct uvm_hook_data hook_data;
    struct uvm_hf_info_t *uvm_hf_info;
    struct uvm_ai_sr_info *ai_sr_info;
    int dup_fd = -1;
    int w;
    int h;
    int ready_size = 0;
    int crop_right;
    int crop_bottom;

    log_level = PropGetInt("vendor.hwc.nn_log", 0);

    mNeed_fence = false;
    processFence = -1;
    outfb = inputfb;

    if (!mIsModelInterfaceExist || !mModelLoaded) {
        ALOGD("%s: NN model don't ok.\n", __FUNCTION__);
        goto bypass;
    }

    if (/*inputfb->mFbType == DRM_FB_VIDEO_OMX_V4L ||*/
        inputfb->mFbType == DRM_FB_VIDEO_UVM_DMA) {
        input_fd = am_gralloc_get_omx_v4l_file(buf);
    } else if (inputfb->mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        input_fd = inputfb->getVtBuffer();
    } else
        ALOGE("%s: get fd fail mFbType=%d.", __FUNCTION__, inputfb->mFbType);

    if (input_fd == -1) {
        ALOGD_IF(nn_check_D(), "%s: input_fd invalid.", __FUNCTION__);
        goto bypass;
    }

    w = am_gralloc_get_width(inputfb->mBufferHandle);
    h = am_gralloc_get_height(inputfb->mBufferHandle);
    crop_right = inputfb->mSourceCrop.right;
    crop_bottom = inputfb->mSourceCrop.bottom;

    ALOGD_IF(nn_check_D(), "%s: %d*%d, crop:%d*%d",
             __FUNCTION__,
             w,
             h,
             crop_right,
             crop_bottom);

    if (crop_right > 1920 || crop_bottom > 1080)
        goto bypass;

    if (!mUvmHander) {
        ALOGE("%s: uvm not opened.\n", __FUNCTION__);
        goto bypass;
    }

    memset(&hook_data, 0, sizeof(struct uvm_hook_data));

    uvm_hf_info = (struct uvm_hf_info_t *)&hook_data;
    ai_sr_info = &(uvm_hf_info->ai_sr_info);

    uvm_hf_info->mode_type = PROCESS_NN;
    uvm_hf_info->shared_fd = input_fd;
    ai_sr_info->nn_out_fd = -1;
    ai_sr_info->shared_fd = input_fd;
    ai_sr_info->get_info_type = GET_HF_INFO;

    ret = ioctl(mUvmHander, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD("%s: UVM_IOC_GET_INFO failed.", __FUNCTION__);
    } else {
        ALOGD_IF(nn_check_D(),
            "asyncProcess_1: get_info: nn_status=%d, hf_phy_addr=%llx, %d*%d",
            ai_sr_info->nn_status,
            ai_sr_info->hf_phy_addr,
            ai_sr_info->hf_width,
            ai_sr_info->hf_height);
        if (ai_sr_info->nn_status == NN_WAIT_DOING
            || ai_sr_info->nn_status == NN_START_DOING
            || ai_sr_info->nn_status == NN_DONE) {
            ALOGD_IF(nn_check_D(),
                "nn not need do again, nn_index=%d",
                ai_sr_info->nn_index);
            goto bypass;
        }
        ALOGD_IF(nn_check_D(),
            "asyncProcess_2: get_info: nn_status=%d, hf_phy_addr=%llx, %d*%d",
            ai_sr_info->nn_status,
            ai_sr_info->hf_phy_addr,
            ai_sr_info->hf_width,
            ai_sr_info->hf_height);

        if (ai_sr_info->hf_phy_addr == 0 ||
            ai_sr_info->hf_width == 0 ||
            ai_sr_info->hf_height == 0) {
            ALOGD_IF(nn_check_D(), "%s: vf no hf", __FUNCTION__);
            goto bypass;
        }
    }

    ai_sr_info->need_do_aisr = 0;
    ai_sr_info->fence_fd = fence_fd;
    ai_sr_info->nn_out_fd = -1;
    ai_sr_info->nn_status = NN_WAIT_DOING;

    ret_attatch = ioctl(mUvmHander, UVM_IOC_ATTATCH, &hook_data);
    if (ret_attatch != 0) {
        ALOGE("attatch err: ret_attatch =%d", ret_attatch);
        goto bypass;
    }

    if (ai_sr_info->need_do_aisr == 0) {
        ALOGD_IF(nn_check_D(), "%s: aisr bypass", __FUNCTION__);
        goto error;
    }

    if (ai_sr_info->hf_phy_addr == 0 ||
        ai_sr_info->hf_width == 0 ||
        ai_sr_info->hf_height == 0) {
        ALOGD_IF(nn_check_D(), "%s: vf no hf", __FUNCTION__);
        goto error;
    }

    if ((mVInfo_width == 0) || (mVInfo_height == 0)) {
        ai_sr_info->get_info_type = GET_VINFO_SIZE;
        ret = ioctl(mUvmHander, UVM_IOC_GET_INFO, &hook_data);
        if (ret < 0) {
            ALOGE("GET_VINFO_SIZE failed.\n");
            goto bypass;
        } else {
            mVInfo_width = ai_sr_info->vinfo_width;
            mVInfo_height = ai_sr_info->vinfo_height;
            ALOGD_IF(nn_check_D(), "vinfo width: %d, height: %d.\n",
                     mVInfo_width,
                     mVInfo_height);
        }
    }

    if (mVInfo_width == 1920 &&
        (ai_sr_info->hf_width > 960 || ai_sr_info->hf_height > 540)) {
        ALOGD_IF(nn_check_D(), "not 540p, don't support.\n");
        goto error;
    }

    if (!mBuf_Alloced) {
        ret = allocDmaBuffer();
        if (ret) {
            ALOGE("%s: alloc buffer fail", __FUNCTION__);
            goto error;
        }
        mBuf_Alloced = true;
    }

    dup_fd = dup(input_fd);
    mDupCount++;
    mTotalDupCount++;

    mBuf_index++;
    if (mBuf_index == SR_OUT_BUF_COUNT)
        mBuf_index = 0;

    mBuf_index_cur = mBuf_index;
    ALOGD_IF(nn_check_D(),
        "%s: i=%d, fence_fd_last =%d, fence_fd=%d, %lld %lld",
        __FUNCTION__,
        mBuf_index,
        mSrBuf[mBuf_index].fence_fd_last,
        mSrBuf[mBuf_index].fence_fd,
        mDupCount,
        mCloseCount);

    mSrBuf[mBuf_index].fence_fd_last = mSrBuf[mBuf_index].fence_fd;
    mSrBuf[mBuf_index].fence_fd = -1;
    mSrBuf[mBuf_index].status = BUF_NN_START;
    mSrBuf[mBuf_index].outFb = outfb;
    mSrBuf[mBuf_index].shared_fd = dup_fd;

    ALOGD_IF(nn_check_D(),
        "%s: dup_fd =%d, buf_index=%d",
        __FUNCTION__,
        dup_fd,
        mBuf_index);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_index_q.push(mBuf_index);
    }

    triggerEvent();
    mNeed_fence = true;
    while (1) {
        ready_size = mBuf_index_q.size();
        if (ready_size >= SR_OUT_BUF_COUNT) {
            usleep(2*1000);
            ALOGE("too many buf need nn process, wait");
        } else
            break;
    }

    return 0;
error:
    ALOGD_IF(nn_check_D(), "set NN_INVALID");
    uvm_hf_info->mode_type = PROCESS_NN;
    uvm_hf_info->shared_fd = input_fd;

    ai_sr_info->shared_fd = input_fd;
    ai_sr_info->nn_status = NN_INVALID;
    ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGE("setinfo input_fd fail =%d", ret);
    }

bypass:
    ALOGD_IF(nn_check_D(), "NN_BYPASS");
    return 0;
}

int32_t NnProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {

    if (!mNeed_fence) {
        if (releaseFence != -1)
            close(releaseFence);
        return 0;
    }

    mSrBuf[mBuf_index].fence_fd = releaseFence;
    mFence_receive_count += 1;

    ALOGD_IF(nn_check_D(),
             "setfence: index=%d, fence_fd=%d, r=%lld,wait=%lld, r-w=%lld",
             mBuf_index,
             releaseFence,
             mFence_receive_count,
             mFence_wait_count,
             mFence_receive_count - mFence_wait_count);

    return 0;
}

int32_t NnProcessor::teardown() {
    mExitThread = true;
    int i;
    int shared_fd = -1;
    int buf_index;
    struct sr_buffer_t *sr_buf;

    struct uvm_hook_data hook_data;
    struct uvm_hf_info_t *uvm_hf_info;
    struct uvm_ai_sr_info *ai_sr_info;
    int ret;

    ALOGD("%s.\n", __FUNCTION__);

    if (mInited)
        pthread_join(mThread, NULL);

    while (mBuf_index_q.size() > 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        buf_index = mBuf_index_q.front();
        sr_buf = &mSrBuf[buf_index];
        shared_fd = sr_buf->shared_fd;

        uvm_hf_info = (struct uvm_hf_info_t *)&hook_data;
        ai_sr_info = &(uvm_hf_info->ai_sr_info);
        uvm_hf_info->mode_type = PROCESS_NN;
        uvm_hf_info->shared_fd = shared_fd;
        ai_sr_info->shared_fd = shared_fd;
        ai_sr_info->nn_out_fd = -1;
        ai_sr_info->nn_status = NN_INVALID;
        ai_sr_info->nn_index = -1;
        ai_sr_info->nn_out_width = 0;
        ai_sr_info->nn_out_height = 0;
        ai_sr_info->nn_mode = -1;

        ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
        if (ret < 0) {
            ALOGE("teardown: UVM_IOC_SET_HF_OUTPUT fail =%d.\n", ret);
        }

        if (shared_fd != -1) {
            close(shared_fd);
            mCloseCount++;
            mTotalCloseCount++;
        }
        mBuf_index_q.pop();
        ALOGD("%s: close fd =%d, buf_index=%d\n", __FUNCTION__, shared_fd, buf_index);
    }

    freeDmaBuffers();

    for (i = 0; i < SR_OUT_BUF_COUNT; i++) {
        if (mSrBuf[i].fence_fd != -1) {
            close(mSrBuf[i].fence_fd);
            mFence_wait_count++;
            ALOGD("%s: close fd=%d", __FUNCTION__, mSrBuf[i].fence_fd);
            mSrBuf[i].fence_fd = -1;
        }
        if (mSrBuf[i].fence_fd_last != -1) {
            close(mSrBuf[i].fence_fd_last);
            mFence_wait_count++;
            ALOGD("%s: close fd_last=%d", __FUNCTION__, mSrBuf[i].fence_fd_last);
            mSrBuf[i].fence_fd_last = -1;
        }
    }

    mBuf_Alloced = false;
    mInited = false;
    return 0;
}

void NnProcessor::threadProcess() {
    int shared_fd = -1;
    struct sr_buffer_t *sr_buf;
    int nn_bypass = false;
    int size = 0;
    int buf_index;
    struct timespec tm_1;
    struct timespec tm_2;
    uint64_t mTime_1;
    uint64_t mTime_2;
    uint64_t nn_time;

    size = mBuf_index_q.size();
    if (size == 0) {
        waitEvent(2 * 1000);
        return;
    }
    if (size > 1)
        ALOGE("%s: more than one buf need process size=%d", __FUNCTION__, size);
    nn_bypass = PropGetInt("vendor.hwc.nn_bypass", 0);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        buf_index = mBuf_index_q.front();
    }

    sr_buf = &mSrBuf[buf_index];
    shared_fd = sr_buf->shared_fd;
    if (sr_buf->fence_fd_last >= 0) {
        sp<Fence> fence = new Fence(sr_buf->fence_fd_last);
        clock_gettime(CLOCK_MONOTONIC, &tm_1);
        status_t res = fence->wait(FENCE_TIMEOUT_MS);
        if (res != OK) {
            ALOGE("wait fence timeout");
        }
        clock_gettime(CLOCK_MONOTONIC, &tm_2);

        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        nn_time = mTime_2 - mTime_1;
        mFence_wait_count += 1;
        ALOGD_IF(nn_check_D(),
                 "fence: wait %lldms, buf_index=%d, fence_fd=%d",
                 nn_time / 1000, buf_index, sr_buf->fence_fd_last);
        if (nn_time > 5000)
            ALOGE("fence: wait too long %lld", nn_time);

        sr_buf->fence_fd_last = -1;
    }
    /*wait pre_process or decoder fence*/
    ai_sr_process(shared_fd, sr_buf, nn_bypass);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_index_q.pop();
    }

    close(shared_fd);
    mCloseCount++;
    mTotalCloseCount++;
    return;
}

void * NnProcessor::threadMain(void * data) {
    NnProcessor * pThis = (NnProcessor *) data;
    struct sched_param param = {0};

    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("%s: Couldn't set SCHED_FIFO: %d.\n", __FUNCTION__, errno);
    }

    MESON_ASSERT(data, "NnProcessor data should not be NULL.\n");

    while (!pThis->mExitThread) {
        pThis->threadProcess();
    }

    ALOGD("%s exit.\n", __FUNCTION__);
    pthread_exit(0);
    return NULL;
}

int NnProcessor::LoadNNModel() {
    ALOGD("NnProcessor: %s start.\n", __FUNCTION__);
    int ret = 1;
    struct timespec time1, time2;
    clock_gettime(CLOCK_MONOTONIC, &time1);
    mNn_qcontext[0] = nn_init(NN_PB_4);
    mNn_qcontext[1] = nn_init(NN_PB_3);
    mNn_qcontext[2] = nn_init(NN_PB_2);

    if (mNeed_check_interlace) {
        mNn_qcontext[3] = nn_init(NN_PB_4_I);
        mNn_qcontext[4] = nn_init(NN_PB_3_I);
        mNn_qcontext[5] = nn_init(NN_PB_2_I);
    }
    clock_gettime(CLOCK_MONOTONIC, &time2);
    uint64_t totalTime = (time2.tv_sec * 1000000LL + time2.tv_nsec / 1000)
                    - (time1.tv_sec * 1000000LL + time1.tv_nsec / 1000);

    if ((mNn_qcontext[0] == NULL) || (mNn_qcontext[1] == NULL) || (mNn_qcontext[2] == NULL) ||
        (mNeed_check_interlace &&
        ((mNn_qcontext[3] == NULL) || (mNn_qcontext[4] == NULL) || (mNn_qcontext[5] == NULL)))) {
        ALOGE("%s: load NN model failed.\n", __FUNCTION__);
        ret = 0;
    } else {
        ALOGD("%s: load NN model spend %lld ns.\n", __FUNCTION__, totalTime);
    }
    return ret;
}

int32_t NnProcessor::ai_sr_process(
    int input_fd,
    struct sr_buffer_t *sr_buf,
    int nn_bypass) {
    int ret;
    struct timespec tm_1;
    struct timespec tm_2;
    uint64_t mTime_1;
    uint64_t mTime_2;
    uint64_t nn_time;
    int need_nn_mode;
    int dump_debug;
    bool hf_info_err = false;
    int nn_mode_index;
    bool mode_changed = false;
    int i;

    struct uvm_hook_data hook_data;
    struct uvm_hf_info_t *uvm_hf_info;
    struct uvm_ai_sr_info *ai_sr_info;

    uvm_hf_info = (struct uvm_hf_info_t *)&hook_data;
    ai_sr_info = &(uvm_hf_info->ai_sr_info);

    uvm_hf_info->mode_type = PROCESS_NN;
    uvm_hf_info->shared_fd = input_fd;

    ai_sr_info->shared_fd = input_fd;

    ai_sr_info->nn_out_fd = sr_buf->fd;
    ai_sr_info->nn_status = NN_WAIT_DOING;
    ai_sr_info->nn_index = mNn_Index++;
    ai_sr_info->nn_out_width = mVInfo_width;
    ai_sr_info->nn_out_height = mVInfo_height;

    ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGE("UVM_IOC_SET_HF_OUTPUT fail =%d.\n", ret);
    }

    ai_sr_info->get_info_type = GET_HF_INFO;
    ret = ioctl(mUvmHander, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(nn_check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
    }
    ALOGD_IF(nn_check_D(),
        "hf_phy=%lld, %d * %d, align: %d * %d, interlace: %d, sf_fd=%d, input_fd=%d.\n",
        ai_sr_info->hf_phy_addr,
        ai_sr_info->hf_width,
        ai_sr_info->hf_height,
        ai_sr_info->buf_align_w,
        ai_sr_info->buf_align_h,
        ai_sr_info->src_interlace_flag,
        sr_buf->fd,
        input_fd);
    ai_sr_info->nn_out_fd = sr_buf->fd;
    if (mVInfo_width == 3840) {
        if (ai_sr_info->hf_align_w == 960) {
            if (ai_sr_info->hf_width > 960 || ai_sr_info->hf_height > 540)
                hf_info_err = true;
            need_nn_mode = NN_MODE_4X4;
            ai_sr_info->buf_align_w = 960;
            ai_sr_info->buf_align_h = 540;
        } else if (ai_sr_info->hf_align_w == 1280) {
            if (ai_sr_info->hf_width > 1280 || ai_sr_info->hf_height > 720)
                hf_info_err = true;
            need_nn_mode = NN_MODE_3X3;
            ai_sr_info->buf_align_w = 1280;
            ai_sr_info->buf_align_h = 720;
        } else if (ai_sr_info->hf_align_w == 1920) {
            if (ai_sr_info->hf_width > 1920 || ai_sr_info->hf_height > 1080)
                hf_info_err = true;
            need_nn_mode = NN_MODE_2X2;
            ai_sr_info->buf_align_w = 1920;
            ai_sr_info->buf_align_h = 1080;
        } else
            return 0;
    } else {
        if (ai_sr_info->hf_align_w == 960) {
            if (ai_sr_info->hf_width > 960 || ai_sr_info->hf_height > 540)
                hf_info_err = true;
            need_nn_mode = NN_MODE_2X2;
            ai_sr_info->buf_align_w = 1920;
            ai_sr_info->buf_align_h = 1080;
        } else {
            ALOGD_IF(nn_check_D(), "not 540p,no need ai_sr.\n");
            return 0;
        }
    }

    if (hf_info_err) {
        ALOGE("hf info err: %d * %d, hf_align: %d * %d, buf_align: %d * %d.\n",
            ai_sr_info->hf_width,
            ai_sr_info->hf_height,
            ai_sr_info->hf_align_w,
            ai_sr_info->hf_align_h,
            ai_sr_info->buf_align_w,
            ai_sr_info->buf_align_h);
        return 0;
    }

    if ((need_nn_mode != mNn_mode) ||
        (mNeed_check_interlace && (mNn_interlace_flag != ai_sr_info->src_interlace_flag))) {
        mNn_mode = need_nn_mode;
        mNn_interlace_flag = ai_sr_info->src_interlace_flag;
        mode_changed = true;
    }

    if ((mNeed_check_interlace && (mNn_interlace_flag == 1)) ||
        !mNeed_check_interlace)
        nn_mode_index = NN_MODE_COUNT - mNn_mode;//interlace
    else
        nn_mode_index = NN_MODE_COUNT - mNn_mode - 3;

    ai_sr_info->nn_mode = mNn_mode;
    ai_sr_info->nn_status = NN_START_DOING;
    ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGE("UVM_IOC_SET_HF_OUTPUT fail =%d.\n", ret);
    }

    clock_gettime(CLOCK_MONOTONIC, &tm_1);

    if (!nn_bypass)
        ret = nn_process_network(mNn_qcontext[nn_mode_index],
                                 (unsigned char *)ai_sr_info->hf_phy_addr,
                                 (unsigned char *)ai_sr_info->nn_out_phy_addr);

    clock_gettime(CLOCK_MONOTONIC, &tm_2);
    if (ret !=0)
        ALOGE("nn_process_network: err: ret=%d.\n", ret);
    else {
        dump_debug = PropGetInt("vendor.hwc.nn_dump", 0);
        if (dump_debug != mDumpHf)
            dump_nn_out(sr_buf);
        mDumpHf = dump_debug;
        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        nn_time = mTime_2 - mTime_1;
        ALOGD_IF(nn_check_D(),
            "nn process %lld index=%d, mNn_mode=%d.\n",
            nn_time,
            ai_sr_info->nn_index,
            mNn_mode);
        if (nn_time > 14000)
            ALOGE("nn time too long %lld index=%d, mNn_mode=%d.\n",
                nn_time,
                ai_sr_info->nn_index,
                mNn_mode);
        if (mode_changed == false) {
            if (mTime[nn_mode_index].count == 0) {
                mTime[nn_mode_index].max_time = nn_time;
                mTime[nn_mode_index].min_time = nn_time;
            }
            mTime[nn_mode_index].count++;
            if (nn_time > mTime[nn_mode_index].max_time)
                mTime[nn_mode_index].max_time = nn_time;
            else if (nn_time < mTime[nn_mode_index].min_time)
                mTime[nn_mode_index].min_time = nn_time;

            mTime[nn_mode_index].total_time += nn_time;
        } else
            ALOGD("nn process mode changed.\n");
    }
    sr_buf->status = BUF_NN_DONE;

    ai_sr_info->nn_status = NN_DONE;
    ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGE("UVM_IOC_SET_HF_OUTPUT fail =%d.\n", ret);
    }

    if ((mNn_Index % 3000) == 0) {
        for (i = 0; i < NN_MODE_COUNT; i++) {
            if (!mNeed_check_interlace && i > 2)
                break;
            if (mTime[i].count > 0) {
                mTime[i].avg_time = mTime[i].total_time / mTime[i].count;
            }
            ALOGD("NnProcessor: time1: i=%d, count=%lld, max=%lld, min=%lld, avg=%lld",
                i,
                mTime[i].count,
                mTime[i].max_time,
                mTime[i].min_time,
                mTime[i].avg_time);
        }
    }
    return ret;
}

void NnProcessor::dump_nn_out(struct sr_buffer_t *sr_buf) {
    const char* dump_path = "/data/nn_out.yuv";
    FILE * dump_file = NULL;

    ALOGD("%s: fd_ptr=%p, phy=%llx, size=%d",
        __FUNCTION__,
        sr_buf->fd_ptr,
        sr_buf->phy,
        sr_buf->size);

    dump_file = fopen(dump_path, "wb");
    if (dump_file != NULL) {
        fwrite(sr_buf->fd_ptr, sr_buf->size, 1, dump_file);
        fclose(dump_file);
    } else
        ALOGE("open %s fail.\n", dump_path);
}

int32_t NnProcessor::waitEvent(int microseconds)
{
    int ret;
    struct timespec pthread_ts;
    struct timeval now;

    gettimeofday(&now, NULL);
    pthread_ts.tv_sec = now.tv_sec + (microseconds + now.tv_usec) / 1000000;
    pthread_ts.tv_nsec = ((microseconds + now.tv_usec) * 1000) % 1000000000;

    pthread_mutex_lock(&m_waitMutex);
    ret = pthread_cond_timedwait(&m_waitCond, &m_waitMutex, &pthread_ts);
    pthread_mutex_unlock(&m_waitMutex);
    return ret;
}

void NnProcessor::triggerEvent(void) {
    pthread_mutex_lock(&m_waitMutex);
    pthread_cond_signal(&m_waitCond);
    pthread_mutex_unlock(&m_waitMutex);
};

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)

int NnProcessor::allocDmaBuffer() {
    unsigned int ion_flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    int buffer_size = 3840 * 2160;
    int i = 0;
    int ret = 0;
    int shared_fd = -1;

    mIonFd = ion_open();
    if (mIonFd < 0) {
        ALOGE("ion open failed!\n");
        return -1;
    }

    ion_user_handle_t ion_hnd;
    int cnt;
    bool query_custom_type = false;
    uint32_t custom_type = ION_HEAP_TYPE_CUSTOM;
    int err = ion_query_heap_cnt(mIonFd, &cnt);
    if (err < 0) {
        ALOGD("ion get heap cnt fail\n");
    }
    std::vector<ion_heap_data> heaps;
    heaps.resize(cnt);
    err = ion_query_get_heaps(mIonFd, cnt, &heaps[0]);
    if (err < 0) {
        ALOGE("ion get heap fail\n");
    }
    for (int i = 0; i < cnt; i ++) {
        ALOGD("heap name:%s id:%d", heaps[i].name, heaps[i].heap_id);
        if (strstr(heaps[i].name, "codec_mm_cma") != NULL) {
            query_custom_type = true;
            custom_type = heaps[i].heap_id;
            break;
        }
    }

    if (!query_custom_type) {
        ALOGE("query_custom_type fail\n");
        return -1;
    }

    while (i < SR_OUT_BUF_COUNT) {
        ALOGD("ion_alloc_fd:0<<i=%d, mIonFd=%d, buffer_size=%d, custom_type=%d, ion_flags=%d, is_legacy=%d",
            i,
            mIonFd,
            buffer_size,
            custom_type,
            ion_flags,
            ion_is_legacy(mIonFd));
        if (ion_is_legacy(mIonFd)) {
            ret = ion_alloc(mIonFd, buffer_size,
                               0,
                               1 << custom_type,
                               ion_flags,
                               &ion_hnd);
            if (ret) {
                ALOGE("ion alloc error, ret=%x\n", ret);
                freeDmaBuffers();
                return -1;
            } else {
                mSrBuf[i].ion_hnd = ion_hnd;
            }
            ret = ion_share(mIonFd, ion_hnd, &shared_fd);
            if (ret) {
                ALOGE("ion share error!\n");
                freeDmaBuffers();
                return -1;
            } else {
                mSrBuf[i].fd = shared_fd;
            }
        } else {
            ret = ion_alloc_fd(mIonFd, buffer_size,
                               0,
                               1 << custom_type,
                               ION_FLAG_EXTEND_MESON_HEAP,
                               &shared_fd);
            if (ret) {
                ALOGE("ion alloc error, ret=%x\n", ret);
                freeDmaBuffers();
                return -1;
            } else {
                mSrBuf[i].fd = shared_fd;
            }
        }

        void *cpu_ptr = mmap(NULL,
                             buffer_size,
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             shared_fd,
                             0);
        if (MAP_FAILED == cpu_ptr) {
            ALOGE("ion mmap error!\n");
            freeDmaBuffers();
            return -1;
        } else {
            mSrBuf[i].fd_ptr = cpu_ptr;
        }
        mSrBuf[i].size = buffer_size;
        mSrBuf[i].outFb = NULL;
        mSrBuf[i].fence_fd = -1;
        mSrBuf[i].fence_fd_last = -1;
        mSrBuf[i].shared_fd = -1;
        mSrBuf[i].status = BUF_INVALID;
        ALOGD("%s: shared_fd=%d, mIonFd=%d, fd_ptr=%p, fd=%d,cpu_ptr=%p\n",
            __FUNCTION__,
            shared_fd,
            mIonFd,
            cpu_ptr,
            shared_fd,
            cpu_ptr);
        i++;
    }
    return ret;
};

int NnProcessor::freeDmaBuffers() {
    int buffer_size = 3840 * 2160;
    int i = 0;

        while (i < SR_OUT_BUF_COUNT) {
            ALOGD("%s: ion_hnd=%d, fd=%d, mIonFd=%d\n",
                __FUNCTION__,
                mSrBuf[i].ion_hnd,
                mSrBuf[i].fd,
                mIonFd);
            if (mSrBuf[i].fd_ptr) {
                munmap(mSrBuf[i].fd_ptr, buffer_size);
                mSrBuf[i].fd_ptr = NULL;
            }
            if (mSrBuf[i].fd != -1) {
                close(mSrBuf[i].fd);
                mSrBuf[i].fd = -1;
            }
            if (mSrBuf[i].ion_hnd != -1) {
                ion_free(mIonFd, mSrBuf[i].ion_hnd);
                mSrBuf[i].ion_hnd = NULL;
            }
            i++;
        }

    int ret = 0;
    if (mIonFd != -1) {
        ret = ion_close(mIonFd);
        mIonFd = -1;
    }
    return ret;
}

