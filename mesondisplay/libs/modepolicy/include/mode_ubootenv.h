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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * save user set hdmi output resolution
 * default value is "none"
 */
#define UBOOTENV_HDMIMODE               "ubootenv.var.hdmimode"

/*
 * save user prefer color format
 * default value is "none"
 */
#define UBOOTENV_USER_COLORATTRIBUTE   "ubootenv.var.user_colorattribute"

/*
 * save hdmi output dv mode
 * uboot output dv signal base this value
 * 0:dv disable
 * 1:sink-led
 * 2:source-led
 */
#define UBOOTENV_DOLBYSTATUS            "ubootenv.var.dolby_status"

/*
 * save user set dv mode
 * hdmi output dv signal base this value
 * 0:disable dv
 * 1:sink-led
 * 2:source-led
 */
#define UBOOTENV_USER_DV_TYPE                "ubootenv.var.user_prefer_dv_type"

/*
 * save user prefer dv enable or disable
 * 0:disable
 * 1:enable
 */
#define UBOOTENV_DV_ENABLE              "ubootenv.var.dv_enable"

/*
 * save user prefer hdr policy
 * 0:always hdr(output signal base TV)
 * 1:adaptive hdr(output signal base tv and play content)
 * 2:force hdr(output signal base hdr_force_mode)
 */
#define UBOOTENV_HDR_POLICY             "ubootenv.var.hdr_policy"

/*
 * save user force hdr mode
 * 0:invalid hdr mode
 * 1:force sdr
 * 2:force dv
 * 3:force hdr10
 * 4:force hdr10+,need to do
 * 5:force hlg
 */
#define UBOOTENV_HDR_FORCE_MODE         "ubootenv.var.hdr_force_mode"

/*
 * save user preferred hdr type
 * bit0: invalid
 * bit1: 1 → disable dv, 0 → enable dv
 * bit2: 1 → disable hdr10/hdr10+, 0 → enable hdr10/hdr10+
 * bit3: 1 → disable hlg, 0 → enable hlg
 */
#define UBOOTENV_USER_PREFERRED_HDR_TYPE         "ubootenv.var.user_hdr_type"

/*
 * save user preferred hdr policy
 * "0":System-preferred conversion
 * "1":Match content Dynamic range
 * "2":Force conversion
 */
#define UBOOTENV_HDR_PREFERRED_POLICY         "ubootenv.var.hdr_preferred_policy"

int meson_mode_init_ubootenv();

const char *meson_mode_get_ubootenv(const char * key);

int32_t meson_mode_set_ubootenv(const char* name, const char* value);

void meson_mode_ubootenv_dump(int fd);


#ifdef __cplusplus
}
#endif

#endif  /* MESON_MODE_POLICY_UBOOT_ENV_H */
