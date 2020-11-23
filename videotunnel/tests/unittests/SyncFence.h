#ifndef _MESON_VIDEO_TUNNEL_SYNC_FENCE_H
#define _MESON_VIDEO_TUNNEL_SYNC_FENCE_H

#include <vector>
#include <string>

// C++ wrapper class for sync timeline.
class SyncTimeline {
public:
    SyncTimeline(const SyncTimeline &) = delete;
    SyncTimeline& operator=(SyncTimeline&) = delete;
    SyncTimeline() noexcept;
    ~SyncTimeline();

    void destroy();
    bool isValid() const;
    int getFd() const;
    
    int inc(int val = 1);

protected:
    int m_fd = -1;
    bool m_fdInitialized = false;
};

struct SyncPointInfo {
    std::string driverName;
    std::string objectName;
    uint64_t timeStampNs;
    int status; // 1 sig, 0 active, neg is err
};

// Wrapper class for sync fence.
class SyncFence {
public:
    SyncFence& operator=(SyncFence &&rhs) noexcept;
    SyncFence(SyncFence &&fence) noexcept;
    SyncFence(const SyncFence &fence) noexcept;
    SyncFence(const SyncTimeline *timeline,
              int value,
              const char *name = nullptr) noexcept;
    SyncFence(const SyncFence &a, const SyncFence &b, const char *name = nullptr) noexcept;
    SyncFence(int fd);
    ~SyncFence();

    bool isValid() const;

    void destroy();
    int getFd() const;
    int wait(int timeout = -1);

    std::vector<SyncPointInfo> getInfo() const;
    int getSize() const;
    int getSignaledCount() const;
    int getActiveCount() const;
    int getErrorCount() const;

    void setFd(int fd);
    void clearFd();

protected:
    int countWithStatus(int status) const;

    int m_fd = -1;
    bool m_fdInitialized = false;
};

#endif /* _MESON_VIDEO_TUNNEL_SYNC_FENCE_H */
