/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "SimpleStrategy.h"
#include "Composition.h"
#include <MesonLog.h>

SimpleStrategy::SimpleStrategy() {
}

SimpleStrategy::~SimpleStrategy() {
}

void SimpleStrategy::setUp(
    std::vector<std::shared_ptr<DrmFramebuffer>> & layers,
    std::vector<std::shared_ptr<IComposeDevice>> & composers,
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {

    classifyLayers(layers);
    classifyComposers(composers);
    classifyPlanes(planes);

    mHaveClientLayer = false;
    mUiLayers.clear();
    mUiComposer.reset();
}

void SimpleStrategy::classifyLayers(
    std::vector<std::shared_ptr<DrmFramebuffer>> & layers) {
    mLayers.clear();
    mLayers = layers;
}

void SimpleStrategy::classifyComposers(
    std::vector<std::shared_ptr<IComposeDevice>> & composers) {
    mDummyComposer.reset();
    mClientComposer.reset();
    mComposers.clear();

    std::vector<std::shared_ptr<IComposeDevice>>::iterator it;
    for (it = composers.begin() ; it != composers.end(); ++it) {
        std::shared_ptr<IComposeDevice> composer = *it;

        if (composer->isCompositionSupport(MESON_COMPOSITION_DUMMY) == true) {
            if (mDummyComposer == NULL) {
                mDummyComposer = composer;
            } else {
                MESON_LOGE("Meet more than one dummy composer.");
            }
            continue;
        }

        if (composer->isCompositionSupport(MESON_COMPOSITION_CLIENT) == true) {
            if (mClientComposer == NULL) {
                mClientComposer = composer;
            } else {
                MESON_LOGE("Meet more than one client composer.");
            }
            continue;
        }

        mComposers.push_back(composer);
    }

    if (mDummyComposer == NULL) {
        MESON_LOGE("ERROR:No dummy composer find.!!");
    }
    if (mClientComposer == NULL) {
        MESON_LOGE("ERROR:No client composer find.!!");
    }
}

void SimpleStrategy::classifyPlanes(
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes) {
    std::shared_ptr<HwDisplayPlane> plane;
    mVideoPlanes.clear();
    mOsdPlanes.clear();
    mCursorPlanes.clear();

    std::vector<std::shared_ptr<HwDisplayPlane>>::iterator it;
    for (it = planes.begin() ; it != planes.end(); ++it) {
        plane = *it;
        if (plane->getPlaneType()  & OSD_PLANE) {
            mOsdPlanes.push_back(plane);
        } else if (plane->getPlaneType()  & VIDEO_PLANE) {
            mVideoPlanes.push_back(plane);
        } else if (plane->getPlaneType() == CURSOR_PLANE) {
            mCursorPlanes.push_back(plane);
        }
    }
}

void SimpleStrategy::setUiComposer() {
    /*
    * Only use one composer, DONT use server composers together.
    * 1) If client layer exists, use client composer.
    * 2) If no other composer exists, use client composer.
    * 3) choose composer can supported all the drmFramebuffer in uilayer.
    */
    if (mHaveClientLayer || mComposers.size() == 0) {
        mUiComposer = mClientComposer;
        return;
    }

    std::list<std::shared_ptr<IComposeDevice>>::iterator it;
    for (it = mComposers.begin(); it != mComposers.end(); ++it) {
        mUiComposer = *it;
        std::vector<std::shared_ptr<DrmFramebuffer>>::iterator layer;
        for (layer = mUiLayers.begin(); layer != mUiLayers.end(); ++layer) {
            if (mUiComposer->isFbSupport(*layer) == false) {
                mUiComposer.reset();
                break;
            }
        }
    }

    /*Default will be client composer.*/
    if (mUiComposer == NULL) {
        mUiComposer = mClientComposer;
    }
}

void SimpleStrategy::preProcessLayers() {
    uint32_t videoFbNum =0, cursorFbNum = 0;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;

    for (it = mLayers.begin(); it != mLayers.end(); ++it) {
        std::shared_ptr<DrmFramebuffer> layer = *it;
        switch (layer->mCompositionType) {
            case MESON_COMPOSITION_NONE:
            {
                if (layer->mFbType == DRM_FB_VIDEO_OVERLAY ||
                    layer->mFbType == DRM_FB_VIDEO_OMX ||
                    layer->mFbType == DRM_FB_VIDEO_SIDEBAND) {
                    videoFbNum++;
                    if (videoFbNum <= mVideoPlanes.size()) {
                        layer->mCompositionType =
                            MESON_COMPOSITION_PLANE_VIDEO_SIDEBAND;
                    } else {
                        layer->mCompositionType =
                            MESON_COMPOSITION_DUMMY;
                    }
                } else if (layer->mFbType == DRM_FB_CURSOR) {
                    cursorFbNum ++;
                    if (videoFbNum <= mCursorPlanes.size()) {
                        layer->mCompositionType =
                            MESON_COMPOSITION_PLANE_CURSOR;
                    }
                }
            }
            break;
        case MESON_COMPOSITION_CLIENT:
            mHaveClientLayer = true;
            break;
        }

        if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
            mUiLayers.push_back(layer);
        }
    }
}

bool SimpleStrategy::isPlaneSupported(
    std::shared_ptr<DrmFramebuffer> & fb) {
    if (fb->mTransform != 0) {
        return false;
    }
    return true;
}

int32_t SimpleStrategy::decideComposition() {
    if (mLayers.empty()) {
        MESON_LOGV("preProcessLayers return with no layers.");
        return 0;
    }

    int composedLayers = 0;
    std::shared_ptr<DrmFramebuffer> layer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator firstUiLayer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator lastUiLayer;

    preProcessLayers();
    setUiComposer();

    firstUiLayer = lastUiLayer = mUiLayers.end();

    /*
    * DRM_FB_RENDER, cannot consumed by plane,
    * should consumed by composer.
    */
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mUiLayers.begin(); it != mUiLayers.end(); ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_NONE &&
            layer->mFbType == DRM_FB_RENDER) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
        } else if (isPlaneSupported(layer) == false) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
        }

        if (isComposerComposition(layer->mCompositionType)) {
            if (firstUiLayer == mUiLayers.end()) {
                firstUiLayer = it;
            }
            lastUiLayer = it;
            composedLayers++;
        }
    }

    /*composer need sequent layers excpet special layers.*/
    for (it = firstUiLayer; it != lastUiLayer; ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
            composedLayers++;
        }
    }

    /*If layer num > plane num, need compose more.*/
    int numUiLayers = mUiLayers.size(), numOsdPlanes = mOsdPlanes.size();
    int needComposedLayers = 0;
    /* When layers > planes num, we need use composer to conume more layers.*/
    if ((numUiLayers - composedLayers) > (numOsdPlanes - (composedLayers ? 1 : 0))) {
        needComposedLayers = (numUiLayers - composedLayers) - (numOsdPlanes - 1);
    }

    if (needComposedLayers > 0) {
        if (lastUiLayer != mUiLayers.end()) {
            for (it = ++lastUiLayer; needComposedLayers > 0 && it != mUiLayers.end(); it++) {
                layer = *it;
                if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
                    layer->mCompositionType = mUiComposer->getCompostionType(layer);
                    needComposedLayers--;
                }
            }
        }

        for (it = firstUiLayer; needComposedLayers > 0 && it != mUiLayers.begin();) {
            it--; layer = *it;
            if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
                layer->mCompositionType = mUiComposer->getCompostionType(layer);
                needComposedLayers--;
            }
        }
    }

    /*To set plane*/
    for (it = mUiLayers.begin(); it != mUiLayers.end(); ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
            if (layer->mFbType & DRM_FB_COLOR) {
                layer->mCompositionType = MESON_COMPOSITION_PLANE_OSD_COLOR;
            } else if (layer->mFbType & DRM_FB_SCANOUT) {
                layer->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
            }
        }
    }

    return 0;
}

int32_t SimpleStrategy::commit() {
    std::shared_ptr<DrmFramebuffer> layer;
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator videoPlane =
        mVideoPlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator osdPlane =
        mOsdPlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator cursorPlane =
        mCursorPlanes.begin();

    if (!mLayers.empty()) {
        bool setComposerPlane = false;
        std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
        for (it = mLayers.begin(); it != mLayers.end(); ++it) {
            layer = *it;
            switch (layer->mCompositionType) {
                case MESON_COMPOSITION_DUMMY:
                    mDummyComposer->addInput(layer);
                    break;
                case MESON_COMPOSITION_CLIENT:
                case MESON_COMPOSITION_GE2D:
                    mUiComposer->addInput(layer);
                    if (!setComposerPlane) {
                        std::shared_ptr<DrmFramebuffer> fb = mUiComposer->getOutput();
                        if (fb.get()) {
                            fb->mZorder = layer->mZorder;
                            (*osdPlane++)->setPlane(fb);
                            setComposerPlane = true;
                        }
                    }
                    break;
                case MESON_COMPOSITION_PLANE_VIDEO:
                case MESON_COMPOSITION_PLANE_VIDEO_SIDEBAND:
                    (*videoPlane++)->setPlane(layer);
                    break;
                case MESON_COMPOSITION_PLANE_OSD:
                case MESON_COMPOSITION_PLANE_OSD_COLOR:
                    (*osdPlane++)->setPlane(layer);
                    break;
                case MESON_COMPOSITION_PLANE_CURSOR:
                    (*cursorPlane++)->setPlane(layer);
                    break;
            }
        }

        mDummyComposer->start();
        mUiComposer->start();
    }

    /*Set blank framebuffer to */
    while (videoPlane != mVideoPlanes.end()) {
        (*videoPlane++)->blank();
    }
    while (osdPlane != mOsdPlanes.end()) {
        (*osdPlane++)->blank();
    }
    while (cursorPlane != mCursorPlanes.end()) {
        (*cursorPlane++)->blank();
    }
    return 0;
}

void SimpleStrategy::dump(String8 & dumpstr) {

}

