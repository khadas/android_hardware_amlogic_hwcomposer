/*
// Copyright(c) 2016 Amlogic Corporation
*/

#ifndef UEVENT_OBSERVER_H
#define UEVENT_OBSERVER_H

#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <SimpleThread.h>

namespace android {
namespace amlogic {

typedef void (*UeventListenerFunc)(void *data);

class UeventObserver
{
public:
    UeventObserver();
    virtual ~UeventObserver();

public:
    bool initialize();
    void deinitialize();
    void start();
    void registerListener(const char *event, UeventListenerFunc func, void *data);

private:
    DECLARE_THREAD(UeventObserverThread, UeventObserver);
    void onUevent();

private:
    enum {
        UEVENT_MSG_LEN = 4096,
    };

    char mUeventMessage[UEVENT_MSG_LEN];
    int mUeventFd;
    int mExitRDFd;
    int mExitWDFd;
    struct UeventListener {
        UeventListenerFunc func;
        void *data;
    };
    KeyedVector<String8, UeventListener*> mListeners;
};

} // namespace intel
} // namespace android

#endif

