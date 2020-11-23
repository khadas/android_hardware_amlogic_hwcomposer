#include <fcntl.h>
#include <unistd.h>

#include <sync/sync.h>
#include <SyncFence.h>
#include <android/sync.h>

#include "sw_sync.h"

// synctime line
SyncTimeline::SyncTimeline() noexcept {
    int fd = sw_sync_timeline_create();
    if (fd == -1)
        return;
    m_fdInitialized = true;
    m_fd = fd;
}

SyncTimeline::~SyncTimeline() {
    destroy();
}

void SyncTimeline::destroy() {
    if (m_fdInitialized) {
        close(m_fd);
        m_fd = -1;
        m_fdInitialized = false;
    }
}

bool SyncTimeline::isValid() const {
    if (m_fdInitialized) {
        int status = fcntl(m_fd, F_GETFD, 0);
        if (status >= 0)
            return true;
        else
            return false;
    } else {
        return false;
    }
}

int SyncTimeline::getFd() const {
    return m_fd;
}
    
int SyncTimeline::inc(int val) {
    return sw_sync_timeline_inc(m_fd, val);
}

static int s_fenceCount;

void SyncFence::setFd(int fd) {
    m_fd = fd;
    m_fdInitialized = true;
}

void SyncFence::clearFd() {
    m_fd = -1;
    m_fdInitialized = false;
}

void SyncFence::destroy() {
    if (isValid()) {
        close(m_fd);
        clearFd();
    }
}

// Wrapper class for sync fence.
SyncFence& SyncFence::operator=(SyncFence &&rhs) noexcept {
    destroy();
    if (rhs.isValid()) {
        setFd(rhs.getFd());
        rhs.clearFd();
    }
    return *this;
}

SyncFence::SyncFence(SyncFence &&fence) noexcept {
    if (fence.isValid()) {
        setFd(fence.getFd());
        fence.clearFd();
    }
}

SyncFence::SyncFence(const SyncFence &fence) noexcept {
    // This is ok, as sync fences are immutable after construction, so a dup
    // is basically the same thing as a copy.
    if (fence.isValid()) {
        int fd = dup(fence.getFd());
        if (fd == -1)
            return;
        setFd(fd);
    }
}

SyncFence::SyncFence(const SyncTimeline *timeline, int value, const char *name ) noexcept {
    std::string autoName = "allocFence";
    autoName += s_fenceCount;
    s_fenceCount++;
    int fd = sw_sync_fence_create(timeline->getFd(), name ? name : autoName.c_str(), value);
    if (fd == -1)
        return;
    setFd(fd);
}

SyncFence::SyncFence(const SyncFence &a, const SyncFence &b, const char *name) noexcept {
    std::string autoName = "mergeFence";
    autoName += s_fenceCount;
    s_fenceCount++;
    int fd = sync_merge(name ? name : autoName.c_str(), a.getFd(), b.getFd());
    if (fd == -1)
        return;
    setFd(fd);
}

SyncFence::SyncFence(int fd) {
    setFd(fd);
}

SyncFence::~SyncFence() {
    destroy();
}

bool SyncFence::isValid() const { 
    if (m_fdInitialized) {
        int status = fcntl(m_fd, F_GETFD, 0);
        if (status >= 0)
            return true;
        else
            return false;
    } else {
        return false;
    }
}

int SyncFence::getFd() const {
    return m_fd;
}

int SyncFence::wait(int timeout) {
    return sync_wait(m_fd, timeout);
}

std::vector<SyncPointInfo> SyncFence::getInfo() const {
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

int SyncFence::getSize() const {
    return getInfo().size();
}

int SyncFence::getSignaledCount() const {
    return countWithStatus(1);
}

int SyncFence::getActiveCount() const {
    return countWithStatus(0);
}

int SyncFence::getErrorCount() const {
    return countWithStatus(-1);
}

int SyncFence::countWithStatus(int status) const {
    int count = 0;
    for (auto &info : getInfo()) {
        if (info.status == status) {
            count++;
        }
    }
    return count;
}
