/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>
#define EGL_NO_CONFIG ((EGLConfig)0)

#include <MesonLog.h>
#include "CompositionProcessor.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <ui/Fence.h>

const GLfloat gTriangleVertices[] = {
       -1.0f, 1.0f, 0.0f,       // Position 0
       -1.0f, -1.0f, 0.0f,  // Position 1
       1.0f, -1.0f, 0.0f,       // Position 2
       1.0f, 1.0f, 0.0f,        // Position 3
};
GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

const GLfloat gTextureCoords[] = {
       0.0f,  1.0f,   // TexCoord 0
       0.0f,  0.0f,   // TexCoord 1
       1.0f,  0.0f,   // TexCoord 2
       1.0f,  1.0f    // TexCoord 3
};

static const char gVertexShader[] = "attribute vec4 vPosition;\n"
    "varying vec2 yuvTexCoords;\n"
    "attribute vec2 a_texCoord;\n"
    "void main() {\n"
    "  yuvTexCoords = a_texCoord;\n"
    "  gl_Position = vPosition;\n"
    "}\n";

//Just show the white board when the pixel's alpha is Nonzero
static const char gFragmentShader[] = "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES yuvTexSampler;\n"
    "uniform samplerExternalOES WBTexSampler;\n"
    "varying vec2 yuvTexCoords;\n"
    "uniform float posx;\n"
    "uniform float posy;\n"
    "void main() {\n"
    "    vec4 baseColor1;\n"
    "    vec4 baseColor2;\n"
    "    vec4 target;\n"
    "    if (yuvTexCoords.x > posx  && yuvTexCoords.y > posy) {\n"
    "        vec2 wbcoords = vec2(yuvTexCoords.x - posx, yuvTexCoords.y - posy);\n"
    "        baseColor1 = texture2D(WBTexSampler, wbcoords);\n"
    "        baseColor2 = texture2D(yuvTexSampler, yuvTexCoords);\n"
    "        target = mix(baseColor2, baseColor1, baseColor1.a);\n"
    "        gl_FragColor = vec4(target.r, target.g, target.b, (1.0 - (1.0 - baseColor2.a)*(1.0 - baseColor1.a)));\n"
    "    } else {\n"
    "        gl_FragColor = texture2D(yuvTexSampler, yuvTexCoords);\n"
    "    }\n"
    "}\n";


CompositionProcessor::CompositionProcessor() {
    mInitialized = false;
    mProgramId = -1;
    inWBTex = -1;
    wbbuffer = nullptr;
    inWBImg = nullptr;
    inUITex = -1;
    inUIImg = nullptr;
    uibuffer = nullptr;

    memset(&outImg , 0, sizeof(outImg ));
    memset(&outbuffer , 0, sizeof(outbuffer ));
    gvPositionHandle = -1;;
    gvTexcoord = -1;;
    gwbPosX = -1;
    gwbPosY = -1;
    gYuvTexSamplerHandle = -1;
    gWBTexSamplerHandle = -1;
}

CompositionProcessor::~CompositionProcessor() {
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        MESON_LOGE("after %s() glError (0x%x)\n", op, error);
    }
}

void CompositionProcessor::checkglerror(){
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        switch (error) {
        case GL_INVALID_ENUM:
            MESON_LOGE("GL Error: GL_INVALID_ENUM");
            break;
        case GL_INVALID_VALUE:
            MESON_LOGE("GL Error: GL_INVALID_VALUE ");
            break;
        case GL_INVALID_OPERATION:
            MESON_LOGE("GL Error: GL_INVALID_OPERATION");
            break;
        case GL_OUT_OF_MEMORY:
            MESON_LOGE("GL Error: GL_OUT_OF_MEMORY");
            break;
        default:
            MESON_LOGE("GL Error: 0x%x",error);
            break;
        }

    }
}

GLuint CompositionProcessor::loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    MESON_LOGE("Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
            } else {
                MESON_LOGE("Guessing at GL_INFO_LOG_LENGTH size\n");
                char* buf = (char*) malloc(0x1000);
                if (buf) {
                    glGetShaderInfoLog(shader, 0x1000, NULL, buf);
                    MESON_LOGE("Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
            }
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

GLuint CompositionProcessor::createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }
    MESON_LOGE("creat vertexshader ok");

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }
    MESON_LOGE("creat pixelshader ok");

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    MESON_LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    MESON_LOGE("creat program ok");

    return program;
}


int32_t CompositionProcessor::setup() {
    if (mInitialized == false) {
        mWBHelper.createEglContext();
        mProgramId = createProgram(gVertexShader, gFragmentShader);
        mInitialized = true;
    }

    return 0;
}

int32_t CompositionProcessor::update(drm_rect_t pos __unused) {
    mPos[0] = (float(pos.left)) / (float(3840));
    mPos[1] = (float(pos.top)) / (float(2160));
    return 0;
}

int32_t CompositionProcessor::composite(
    std::shared_ptr<DrmFramebuffer> & inputUIfb ,std::shared_ptr<DrmFramebuffer> & inputWBfb ,
    std::shared_ptr<DrmFramebuffer> & outfb ) {
    ATRACE_CALL();
    //Refresh multithreading protection to avoid context loss caused by thread switching
    if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
        EGLDisplay mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
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
        mWBHelper.mContext = eglCreateContext(mDisplay, EGL_NO_CONFIG, mWBHelper.mContext,
            ctxConfigs);
        if (mWBHelper.mContext == EGL_NO_CONTEXT) {
            MESON_LOGE("eglCreateContext error: %#x\n", eglGetError());
            return -EINVAL;
        }

        rtn = eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mWBHelper.mContext);
        if (rtn != EGL_TRUE) {
            MESON_LOGE("eglMakeCurrent error: %#x\n", eglGetError());
            return -EINVAL;
        }
        mThreadChanged = true;
    }

    int outfbId = (int)outfb->getUniqueId();
    if (outbuf[outfbId] == NULL) {
        outbuf[outfbId] = new GraphicBuffer(outfb->mBufferHandle,
            GraphicBuffer:: WRAP_HANDLE,
            am_gralloc_get_width(outfb->mBufferHandle),
            am_gralloc_get_height(outfb->mBufferHandle),
            am_gralloc_get_format(outfb->mBufferHandle),
            1,
            am_gralloc_get_consumer_usage(outfb->mBufferHandle),
            am_gralloc_get_stride_in_pixel(outfb->mBufferHandle));
        outbuffer[outfbId] = outbuf[outfbId]->getNativeBuffer();
        mWBHelper.createImage (outbuffer[outfbId], &outImg[outfbId], false);

        glGenTextures(1,&outTex[outfbId]);
        glGenFramebuffers(1, &outFBO[outfbId]);

        glBindTexture(GL_TEXTURE_2D, outTex[outfbId]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3840, 2160, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)outImg[outfbId]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, outFBO[outfbId]);
    glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                outTex[outfbId],
                0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        MESON_LOGE("Framebuffer create failed! %x", status);

    if (mFirst || mThreadChanged) {
        gvPositionHandle = glGetAttribLocation(mProgramId, "vPosition");
        checkGlError("glGetAttribLocation vPosition");

        gvTexcoord = glGetAttribLocation(mProgramId, "a_texCoord");
        checkGlError("glGetAttribLocation a_texCoord");

        gwbPosX = glGetUniformLocation(mProgramId, "posx");
        checkGlError("glGetUniformLocation posx");

        gwbPosY = glGetUniformLocation(mProgramId, "posy");
        checkGlError("glGetUniformLocation posy");

        gYuvTexSamplerHandle = glGetUniformLocation(mProgramId, "yuvTexSampler");
        checkGlError("glGetUniformLocation");

        gWBTexSamplerHandle = glGetUniformLocation(mProgramId, "WBTexSampler");
        checkGlError("glGetUniformLocation");

        glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT,GL_FALSE, 3 * sizeof (GLfloat), gTriangleVertices);
        checkGlError("glVertexAttribPointer Position ");
        glEnableVertexAttribArray(gvPositionHandle);
        checkGlError("glEnableVertexAttribArray Position");

        glVertexAttribPointer (gvTexcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof (GLfloat), gTextureCoords);
        checkGlError("glVertexAttribPointer textureCoord");
        glEnableVertexAttribArray(gvTexcoord);
        checkGlError("glEnableVertexAttribArray textureCoord");

        wbbuf = new GraphicBuffer(inputWBfb->mBufferHandle,
            GraphicBuffer:: WRAP_HANDLE,
            am_gralloc_get_width(inputWBfb->mBufferHandle),
            am_gralloc_get_height(inputWBfb->mBufferHandle),
            am_gralloc_get_format(inputWBfb->mBufferHandle),
            1,
            am_gralloc_get_consumer_usage(inputWBfb->mBufferHandle),
            am_gralloc_get_stride_in_pixel(inputWBfb->mBufferHandle));
        wbbuffer = wbbuf->getNativeBuffer();
        mWBHelper.createImage (wbbuffer, &inWBImg, false);
        mWBHelper.createExternalTexture(inWBImg, &inWBTex);

        uibuf = new GraphicBuffer(inputUIfb->mBufferHandle,
            GraphicBuffer:: WRAP_HANDLE,
            am_gralloc_get_width(inputUIfb->mBufferHandle),
            am_gralloc_get_height(inputUIfb->mBufferHandle),
            am_gralloc_get_format(inputUIfb->mBufferHandle),
               1,
            am_gralloc_get_consumer_usage(inputUIfb->mBufferHandle),
            am_gralloc_get_stride_in_pixel(inputUIfb->mBufferHandle));
        uibuffer = uibuf->getNativeBuffer();
        mWBHelper.createImage (uibuffer, &inUIImg, false);
        mWBHelper.createExternalTexture(inUIImg, &inUITex);

        mFirst = false;
    }

    mThreadChanged = false;

    float width = am_gralloc_get_width(outfb->mBufferHandle);
    float height = am_gralloc_get_height(outfb->mBufferHandle);

    glViewport(0, 0, width, height);
    checkGlError("glViewport");

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    glUseProgram(mProgramId);
    checkGlError("glUseProgram");

    glUniform1f (gwbPosX, mPos[0]);
    glUniform1f (gwbPosY, mPos[1]);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gYuvTexSamplerHandle, 0);
    checkGlError("glUniform1i");
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, inUITex);
    checkGlError("glBindTexture");

    glActiveTexture(GL_TEXTURE1);
    glUniform1i(gWBTexSamplerHandle, 1);
    checkGlError("glUniform1i");
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, inWBTex);
    checkGlError("glBindTexture");

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    checkGlError("glDrawElements");

    EGLSyncKHR sync = eglCreateSyncKHR(eglGetCurrentDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (sync == EGL_NO_SYNC_KHR) {
        MESON_LOGE("failed to create EGL native fence sync: %#x", eglGetError());
    }

    glFlush();

    int fenceFd = eglDupNativeFenceFDANDROID(eglGetCurrentDisplay(), sync);
    if (fenceFd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
        MESON_LOGE("failed to dup EGL native fence sync: %#x", eglGetError());
    }

    eglDestroySyncKHR(eglGetCurrentDisplay(), sync);

    outfb->setAcquireFence(dup(fenceFd));

    ATRACE_BEGIN("waiting for GPU completion");
    mBufferLock.try_lock();
    DrmFence fence(fenceFd);
    fence.wait(3000);
    mBufferLock.unlock();
    ATRACE_END();

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;
}

void CompositionProcessor::enableSyncProtection(bool mode) {
    if (mode == true) {
        mBufferLock.lock();
    } else {
        mBufferLock.unlock();
    }
    return;
}

int32_t CompositionProcessor::teardown() {
    if (mInitialized) {
        mWBHelper.destroyEglContext();
        glDeleteProgram(mProgramId);
        mWBHelper.destroyImage(outImg[1]);
        mWBHelper.destroyExternalTexture (inWBTex);
        mWBHelper.destroyImage(inWBImg);

        mWBHelper.destroyExternalTexture (inUITex);
        mWBHelper.destroyImage(inUIImg);

        mWBHelper.destroyNativeBuffer(wbbuffer);
        mWBHelper.destroyNativeBuffer(uibuffer);
        mWBHelper.destroyNativeBuffer(outbuffer[0]);
        mWBHelper.destroyNativeBuffer(outbuffer[1]);
        glDeleteFramebuffers(1, &outFBO[0]);
        glDeleteFramebuffers(1, &outFBO[1]);
        glDeleteTextures(1, &outTex[0]);
        glDeleteTextures(1, &outTex[1]);

        mInitialized = false;
    }
    return 0;
}

int32_t CompositionProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb __unused,
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int & processFence __unused) {
    return 0;
}

int32_t CompositionProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb,
    std::shared_ptr<DrmFramebuffer> & outfb) {
    UNUSED(inputfb);
    UNUSED(outfb);
    return 0;
}

int32_t CompositionProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence __unused) {
    return 0;
}
