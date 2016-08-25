/*
// Copyright(c) 2016 Amlogic Corporation
*/

#ifndef VSYNC_MANAGER_H
#define VSYNC_MANAGER_H

#include <IDisplayDevice.h>
#include <utils/threads.h>

namespace android {
namespace amlogic {


class Hwcomposer;

class VsyncManager {
public:
    VsyncManager(Hwcomposer& hwc);
    virtual ~VsyncManager();

public:
    bool initialize();
    void deinitialize();
    int32_t handleVsyncControl(int disp, bool enabled);
    void resetVsyncSource();
    int getVsyncSource();
    void enableDynamicVsync(bool enable);

private:
    inline int getCandidate();
    inline bool enableVsync(int candidate);
    inline void disableVsync();
    IDisplayDevice* getDisplayDevice(int dispType);

private:
    Hwcomposer &mHwc;
    bool mInitialized;
    bool mEnableDynamicVsync;
    bool mEnabled;
    int  mVsyncSource;
    Mutex mLock;

private:
    // toggle this constant to use primary vsync only or enable dynamic vsync.
    static const bool scUsePrimaryVsyncOnly = false;
};

} // namespace amlogic
} // namespace android



#endif /* VSYNC_MANAGER_H */
