/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aipq"

#include "AipqProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include "pq_sdk.h"
#include <cutils/properties.h>

#define BUFFER_SIZE 224 * 224 * 3

#define FENCE_TIMEOUT_MS 1000

#define UVM_IOC_MAGIC 'U'

#define UVM_IOC_ATTATCH _IOWR(UVM_IOC_MAGIC, 5, \
                struct uvm_hook_data)
#define UVM_IOC_GET_INFO _IOWR(UVM_IOC_MAGIC, 6, \
                struct uvm_hook_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, \
                struct uvm_hook_data)

int AipqProcessor::mInstanceID = 0;
int64_t AipqProcessor::mTotalDupCount = 0;
int64_t AipqProcessor::mTotalCloseCount = 0;
struct aipq_time_info_t AipqProcessor::mTime;
int AipqProcessor::mSkin_index_class1;
int AipqProcessor::mSkin_index_class2;
bool AipqProcessor::mModelLoaded;
void* AipqProcessor::mNn_qcontext;
int AipqProcessor::mLogLevel = 0;

int AipqProcessor::check_D() {
    return (mLogLevel > 0);
}

static char **scenens_data = NULL;
static char **nn_scenes_data = NULL;
static char **aipq_scenes_data = NULL;
static int scenens_num = 0;
static int nn_scenes_cnt = 0;
static int aipq_scenes_cnt = 0;
static int index = -1;

enum data_mode {
    NN_SCENE_DATA = 0,    //model_type is 0
    NN_MODE_SCENE,        //model_type is 1
    AIPQ_MODE_SCENE,      //AI PQ use data
    UNKNOWN_MODE_SCENE,   //Unknown data
};

static void free_scenes_buffer(enum data_mode mode)
{
    char **data_buffer;
    int i, count;

    switch (mode) {
    case NN_SCENE_DATA:
        data_buffer = scenens_data;
        count = scenens_num;
        break;
    case NN_MODE_SCENE:
        data_buffer = nn_scenes_data;
        count = nn_scenes_cnt;
        break;
    case AIPQ_MODE_SCENE:
        data_buffer = aipq_scenes_data;
        count = aipq_scenes_cnt;
        break;
    default: {
            ALOGD("%s unknown mode=%d\n", __func__, mode);
            return;
        }
    }
    if (data_buffer) {
        for (i = 0; i < count; i++) {
            if (data_buffer[i]) {
                free(data_buffer[i]);
                data_buffer[i] = NULL;
            }
        }
        free(data_buffer);
    }
}

static int get_scenes_buffers(int total, int len, enum data_mode mode)
{
    char **data_buffer;
    int i;

    data_buffer = (char **)malloc(total * sizeof(char*));
    if (!data_buffer) {
        ALOGE("mode: %d, data_buffer malloc fail\n", mode);
        return -1;
    }
    memset(data_buffer, 0, total * sizeof(char*));
    for (i = 0; i < total; i++) {
        data_buffer[i] = (char *)malloc(len * sizeof(char));
        if (!data_buffer[i]) {
            ALOGE("mode: %d data_buffer[%d] malloc fail\n", i, mode);
            while (i--)
                free(data_buffer[i]);
            return -1;
        }
        memset(data_buffer[i], 0, len * sizeof(char));
    }
    switch (mode) {
    case NN_SCENE_DATA:
        scenens_data = data_buffer;
        scenens_num = total;
        break;
    case NN_MODE_SCENE:
        nn_scenes_data = data_buffer;
        nn_scenes_cnt = total;
        break;
    case AIPQ_MODE_SCENE:
        aipq_scenes_data = data_buffer;
        aipq_scenes_cnt = total;
        break;
    default: {
            ALOGD("%s unknown mode=%d\n", __func__, mode);
            return -1;
        }
    }
    return 0;
}

static int do_fill_buffer(char *src, enum data_mode mode, bool is_scenes_data)
{
    char **data_buffer;
    int data_count = -1, tmp = -1, ret = 0;

    switch (mode) {
    case NN_SCENE_DATA:
        data_buffer = scenens_data;
        data_count = scenens_num;
        break;
    case NN_MODE_SCENE:
        data_buffer = nn_scenes_data;
        data_count = nn_scenes_cnt;
        break;
    case AIPQ_MODE_SCENE:
        data_buffer = aipq_scenes_data;
        data_count = aipq_scenes_cnt;
        break;
    default: {
            ALOGD("%s unknown mode=%d, line:%d\n", __func__, mode, __LINE__);
            return -1;
        }
    }
    if (is_scenes_data) {
        memcpy(data_buffer[index], src, strlen(src));
        data_buffer[index][strlen(src) + 1] = '\0';
        ret = 0;
    } else {
        tmp = atoi(src);
        if (tmp >= 0 && tmp < data_count && index < tmp) {
            index = tmp;
            ret = 1;
        }
    }
    return ret;
}

static int get_file_data(char *source_data)
{
    char *src_data = source_data;
    char *token = NULL;
    bool is_total = false, is_lenghth = false, is_description = false;
    int total = 0, len = 0;
    int ret = -1;
    bool begin_get_scenes_data = false, is_scenes_data = false;
    enum data_mode mode;

    do {
        token = strsep(&src_data, "，：；=,:;\n ");
        if (!token) {
            break;
        }

        if (*token == '\0')
            continue;

        if (!is_description) {
            if (strstr(token, "/*")) {
                is_description = true;
                continue;
            }
        }

        if (is_description) {
            if (strstr(token, "*/"))
                is_description = false;
            continue;
        }
        if (is_total) {
            total = atoi(token);
            is_total = false;
            continue;
        }

        if (is_lenghth) {
            len = atoi(token);
            is_lenghth = false;
            continue;
        }

        if (total && len) {
            ret = get_scenes_buffers(total, len, mode);
            if (ret != 0) {
                ALOGE("mode: %d get_scenes_buffers fail\n", mode);
                return -1;
            }
            begin_get_scenes_data = true;
            total = 0;
            len = 0;
        }

        if (!strcmp(token, "scenes_count")) {
            is_total = true;
            continue;
        }
        if (!strcmp(token, "max_len")) {
            is_lenghth = true;
            continue;
        }

        if (!strcmp(token, "scenens_data") ||
            !strcmp(token, "NN_scenes_data") ||
            !strcmp(token, "AIPQ_scenes_data")) {
            index = -1;
            is_scenes_data = false;
            begin_get_scenes_data = false;
            if (!strcmp(token, "scenens_data"))
                mode = NN_SCENE_DATA;
            else if (!strcmp(token, "NN_scenes_data"))
                mode = NN_MODE_SCENE;
            else if (!strcmp(token, "AIPQ_scenes_data"))
                mode = AIPQ_MODE_SCENE;
            else
                mode = UNKNOWN_MODE_SCENE;
            continue;
        }

        if (begin_get_scenes_data) {
            ret = do_fill_buffer(token, mode, is_scenes_data);
            if (ret == 0)
                is_scenes_data = false;
            else if (ret == 1)
                is_scenes_data = true;
            else if (ret < 0) {
                ALOGD("mode: %d do_fill_buffer fail\n", mode);
                return -2;
            }
        }
    } while (token);
    ALOGD("%s end line: %d\n", __func__, __LINE__);
    return 0;
}

static void get_vnn_scenes_data()
{
    FILE *fp = NULL;
    int file_size = 0, ret = 0;
    char *tmp = NULL;

    fp = fopen(AIPQ_SCENE_DATA_PATH, "r");
    if (!fp) {
        ALOGE("open %s err.\n", AIPQ_SCENE_DATA_PATH);
        return;
    }

    fseek(fp, 0 , SEEK_END);
    file_size = ftell(fp);
    if (!file_size) {
        ALOGE("file size is 0\n");
        return;
    }
    fseek(fp, 0, SEEK_SET);
    tmp = (char *)malloc(file_size * sizeof(char));
    if (!tmp) {
        ALOGE("malloc tmp buffer err\n");
        return;
    }
    fread(tmp, file_size , sizeof(char), fp);
    free_scenes_buffer(NN_SCENE_DATA);
    scenens_num = 0;
    scenens_data = NULL;
    free_scenes_buffer(NN_MODE_SCENE);
    nn_scenes_cnt = 0;
    nn_scenes_data = NULL;
    free_scenes_buffer(AIPQ_MODE_SCENE);
    aipq_scenes_cnt = 0;
    aipq_scenes_data = NULL;
    ret = get_file_data(tmp);
    if (ret == 0)
        ALOGD("get vnn scenes data successful\n");
    free(tmp);
}

AipqProcessor::AipqProcessor() {
    ALOGD("%s", __FUNCTION__);
    mBuf_Alloced = false;
    mExitThread = true;
    pthread_mutex_init(&m_waitMutex, NULL);
    pthread_cond_init(&m_waitCond, NULL);
    mIonFd = -1;

    mAipq_Buf.fd = -1;
    mAipq_Buf.fd_ptr = NULL;
    mAipq_Buf.ion_hnd = -1;
    mAipq_Buf.size = -1;

    mInited = false;
    mUvmHander = -1;
    mNn_Index = 0;
    mCacheIndex = 0;
    mBuf_index = 0;

    if (mInstanceID == 0) {
        mTime.count = 0;
        mTime.max_time = 0;
        mTime.min_time = 0;
        mTime.total_time = 0;
        mTime.avg_time = 0;
        mNn_qcontext = NULL;
        mModelLoaded = false;
    }

    if (mInstanceID == 0) {
        isPqInterfaceImplement();
    }

    if (!mModelLoaded)
        LoadNNModel();

    mUvmHander = open("/dev/uvm", O_RDWR | O_NONBLOCK);
    if (mUvmHander < 0) {
        ALOGE("can not open uvm");
    }

    mInstanceID++;
    mDupCount = 0;
    mCloseCount = 0;

    for (int i = 0; i < AI_PQ_TOP; i++) {
        mLastNnValue[i].maxclass = 0;
        mLastNnValue[i].maxprob = 0;
    }

    for (int i = 0; i < AIPQ_MAX_CACHE_COUNT; i++) {
        mAipqIndex[i].buf_index = 0;
        mAipqIndex[i].pq_value_index = 0;
        mAipqIndex[i].shared_fd = 0;
    }
}

AipqProcessor::~AipqProcessor() {
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

    if (mUvmHander) {
        close(mUvmHander);
        mUvmHander = NULL;
    }
}

int AipqProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    //ALOGD("%s is not set used def=%d\n", str, ret);
    return ret;
}

int32_t AipqProcessor::setup() {
    ALOGD("%s", __FUNCTION__);
    if (!mUvmHander) {
        ALOGD("%s: init action is not ok.\n", __FUNCTION__);
        return -1;
    }

    if (mExitThread == true) {
            ALOGD("threadMain creat");
            int ret = pthread_create(&mThread,
                                     NULL,
                                     AipqProcessor::threadMain,
                                     (void *)this);
            if (ret != 0) {
                ALOGE("failed to start AipqProcessor main thread: %s",
                      strerror(ret));
            } else
                mExitThread = false;
    }

    mInited = true;

    return 0;
}

int32_t AipqProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t AipqProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    int ret;
    int ret_attatch = 0;
    buffer_handle_t buf = inputfb->mBufferHandle;
    struct uvm_hook_data hook_data;
    struct uvm_aipq_info_t *uvm_info;
    struct uvm_aipq_info *aipq_info;
    int dup_fd = -1;
    int ready_size = 0;
    int input_fd = -1;
    int pq_value_index;

    mLogLevel = PropGetInt("vendor.hwc.aipq_log", 0);

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


    if (!mUvmHander) {
        goto bypass;
    }

    memset(&hook_data, 0, sizeof(struct uvm_hook_data));

    uvm_info = (struct uvm_aipq_info_t *)&hook_data;
    aipq_info = &(uvm_info->aipq_info);

    uvm_info->mode_type = PROCESS_AIPQ;
    uvm_info->shared_fd = input_fd;
    aipq_info->shared_fd = input_fd;
    aipq_info->need_do_aipq = 0;
    aipq_info->repert_frame = 0;

    {
        std::lock_guard<std::mutex> lock(mMutex_index);

        for (int i = 0; i < AI_PQ_TOP; i++) {
            aipq_info->nn_value[i]= mLastNnValue[i];
        }
        aipq_info->nn_value[AI_PQ_TOP - 1].maxclass = mBuf_index;
        pq_value_index = mLastNnValue[AI_PQ_TOP - 1].maxprob;
    }

    ret_attatch = ioctl(mUvmHander, UVM_IOC_ATTATCH, &hook_data);
    if (ret_attatch != 0) {
        ALOGE("attatch err: ret_attatch =%d", ret_attatch);
        goto bypass;
    }

    if (aipq_info->need_do_aipq == 0) {
        ALOGD_IF(check_D(), "attatch: aipq bypass");
        goto error;
    }

    if (aipq_info->repert_frame != 0) {
        ALOGD_IF(check_D(), "aipq not need do again");
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

    dup_fd = dup(input_fd);
    mDupCount++;
    mTotalDupCount++;

    mAipqIndex[mCacheIndex].buf_index = mBuf_index;
    mAipqIndex[mCacheIndex].pq_value_index = pq_value_index;
    mAipqIndex[mCacheIndex].shared_fd = dup_fd;

    ALOGD_IF(check_D(), "dup_fd =%d, mBuf_index=%d, pq_index=%d, diff=%d",
        dup_fd, mBuf_index, pq_value_index, mBuf_index - pq_value_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.push(mCacheIndex);
    }

    mBuf_index++;
    mCacheIndex++;
    if (mCacheIndex == AIPQ_MAX_CACHE_COUNT)
        mCacheIndex = 0;

    triggerEvent();
    while (1) {
        ready_size = mBuf_fd_q.size();
        if (ready_size >= AIPQ_MAX_CACHE_COUNT) {
            usleep(2*1000);
            ALOGE("too many buf need aipq process, wait ready_size =%d", ready_size);
        } else
            break;
    }

    return 0;
error:
    ALOGD_IF(check_D(), "set NN_INVALID");

bypass:
    return 0;
}

int32_t AipqProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {

    if (releaseFence != -1)
        close(releaseFence);
    return 0;
}

int32_t AipqProcessor::teardown() {
    mExitThread = true;
    int shared_fd = -1;
    int cache_index;

    ALOGD("%s.\n", __FUNCTION__);
    if (mInited)
        pthread_join(mThread, NULL);

    while (mBuf_fd_q.size() > 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
        shared_fd = mAipqIndex[cache_index].shared_fd;
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
    mInited = false;
    return 0;
}

void AipqProcessor::threadProcess() {
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
    shared_fd = mAipqIndex[cache_index].shared_fd;

    ai_pq_process(cache_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.pop();
    }

    close(shared_fd);
    mCloseCount++;
    mTotalCloseCount++;
    return;
}

void * AipqProcessor::threadMain(void * data) {
    AipqProcessor * pThis = (AipqProcessor *) data;
    struct sched_param param = {0};

    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("%s: Couldn't set SCHED_FIFO: %d.\n", __FUNCTION__, errno);
    }

    MESON_ASSERT(data, "AipqProcessor data should not be NULL.\n");

    while (!pThis->mExitThread) {
        pThis->threadProcess();
    }

    ALOGD("%s exit.\n", __FUNCTION__);
    pthread_exit(0);
    return NULL;
}

int AipqProcessor::LoadNNModel() {
    ALOGD("AipqProcessor: %s start.\n", __FUNCTION__);
    int ret = 1;
    struct timespec time1, time2;
    int j = 0;

    clock_gettime(CLOCK_MONOTONIC, &time1);

    get_vnn_scenes_data();
    if (scenens_num > 0) {
        for (j = 0; j < scenens_num; j++)
            ALOGD("scenens_data[%d]: %s\n", j, scenens_data[j]);
    }
    if (nn_scenes_cnt > 0) {
        for (j = 0; j < nn_scenes_cnt; j++)
            ALOGD("nn_scenes_data[%d]: %s\n", j, nn_scenes_data[j]);
    }
    if (aipq_scenes_cnt > 0) {
        for (j = 0; j < aipq_scenes_cnt; j++)
            ALOGD("aipq_scenes_data[%d]: %s\n", j, aipq_scenes_data[j]);
    }
    for (j = 0; j < nn_scenes_cnt; j++) {
        if (strncmp(nn_scenes_data[j],
                    "Skin", strlen(nn_scenes_data[j])) == 0) {
            mSkin_index_class1 = j;
            break;
        }
    }

    for (j = 0; j < aipq_scenes_cnt; j++) {
        if (strncmp(aipq_scenes_data[j],
                    "Skin", strlen(aipq_scenes_data[j])) == 0) {
            mSkin_index_class2 = j;
            break;
        }
    }
    mNn_qcontext = init(AIPQ_NB_PATH, 1);
    if (mNn_qcontext == NULL) {
        ALOGE("ai_pq_init fail! %s\n", AIPQ_NB_PATH);
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

void AipqProcessor::nn_value_reorder(img_classify_out_t *nn_out, struct nn_value_t *scenes) {
    int i = 0;
    int j = 0;
    img_classify_out_t nn_out_original;
    bool has_skin = false;
    bool has_grouppeople = false;
    int has_skin_index;
    int has_grouppeople_index;

    for (i = 0; i < AI_OUT_SCENE; i++) {
        ALOGD_IF(mLogLevel > 1, "1__%3d: %8.6f, %s\n",
            nn_out->topClass[i], nn_out->score[i],
            nn_scenes_data[nn_out->topClass[i]]);
    }

    has_skin = false;
    has_grouppeople = false;
    nn_out_original = *nn_out;

    for (i = 0; i < AI_OUT_SCENE; i++) {
        if (strncmp(nn_scenes_data[nn_out->topClass[i]],
                    "Skin", strlen("Skin")) == 0) {
            has_skin = true;
            has_skin_index = i;
        }
        if (strncmp(nn_scenes_data[nn_out->topClass[i]],
                    "Grouppeople", strlen("Grouppeople")) == 0) {
            has_grouppeople = true;
            has_grouppeople_index = i;
        }
    }
    if (has_grouppeople) {
        if (has_skin) {
            nn_out_original.score[has_skin_index] +=
                nn_out_original.score[has_grouppeople_index];
            for (i = 0; i < AI_OUT_SCENE; i++) {
                for (j = i; j < AI_OUT_SCENE; j++) {
                    if (nn_out_original.score[i] <
                        nn_out_original.score[j]) {
                        float tmp_score =
                                    nn_out_original.score[j];
                        int tmp_topClass =
                                    nn_out_original.topClass[j];
                        nn_out_original.score[j] =
                                    nn_out_original.score[i];
                        nn_out_original.topClass[j] =
                                    nn_out_original.topClass[i];
                        nn_out_original.score[i] = tmp_score;
                        nn_out_original.topClass[i] = tmp_topClass;
                    }
                }
            }
        }else {
            nn_out_original.topClass[has_grouppeople_index] = mSkin_index_class1;
        }
    }
    nn_out = &nn_out_original;

    for (i = 0; i < 5; i++) {
        ALOGD_IF(mLogLevel > 1, "2__%3d: %8.6f, %s\n",
            nn_out->topClass[i], nn_out->score[i],
            nn_scenes_data[nn_out->topClass[i]]);
    }

    for (i = 0; i < AI_OUT_SCENE; i++) {
        for (j = 0; j < MAX_SCENE; j++) {
            if (strncmp(nn_scenes_data[nn_out->topClass[i]],
                        aipq_scenes_data[j],
                        strlen(aipq_scenes_data[j])) == 0) {
                scenes[i].maxclass = j;
                scenes[i].maxprob = nn_out->score[i] * 10000;
                break;
            } else if (j == MAX_SCENE -1) {
                scenes[i].maxclass = MAX_SCENE;
                scenes[i].maxprob = 0;
            }
        }
    }

    int zero_count = 0;
    for (i = AI_OUT_SCENE - 1; i >= 0; i--) {
        if (scenes[i].maxprob == 0) {
            for (j = i; j < AI_OUT_SCENE - 1; j++) {
                scenes[j].maxclass = scenes[j+1].maxclass;
                scenes[j].maxprob = scenes[j+1].maxprob;
            }
            zero_count++;
        }
    }

    for (i = AI_OUT_SCENE - zero_count; i < AI_OUT_SCENE; i++) {
        scenes[i].maxclass = MAX_SCENE;
        scenes[i].maxprob = 0;
    }

    for (i = 0; i < 5; i++)
        ALOGD_IF(mLogLevel > 1, "index=%d, %d: %d, %s\n",
              i, scenes[i].maxclass, scenes[i].maxprob,
              aipq_scenes_data[scenes[i].maxclass]);
}


int32_t AipqProcessor::ai_pq_process(int cache_index) {
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
    img_classify_out_t *nn_out = NULL;
    int input_fd  = mAipqIndex[cache_index].shared_fd;
    bool update_pq_value = false;
    int next_index;

    struct uvm_hook_data hook_data;
    struct uvm_aipq_info_t *uvm_info;
    struct uvm_aipq_info *aipq_info;

    uvm_info = (struct uvm_aipq_info_t *)&hook_data;
    aipq_info = &(uvm_info->aipq_info);

    uvm_info->mode_type = PROCESS_AIPQ;
    uvm_info->shared_fd = input_fd;

    aipq_info->shared_fd = input_fd;
    aipq_info->aipq_fd = mAipq_Buf.fd;

    aipq_info->get_info_type = AIPQ_GET_224_DATA;

    clock_gettime(CLOCK_MONOTONIC, &tm_0);
    ret = ioctl(mUvmHander, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_HF_INFO fail =%d.\n", ret);
        return ret;
    }

    clock_gettime(CLOCK_MONOTONIC, &tm_1);

    nn_out = (img_classify_out_t *)process_network(mNn_qcontext, (unsigned char *)mAipq_Buf.fd_ptr);

    clock_gettime(CLOCK_MONOTONIC, &tm_2);
    if (nn_out == NULL) {
        ALOGE("nn_process_network: err: ret=%d.\n", ret);
        return 0;
    } else {
        dump_index = PropGetInt("vendor.hwc.aipq_dump", 0);
        if (dump_index != mDumpIndex) {
            mDumpIndex = dump_index;
            dump_nn_info();
        }

        mTime_0 = tm_0.tv_sec * 1000000LL + tm_0.tv_nsec / 1000;
        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        ge2d_time = mTime_1 - mTime_0;
        nn_time = mTime_2 - mTime_1;
        ALOGD_IF(check_D(), "aipq_process ge2d %lld, nn %lld, total %lld mNn_Index=%d\n",
            ge2d_time, nn_time, ge2d_time + nn_time, mNn_Index);
        if (nn_time > 20000)
            ALOGE("nn time too long %lld.\n", nn_time);
        mTime.total_time += nn_time;
        mTime.count++;
    }

    nn_value_reorder(nn_out, aipq_info->nn_value);

    {
        std::lock_guard<std::mutex> lock(mMutex_index);

        aipq_info->nn_value[AI_PQ_TOP - 1].maxprob = mNn_Index;

        for (int i = 0; i < AI_PQ_TOP; i++) {
            mLastNnValue[i] = aipq_info->nn_value[i];
        }
    }

    if (cache_index == AIPQ_MAX_CACHE_COUNT - 1)
        next_index = 0;
    else
        next_index = cache_index + 1;

    if (mAipqIndex[next_index].buf_index > mAipqIndex[next_index].pq_value_index + 1
        && mAipqIndex[next_index].buf_index > mAipqIndex[cache_index].buf_index) {
        update_pq_value = true;
        aipq_info->nn_value[AI_PQ_TOP - 1].maxclass = mAipqIndex[next_index].buf_index;
        ALOGD("update_pq_value: %d, %d, new=%d",
            mAipqIndex[next_index].buf_index,
            mAipqIndex[next_index].pq_value_index,
            mNn_Index);
    }

    if (update_pq_value) {
        ret = ioctl(mUvmHander, UVM_IOC_SET_INFO, &hook_data);
        if (ret < 0) {
            ALOGE("UVM_IOC_SET_HF_OUTPUT fail =%d.\n", ret);
        }
    }

    if ((mNn_Index % 3000) == 0) {
            if (mTime.count > 0) {
                mTime.avg_time = mTime.total_time / mTime.count;
            }
            ALOGD("AipqProcessor: time1: count=%lld, max=%lld, min=%lld, avg=%lld",
                mTime.count,
                mTime.max_time,
                mTime.min_time,
                mTime.avg_time);
    }

    mNn_Index++;

    return ret;
}

void AipqProcessor::dump_nn_info() {
    const char* dump_path = "/data/tmp/nn_in.rgb";
    FILE * dump_file = NULL;

    ALOGD("%s: fd_ptr=%p, size=%d",
        __FUNCTION__,
        mAipq_Buf.fd_ptr,
        mAipq_Buf.size);

    dump_file = fopen(dump_path, "wb");
    if (dump_file != NULL) {
        fwrite(mAipq_Buf.fd_ptr, mAipq_Buf.size, 1, dump_file);
        fclose(dump_file);
    } else
        ALOGE("open %s fail.\n", dump_path);
}

int32_t AipqProcessor::waitEvent(int microseconds)
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

void AipqProcessor::triggerEvent(void) {
    pthread_mutex_lock(&m_waitMutex);
    pthread_cond_signal(&m_waitCond);
    pthread_mutex_unlock(&m_waitMutex);
};

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)

int AipqProcessor::allocDmaBuffer() {
    unsigned int ion_flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    int buffer_size = BUFFER_SIZE;

    int ret = 0;
    int shared_fd = -1;

    mIonFd = ion_open();
    if (mIonFd < 0) {
        ALOGE("ion open failed!\n");
        return -1;
    }

    ALOGD("mIonFd=%d, buffer_size=%d, ION_HEAP_TYPE_CUSTOM=%d, ion_flags=%d",
        mIonFd,
        buffer_size,
        ION_HEAP_TYPE_CUSTOM,
        ion_flags);
    ret = ion_alloc_fd(mIonFd, buffer_size,
                       0,
                       1 << 16,
                       ION_FLAG_EXTEND_MESON_HEAP | ion_flags,
                       &shared_fd);
    if (ret) {
        ALOGE("ion alloc error, ret=%x\n", ret);
        freeDmaBuffers();
        return -1;
    } else {
        mAipq_Buf.fd = shared_fd;
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
        mAipq_Buf.fd_ptr = cpu_ptr;
    }
    mAipq_Buf.size = buffer_size;
    ALOGD("%s: shared_fd=%d, mIonFd=%d, fd_ptr=%p, fd=%d\n",
        __FUNCTION__,
        shared_fd,
        mIonFd,
        cpu_ptr,
        shared_fd);

    return ret;
};

int AipqProcessor::freeDmaBuffers() {
    int buffer_size = BUFFER_SIZE;

    ALOGD("%s: ion_hnd=%d, fd=%d, mIonFd=%d\n",
        __FUNCTION__,
        mAipq_Buf.ion_hnd,
        mAipq_Buf.fd,
        mIonFd);
    if (mAipq_Buf.fd_ptr) {
        munmap(mAipq_Buf.fd_ptr, buffer_size);
        mAipq_Buf.fd_ptr = NULL;
    }
    if (mAipq_Buf.fd != -1) {
        close(mAipq_Buf.fd);
        mAipq_Buf.fd = -1;
    }
    if (mAipq_Buf.ion_hnd != -1) {
        ion_free(mIonFd, mAipq_Buf.ion_hnd);
        mAipq_Buf.ion_hnd = NULL;
    }


    int ret = 0;
    if (mIonFd != -1) {
        ret = ion_close(mIonFd);
        mIonFd = -1;
    }
    return ret;
}

