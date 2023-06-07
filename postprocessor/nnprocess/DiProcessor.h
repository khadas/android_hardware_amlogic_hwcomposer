/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef DI_PROCESSOR_H
#define DI_PROCESSOR_H

#include <FbProcessor.h>
#include <queue>

#define DI_OUT_BUF_COUNT 10

struct frame_info_t {
    int32_t in_fd;
    int32_t out_fd;
    int32_t is_repeat;
    int32_t out_fence_fd;
    int32_t is_i;
    int32_t omx_index;
    int32_t need_bypass;
    int32_t reserved[15];
};

enum out_status_e {
    DI_OUT_INVALID = 0,
    DI_OUT_FD = 1,
    DI_OUT_FENCE = 2,
};

struct di_out_t {
    int32_t index;
    int32_t fd;
    int32_t fence;
    bool used;
    int32_t status;
    bool is_i;
};

class DiProcessor : public FbProcessor {
public:
    DiProcessor();
    ~DiProcessor();

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
    meson_fb_processor_t getFbProcessorType() {return FB_DI_PROCESSOR;};
    void threadProcess();
    static void * threadMain(void * data);
    pthread_t mThread;
    bool mExitThread;
    bool mInited;
    bool mNeed_fence;
    int PropGetInt(const char* str, int def);
    int di_check_D();
    static int log_level;
    int mHandler;
    struct di_out_t mDi_Out[DI_OUT_BUF_COUNT];
    int32_t mBuf_index;
    std::queue<int> mBuf_index_q;
    int32_t mLastFd;
    int32_t mLastFenceFd;
    int32_t mLastFenceOutFd;
    bool mLastFrameIsI;
    int32_t mBuf_index_Last;
    int32_t mReceiveCount;
    int32_t mAsyncCount;
    int32_t mDisplayedCount;
};

#define DI_PROCESS_IOC_MAGIC  'I'
#define DI_PROCESS_IOCTL_INIT        _IO(DI_PROCESS_IOC_MAGIC, 0x00)
#define DI_PROCESS_IOCTL_UNINIT      _IO(DI_PROCESS_IOC_MAGIC, 0x01)
#define DI_PROCESS_IOCTL_SET_FRAME   _IOW(DI_PROCESS_IOC_MAGIC, 0x02, struct frame_info_t)
#define DI_PROCESS_IOCTL_Q_OUTPUT    _IOW(DI_PROCESS_IOC_MAGIC, 0x03, int)

#endif

