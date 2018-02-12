/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <HwDisplayManager.h>
#include <MesonLog.h>
#include <HwDisplayConnector.h>
#include "HwConnectorFactory.h"
#include "OsdPlane.h"
#include "VideoPlane.h"
#include <utils/Tokenizer.h>
#include <DisplayMode.h>
#define DEVICE_STR_MBOX                 "MBOX"
#define DEVICE_STR_TV                   "TV"

#if PLATFORM_SDK_VERSION >= 26 //8.0
#define pConfigPath "/vendor/etc/mesondisplay.cfg"
#else
#define pConfigPath "/system/etc/mesondisplay.cfg"
#endif


ANDROID_SINGLETON_STATIC_INSTANCE(HwDisplayManager)

HwDisplayManager::HwDisplayManager() {
    crtc_ids = connector_ids = NULL;
    count_crtcs = count_connectors = count_pipes = 0;
    getResources();

#if MESON_HW_DISPLAY_VSYNC_SOFTWARE
        mVsync = std::make_shared<HwDisplayVsync>(true, this);
#else
        mVsync = std::make_shared<HwDisplayVsync>(false, this);
#endif
}

HwDisplayManager::~HwDisplayManager() {
    freeResources();
}

int32_t HwDisplayManager::getResources() {
    int32_t ret = getDrmResources();
    if (0 != ret) {
        MESON_LOGE("%s getDrmResources failed, (%d)", __func__, strerror(ret));
        return ret;
    }

    int i = 0;
    for (; i < count_crtcs; i++) {
        getCrtc(crtc_ids[i]);
    }

    for (i = 0; i < count_connectors; i++) {
        getConnector(connector_ids[i]);
    }

    getPlanes();

    buildDisplayPipes();
    return 0;
}

int32_t HwDisplayManager::freeResources() {
    if (crtc_ids) {
        delete crtc_ids;
        crtc_ids = NULL;
    }

    if (connector_ids) {
        delete connector_ids;
        connector_ids = NULL;
    }

    count_crtcs = count_connectors = 0;

    mCrtcs.clear();
    mConnectors.clear();
    mPlanes.clear();
    return 0;
}

int32_t HwDisplayManager::getHwDisplayIds(uint32_t * displayNum,
        hw_display_id * hwDisplayIds) {
    *displayNum = count_pipes;

    if (hwDisplayIds) {
        int i;
        for (i = 0 ;i < count_pipes; ++i) {
            hwDisplayIds[i] = pipes[i].crtc_id;
        }
    }

    return 0;
}

int32_t HwDisplayManager::getPlanes(uint32_t hwDisplayId,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    int ret = -ENXIO, i = 0;
    mMutex.lock();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            int iplane = 0;
            for (iplane = 0; iplane < pipes[i].planes_num; iplane ++) {
                planes.push_back(mPlanes.find(pipes[i].plane_ids[iplane])->second);
            }

            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::getCrtc(hw_display_id hwDisplayId,
        std::shared_ptr<HwDisplayCrtc> & crtc) {
    int ret = -ENXIO, i = 0;
    mMutex.lock();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            crtc = mCrtcs.find(pipes[i].crtc_id)->second;
            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::getConnector(hw_display_id hwDisplayId,
        std::shared_ptr<HwDisplayConnector> & connector) {
    int ret = -ENXIO, i = 0;
    mMutex.lock();
    for (i = 0; i < count_pipes; i ++) {
        if (pipes[i].crtc_id == hwDisplayId) {
            connector = mConnectors.find(pipes[i].connector_id)->second;
            ret = 0;
        }
    }
    mMutex.unlock();
    return ret;
}

int32_t HwDisplayManager::enableVBlank(bool enabled) {
    mVsync->setEnabled(enabled);
    return 0;
}

int32_t HwDisplayManager::registerObserver(hw_display_id hwDisplayId,
        HwDisplayObserver * observer) {
    mObserver.emplace(hwDisplayId, observer);
    return 0;
}

int32_t HwDisplayManager::unregisterObserver(hw_display_id hwDisplayId) {
    std::map<hw_display_id, HwDisplayObserver * >::iterator it;
    it = mObserver.find(hwDisplayId);
    if (it != mObserver.end())
        mObserver.erase(it);

    return 0;
}

int32_t HwDisplayManager::waitVBlank(nsecs_t & timestamp) {
    MESON_LOGE("TODO: empty wait hw vblank.");
    return -1;
}

void HwDisplayManager::onVsync(int64_t timestamp) {
    std::map<hw_display_id, HwDisplayObserver *>::iterator it;
    for (it = mObserver.begin(); it != mObserver.end(); ++it) {
        it->second->onVsync(timestamp);
    }
}

void HwDisplayManager::dump(String8 & dumpstr) {
    MESON_LOG_EMPTY_FUN();
}

/********************************************************************
 *   The following functions need update with drm.                  *
 *   Now is hard code for 1 crtc , 1 connector.                     *
 ********************************************************************/
#define CRTC_IDX_MIN (10)
#define CONNECTOR_IDX_MIN (20)
#define OSD_PLANE_IDX_MIN (30)
#define VIDEO_PLANE_IDX_MIN (40)

int32_t HwDisplayManager::getDrmResources() {
    /*need load from config.*/
    count_crtcs = 1;
    crtc_ids = new uint32_t [count_crtcs];
    crtc_ids[0] = CRTC_IDX_MIN + 0;

    count_connectors = 1;
    connector_ids = new uint32_t [count_connectors];
    connector_ids[0] = CONNECTOR_IDX_MIN + 0;

    return 0;
}

int32_t HwDisplayManager::getCrtc(uint32_t crtcid) {
    HwDisplayCrtc * crtc = new HwDisplayCrtc(-1, crtcid);
    mCrtcs.emplace(crtcid, std::move(crtc));

    return 0;
}

int32_t HwDisplayManager::getConnector(uint32_t connector_id) {
    /*TODO: should load config file/or get driver state,
    *and get connector type by connetor id.
    */
    drm_connector_type_t connector_type = DRM_MODE_CONNECTOR_HDMI;
    const char* WHITESPACE = " \t\r";
    char mDefaultModeSink[64];
   // SysTokenizer* tokenizer;
      Tokenizer* tokenizer;
    int status = Tokenizer::open(String8(pConfigPath), &tokenizer);
    if (status) {
        MESON_LOGE("Error %d opening display config file %s.", status, pConfigPath);
    } else {
        while (!tokenizer->isEof()) {
            MESON_LOGE("Parsing %s: %s", (tokenizer->getLocation()).string(), (tokenizer->peekRemainderOfLine()).string());

            tokenizer->skipDelimiters(WHITESPACE);
            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {

                const char *token = (tokenizer->nextToken(WHITESPACE)).string();
                if (!strcmp(token, DEVICE_STR_MBOX)) {
                    connector_type = DRM_MODE_CONNECTOR_HDMI;
                } else if (!strcmp(token, DEVICE_STR_TV)) {
                    connector_type = DRM_MODE_CONNECTOR_PANEL;
                } else {
                    MESON_LOGE("%s: Expected keyword, got '%s'.", (tokenizer->getLocation()).string(), token);
                    break;
                }
                tokenizer->skipDelimiters(WHITESPACE);
                tokenizer->nextToken(WHITESPACE);
                tokenizer->skipDelimiters(WHITESPACE);
                strcpy(mDefaultModeSink, tokenizer->nextToken(WHITESPACE));
            }

            tokenizer->nextLine();
        }
        delete tokenizer;
    }
    HwDisplayConnector* connector = HwConnectorFactory::create(
            connector_type/*, -1, connector_id*/);

    mConnectors.emplace(connector_id, std::move(connector));
    return 0;
}

int32_t HwDisplayManager::getPlanes() {
    /* scan /dev/graphics/fbx to get planes */
    int fd = -1;
    char path[64];
    int count_osd = 0, count_video = 0;
    int idx = 0, plane_idx = 0;

    do {
        snprintf(path, 64, "/dev/graphics/fb%u", idx);
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            plane_idx = OSD_PLANE_IDX_MIN + idx;
            OsdPlane * plane = new OsdPlane(fd, plane_idx);
            mPlanes.emplace(plane_idx, std::move(plane));
            count_osd ++;
        }
        idx ++;
    } while(fd >= 0);

    idx = 0;
    do {
        if (idx == 0) {
            snprintf(path, 64, "/dev/amvideo");
        } else {
            snprintf(path, 64, "/dev/amvideo%u", idx);
        }
        fd = open(path, O_RDWR, 0);
        if (fd >= 0) {
            plane_idx = VIDEO_PLANE_IDX_MIN + idx;
            VideoPlane * plane = new VideoPlane(fd, plane_idx);
            mPlanes.emplace(plane_idx, std::move(plane));
            count_video ++;
        }
        idx ++;
    } while(fd >= 0);

    MESON_LOGD("get osd planes (%d), video planes (%d)", count_osd, count_video);

    return 0;
}

int32_t HwDisplayManager::buildDisplayPipes() {
    count_pipes = 1;

    pipes[0].crtc_id = crtc_ids[0];
    pipes[0].connector_id = connector_ids[0];
    pipes[0].planes_num = mPlanes.size();
    pipes[0].plane_ids = new uint32_t [pipes[0].planes_num];

    int i = 0;
    std::map<uint32_t, std::shared_ptr<HwDisplayPlane>>::iterator it = mPlanes.begin();
    for (; it!=mPlanes.end(); ++it) {
        pipes[0].plane_ids[i] = it->first;
        i++;
    }

    return 0;
}

