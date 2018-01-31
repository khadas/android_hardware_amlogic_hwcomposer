/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef HW_DISPLAY_MANAGER_H
#define HW_DISPLAY_MANAGER_H

#include <time.h>

#include <BasicTypes.h>
#include <HwDisplayCrtc.h>
#include <HwDisplayPlane.h>
#include <HwDisplayConnector.h>
#include <HwDisplayVsync.h>
#include <HwDisplayDefs.h>

using namespace::android;

class HwDisplayObserver {
public:
    HwDisplayObserver() {}
    virtual ~HwDisplayObserver() {}
    virtual void onVsync(int64_t timestamp) = 0;
    virtual void onHotplug(bool connected) = 0;
    virtual void onModeChanged() = 0;
};

class HwDisplayManager : public android::Singleton<HwDisplayManager>,
                         public HwVsyncObserver {
friend class HwDisplayCrtc;
friend class HwDisplayConnector;
friend class HwDisplayPlane;
friend class HwDisplayVsync;

public:
    HwDisplayManager();
    ~HwDisplayManager();

    /* get all HwDisplayIds.*/
    int32_t getHwDisplayIds(uint32_t * displayNum,
            hw_display_id * hwDisplayIds);

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    int32_t getPlanes(hw_display_id hwDisplayId,
            std::vector<std::shared_ptr<HwDisplayPlane>> & planes);

    /* get displayplanes by hw display idx, the planes may change when connector changed.*/
    int32_t getCrtc(hw_display_id hwDisplayId,
            std::shared_ptr<HwDisplayCrtc> & crtc);

    int32_t getConnector(hw_display_id hwDisplayId,
            std::shared_ptr<HwDisplayConnector> & connector);

    /*hw vsync*/
    int32_t enableVBlank(bool enabled);

    /*registe display observe*/
    int32_t registerObserver(hw_display_id hwDisplayId,
            HwDisplayObserver * observer);
    int32_t unregisterObserver(hw_display_id hwDisplayId);

    void dump(String8 & dumpstr);
protected:
    void onVsync(int64_t timestamp);
    int32_t buildDisplayPipes();

protected:
    class HwDisplayPipe {
    public:
        uint32_t crtc_id;
        uint32_t connector_id;
        uint32_t *plane_ids;
        uint32_t planes_num;
    };

    uint32_t count_pipes;
    HwDisplayPipe pipes[MESON_HW_DISPLAYS_MAX];

/*********************************************
 * drm apis.
 *********************************************/
protected:
    int32_t getResources();
    int32_t freeResources();

    int32_t getDrmResources();
    int32_t getCrtc(uint32_t crtcid);
    int32_t getConnector(uint32_t connector_id);
    int32_t getPlanes();
    int32_t waitVBlank(nsecs_t & timestamp);

protected:
    uint32_t count_crtcs;
    uint32_t * crtc_ids;
    uint32_t count_connectors;
    uint32_t * connector_ids;

    std::map<uint32_t, std::shared_ptr<HwDisplayCrtc>> mCrtcs;
    std::map<uint32_t, std::shared_ptr<HwDisplayConnector>> mConnectors;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>> mPlanes;
    std::shared_ptr<HwDisplayVsync> mVsync;
    std::map<hw_display_id, HwDisplayObserver * > mObserver;

    std::mutex mMutex;
};

#endif/*HW_DISPLAY_MANAGER_H*/