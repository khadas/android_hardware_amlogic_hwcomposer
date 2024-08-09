/*
 *Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 *This source code is subject to the terms and conditions defined in the
 *file 'LICENSE' which is part of this source code package.
 *
 *Description:
 */

#ifndef HWC_GE2DHELPER_H
#define HWC_GE2DHELPER_H

#include <aml_ge2d.h>
#include <ge2d_port.h>
#include <MesonLog.h>

struct Ge2dBufferInfo {
    int fd = -1;
    int fmt = 0;
    int w = 0;
    int h = 0;
    int stride = 0;
};

class Ge2dHelper {

public:
    Ge2dHelper();

    ~Ge2dHelper();

    int ge2DFmtConvert(Ge2dBufferInfo srcInfo, Ge2dBufferInfo dstInfo, int32_t rotation = 0);

private:
    inline void clearGe2DInfo();

    aml_ge2d_t m_amlge2d;
};


#endif //HWC_GE2DHELPER_H
