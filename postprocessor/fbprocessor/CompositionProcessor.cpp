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
    "    if (yuvTexCoords.x > posx  && yuvTexCoords.y > posy) {\n"
    "        vec2 wbcoords = vec2(yuvTexCoords.x - posx, yuvTexCoords.y - posy);\n"
    "        baseColor1 = texture2D(WBTexSampler, wbcoords);\n"
    "        if(baseColor1.a > 0.5) {\n"
    "            gl_FragColor = baseColor1;\n"
    "        } else {\n"
    "            gl_FragColor = texture2D(yuvTexSampler, yuvTexCoords);\n"
    "        }\n"
    "    } else {\n"
    "        gl_FragColor = texture2D(yuvTexSampler, yuvTexCoords);\n"
    "    }\n"
    "}\n";

CompositionProcessor::CompositionProcessor() {
    mInitialized = false;
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
    }

    EGLImageKHR outImg;
    GLuint outTex,outFBO;
    ANativeWindowBuffer * outBuf = mWBHelper.createNativeBuffer (outfb);
    bool outProtect = am_gralloc_is_secure_buffer(outfb->mBufferHandle) ? true:false;
    mWBHelper.createImage (outBuf, &outImg, outProtect);
    mWBHelper.bindFBO(outImg, &outTex, &outFBO);

    GLuint inUITex;
    EGLImageKHR inUIImg;
    ANativeWindowBuffer * inUIBuf = mWBHelper.createNativeBuffer(inputUIfb);
    bool inUIProtect = am_gralloc_is_secure_buffer(inputUIfb->mBufferHandle) ? true:false;
    mWBHelper.createImage (inUIBuf, &inUIImg, inUIProtect);
    mWBHelper.createExternalTexture(inUIImg, &inUITex);

    GLuint inWBTex;
    EGLImageKHR inWBImg;
    ANativeWindowBuffer * inWBBuf = mWBHelper.createNativeBuffer(inputWBfb);
    bool inWBProtect = am_gralloc_is_secure_buffer(inputWBfb->mBufferHandle) ? true:false;
    mWBHelper.createImage (inWBBuf, &inWBImg, inWBProtect);
    mWBHelper.createExternalTexture(inWBImg, &inWBTex);

    float width = am_gralloc_get_width(outfb->mBufferHandle);
    float height = am_gralloc_get_height(outfb->mBufferHandle);

    glViewport(0, 0, width, height);
    checkGlError("glViewport");

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    GLint gvPositionHandle;
    GLint gvTexcoord;
    GLint gwbPosX;
    GLint gwbPosY;
    GLint gYuvTexSamplerHandle;
    GLint gWBTexSamplerHandle;

    const GLfloat gTriangleVertices[] = {
        -1.0f, 1.0f, 0.0f,   // Position 0
        -1.0f, -1.0f, 0.0f,  // Position 1
        1.0f, -1.0f, 0.0f,   // Position 2
        1.0f, 1.0f, 0.0f,    // Position 3
    };
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

    const GLfloat gTextureCoords[] = {
        0.0f,  1.0f,   // TexCoord 0
        0.0f,  0.0f,   // TexCoord 1
        1.0f,  0.0f,   // TexCoord 2
        1.0f,  1.0f    // TexCoord 3
    };

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

    glUseProgram(mProgramId);
    checkGlError("glUseProgram");

    glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT,GL_FALSE, 3 * sizeof (GLfloat), gTriangleVertices);
    checkGlError("glVertexAttribPointer Position ");
    glEnableVertexAttribArray(gvPositionHandle);
    checkGlError("glEnableVertexAttribArray Position");

    glVertexAttribPointer (gvTexcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof (GLfloat), gTextureCoords);
    checkGlError("glVertexAttribPointer textureCoord");
    glEnableVertexAttribArray(gvTexcoord);
    checkGlError("glEnableVertexAttribArray textureCoord");

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

    close(fenceFd);
    glUseProgram(0);
    mWBHelper.unbindFBO(outFBO, outTex);
    mWBHelper.destroyImage(outImg);

    mWBHelper.destroyExternalTexture (inWBTex);
    mWBHelper.destroyImage(inWBImg);

    mWBHelper.destroyExternalTexture (inUITex);
    mWBHelper.destroyImage(inUIImg);

    mWBHelper.destroyNativeBuffer(outBuf);
    mWBHelper.destroyNativeBuffer(inUIBuf);
    mWBHelper.destroyNativeBuffer(inWBBuf);

    return 0;
}

int32_t CompositionProcessor::teardown() {
    if (mInitialized) {
        mWBHelper.destroyEglContext();
        glDeleteProgram(mProgramId);
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
