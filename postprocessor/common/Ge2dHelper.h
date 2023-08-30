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

class Ge2dHelper {

public:
    Ge2dHelper();

    ~Ge2dHelper();

    int ge2DFmtConvert(int dst_fd, unsigned int dst_fmt, size_t dst_w, size_t dst_h, int src_fd,
                         unsigned int src_fmt, size_t src_w, size_t src_h);

private:
    inline void clearGe2DInfo();

    aml_ge2d_t m_amlge2d;
};


#endif //HWC_GE2DHELPER_H
