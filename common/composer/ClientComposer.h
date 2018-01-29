/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef CLIENT_COMPOSER_H
#define CLIENT_COMPOSER_H

#include <IComposeDevice.h>

#define CLIENT_COMPOSER_NAME "Client"

/*
 * ClientComposer: ask SurfaceFlinger to do composition.
 */
class ClientComposer : public IComposeDevice {
public:
    ClientComposer();
    ~ClientComposer();

    const char* getName() { return CLIENT_COMPOSER_NAME; }

    bool isCompositionSupport(meson_compositon_t type);

    bool isFbSupport(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t prepare();

    meson_compositon_t getCompostionType(
        std::shared_ptr<DrmFramebuffer> & fb);

    int32_t addInput(std::shared_ptr<DrmFramebuffer> & fb);

    int32_t setOutput(std::shared_ptr<DrmFramebuffer> & fb,
        hwc_region_t damage);

    int32_t start();

    std::shared_ptr<DrmFramebuffer> getOutput();

protected:
    std::shared_ptr<DrmFramebuffer> mClientTarget;
};

#endif/*CLIENT_COMPOSER_H*/
