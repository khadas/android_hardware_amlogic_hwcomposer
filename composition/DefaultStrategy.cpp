/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

int32_t StrategyNode::addChild(std::shared_ptr<StrategyNode> node) {
    child.push_back(node);
    return 0;
}

void DefaultStrategy::cleanup() {
    mLayers.clear();
    mComposers.clear();
    mPlanes.clear();

    mLayerNodes.clear();
    mComposerNodes.clear();
    mVideoNodes.clear();
    mOsdNodes.clear();
    mCursorNodes.clear();

    mOutputNodes.clear();

    mDummyConsumer.reset();
    mClientConsumer.reset();
}

int32_t DefaultStrategy::buildStrategyNodes() {
    int nodeidx = 0;

    /*build layer nodes.*/
    std::vector<std::shared_ptr<Hwc2Layer>>::iterator layer_it;
    for (layer_it = mLayers.begin(), nodeidx = 0;
        layer_it != mLayers.end(); ++layer_it, ++nodeidx) {
        StrategyNode * node = new StrategyNode();
        node->type = LAYER_NODE;
        node->index = nodeidx;
        mLayerNodes.push_back(std::move(node));
    }

    /*build composer nodes.*/
    std::vector<std::shared_ptr<IComposeDevice>>::iterator composer_it;
    for (composer_it = mComposers.begin(), nodeidx = 0;
        composer_it != mComposers.end(); ++composer_it, ++nodeidx) {
        StrategyNode * node = new StrategyNode();
        node->type = COMPOSER_NODE;
        node->index = nodeidx;
        mComposerNodes.push_back(std::move(node));
    }

    /*build plane nodes.*/
    std::vector<std::shared_ptr<HwDisplayPlane>>::iterator plane_it;
    for (plane_it = mPlanes.begin(), nodeidx = 0;
        plane_it != mPlanes.end(); ++plane_it, ++nodeidx) {
        StrategyNode * node = new StrategyNode();
        node->type =  PLANE_NODE;
        node->index = nodeidx;
        mPlaneNodes.push_back(std::move(node));

        if (*plane_it->getPlaneType()  & OSD_PLANE) {
            mOsdNodes.push_back(std::move(node));
        } else if (*plane_it->getPlaneType()  & VIDEO_PLANE) {
            mVideoNodes.push_back(std::move(node));
        } else if (*plane_it->getPlaneType()  == CURSOR_PLANE) {
            mCursorNodes.push_back(std::move(node));
        }
    }

    return 0;
}

int32_t DefaultStrategy::findByCompositionType(
    uint32_t compositionType,
    std::vector<std::shared_ptr<StrategyNode>> & nodes) {
    int32_t ret = -ENXIO;

    //travel composer list to find valid composer.
    std::list<StrategyNode>::iterator it = mComposerNodes.begin();

    for ( ; it != mComposerNodes.end(); ++it) {
        uint32_t type = (*it)->getCompositionCapabilities();
        if (compositionType & type) {
            nodes.push_back(*it);
            ret = 0;
        }
    }

    return ret;
}

std::shared_ptr<StrategyNode> DefaultStrategy::findDummyComposer() {
    if (mDummyConsumer != NULL)
        return mDummyConsumer;

    std::vector<std::shared_ptr<StrategyNode>> composerNodes;
    if (!findByCompositionType(MESON_COMPOSITION_DUMMY, composerNodes)) {
        if (composerNodes.size() == 0) {
            MESON_LOGE("No composer find for MESON_COMPOSITION_DUMMY.");
        } else {
             if (composerNodes.size() > 1) {
                MESON_LOGE("More than one composer for MESON_COMPOSITION_DUMMY," \
                    "use the first one.");
             }
             mDummyConsumer = composerNodes.front();
        }
    } else {
        MESON_LOGE("Find composer for MESON_COMPOSITION_DUMMY failed.");
    }

    return mDummyConsumer;
}

std::shared_ptr<StrategyNode> DefaultStrategy::findClientComposer() {
    if (mClientConsumer != NULL)
        return mClientConsumer;

    std::vector<std::shared_ptr<StrategyNode>> composerNodes;
    if (!findByCompositionType(MESON_COMPOSITION_CLIENT, composerNodes)) {
        if (composerNodes.size() == 0) {
            MESON_LOGE("No composer find for MESON_COMPOSITION_CLIENT.");
        } else {
             if (composerNodes.size() > 1) {
                MESON_LOGE("More than one composer for MESON_COMPOSITION_CLIENT," \
                    "use the first one.");
             }
             mClientConsumer = composerNodes.front();
        }
    } else {
        MESON_LOGE("Find composer for MESON_COMPOSITION_CLIENT failed.");
    }

    return mClientConsumer;
}

int32_t DefaultStrategy::makeComb(
    std::vector<std::shared_ptr<Hwc2Layer>> layers,
    std::vector<std::shared_ptr<IComposeDevice>> composers,
    std::vector<std::shared_ptr<HwDisplayPlane>> planes) {
    bool haveClientComposer = false;
    int32_t videoFbNum = 0, cursorFbNum = 0;

    mLayers = layers;
    mComposers = composers;
    mPlanes = planes;

    buildStrategyNodes();

     std::list<std::shared_ptr<StrategyNode>>::iterator videoPanelIt =
            mVideoNodes.begin();
     std::list<std::shared_ptr<StrategyNode>>::iterator osdPanelIt  =
            mOsdNodes.begin();
     std::list<std::shared_ptr<StrategyNode>>::iterator cursorPanelIt =
            mCursorNodes.begin();
     std::vector<std::shared_ptr<StrategyNode>> consumerNodes;
     std::list<std::shared_ptr<StrategyNode>>::iterator layerNode;

    std::list<std::shared_ptr<StrategyNode>>::iterator mFirstClientNode
            = mLayerNodes.end();
    std::list<std::shared_ptr<StrategyNode>>::iterator mLastClientNode
            = mLayerNodes.end();

    /*
   * Handle special layers:
   * 1) MESON_COMPOSITION_VIDEO_SIDEBAND
   * 2) MESON_COMPOSITION_CURSOR
   * 3) MESON_COMPOSITION_DUMMY
   */
    for (layerNode = mLayerNodes.begin(); layerNode != mLayerNodes.end(); ++layerNode) {
        std::shared_ptr<Hwc2Layer> layer = mLayers[layerNode->index];
        switch (layer->mInternalComposition)
            case MESON_COMPOSITION_NONE:
                {
                    //handle video bypass mode.
                    if (layer->mFbType == DRM_FB_VIDEO_OVERLAY ||
                        layer->mFbType == DRM_FB_VIDEO_OMX ||
                        layer->mFbType == DRM_FB_VIDEO_SIDEBAND) {
                        videoFbNum++;
                        if (videoPanelIt != mVideoNodes.end()) {
                            layer->mCompositionType = MESON_COMPOSITION_PLANE_VIDEO_SIDEBAND;
                            *videoPanelIt->addChild(layerNode);
                            videoPanelIt ++;
                        } else {
                            layer->mCompositionType = MESON_COMPOSITION_DUMMY;
                        }
                    } else if (layer->mFbType == DRM_FB_CURSOR) {
                        cursorFbNum ++;
                        if (cursorPanelIt != mCursorNodes.end()) {
                            layer->mCompositionType = MESON_COMPOSITION_PLANE_CURSOR;
                            *cursorPanelIt->addChild(layerNode);
                            cursorPanelIt ++;
                        }
                    }
                }
                break;

            case MESON_COMPOSITION_CLIENT:
                if (mFirstClientNode == mLayerNodes.end()) {
                    mFirstClientNode = layerNode;
                }
                mLastClientNode = layerNode;
                haveClientComposer = true;
                break;

        //fall back to dummy composer.
        if (layer->mCompositionType == MESON_COMPOSITION_DUMMY) {
            std::shared_ptr<StrategyNode> composerNode = findDummyComposer();
            composerNode->addChild(layerNode);
        }
    }

    /*
   * Choose multi-layers composer, we only chosse one multi-layer composer
   * 1) if client layer exists, use client composer.
   * 2) check if there're DRM_FB_RENDER£¬select composer support DRM_FB_RENDER.
   */
    std::shared_ptr<StrategyNode> composerNode;

    if (haveClientComposer) {
        composerNode = findClientComposer();
        for (layerNode = mFirstClientNode;
            layerNode != mLastClientNode; ++layerNode) {
                std::shared_ptr<Hwc2Layer> layer = mLayers[layerNode->index];
                if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
                    layer->mCompositionType = MESON_COMPOSITION_CLIENT;
                    composerNode->addChild(layerNode);
                }
        }

        for (layerNode = mLayerNodes.begin(); layerNode != mLayerNodes.end(); ++layerNode) {
            std::shared_ptr<Hwc2Layer> layer = mLayers[layerNode->index];
            if (layer->mCompositionType == MESON_COMPOSITION_NONE) {
                mOutputNodes.push_back(layerNode);
            } else if (layer->mCompositionType == MESON_COMPOSITION_CLIENT) {
                mOutputNodes.push_back(composerNode);
            }
        }
    }


}

