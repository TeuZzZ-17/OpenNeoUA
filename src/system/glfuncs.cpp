// OpenGL function loading.
#include "glfuncs.h"
#include <stdexcept>

namespace GFX
{
PFNGLGENFRAMEBUFFERSPROC Glext::GLGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC Glext::GLBindFramebuffer = nullptr;
PFNGLGENRENDERBUFFERSPROC Glext::GLGenRenderbuffers = nullptr;
PFNGLBINDRENDERBUFFERPROC Glext::GLBindRenderbuffer = nullptr;
PFNGLFRAMEBUFFERTEXTUREPROC Glext::GLFrameBufferTexture = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC Glext::GLFrameBufferTexture2D = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC Glext::GLRenderbufferStorage = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC Glext::GLFramebufferRenderbuffer = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC Glext::GLCheckFramebufferStatus = nullptr;
PFNGLGENERATEMIPMAPPROC Glext::GLGenerateMipmap = nullptr;
PFNGLCREATESHADERPROC Glext::GLCreateShader = nullptr;
PFNGLSHADERSOURCEPROC Glext::GLShaderSource = nullptr;
PFNGLDELETESHADERPROC Glext::GLDeleteShader = nullptr;
PFNGLCOMPILESHADERPROC Glext::GLCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC Glext::GLCreateProgram = nullptr;
PFNGLDELETEPROGRAMPROC Glext::GLDeleteProgram = nullptr;
PFNGLATTACHSHADERPROC Glext::GLAttachShader = nullptr;
PFNGLLINKPROGRAMPROC Glext::GLLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC Glext::GLGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC Glext::GLGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC Glext::GLUseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC Glext::GLGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC Glext::GLUniform1f = nullptr;
PFNGLUNIFORM1IPROC Glext::GLUniform1i = nullptr;
PFNGLUNIFORM2FPROC Glext::GLUniform2f = nullptr;
PFNGLUNIFORM2IPROC Glext::GLUniform2i = nullptr;
PFNGLUNIFORM3FPROC Glext::GLUniform3f = nullptr;
PFNGLUNIFORM3IPROC Glext::GLUniform3i = nullptr;
PFNGLUNIFORM4FPROC Glext::GLUniform4f = nullptr;
PFNGLUNIFORM4IPROC Glext::GLUniform4i = nullptr;

PFNGLUNIFORMMATRIX2FVPROC Glext::GLUniformMatrix2fv = nullptr;
PFNGLUNIFORMMATRIX3FVPROC Glext::GLUniformMatrix3fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC Glext::GLUniformMatrix4fv = nullptr;

PFNGLGETATTRIBLOCATIONPROC Glext::GLGetAttribLocation = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC Glext::GLVertexAttribPointer = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC Glext::GLDisableVertexAttribArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC Glext::GLEnableVertexAttribArray = nullptr;

PFNGLBINDVERTEXARRAYPROC Glext::GLBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC Glext::GLDeleteVertexArrays = nullptr;
PFNGLGENVERTEXARRAYSPROC Glext::GLGenVertexArrays = nullptr;

PFNGLGETSHADERIVPROC Glext::GLGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC Glext::GLGetShaderInfoLog = nullptr;

PFNGLBINDBUFFERPROC Glext::GLBindBuffer = nullptr;
PFNGLDELETEBUFFERSPROC Glext::GLDeleteBuffers = nullptr;
PFNGLGENBUFFERSPROC Glext::GLGenBuffers = nullptr;
PFNGLISBUFFERPROC Glext::GLIsBuffer = nullptr;
PFNGLBUFFERDATAPROC Glext::GLBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC Glext::GLBufferSubData = nullptr;
PFNGLGETBUFFERSUBDATAPROC Glext::GLGetBufferSubData = nullptr;
PFNGLMAPBUFFERPROC Glext::GLMapBuffer = nullptr;
PFNGLMAPBUFFERRANGEPROC Glext::GLMapBufferRange = nullptr;
PFNGLUNMAPBUFFERPROC Glext::GLUnmapBuffer = nullptr;
PFNGLGETBUFFERPARAMETERIVPROC Glext::GLGetBufferParameteriv = nullptr;
PFNGLGETBUFFERPOINTERVPROC Glext::GLGetBufferPointerv = nullptr;

PFNGLGETUNIFORMBLOCKINDEXPROC Glext::GLGetUniformBlockIndex = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC Glext::GLUniformBlockBinding = nullptr;
PFNGLBINDBUFFERBASEPROC Glext::GLBindBufferBase = nullptr;

// OpenNeoUA custom: fullscreen visual filter LUT
PFNGLACTIVETEXTUREPROC Glext::GLActiveTexture = nullptr;

template<typename T>
inline void SetGLFunc(T &pFunc, const char *name)
{
    pFunc = (T)SDL_GL_GetProcAddress(name);

    if (!pFunc)
        throw std::runtime_error(std::string("Can't get GL extension address for ") + std::string(name));
}


bool Glext::init()
{
    int major = 0, minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);

    if (major < 2)
        return false;

    try {
    SetGLFunc(GLGenFramebuffers, "glGenFramebuffers");
    SetGLFunc(GLBindFramebuffer, "glBindFramebuffer");
    SetGLFunc(GLGenRenderbuffers, "glGenRenderbuffers");
    SetGLFunc(GLBindRenderbuffer, "glBindRenderbuffer");
    SetGLFunc(GLFrameBufferTexture, "glFramebufferTexture");
    SetGLFunc(GLFrameBufferTexture2D, "glFramebufferTexture2D");
    SetGLFunc(GLRenderbufferStorage, "glRenderbufferStorage");
    SetGLFunc(GLFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    SetGLFunc(GLCheckFramebufferStatus, "glCheckFramebufferStatus");
    SetGLFunc(GLGenerateMipmap, "glGenerateMipmap");
    SetGLFunc(GLCreateShader, "glCreateShader");
    SetGLFunc(GLShaderSource, "glShaderSource");
    SetGLFunc(GLCompileShader, "glCompileShader");
    SetGLFunc(GLDeleteShader, "glDeleteShader");
    SetGLFunc(GLCreateProgram, "glCreateProgram");
    SetGLFunc(GLDeleteProgram, "glDeleteProgram");
    SetGLFunc(GLAttachShader, "glAttachShader");
    SetGLFunc(GLLinkProgram, "glLinkProgram");
    SetGLFunc(GLGetProgramiv, "glGetProgramiv");
    SetGLFunc(GLGetProgramInfoLog, "glGetProgramInfoLog");
    SetGLFunc(GLUseProgram, "glUseProgram");
    SetGLFunc(GLGetUniformLocation, "glGetUniformLocation");

    SetGLFunc(GLUniform1f, "glUniform1f");
    SetGLFunc(GLUniform1i, "glUniform1i");
    SetGLFunc(GLUniform2f, "glUniform2f");
    SetGLFunc(GLUniform2i, "glUniform2i");
    SetGLFunc(GLUniform3f, "glUniform3f");
    SetGLFunc(GLUniform3i, "glUniform3i");
    SetGLFunc(GLUniform4f, "glUniform4f");
    SetGLFunc(GLUniform4i, "glUniform4i");

    SetGLFunc(GLUniformMatrix2fv, "glUniformMatrix2fv");
    SetGLFunc(GLUniformMatrix3fv, "glUniformMatrix3fv");
    SetGLFunc(GLUniformMatrix4fv, "glUniformMatrix4fv");

    SetGLFunc(GLGetAttribLocation, "glGetAttribLocation");
    SetGLFunc(GLVertexAttribPointer, "glVertexAttribPointer");
    SetGLFunc(GLDisableVertexAttribArray, "glDisableVertexAttribArray");
    SetGLFunc(GLEnableVertexAttribArray, "glEnableVertexAttribArray");

    SetGLFunc(GLBindVertexArray, "glBindVertexArray");
    SetGLFunc(GLDeleteVertexArrays, "glDeleteVertexArrays");
    SetGLFunc(GLGenVertexArrays, "glGenVertexArrays");

    SetGLFunc(GLGetShaderiv, "glGetShaderiv");
    SetGLFunc(GLGetShaderInfoLog, "glGetShaderInfoLog");


    SetGLFunc(GLBindBuffer, "glBindBuffer");
    SetGLFunc(GLDeleteBuffers, "glDeleteBuffers");
    SetGLFunc(GLGenBuffers, "glGenBuffers");
    SetGLFunc(GLIsBuffer, "glIsBuffer");
    SetGLFunc(GLBufferData, "glBufferData");
    SetGLFunc(GLBufferSubData, "glBufferSubData");
    SetGLFunc(GLGetBufferSubData, "glGetBufferSubData");
    SetGLFunc(GLMapBuffer, "glMapBuffer");
    SetGLFunc(GLMapBufferRange, "glMapBufferRange");
    SetGLFunc(GLUnmapBuffer, "glUnmapBuffer");
    SetGLFunc(GLGetBufferParameteriv, "glGetBufferParameteriv");
    SetGLFunc(GLGetBufferPointerv, "glGetBufferPointerv");

    SetGLFunc(GLGetUniformBlockIndex, "glGetUniformBlockIndex");
    SetGLFunc(GLUniformBlockBinding, "glUniformBlockBinding");
    SetGLFunc(GLBindBufferBase, "glBindBufferBase");

    SetGLFunc(GLActiveTexture, "glActiveTexture");
    }
    catch(const std::runtime_error &e)
    {
        return false;
    }

    return true;
}


}