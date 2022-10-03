/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once

#include <stdlib.h>
#include <string>


class IModePolicy {
public:
    IModePolicy() {}
    virtual ~IModePolicy() { }

    virtual int32_t bindConnector(std::shared_ptr<HwDisplayConnector> & connector);

    virtual bool setPolicy(int32_t policy) = 0;

    virtual int32_t initialize() = 0;

    virtual void onHotplug(bool connected) = 0;

    virtual void dump(std::string &dumpstr) = 0;

};
