/*
 *Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 *This source code is subject to the terms and conditions defined in the
 *file 'LICENSE' which is part of this source code package.
 *
 *Description:
 */
#ifndef GL_COMMONHELPER_H
#define GL_COMMONHELPER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <BasicTypes.h>
#include <DrmFramebuffer.h>
#include <ui/GraphicBuffer.h>

class WBGLHelper {
public:
    WBGLHelper();
    ~WBGLHelper();

    int32_t createEglContext();
    void destroyEglContext();

    ANativeWindowBuffer * createNativeBuffer(std::shared_ptr<DrmFramebuffer> & fb);
    void destroyNativeBuffer(ANativeWindowBuffer * buffer);

    int32_t createImage(ANativeWindowBuffer * buf, EGLImageKHR * image, bool isProtected);
    int32_t destroyImage(EGLImageKHR  image);

    int32_t createExternalTexture(EGLImageKHR img, GLuint* tex);
    int32_t destroyExternalTexture(GLuint tex);

    int32_t bindFBO(EGLImageKHR img, GLuint* tex, GLuint* fbo);
    int32_t unbindFBO(GLuint fbo, GLuint tex);
    EGLDisplay mDisplay;
    EGLContext mContext;

private:
    EGLConfig mConfig;
};

#endif

