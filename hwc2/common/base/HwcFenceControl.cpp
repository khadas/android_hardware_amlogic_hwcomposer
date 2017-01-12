/*
 * Copyright (C) 2012 The Android Open Source Project
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

// We would eliminate the non-conforming zero-length array, but we can't since
// this is effectively included from the Linux kernel
#include <sync/sync.h>
#include <sw_sync.h>

#include <HwcFenceControl.h>
#include <unistd.h>
#include <utils/Log.h>
#include <HwcTrace.h>

namespace android {
namespace amlogic {

const sp<HwcFenceControl> HwcFenceControl::NO_FENCE = sp<HwcFenceControl>(new HwcFenceControl);

HwcFenceControl::HwcFenceControl() :
    mFenceFd(-1) {
}

HwcFenceControl::HwcFenceControl(int32_t fenceFd) :
    mFenceFd(fenceFd) {
}

HwcFenceControl::~HwcFenceControl() {
    if (mFenceFd != -1) {
        close(mFenceFd);
    }
}

int32_t HwcFenceControl::createFenceTimeline() {
    int32_t syncTimelineFd;

    syncTimelineFd = sw_sync_timeline_create();
    if (syncTimelineFd < 0) {
        ETRACE("Stark, can't create sw_sync_timeline:");
        return -1;
    }

    return syncTimelineFd;
}

int32_t HwcFenceControl::createFence(int32_t syncTimelineFd,
        char* str, uint32_t val) {

    int32_t fenceFd = sw_sync_fence_create(syncTimelineFd, str, val);
    if (fenceFd < 0) {
        ETRACE("can't create sync pt %d: %s", val, strerror(errno));
        return -1;
    }

    return fenceFd;
}

status_t HwcFenceControl::syncTimelineInc(int32_t syncTimelineFd) {
    status_t err;

    err = sw_sync_timeline_inc(syncTimelineFd, 1);
    if (err < 0) {
        ETRACE("can't increment sync obj:");
        return -1;
    }
    return err;
}

status_t HwcFenceControl::traceFenceInfo(int32_t fence) {
    status_t err;
    struct sync_fence_info_data *info;

    err = sync_wait(fence, 10000);

    if (err < 0) {
        ALOGI("wait %d failed: %s\n", fence, strerror(errno));
    } else {
        ALOGI("wait %d done\n", fence);
    }
    info = sync_fence_info(fence);
    if (info) {
        struct sync_pt_info *pt_info = NULL;
        ALOGI("  fence %s %d\n", info->name, info->status);

        while ((pt_info = sync_pt_info(info, pt_info))) {
            int ts_sec = pt_info->timestamp_ns / 1000000000LL;
            int ts_usec = (pt_info->timestamp_ns % 1000000000LL) / 1000LL;
            ALOGI("    pt %s %s %d %d.%06d", pt_info->obj_name,
                   pt_info->driver_name, pt_info->status,
                   ts_sec, ts_usec);
            if (!strcmp(pt_info->driver_name, "sw_sync"))
                ALOGI(" val=%d\n", *(uint32_t *)pt_info->driver_data);
            else
                ALOGI("\n");
        }
        sync_fence_info_free(info);
    }

    // closeFd( fence);
    return err;
}

status_t HwcFenceControl::wait(int32_t fence, int32_t timeout) {
    if (fence == -1) {
        return NO_ERROR;
    }
    int32_t err = sync_wait(fence, timeout);
    return err < 0 ? -errno : status_t(NO_ERROR);
}

status_t HwcFenceControl::waitForever(const char* logname) {
    if (mFenceFd == -1) {
        return NO_ERROR;
    }
    int32_t warningTimeout = 3000;
    int32_t err = sync_wait(mFenceFd, warningTimeout);
    if (err < 0 && errno == ETIME) {
        ALOGE("%s: fence %d didn't signal in %u ms", logname, mFenceFd,
                warningTimeout);
        err = sync_wait(mFenceFd, TIMEOUT_NEVER);
    }
    return err < 0 ? -errno : status_t(NO_ERROR);
}

int32_t HwcFenceControl::merge(const String8& name, const int32_t& f1,
        const int32_t& f2) {
    int32_t result;
    // Merge the two fences.  In the case where one of the fences is not a
    // valid fence (e.g. NO_FENCE) we merge the one valid fence with itself so
    // that a new fence with the given name is created.
    if (f1 != -1 && f2 != -1) {
        result = sync_merge(name.string(), f1, f2);
    } else if (f1 != -1) {
        result = sync_merge(name.string(), f1, f1);
    } else if (f2 != -1) {
        result = sync_merge(name.string(), f2, f2);
    } else {
        return -1;
    }
    if (result == -1) {
        status_t err = -errno;
        ETRACE("merge: sync_merge(\"%s\", %d, %d) returned an error: %s (%d)",
                name.string(), f1, f2,
                strerror(-err), err);
        return -1;
    }
    return result;
}

int32_t HwcFenceControl::dup() const {
    return ::dup(mFenceFd);
}

int32_t HwcFenceControl::dupFence(int32_t fence) {
    if (-1 == fence) {
        DTRACE("acquire fence already been signaled.");
        return -1;
    }

    int32_t dupFence = ::dup(fence);
    if (dupFence < 0) {
        ETRACE("acquire fence dup failed! please check it immeditely!");
    }

    return dupFence;
}

int32_t HwcFenceControl::getFenceFd() const {
    return mFenceFd;
}

nsecs_t HwcFenceControl::getSignalTime() const {
    if (mFenceFd == -1) {
        return -1;
    }

    struct sync_fence_info_data* finfo = sync_fence_info(mFenceFd);
    if (finfo == NULL) {
        ETRACE("sync_fence_info returned NULL for fd %d", mFenceFd);
        return -1;
    }
    if (finfo->status != 1) {
        sync_fence_info_free(finfo);
        return INT64_MAX;
    }

    struct sync_pt_info* pinfo = NULL;
    uint64_t timestamp = 0;
    while ((pinfo = sync_pt_info(finfo, pinfo)) != NULL) {
        if (pinfo->timestamp_ns > timestamp) {
            timestamp = pinfo->timestamp_ns;
        }
    }
    sync_fence_info_free(finfo);

    return nsecs_t(timestamp);
}

} // namespace amlogic
} // namespace android
