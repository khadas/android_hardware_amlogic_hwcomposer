/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "HwConnectorFactory.h"
#include "ConnectorHdmi.h"
#include "ConnectorPanel.h"

HwDisplayConnector* HwConnectorFactory::create(
    drm_connector_type_t connectorType,
    int32_t connectorDrv,
    uint32_t connectorId) {
    switch (connectorType) {
        case DRM_MODE_CONNECTOR_HDMI:
            return new ConnectorHdmi(connectorDrv, connectorId);
        case DRM_MODE_CONNECTOR_PANEL:
            return new ConnectorPanel(connectorDrv, connectorId);
        case DRM_MODE_CONNECTOR_CVBS:
        default:
            break;
    }

    return NULL;
}

