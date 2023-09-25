/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "hwc_di"

#include "DiProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <cutils/properties.h>
#include <sys/mman.h>
#include <sched.h>

#define FENCE_TIMEOUT_MS 1000

int DiProcessor::log_level = 0;

int DiProcessor::di_check_D() {
    return (log_level > 0);
}

DiProcessor::DiProcessor() {
    ALOGD("DiProcessor");
    mExitThread = true;
    mThread = 0;
    mInited = false;
    mLastFd = -1;
    mLastFenceFd = -1;
    mReceiveCount = 0;

    mHandler = open("/dev/di_process.0", O_RDWR | O_NONBLOCK);
    if (mHandler < 0) {
        ALOGE("can not open /dev/di_process.0");
        mHandler = open("/dev/di_process.1", O_RDWR | O_NONBLOCK);
        if (mHandler < 0) {
            ALOGE("can not open /dev/di_process.1");
        }
    }
    ALOGD("DiProcessor: mHandler=%d", mHandler);
}

DiProcessor::~DiProcessor() {
    ALOGD("~DiProcessor");
    if (mInited)
        teardown();

    if (mHandler >= 0) {
        close(mHandler);
        mHandler = -1;
    }

    if (mLastFd >= 0) {
        close(mLastFd);
        mLastFd = -1;
    }
    if (mLastFenceFd >= 0) {
        close(mLastFenceFd);
        mLastFenceFd = -1;
    }
}

int DiProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    //ALOGD("%s is not set used def=%d\n", str, ret);
    return ret;
}

int32_t DiProcessor::setup() {
    int ret;
    int i;

    ALOGD("%s___", __FUNCTION__);

    if (mHandler < 0)
        return -1;

    ret = ioctl(mHandler, DI_PROCESS_IOCTL_INIT, NULL);
    if (ret != 0) {
        ALOGE("init err: ret =%d", ret);
        return -1;
    }

    if (mExitThread == true) {
        mExitThread = false;
        int ret = pthread_create(&mThread,
                                 NULL,
                                 DiProcessor::threadMain,
                                 (void *)this);
        if (ret != 0) {
            ALOGE("failed to start DiProcessor main thread: %s",
                  strerror(ret));
        } else
            mExitThread = false;
    }
    mInited = true;
    mNeed_fence = false;
    mLastFrameIsI = false;
    mLastFenceOutFd = -1;
    mAsyncCount = 0;
    mDisplayedCount = 0;
    mReceiveCount = 0;

    for (i = 0; i < DI_OUT_BUF_COUNT; i++) {
        mDi_Out[i].index  = -i;
        mDi_Out[i].fd  = -1;
        mDi_Out[i].fence = -1;
        mDi_Out[i].used = false;
    }

    return 0;
}

int32_t DiProcessor::teardown() {
    int ret;
    int i;

    ALOGD("%s.\n", __FUNCTION__);
    mExitThread = true;

    if (mInited && mThread) {
        pthread_join(mThread, NULL);
        mThread = 0;
    }

    mInited = false;

    if (mLastFenceOutFd >= 0) {
        ALOGD("%s: wait last fence\n", __FUNCTION__);
        sp<Fence> fence = new Fence(mLastFenceOutFd);
        status_t res = fence->wait(FENCE_TIMEOUT_MS);
        if (res != OK) {
            ALOGE("teardown: wait fence timeout");
        }
        ALOGD("%s: wait last fence done\n", __FUNCTION__);
        mLastFenceOutFd = -1;
    }

    for (i = 0; i < DI_OUT_BUF_COUNT; i++) {
        if (mDi_Out[i].used) {
            ALOGE("release mDi_Out i =%d", i);
            if (mDi_Out[i].fd >= 0) {
                close(mDi_Out[i].fd);
                mDi_Out[i].fd = -1;
            }
            if (mDi_Out[i].fence >= 0) {
                close(mDi_Out[i].fence);
                mDi_Out[i].fence = -1;
            }
            mDi_Out[i].used = false;
        }
    }

    if (mLastFd >= 0) {
        close(mLastFd);
        mLastFd = -1;
    }
    if (mLastFenceFd >= 0) {
        close(mLastFenceFd);
        mLastFenceFd = -1;
    }

    ret = ioctl(mHandler, DI_PROCESS_IOCTL_UNINIT, NULL);
    if (ret != 0) {
        ALOGE("uninit err: ret =%d", ret);
        return -1;
    }

    return 0;
}

int32_t DiProcessor::reset() {
    int ret = 0;

    ALOGD("reset");
    ret = teardown();
    if (ret)
        ALOGE("teardown err: ret =%d", ret);

    if (mHandler >= 0) {
        close(mHandler);
        mHandler = -1;
    }

    mHandler = open("/dev/di_process.0", O_RDWR | O_NONBLOCK);
    if (mHandler < 0) {
        ALOGE("can not open /dev/di_process.0");
        mHandler = open("/dev/di_process.1", O_RDWR | O_NONBLOCK);
        if (mHandler < 0) {
            ALOGE("can not open /dev/di_process.1");
        }
    }
    ret = setup();
    if (ret)
        ALOGE("setup err: ret =%d", ret);
    return ret;
}

int32_t DiProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t DiProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    int input_fd = -1;
    int output_fd = -1;
    struct frame_info_t frame_info;
    int ret;
    int i;
    int disp_q_size;

    log_level = PropGetInt("vendor.hwc.di_log", 0);

    mNeed_fence = false;
    processFence = -1;
    outfb = inputfb;
    outfb->setDiProcessorFd(-2);

    mAsyncCount++;

    drm_fb_type_t type = inputfb->getFbType();
    if (type == DRM_FB_VIDEO_UVM_DMA ||
        type == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        input_fd = inputfb->getBufferFd();
    } else
        ALOGE("%s: get fd fail type=%d.", __FUNCTION__, type);

    if (input_fd == -1) {
        ALOGD_IF(di_check_D(), "%s: input_fd invalid.", __FUNCTION__);
        return -1;
    }

    frame_info.in_fd = input_fd;

    ret = ioctl(mHandler, DI_PROCESS_IOCTL_SET_FRAME, &frame_info);
    if (ret == 1) {
        ALOGD("need reinit di for tvp");
        reset();
        mAsyncCount++;
        ret = ioctl(mHandler, DI_PROCESS_IOCTL_SET_FRAME, &frame_info);
    }
    if (ret != 0) {
        ALOGE("set frame err: ret =%d", ret);
        return -1;
    }

    mReceiveCount++;

    ALOGD_IF(di_check_D(), "%s: input_fd =%d, outfd=%d, is_repeat=%d, fence_fd=%d, omx_index=%d, mReceiveCount=%d,bypass=%d, tvp=%d",
        __FUNCTION__, input_fd, frame_info.out_fd, frame_info.is_repeat, frame_info.out_fence_fd,
        frame_info.omx_index, mReceiveCount, frame_info.need_bypass, frame_info.is_tvp);

    if (frame_info.need_bypass) {
        mLastFrameIsI = false;
        frame_info.out_fd = -1;
        frame_info.out_fence_fd = -1;
        goto USE_ORG;
    }
    if (frame_info.is_repeat == 0) {
        mNeed_fence = true;
        if (frame_info.is_i) {
            if (mLastFrameIsI) {
                mFirstI = false;
                outfb->setDiProcessorFd(mLastFd);
                output_fd = mLastFd;
                processFence = mLastFenceFd;
                ALOGD_IF(di_check_D(), "%s: repeat 0; I->I: outfd=%d, outfencefd=%d",
                    __FUNCTION__, mLastFd, processFence);
                mLastFd = frame_info.out_fd;
                mLastFenceFd = frame_info.out_fence_fd;
            } else {
                mLastFrameIsI = frame_info.is_i;
                mFirstI = true;
                ALOGD_IF(di_check_D(), "%s: repeat 0; P->I: outfd=-2, outfencefd=-1", __FUNCTION__);
                goto USE_ORG;
            }
        } else {
            if (mLastFrameIsI) {
                while (1) {
                    for (i = 0; i < DI_OUT_BUF_COUNT; i++) {
                        if (mDi_Out[i].used == false)
                            break;
                    }
                    if (i < DI_OUT_BUF_COUNT) {
                        break;
                    } else
                        usleep(2*1000);
                }
                mDi_Out[i].fd = mLastFd;
                mDi_Out[i].used = true;
                mDi_Out[i].is_i = true;
                mDi_Out[i].fence = mLastFenceFd;
                mDi_Out[i].status = DI_OUT_FENCE;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mBuf_index_q.push(i);
                }

                ALOGD("I to P:  push the last I to list for recycle");
            }
            outfb->setDiProcessorFd(frame_info.out_fd);
            output_fd = frame_info.out_fd;
            processFence = frame_info.out_fence_fd;
            mLastFd = -1;
            mLastFenceFd = -1;
            ALOGD_IF(di_check_D(), "%s: repeat 0; I/P->P: outfd=%d, outfencefd=%d",
                __FUNCTION__, output_fd, processFence);
        }
    } else {
        if (frame_info.is_i) {
            /*case1:P I0 I0 I0: first I0 bypass; second I0 not bypass, need return I0 to vc*/
            /*first I0 bypass,  mLastFrameIsI is false*/
            /*case2:P I0 I0 I1 I1 I2: two I0 bypass, I1 disp, two I1 need use_org*/
            if (!mLastFrameIsI || mFirstI) {
                ALOGD("need org2: mLastFrameIsI=%d, mFirstI=%d", mLastFrameIsI, mFirstI);
                mNeed_fence = false;
                outfb->setDiProcessorFd(-2);
                output_fd = -2;
                processFence = -1;
                if (mLastFd >= 0) {
                    close(mLastFd);
                    mLastFd = -1;
                }
                if (!mLastFrameIsI) {
                    if (mLastFenceFd >= 0) {
                        close(mLastFenceFd);
                        mLastFenceFd = -1;
                    }
                    mLastFenceFd = frame_info.out_fence_fd;
                }
                mLastFrameIsI = true;
                mLastFd = frame_info.out_fd;
                ALOGD("use org2: processFence=%d, omx_index=%d,bypass=%d, tvp=%d, mFirstI=%d",
                    processFence, frame_info.omx_index,
                    frame_info.need_bypass, frame_info.is_tvp, mFirstI);
                return 0;
            } else {
                output_fd = dup(mDi_Out[mBuf_index].fd);
                outfb->setDiProcessorFd(output_fd);
                processFence = dup(mLastFenceOutFd);
                if (frame_info.out_fd >= 0) {
                    close(frame_info.out_fd);
                }
                if (frame_info.out_fence_fd >= 0) {
                    close(frame_info.out_fence_fd);
                }
            }
            ALOGD_IF(di_check_D(), "%s: repeat 1; last I, cur I", __FUNCTION__);
        } else {
            if (mLastFrameIsI) {
                ALOGD("I->P:last I frame not sent to vc, but need wait fence to recycle di buffer");
                while (1) {
                    for (i = 0; i < DI_OUT_BUF_COUNT; i++) {
                        if (mDi_Out[i].used == false)
                            break;
                    }
                    if (i < DI_OUT_BUF_COUNT) {
                        break;
                    } else
                        usleep(2*1000);
                }
                mDi_Out[i].fd = mLastFd;
                mDi_Out[i].used = true;
                mDi_Out[i].is_i = frame_info.is_i;
                mDi_Out[i].fence = mLastFenceFd;
                mDi_Out[i].status = DI_OUT_FENCE;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mBuf_index_q.push(i);
                }

            } else {
                if (mLastFd >= 0) {
                    close(mLastFd);
                    mLastFd = -1;
                }
                if (mLastFenceFd >= 0) {
                    close(mLastFenceFd);
                    mLastFenceFd = -1;
                }
            }
            outfb->setDiProcessorFd(frame_info.out_fd);
            output_fd = frame_info.out_fd;

            processFence = dup(mLastFenceOutFd);
            if (frame_info.out_fence_fd >= 0) {
                ALOGE("P: repert, out_fence_fd =%d", frame_info.out_fence_fd);
                close(frame_info.out_fence_fd);
            }

            mLastFd = -1;
            mLastFenceFd = -1;
            ALOGD_IF(di_check_D(), "%s: repeat 1; last P, cur P", __FUNCTION__);
        }
        mLastFrameIsI = frame_info.is_i;

        if (processFence >= 0) {
            if (mLastFenceOutFd >= 0)
                close(mLastFenceOutFd);
            mLastFenceOutFd = dup(processFence);
        }
        return 0;
    }

    if (processFence >= 0) {
        if (mLastFenceOutFd >= 0)
            close(mLastFenceOutFd);
        mLastFenceOutFd = dup(processFence);
    }

    mLastFrameIsI = frame_info.is_i;

    while (1) {
        for (i = 0; i < DI_OUT_BUF_COUNT; i++) {
            if (mDi_Out[i].used == false)
                break;
        }
        if (i < DI_OUT_BUF_COUNT) {
            break;
        } else
            usleep(2*1000);
    }
    mDi_Out[i].fd = dup(output_fd);
    mDi_Out[i].status = DI_OUT_FD;
    mDi_Out[i].used = true;
    mDi_Out[i].is_i = frame_info.is_i;
    mBuf_index_Last = mBuf_index;
    mBuf_index = i;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        disp_q_size = mBuf_index_q.size();
    }
    ALOGD_IF(di_check_D(), "%s: mDi_Out[i].fd=%d, i=%d, is_i=%d, disp_q_size=%d",
        __FUNCTION__, mDi_Out[i].fd, i, mDi_Out[i].is_i, disp_q_size);

    return 0;

USE_ORG:
    mNeed_fence = false;
    outfb->setDiProcessorFd(-2);
    output_fd = -2;
    processFence = -1;
    if (mLastFd >= 0) {
        close(mLastFd);
        mLastFd = -1;
    }
    if (mLastFenceFd >= 0) {
        close(mLastFenceFd);
        mLastFenceFd = -1;
    }
    mLastFd = frame_info.out_fd;
    mLastFenceFd = frame_info.out_fence_fd;
    ALOGD("use org:is_repeat=%d, omx_index=%d,bypass=%d, tvp=%d, mFirstI=%d",
        frame_info.is_repeat, frame_info.omx_index,
        frame_info.need_bypass, frame_info.is_tvp, mFirstI);
    return 0;
}

int32_t DiProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {
    int index = mBuf_index;
    int disp_size;

    mDisplayedCount++;

    ALOGD_IF(di_check_D(), "%s: count %d %d, diff=%d",
        __FUNCTION__, mAsyncCount, mDisplayedCount, mAsyncCount - mDisplayedCount);

    if (mAsyncCount > mDisplayedCount)
        ALOGE("%s: count %d %d", __FUNCTION__, mAsyncCount, mDisplayedCount);

    if (!mNeed_fence) {
        if (releaseFence != -1)
            close(releaseFence);
        return 0;
    }

    if (mDi_Out[index].status != DI_OUT_FD)
        ALOGE("%s: status err %d", __FUNCTION__, mDi_Out[index].status);

    mDi_Out[index].fence = releaseFence;
    mDi_Out[index].status = DI_OUT_FENCE;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_index_q.push(index);
    }

    ALOGD_IF(di_check_D(), "push i=%d to list", index);

    while (1) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            disp_size = mBuf_index_q.size();
        }

        if (disp_size > 4) {
            usleep(2*1000);
            ALOGE("too many buf need di process, wait: disp_size=%d", disp_size);
        } else
            break;
    }

    return 0;
}

void DiProcessor::threadProcess() {
    int size;
    int buf_index;
    struct timespec tm_1;
    struct timespec tm_2;
    uint64_t mTime_1;
    uint64_t mTime_2;
    uint64_t wait_time;
    int ret;
    int fd;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        size = mBuf_index_q.size();
    }

    if (size < 2) {
        usleep(2 * 1000);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        buf_index = mBuf_index_q.front();
    }

    if (mDi_Out[buf_index].status != DI_OUT_FENCE)
        ALOGE("%s: status err %d", __FUNCTION__, mDi_Out[mBuf_index].status);

    if (mDi_Out[buf_index].fence >= 0) {
        sp<Fence> fence = new Fence(mDi_Out[buf_index].fence);
        clock_gettime(CLOCK_MONOTONIC, &tm_1);
        status_t res = fence->wait(FENCE_TIMEOUT_MS);
        if (res != OK) {
            ALOGE("wait fence timeout");
        }
        clock_gettime(CLOCK_MONOTONIC, &tm_2);

        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        wait_time = mTime_2 - mTime_1;
        ALOGD_IF(di_check_D(),
                 "fence: wait %" PRIu64 "ms, buf_index=%d, fence_fd=%d",
                 wait_time / 1000, buf_index, mDi_Out[buf_index].fence);
        if (wait_time > 50 * 1000)
            ALOGE("fence: wait too long %" PRIu64, wait_time);
    }

    fd = mDi_Out[buf_index].fd;
    ret = ioctl(mHandler, DI_PROCESS_IOCTL_Q_OUTPUT, &fd);
    if (ret != 0) {
        ALOGE("q_buf err: ret =%d", ret);
        return;
    }

    close(mDi_Out[buf_index].fd);

    mDi_Out[buf_index].fence = -1;
    mDi_Out[buf_index].fd = -1;
    mDi_Out[buf_index].status = DI_OUT_INVALID;
    mDi_Out[buf_index].used = false; /*must at the last line*/
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_index_q.pop();
    }

    return;
}

void * DiProcessor::threadMain(void * data) {
    DiProcessor * pThis = (DiProcessor *) data;
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
    pThis->mThread = 0;
    pthread_exit(0);
    return NULL;
}

