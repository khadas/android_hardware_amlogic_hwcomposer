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

class HwDisplayCrtc;

/* IComposerClient@2.4::ContentType */
enum {
    CONTENT_TYPE_NONE = 0,
    CONTENT_TYPE_GRAPHICS = 1,
    CONTENT_TYPE_PHOTO = 2,
    CONTENT_TYPE_CINEMA = 3,
    CONTENT_TYPE_GAME = 4,
};

class HwDisplayConnector {
public:
    HwDisplayConnector() { }
    virtual ~HwDisplayConnector() { }

    virtual int32_t setCrtc(HwDisplayCrtc * crtc) = 0;
    virtual HwDisplayCrtc * getCrtc() = 0;

    virtual int32_t loadProperities() = 0;
    virtual int32_t update() = 0;

    virtual int32_t getModes(std::map<uint32_t, drm_mode_info_t> & modes) = 0;

    virtual const char * getName() = 0;
    virtual drm_connector_type_t getType() = 0;
    virtual bool isRemovable() = 0;
    virtual bool isSecure() = 0;
    virtual bool isConnected() = 0;

    virtual void getHdrCapabilities(drm_hdr_capabilities * caps) = 0;
    virtual int32_t getIdentificationData(std::vector<uint8_t>& idOut) = 0;
    virtual void getSupportedContentTypes(std::vector<uint32_t> & supportedContentTypesOut) = 0;

    virtual int32_t setAutoLowLatencyMode(bool on) = 0;
    virtual int32_t setContentType(uint32_t contentType) = 0;

    virtual void dump(String8 & dumpstr) = 0;

    virtual int32_t setMode(drm_mode_info_t & mode __unused)  = 0;
    virtual uint32_t getId() = 0;
    virtual std::string getCurrentHdrType() { return "SDR"; }
};

#endif/*HW_DISPLAY_CONNECTOR_H*/

