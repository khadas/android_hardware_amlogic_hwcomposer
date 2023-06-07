/*
 *Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 *This source code is subject to the terms and conditions defined in the
 *file 'LICENSE' which is part of this source code package.
 *
 *Description:
 */

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#define EGL_NO_CONFIG ((EGLConfig)0)
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include <MesonLog.h>
#include <WBGLHelper.h>
#include <cutils/properties.h>

WBGLHelper::WBGLHelper() {
    mDisplay = nullptr;
    mContext = nullptr;
    mConfig = nullptr;

}

WBGLHelper::~WBGLHelper() {
}

void printEGLConfiguration(EGLDisplay dpy, EGLConfig config) {

#define X(VAL) {VAL, #VAL}
    struct {EGLint attribute; const char* name;} names[] = {
    X(EGL_BUFFER_SIZE),
    X(EGL_ALPHA_SIZE),
    X(EGL_BLUE_SIZE),
    X(EGL_GREEN_SIZE),
    X(EGL_RED_SIZE),
    X(EGL_DEPTH_SIZE),
    X(EGL_STENCIL_SIZE),
    X(EGL_CONFIG_CAVEAT),
    X(EGL_CONFIG_ID),
    X(EGL_LEVEL),
    X(EGL_MAX_PBUFFER_HEIGHT),
    X(EGL_MAX_PBUFFER_PIXELS),
    X(EGL_MAX_PBUFFER_WIDTH),
    X(EGL_NATIVE_RENDERABLE),
    X(EGL_NATIVE_VISUAL_ID),
    X(EGL_NATIVE_VISUAL_TYPE),
    X(EGL_SAMPLES),
    X(EGL_SAMPLE_BUFFERS),
    X(EGL_SURFACE_TYPE),
    X(EGL_TRANSPARENT_TYPE),
    X(EGL_TRANSPARENT_RED_VALUE),
    X(EGL_TRANSPARENT_GREEN_VALUE),
    X(EGL_TRANSPARENT_BLUE_VALUE),
    X(EGL_BIND_TO_TEXTURE_RGB),
    X(EGL_BIND_TO_TEXTURE_RGBA),
    X(EGL_MIN_SWAP_INTERVAL),
    X(EGL_MAX_SWAP_INTERVAL),
    X(EGL_LUMINANCE_SIZE),
    X(EGL_ALPHA_MASK_SIZE),
    X(EGL_COLOR_BUFFER_TYPE),
    X(EGL_RENDERABLE_TYPE),
    X(EGL_CONFORMANT),
   };
#undef X

    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
        EGLint value = -1;
        EGLint returnVal = eglGetConfigAttrib(dpy, config, names[j].attribute, &value);
        EGLint error = eglGetError();
        if (returnVal && error == EGL_SUCCESS) {
            MESON_LOGE(" %s: ", names[j].name);
            MESON_LOGE("%d (0x%x)", value, value);
        }
    }
    MESON_LOGE("\n");
}

int32_t WBGLHelper::createEglContext() {
    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mDisplay == EGL_NO_DISPLAY) {
        MESON_LOGE("eglGetDisplay error: %#x\n", eglGetError());
        return -EINVAL;
    }

    bool rtn = eglInitialize(mDisplay, NULL, NULL);
    if (rtn != EGL_TRUE) {
        MESON_LOGE("eglInitialize error: %#x\n", eglGetError());
        return -EINVAL;
    }

    EGLint ctxConfigs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
        EGL_NONE
    };
    mContext = eglCreateContext(mDisplay, EGL_NO_CONFIG, EGL_NO_CONTEXT,
        ctxConfigs);
    if (mContext == EGL_NO_CONTEXT) {
        MESON_LOGE("eglCreateContext error: %#x\n", eglGetError());
        return -EINVAL;
    }
    MESON_LOGE("in creat context ok");

    rtn = eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext);
    if (rtn != EGL_TRUE) {
        MESON_LOGE("eglMakeCurrent error: %#x\n", eglGetError());
        return -EINVAL;
    }
    MESON_LOGE("in make current ok");
    return 0;
}

void WBGLHelper::destroyEglContext() {
    if (mDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                EGL_NO_CONTEXT);
    }
    if (mContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mDisplay, mContext);
    }
    mDisplay = EGL_NO_DISPLAY;
    mContext = EGL_NO_CONTEXT;
    mConfig = 0;
}

ANativeWindowBuffer * WBGLHelper::createNativeBuffer(std::shared_ptr<DrmFramebuffer> & fb) {
    MESON_ASSERT(fb.get() != NULL, "createNativeBuffer need valid fb.");
    sp<GraphicBuffer> buf = new GraphicBuffer(fb->mBufferHandle,
                                                    GraphicBuffer:: WRAP_HANDLE,
                                                    am_gralloc_get_width(fb->mBufferHandle),
                                                    am_gralloc_get_height(fb->mBufferHandle),
                                                    am_gralloc_get_format(fb->mBufferHandle),
                                                    1,
                                                    am_gralloc_get_consumer_usage(fb->mBufferHandle),
                                                    am_gralloc_get_stride_in_pixel(fb->mBufferHandle));
    ANativeWindowBuffer * buffer = buf->getNativeBuffer();
    buffer->incStrong(buffer);
    return buffer;
}

void WBGLHelper::destroyNativeBuffer(ANativeWindowBuffer * buffer) {
    buffer->decStrong(buffer);
}

int32_t WBGLHelper::createImage(ANativeWindowBuffer * buf, EGLImageKHR * image, bool isProtected) {
    std::vector<EGLint> egl_img_attr;
    egl_img_attr.reserve(16);

    egl_img_attr.push_back(EGL_IMAGE_PRESERVED_KHR);
    egl_img_attr.push_back(EGL_TRUE);

    if (isProtected) {
        egl_img_attr.push_back(EGL_PROTECTED_CONTENT_EXT);
        egl_img_attr.push_back(EGL_TRUE);
    }
    egl_img_attr.push_back(EGL_NONE);

    *image = eglCreateImageKHR(mDisplay,
                                        EGL_NO_CONTEXT,
                                        EGL_NATIVE_BUFFER_ANDROID,
                                        static_cast<EGLClientBuffer>(buf),
                                        egl_img_attr.data());
    if (*image == EGL_NO_IMAGE_KHR) {
        MESON_LOGE("createImage for buf %p-%x failed.", buf, eglGetError());
        return -ENOMEM;
    }

    return 0;
}

int32_t WBGLHelper::destroyImage(EGLImageKHR image) {
    eglDestroyImageKHR(mDisplay, image);
    return 0;
}

int32_t WBGLHelper::createExternalTexture(EGLImageKHR img, GLuint* tex) {
    GLuint tname;
    glGenTextures(1, &tname);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tname);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img);

    *tex = tname;
    return 0;
}

int32_t WBGLHelper::destroyExternalTexture(GLuint tex) {
    glDeleteTextures(1, &tex);
    return 0;
}

int32_t WBGLHelper::bindFBO(EGLImageKHR img, GLuint* tex, GLuint* fbo) {
    GLuint tname, fboName;
    glGenTextures(1, &tname);
    glGenFramebuffers(1, &fboName);

    glBindTexture(GL_TEXTURE_2D, tname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3840, 2160, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);



    glBindFramebuffer(GL_FRAMEBUFFER, fboName);
    glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                tname,
                0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        MESON_LOGE("Framebuffer create failed! %x", status);

    *tex = tname;
    *fbo = fboName;
    return 0;
}

int32_t WBGLHelper::unbindFBO(GLuint fbo, GLuint tex) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return 0;
}
