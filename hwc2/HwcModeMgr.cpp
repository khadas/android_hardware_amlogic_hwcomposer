/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "HwcModeMgr.h"
#include "FixedSizeModeMgr.h"

std::shared_ptr<HwcModeMgr> createModeMgr(
    std::shared_ptr<HwDisplayConnector> & connector) {
    if (connector != NULL) {
        HwcModeMgr * modeMgr = (HwcModeMgr *) new FixedSizeModeMgr();
        return std::shared_ptr<HwcModeMgr>(std::move(modeMgr));
    }

    return NULL;
}
