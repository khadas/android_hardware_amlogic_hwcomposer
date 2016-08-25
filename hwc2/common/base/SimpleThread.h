/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef SIMPLE_THREAD_H
#define SIMPLE_THREAD_H

#include <utils/threads.h>

#define DECLARE_THREAD(THREADNAME, THREADOWNER) \
    class THREADNAME: public Thread { \
    public: \
        THREADNAME(THREADOWNER *owner) { mOwner = owner; } \
        THREADNAME() { mOwner = NULL; } \
    private: \
        virtual bool threadLoop() { return mOwner->threadLoop(); } \
    private: \
        THREADOWNER *mOwner; \
    }; \
    friend class THREADNAME; \
    bool threadLoop(); \
    sp<THREADNAME> mThread;


#endif /* SIMPLE_THREAD_H */

