/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MESON_MODE_POLICY_UTIL_H
#define MESON_MODE_POLICY_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BUF_LEN 4096
#define MESON_MODE_LEN 64

#define DEBUG

#ifdef DEBUG
#define COLOR_F (getpid()%6)+1
#define COLOR_B 8

#ifndef UBOOT
#include <log/log.h>

#ifndef LOG_TAG
#define LOG_TAG "MesonHwc"
#endif

#define SYS_LOGV(fmt,...)     ALOGV(fmt, ##__VA_ARGS__)
#define SYS_LOGD(fmt,...)		ALOGD(fmt, ##__VA_ARGS__)
//#define SYS_LOGI(fmt,...)		ALOGI(fmt, ##__VA_ARGS__)
#define SYS_LOGI(fmt,...)		ALOGI("[%s, %s, %d] " fmt, strrchr(__FILE__, '/'), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SYS_LOGW(fmt,...)		ALOGW(fmt, ##__VA_ARGS__)
#define SYS_LOGE(fmt,...)		ALOGE(fmt, ##__VA_ARGS__)
#define SYS_ASSERT(condition,fmt,...) \
        if (!(condition)) { \
            ALOGE(fmt, ##__VA_ARGS__); \
            abort(); \
        }

#else
#define SYS_LOGD(fmt, arg...) do { fprintf(stderr, "[meson_display: Debug:PID[%5d]:%8ld]\033[3%d;4%dm " fmt "\033[0m [in %s:%d]\n",getpid(), time(NULL), COLOR_F, COLOR_B, ##arg, __func__, __LINE__);}while(0)
#endif  //UBOOT

#else
#define SYS_LOGV(fmt,...)		((void)0)
#define SYS_LOGD(fmt,...)		((void)0)
#define SYS_LOGI(fmt,...)		((void)0)
#define SYS_LOGW(fmt,...)		((void)0)

#endif //DEBUG

int32_t meson_mode_write_sys(const char *path, const char *val);
int32_t meson_mode_read_sys(const char *path, char *val, bool original, int valSize);
bool meson_write_valid_mode_sys(const char *path, const char *outputmode);

#ifdef __cplusplus
}
#endif

#endif // MESON_MODE_POLICY_UTIL_H
