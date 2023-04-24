/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef COMPOSITION_PROCESSOR_H
#define COMPOSITION_PROCESSOR_H
#include <FbProcessor.h>
#include "WBGLHelper.h"
#include <android-base/unique_fd.h>


class CompositionProcessor : public FbProcessor {
public:
    CompositionProcessor();
    ~CompositionProcessor();

    int32_t setup();
    int32_t composite(
        std::shared_ptr<DrmFramebuffer> & inputUIfb,std::shared_ptr<DrmFramebuffer> & inputWBfb,
        std::shared_ptr<DrmFramebuffer> & outfb);
    int32_t teardown();
    int32_t asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence);
    int32_t onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb,
        int releaseFence);

    int32_t process(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb);
    int32_t update(drm_rect_t pos);
    void enableSyncProtection(bool mode);
protected:
    GLuint createProgram(const char* pVertexSource, const char* pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char* pSource);
    void checkglerror();

    GLuint mProgramId;
    WBGLHelper mWBHelper;
    bool mInitialized;
    GLfloat mPos[2];

    std::mutex mBufferLock;

    //white board
    GLuint inWBTex;
    sp<GraphicBuffer> wbbuf;
    ANativeWindowBuffer * wbbuffer;
    EGLImageKHR inWBImg;
    //OSD
    GLuint inUITex;
    EGLImageKHR inUIImg;
    sp<GraphicBuffer> uibuf;
    ANativeWindowBuffer * uibuffer;

    //Render Target
    EGLImageKHR outImg[2];
    GLuint outTex[2], outFBO[2];
    sp<GraphicBuffer> outbuf[2];
    ANativeWindowBuffer * outbuffer[2];

    bool mFirst = true;

    GLint gvPositionHandle;
    GLint gvTexcoord;
    GLint gwbPosX;
    GLint gwbPosY;
    GLint gYuvTexSamplerHandle;
    GLint gWBTexSamplerHandle;

    bool mThreadChanged = false;
};

#endif
