/*
 *Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 *
 *This source code is subject to the terms and conditions defined in the
 *file 'LICENSE' which is part of this source code package.
 *
 *Description:
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "Ge2dHelperer"
#include <cerrno>
#include "Ge2dHelper.h"

Ge2dHelper::Ge2dHelper() {
    memset(&m_amlge2d, 0, sizeof(aml_ge2d_t));
    memset(&(m_amlge2d.ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.dst_info), 0, sizeof(buffer_info_t));
    int ret = aml_ge2d_init(&m_amlge2d);
    if (ret < 0) {
        aml_ge2d_exit(&m_amlge2d);
        ALOGE("%s: %s", __FUNCTION__, strerror(errno));
    }
}

Ge2dHelper::~Ge2dHelper() {
    aml_ge2d_mem_free(&m_amlge2d);
    aml_ge2d_exit(&m_amlge2d);
}

//format convert. eg. UYVY->NV12
int Ge2dHelper::ge2DFmtConvert(Ge2dBufferInfo srcInfo, Ge2dBufferInfo dstInfo, int rotation) {
    clearGe2DInfo();
    m_amlge2d.ge2dinfo.src_info[0].shared_fd[0] = srcInfo.fd;
    m_amlge2d.ge2dinfo.src_info[0].memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.src_info[0].mem_alloc_type = AML_GE2D_MEM_ION;;
    m_amlge2d.ge2dinfo.src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    m_amlge2d.ge2dinfo.src_info[1].mem_alloc_type = AML_GE2D_MEM_INVALID;

    m_amlge2d.ge2dinfo.src_info[0].plane_number = 1;
    m_amlge2d.ge2dinfo.src_info[0].canvas_w = srcInfo.stride;
    m_amlge2d.ge2dinfo.src_info[0].canvas_h = srcInfo.h;
    m_amlge2d.ge2dinfo.src_info[0].rect.x = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.y = 0;
    m_amlge2d.ge2dinfo.src_info[0].rect.w = srcInfo.w;
    m_amlge2d.ge2dinfo.src_info[0].rect.h = srcInfo.h;

    m_amlge2d.ge2dinfo.src_info[0].format = srcInfo.fmt; //PIXEL_FORMAT_YCbCr_422_UYVY; //PIXEL_FORMAT_YCbCr_420_SP_NV12
    m_amlge2d.ge2dinfo.src_info[0].plane_alpha = 0xFF; /* global plane alpha*/

    m_amlge2d.ge2dinfo.dst_info.shared_fd[0] = dstInfo.fd;
    m_amlge2d.ge2dinfo.dst_info.memtype = GE2D_CANVAS_ALLOC;
    m_amlge2d.ge2dinfo.dst_info.mem_alloc_type = AML_GE2D_MEM_ION;;
    m_amlge2d.ge2dinfo.dst_info.plane_number = 1;
    m_amlge2d.ge2dinfo.dst_info.canvas_w = dstInfo.stride;
    m_amlge2d.ge2dinfo.dst_info.canvas_h = dstInfo.h;
    m_amlge2d.ge2dinfo.dst_info.rect.x = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.y = 0;
    m_amlge2d.ge2dinfo.dst_info.rect.w = dstInfo.w;
    m_amlge2d.ge2dinfo.dst_info.rect.h = dstInfo.h;
    m_amlge2d.ge2dinfo.dst_info.rotation = rotation;
    m_amlge2d.ge2dinfo.dst_info.format = dstInfo.fmt; //PIXEL_FORMAT_YCbCr_420_SP_NV12; //PIXEL_FORMAT_RGBA_8888;
    m_amlge2d.ge2dinfo.dst_info.plane_alpha = 0xFF; /* global plane alpha*/

    m_amlge2d.ge2dinfo.ge2d_op = AML_GE2D_STRETCHBLIT;
    int ret = aml_ge2d_process(&m_amlge2d.ge2dinfo);
    if (ret < 0) {
        ALOGE("ge2d process failed, %s (%d)\n", __func__, __LINE__);
    }
    return ret;
}

void Ge2dHelper::clearGe2DInfo() {
    memset(&(m_amlge2d.ge2dinfo.src_info[0]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.src_info[1]), 0, sizeof(buffer_info_t));
    memset(&(m_amlge2d.ge2dinfo.dst_info), 0, sizeof(buffer_info_t));
}

