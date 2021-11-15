/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef MESON_VT_OBSERVER_H
#define MESON_VT_OBSERVER_H

class VtObserver {
public:
    VtObserver() { }
    virtual ~VtObserver() { }
    //game enable or not
    virtual int32_t onGameMode(bool enable) = 0;
    virtual int32_t onFrameAvailable() = 0;
};

#endif  /* MESON_VT_OBSERVER_H */
