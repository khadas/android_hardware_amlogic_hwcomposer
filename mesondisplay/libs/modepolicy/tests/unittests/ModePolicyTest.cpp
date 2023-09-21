/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#define LOG_TAG "ModePolicyTest"

#include <utils/Errors.h>
#include <cutils/properties.h>
#include <utils/Timers.h>

#include <gtest/gtest.h>
#include <log/log.h>

#include "VtsModePolicy.h"
#include "mode_policy.h"
#include "mode_ubootenv.h"

#undef DEBUG

using ::android::OK;

class ModePolicyTest : public testing::Test {
public:

    virtual void SetUp() {
        mModePolicy = std::make_unique<VtsModePolicy>();
        mModePolicy->setUp();
    }

    virtual void TearDown() {
        mModePolicy->tearDown();
        mModePolicy.reset();
    }
protected:

    std::unique_ptr<VtsModePolicy> mModePolicy;
};

// sdr 4k30hz
TEST_F(ModePolicyTest, 4K30HZ_SDR)
{
    // case 1: output best policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/4K30HZ_SDR.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
#ifdef DEBUG
    mModePolicy->dump();
    ALOGD("line:%d outmode:%s outcolor:%s", __LINE__, out.displaymode, out.deepcolor);
#endif
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
    mModePolicy->setDeepColorMode(true);
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.1: edid parsing error
    mModePolicy->setEdidParsing("error");
    mModePolicy->setDeepColorMode(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, MESON_DEFAULT_HDMI_MODE));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.2 sink type is none, hdmi not connected
    mModePolicy->setSinkType(MESON_SINK_TYPE_NONE);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCvbsMode()));

    // case2: Resolution policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_RESOLUTION);
    mModePolicy->setEdidParsing("ok");
    mModePolicy->setSinkType(MESON_SINK_TYPE_SINK);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p30hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case3: switch state
    mModePolicy->setModeState(MESON_SCENE_STATE_SWITCH);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
}

// sdr 4k60hz
TEST_F(ModePolicyTest, 4K60HZ_SDR)
{
    // case 1: output best policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/4K60HZ_SDR.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
    mModePolicy->setDeepColorMode(true);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT_4K));

    // case 1.1: edid parsing error
    mModePolicy->setEdidParsing("error");
    mModePolicy->setDeepColorMode(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, MESON_DEFAULT_HDMI_MODE));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.2 sink type is none, hdmi not connected
    mModePolicy->setSinkType(MESON_SINK_TYPE_NONE);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCvbsMode()));

    // case2: Resolution policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_RESOLUTION);
    mModePolicy->setEdidParsing("ok");
    mModePolicy->setSinkType(MESON_SINK_TYPE_SINK);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case3: switch state
    mModePolicy->setModeState(MESON_SCENE_STATE_SWITCH);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
}


// sdr 1080p60hz
TEST_F(ModePolicyTest, 1080P60HZ_SDR)
{
    // case 1: output best policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/1080P60HZ_SDR.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
    mModePolicy->setDeepColorMode(true);
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.1: edid parsing error
    mModePolicy->setEdidParsing("error");
    mModePolicy->setDeepColorMode(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, MESON_DEFAULT_HDMI_MODE));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.2 sink type is none, hdmi not connected
    mModePolicy->setSinkType(MESON_SINK_TYPE_NONE);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCvbsMode()));

    // case2: Resolution policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_RESOLUTION);
    mModePolicy->setEdidParsing("ok");
    mModePolicy->setSinkType(MESON_SINK_TYPE_SINK);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case3: switch state
    mModePolicy->setModeState(MESON_SCENE_STATE_SWITCH);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
}

// sdr 720p60hz
TEST_F(ModePolicyTest, 720P60HZ_SDR)
{
    // case 1: output best policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/720P60HZ_SDR.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "720p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
    mModePolicy->setDeepColorMode(true);
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "720p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.1: edid parsing error
    mModePolicy->setEdidParsing("error");
    mModePolicy->setDeepColorMode(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, MESON_DEFAULT_HDMI_MODE));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case 1.2 sink type is none, hdmi not connected
    mModePolicy->setSinkType(MESON_SINK_TYPE_NONE);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCvbsMode()));

    // case2: Resolution policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_RESOLUTION);
    mModePolicy->setEdidParsing("ok");
    mModePolicy->setSinkType(MESON_SINK_TYPE_SINK);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "720p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));

    // case3: switch state
    mModePolicy->setModeState(MESON_SCENE_STATE_SWITCH);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, MESON_DEFAULT_COLOR_FORMAT));
}

// HDR cases 4k60hz
TEST_F(ModePolicyTest, 4K60HZ_HDR)
{
    // case 1-4 init or hotplug
    // case 1: output best policy under init state
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/4K60HZ_HDR.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR420_10BIT));
    mModePolicy->setSupport4k(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR422_12BIT));

    // case 2:User set color format
    mModePolicy->setBestColorSpace(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, mModePolicy->getUenvColor()));

    // case 3: none policy
    mModePolicy->setModePolicy(MESON_POLICY_INVALID);
    mModePolicy->setBestColorSpace(true);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR422_12BIT));

    // case 4: none policy, bestcolorspace false
    mModePolicy->setBestColorSpace(false);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, mModePolicy->getUenvColor()));

    // case 5: switch, bestcolorspace false
    mModePolicy->setModeState(MESON_SCENE_STATE_SWITCH);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, mModePolicy->getUenvColor()));
    // case 6: switch, bestcolorspace true
    mModePolicy->setBestColorSpace(true);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, mModePolicy->getCurMode()));
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR422_12BIT));

#if 0
    // case 3.1 swith but current not find
    mModePolicy->setCurMode("1366p60hz");
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR420_10BIT));
#endif
}

// DV cases max support 4k60hz
TEST_F(ModePolicyTest, 4K60HZ_DV)
{
    // case 1: best policy
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/4K60HZ_DV.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();
    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_EQ(out.dv_type, DOLBY_VISION_LL_YUV);
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR422_12BIT));

    // case 2: max support to 1080p50hz
    mModePolicy->setDvMaxMode("1080p50hz");
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p50hz"));

    // case 3: color deep LL RGB
    mModePolicy->setDvMaxMode("2160p60hz");
    mModePolicy->setDvColor("LL_RGB_444_12BIT");
    mModePolicy->setUenvDvType("3");
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
//#ifdef DEBUG
    mModePolicy->dump();
    ALOGD("line:%d outmode:%s outcolor:%s", __LINE__, out.displaymode, out.deepcolor);
//#endif
    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
    EXPECT_EQ(out.dv_type, DOLBY_VISION_LL_RGB);
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR444_12BIT));

    // case 4: color deep STD
    mModePolicy->setDvMaxMode("2160p60hz");
    mModePolicy->setDvColor("DV_RGB_444_8BIT");
    mModePolicy->setUenvDvType("1");
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
//#ifdef DEBUG
    mModePolicy->dump();
    ALOGD("line:%d outmode:%s outcolor:%s", __LINE__, out.displaymode, out.deepcolor);
//#endif
    EXPECT_TRUE(!strcmp(out.displaymode, "2160p60hz"));
    EXPECT_EQ(out.dv_type, DOLBY_VISION_STD_ENABLE);
    EXPECT_TRUE(!strcmp(out.deepcolor, COLOR_YCBCR444_8BIT));

    // case5: resolution policy
    mModePolicy->setDvMaxMode("720p60hz");
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->sceneProcess();
    mModePolicy->getModePolicyOut(out);
    EXPECT_TRUE(!strcmp(out.displaymode, "720p60hz"));

}

// DV cases 4k60hz under best policy when init
TEST_F(ModePolicyTest, 4K30HZ_DV)
{
    mModePolicy->setModePolicy(MESON_POLICY_BEST);
    mModePolicy->setEdidInfoPath("/data/4K30HZ_DV.json");
    mModePolicy->setModeState(MESON_SCENE_STATE_INIT);
    mModePolicy->sceneProcess();

    struct meson_policy_out out;
    mModePolicy->getModePolicyOut(out);

    EXPECT_TRUE(!strcmp(out.displaymode, "1080p60hz"));
}
