#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <cutils/properties.h>
#include <utils/Timers.h>

#include <sync/sync.h>
#include <vector>
#include <string>
#include <android/sync.h>
#include <fcntl.h>

#include <video_tunnel.h>

#include "unittests/sw_sync.h"

#define VT_TIME_STAMP 37510560

// C++ wrapper class for sync timeline.
class SyncTimeline {
    int m_fd = -1;
    bool m_fdInitialized = false;
public:
    SyncTimeline(const SyncTimeline &) = delete;
    SyncTimeline& operator=(SyncTimeline&) = delete;
    SyncTimeline() noexcept {
        int fd = sw_sync_timeline_create();
        if (fd == -1)
            return;
        m_fdInitialized = true;
        m_fd = fd;
    }
    void destroy() {
        if (m_fdInitialized) {
            close(m_fd);
            m_fd = -1;
            m_fdInitialized = false;
        }
    }
    ~SyncTimeline() {
        destroy();
    }
    bool isValid() const {
        if (m_fdInitialized) {
            int status = fcntl(m_fd, F_GETFD, 0);
            if (status >= 0)
                return true;
            else
                return false;
        }
        else {
            return false;
        }
    }
    int getFd() const {
        return m_fd;
    }
    int inc(int val = 1) {
        return sw_sync_timeline_inc(m_fd, val);
    }
};

struct SyncPointInfo {
    std::string driverName;
    std::string objectName;
    uint64_t timeStampNs;
    int status; // 1 sig, 0 active, neg is err
};

static int s_fenceCount;
// Wrapper class for sync fence.
class SyncFence {
    int m_fd = -1;
    bool m_fdInitialized = false;

    void setFd(int fd) {
        m_fd = fd;
        m_fdInitialized = true;
    }
    void clearFd() {
        m_fd = -1;
        m_fdInitialized = false;
    }
public:
    bool isValid() const {
        if (m_fdInitialized) {
            int status = fcntl(m_fd, F_GETFD, 0);
            if (status >= 0)
                return true;
            else
                return false;
        }
        else {
            return false;
        }
    }
    SyncFence& operator=(SyncFence &&rhs) noexcept {
        destroy();
        if (rhs.isValid()) {
            setFd(rhs.getFd());
            rhs.clearFd();
        }
        return *this;
    }
    SyncFence(SyncFence &&fence) noexcept {
        if (fence.isValid()) {
            setFd(fence.getFd());
            fence.clearFd();
        }
    }
    SyncFence(const SyncFence &fence) noexcept {
        // This is ok, as sync fences are immutable after construction, so a dup
        // is basically the same thing as a copy.
        if (fence.isValid()) {
            int fd = dup(fence.getFd());
            if (fd == -1)
                return;
            setFd(fd);
        }
    }
    SyncFence(const SyncTimeline *timeline,
              int value,
              const char *name = nullptr) noexcept {
        std::string autoName = "allocFence";
        autoName += s_fenceCount;
        s_fenceCount++;
        int fd = sw_sync_fence_create(timeline->getFd(), name ? name : autoName.c_str(), value);
        if (fd == -1)
            return;
        setFd(fd);
    }
    SyncFence(const SyncFence &a, const SyncFence &b, const char *name = nullptr) noexcept {
        std::string autoName = "mergeFence";
        autoName += s_fenceCount;
        s_fenceCount++;
        int fd = sync_merge(name ? name : autoName.c_str(), a.getFd(), b.getFd());
        if (fd == -1)
            return;
        setFd(fd);
    }
    SyncFence(const std::vector<SyncFence> &sources) noexcept {
        assert(sources.size());
        SyncFence temp(*begin(sources));
        for (auto itr = ++begin(sources); itr != end(sources); ++itr) {
            temp = SyncFence(*itr, temp);
        }
        if (temp.isValid()) {
            setFd(temp.getFd());
            temp.clearFd();
        }
    }
    void destroy() {
        if (isValid()) {
            close(m_fd);
            clearFd();
        }
    }
    ~SyncFence() {
        destroy();
    }
    int getFd() const {
        return m_fd;
    }
    int wait(int timeout = -1) {
        return sync_wait(m_fd, timeout);
    }
    std::vector<SyncPointInfo> getInfo() const {
        std::vector<SyncPointInfo> fenceInfo;
        struct sync_file_info *info = sync_file_info(getFd());
        if (!info) {
            return fenceInfo;
        }
        const auto fences = sync_get_fence_info(info);
        for (uint32_t i = 0; i < info->num_fences; i++) {
            fenceInfo.push_back(SyncPointInfo{
                fences[i].driver_name,
                fences[i].obj_name,
                fences[i].timestamp_ns,
                fences[i].status});
        }
        sync_file_info_free(info);
        return fenceInfo;
    }
    int getSize() const {
        return getInfo().size();
    }
    int getSignaledCount() const {
        return countWithStatus(1);
    }
    int getActiveCount() const {
        return countWithStatus(0);
    }
    int getErrorCount() const {
        return countWithStatus(-1);
    }
private:
    int countWithStatus(int status) const {
        int count = 0;
        for (auto &info : getInfo()) {
            if (info.status == status) {
                count++;
            }
        }
        return count;
    }
};


struct vt_ctx {
    int dev_fd;
    pthread_t thread_id;
    int tunnel_id;
    int trans_fd;
    int is_exit;
    SyncTimeline *timeline;
};

static void usage(const char* pname) {
    fprintf(stderr,
            "usage: %s [-h] [-c] [-p] [-t tunnel_id] [FILENAME]\n"
            "    -h: print this message\n"
            "    -c: as consumer.\n"
            "    -p: as producer.\n"
            "    -t: specify the tunnel_id\n"
            " FILENAME is the file to transfer\n",
            pname);
}

static int do_write(int fd, char *content) {
    char time[128] = {};
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    sprintf(time, "-%lld\n", now);
    strcat(content, time);

    return write(fd, content, strlen(content));
}

static void *vt_producer_thread(void *arg) {
    struct vt_ctx *ctx = (struct vt_ctx *) arg;
    int dequeue_fd, fence_fd;
    int ret;

    meson_vt_connect(ctx->dev_fd, ctx->tunnel_id, 0);

    while (!ctx->is_exit) {
        char buffer[1024] = "producer";

        meson_vt_queue_buffer(ctx->dev_fd, ctx->tunnel_id, ctx->trans_fd, -1, VT_TIME_STAMP);
        fprintf(stderr, "queuebuffer fd:%d to tunnel:%d\n", ctx->trans_fd, ctx->tunnel_id);
        sleep(1);
        do {
            ret = meson_vt_dequeue_buffer(ctx->dev_fd, ctx->tunnel_id, &dequeue_fd, &fence_fd);
        } while (ret == -EAGAIN);

        do_write(dequeue_fd, buffer);

        fprintf(stderr, "dequeubuffer fd:%d from tunnel:%d", dequeue_fd, ctx->tunnel_id);
    }

    return NULL;
}

static void *vt_consumer_thread(void *arg) {
    struct vt_ctx *ctx = (struct vt_ctx *) arg;
    int ret, times = 0;
    int acquire_fd, fence_fd;
    int64_t time_stamp;

    meson_vt_connect(ctx->dev_fd, ctx->tunnel_id, 1);

    while (!ctx->is_exit) {
        char buffer[1024] = "consumer";
        SyncFence fence(ctx->timeline, times);
        times++;

        do {
            ret = meson_vt_acquire_buffer(ctx->dev_fd, ctx->tunnel_id, &acquire_fd, &fence_fd, &time_stamp);
        } while (ret == -EAGAIN);

        fprintf(stderr, "acqurie buffer fd:%d from tunnel:%d, ret:%d\n", acquire_fd, ctx->tunnel_id, ret);
        do_write(acquire_fd, buffer);

        sleep(1);
        ctx->timeline->inc(1);
        fence_fd = fence.getFd();

        meson_vt_release_buffer(ctx->dev_fd, ctx->tunnel_id, acquire_fd, fence_fd);
        fprintf(stderr, "release buffer fd:%d to tunnel:%d\n", acquire_fd, ctx->tunnel_id);
    }

    return NULL;
}

int main(int argc, char **argv) {
    const char* pname = argv[0];
    int c;
    bool is_producer = true;
    bool is_consumer = false;
    int tunnel_id = 0;
    struct vt_ctx ctx;

    while ((c = getopt(argc, argv, "cpht:")) != -1) {
        switch (c) {
            case 'c':
                is_consumer = true;
                is_producer = false;
                break;
            case 'p':
                is_consumer = false;
                is_producer = true;
                break;
            case 't':
                tunnel_id = atoi(optarg);
                break;
            case '?':
            case 'h':
                usage(pname);
                return 1;
        }
    }

    if (is_producer && is_consumer) {
        fprintf(stderr, "can not be both producer and consumer\n");
        usage(pname);
        return 1;
    }

    argc -= optind;
    argv += optind;

    int fd = -1;
    const char* fn = NULL;

    if (argc == 1) {
        fn = argv[0];
        fd = open(fn, O_WRONLY | O_CREAT , 0664);
        if (fd == -1) {
            fprintf(stderr, "Error opening file: %s (%s)\n", fn, strerror(errno));
            return 1;
        } }

    if (is_producer && fd == -1) {
        usage(pname);
        return 1;
    }

    int dev_fd = meson_vt_open();
    fprintf(stderr, "open viduotunnel dev fd:%d\n", dev_fd);
    ctx.dev_fd = dev_fd;
    ctx.is_exit = false;

    std::shared_ptr<SyncTimeline> timeline;
    int ret = -1;
    if (is_producer) {
        ctx.tunnel_id = -1;
        ret = meson_vt_alloc_id(ctx.dev_fd, &ctx.tunnel_id);
        fprintf(stderr, "alloc viduotunnel id:%d\n", ctx.tunnel_id);
        ctx.trans_fd = fd;

        pthread_create(&ctx.thread_id, NULL, vt_producer_thread, &ctx);
    } else {
        timeline = std::make_shared<SyncTimeline>();
        if (timeline->isValid() == false) {
            return -1;
        }

        ctx.tunnel_id = tunnel_id;
        ctx.timeline = timeline.get();

        pthread_create(&ctx.thread_id, NULL, vt_consumer_thread, &ctx);
    }

    while (true) {
        if (property_get_bool("vendor.meson.vt_producer_exit", false)) {
            if (is_producer) {
                meson_vt_disconnect(ctx.dev_fd, ctx.tunnel_id, 0);
                ctx.is_exit = true;
                pthread_join(ctx.thread_id, NULL);
                break;
            }
        }

        if (property_get_bool("vendor.meson.vt_consumer_exit", false)) {
            if (is_consumer) {
                meson_vt_disconnect(ctx.dev_fd, ctx.tunnel_id, 1);
                ctx.is_exit = true;
                pthread_join(ctx.thread_id, NULL);
                break;
            }
        }

        sleep(2);
    }

    while (true) {
        if (property_get_bool("vendor.meson.vt_exit", false)) {
            break;
        }
        sleep(1);
    }

    if (is_producer)
        ret = meson_vt_free_id(ctx.dev_fd, ctx.tunnel_id);

    meson_vt_close(dev_fd);
    return 0;
}
