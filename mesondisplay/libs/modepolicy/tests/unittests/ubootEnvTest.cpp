/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_TAG "ubootenvTest"

#include <utils/Errors.h>
#include <cutils/properties.h>
#include <utils/Timers.h>

#include <gtest/gtest.h>

#include <log/log.h>

#include <mode_ubootenv.h>

using ::android::OK;

#define DEFAULT_DEEP_COLOR_ATTR         "444,8bit"
#define DEFAULT_420_DEEP_COLOR_ATTR     "420,8bit"

#define MODE_1080P50HZ   "1080p50hz"
#define MODE_1080P60HZ   "1080p60hz"
#define MODE_720p60HZ    "720p60hz"
#define MODE_576_CVBS    "576cvbs"

#define UBOOTENV_HDMIMODE               "ubootenv.var.hdmimode"
#define UBOOTENV_CVBSMODE               "ubootenv.var.cvbsmode"
#define UBOOTENV_OUTPUTMODE             "ubootenv.var.outputmode"
#define UBOOTENV_ISBESTMODE             "ubootenv.var.is.bestmode"
#define UBOOTENV_BESTDOLBYVISION        "ubootenv.var.bestdolbyvision"
#define UBOOTENV_EDIDCRCVALUE           "ubootenv.var.hdmichecksum"
#define UBOOTENV_HDMICOLORSPACE         "ubootenv.var.hdmi_colorspace"
#define UBOOTENV_HDMICOLORDEPTH         "ubootenv.var.hdmi_colordepth"
#define UBOOTENV_DOLBYSTATUS            "ubootenv.var.dolby_status"

class ubootenvTest : public testing::Test {
public:

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
protected:
};

TEST_F(ubootenvTest, uboot_env_init)
{
    EXPECT_EQ(OK, meson_mode_init_ubootenv());

}

TEST_F(ubootenvTest, uboot_env_set_hdmi_colordepth)
{
    EXPECT_EQ(OK, meson_mode_set_ubootenv(UBOOTENV_HDMICOLORDEPTH,"9"));

}

TEST_F(ubootenvTest, uboot_env_set_hdmimode)
{
    EXPECT_EQ(OK, meson_mode_set_ubootenv(UBOOTENV_HDMIMODE,MODE_720p60HZ));

}

TEST_F(ubootenvTest, uboot_env_set_cvbsmode)
{
    EXPECT_EQ(OK, meson_mode_set_ubootenv(UBOOTENV_CVBSMODE,MODE_576_CVBS));

}


TEST_F(ubootenvTest, uboot_env_set_outputmode)
{
    EXPECT_EQ(OK, meson_mode_set_ubootenv(UBOOTENV_OUTPUTMODE,MODE_1080P50HZ));

}

TEST_F(ubootenvTest, uboot_env_set_isbestmode)
{
    EXPECT_EQ(OK, meson_mode_set_ubootenv(UBOOTENV_ISBESTMODE,"true"));

}

