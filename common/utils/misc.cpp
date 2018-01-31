/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <misc.h>

#include <cutils/properties.h>

bool sys_get_bool_prop(const char* prop, bool defVal) {
    return property_get_bool(prop, defVal);
}

int32_t sys_get_string_prop(const char* prop, char* val) {
    return property_get(prop, val, NULL);
}

