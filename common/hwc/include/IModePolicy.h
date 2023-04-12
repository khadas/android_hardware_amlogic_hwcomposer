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

    //TODO: refactor it
    virtual void setActiveConfig(std::string mode) = 0;

    // defaut boot config
    virtual int32_t getPreferredBootConfig(std::string &config) = 0;
    virtual int32_t setBootConfig(std::string &config) = 0;
    virtual int32_t clearBootConfig() = 0;

    virtual int32_t setHdrConversionPolicy(bool passthrough, int32_t forceType) = 0;

    virtual void dump(String8 &dumpstr) = 0;
};
