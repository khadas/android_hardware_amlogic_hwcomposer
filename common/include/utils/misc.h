/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MISC_H
#define MISC_H

#include <stdlib.h>

#if PLATFORM_SDK_VERSION < 28
#include <gralloc_priv.h>
#endif

#define MAX_STR_LEN         512
#define PROP_VALUE_LEN_MAX  92

bool sys_get_bool_prop(const char* prop, bool defVal);
int32_t sys_get_string_prop(const char* prop, char * val);
int32_t sys_set_prop(const char *prop, const char *val);

int32_t sysfs_get_int(const char* path, int32_t def);
int32_t sysfs_get_string(const char* path, char *str);
int32_t sysfs_get_string_ex(const char* path, char *str, int32_t size, bool needOriginalData);
int32_t sysfs_set_string(const char *path, const char *val);

#if PLATFORM_SDK_VERSION < 28
/*
 * cloneNativeHandle
 *
 * creates a native_handle_t and initializes it from another native_handle_t.
 * Must be destroyed with native_handle_delete().
 *
 */
native_handle_t* native_handle_clone(const native_handle_t* handle);
#endif

#endif/*MISC_H*/
