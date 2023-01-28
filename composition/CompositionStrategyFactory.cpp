/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "CompositionStrategyFactory.h"
#include "simplestrategy/SingleplaneComposition/SingleplaneComposition.h"
#include "simplestrategy/MultiplanesComposition/MultiplanesComposition.h"
#include "simplestrategy/MultiplanesWithDiComposition/MultiplanesWithDiComposition.h"
#include "simplestrategy/MultiplanesWithHRComposition/MultiplanesWithHRComposition.h"

#include <MesonLog.h>

std::shared_ptr<ICompositionStrategy> CompositionStrategyFactory::create(
    uint32_t type, uint32_t flags) {
    std::shared_ptr<ICompositionStrategy> strategy = NULL;

    if (type == SIMPLE_STRATEGY) {
        if (flags & MULTI_OSD_PLANES) {
            /*VPU have multi osd/video planes*/
            return std::make_shared<MultiplanesComposition>();
        }

        if (flags & MULTI_PLANES_WITH_DI) {
            return std::make_shared<MultiplanesWithDiComposition>();
        }

        if (flags & MULTI_PLANES_WITH_HR) {
            return std::make_shared<MultiplanesWithHRComposition>();
        }

       return std::make_shared<SingleplaneComposition>();
    }

    MESON_LOGE("Strategy: (%d) not supported", type);
    return NULL;
}
