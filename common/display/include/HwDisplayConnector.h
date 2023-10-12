/* Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef HW_DISPLAY_CONNECTOR_H
#define HW_DISPLAY_CONNECTOR_H

#include <string>
#include <vector>

#include <utils/String8.h>
#include <utils/Errors.h>
#include <sys/types.h>

#include <DrmTypes.h>
#include <BasicTypes.h>

#define DEFAULT_DISPLAY_DPI 160
#define LOW_LATENCY         "/sys/module/aml_media/parameters/use_low_latency"
#define LOW_LATENCY_DISABLE           "0"
#define LOW_LATENCY_ENABLE            "1"

class HwDisplayCrtc;

/* IComposerClient@2.4::ContentType */
enum {
    CONTENT_TYPE_NONE = 0,
    CONTENT_TYPE_GRAPHICS = 1,
    CONTENT_TYPE_PHOTO = 2,
    CONTENT_TYPE_CINEMA = 3,
    CONTENT_TYPE_GAME = 4,
};

enum {
    REFRESH_24kHZ = 24,
    REFRESH_25kHZ = 25,
    REFRESH_30kHZ = 30,
    REFRESH_50kHZ = 50,
    REFRESH_60kHZ = 60,
    REFRESH_120kHZ = 120,
    REFRESH_240kHZ = 240
};

enum {
    MODE_FRACTION = 0,
    MODE_NON_FRACTION,
    MODE_ALL
};

class HwDisplayConnector {
public:
    HwDisplayConnector() { }
    virtual ~HwDisplayConnector() { }

    virtual uint32_t getId() = 0;
    virtual const char * getName() = 0;
    virtual drm_connector_type_t getType() = 0;

    virtual bool isSecure() = 0;
    virtual bool isConnected() = 0;

    virtual int32_t setCrtcId(uint32_t crtcid) = 0;
    virtual uint32_t getCrtcId() = 0;

    virtual int32_t update() = 0;

    virtual int32_t setMode(drm_mode_info_t & mode __unused)  = 0;
    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes) = 0;

    virtual void updateHdrCaps() = 0;
    virtual void getHdrCapabilities(drm_hdr_capabilities * caps) = 0;
    virtual std::string getCurrentHdrType() { return "SDR"; }
    virtual bool checkFracMode(const drm_mode_info_t & mode __unused) { return true; };
    virtual bool getDvCap(std::string & dv_cap) {UNUSED(dv_cap); return false; };
    virtual bool isDvEnable() { return false; };

    virtual int32_t getIdentificationData(std::vector<uint8_t>& idOut) = 0;

    virtual void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut) = 0;
    virtual int32_t setContentType(uint32_t contentType) = 0;

    virtual int32_t setAutoLowLatencyMode(bool on) = 0;

    virtual void dump(String8 & dumpstr) = 0;

    virtual bool isTvSupportALLM() = 0;
    virtual bool supportSourceLed() { return false; };
    virtual bool supportSinkLed() { return false; };

    virtual int32_t getHdrPriority(uint32_t & hdrPriority __unused) {return 0; }
    virtual int32_t setHdrPriority(uint32_t value __unused) {return 0; }
    virtual int32_t setColorAttribute(const std::string & hdmiColorAttr __unused) {return 0; };
    virtual int32_t applyConnectorProps(drmModeAtomicReqPtr &req __unused) {return 0; }
    virtual int32_t getColorDepth(uint32_t & colorDepth __unused) {return 0; };
    virtual ENUM_HDMI_COLOR_SPACE getColorSpace() {return ENUM_HDMI_COLOR_SPACE(0); };

    virtual bool isReady() {return true; }

};

#endif/*HW_DISPLAY_CONNECTOR_H*/

