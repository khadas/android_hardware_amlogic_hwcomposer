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
    static int32_t checkAndDupFd(int32_t fd);
    static inline void closeFd(int32_t fd) {
        if (fd > -1) close(fd);
    }
#if WITH_LIBPLAYER_MODULE
    static bool checkSysfsStatus(const char* sysfstr, char* lastr, int32_t size);
#endif
    static bool checkOutputMode(char* curmode, int32_t* rate);

    static bool checkVinfo(framebuffer_info_t *fbinfo);

    static const char* getUeventEnvelope();
    static const char* getHotplugInString();
    static const char* getHotplugOutString();

    template <typename T, typename S>
    static inline bool compareRect(T a, S b) {
        if ((int32_t)a.left == (int32_t)b.left
                && (int32_t)a.top == (int32_t)b.top
                && (int32_t)a.right == (int32_t)b.right
                && (int32_t)a.bottom == (int32_t)b.bottom) {
            return true;
        }
        return false;
    }
    template <typename T, typename S>
    static inline bool compareSize(T a, S b) {
        if ((int32_t)(a.right-a.left) == (int32_t)(b.right-b.left)
                && (int32_t)(a.bottom-a.top) == (int32_t)(b.bottom-b.top)) {
            return true;
        }
        return false;
    }
    template <typename T>
    static inline T min(T a, T b) {
        return a<b ? a : b;
    }
    template <typename T>
    static inline T max(T a, T b) {
        return a>b ? a : b;
    }
    template <typename T>
    static inline void swap(T& a, T& b) {
        T t = a;
        a = b;
        b = t;
    }

};

} // namespace amlogic
} // namespace android
#endif /* UTILS_H_ */
