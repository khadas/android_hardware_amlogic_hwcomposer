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
#define NOTIMPLEMENTED DEBUG_INFO("Function:%s not implemented", __PRETTY_FUNCTION__);
#endif
#endif //MESON_DISPLAY_UTIL_H
