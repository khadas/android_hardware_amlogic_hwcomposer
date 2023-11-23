/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aiface"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include "AiFaceProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <cutils/properties.h>
#include <ui/GraphicBufferAllocator.h>
#include <hardware/gralloc1.h>
#include <hardware/gralloc.h>

#define UVM_IOC_MAGIC 'U'

#define UVM_IOC_ATTACH _IOWR(UVM_IOC_MAGIC, 5, \
                struct uvm_hook_data)
#define UVM_IOC_GET_INFO _IOWR(UVM_IOC_MAGIC, 6, \
                struct uvm_hook_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, \
                struct uvm_hook_data)

#define AIFACE_NB_PATH    "/vendor/bin/nn/AIFace.nb"
#define AIFACE_SKIP_FRAME_HEIGHT    1088

enum nn_status_e {
    NN_INVALID = 0,
    NN_WAIT_DOING = 1,
    NN_START_DOING = 2,
    NN_DONE = 3,
    NN_DISPLAYED = 4
};

int AiFaceProcessor::mInstanceID = 0;
int64_t AiFaceProcessor::mTotalDupCount = 0;
int64_t AiFaceProcessor::mTotalCloseCount = 0;
struct aiface_time_info_t AiFaceProcessor::mTime;
//bool AiFaceProcessor::mModelLoaded;
//void* AiFaceProcessor::mNn_qcontext;
int AiFaceProcessor::mLogLevel = 0;
std::mutex AiFaceProcessor::mMutexAi;

int AiFaceProcessor::check_D() {
    return (mLogLevel > 0);
}

AiFaceProcessor::AiFaceProcessor() {
    ALOGD("%s", __FUNCTION__);
    mNnDoing = false;
    mBuf_Alloced = false;
    mExitThread = true;
    pthread_mutex_init(&m_waitMutex, NULL);
    pthread_cond_init(&m_waitCond, NULL);

    mAiFace_Buf.fd = -1;
    mAiFace_Buf.fd_ptr = NULL;
    mAiFace_Buf.size = -1;
    mAiFace_Buf.buffer_handle = NULL;

    mInited = false;
    mUvmHandler = -1;
    mNn_Index = 0;
    mCacheIndex = 0;
    mBuf_index = 0;
    mThread = 0;

    mNnInputWidth = PropGetInt("vendor.hwc.aiface.nn_width", AIFACE_INPUT_WIDTH);
    mNnInputHeight = PropGetInt("vendor.hwc.aiface.nn_height", AIFACE_INPUT_HEIGH);
    mNnInputChannel = PropGetInt("vendor.hwc.aiface.nn_channel", AIFACE_INPUT_CHANNEL);
    mNnInputFormat = PropGetInt("vendor.hwc.aiface.nn_format", AIFACE_INPUT_ROTMAT);

    if (mInstanceID == 0) {
        mTime.count = 0;
        mTime.max_time = 0;
        mTime.min_time = 0;
        mTime.total_time = 0;
        mTime.avg_time = 0;
        mNn_qcontext = NULL;
        mModelLoaded = false;
        isFaceInterfaceImplement();
    }

    if (!mModelLoaded)
        LoadNNModel();

    mUvmHandler = open("/dev/uvm", O_RDWR | O_NONBLOCK);
    if (mUvmHandler < 0) {
        ALOGE("can not open uvm");
    }

    mInstanceID++;
    mDupCount = 0;
    mCloseCount = 0;
    mDmaBufAddr = NULL;

    for (int i = 0; i < AIFACE_MAX_CACHE_COUNT; i++) {
        mAiFaceIndex[i].buf_index = 0;
        mAiFaceIndex[i].shared_fd = 0;
    }
}

AiFaceProcessor::~AiFaceProcessor() {
    ALOGD("%s: mDupCount =%lld, mCloseCount =%lld, total %lld %lld",
        __FUNCTION__, mDupCount, mCloseCount, mTotalDupCount, mTotalCloseCount);

    if (mDupCount != mCloseCount)
        ALOGE("%s: count err: %lld %lld", __FUNCTION__, mDupCount, mCloseCount);

    if (mTotalDupCount != mTotalCloseCount)
        ALOGE("%s: total count err: %lld %lld",
             __FUNCTION__,mTotalDupCount, mTotalCloseCount);

    if (mInited)
        teardown();

    if (mTime.count > 0) {
        mTime.avg_time = mTime.total_time / mTime.count;
    }
    ALOGD("%s: time: count=%lld, max=%lld, min=%lld, avg=%lld",
        __FUNCTION__, mTime.count, mTime.max_time, mTime.min_time, mTime.avg_time);

    if (mUvmHandler) {
        close(mUvmHandler);
        mUvmHandler = NULL;
    }
}

int AiFaceProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    return ret;
}

int32_t AiFaceProcessor::setup() {
    ATRACE_CALL();
    ALOGD("%s", __FUNCTION__);
    if (!mUvmHandler) {
        ALOGD("%s: init action is not ok.\n", __FUNCTION__);
        return -1;
    }

    if (mExitThread == true) {
            ALOGD("threadMain creat");
            mExitThread = false;
            int ret = pthread_create(&mThread,
                                     NULL,
                                     AiFaceProcessor::threadMain,
                                     (void *)this);
            if (ret != 0) {
                ALOGE("failed to start AiFaceProcessor main thread: %s",
                      strerror(ret));
                mExitThread = true;
            }
    }

    mInited = true;

    return 0;
}

int32_t AiFaceProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t AiFaceProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    ATRACE_CALL();
    int ret;
    int ret_attach = 0;
    buffer_handle_t buf = inputfb->mBufferHandle;
    struct uvm_hook_data hook_data;
    struct uvm_aiface_info_t *uvm_info;
    struct uvm_aiface_info *aiface_info;
    int dup_fd = -1;
    int ready_size = 0;
    int input_fd = -1;

    mLogLevel = PropGetInt("vendor.hwc.aiface_log", 0);

    processFence = -1;
    outfb = inputfb;

    drm_fb_type_t type = inputfb->getFbType();
    if (type == DRM_FB_VIDEO_UVM_DMA ||
        type == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        input_fd = inputfb->getBufferFd();
    } else
        ALOGE("%s: get fd fail type=%d.", __FUNCTION__, type);

    if (input_fd == -1) {
        ALOGD_IF(check_D(), "%s: input_fd invalid.", __FUNCTION__);
        goto bypass;
    }

    if (!mUvmHandler) {
        goto bypass;
    }

    if (!mInited) {
        ALOGE("%s: has teardown need bypass", __FUNCTION__);
        goto bypass;
    }

    memset(&hook_data, 0, sizeof(struct uvm_hook_data));

    uvm_info = (struct uvm_aiface_info_t *)&hook_data;
    aiface_info = &(uvm_info->aiface_info);

    uvm_info->mode_type = PROCESS_AIFACE;
    uvm_info->shared_fd = input_fd;
    aiface_info->shared_fd = input_fd;
    aiface_info->need_do_aiface = 0;
    aiface_info->repeat_frame = 0;
    aiface_info->nn_input_frame_width = mNnInputWidth;
    aiface_info->nn_input_frame_height = mNnInputHeight;
    aiface_info->nn_input_frame_format = mNnInputFormat;

    ret_attach = ioctl(mUvmHandler, UVM_IOC_ATTACH, &hook_data);
    if (ret_attach != 0) {
        ALOGE("attach err: ret_attach =%d", ret_attach);
        goto bypass;
    }

    if (aiface_info->need_do_aiface == 0) {
        ALOGD_IF(check_D(), "attach: aiface bypass");
        goto error;
    }

    ALOGD("dma_buf_addr=%p", aiface_info->dma_buf_addr);
    if (mDmaBufAddr == aiface_info->dma_buf_addr) {
        ALOGD_IF(check_D(), "aiface not need do again");
        goto bypass;
    }

    mDmaBufAddr = aiface_info->dma_buf_addr;

    if (!mBuf_Alloced) {
        ret = allocDmaBuffer();
        if (ret) {
            ALOGE("%s: alloc buffer fail", __FUNCTION__);
            goto error;
        }
        mBuf_Alloced = true;
    }

    //reduce system loading by bypass aiface when 4kh264 video//
    if ((aiface_info->dw_height > AIFACE_SKIP_FRAME_HEIGHT) &&
        ((mBuf_index % 2) == 0)) {
        mBuf_index++;
        goto error;
    }

    ALOGD_IF(check_D(), "set NN_WAIT_DOING: omx_index=%d", aiface_info->omx_index);
    uvm_info->mode_type = PROCESS_AIFACE;
    uvm_info->shared_fd = input_fd;

    aiface_info->shared_fd = input_fd;
    aiface_info->nn_status = NN_WAIT_DOING;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    dup_fd = dup(input_fd);
    mDupCount++;
    mTotalDupCount++;

    mAiFaceIndex[mCacheIndex].buf_index = mBuf_index;
    mAiFaceIndex[mCacheIndex].shared_fd = dup_fd;

    ALOGD_IF(check_D(), "dup_fd =%d, mBuf_index=%d",
        dup_fd, mBuf_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.push(mCacheIndex);
    }
    mBuf_index++;
    mCacheIndex++;
    if (mCacheIndex == AIFACE_MAX_CACHE_COUNT)
        mCacheIndex = 0;

    triggerEvent();
    while (1) {
        ready_size = mBuf_fd_q.size();
        if (ready_size >= AIFACE_MAX_CACHE_COUNT) {
            usleep(2*1000);
            ALOGE("too many buf need aiface process, wait ready_size =%d, mNnDoing=%d",
            ready_size, mNnDoing);
        } else
            break;
    }

    return 0;
error:
    ALOGD_IF(check_D(), "set NN_INVALID");
    uvm_info->mode_type = PROCESS_AIFACE;
    uvm_info->shared_fd = input_fd;

    aiface_info->shared_fd = input_fd;
    aiface_info->nn_status = NN_INVALID;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

bypass:
    return 0;
}

int32_t AiFaceProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {

    if (releaseFence != -1)
        close(releaseFence);
    return 0;
}

int32_t AiFaceProcessor::teardown() {
    ATRACE_CALL();
    mExitThread = true;
    int shared_fd = -1;
    int cache_index;
    struct uvm_hook_data hook_data;
    struct uvm_aiface_info_t *uvm_info;
    struct uvm_aiface_info *aiface_info;
    int ret;

    ALOGD("%s.\n", __FUNCTION__);
    if (mInited && mThread) {
        mInited = false;
        pthread_join(mThread, NULL);
    }

    while (mBuf_fd_q.size() > 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
        shared_fd = mAiFaceIndex[cache_index].shared_fd;

        uvm_info = (struct uvm_aiface_info_t *)&hook_data;
        aiface_info = &(uvm_info->aiface_info);

        uvm_info->mode_type = PROCESS_AIFACE;
        uvm_info->shared_fd = shared_fd;

        aiface_info->shared_fd = shared_fd;
        aiface_info->nn_status = NN_INVALID;

        ret = ioctl(mUvmHandler, UVM_IOC_GET_INFO, &hook_data);
        if (ret < 0) {
            ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
            return ret;
        }

        if (shared_fd != -1) {
            close(shared_fd);
            mCloseCount++;
            mTotalCloseCount++;
        }
        mBuf_fd_q.pop();
        ALOGD("%s: close fd =%d\n", __FUNCTION__, shared_fd);
    }

    freeDmaBuffers();
    mBuf_Alloced = false;
    return 0;
}

void AiFaceProcessor::threadProcess() {
    int shared_fd = -1;
    int size = 0;
    int cache_index;

    size = mBuf_fd_q.size();
    if (size == 0) {
        waitEvent(2 * 1000);
        return;
    }

    if (size > 1)
        ALOGE("%s: more than one buf need process size=%d", __FUNCTION__, size);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
    }
    shared_fd = mAiFaceIndex[cache_index].shared_fd;

    ai_face_process(cache_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.pop();
    }

    close(shared_fd);
    mCloseCount++;
    mTotalCloseCount++;
    return;
}

void * AiFaceProcessor::threadMain(void * data) {
    AiFaceProcessor * pThis = (AiFaceProcessor *) data;
    struct sched_param param = {0};

    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("%s: Couldn't set SCHED_FIFO: %d.\n", __FUNCTION__, errno);
    }

    MESON_ASSERT(data, "AiFaceProcessor data should not be NULL.\n");

    while (!pThis->mExitThread) {
        pThis->threadProcess();
    }

    ALOGD("%s exit.\n", __FUNCTION__);
    pThis->mThread = 0;
    pthread_exit(0);
    return NULL;
}

int AiFaceProcessor::LoadNNModel() {
    ALOGD("AiFaceProcessor: %s start.\n", __FUNCTION__);
    int ret = 1;
    struct timespec time1, time2;

    clock_gettime(CLOCK_MONOTONIC, &time1);

    mNn_qcontext = face_init(AIFACE_NB_PATH, mNnInputWidth, mNnInputHeight, mNnInputChannel);
    if (mNn_qcontext == NULL) {
        ALOGE("ai_pq_init fail! %s\n", AIFACE_NB_PATH);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &time2);
    uint64_t totalTime = (time2.tv_sec * 1000000LL + time2.tv_nsec / 1000)
                    - (time1.tv_sec * 1000000LL + time1.tv_nsec / 1000);

    if (mNn_qcontext == NULL) {
        ALOGE("%s: load NN model failed.\n", __FUNCTION__);
        ret = 0;
    } else {
        mModelLoaded = true;
        ALOGD("%s: load NN model spend %lld ns.\n", __FUNCTION__, totalTime);
    }
    return ret;
}

int32_t AiFaceProcessor::ai_face_process(int cache_index) {
    int ret;
    struct timespec tm_0;
    struct timespec tm_1;
    struct timespec tm_2;
    uint64_t mTime_0;
    uint64_t mTime_1;
    uint64_t mTime_2;
    uint64_t ge2d_time;
    uint64_t nn_time;
    int dump_index;
    face_landmark5_out_t *nn_out = NULL;
    int input_fd  = mAiFaceIndex[cache_index].shared_fd;
    int i;

    struct uvm_hook_data hook_data;
    struct uvm_aiface_info_t *uvm_info;
    struct uvm_aiface_info *aiface_info;

    uvm_info = (struct uvm_aiface_info_t *)&hook_data;
    aiface_info = &(uvm_info->aiface_info);

    uvm_info->mode_type = PROCESS_AIFACE;
    uvm_info->shared_fd = input_fd;

    aiface_info->shared_fd = input_fd;
    aiface_info->aiface_fd = mAiFace_Buf.fd;
    aiface_info->get_info_type = AIFACE_GET_RGB_DATA;
    aiface_info->nn_input_frame_width = mNnInputWidth;
    aiface_info->nn_input_frame_height = mNnInputHeight;
    aiface_info->nn_input_frame_format = mNnInputFormat;

    ALOGD_IF(check_D(), "get RGB DATA.\n");
    clock_gettime(CLOCK_MONOTONIC, &tm_0);
    ret = ioctl(mUvmHandler, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    ALOGD_IF(check_D(), "set NN_START_DOING: omx_index=%d, buf_phy=%lld.\n",
             aiface_info->omx_index, aiface_info->buf_phy_addr);
    aiface_info->nn_status = NN_START_DOING;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    mNnDoing = true;
    clock_gettime(CLOCK_MONOTONIC, &tm_1);

    mMutexAi.lock();
    nn_out = (face_landmark5_out_t *)face_process_network(mNn_qcontext, (unsigned char *)aiface_info->buf_phy_addr, &nn_out_temp);
    mMutexAi.unlock();

    clock_gettime(CLOCK_MONOTONIC, &tm_2);
    mNnDoing = false;
    if (nn_out == NULL) {
        ALOGE("nn_process_network: err: ret=%d.\n", ret);
        return 0;
    } else {
        dump_index = PropGetInt("vendor.hwc.aiface_dump", 0);
        if (dump_index) {
            if (mDumpIndex < dump_index) {
                dump_nn_info(mDumpIndex + 1);
                mDumpIndex++;
            } else {
                ALOGE("finish dump %d vframe.\n", dump_index);
                property_set("vendor.hwc.aiface_dump", "0");
                mDumpIndex = 0;
            }
        }
        ALOGD_IF(check_D(), "nn out_count = %d.\n", nn_out->detNum);
        if (nn_out->detNum >= MAX_FACE_COUNT)
                nn_out->detNum = MAX_FACE_COUNT;

        for (i = 0; i < nn_out->detNum; i++) {
            aiface_info->nn_value[i].x = nn_out->facebox[i].x + 0.5;
            aiface_info->nn_value[i].y = nn_out->facebox[i].y + 0.5;
            aiface_info->nn_value[i].w = nn_out->facebox[i].w + 0.5;
            aiface_info->nn_value[i].h = nn_out->facebox[i].h + 0.5;
            aiface_info->nn_value[i].score = nn_out->facebox[i].score * 10000;
            ALOGD_IF(check_D(), "nn out: omx_index=%d: i=%d: %f, %f, %f, %f, %f\n",
                aiface_info->omx_index,
                i,
                nn_out->facebox[i].x,
                nn_out->facebox[i].y,
                nn_out->facebox[i].w,
                nn_out->facebox[i].h,
                nn_out->facebox[i].score);
        }

        mTime_0 = tm_0.tv_sec * 1000000LL + tm_0.tv_nsec / 1000;
        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        ge2d_time = mTime_1 - mTime_0;
        nn_time = mTime_2 - mTime_1;
        ALOGD_IF(check_D(), "aiface_process ge2d %lld, nn %lld, total %lld mNn_Index=%d\n",
            ge2d_time, nn_time, ge2d_time + nn_time, mNn_Index);
        if (nn_time > 20000)
            ALOGE("nn time too long %lld.\n", nn_time);
        mTime.total_time += nn_time;
        mTime.count++;
    }

    aiface_info->nn_status = NN_DONE;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    if ((mNn_Index % 3000) == 0) {
            if (mTime.count > 0) {
                mTime.avg_time = mTime.total_time / mTime.count;
            }
            ALOGD("AiFaceProcessor: time1: count=%lld, max=%lld, min=%lld, avg=%lld",
                mTime.count,
                mTime.max_time,
                mTime.min_time,
                mTime.avg_time);
    }

    mNn_Index++;

    return ret;
}

void AiFaceProcessor::dump_nn_info(int num) {
    char dump_path[32];
    FILE * dump_file = NULL;

    ALOGD("%s: fd_ptr=%p, size=%d",
        __FUNCTION__,
        mAiFace_Buf.fd_ptr,
        mAiFace_Buf.size);

    snprintf(dump_path, sizeof(dump_path), "/data/aiface_in_%d.rgb", num);
    dump_file = fopen(dump_path, "wb");
    if (dump_file != NULL) {
        fwrite(mAiFace_Buf.fd_ptr, mAiFace_Buf.size, 1, dump_file);
        fclose(dump_file);
    } else
        ALOGE("open %s fail.\n", dump_path);
}

int32_t AiFaceProcessor::waitEvent(int microseconds)
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

void AiFaceProcessor::triggerEvent(void) {
    pthread_mutex_lock(&m_waitMutex);
    pthread_cond_signal(&m_waitCond);
    pthread_mutex_unlock(&m_waitMutex);
};

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)

int AiFaceProcessor::allocDmaBuffer() {
    int buffer_size = mNnInputWidth * mNnInputHeight * 3;
    uint32_t stride;
    int format = 17;
    int gralloc_fd = -1;
    void * cpu_ptr = NULL;
    uint64_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (NO_ERROR != allocService.allocate(
        mNnInputWidth, mNnInputHeight * 2, format, 1, usage,
        &mAiFace_Buf.buffer_handle, &stride, 0, "aiface")) {
        ALOGE("alloc buffer failed");
    }

    if (mAiFace_Buf.buffer_handle) {
        gralloc_fd = am_gralloc_get_buffer_fd((native_handle_t *)mAiFace_Buf.buffer_handle);
        if (gralloc_fd < 0) {
            allocService.free(mAiFace_Buf.buffer_handle);
            ALOGE("get fd fail");
            return -1;
        }

        cpu_ptr = (unsigned char *)mmap(NULL, buffer_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, gralloc_fd, 0);

        if (MAP_FAILED == cpu_ptr) {
            ALOGE("mmap error!");
            freeDmaBuffers();
            return -1;
        } else {
            mAiFace_Buf.fd_ptr = cpu_ptr;
        }
    } else {
        return -1;
    }

    mAiFace_Buf.size = buffer_size;
    mAiFace_Buf.fd = gralloc_fd;
    ALOGD("%s: fd=%d, fd_ptr=%p buffer_size=%d", __FUNCTION__, gralloc_fd, cpu_ptr, buffer_size);

    return 0;
};

int AiFaceProcessor::freeDmaBuffers() {
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (mAiFace_Buf.fd_ptr) {
        munmap(mAiFace_Buf.fd_ptr, mAiFace_Buf.size);
        mAiFace_Buf.fd_ptr = NULL;
    }
    if (mAiFace_Buf.fd != -1)
        mAiFace_Buf.fd = -1;

    if (mAiFace_Buf.buffer_handle) {
        allocService.free(mAiFace_Buf.buffer_handle);
        mAiFace_Buf.buffer_handle = NULL;
    }

    return 0;
}


