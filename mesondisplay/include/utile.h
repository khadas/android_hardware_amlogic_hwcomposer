/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MESON_DISPLAY_UTIL_H
#define MESON_DISPLAY_UTIL_H
#define DEBUG
#ifdef DEBUG
#include <unistd.h>
#include <time.h>
#define COLOR_F (getpid()%6)+1
#define COLOR_B 8
#ifndef RECOVERY_MODE
#include <android/log.h>
#define DEBUG_INFO(fmt, arg...) do { __android_log_print(ANDROID_LOG_INFO, "MesonDisplay", fmt " [in %s:%d]\n", ##arg, __func__, __LINE__);}while(0)
#else
#define DEBUG_INFO(fmt, arg...) do { fprintf(stderr, "[meson_display: Debug:PID[%5d]:%8ld]\033[3%d;4%dm " fmt "\033[0m [in %s:%d]\n",getpid(), time(NULL), COLOR_F, COLOR_B, ##arg, __func__, __LINE__);}while(0)
#endif
#else
#define DEBUG_INFO(fmt, arg...)
#endif //DEBUG

#define DEBUG_INFO_ONCE(...) do { \
    static int a = 1;  \
    if (a == 1) { \
        a = 0; \
        DEBUG_INFO(__VA_ARGS__); \
    };} while(0)


#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;      \
    void operator=(const TypeName&) = delete

#ifndef UNUSED
#define UNUSED(s) (void)s
#endif
#ifndef NOTIMPLEMENTED
#define NOTIMPLEMENTED DEBUG_INFO("Function:%s not implemented", __PRETTY_FUNCTION__)
#endif
inline void wait_debug(void) {
    DEBUG_INFO("Waiting for debugger, send SIGCONT to continue (PID=%d)...\033[0m\n",getpid());
  raise(SIGSTOP);
}
#ifndef RECOVERY_MODE
#ifdef DEBUG_USE_BACKTRACE
#include <execinfo.h>
inline void call_stack_dump() {
    const int size = 512;
    int i;
    void* buffer[size];
    char** string;
    int ptrCount = 0;
    DEBUG_INFO("buffer size is %z", sizeof(buffer));
    ptrCount = backtrace(buffer, size);
    DEBUG_INFO("Begin call stack: %d", ptrCount);
    string = backtrace_symbols(buffer, ptrCount);
    if (string != NULL) {
        for (i = 0; i < ptrCount; i++) {
            DEBUG_INFO("%s", string[i]);
        }
        free(string);
    }
}
#else
#ifdef DEBUG_USE_CALLSTACK
//On vendor the CallStack.h can't usable becaluse the libcallstacks can't link.
#include <utils/CallStack.h>
inline void call_stack_dump() {
    android::CallStack cs("meson_display");
}
#else
#ifdef DEBUG_USE_CALLDUMP
#ifndef CALLDUMP_SKIP
#define CALLDUMP_SKIP 0
#endif
inline void call_stack_dump() {
    static int skip_count  = CALLDUMP_SKIP;
    if (skip_count-- == 0) {
        DEBUG_INFO("Call croedump!");
        char* a = NULL;
        *a = 'b';
    }
}
#else
inline void call_stack_dump() {
    DEBUG_INFO("not implemented!");
}

#endif// DEBUG_USE_CALLDUMP
#endif// DEBUG_USE_CALLSTACK
#endif// DEBUG_USE_BACKTRACE
#endif// RECOVERY_MODE

#endif //MESON_DISPLAY_UTIL_H
