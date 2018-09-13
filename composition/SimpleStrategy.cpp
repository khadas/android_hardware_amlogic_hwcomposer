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
    std::vector<std::shared_ptr<HwDisplayPlane>> & planes,
    uint32_t flags) {
    MESON_LOG_FUN_ENTER();
    mForceClientComposer = false;
    mHideSecureLayer = false;

    mHaveClientLayer = false;
    mSetComposerPlane = false;
    mOsdPlaneAssignedManually = false;
    mVideoLayers.clear();
    mUiComposer.reset();
    mUiLayers.clear();
    mAssignedPlaneLayers.clear();

    if (flags & COMPOSE_FORCE_CLIENT)
        mForceClientComposer = true;
    if (flags & COMPOSE_HIDE_SECURE_FB)
        mHideSecureLayer = true;

    classifyLayers(layers);
    classifyComposers(composers);
    classifyPlanes(planes);

    mDumpStr.clear();
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

        if (!mForceClientComposer)
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
    MESON_LOG_FUN_ENTER();
    std::shared_ptr<HwDisplayPlane> plane;
    mCursorPlanes.clear();

    mAmVideoPlanes.clear();
    mHwcVideoPlanes.clear();

    mOsdPlanes.clear();
    mPresentOsdPlanes.clear();
    mOsdDiscretePlanes.clear();
    mOsdContinuousPlanes.clear();

    std::vector<std::shared_ptr<HwDisplayPlane>>::iterator it;
    for (it = planes.begin() ; it != planes.end(); ++it) {
        plane = *it;
        uint32_t planeType = plane->getPlaneType();
        if (planeType == OSD_PLANE) {
            mOsdPlanes.push_back(plane);
        } else if (planeType == LEGACY_VIDEO_PLANE) {
            mAmVideoPlanes.push_back(plane);
        } else if (planeType == HWC_VIDEO_PLANE) {
            mHwcVideoPlanes.push_back(plane);
        } else if (planeType == CURSOR_PLANE) {
            mCursorPlanes.push_back(plane);
        } else {
            plane->blank(BLANK_FOR_NO_CONENT);
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
    uint32_t videoFbNum = 0, cursorFbNum = 0;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;

    for (it = mLayers.begin(); it != mLayers.end(); ++it) {
        std::shared_ptr<DrmFramebuffer> layer = *it;
        switch (layer->mCompositionType) {
            case MESON_COMPOSITION_UNDETERMINED:
            {
                if (layer->mFbType == DRM_FB_VIDEO_OVERLAY
                    || layer->mFbType == DRM_FB_VIDEO_OMX_PTS
                    || layer->mFbType == DRM_FB_VIDEO_SIDEBAND
                    || layer->mFbType == DRM_FB_VIDEO_OMX_V4L) {
                    videoFbNum++;
                    if (videoFbNum <= (mAmVideoPlanes.size() + mHwcVideoPlanes.size())) {
                        if (layer->mFbType == DRM_FB_VIDEO_SIDEBAND) {
                            layer->mCompositionType =
                                MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND;
                        } else if (layer->mFbType == DRM_FB_VIDEO_OMX_V4L) {
                            layer->mCompositionType =
                                MESON_COMPOSITION_PLANE_HWCVIDEO;
                        } else {
                            layer->mCompositionType =
                                MESON_COMPOSITION_PLANE_AMVIDEO;
                        }
                        mVideoLayers.push_back(layer);
                    } else {
                        layer->mCompositionType =
                            MESON_COMPOSITION_DUMMY;
                    }
                } else if (layer->mFbType == DRM_FB_CURSOR) {
                    cursorFbNum ++;
                    if (cursorFbNum <= mCursorPlanes.size()) {
                        layer->mCompositionType =
                            MESON_COMPOSITION_PLANE_CURSOR;
                    }
                }
            }
                break;
            case MESON_COMPOSITION_CLIENT:
                mHaveClientLayer = true;
                break;
            default:
                MESON_LOGE("Not support composition type now");
        }

        if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
            if (mHideSecureLayer && layer->mSecure) {
                layer->mCompositionType = MESON_COMPOSITION_DUMMY;
            } else {
                if (mForceClientComposer) {
                    layer->mCompositionType = MESON_COMPOSITION_CLIENT;
                } else {
                    mUiLayers.push_back(layer);
                }
            }
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

void SimpleStrategy::sortLayersByZReversed(
        std::vector<std::shared_ptr<DrmFramebuffer>> &layers) {
    if (layers.size() > 1) {
        struct {
            bool operator() (std::shared_ptr<DrmFramebuffer> a,
                std::shared_ptr<DrmFramebuffer> b) {
                return a->mZorder > b->mZorder;
            }
        } zorderCompare;
        /* Sort layers by zorder. */
        std::sort(layers.begin(), layers.end(), zorderCompare);
    }
}

void SimpleStrategy::sortLayersByZ(
        std::vector<std::shared_ptr<DrmFramebuffer>> &layers) {
    if (layers.size() > 1) {
        struct {
            bool operator() (std::shared_ptr<DrmFramebuffer> a,
                std::shared_ptr<DrmFramebuffer> b) {
                return a->mZorder < b->mZorder;
            }
        } zorderCompare;
        /* Sort layers by zorder. */
        std::sort(layers.begin(), layers.end(), zorderCompare);
    }
}

int32_t SimpleStrategy::makeCurrentOsdPlanes(
        int32_t &numConflictPlanes) {
    MESON_LOG_FUN_ENTER();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator it;
    std::shared_ptr<HwDisplayPlane> osdPlane;
    int32_t i = 0, maskedConflictPlanes = 0;

    /*TODO: need more improve to adapt more cases. */
    for (it = mOsdPlanes.begin(); it != mOsdPlanes.end(); ++it, i++) {
        osdPlane = *it;
        if (osdPlane->getCapabilities() & PLANE_VIDEO_CONFLICT) {
            mOsdContinuousPlanes.push_back(osdPlane);
            maskedConflictPlanes |= (1 << i);
            numConflictPlanes++;
        } else {
            mOsdDiscretePlanes.push_back(osdPlane);
        }
    }

    /* make sure discrete osd plane will be assigned with first prior */
    mPresentOsdPlanes.assign(mOsdDiscretePlanes.begin(), mOsdDiscretePlanes.end());
    for (it = mOsdContinuousPlanes.begin(); it != mOsdContinuousPlanes.end(); ++it, i++) {
        osdPlane = *it;
        mPresentOsdPlanes.push_back(osdPlane);
    }

    return maskedConflictPlanes;
}

void SimpleStrategy::changeDeviceToClientByZ(
        uint32_t from, uint32_t to) {
    if (mUiLayers.size() > 1) {
        std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
        for (it = mUiLayers.begin(); it != mUiLayers.end(); ++it) {
             std::shared_ptr<DrmFramebuffer> layer = *it;
            if (layer->mZorder >= from && layer->mZorder <= to) {
                if (layer->mCompositionType == MESON_COMPOSITION_PLANE_OSD
                    || layer->mCompositionType == MESON_COMPOSITION_PLANE_OSD) {
                    layer->mCompositionType = mUiComposer->getCompostionType(layer);
                    MESON_LOGD("changeDeviceToClientByZ: (%d)", layer->mZorder);
                }
            }
        }
    }
}

bool SimpleStrategy::expandComposedLayers(
        std::vector<std::shared_ptr<DrmFramebuffer>> &layers,
        std::vector<std::shared_ptr<DrmFramebuffer>> &composedLayers) {
    bool found = false;
    if (composedLayers.size() > 1) {
        uint32_t composedZorder = composedLayers.front()->mZorder;
        std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;

        for (it = layers.begin(); it != layers.end(); ++it) {
             std::shared_ptr<DrmFramebuffer> layer = *it;
            if (layer->mZorder == composedZorder) {
                found = true;
                break;
            }
        }
        if (found) {
            for (it = composedLayers.begin() + 1; it != composedLayers.end(); ++it) {
                std::shared_ptr<DrmFramebuffer> layer = *it;
                layers.push_back(layer);
            }
        }
    }

    return found;
}

bool SimpleStrategy::isVideoLayer(
        std::shared_ptr<DrmFramebuffer> &layer) {

    if (layer->mCompositionType == MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND
            || layer->mCompositionType == MESON_COMPOSITION_PLANE_AMVIDEO
            || layer->mCompositionType == MESON_COMPOSITION_PLANE_HWCVIDEO) {
        return true;
    }
    return false;
}

void SimpleStrategy::makeFinalDecision(
        std::vector<std::shared_ptr<DrmFramebuffer>> &composedLayers) {
    std::vector<std::shared_ptr<DrmFramebuffer>> assignedPlaneLayers;

    assignedPlaneLayers.clear();
    if (composedLayers.size() > 1) {
        /* sort layers by zorder from high to low. and add layer of highest zorder to assignedPlaneLayers */
        sortLayersByZReversed(composedLayers);
        assignedPlaneLayers.push_back(composedLayers.front());
    }

    /*To set plane*/
    std::shared_ptr<DrmFramebuffer> layer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mUiLayers.begin(); it != mUiLayers.end(); ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
            if (layer->mFbType == DRM_FB_SCANOUT || layer->mFbType == DRM_FB_CURSOR) {
                layer->mCompositionType = MESON_COMPOSITION_PLANE_OSD;
                assignedPlaneLayers.push_back(layer);
            }
        }
    }

    int32_t numConflictPlanes = 0;
    makeCurrentOsdPlanes(numConflictPlanes);
    if (!mVideoLayers.empty() && numConflictPlanes > 1) {
        if (assignedPlaneLayers.size() == mPresentOsdPlanes.size()) {
            /* find out video layer is between osd planes or not. */
            for (it = mVideoLayers.begin(); it != mVideoLayers.end(); ++it) {
                layer = *it;
                assignedPlaneLayers.push_back(layer);
            }
            sortLayersByZ(assignedPlaneLayers);

            /* !find out layers that need to assign to special planes */
            std::shared_ptr<DrmFramebuffer> layer0;
            std::shared_ptr<DrmFramebuffer> layer1;
            std::vector<std::shared_ptr<DrmFramebuffer>> assignToSpecialOsdPlaneLayers;
            for (it = assignedPlaneLayers.begin(); it != assignedPlaneLayers.end(); ++it) {
                layer0 = *(it++);
                if (it != assignedPlaneLayers.end()) {
                    layer1 = *it;
                    if (mOsdContinuousPlanes.size() > assignToSpecialOsdPlaneLayers.size()
                            && !isVideoLayer(layer0)
                            && !isVideoLayer(layer1)) {
                        assignToSpecialOsdPlaneLayers.push_back(layer0);
                        assignToSpecialOsdPlaneLayers.push_back(layer1);
                        continue;
                    }
                } else {
                    break;
                }
            }

            if (!assignToSpecialOsdPlaneLayers.empty()) {
                expandComposedLayers(assignToSpecialOsdPlaneLayers, composedLayers);
                for (it = assignToSpecialOsdPlaneLayers.begin(); it != assignToSpecialOsdPlaneLayers.end(); ++it) {
                    layer = *it;
                    layer->mComposeToType |= MESON_COMPOSE_TO_CONTINUOUS_PLANE;
                }
                mOsdPlaneAssignedManually = true;
            } else {
                changeDeviceToClientByZ(0, mVideoLayers.front()->mZorder);
            }
        } else {
            /* if we can make sure discrete osd planes are assigned with layers firstly
             * in commit stage, this case will not a harm to us. */
        }
    }
}

int32_t SimpleStrategy::decideComposition() {
    if (mLayers.empty()) {
        MESON_LOGV("preProcessLayers return with no layers.");
        return 0;
    }

    std::vector<std::shared_ptr<DrmFramebuffer>> composedLayers;
    std::shared_ptr<DrmFramebuffer> layer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator firstUiLayer;
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator lastUiLayer;

    preProcessLayers();
    setUiComposer();

    firstUiLayer = lastUiLayer = mUiLayers.end();
    composedLayers.clear();

    /*
    * DRM_FB_RENDER, cannot consumed by plane,
    * should consumed by composer.
    */
    std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
    for (it = mUiLayers.begin(); it != mUiLayers.end(); ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED &&
            (layer->mFbType == DRM_FB_RENDER
            || layer->mFbType == DRM_FB_COLOR)) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
        } else if (isPlaneSupported(layer) == false) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
        }

        if (isComposerComposition(layer->mCompositionType)) {
            if (firstUiLayer == mUiLayers.end()) {
                firstUiLayer = it;
            }
            lastUiLayer = it;
            composedLayers.push_back(layer);
        }
    }

    /*composer need sequent layers excpet special layers.*/
    for (it = firstUiLayer; it != lastUiLayer; ++it) {
        layer = *it;
        if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
            layer->mCompositionType = mUiComposer->getCompostionType(layer);
            composedLayers.push_back(layer);
        }
    }

    /*If layer num > plane num, need compose more.*/
    int numUiLayers = mUiLayers.size();
    int numOsdPlanes = mOsdPlanes.size();
    int numComposedLayers = composedLayers.size();
    int numNeedComposeLayers = 0;
    /* When layers > planes num, we need use composer to conume more layers.*/
    if ((numUiLayers - numComposedLayers) > (numOsdPlanes - (numComposedLayers ? 1 : 0))) {
        numNeedComposeLayers = (numUiLayers - numComposedLayers) - (numOsdPlanes - 1);
    }

    if (numNeedComposeLayers > 0) {
        if (lastUiLayer != mUiLayers.end()) {
            for (it = ++lastUiLayer; numNeedComposeLayers > 0 && it != mUiLayers.end(); it++) {
                layer = *it;
                if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
                    layer->mCompositionType = mUiComposer->getCompostionType(layer);
                    numNeedComposeLayers--;
                    composedLayers.push_back(layer);
                }
            }
        }

        for (it = firstUiLayer; numNeedComposeLayers > 0 && it != mUiLayers.begin();) {
            it--; layer = *it;
            if (layer->mCompositionType == MESON_COMPOSITION_UNDETERMINED) {
                layer->mCompositionType = mUiComposer->getCompostionType(layer);
                numNeedComposeLayers--;
                composedLayers.push_back(layer);
            }
        }
    }

    makeFinalDecision(composedLayers);
    return 0;
}

int32_t SimpleStrategy::commit() {
    MESON_LOG_FUN_ENTER();

    std::shared_ptr<DrmFramebuffer> layer;
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator amVideoPlane =
        mAmVideoPlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator hwcVideoPlane =
        mHwcVideoPlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator osdPlane =
        mPresentOsdPlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator cursorPlane =
        mCursorPlanes.begin();

    /*special osd planes*/
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator osdDiscretePlane =
        mOsdDiscretePlanes.begin();
    std::list<std::shared_ptr<HwDisplayPlane>>::iterator osdContinuousPlane =
        mOsdContinuousPlanes.begin();

    if (!mLayers.empty()) {
        bool setComposerPlane = false;

        std::vector<std::shared_ptr<DrmFramebuffer>>::iterator it;
        for (it = mLayers.begin(); it != mLayers.end(); ++it) {
            layer = *it;
            int planeBlank = UNBLANK;
            std::shared_ptr<DrmFramebuffer> sourceFb = NULL;
            std::shared_ptr<HwDisplayPlane> targetPlane = NULL;

            switch (layer->mCompositionType) {
                case MESON_COMPOSITION_DUMMY:
                    mDummyComposer->addInput(layer);
                    break;
                case MESON_COMPOSITION_CLIENT:
                case MESON_COMPOSITION_GE2D:
                    mUiComposer->addInput(layer);
                    if (!setComposerPlane) {
                        sourceFb = mUiComposer->getOutput();
                        if (sourceFb.get()) {
                            sourceFb->mZorder = layer->mZorder;
                            if (mOsdPlaneAssignedManually) {
                                if (layer->mComposeToType & MESON_COMPOSE_TO_CONTINUOUS_PLANE) {
                                    targetPlane = (*osdContinuousPlane);
                                    osdContinuousPlane++;
                                } else {
                                    targetPlane = (*osdDiscretePlane);
                                    osdDiscretePlane++;
                                }
                            } else {
                                targetPlane = (*osdPlane);
                                osdPlane++;
                            }
                            setComposerPlane = true;
                        }
                    }
                    break;
                case MESON_COMPOSITION_PLANE_HWCVIDEO:
                    planeBlank = (layer->mSecure && mHideSecureLayer) ?
                        BLANK_FOR_SECURE_CONTENT : UNBLANK;
                    targetPlane = (*hwcVideoPlane);
                    hwcVideoPlane++;
                    break;
                case MESON_COMPOSITION_PLANE_AMVIDEO:
                case MESON_COMPOSITION_PLANE_AMVIDEO_SIDEBAND:
                    planeBlank = (layer->mSecure && mHideSecureLayer) ?
                        BLANK_FOR_SECURE_CONTENT : UNBLANK;
                    targetPlane = (*amVideoPlane);
                    amVideoPlane++;
                    break;
                case MESON_COMPOSITION_PLANE_OSD:
                    if (mOsdPlaneAssignedManually) {
                        if (layer->mComposeToType & MESON_COMPOSE_TO_CONTINUOUS_PLANE) {
                            targetPlane = (*osdContinuousPlane);
                            osdContinuousPlane++;
                        } else {
                            targetPlane = (*osdDiscretePlane);
                            osdDiscretePlane++;
                        }
                    } else {
                        targetPlane = (*osdPlane);
                        osdPlane++;
                    }
                    break;
                case MESON_COMPOSITION_PLANE_CURSOR:
                    targetPlane = (*cursorPlane);
                    cursorPlane++;
                    break;
                default:
                    MESON_LOGE("Error: invalid composition type!");
            }

            if (targetPlane.get()) {
                if (sourceFb.get())
                    targetPlane->setPlane(sourceFb);
                else
                    targetPlane->setPlane(layer);

                targetPlane->blank(planeBlank);
            }
            addCompositionInfo(layer, NULL, targetPlane, planeBlank);
        }

        mDummyComposer->start();
        mUiComposer->start();
    } else {
        MESON_LOGE("layers is empty.");
    }

    /*Set blank framebuffer to */
    while (amVideoPlane != mAmVideoPlanes.end()) {
        addCompositionInfo(NULL, NULL, (*amVideoPlane), BLANK_FOR_NO_CONENT);
        (*amVideoPlane++)->blank(BLANK_FOR_NO_CONENT);
    }
    while (hwcVideoPlane != mHwcVideoPlanes.end()) {
        addCompositionInfo(NULL, NULL, (*hwcVideoPlane), BLANK_FOR_NO_CONENT);
        (*hwcVideoPlane++)->blank(BLANK_FOR_NO_CONENT);
    }

    if (mOsdPlaneAssignedManually) {
        while (osdContinuousPlane != mOsdContinuousPlanes.end()) {
            addCompositionInfo(NULL, NULL, (*osdContinuousPlane), BLANK_FOR_NO_CONENT);
            (*osdContinuousPlane++)->blank(BLANK_FOR_NO_CONENT);
        }
        while (osdDiscretePlane != mOsdDiscretePlanes.end()) {
            addCompositionInfo(NULL, NULL, (*osdDiscretePlane), BLANK_FOR_NO_CONENT);
            (*osdDiscretePlane++)->blank(BLANK_FOR_NO_CONENT);
        }
    } else {
        while (osdPlane != mPresentOsdPlanes.end()) {
            addCompositionInfo(NULL, NULL, (*osdPlane), BLANK_FOR_NO_CONENT);
            (*osdPlane++)->blank(BLANK_FOR_NO_CONENT);
        }
    }

    while (cursorPlane != mCursorPlanes.end()) {
        addCompositionInfo(NULL, NULL, (*cursorPlane), BLANK_FOR_NO_CONENT);
        (*cursorPlane++)->blank(BLANK_FOR_NO_CONENT);
    }

    return 0;
}

void SimpleStrategy::addCompositionInfo(
    std::shared_ptr<DrmFramebuffer>  layer,
    std::shared_ptr<IComposeDevice>  composer __unused,
    std::shared_ptr<HwDisplayPlane>  plane,
    int planeBlank) {
    const char * planeName = "NULL";
    const char * blankStat = "";
    int32_t layerZ = layer.get() ? layer->mZorder : -1;
    const char * compType = layer.get() ?
        compositionTypeToString(layer->mCompositionType) : "";

    if (plane.get()) {
        planeName = plane->getName();
        switch (planeBlank) {
            case UNBLANK:
                blankStat = "UnBlank";
                break;
            case BLANK_FOR_NO_CONENT:
                blankStat = "Blank";
                break;
            case BLANK_FOR_SECURE_CONTENT:
                blankStat = "Secure-Blank";
                break;
            default:
                blankStat = "Unknown";
                break;
        }
    } else {
        planeName = "";
        blankStat = "";
    }

    mDumpStr.append("+------+-------------+----------+--------------+\n");
    mDumpStr.appendFormat("|%6d|%13s|%10s|%14s|\n",
        layerZ, compType, planeName, blankStat);
}

void SimpleStrategy::dump(String8 & dumpstr) {
    dumpstr.appendFormat("Compostion: SimpleStrategy(ForceClient %d, HideSecure %d):\n",
        mForceClientComposer, mHideSecureLayer);
    dumpstr.append("------------------------------------------------\n");
    dumpstr.append("|zorder|  Comp Type  |  Plane   |  PlaneStat   |\n");
    dumpstr.append(mDumpStr);
    dumpstr.append("------------------------------------------------\n");
}

