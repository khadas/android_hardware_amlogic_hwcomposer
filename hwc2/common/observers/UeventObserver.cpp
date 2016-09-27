/*
// Copyright(c) 2016 Amlogic Corporation
*/

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <HwcTrace.h>
#include <UeventObserver.h>
#include <Utils.h>

namespace android {
namespace amlogic {

UeventObserver::UeventObserver()
    : mUeventFd(-1),
      mExitRDFd(-1),
      mExitWDFd(-1),
      mListeners()
{
}

UeventObserver::~UeventObserver()
{
    deinitialize();
}

bool UeventObserver::initialize()
{
    mListeners.clear();

    if (mUeventFd != -1) {
        return true;
    }

    mThread = new UeventObserverThread(this);
    if (!mThread.get()) {
        ETRACE("failed to create uevent observer thread");
        return false;
    }

    // init uevent socket
    struct sockaddr_nl addr;
    // set the socket receive buffer to 64K
    // NOTE: this is only called for once
    int sz = 64 * 1024;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid =  pthread_self() | getpid();
    addr.nl_groups = 0xffffffff;

    mUeventFd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (mUeventFd < 0) {
        DEINIT_AND_RETURN_FALSE("failed to create uevent socket");
    }

    if (setsockopt(mUeventFd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz))) {
        WTRACE("setsockopt() failed");
        //return false;
    }

    if (bind(mUeventFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        DEINIT_AND_RETURN_FALSE("failed to bind scoket");
        return false;
    }

    memset(mUeventMessage, 0, UEVENT_MSG_LEN);

    int exitFds[2];
    if (pipe(exitFds) < 0) {
        ETRACE("failed to make pipe");
        deinitialize();
        return false;
    }
    mExitRDFd = exitFds[0];
    mExitWDFd = exitFds[1];

    return true;
}

void UeventObserver::deinitialize()
{
    if (mUeventFd != -1) {
        if (mExitWDFd != -1) {
            close(mExitWDFd);
            mExitWDFd = -1;
        }
        close(mUeventFd);
        mUeventFd = -1;
    }

    if (mThread.get()) {
        mThread->requestExitAndWait();
        mThread = NULL;
    }

    while (!mListeners.isEmpty()) {
        UeventListener *listener = mListeners.valueAt(0);
        mListeners.removeItemsAt(0);
        delete listener;
    }
}

void UeventObserver::start()
{
    if (mThread.get()) {
        mThread->run("UeventObserver", PRIORITY_URGENT_DISPLAY);
    }
}


void UeventObserver::registerListener(const char *event, UeventListenerFunc func, void *data)
{
    if (!event || !func) {
        ETRACE("invalid event string or listener to register");
        return;
    }

    String8 key(event);
    if (mListeners.indexOfKey(key) >= 0) {
        ETRACE("listener for uevent %s exists", event);
        return;
    }

    UeventListener *listener = new UeventListener;
    if (!listener) {
        ETRACE("failed to create Uevent Listener");
        return;
    }
    listener->func = func;
    listener->data = data;

    mListeners.add(key, listener);
}

bool UeventObserver::threadLoop()
{
    if (mUeventFd == -1) {
        ETRACE("invalid uEvent file descriptor");
        return false;
    }

    struct pollfd fds[2];
    int nr;

    fds[0].fd = mUeventFd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = mExitRDFd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;
    nr = poll(fds, 2, -1);

    if (nr > 0 && fds[0].revents == POLLIN) {
        int count = recv(mUeventFd, mUeventMessage, UEVENT_MSG_LEN - 2, 0);
        if (count > 0) {
            onUevent();
        }
    } else if (fds[1].revents) {
        close(mExitRDFd);
        mExitRDFd = -1;
        ITRACE("exiting wait");
        return false;
    }
    // always looping
    return true;
}

void UeventObserver::onUevent()
{
    char *msg = mUeventMessage;
    const char *envelope = Utils::getUeventEnvelope();
    DTRACE("onUevent: %s", mUeventMessage);
    if (strncmp(msg, envelope, strlen(envelope)) != 0)
        return;

    msg += strlen(msg) + 1;

    UeventListener *listener;
    String8 key;
    while (*msg) {
        key = String8(msg);
        DTRACE("received Uevent: %s", msg);
        if (mListeners.indexOfKey(key) >= 0) {
            DTRACE("received Uevent: %s", msg);
            listener = mListeners.valueFor(key);
            if (listener) {
                listener->func(listener->data);
            } else {
                ETRACE("no listener for uevent %s", msg);
            }
        }
        msg += strlen(msg) + 1;
    }
}

} // namespace intel
} // namespace android

