/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aicolor"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include "AiColorProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <cutils/properties.h>
#include <ui/GraphicBufferAllocator.h>
#include <hardware/gralloc1.h>

#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_ATTACH        _IOWR(UVM_IOC_MAGIC, 5, struct uvm_hook_data)
#define UVM_IOC_GET_INFO      _IOWR(UVM_IOC_MAGIC, 6, struct uvm_hook_data)
#define UVM_IOC_SET_INFO      _IOWR(UVM_IOC_MAGIC, 7, struct uvm_hook_data)

#define AICOLOR_NB_PATH    "/vendor/bin/nn/AIColor.nb"
#define AICOLOR_SKIP_FRAME_HEIGHT    1088

enum nn_status_e {
    NN_INVALID = 0,
    NN_WAIT_DOING = 1,
    NN_START_DOING = 2,
    NN_DONE = 3,
    NN_DISPLAYED = 4
};

int AiColorProcessor::mInstanceID = 0;
int64_t AiColorProcessor::mTotalDupCount = 0;
int64_t AiColorProcessor::mTotalCloseCount = 0;
struct aicolor_time_info_t AiColorProcessor::mTime;
bool AiColorProcessor::mModelLoaded;
void* AiColorProcessor::mNn_qcontext;
int AiColorProcessor::mLogLevel = 0;

int AiColorProcessor::check_D() {
    return (mLogLevel > 0);
}

AiColorProcessor::AiColorProcessor() {
    ALOGD("%s", __FUNCTION__);
    mNnDoing = false;
    mBuf_Alloced = false;
    mExitThread = true;
    pthread_mutex_init(&m_waitMutex, NULL);
    pthread_cond_init(&m_waitCond, NULL);

    mAiColor_Buf.fd = -1;
    mAiColor_Buf.fd_ptr = NULL;
    mAiColor_Buf.size = -1;
    mAiColor_Buf.buffer_handle = NULL;

    mInited = false;
    mUvmHandler = -1;
    mNn_Index = 0;
    mCacheIndex = 0;
    mBuf_index = 0;

    mNnInputWidth = PropGetInt("vendor.hwc.aicolor.nn_width", AICOLOR_INPUT_WIDTH);
    mNnInputHeight = PropGetInt("vendor.hwc.aicolor.nn_height", AICOLOR_INPUT_HEIGH);

    if (mInstanceID == 0) {
        mTime.count = 0;
        mTime.max_time = 0;
        mTime.min_time = 0;
        mTime.total_time = 0;
        mTime.avg_time = 0;
        mNn_qcontext = NULL;
        mModelLoaded = false;
        isColorInterfaceImplement();
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

    for (int i = 0; i < AICOLOR_MAX_CACHE_COUNT; i++) {
        mAiColorIndex[i].buf_index = 0;
        mAiColorIndex[i].shared_fd = 0;
    }
}

AiColorProcessor::~AiColorProcessor() {
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

int AiColorProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    return ret;
}

int32_t AiColorProcessor::setup() {
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
                                     AiColorProcessor::threadMain,
                                     (void *)this);
            if (ret != 0) {
                ALOGE("failed to start AiColorProcessor main thread: %s",
                      strerror(ret));
                mExitThread = true;
            }
    }

    mInited = true;

    return 0;
}

int32_t AiColorProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t AiColorProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    ATRACE_CALL();
    int ret;
    int ret_attach = 0;
    buffer_handle_t buf = inputfb->mBufferHandle;
    struct uvm_hook_data hook_data;
    struct uvm_aicolor_info_t *uvm_info;
    struct uvm_aicolor_info *aicolor_info;
    int dup_fd = -1;
    int ready_size = 0;
    int input_fd = -1;

    mLogLevel = PropGetInt("vendor.hwc.aicolor_log", 0);

    processFence = -1;
    outfb = inputfb;

    if (/*inputfb->mFbType == DRM_FB_VIDEO_OMX_V4L ||*/
        inputfb->mFbType == DRM_FB_VIDEO_UVM_DMA) {
        input_fd = am_gralloc_get_omx_v4l_file(buf);
    } else if (inputfb->mFbType == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        input_fd = inputfb->getVtBuffer();
    } else
        ALOGE("%s: get fd fail mFbType=%d.", __FUNCTION__, inputfb->mFbType);

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

    uvm_info = (struct uvm_aicolor_info_t *)&hook_data;
    aicolor_info = &(uvm_info->aicolor_info);

    uvm_info->mode_type = PROCESS_AICOLOR;
    uvm_info->shared_fd = input_fd;
    aicolor_info->shared_fd = input_fd;
    aicolor_info->need_do_aicolor = 0;
    aicolor_info->repeat_frame = 0;
    aicolor_info->nn_input_frame_width = mNnInputWidth;
    aicolor_info->nn_input_frame_height = mNnInputHeight;

    ret_attach = ioctl(mUvmHandler, UVM_IOC_ATTACH, &hook_data);
    if (ret_attach != 0) {
        ALOGE("attach err: ret_attach =%d", ret_attach);
        goto bypass;
    }

    if (aicolor_info->need_do_aicolor == 0) {
        ALOGD_IF(check_D(), "attach: aicolor bypass");
        goto error;
    }

    if (aicolor_info->repeat_frame != 0) {
        ALOGD_IF(check_D(), "aicolor not need do again");
        goto bypass;
    }

    if (!mBuf_Alloced) {
        ret = allocDmaBuffer();
        if (ret) {
            ALOGE("%s: alloc buffer fail", __FUNCTION__);
            goto error;
        }
        mBuf_Alloced = true;
    }

    //reduce system loading by bypass aiface when 4kh264 video//
    if ((aicolor_info->dw_height > AICOLOR_SKIP_FRAME_HEIGHT) &&
        ((mBuf_index % 2) == 0)) {
        mBuf_index++;
        goto error;
    }

    ALOGD_IF(check_D(), "set NN_WAIT_DOING: omx_index=%d", aicolor_info->omx_index);
    uvm_info->mode_type = PROCESS_AICOLOR;
    uvm_info->shared_fd = input_fd;

    aicolor_info->shared_fd = input_fd;
    aicolor_info->nn_status = NN_WAIT_DOING;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    dup_fd = dup(input_fd);
    mDupCount++;
    mTotalDupCount++;

    mAiColorIndex[mCacheIndex].buf_index = mBuf_index;
    mAiColorIndex[mCacheIndex].shared_fd = dup_fd;

    ALOGD_IF(check_D(), "dup_fd =%d, mBuf_index=%d",
        dup_fd, mBuf_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.push(mCacheIndex);
    }
    mBuf_index++;
    mCacheIndex++;
    if (mCacheIndex == AICOLOR_MAX_CACHE_COUNT)
        mCacheIndex = 0;

    triggerEvent();
    while (1) {
        ready_size = mBuf_fd_q.size();
        if (ready_size >= AICOLOR_MAX_CACHE_COUNT) {
            usleep(2*1000);
            ALOGE("too many buf need aiface process, wait ready_size =%d, mNnDoing=%d",
            ready_size, mNnDoing);
        } else
            break;
    }

    return 0;
error:
    ALOGD_IF(check_D(), "set NN_INVALID");
    uvm_info->mode_type = PROCESS_AICOLOR;
    uvm_info->shared_fd = input_fd;

    aicolor_info->shared_fd = input_fd;
    aicolor_info->nn_status = NN_INVALID;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

bypass:
    return 0;
}

int32_t AiColorProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {

    if (releaseFence != -1)
        close(releaseFence);
    return 0;
}

int32_t AiColorProcessor::teardown() {
    ATRACE_CALL();
    mExitThread = true;
    int shared_fd = -1;
    int cache_index;
    struct uvm_hook_data hook_data;
    struct uvm_aicolor_info_t *uvm_info;
    struct uvm_aicolor_info *aicolor_info;
    int ret;

    ALOGD("%s.\n", __FUNCTION__);
    if (mInited) {
        mInited = false;
        pthread_join(mThread, NULL);
    }

    while (mBuf_fd_q.size() > 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
        shared_fd = mAiColorIndex[cache_index].shared_fd;

        uvm_info = (struct uvm_aicolor_info_t *)&hook_data;
        aicolor_info = &(uvm_info->aicolor_info);

        uvm_info->mode_type = PROCESS_AICOLOR;
        uvm_info->shared_fd = shared_fd;

        aicolor_info->shared_fd = shared_fd;
        aicolor_info->nn_status = NN_INVALID;

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

void AiColorProcessor::threadProcess() {
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
    shared_fd = mAiColorIndex[cache_index].shared_fd;

    ai_color_process(cache_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.pop();
    }

    close(shared_fd);
    mCloseCount++;
    mTotalCloseCount++;
    return;
}

void * AiColorProcessor::threadMain(void * data) {
    AiColorProcessor * pThis = (AiColorProcessor *) data;
    struct sched_param param = {0};

    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("%s: Couldn't set SCHED_FIFO: %d.\n", __FUNCTION__, errno);
    }

    MESON_ASSERT(data, "AiColorProcessor data should not be NULL.\n");

    while (!pThis->mExitThread) {
        pThis->threadProcess();
    }

    ALOGD("%s exit.\n", __FUNCTION__);
    pthread_exit(0);
    return NULL;
}

int AiColorProcessor::LoadNNModel() {
    ALOGD("AiColorProcessor: %s start.\n", __FUNCTION__);
    int ret = 1;
    struct timespec time1, time2;

    clock_gettime(CLOCK_MONOTONIC, &time1);

    mNn_qcontext = color_init(AICOLOR_NB_PATH, 7, mNnInputWidth, mNnInputHeight);
    if (mNn_qcontext == NULL) {
        ALOGE("%s: load %s failed.\n", __FUNCTION__, AICOLOR_NB_PATH);
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

int32_t AiColorProcessor::ai_color_process(int cache_index) {
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
    unsigned char *nn_out = NULL;
    int input_fd  = mAiColorIndex[cache_index].shared_fd;
    int i;

    struct uvm_hook_data hook_data;
    struct uvm_aicolor_info_t *uvm_info;
    struct uvm_aicolor_info *aicolor_info;

    uvm_info = (struct uvm_aicolor_info_t *)&hook_data;
    aicolor_info = &(uvm_info->aicolor_info);

    uvm_info->mode_type = PROCESS_AICOLOR;
    uvm_info->shared_fd = input_fd;

    aicolor_info->shared_fd = input_fd;
    aicolor_info->aicolor_fd = mAiColor_Buf.fd;
    aicolor_info->get_info_type = AICOLOR_GET_RGB_DATA;
    aicolor_info->nn_input_frame_width = mNnInputWidth;
    aicolor_info->nn_input_frame_height = mNnInputHeight;

    clock_gettime(CLOCK_MONOTONIC, &tm_0);
    ret = ioctl(mUvmHandler, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    aicolor_info->nn_status = NN_START_DOING;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    mNnDoing = true;
    clock_gettime(CLOCK_MONOTONIC, &tm_1);

    nn_out = (unsigned char *)color_process_network(mNn_qcontext,
                                                    (unsigned char *)mAiColor_Buf.fd_ptr);

    clock_gettime(CLOCK_MONOTONIC, &tm_2);
    mNnDoing = false;
    if (nn_out == NULL) {
        ALOGE("nn_process_network: err: ret=%d.\n", ret);
        return 0;
    } else {
        dump_index = PropGetInt("vendor.hwc.aicolor_dump", 0);
        if (dump_index != mDumpIndex) {
            mDumpIndex = dump_index;
            dump_nn_info();
        }

        for (i = 0; i < MAX_AICOLOR_COUNT; i++) {
            aicolor_info->nn_value[i] = *(nn_out + i);
            ALOGD_IF(check_D(), "nn out: omx_index=%d: i=%d, nn_value=%d\n",
                aicolor_info->omx_index,
                i,
                aicolor_info->nn_value[i]);
        }

        mTime_0 = tm_0.tv_sec * 1000000LL + tm_0.tv_nsec / 1000;
        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        ge2d_time = mTime_1 - mTime_0;
        nn_time = mTime_2 - mTime_1;
        ALOGD_IF(check_D(), "aicolor_process ge2d %lld, nn %lld, total %lld mNn_Index=%d\n",
            ge2d_time, nn_time, ge2d_time + nn_time, mNn_Index);
        if (nn_time > 20000)
            ALOGE("nn time too long %lld.\n", nn_time);
        mTime.total_time += nn_time;
        mTime.count++;
    }

    aicolor_info->nn_status = NN_DONE;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    if ((mNn_Index % 3000) == 0) {
            if (mTime.count > 0) {
                mTime.avg_time = mTime.total_time / mTime.count;
            }
            ALOGD("AiColorProcessor: time1: count=%lld, max=%lld, min=%lld, avg=%lld",
                mTime.count,
                mTime.max_time,
                mTime.min_time,
                mTime.avg_time);
    }

    mNn_Index++;

    return ret;
}

void AiColorProcessor::dump_nn_info() {
    const char* dump_path = "/data/aicolor_in.rgb";
    FILE * dump_file = NULL;

    ALOGD("%s: fd_ptr=%p, size=%d",
        __FUNCTION__,
        mAiColor_Buf.fd_ptr,
        mAiColor_Buf.size);

    dump_file = fopen(dump_path, "wb");
    if (dump_file != NULL) {
        fwrite(mAiColor_Buf.fd_ptr, mAiColor_Buf.size, 1, dump_file);
        fclose(dump_file);
    } else
        ALOGE("open %s fail.\n", dump_path);
}

int32_t AiColorProcessor::waitEvent(int microseconds)
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

void AiColorProcessor::triggerEvent(void) {
    pthread_mutex_lock(&m_waitMutex);
    pthread_cond_signal(&m_waitCond);
    pthread_mutex_unlock(&m_waitMutex);
};

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)

int AiColorProcessor::allocDmaBuffer() {
    int buffer_size = mNnInputWidth * mNnInputHeight * 3;
    uint32_t stride;
    int format = 17;
    int gralloc_fd = -1;
    void * cpu_ptr = NULL;
    uint64_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (NO_ERROR != allocService.allocate(
        mNnInputWidth, mNnInputHeight * 2, format, 1, usage,
        &mAiColor_Buf.buffer_handle, &stride, 0, "aicolor")) {
        ALOGE("alloc buffer failed");
    }

    if (mAiColor_Buf.buffer_handle) {
        gralloc_fd = am_gralloc_get_buffer_fd((native_handle_t *)mAiColor_Buf.buffer_handle);
        if (gralloc_fd < 0) {
            allocService.free(mAiColor_Buf.buffer_handle);
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
            mAiColor_Buf.fd_ptr = cpu_ptr;
        }
    } else {
        return -1;
    }

    mAiColor_Buf.size = buffer_size;
    mAiColor_Buf.fd = gralloc_fd;
    ALOGD("%s: fd=%d, fd_ptr=%p buffer_size=%d", __FUNCTION__, gralloc_fd, cpu_ptr, buffer_size);

    return 0;
};

int AiColorProcessor::freeDmaBuffers() {
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (mAiColor_Buf.fd_ptr) {
        munmap(mAiColor_Buf.fd_ptr, mAiColor_Buf.size);
        mAiColor_Buf.fd_ptr = NULL;
    }
    if (mAiColor_Buf.fd != -1)
        mAiColor_Buf.fd = -1;

    if (mAiColor_Buf.buffer_handle) {
        allocService.free(mAiColor_Buf.buffer_handle);
        mAiColor_Buf.buffer_handle = NULL;
    }

    return 0;
}
