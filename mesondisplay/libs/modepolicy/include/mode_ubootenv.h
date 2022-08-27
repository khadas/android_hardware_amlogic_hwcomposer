/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef MESON_MODE_POLICY_UBOOT_ENV_H
#define MESON_MODE_POLICY_UBOOT_ENV_H

/* define struct */


const char *meson_mode_get_ubootenv(const char * key);

int32_t meson_mode_set_ubootenv(const char* name, const char* value);

void meson_mode_ubootenv_dump(int fd);

#endif  /* MESON_MODE_POLICY_UBOOT_ENV_H */
