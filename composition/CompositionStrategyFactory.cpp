/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "CompositionStrategyFactory.h"
#include "SimpleStrategy.h"

#include <MesonLog.h>

std::shared_ptr<ICompositionStrategy> CompositionStrategyFactory::create(
    uint32_t type, uint32_t flags __unused) {
    switch (type) {
        case SIMPLE_STRATEGY:
            return std::make_shared<SimpleStrategy>();
        default:
            MESON_LOGE("Strategy: (%d) not supported", type);
            return NULL;
    }
}

