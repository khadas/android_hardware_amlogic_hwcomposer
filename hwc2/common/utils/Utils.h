/*
// Copyright(c) 2016 Amlogic Corporation
*/

#ifndef UTILS_H_
#define UTILS_H_

#include <gralloc_priv.h>
#if WITH_LIBPLAYER_MODULE
#include <Amavutils.h>
#endif

#define SYSFS_AMVIDEO_CURIDX            "/sys/module/amvideo/parameters/cur_dev_idx"
#define SYSFS_DISPLAY_MODE              "/sys/class/display/mode"
#define SYSFS_FB0_FREE_SCALE            "/sys/class/graphics/fb0/free_scale"
#define SYSFS_FB1_FREE_SCALE            "/sys/class/graphics/fb0/free_scale"
#define SYSFS_VIDEO_AXIS                "/sys/class/video/axis"
#define SYSFS_VIDEOBUFUSED              "/sys/class/amstream/videobufused"
#define SYSFS_WINDOW_AXIS               "/sys/class/graphics/fb0/window_axis"

namespace android {
namespace amlogic {

class Utils {
public:
    Utils();
    ~Utils();
    static bool checkBoolProp(const char* prop);
    static int32_t checkIntProp(const char* prop);
    static int32_t checkAndDupFence(int32_t fence);
#if WITH_LIBPLAYER_MODULE
    static bool checkSysfsStatus(const char* sysfstr, char* lastr, int32_t size);
#endif
    static bool checkOutputMode(char* curmode, int32_t* rate);

    static bool checkVinfo(framebuffer_info_t *fbinfo);

    static const char* getUeventEnvelope();
    static const char* getHotplugString();

};

} // namespace amlogic
} // namespace android
#endif /* UTILS_H_ */
