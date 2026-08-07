/*********************************************************************************
*
* compute_emulator.c
*
* Direct3D 9 has no compute stage.  GLDirect therefore executes a compute-only
* program in a private software context, mirrors the wrapper's indexed UBO,
* SSBO and atomic-counter bindings into that context, waits for completion and
* copies every writable buffer back into the CPU shadow consumed by the D3D9
* translation path.  The application's WGL context and public GL dispatch stay
* owned by GLDirect; this worker never renders or presents a frame.
*
*********************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <glad/gl.h>

#include "compute_emulator.h"
#include "gl_impl.h"
#include "glsl_to_hlsl.h"
#include "gld_diag.h"

typedef int   (WINAPI *PFN_WGL_CHOOSE_PIXEL_FORMAT)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL  (WINAPI *PFN_WGL_SET_PIXEL_FORMAT)(HDC, int, const PIXELFORMATDESCRIPTOR *);
typedef HGLRC (WINAPI *PFN_WGL_CREATE_CONTEXT)(HDC);
typedef BOOL  (WINAPI *PFN_WGL_DELETE_CONTEXT)(HGLRC);
typedef BOOL  (WINAPI *PFN_WGL_MAKE_CURRENT)(HDC, HGLRC);
typedef PROC  (WINAPI *PFN_WGL_GET_PROC_ADDRESS)(LPCSTR);

typedef GLuint (APIENTRY *PFN_GL_CREATE_SHADER)(GLenum);
typedef void (APIENTRY *PFN_GL_SHADER_SOURCE)(GLuint, GLsizei, const GLchar *const*, const GLint *);
typedef void (APIENTRY *PFN_GL_COMPILE_SHADER)(GLuint);
typedef void (APIENTRY *PFN_GL_GET_SHADER_IV)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *PFN_GL_GET_SHADER_INFO_LOG)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void (APIENTRY *PFN_GL_DELETE_SHADER)(GLuint);
typedef GLuint (APIENTRY *PFN_GL_CREATE_PROGRAM)(void);
typedef void (APIENTRY *PFN_GL_ATTACH_SHADER)(GLuint, GLuint);
typedef void (APIENTRY *PFN_GL_BIND_ATTRIB_LOCATION)(GLuint, GLuint, const GLchar *);
typedef void (APIENTRY *PFN_GL_TRANSFORM_FEEDBACK_VARYINGS)(GLuint, GLsizei, const GLchar *const*, GLenum);
typedef void (APIENTRY *PFN_GL_LINK_PROGRAM)(GLuint);
typedef void (APIENTRY *PFN_GL_GET_PROGRAM_IV)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *PFN_GL_GET_PROGRAM_INFO_LOG)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void (APIENTRY *PFN_GL_DELETE_PROGRAM)(GLuint);
typedef void (APIENTRY *PFN_GL_USE_PROGRAM)(GLuint);
typedef GLint (APIENTRY *PFN_GL_GET_UNIFORM_LOCATION)(GLuint, const GLchar *);
typedef void (APIENTRY *PFN_GL_UNIFORM_1I)(GLint, GLint);
typedef void (APIENTRY *PFN_GL_UNIFORM_1F)(GLint, GLfloat);
typedef void (APIENTRY *PFN_GL_UNIFORM_2FV)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY *PFN_GL_UNIFORM_3FV)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY *PFN_GL_UNIFORM_4FV)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY *PFN_GL_UNIFORM_MATRIX_2FV)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY *PFN_GL_UNIFORM_MATRIX_3FV)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY *PFN_GL_UNIFORM_MATRIX_4FV)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY *PFN_GL_GEN_BUFFERS)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_BUFFERS)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_BIND_BUFFER)(GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_BUFFER_DATA)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (APIENTRY *PFN_GL_BIND_BUFFER_BASE)(GLenum, GLuint, GLuint);
typedef void (APIENTRY *PFN_GL_BIND_BUFFER_RANGE)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);
typedef void (APIENTRY *PFN_GL_GET_BUFFER_SUB_DATA)(GLenum, GLintptr, GLsizeiptr, void *);
typedef void (APIENTRY *PFN_GL_GEN_TEXTURES)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_TEXTURES)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_ACTIVE_TEXTURE)(GLenum);
typedef void (APIENTRY *PFN_GL_BIND_TEXTURE)(GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_TEX_IMAGE_1D)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *);
typedef void (APIENTRY *PFN_GL_TEX_IMAGE_2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
typedef void (APIENTRY *PFN_GL_TEX_IMAGE_3D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
typedef void (APIENTRY *PFN_GL_TEX_BUFFER)(GLenum, GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_TEX_BUFFER_RANGE)(GLenum, GLenum, GLuint, GLintptr, GLsizeiptr);
typedef void (APIENTRY *PFN_GL_TEX_PARAMETER_I)(GLenum, GLenum, GLint);
typedef void (APIENTRY *PFN_GL_BIND_IMAGE_TEXTURE)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
typedef void (APIENTRY *PFN_GL_GET_TEX_IMAGE)(GLenum, GLint, GLenum, GLenum, void *);
typedef void (APIENTRY *PFN_GL_PIXEL_STORE_I)(GLenum, GLint);
typedef void (APIENTRY *PFN_GL_GEN_SAMPLERS)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_SAMPLERS)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_BIND_SAMPLER)(GLuint, GLuint);
typedef void (APIENTRY *PFN_GL_SAMPLER_PARAMETER_I)(GLuint, GLenum, GLint);
typedef void (APIENTRY *PFN_GL_SAMPLER_PARAMETER_F)(GLuint, GLenum, GLfloat);
typedef void (APIENTRY *PFN_GL_SAMPLER_PARAMETER_FV)(GLuint, GLenum, const GLfloat *);
typedef void (APIENTRY *PFN_GL_GEN_VERTEX_ARRAYS)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_VERTEX_ARRAYS)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_BIND_VERTEX_ARRAY)(GLuint);
typedef void (APIENTRY *PFN_GL_ENABLE_VERTEX_ATTRIB_ARRAY)(GLuint);
typedef void (APIENTRY *PFN_GL_DISABLE_VERTEX_ATTRIB_ARRAY)(GLuint);
typedef void (APIENTRY *PFN_GL_VERTEX_ATTRIB_POINTER)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void (APIENTRY *PFN_GL_VERTEX_ATTRIB_I_POINTER)(GLuint, GLint, GLenum, GLsizei, const void *);
typedef void (APIENTRY *PFN_GL_VERTEX_ATTRIB_DIVISOR)(GLuint, GLuint);
typedef void (APIENTRY *PFN_GL_VERTEX_ATTRIB_4FV)(GLuint, const GLfloat *);
typedef void (APIENTRY *PFN_GL_PATCH_PARAMETER_I)(GLenum, GLint);
typedef void (APIENTRY *PFN_GL_PATCH_PARAMETER_FV)(GLenum, const GLfloat *);
typedef void (APIENTRY *PFN_GL_DRAW_ARRAYS)(GLenum, GLint, GLsizei);
typedef void (APIENTRY *PFN_GL_DRAW_ELEMENTS_BASE_VERTEX)(GLenum, GLsizei, GLenum, const void *, GLint);
typedef void (APIENTRY *PFN_GL_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE)(GLenum, GLint, GLsizei, GLsizei, GLuint);
typedef void (APIENTRY *PFN_GL_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE)(GLenum, GLsizei, GLenum, const void *, GLsizei, GLint, GLuint);
typedef void (APIENTRY *PFN_GL_BEGIN_TRANSFORM_FEEDBACK)(GLenum);
typedef void (APIENTRY *PFN_GL_END_TRANSFORM_FEEDBACK)(void);
typedef void (APIENTRY *PFN_GL_ENABLE)(GLenum);
typedef void (APIENTRY *PFN_GL_DISABLE)(GLenum);
typedef void (APIENTRY *PFN_GL_GET_TRANSFORM_FEEDBACK_VARYING)(GLuint, GLuint, GLsizei, GLsizei *, GLsizei *, GLenum *, GLchar *);
typedef void (APIENTRY *PFN_GL_DISPATCH_COMPUTE)(GLuint, GLuint, GLuint);
typedef void (APIENTRY *PFN_GL_MEMORY_BARRIER)(GLbitfield);
typedef void (APIENTRY *PFN_GL_FINISH)(void);
typedef GLenum (APIENTRY *PFN_GL_GET_ERROR)(void);
typedef void (APIENTRY *PFN_GL_GEN_FRAMEBUFFERS)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_FRAMEBUFFERS)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_BIND_FRAMEBUFFER)(GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_FRAMEBUFFER_TEXTURE_2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY *PFN_GL_CHECK_FRAMEBUFFER_STATUS)(GLenum);
typedef void (APIENTRY *PFN_GL_GEN_RENDERBUFFERS)(GLsizei, GLuint *);
typedef void (APIENTRY *PFN_GL_DELETE_RENDERBUFFERS)(GLsizei, const GLuint *);
typedef void (APIENTRY *PFN_GL_BIND_RENDERBUFFER)(GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_RENDERBUFFER_STORAGE)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY *PFN_GL_FRAMEBUFFER_RENDERBUFFER)(GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_DRAW_BUFFERS)(GLsizei, const GLenum *);
typedef void (APIENTRY *PFN_GL_READ_BUFFER)(GLenum);
typedef void (APIENTRY *PFN_GL_READ_PIXELS)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef void (APIENTRY *PFN_GL_VIEWPORT)(GLint, GLint, GLsizei, GLsizei);
typedef void (APIENTRY *PFN_GL_SCISSOR)(GLint, GLint, GLsizei, GLsizei);
typedef void (APIENTRY *PFN_GL_COLOR_MASK)(GLboolean, GLboolean, GLboolean, GLboolean);
typedef void (APIENTRY *PFN_GL_BLEND_FUNC_SEPARATE)(GLenum, GLenum, GLenum, GLenum);
typedef void (APIENTRY *PFN_GL_BLEND_EQUATION_SEPARATE)(GLenum, GLenum);
typedef void (APIENTRY *PFN_GL_BLEND_COLOR)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY *PFN_GL_DEPTH_FUNC)(GLenum);
typedef void (APIENTRY *PFN_GL_DEPTH_MASK)(GLboolean);
typedef void (APIENTRY *PFN_GL_DEPTH_RANGE)(GLdouble, GLdouble);
typedef void (APIENTRY *PFN_GL_CULL_FACE)(GLenum);
typedef void (APIENTRY *PFN_GL_FRONT_FACE)(GLenum);
typedef void (APIENTRY *PFN_GL_STENCIL_FUNC_SEPARATE)(GLenum, GLenum, GLint, GLuint);
typedef void (APIENTRY *PFN_GL_STENCIL_OP_SEPARATE)(GLenum, GLenum, GLenum, GLenum);
typedef void (APIENTRY *PFN_GL_STENCIL_MASK_SEPARATE)(GLenum, GLuint);
typedef void (APIENTRY *PFN_GL_POLYGON_OFFSET)(GLfloat, GLfloat);
typedef void (APIENTRY *PFN_GL_LINE_WIDTH)(GLfloat);
typedef void (APIENTRY *PFN_GL_CLIP_CONTROL)(GLenum, GLenum);
typedef void (APIENTRY *PFN_GL_CLEAR_DEPTH)(GLdouble);
typedef void (APIENTRY *PFN_GL_CLEAR_STENCIL)(GLint);
typedef void (APIENTRY *PFN_GL_CLEAR)(GLbitfield);

typedef struct {
    HMODULE module;
    HWND window;
    HDC dc;
    HGLRC context;
    ATOM windowClass;
    CRITICAL_SECTION lock;
    BOOL lockReady;
    BOOL initialized;
    BOOL attempted;

    PFN_WGL_CHOOSE_PIXEL_FORMAT wglChoosePixelFormat;
    PFN_WGL_SET_PIXEL_FORMAT wglSetPixelFormat;
    PFN_WGL_CREATE_CONTEXT wglCreateContext;
    PFN_WGL_DELETE_CONTEXT wglDeleteContext;
    PFN_WGL_MAKE_CURRENT wglMakeCurrent;
    PFN_WGL_GET_PROC_ADDRESS wglGetProcAddress;

    PFN_GL_CREATE_SHADER CreateShader;
    PFN_GL_SHADER_SOURCE ShaderSource;
    PFN_GL_COMPILE_SHADER CompileShader;
    PFN_GL_GET_SHADER_IV GetShaderiv;
    PFN_GL_GET_SHADER_INFO_LOG GetShaderInfoLog;
    PFN_GL_DELETE_SHADER DeleteShader;
    PFN_GL_CREATE_PROGRAM CreateProgram;
    PFN_GL_ATTACH_SHADER AttachShader;
    PFN_GL_BIND_ATTRIB_LOCATION BindAttribLocation;
    PFN_GL_TRANSFORM_FEEDBACK_VARYINGS TransformFeedbackVaryings;
    PFN_GL_LINK_PROGRAM LinkProgram;
    PFN_GL_GET_PROGRAM_IV GetProgramiv;
    PFN_GL_GET_PROGRAM_INFO_LOG GetProgramInfoLog;
    PFN_GL_DELETE_PROGRAM DeleteProgram;
    PFN_GL_USE_PROGRAM UseProgram;
    PFN_GL_GET_UNIFORM_LOCATION GetUniformLocation;
    PFN_GL_UNIFORM_1I Uniform1i;
    PFN_GL_UNIFORM_1F Uniform1f;
    PFN_GL_UNIFORM_2FV Uniform2fv;
    PFN_GL_UNIFORM_3FV Uniform3fv;
    PFN_GL_UNIFORM_4FV Uniform4fv;
    PFN_GL_UNIFORM_MATRIX_2FV UniformMatrix2fv;
    PFN_GL_UNIFORM_MATRIX_3FV UniformMatrix3fv;
    PFN_GL_UNIFORM_MATRIX_4FV UniformMatrix4fv;
    PFN_GL_GEN_BUFFERS GenBuffers;
    PFN_GL_DELETE_BUFFERS DeleteBuffers;
    PFN_GL_BIND_BUFFER BindBuffer;
    PFN_GL_BUFFER_DATA BufferData;
    PFN_GL_BIND_BUFFER_BASE BindBufferBase;
    PFN_GL_BIND_BUFFER_RANGE BindBufferRange;
    PFN_GL_GET_BUFFER_SUB_DATA GetBufferSubData;
    PFN_GL_GEN_TEXTURES GenTextures;
    PFN_GL_DELETE_TEXTURES DeleteTextures;
    PFN_GL_ACTIVE_TEXTURE ActiveTexture;
    PFN_GL_BIND_TEXTURE BindTexture;
    PFN_GL_TEX_IMAGE_1D TexImage1D;
    PFN_GL_TEX_IMAGE_2D TexImage2D;
    PFN_GL_TEX_IMAGE_3D TexImage3D;
    PFN_GL_TEX_BUFFER TexBuffer;
    PFN_GL_TEX_BUFFER_RANGE TexBufferRange;
    PFN_GL_TEX_PARAMETER_I TexParameteri;
    PFN_GL_BIND_IMAGE_TEXTURE BindImageTexture;
    PFN_GL_GET_TEX_IMAGE GetTexImage;
    PFN_GL_PIXEL_STORE_I PixelStorei;
    PFN_GL_GEN_SAMPLERS GenSamplers;
    PFN_GL_DELETE_SAMPLERS DeleteSamplers;
    PFN_GL_BIND_SAMPLER BindSampler;
    PFN_GL_SAMPLER_PARAMETER_I SamplerParameteri;
    PFN_GL_SAMPLER_PARAMETER_F SamplerParameterf;
    PFN_GL_SAMPLER_PARAMETER_FV SamplerParameterfv;
    PFN_GL_GEN_VERTEX_ARRAYS GenVertexArrays;
    PFN_GL_DELETE_VERTEX_ARRAYS DeleteVertexArrays;
    PFN_GL_BIND_VERTEX_ARRAY BindVertexArray;
    PFN_GL_ENABLE_VERTEX_ATTRIB_ARRAY EnableVertexAttribArray;
    PFN_GL_DISABLE_VERTEX_ATTRIB_ARRAY DisableVertexAttribArray;
    PFN_GL_VERTEX_ATTRIB_POINTER VertexAttribPointer;
    PFN_GL_VERTEX_ATTRIB_I_POINTER VertexAttribIPointer;
    PFN_GL_VERTEX_ATTRIB_DIVISOR VertexAttribDivisor;
    PFN_GL_VERTEX_ATTRIB_4FV VertexAttrib4fv;
    PFN_GL_PATCH_PARAMETER_I PatchParameteri;
    PFN_GL_PATCH_PARAMETER_FV PatchParameterfv;
    PFN_GL_DRAW_ARRAYS DrawArrays;
    PFN_GL_DRAW_ELEMENTS_BASE_VERTEX DrawElementsBaseVertex;
    PFN_GL_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE DrawArraysInstancedBaseInstance;
    PFN_GL_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE DrawElementsInstancedBaseVertexBaseInstance;
    PFN_GL_BEGIN_TRANSFORM_FEEDBACK BeginTransformFeedback;
    PFN_GL_END_TRANSFORM_FEEDBACK EndTransformFeedback;
    PFN_GL_ENABLE Enable;
    PFN_GL_DISABLE Disable;
    PFN_GL_GET_TRANSFORM_FEEDBACK_VARYING GetTransformFeedbackVarying;
    PFN_GL_DISPATCH_COMPUTE DispatchCompute;
    PFN_GL_MEMORY_BARRIER MemoryBarrier;
    PFN_GL_FINISH Finish;
    PFN_GL_GET_ERROR GetError;
    PFN_GL_GEN_FRAMEBUFFERS GenFramebuffers;
    PFN_GL_DELETE_FRAMEBUFFERS DeleteFramebuffers;
    PFN_GL_BIND_FRAMEBUFFER BindFramebuffer;
    PFN_GL_FRAMEBUFFER_TEXTURE_2D FramebufferTexture2D;
    PFN_GL_CHECK_FRAMEBUFFER_STATUS CheckFramebufferStatus;
    PFN_GL_GEN_RENDERBUFFERS GenRenderbuffers;
    PFN_GL_DELETE_RENDERBUFFERS DeleteRenderbuffers;
    PFN_GL_BIND_RENDERBUFFER BindRenderbuffer;
    PFN_GL_RENDERBUFFER_STORAGE RenderbufferStorage;
    PFN_GL_FRAMEBUFFER_RENDERBUFFER FramebufferRenderbuffer;
    PFN_GL_DRAW_BUFFERS DrawBuffers;
    PFN_GL_READ_BUFFER ReadBuffer;
    PFN_GL_READ_PIXELS ReadPixels;
    PFN_GL_VIEWPORT Viewport;
    PFN_GL_SCISSOR Scissor;
    PFN_GL_COLOR_MASK ColorMask;
    PFN_GL_BLEND_FUNC_SEPARATE BlendFuncSeparate;
    PFN_GL_BLEND_EQUATION_SEPARATE BlendEquationSeparate;
    PFN_GL_BLEND_COLOR BlendColor;
    PFN_GL_DEPTH_FUNC DepthFunc;
    PFN_GL_DEPTH_MASK DepthMask;
    PFN_GL_DEPTH_RANGE DepthRange;
    PFN_GL_CULL_FACE CullFace;
    PFN_GL_FRONT_FACE FrontFace;
    PFN_GL_STENCIL_FUNC_SEPARATE StencilFuncSeparate;
    PFN_GL_STENCIL_OP_SEPARATE StencilOpSeparate;
    PFN_GL_STENCIL_MASK_SEPARATE StencilMaskSeparate;
    PFN_GL_POLYGON_OFFSET PolygonOffset;
    PFN_GL_LINE_WIDTH LineWidth;
    PFN_GL_CLIP_CONTROL ClipControl;
    PFN_GL_CLEAR_DEPTH ClearDepth;
    PFN_GL_CLEAR_STENCIL ClearStencil;
    PFN_GL_CLEAR Clear;

    GLuint programs[GLS_MAX_PROGRAMS];
    GLuint graphicsPrograms[GLS_MAX_PROGRAMS];
    GLuint buffers[GLS_MAX_BUFFERS];
    GLuint textures[GLS_MAX_TEXTURES];
    GLuint samplers[GLS_MAX_SAMPLERS];
    GLuint stageVao;
    GLuint stageCaptureBuffer;
    GLuint fragmentFbo;
    GLuint fragmentColor;
    GLuint fragmentDepthStencil;
    int fragmentWidth;
    int fragmentHeight;
} ComputeWorker;

static ComputeWorker g_worker;

static LRESULT CALLBACK computeWindowProc(HWND window, UINT message,
                                           WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(window, message, wParam, lParam);
}

static void computeLog(char *dst, int dstSize, const char *message)
{
    if (!dst || dstSize <= 0) return;
    if (!message) message = "";
    strncpy(dst, message, (size_t)dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static PROC computeResolve(const char *name)
{
    /* Prefer the renamed Mesa module's own core exports.  Asking WGL first is
     * unsafe in an opengl32 wrapper process: Mesa may resolve a GL 1.1 name
     * through the process-wide opengl32 import and return this wrapper's public
     * thunk.  The private context would then call back into GLDirect state.
     * Extensions are not module exports, so they retain the WGL fallback. */
    PROC p = (PROC)GetProcAddress(g_worker.module, name);
    if (!p && g_worker.wglGetProcAddress)
        p = g_worker.wglGetProcAddress(name);
    if (!p || p == (PROC)1 || p == (PROC)2 || p == (PROC)3 || p == (PROC)-1)
        p = (PROC)GetProcAddress(g_worker.module, name);
    return p;
}

#define RESOLVE_REQUIRED(field, type, name) do { \
    g_worker.field = (type)computeResolve(name); \
    if (!g_worker.field) { gldDiagLog("compute emulator: missing %s", name); goto fail; } \
} while (0)

static BOOL computeLoadFunctions(void)
{
    RESOLVE_REQUIRED(CreateShader, PFN_GL_CREATE_SHADER, "glCreateShader");
    RESOLVE_REQUIRED(ShaderSource, PFN_GL_SHADER_SOURCE, "glShaderSource");
    RESOLVE_REQUIRED(CompileShader, PFN_GL_COMPILE_SHADER, "glCompileShader");
    RESOLVE_REQUIRED(GetShaderiv, PFN_GL_GET_SHADER_IV, "glGetShaderiv");
    RESOLVE_REQUIRED(GetShaderInfoLog, PFN_GL_GET_SHADER_INFO_LOG, "glGetShaderInfoLog");
    RESOLVE_REQUIRED(DeleteShader, PFN_GL_DELETE_SHADER, "glDeleteShader");
    RESOLVE_REQUIRED(CreateProgram, PFN_GL_CREATE_PROGRAM, "glCreateProgram");
    RESOLVE_REQUIRED(AttachShader, PFN_GL_ATTACH_SHADER, "glAttachShader");
    RESOLVE_REQUIRED(BindAttribLocation, PFN_GL_BIND_ATTRIB_LOCATION, "glBindAttribLocation");
    RESOLVE_REQUIRED(TransformFeedbackVaryings, PFN_GL_TRANSFORM_FEEDBACK_VARYINGS, "glTransformFeedbackVaryings");
    RESOLVE_REQUIRED(LinkProgram, PFN_GL_LINK_PROGRAM, "glLinkProgram");
    RESOLVE_REQUIRED(GetProgramiv, PFN_GL_GET_PROGRAM_IV, "glGetProgramiv");
    RESOLVE_REQUIRED(GetProgramInfoLog, PFN_GL_GET_PROGRAM_INFO_LOG, "glGetProgramInfoLog");
    RESOLVE_REQUIRED(DeleteProgram, PFN_GL_DELETE_PROGRAM, "glDeleteProgram");
    RESOLVE_REQUIRED(UseProgram, PFN_GL_USE_PROGRAM, "glUseProgram");
    RESOLVE_REQUIRED(GetUniformLocation, PFN_GL_GET_UNIFORM_LOCATION, "glGetUniformLocation");
    RESOLVE_REQUIRED(Uniform1i, PFN_GL_UNIFORM_1I, "glUniform1i");
    RESOLVE_REQUIRED(Uniform1f, PFN_GL_UNIFORM_1F, "glUniform1f");
    RESOLVE_REQUIRED(Uniform2fv, PFN_GL_UNIFORM_2FV, "glUniform2fv");
    RESOLVE_REQUIRED(Uniform3fv, PFN_GL_UNIFORM_3FV, "glUniform3fv");
    RESOLVE_REQUIRED(Uniform4fv, PFN_GL_UNIFORM_4FV, "glUniform4fv");
    RESOLVE_REQUIRED(UniformMatrix2fv, PFN_GL_UNIFORM_MATRIX_2FV, "glUniformMatrix2fv");
    RESOLVE_REQUIRED(UniformMatrix3fv, PFN_GL_UNIFORM_MATRIX_3FV, "glUniformMatrix3fv");
    RESOLVE_REQUIRED(UniformMatrix4fv, PFN_GL_UNIFORM_MATRIX_4FV, "glUniformMatrix4fv");
    RESOLVE_REQUIRED(GenBuffers, PFN_GL_GEN_BUFFERS, "glGenBuffers");
    RESOLVE_REQUIRED(DeleteBuffers, PFN_GL_DELETE_BUFFERS, "glDeleteBuffers");
    RESOLVE_REQUIRED(BindBuffer, PFN_GL_BIND_BUFFER, "glBindBuffer");
    RESOLVE_REQUIRED(BufferData, PFN_GL_BUFFER_DATA, "glBufferData");
    RESOLVE_REQUIRED(BindBufferBase, PFN_GL_BIND_BUFFER_BASE, "glBindBufferBase");
    RESOLVE_REQUIRED(BindBufferRange, PFN_GL_BIND_BUFFER_RANGE, "glBindBufferRange");
    RESOLVE_REQUIRED(GetBufferSubData, PFN_GL_GET_BUFFER_SUB_DATA, "glGetBufferSubData");
    RESOLVE_REQUIRED(GenTextures, PFN_GL_GEN_TEXTURES, "glGenTextures");
    RESOLVE_REQUIRED(DeleteTextures, PFN_GL_DELETE_TEXTURES, "glDeleteTextures");
    RESOLVE_REQUIRED(ActiveTexture, PFN_GL_ACTIVE_TEXTURE, "glActiveTexture");
    RESOLVE_REQUIRED(BindTexture, PFN_GL_BIND_TEXTURE, "glBindTexture");
    RESOLVE_REQUIRED(TexImage1D, PFN_GL_TEX_IMAGE_1D, "glTexImage1D");
    RESOLVE_REQUIRED(TexImage2D, PFN_GL_TEX_IMAGE_2D, "glTexImage2D");
    RESOLVE_REQUIRED(TexImage3D, PFN_GL_TEX_IMAGE_3D, "glTexImage3D");
    RESOLVE_REQUIRED(TexBuffer, PFN_GL_TEX_BUFFER, "glTexBuffer");
    RESOLVE_REQUIRED(TexBufferRange, PFN_GL_TEX_BUFFER_RANGE, "glTexBufferRange");
    RESOLVE_REQUIRED(TexParameteri, PFN_GL_TEX_PARAMETER_I, "glTexParameteri");
    RESOLVE_REQUIRED(BindImageTexture, PFN_GL_BIND_IMAGE_TEXTURE, "glBindImageTexture");
    RESOLVE_REQUIRED(GetTexImage, PFN_GL_GET_TEX_IMAGE, "glGetTexImage");
    RESOLVE_REQUIRED(PixelStorei, PFN_GL_PIXEL_STORE_I, "glPixelStorei");
    RESOLVE_REQUIRED(GenSamplers, PFN_GL_GEN_SAMPLERS, "glGenSamplers");
    RESOLVE_REQUIRED(DeleteSamplers, PFN_GL_DELETE_SAMPLERS, "glDeleteSamplers");
    RESOLVE_REQUIRED(BindSampler, PFN_GL_BIND_SAMPLER, "glBindSampler");
    RESOLVE_REQUIRED(SamplerParameteri, PFN_GL_SAMPLER_PARAMETER_I, "glSamplerParameteri");
    RESOLVE_REQUIRED(SamplerParameterf, PFN_GL_SAMPLER_PARAMETER_F, "glSamplerParameterf");
    RESOLVE_REQUIRED(SamplerParameterfv, PFN_GL_SAMPLER_PARAMETER_FV, "glSamplerParameterfv");
    RESOLVE_REQUIRED(GenVertexArrays, PFN_GL_GEN_VERTEX_ARRAYS, "glGenVertexArrays");
    RESOLVE_REQUIRED(DeleteVertexArrays, PFN_GL_DELETE_VERTEX_ARRAYS, "glDeleteVertexArrays");
    RESOLVE_REQUIRED(BindVertexArray, PFN_GL_BIND_VERTEX_ARRAY, "glBindVertexArray");
    RESOLVE_REQUIRED(EnableVertexAttribArray, PFN_GL_ENABLE_VERTEX_ATTRIB_ARRAY, "glEnableVertexAttribArray");
    RESOLVE_REQUIRED(DisableVertexAttribArray, PFN_GL_DISABLE_VERTEX_ATTRIB_ARRAY, "glDisableVertexAttribArray");
    RESOLVE_REQUIRED(VertexAttribPointer, PFN_GL_VERTEX_ATTRIB_POINTER, "glVertexAttribPointer");
    RESOLVE_REQUIRED(VertexAttribIPointer, PFN_GL_VERTEX_ATTRIB_I_POINTER, "glVertexAttribIPointer");
    RESOLVE_REQUIRED(VertexAttribDivisor, PFN_GL_VERTEX_ATTRIB_DIVISOR, "glVertexAttribDivisor");
    RESOLVE_REQUIRED(VertexAttrib4fv, PFN_GL_VERTEX_ATTRIB_4FV, "glVertexAttrib4fv");
    RESOLVE_REQUIRED(PatchParameteri, PFN_GL_PATCH_PARAMETER_I, "glPatchParameteri");
    RESOLVE_REQUIRED(PatchParameterfv, PFN_GL_PATCH_PARAMETER_FV, "glPatchParameterfv");
    RESOLVE_REQUIRED(DrawArrays, PFN_GL_DRAW_ARRAYS, "glDrawArrays");
    RESOLVE_REQUIRED(DrawElementsBaseVertex, PFN_GL_DRAW_ELEMENTS_BASE_VERTEX, "glDrawElementsBaseVertex");
    RESOLVE_REQUIRED(DrawArraysInstancedBaseInstance, PFN_GL_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE, "glDrawArraysInstancedBaseInstance");
    RESOLVE_REQUIRED(DrawElementsInstancedBaseVertexBaseInstance, PFN_GL_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE, "glDrawElementsInstancedBaseVertexBaseInstance");
    RESOLVE_REQUIRED(BeginTransformFeedback, PFN_GL_BEGIN_TRANSFORM_FEEDBACK, "glBeginTransformFeedback");
    RESOLVE_REQUIRED(EndTransformFeedback, PFN_GL_END_TRANSFORM_FEEDBACK, "glEndTransformFeedback");
    RESOLVE_REQUIRED(Enable, PFN_GL_ENABLE, "glEnable");
    RESOLVE_REQUIRED(Disable, PFN_GL_DISABLE, "glDisable");
    RESOLVE_REQUIRED(GetTransformFeedbackVarying, PFN_GL_GET_TRANSFORM_FEEDBACK_VARYING, "glGetTransformFeedbackVarying");
    RESOLVE_REQUIRED(DispatchCompute, PFN_GL_DISPATCH_COMPUTE, "glDispatchCompute");
    RESOLVE_REQUIRED(MemoryBarrier, PFN_GL_MEMORY_BARRIER, "glMemoryBarrier");
    RESOLVE_REQUIRED(Finish, PFN_GL_FINISH, "glFinish");
    RESOLVE_REQUIRED(GetError, PFN_GL_GET_ERROR, "glGetError");
    RESOLVE_REQUIRED(GenFramebuffers, PFN_GL_GEN_FRAMEBUFFERS, "glGenFramebuffers");
    RESOLVE_REQUIRED(DeleteFramebuffers, PFN_GL_DELETE_FRAMEBUFFERS, "glDeleteFramebuffers");
    RESOLVE_REQUIRED(BindFramebuffer, PFN_GL_BIND_FRAMEBUFFER, "glBindFramebuffer");
    RESOLVE_REQUIRED(FramebufferTexture2D, PFN_GL_FRAMEBUFFER_TEXTURE_2D, "glFramebufferTexture2D");
    RESOLVE_REQUIRED(CheckFramebufferStatus, PFN_GL_CHECK_FRAMEBUFFER_STATUS, "glCheckFramebufferStatus");
    RESOLVE_REQUIRED(GenRenderbuffers, PFN_GL_GEN_RENDERBUFFERS, "glGenRenderbuffers");
    RESOLVE_REQUIRED(DeleteRenderbuffers, PFN_GL_DELETE_RENDERBUFFERS, "glDeleteRenderbuffers");
    RESOLVE_REQUIRED(BindRenderbuffer, PFN_GL_BIND_RENDERBUFFER, "glBindRenderbuffer");
    RESOLVE_REQUIRED(RenderbufferStorage, PFN_GL_RENDERBUFFER_STORAGE, "glRenderbufferStorage");
    RESOLVE_REQUIRED(FramebufferRenderbuffer, PFN_GL_FRAMEBUFFER_RENDERBUFFER, "glFramebufferRenderbuffer");
    RESOLVE_REQUIRED(DrawBuffers, PFN_GL_DRAW_BUFFERS, "glDrawBuffers");
    RESOLVE_REQUIRED(ReadBuffer, PFN_GL_READ_BUFFER, "glReadBuffer");
    RESOLVE_REQUIRED(ReadPixels, PFN_GL_READ_PIXELS, "glReadPixels");
    RESOLVE_REQUIRED(Viewport, PFN_GL_VIEWPORT, "glViewport");
    RESOLVE_REQUIRED(Scissor, PFN_GL_SCISSOR, "glScissor");
    RESOLVE_REQUIRED(ColorMask, PFN_GL_COLOR_MASK, "glColorMask");
    RESOLVE_REQUIRED(BlendFuncSeparate, PFN_GL_BLEND_FUNC_SEPARATE, "glBlendFuncSeparate");
    RESOLVE_REQUIRED(BlendEquationSeparate, PFN_GL_BLEND_EQUATION_SEPARATE, "glBlendEquationSeparate");
    RESOLVE_REQUIRED(BlendColor, PFN_GL_BLEND_COLOR, "glBlendColor");
    RESOLVE_REQUIRED(DepthFunc, PFN_GL_DEPTH_FUNC, "glDepthFunc");
    RESOLVE_REQUIRED(DepthMask, PFN_GL_DEPTH_MASK, "glDepthMask");
    RESOLVE_REQUIRED(DepthRange, PFN_GL_DEPTH_RANGE, "glDepthRange");
    RESOLVE_REQUIRED(CullFace, PFN_GL_CULL_FACE, "glCullFace");
    RESOLVE_REQUIRED(FrontFace, PFN_GL_FRONT_FACE, "glFrontFace");
    RESOLVE_REQUIRED(StencilFuncSeparate, PFN_GL_STENCIL_FUNC_SEPARATE, "glStencilFuncSeparate");
    RESOLVE_REQUIRED(StencilOpSeparate, PFN_GL_STENCIL_OP_SEPARATE, "glStencilOpSeparate");
    RESOLVE_REQUIRED(StencilMaskSeparate, PFN_GL_STENCIL_MASK_SEPARATE, "glStencilMaskSeparate");
    RESOLVE_REQUIRED(PolygonOffset, PFN_GL_POLYGON_OFFSET, "glPolygonOffset");
    RESOLVE_REQUIRED(LineWidth, PFN_GL_LINE_WIDTH, "glLineWidth");
    RESOLVE_REQUIRED(ClipControl, PFN_GL_CLIP_CONTROL, "glClipControl");
    RESOLVE_REQUIRED(ClearDepth, PFN_GL_CLEAR_DEPTH, "glClearDepth");
    RESOLVE_REQUIRED(ClearStencil, PFN_GL_CLEAR_STENCIL, "glClearStencil");
    RESOLVE_REQUIRED(Clear, PFN_GL_CLEAR, "glClear");
    return TRUE;
fail:
    return FALSE;
}

#undef RESOLVE_REQUIRED

static BOOL computeInitialize(void)
{
    char path[MAX_PATH];
    char *slash;
    HMODULE self = NULL;
    WNDCLASSEXA wc;
    PIXELFORMATDESCRIPTOR pfd;
    int format;

    if (g_worker.initialized) return TRUE;
    if (g_worker.attempted) return FALSE;
    g_worker.attempted = TRUE;

    if (!GetEnvironmentVariableA("MESA_GL_VERSION_OVERRIDE", path, sizeof(path)))
        SetEnvironmentVariableA("MESA_GL_VERSION_OVERRIDE", "4.6");
    if (!GetEnvironmentVariableA("MESA_GLSL_VERSION_OVERRIDE", path, sizeof(path)))
        SetEnvironmentVariableA("MESA_GLSL_VERSION_OVERRIDE", "460");

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&computeInitialize, &self) ||
        !GetModuleFileNameA(self, path, sizeof(path)))
        return FALSE;
    slash = strrchr(path, '\\');
    if (!slash) return FALSE;
    strcpy(slash + 1, "mesa_gl.dll");
    g_worker.module = LoadLibraryA(path);
    if (!g_worker.module) {
        gldDiagLog("compute emulator: unable to load %s (error %lu)",
                   path, (unsigned long)GetLastError());
        return FALSE;
    }

    g_worker.wglChoosePixelFormat = (PFN_WGL_CHOOSE_PIXEL_FORMAT)GetProcAddress(g_worker.module, "wglChoosePixelFormat");
    g_worker.wglSetPixelFormat = (PFN_WGL_SET_PIXEL_FORMAT)GetProcAddress(g_worker.module, "wglSetPixelFormat");
    g_worker.wglCreateContext = (PFN_WGL_CREATE_CONTEXT)GetProcAddress(g_worker.module, "wglCreateContext");
    g_worker.wglDeleteContext = (PFN_WGL_DELETE_CONTEXT)GetProcAddress(g_worker.module, "wglDeleteContext");
    g_worker.wglMakeCurrent = (PFN_WGL_MAKE_CURRENT)GetProcAddress(g_worker.module, "wglMakeCurrent");
    g_worker.wglGetProcAddress = (PFN_WGL_GET_PROC_ADDRESS)GetProcAddress(g_worker.module, "wglGetProcAddress");
    if (!g_worker.wglChoosePixelFormat || !g_worker.wglSetPixelFormat ||
        !g_worker.wglCreateContext || !g_worker.wglDeleteContext ||
        !g_worker.wglMakeCurrent || !g_worker.wglGetProcAddress)
        goto fail;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = computeWindowProc;
    wc.hInstance = self;
    wc.lpszClassName = "GLDirectComputeEmulator";
    g_worker.windowClass = RegisterClassExA(&wc);
    if (!g_worker.windowClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        goto fail;
    g_worker.window = CreateWindowExA(0, wc.lpszClassName, "", WS_POPUP,
                                      0, 0, 1, 1, NULL, NULL, self, NULL);
    if (!g_worker.window) goto fail;
    g_worker.dc = GetDC(g_worker.window);
    if (!g_worker.dc) goto fail;

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    format = g_worker.wglChoosePixelFormat(g_worker.dc, &pfd);
    if (!format || !g_worker.wglSetPixelFormat(g_worker.dc, format, &pfd)) goto fail;
    g_worker.context = g_worker.wglCreateContext(g_worker.dc);
    if (!g_worker.context || !g_worker.wglMakeCurrent(g_worker.dc, g_worker.context)) goto fail;
    if (!computeLoadFunctions()) goto failCurrent;
    g_worker.wglMakeCurrent(NULL, NULL);
    InitializeCriticalSection(&g_worker.lock);
    g_worker.lockReady = TRUE;
    g_worker.initialized = TRUE;
    gldDiagLog("compute emulator: private Mesa 4.6 worker ready");
    return TRUE;

failCurrent:
    g_worker.wglMakeCurrent(NULL, NULL);
fail:
    gldComputeEmulatorShutdown();
    g_worker.attempted = TRUE;
    return FALSE;
}

static BOOL computeMakeCurrent(void)
{
    if (!computeInitialize()) return FALSE;
    EnterCriticalSection(&g_worker.lock);
    if (!g_worker.wglMakeCurrent(g_worker.dc, g_worker.context)) {
        LeaveCriticalSection(&g_worker.lock);
        return FALSE;
    }
    return TRUE;
}

static void computeReleaseCurrent(void)
{
    g_worker.wglMakeCurrent(NULL, NULL);
    LeaveCriticalSection(&g_worker.lock);
}

BOOL gldComputeEmulatorLink(GLS_Program *program, char *log, int logSize)
{
    GLS_Shader *shader;
    GLuint cs, p;
    GLint ok = GL_FALSE;
    GLsizei length = 0;
    if (!program || !program->computeShader || program->id >= GLS_MAX_PROGRAMS) {
        computeLog(log, logSize, "compute program has no compute shader");
        return FALSE;
    }
    shader = glsFindShader(program->computeShader);
    if (!shader || !shader->source) {
        computeLog(log, logSize, "compute shader has no source");
        return FALSE;
    }
    if (!computeMakeCurrent()) {
        computeLog(log, logSize, "software compute worker unavailable");
        return FALSE;
    }
    if (g_worker.programs[program->id]) {
        g_worker.DeleteProgram(g_worker.programs[program->id]);
        g_worker.programs[program->id] = 0;
    }
    cs = g_worker.CreateShader(GL_COMPUTE_SHADER);
    g_worker.ShaderSource(cs, 1, (const GLchar *const*)&shader->source, NULL);
    g_worker.CompileShader(cs);
    g_worker.GetShaderiv(cs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (log && logSize > 0) {
            g_worker.GetShaderInfoLog(cs, logSize, &length, log);
            log[logSize - 1] = '\0';
        }
        g_worker.DeleteShader(cs);
        computeReleaseCurrent();
        return FALSE;
    }
    p = g_worker.CreateProgram();
    g_worker.AttachShader(p, cs);
    g_worker.LinkProgram(p);
    g_worker.GetProgramiv(p, GL_LINK_STATUS, &ok);
    g_worker.DeleteShader(cs);
    if (!ok) {
        if (log && logSize > 0) {
            g_worker.GetProgramInfoLog(p, logSize, &length, log);
            log[logSize - 1] = '\0';
        }
        g_worker.DeleteProgram(p);
        computeReleaseCurrent();
        return FALSE;
    }
    g_worker.programs[program->id] = p;
    computeLog(log, logSize, "");
    computeReleaseCurrent();
    return TRUE;
}

static char *stageInstrumentGeometrySource(const char *source)
{
    static const char declaration[] =
        "\nflat out uint _gldPrimitiveSerial;\n";
    static const char initializer[] =
        "\n_gldPrimitiveSerial=uint(gl_PrimitiveIDIn)*65536u;\n";
    static const char endPrefix[] = "_gldPrimitiveSerial++;";
    const char *insertDecl, *mainWord, *mainBrace, *p;
    char *out, *w;
    size_t sourceLen, extra = sizeof(declaration) + sizeof(initializer), endCount = 0;

    if (!source) return NULL;
    sourceLen = strlen(source);
    mainWord = strstr(source, "void main");
    if (!mainWord) return NULL;
    mainBrace = strchr(mainWord, '{');
    if (!mainBrace) return NULL;
    insertDecl = source;
    if (!strncmp(source, "#version", 8)) {
        const char *line = strchr(source, '\n');
        if (line) insertDecl = line + 1;
    }
    for (p = source; (p = strstr(p, "EndPrimitive")) != NULL; p += 12) {
        if ((p == source || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
            !(isalnum((unsigned char)p[12]) || p[12] == '_'))
            ++endCount;
    }
    extra += endCount * (sizeof(endPrefix) - 1) + 1;
    out = (char *)malloc(sourceLen + extra);
    if (!out) return NULL;
    w = out;
    memcpy(w, source, (size_t)(insertDecl - source));
    w += insertDecl - source;
    memcpy(w, declaration, sizeof(declaration) - 1); w += sizeof(declaration) - 1;

    p = insertDecl;
    while (*p) {
        if (p == mainBrace + 1) {
            memcpy(w, initializer, sizeof(initializer) - 1);
            w += sizeof(initializer) - 1;
        }
        if (!strncmp(p, "EndPrimitive", 12) &&
            (p == source || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
            !(isalnum((unsigned char)p[12]) || p[12] == '_')) {
            memcpy(w, endPrefix, sizeof(endPrefix) - 1);
            w += sizeof(endPrefix) - 1;
        }
        *w++ = *p++;
    }
    *w = '\0';
    return out;
}

static GLuint stageCompileShader(GLenum type, const char *source,
                                 char *log, int logSize)
{
    GLuint shader;
    GLint ok = GL_FALSE;
    GLsizei length = 0;
    char *instrumented = NULL;
    if (!source) return 0;
    if (type == GL_GEOMETRY_SHADER) {
        instrumented = stageInstrumentGeometrySource(source);
        if (!instrumented) {
            computeLog(log, logSize, "geometry shader could not be instrumented for primitive boundaries");
            return 0;
        }
        source = instrumented;
    }
    shader = g_worker.CreateShader(type);
    g_worker.ShaderSource(shader, 1, (const GLchar *const *)&source, NULL);
    g_worker.CompileShader(shader);
    g_worker.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (log && logSize > 0) {
            g_worker.GetShaderInfoLog(shader, logSize, &length, log);
            log[logSize - 1] = '\0';
        }
        g_worker.DeleteShader(shader);
        free(instrumented);
        return 0;
    }
    free(instrumented);
    return shader;
}

static void stageTypeLayout(GLenum type, int *components, int *bytesPerComponent)
{
    int c = 1, b = 4;
    switch (type) {
    case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_UNSIGNED_INT_VEC2:
    case GL_BOOL_VEC2: case GL_DOUBLE_VEC2: c = 2; break;
    case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_UNSIGNED_INT_VEC3:
    case GL_BOOL_VEC3: case GL_DOUBLE_VEC3: c = 3; break;
    case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_UNSIGNED_INT_VEC4:
    case GL_BOOL_VEC4: case GL_DOUBLE_VEC4: c = 4; break;
    case GL_FLOAT_MAT2: c = 4; break;
    case GL_FLOAT_MAT3: c = 9; break;
    case GL_FLOAT_MAT4: c = 16; break;
    default: c = 1; break;
    }
    if (type == GL_DOUBLE || type == GL_DOUBLE_VEC2 ||
        type == GL_DOUBLE_VEC3 || type == GL_DOUBLE_VEC4) b = 8;
    if (components) *components = c;
    if (bytesPerComponent) *bytesPerComponent = b;
}

static BOOL stageNamePresent(char names[][64], int count, const char *name)
{
    int i;
    for (i = 0; i < count; ++i)
        if (!strcmp(names[i], name)) return TRUE;
    return FALSE;
}

BOOL gldStageEmulatorLinkGraphics(GLS_Program *program, char *log, int logSize)
{
    GLuint shaders[5];
    GLenum types[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER,
                        GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER,
                        GL_FRAGMENT_SHADER };
    GLuint shaderNames[5];
    char captureNames[2 + GLS_MAX_STAGE_VARYINGS + GLS_MAX_VERTEX_ATTRIBS][64];
    const GLchar *capturePointers[2 + GLS_MAX_STAGE_VARYINGS + GLS_MAX_VERTEX_ATTRIBS];
    glslVaryingInfo reflected[GLS_MAX_STAGE_VARYINGS + 1];
    GLS_Shader *fragment;
    GLuint p;
    GLint ok = GL_FALSE;
    GLsizei length = 0;
    int shaderCount = 0, captureCount = 0, i, j;

    if (!program || program->id >= GLS_MAX_PROGRAMS || !program->vertShader) {
        computeLog(log, logSize, "graphics program has no vertex shader");
        return FALSE;
    }

    program->softwareGraphicsStages = program->softwareVertexExecution ||
        program->softwareFragmentExecution ||
        (program->geomShader || program->tessControlShader || program->tessEvalShader)
        ? TRUE : FALSE;
    program->stageVaryingCount = 0;
    program->captureFieldCount = 0;
    program->captureStride = 0;

    fragment = glsFindShader(program->fragShader);
    if (program->softwareGraphicsStages && !program->softwareFragmentExecution &&
        fragment && fragment->source) {
        int count = glslReflectVaryings(fragment->source, 1, reflected,
                                        GLS_MAX_STAGE_VARYINGS + 1);
        if (count > GLS_MAX_STAGE_VARYINGS) {
            computeLog(log, logSize,
                       "fragment interface exceeds the eight D3D9 interpolators");
            return FALSE;
        }
        for (i = 0; i < count; ++i) {
            GLS_StageVarying *v = &program->stageVaryings[i];
            strncpy(v->name, reflected[i].name, sizeof(v->name) - 1);
            v->name[sizeof(v->name) - 1] = '\0';
            v->components = reflected[i].components;
            v->location = reflected[i].location;
            v->isFlat = reflected[i].isFlat;
            v->isInteger = reflected[i].isInteger;
            v->isUnsigned = reflected[i].isUnsigned;
        }
        program->stageVaryingCount = count;
    }

    if (program->softwareGraphicsStages) {
        strcpy(captureNames[captureCount++], "gl_Position");
        if (program->geomShader)
            strcpy(captureNames[captureCount++], "_gldPrimitiveSerial");
        for (i = 0; i < program->stageVaryingCount; ++i) {
            strncpy(captureNames[captureCount], program->stageVaryings[i].name, 63);
            captureNames[captureCount][63] = '\0';
            ++captureCount;
        }
    }
    for (i = 0; i < program->transformFeedbackCount; ++i) {
        const char *name = program->transformFeedbackVaryings[i];
        if (name[0] && !stageNamePresent(captureNames, captureCount, name)) {
            strncpy(captureNames[captureCount], name, 63);
            captureNames[captureCount][63] = '\0';
            ++captureCount;
        }
    }
    if (!captureCount) {
        computeLog(log, logSize, "graphics worker has no stage outputs to capture");
        return FALSE;
    }
    for (i = 0; i < captureCount; ++i) capturePointers[i] = captureNames[i];

    if (!computeMakeCurrent()) {
        computeLog(log, logSize, "software graphics-stage worker unavailable");
        return FALSE;
    }
    if (g_worker.graphicsPrograms[program->id]) {
        g_worker.DeleteProgram(g_worker.graphicsPrograms[program->id]);
        g_worker.graphicsPrograms[program->id] = 0;
    }
    shaders[0] = program->vertShader;
    shaders[1] = program->tessControlShader;
    shaders[2] = program->tessEvalShader;
    shaders[3] = program->geomShader;
    shaders[4] = program->fragShader;
    p = g_worker.CreateProgram();
    for (i = 0; i < 5; ++i) {
        GLS_Shader *sourceShader;
        GLuint compiled;
        if (!shaders[i]) continue;
        sourceShader = glsFindShader(shaders[i]);
        compiled = stageCompileShader(types[i], sourceShader ? sourceShader->source : NULL,
                                      log, logSize);
        if (!compiled) goto fail;
        shaderNames[shaderCount++] = compiled;
        g_worker.AttachShader(p, compiled);
    }
    for (i = 0; i < program->attribBindingCount; ++i) {
        if (program->attribBindings[i].set)
            g_worker.BindAttribLocation(p, program->attribBindings[i].index,
                                        program->attribBindings[i].name);
    }
    g_worker.TransformFeedbackVaryings(p, captureCount, capturePointers,
                                       GL_INTERLEAVED_ATTRIBS);
    g_worker.LinkProgram(p);
    g_worker.GetProgramiv(p, GL_LINK_STATUS, &ok);
    for (i = 0; i < shaderCount; ++i) g_worker.DeleteShader(shaderNames[i]);
    shaderCount = 0;
    if (!ok) {
        if (log && logSize > 0) {
            g_worker.GetProgramInfoLog(p, logSize, &length, log);
            log[logSize - 1] = '\0';
        }
        goto fail;
    }

    for (i = 0; i < captureCount; ++i) {
        GLS_CaptureField *field = &program->captureFields[i];
        GLsizei size = 0, nameLength = 0;
        GLenum type = GL_FLOAT;
        int componentBytes;
        memset(field, 0, sizeof(*field));
        field->userIndex = -1;
        g_worker.GetTransformFeedbackVarying(p, (GLuint)i, sizeof(field->name),
                                             &nameLength, &size, &type, field->name);
        field->name[sizeof(field->name) - 1] = '\0';
        field->type = type;
        field->arraySize = size > 0 ? size : 1;
        stageTypeLayout(type, &field->components, &componentBytes);
        field->offset = program->captureStride;
        field->bytes = field->components * field->arraySize * componentBytes;
        program->captureStride += field->bytes;
        for (j = 0; j < program->transformFeedbackCount; ++j) {
            if (!strcmp(field->name, program->transformFeedbackVaryings[j])) {
                field->userIndex = j;
                break;
            }
        }
    }
    program->captureFieldCount = captureCount;
    g_worker.graphicsPrograms[program->id] = p;
    computeLog(log, logSize, "");
    computeReleaseCurrent();
    return TRUE;

fail:
    for (i = 0; i < shaderCount; ++i) g_worker.DeleteShader(shaderNames[i]);
    if (p) g_worker.DeleteProgram(p);
    computeReleaseCurrent();
    return FALSE;
}

static void computeMirrorBinding(GLenum target, GLuint index,
                                 const GLS_IndexedBufferBinding *binding)
{
    GLS_Buffer *buffer;
    GLuint id;
    if (!binding || !binding->buffer || binding->buffer >= GLS_MAX_BUFFERS) {
        g_worker.BindBufferBase(target, index, 0);
        return;
    }
    buffer = glsFindBuffer(binding->buffer);
    if (!buffer || !buffer->data || buffer->size <= 0) {
        g_worker.BindBufferBase(target, index, 0);
        return;
    }
    id = g_worker.buffers[binding->buffer];
    if (!id) {
        g_worker.GenBuffers(1, &id);
        g_worker.buffers[binding->buffer] = id;
    }
    g_worker.BindBuffer(target, id);
    g_worker.BufferData(target, buffer->size, buffer->data, GL_DYNAMIC_COPY);
    if (binding->offset > 0 || (binding->size > 0 && binding->size < buffer->size))
        g_worker.BindBufferRange(target, index, id, binding->offset,
                                 binding->size > 0 ? binding->size : buffer->size - binding->offset);
    else
        g_worker.BindBufferBase(target, index, id);
}

static void computeReadBinding(GLenum target,
                               const GLS_IndexedBufferBinding *binding)
{
    GLS_Buffer *buffer;
    GLuint id;
    if (!binding || !binding->buffer || binding->buffer >= GLS_MAX_BUFFERS) return;
    buffer = glsFindBuffer(binding->buffer);
    id = g_worker.buffers[binding->buffer];
    if (!buffer || !buffer->data || !id) return;
    g_worker.BindBuffer(target, id);
    g_worker.GetBufferSubData(target, 0, buffer->size, buffer->data);
}

typedef struct {
    GLenum internalFormat;
    GLenum format;
    GLenum type;
    int bytesPerPixel;
} ComputeTextureFormat;

static ComputeTextureFormat computeTextureFormat(GLenum internalFormat)
{
    ComputeTextureFormat f;
    f.internalFormat = GL_RGBA8;
    f.format = GL_RGBA;
    f.type = GL_UNSIGNED_BYTE;
    f.bytesPerPixel = 4;

    switch (internalFormat) {
    case GL_R8:
        f.internalFormat = GL_R8; f.format = GL_RED; f.bytesPerPixel = 1; break;
    case GL_RG8:
        f.internalFormat = GL_RG8; f.format = GL_RG; f.bytesPerPixel = 2; break;
    case GL_RGB:
    case GL_RGB8:
        f.internalFormat = GL_RGB8; f.format = GL_RGB; f.bytesPerPixel = 3; break;
    case GL_RGBA:
    case GL_RGBA8:
        break;
    case GL_R32UI:
        f.internalFormat = GL_R32UI; f.format = GL_RED_INTEGER;
        f.type = GL_UNSIGNED_INT; f.bytesPerPixel = 4; break;
    case GL_R32I:
        f.internalFormat = GL_R32I; f.format = GL_RED_INTEGER;
        f.type = GL_INT; f.bytesPerPixel = 4; break;
    case GL_R32F:
        f.internalFormat = GL_R32F; f.format = GL_RED;
        f.type = GL_FLOAT; f.bytesPerPixel = 4; break;
    case GL_RGBA8UI:
        f.internalFormat = GL_RGBA8UI; f.format = GL_RGBA_INTEGER;
        f.type = GL_UNSIGNED_BYTE; f.bytesPerPixel = 4; break;
    case GL_RGBA8I:
        f.internalFormat = GL_RGBA8I; f.format = GL_RGBA_INTEGER;
        f.type = GL_BYTE; f.bytesPerPixel = 4; break;
    default:
        /* The direct D3D9 texture fallback is RGBA8 for formats it cannot
         * represent natively.  Mirror the resource that actually exists,
         * rather than inventing precision which has already been lost. */
        break;
    }
    return f;
}

static GLenum computeTextureTarget(const GLS_Texture *texture)
{
    if (texture->pCubeTex) return GL_TEXTURE_CUBE_MAP;
    if (texture->pVolTex) return GL_TEXTURE_3D;
    if (texture->target == GL_TEXTURE_1D) return GL_TEXTURE_1D;
    return GL_TEXTURE_2D;
}

static GLuint computeMirrorBufferResource(GLenum target, GLS_Buffer *buffer)
{
    GLuint id;
    if (!buffer || !buffer->data || buffer->size <= 0 ||
        buffer->id >= GLS_MAX_BUFFERS) return 0;
    id = g_worker.buffers[buffer->id];
    if (!id) {
        g_worker.GenBuffers(1, &id);
        g_worker.buffers[buffer->id] = id;
    }
    g_worker.BindBuffer(target, id);
    g_worker.BufferData(target, buffer->size, buffer->data, GL_DYNAMIC_COPY);
    return id;
}

static BOOL computeUploadTextureBuffer(GLS_Texture *texture)
{
    GLS_Buffer *buffer;
    GLuint textureName;
    GLuint bufferName;

    if (!texture || !texture->allocated || texture->target != GL_TEXTURE_BUFFER ||
        texture->id >= GLS_MAX_TEXTURES || !texture->bufferObject) return FALSE;
    buffer = glsFindBuffer(texture->bufferObject);
    bufferName = computeMirrorBufferResource(GL_TEXTURE_BUFFER, buffer);
    if (!bufferName) return FALSE;
    textureName = g_worker.textures[texture->id];
    if (!textureName) {
        g_worker.GenTextures(1, &textureName);
        if (!textureName) return FALSE;
        g_worker.textures[texture->id] = textureName;
    }
    g_worker.BindTexture(GL_TEXTURE_BUFFER, textureName);
    if (texture->bufferSize > 0)
        g_worker.TexBufferRange(GL_TEXTURE_BUFFER, texture->internalFormat,
                                bufferName, texture->bufferOffset,
                                texture->bufferSize);
    else
        g_worker.TexBuffer(GL_TEXTURE_BUFFER, texture->internalFormat,
                           bufferName);
    return TRUE;
}

static BOOL computeUploadTexture(GLS_Texture *texture)
{
    ComputeTextureFormat tf;
    GLenum bindTarget;
    GLuint id;
    int level;

    if (!texture || !texture->allocated ||
        (!texture->pTex && !texture->pCubeTex && !texture->pVolTex)) return FALSE;
    if (texture->id >= GLS_MAX_TEXTURES) return FALSE;

    id = g_worker.textures[texture->id];
    if (!id) {
        g_worker.GenTextures(1, &id);
        if (!id) return FALSE;
        g_worker.textures[texture->id] = id;
    }
    bindTarget = computeTextureTarget(texture);
    tf = computeTextureFormat(texture->internalFormat);
    g_worker.BindTexture(bindTarget, id);
    g_worker.PixelStorei(GL_PACK_ALIGNMENT, 1);
    g_worker.PixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (level = 0; level < 32; ++level) {
        int width, height, depth;
        size_t bytes;
        void *data;
        unsigned int transferTarget = bindTarget;

        if (!_glsTransferTextureLevel(texture, transferTarget, level,
                                      tf.format, tf.type, NULL, FALSE,
                                      &width, &height, &depth)) break;
        if (width <= 0 || height <= 0 || depth <= 0) break;
        bytes = (size_t)width * (size_t)height * (size_t)depth *
                (size_t)tf.bytesPerPixel;
        data = calloc(1, bytes);
        if (!data) return FALSE;

        if (bindTarget == GL_TEXTURE_CUBE_MAP) {
            int face;
            for (face = 0; face < 6; ++face) {
                transferTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (unsigned int)face;
                memset(data, 0, bytes);
                if (_glsTransferTextureLevel(texture, transferTarget, level,
                                             tf.format, tf.type, data, FALSE,
                                             NULL, NULL, NULL)) {
                    g_worker.TexImage2D(transferTarget, level, (GLint)tf.internalFormat,
                                        width, height, 0, tf.format, tf.type, data);
                }
            }
        } else {
            _glsTransferTextureLevel(texture, transferTarget, level,
                                     tf.format, tf.type, data, FALSE,
                                     NULL, NULL, NULL);
            if (bindTarget == GL_TEXTURE_1D)
                g_worker.TexImage1D(bindTarget, level, (GLint)tf.internalFormat,
                                    width, 0, tf.format, tf.type, data);
            else if (bindTarget == GL_TEXTURE_3D)
                g_worker.TexImage3D(bindTarget, level, (GLint)tf.internalFormat,
                                    width, height, depth, 0, tf.format, tf.type, data);
            else
                g_worker.TexImage2D(bindTarget, level, (GLint)tf.internalFormat,
                                    width, height, 0, tf.format, tf.type, data);
        }
        free(data);
    }

    g_worker.TexParameteri(bindTarget, GL_TEXTURE_MIN_FILTER, (GLint)texture->minFilter);
    g_worker.TexParameteri(bindTarget, GL_TEXTURE_MAG_FILTER, (GLint)texture->magFilter);
    g_worker.TexParameteri(bindTarget, GL_TEXTURE_WRAP_S, (GLint)texture->wrapS);
    if (bindTarget != GL_TEXTURE_1D)
        g_worker.TexParameteri(bindTarget, GL_TEXTURE_WRAP_T, (GLint)texture->wrapT);
    if (bindTarget == GL_TEXTURE_3D || bindTarget == GL_TEXTURE_CUBE_MAP)
        g_worker.TexParameteri(bindTarget, GL_TEXTURE_WRAP_R, (GLint)texture->wrapR);
    return level > 0;
}

static void computeMirrorTextures(GLS_State *state)
{
    int unit;
    for (unit = 0; unit < GLS_MAX_TEX_UNITS; ++unit) {
        GLS_Texture *texture;
        g_worker.ActiveTexture(GL_TEXTURE0 + (GLenum)unit);

        texture = glsFindTexture(state->boundTexture2D[unit]);
        if (texture && computeUploadTexture(texture))
            g_worker.BindTexture(computeTextureTarget(texture),
                                 g_worker.textures[texture->id]);
        else
            g_worker.BindTexture(GL_TEXTURE_2D, 0);

        texture = glsFindTexture(state->boundTextureCube[unit]);
        if (texture && computeUploadTexture(texture))
            g_worker.BindTexture(GL_TEXTURE_CUBE_MAP, g_worker.textures[texture->id]);
        else
            g_worker.BindTexture(GL_TEXTURE_CUBE_MAP, 0);

        texture = glsFindTexture(state->boundTexture3D[unit]);
        if (texture && computeUploadTexture(texture))
            g_worker.BindTexture(GL_TEXTURE_3D, g_worker.textures[texture->id]);
        else
            g_worker.BindTexture(GL_TEXTURE_3D, 0);

        texture = glsFindTexture(state->boundTextureBuffer[unit]);
        if (texture && computeUploadTextureBuffer(texture))
            g_worker.BindTexture(GL_TEXTURE_BUFFER, g_worker.textures[texture->id]);
        else
            g_worker.BindTexture(GL_TEXTURE_BUFFER, 0);
    }
}

static void computeMirrorSamplers(GLS_State *state)
{
    int unit;
    for (unit = 0; unit < GLS_MAX_TEX_UNITS; ++unit) {
        GLuint publicName = state->boundSampler[unit];
        GLS_Sampler *sampler = glsFindSampler(publicName);
        GLuint privateName;
        if (!sampler || publicName >= GLS_MAX_SAMPLERS) {
            g_worker.BindSampler((GLuint)unit, 0);
            continue;
        }
        privateName = g_worker.samplers[publicName];
        if (!privateName) {
            g_worker.GenSamplers(1, &privateName);
            g_worker.samplers[publicName] = privateName;
        }
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_MIN_FILTER,
                                   (GLint)sampler->minFilter);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_MAG_FILTER,
                                   (GLint)sampler->magFilter);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_WRAP_S,
                                   (GLint)sampler->wrapS);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_WRAP_T,
                                   (GLint)sampler->wrapT);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_WRAP_R,
                                   (GLint)sampler->wrapR);
        g_worker.SamplerParameterfv(privateName, GL_TEXTURE_BORDER_COLOR,
                                    sampler->borderColor);
        g_worker.SamplerParameterf(privateName, GL_TEXTURE_MIN_LOD,
                                   sampler->minLod);
        g_worker.SamplerParameterf(privateName, GL_TEXTURE_MAX_LOD,
                                   sampler->maxLod);
        g_worker.SamplerParameterf(privateName, GL_TEXTURE_LOD_BIAS,
                                   sampler->lodBias);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_COMPARE_MODE,
                                   (GLint)sampler->compareMode);
        g_worker.SamplerParameteri(privateName, GL_TEXTURE_COMPARE_FUNC,
                                   (GLint)sampler->compareFunc);
        if (sampler->maxAnisotropy > 1.0f)
            g_worker.SamplerParameterf(privateName, GL_TEXTURE_MAX_ANISOTROPY,
                                       sampler->maxAnisotropy);
        g_worker.BindSampler((GLuint)unit, privateName);
    }
}

static void computeMirrorImages(GLS_State *state)
{
    int unit;
    for (unit = 0; unit < GLS_MAX_IMAGE_UNITS; ++unit) {
        const GLS_ImageBinding *binding = &state->imageBindings[unit];
        GLS_Texture *texture = glsFindTexture(binding->texture);
        if (!texture || !computeUploadTexture(texture)) {
            g_worker.BindImageTexture((GLuint)unit, 0, 0, GL_FALSE, 0,
                                      GL_READ_ONLY, GL_RGBA8);
            continue;
        }
        g_worker.BindImageTexture((GLuint)unit, g_worker.textures[texture->id],
                                  binding->level, binding->layered,
                                  binding->layer, binding->access,
                                  binding->format ? binding->format : texture->internalFormat);
    }
}

static void computeReadImages(GLS_State *state)
{
    BOOL copied[GLS_MAX_TEXTURES];
    int unit;
    memset(copied, 0, sizeof(copied));

    for (unit = 0; unit < GLS_MAX_IMAGE_UNITS; ++unit) {
        const GLS_ImageBinding *binding = &state->imageBindings[unit];
        GLS_Texture *texture;
        ComputeTextureFormat tf;
        GLenum target;
        int width, height, depth;
        size_t bytes;
        void *data;

        if (!binding->texture || binding->texture >= GLS_MAX_TEXTURES ||
            binding->access == GL_READ_ONLY || copied[binding->texture]) continue;
        texture = glsFindTexture(binding->texture);
        if (!texture || !g_worker.textures[binding->texture]) continue;
        copied[binding->texture] = TRUE;
        tf = computeTextureFormat(texture->internalFormat);
        target = computeTextureTarget(texture);
        if (!_glsTransferTextureLevel(texture, target, binding->level,
                                      tf.format, tf.type, NULL, FALSE,
                                      &width, &height, &depth)) continue;
        bytes = (size_t)width * (size_t)height * (size_t)depth *
                (size_t)tf.bytesPerPixel;
        data = malloc(bytes);
        if (!data) continue;
        g_worker.ActiveTexture(GL_TEXTURE0);
        g_worker.BindTexture(target, g_worker.textures[binding->texture]);

        if (target == GL_TEXTURE_CUBE_MAP) {
            int face;
            for (face = 0; face < 6; ++face) {
                GLenum faceTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)face;
                g_worker.GetTexImage(faceTarget, binding->level, tf.format, tf.type, data);
                _glsTransferTextureLevel(texture, faceTarget, binding->level,
                                         tf.format, tf.type, data, TRUE,
                                         NULL, NULL, NULL);
            }
        } else {
            g_worker.GetTexImage(target, binding->level, tf.format, tf.type, data);
            _glsTransferTextureLevel(texture, target, binding->level,
                                     tf.format, tf.type, data, TRUE,
                                     NULL, NULL, NULL);
        }
        free(data);
    }
}

static void computeUploadUniformsTo(GLuint mesaProgram, GLS_Program *program)
{
    int i, j;
    for (i = 0; i < program->uniformCount; ++i) {
        GLS_Uniform *u = &program->uniforms[i];
        GLint mesaLocation;
        if (!u->set || u->location < 0 || u->location >= program->resolvedCount) continue;
        mesaLocation = g_worker.GetUniformLocation(mesaProgram,
                                                   program->resolved[u->location].name);
        if (mesaLocation < 0) continue;
        switch (u->type) {
        case 0: g_worker.Uniform1i(mesaLocation, (GLint)u->data[0]); break;
        case 1: g_worker.Uniform1f(mesaLocation, u->data[0]); break;
        case 2: g_worker.Uniform2fv(mesaLocation, 1, u->data); break;
        case 3: g_worker.Uniform3fv(mesaLocation, 1, u->data); break;
        case 4: g_worker.Uniform4fv(mesaLocation, 1, u->data); break;
        case 5: g_worker.UniformMatrix2fv(mesaLocation, 1, GL_FALSE, u->data); break;
        case 6: g_worker.UniformMatrix3fv(mesaLocation, 1, GL_FALSE, u->data); break;
        case 7: g_worker.UniformMatrix4fv(mesaLocation, 1, GL_FALSE, u->data); break;
        default:
            for (j = 0; j < 4; ++j) if (u->data[j] != 0.0f) break;
            g_worker.Uniform4fv(mesaLocation, 1, u->data);
            break;
        }
    }
}

static void computeUploadUniforms(GLS_Program *program)
{
    computeUploadUniformsTo(g_worker.programs[program->id], program);
}

BOOL gldComputeEmulatorDispatch(GLS_Program *program,
                                unsigned int groupsX,
                                unsigned int groupsY,
                                unsigned int groupsZ,
                                char *log, int logSize)
{
    GLS_State *s = glsGetState();
    int i;
    GLenum error;
    if (!program || !program->computeShader) {
        computeLog(log, logSize, "no compute program is bound");
        return FALSE;
    }
    if (!g_worker.programs[program->id] && !gldComputeEmulatorLink(program, log, logSize))
        return FALSE;
    if (!computeMakeCurrent()) {
        computeLog(log, logSize, "software compute worker unavailable");
        return FALSE;
    }
    g_worker.UseProgram(g_worker.programs[program->id]);
    for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
        computeMirrorBinding(GL_UNIFORM_BUFFER, (GLuint)i, &s->uniformBindings[i]);
        computeMirrorBinding(GL_SHADER_STORAGE_BUFFER, (GLuint)i, &s->shaderStorageBindings[i]);
        computeMirrorBinding(GL_ATOMIC_COUNTER_BUFFER, (GLuint)i, &s->atomicCounterBindings[i]);
    }
    computeMirrorTextures(s);
    computeMirrorSamplers(s);
    computeMirrorImages(s);
    computeUploadUniforms(program);
    while (g_worker.GetError() != GL_NO_ERROR) { }
    g_worker.DispatchCompute(groupsX, groupsY, groupsZ);
    g_worker.MemoryBarrier(GL_ALL_BARRIER_BITS);
    g_worker.Finish();
    error = g_worker.GetError();
    if (error == GL_NO_ERROR) {
        computeReadImages(s);
        for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
            computeReadBinding(GL_SHADER_STORAGE_BUFFER, &s->shaderStorageBindings[i]);
            computeReadBinding(GL_ATOMIC_COUNTER_BUFFER, &s->atomicCounterBindings[i]);
        }
        error = g_worker.GetError();
    }
    g_worker.UseProgram(0);
    computeReleaseCurrent();
    if (error != GL_NO_ERROR) {
        char message[128];
        _snprintf(message, sizeof(message), "software compute dispatch failed with GL error 0x%04X", error);
        message[sizeof(message) - 1] = '\0';
        computeLog(log, logSize, message);
        return FALSE;
    }
    computeLog(log, logSize, "");
    return TRUE;
}

static GLuint stageMirrorBuffer(GLenum target, GLS_Buffer *buffer)
{
    return computeMirrorBufferResource(target, buffer);
}

static void stageConfigureVertexInput(GLS_State *state)
{
    GLS_VAO *vao = glsFindVAO(state->boundVAO);
    int i;
    if (!g_worker.stageVao) g_worker.GenVertexArrays(1, &g_worker.stageVao);
    g_worker.BindVertexArray(g_worker.stageVao);
    for (i = 0; i < GLS_MAX_VERTEX_ATTRIBS; ++i) {
        GLS_VertexAttrib *attrib = vao ? &vao->attribs[i] : NULL;
        if (attrib && attrib->enabled) {
            GLS_Buffer *buffer = glsFindBuffer(attrib->bufferBinding);
            const void *pointer = attrib->pointer;
            if (buffer) {
                stageMirrorBuffer(GL_ARRAY_BUFFER, buffer);
            } else {
                g_worker.BindBuffer(GL_ARRAY_BUFFER, 0);
            }
            g_worker.EnableVertexAttribArray((GLuint)i);
            if (attrib->integer)
                g_worker.VertexAttribIPointer((GLuint)i, attrib->size, attrib->type,
                                              attrib->stride, pointer);
            else
                g_worker.VertexAttribPointer((GLuint)i, attrib->size, attrib->type,
                                             attrib->normalized, attrib->stride, pointer);
            g_worker.VertexAttribDivisor((GLuint)i, attrib->divisor);
        } else {
            static const GLfloat defaultValue[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            g_worker.DisableVertexAttribArray((GLuint)i);
            g_worker.VertexAttribDivisor((GLuint)i, 0);
            g_worker.VertexAttrib4fv((GLuint)i,
                                     attrib ? attrib->defaultValue : defaultValue);
        }
    }
}

static GLenum stageOutputPrimitiveMode(const GLS_Program *program,
                                       GLenum inputMode)
{
    GLS_Shader *shader;
    const char *source;
    if (program->geomShader) {
        shader = glsFindShader(program->geomShader);
        source = shader ? shader->source : NULL;
        if (source && strstr(source, "line_strip")) return GL_LINE_STRIP;
        if (source && strstr(source, "triangle_strip")) return GL_TRIANGLE_STRIP;
        return GL_POINTS;
    }
    if (program->tessEvalShader) {
        shader = glsFindShader(program->tessEvalShader);
        source = shader ? shader->source : NULL;
        if (source && strstr(source, "point_mode")) return GL_POINTS;
        if (source && strstr(source, "isolines")) return GL_LINES;
        return GL_TRIANGLES;
    }
    return inputMode;
}

static GLenum stageTransformFeedbackMode(GLenum outputMode)
{
    if (outputMode == GL_POINTS) return GL_POINTS;
    if (outputMode == GL_LINES || outputMode == GL_LINE_STRIP ||
        outputMode == GL_LINE_LOOP) return GL_LINES;
    return GL_TRIANGLES;
}

static size_t stageEstimateVertexCapacity(const GLS_Program *program,
                                          int inputCount, int stride)
{
    size_t count = inputCount > 0 ? (size_t)inputCount : 1;
    const size_t maxBytes = 256u * 1024u * 1024u;
    if (program->tessEvalShader) {
        GLS_State *state = glsGetState();
        int patchVertices = state->patchVertices > 0 ? state->patchVertices : 3;
        size_t patches = (count + (size_t)patchVertices - 1) / (size_t)patchVertices;
        count = patches * 24576u; /* 64x64 quad-domain triangle-list ceiling */
    }
    if (program->geomShader) {
        GLS_Shader *shader = glsFindShader(program->geomShader);
        const char *mark = shader && shader->source
                         ? strstr(shader->source, "max_vertices") : NULL;
        unsigned long maxVertices = 256;
        if (mark) {
            const char *eq = strchr(mark, '=');
            if (eq) {
                unsigned long parsed = strtoul(eq + 1, NULL, 10);
                if (parsed > 0) maxVertices = parsed;
            }
        }
        if (count > (size_t)-1 / maxVertices) count = (size_t)-1;
        else count *= maxVertices;
    }
    if (stride <= 0) return 0;
    if (count > maxBytes / (size_t)stride) count = maxBytes / (size_t)stride;
    if (count < (size_t)inputCount) count = (size_t)inputCount;
    return count;
}

static BOOL stageFieldMatches(const char *fieldName, const char *requested)
{
    size_t n;
    if (!strcmp(fieldName, requested)) return TRUE;
    n = strlen(requested);
    return !strncmp(fieldName, requested, n) && !strcmp(fieldName + n, "[0]");
}

static void stageWriteUserTransformFeedback(GLS_Program *program,
                                            const unsigned char *raw,
                                            size_t vertexCount)
{
    GLS_State *state = glsGetState();
    int user;
    if (!state->transformFeedbackActive || program->transformFeedbackCount <= 0)
        return;

    if (program->transformFeedbackMode == GL_INTERLEAVED_ATTRIBS) {
        GLS_IndexedBufferBinding *binding = &state->transformFeedbackBindings[0];
        GLS_Buffer *buffer = glsFindBuffer(binding->buffer);
        int recordBytes = 0;
        size_t v;
        unsigned char *packed;
        ptrdiff_t dstOffset;
        for (user = 0; user < program->transformFeedbackCount; ++user) {
            int f;
            for (f = 0; f < program->captureFieldCount; ++f)
                if (stageFieldMatches(program->captureFields[f].name,
                                      program->transformFeedbackVaryings[user])) {
                    recordBytes += program->captureFields[f].bytes;
                    break;
                }
        }
        if (!buffer || recordBytes <= 0 || !vertexCount) return;
        packed = (unsigned char *)malloc(vertexCount * (size_t)recordBytes);
        if (!packed) return;
        for (v = 0; v < vertexCount; ++v) {
            int off = 0;
            for (user = 0; user < program->transformFeedbackCount; ++user) {
                int f;
                for (f = 0; f < program->captureFieldCount; ++f) {
                    GLS_CaptureField *field = &program->captureFields[f];
                    if (!stageFieldMatches(field->name,
                                           program->transformFeedbackVaryings[user])) continue;
                    memcpy(packed + v * (size_t)recordBytes + off,
                           raw + v * (size_t)program->captureStride + field->offset,
                           (size_t)field->bytes);
                    off += field->bytes;
                    break;
                }
            }
        }
        dstOffset = binding->offset + state->transformFeedbackWriteOffset[0];
        if (dstOffset < buffer->size) {
            ptrdiff_t bytes = (ptrdiff_t)(vertexCount * (size_t)recordBytes);
            ptrdiff_t limit = binding->size > 0 ? binding->offset + binding->size
                                                : buffer->size;
            if (dstOffset + bytes > limit) bytes = limit - dstOffset;
            if (bytes > 0) _glsWriteBufferObject(buffer, dstOffset, bytes, packed);
            state->transformFeedbackWriteOffset[0] += bytes > 0 ? bytes : 0;
        }
        free(packed);
    } else {
        for (user = 0; user < program->transformFeedbackCount &&
                       user < GLS_MAX_BUFFER_BINDINGS; ++user) {
            GLS_IndexedBufferBinding *binding = &state->transformFeedbackBindings[user];
            GLS_Buffer *buffer = glsFindBuffer(binding->buffer);
            GLS_CaptureField *field = NULL;
            unsigned char *packed;
            size_t v;
            ptrdiff_t dstOffset, bytes, limit;
            int f;
            for (f = 0; f < program->captureFieldCount; ++f)
                if (stageFieldMatches(program->captureFields[f].name,
                                      program->transformFeedbackVaryings[user])) {
                    field = &program->captureFields[f]; break;
                }
            if (!field || !buffer || !vertexCount) continue;
            packed = (unsigned char *)malloc(vertexCount * (size_t)field->bytes);
            if (!packed) continue;
            for (v = 0; v < vertexCount; ++v)
                memcpy(packed + v * (size_t)field->bytes,
                       raw + v * (size_t)program->captureStride + field->offset,
                       (size_t)field->bytes);
            dstOffset = binding->offset + state->transformFeedbackWriteOffset[user];
            bytes = (ptrdiff_t)(vertexCount * (size_t)field->bytes);
            limit = binding->size > 0 ? binding->offset + binding->size : buffer->size;
            if (dstOffset + bytes > limit) bytes = limit - dstOffset;
            if (bytes > 0) _glsWriteBufferObject(buffer, dstOffset, bytes, packed);
            state->transformFeedbackWriteOffset[user] += bytes > 0 ? bytes : 0;
            free(packed);
        }
    }
}

static float stageReadComponent(const unsigned char *data, GLenum type, int index)
{
    if (type == GL_DOUBLE || type == GL_DOUBLE_VEC2 ||
        type == GL_DOUBLE_VEC3 || type == GL_DOUBLE_VEC4)
        return (float)((const double *)data)[index];
    if (type == GL_INT || type == GL_INT_VEC2 || type == GL_INT_VEC3 ||
        type == GL_INT_VEC4 || type == GL_BOOL || type == GL_BOOL_VEC2 ||
        type == GL_BOOL_VEC3 || type == GL_BOOL_VEC4)
        return (float)((const GLint *)data)[index];
    if (type == GL_UNSIGNED_INT || type == GL_UNSIGNED_INT_VEC2 ||
        type == GL_UNSIGNED_INT_VEC3 || type == GL_UNSIGNED_INT_VEC4)
        return (float)((const GLuint *)data)[index];
    return ((const GLfloat *)data)[index];
}

BOOL gldStageEmulatorDraw(GLS_Program *program, unsigned int mode,
                          int first, int count, unsigned int indexType,
                          const void *indices, int baseVertex,
                          int instanceCount, unsigned int baseInstance,
                          GLD_StageDraw *result, char *log, int logSize)
{
    GLS_State *state = glsGetState();
    GLS_VAO *vao = glsFindVAO(state->boundVAO);
    GLuint mesaProgram;
    GLenum outputMode, feedbackMode;
    size_t capacity, bytes, record, vertexCount = 0;
    unsigned char *sentinel = NULL, *raw = NULL;
    GLenum error;
    int i;

    if (result) memset(result, 0, sizeof(*result));
    if (!program || count <= 0 || instanceCount <= 0 ||
        program->captureStride <= 0) return FALSE;
    mesaProgram = g_worker.graphicsPrograms[program->id];
    if (!mesaProgram && !gldStageEmulatorLinkGraphics(program, log, logSize))
        return FALSE;
    mesaProgram = g_worker.graphicsPrograms[program->id];
    if (!computeMakeCurrent()) {
        computeLog(log, logSize, "software graphics-stage worker unavailable");
        return FALSE;
    }

    outputMode = stageOutputPrimitiveMode(program, mode);
    feedbackMode = stageTransformFeedbackMode(outputMode);
    capacity = stageEstimateVertexCapacity(program, count, program->captureStride);
    if ((size_t)instanceCount > 1) {
        size_t maxCapacity = (256u * 1024u * 1024u) /
                             (size_t)program->captureStride;
        if (capacity > maxCapacity / (size_t)instanceCount)
            capacity = maxCapacity;
        else
            capacity *= (size_t)instanceCount;
    }
    if (capacity < (256u * 1024u * 1024u) / (size_t)program->captureStride)
        ++capacity; /* one untouched guard record distinguishes exact fill */
    bytes = capacity * (size_t)program->captureStride;
    sentinel = (unsigned char *)malloc(bytes);
    raw = (unsigned char *)malloc(bytes);
    if (!sentinel || !raw) goto fail;
    memset(sentinel, 0xCD, bytes);

    if (!g_worker.stageCaptureBuffer)
        g_worker.GenBuffers(1, &g_worker.stageCaptureBuffer);
    stageConfigureVertexInput(state);
    g_worker.BindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, g_worker.stageCaptureBuffer);
    g_worker.BufferData(GL_TRANSFORM_FEEDBACK_BUFFER, (GLsizeiptr)bytes,
                        sentinel, GL_DYNAMIC_COPY);
    g_worker.BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                            g_worker.stageCaptureBuffer);

    for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
        computeMirrorBinding(GL_UNIFORM_BUFFER, (GLuint)i, &state->uniformBindings[i]);
        computeMirrorBinding(GL_SHADER_STORAGE_BUFFER, (GLuint)i, &state->shaderStorageBindings[i]);
        computeMirrorBinding(GL_ATOMIC_COUNTER_BUFFER, (GLuint)i, &state->atomicCounterBindings[i]);
    }
    computeMirrorTextures(state);
    computeMirrorSamplers(state);
    computeMirrorImages(state);
    g_worker.UseProgram(mesaProgram);
    computeUploadUniformsTo(mesaProgram, program);
    if (program->tessControlShader || program->tessEvalShader) {
        g_worker.PatchParameteri(GL_PATCH_VERTICES, state->patchVertices);
        g_worker.PatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL,
                                  state->patchDefaultOuter);
        g_worker.PatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL,
                                  state->patchDefaultInner);
    }
    while (g_worker.GetError() != GL_NO_ERROR) { }
    g_worker.Enable(GL_RASTERIZER_DISCARD);
    g_worker.BeginTransformFeedback(feedbackMode);

    if (indexType != 0) {
        GLuint elementName = vao ? vao->elementBuffer : state->boundElementBuffer;
        GLS_Buffer *element = glsFindBuffer(elementName);
        if (element) stageMirrorBuffer(GL_ELEMENT_ARRAY_BUFFER, element);
        else g_worker.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (instanceCount > 1 || baseInstance)
            g_worker.DrawElementsInstancedBaseVertexBaseInstance(
                mode, count, indexType, indices, instanceCount,
                baseVertex, baseInstance);
        else
            g_worker.DrawElementsBaseVertex(mode, count, indexType, indices, baseVertex);
    } else {
        if (instanceCount > 1 || baseInstance)
            g_worker.DrawArraysInstancedBaseInstance(mode, first, count,
                                                      instanceCount, baseInstance);
        else
            g_worker.DrawArrays(mode, first, count);
    }
    g_worker.EndTransformFeedback();
    g_worker.Disable(GL_RASTERIZER_DISCARD);
    g_worker.MemoryBarrier(GL_ALL_BARRIER_BITS);
    g_worker.Finish();
    /* This Mesa build's exported core glGetError thunk dereferences an invalid
     * dispatch slot immediately after an EndTransformFeedback/Finish sequence.
     * The stage calls themselves are synchronous here and the buffer read below
     * is the operation that validates completion, so do not enter that broken
     * core thunk at this point.  glGetError remains usable before the sequence
     * (where it is used above to clear stale state) and on compute dispatches. */
    error = GL_NO_ERROR;

    g_worker.BindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, g_worker.stageCaptureBuffer);
    g_worker.GetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                              (GLsizeiptr)bytes, raw);
    for (record = capacity; record > 0; --record) {
        const unsigned char *p = raw + (record - 1) * (size_t)program->captureStride;
        size_t byte;
        for (byte = 0; byte < (size_t)program->captureStride; ++byte)
            if (p[byte] != 0xCD) break;
        if (byte != (size_t)program->captureStride) {
            vertexCount = record;
            break;
        }
    }
    if (vertexCount == capacity && capacity > 0) {
        computeLog(log, logSize, "software stage capture exceeded its 256 MiB bound");
        goto fail;
    }

    stageWriteUserTransformFeedback(program, raw, vertexCount);
    if (program->softwareGraphicsStages && result && vertexCount) {
        result->vertices = (GLD_StageVertex *)calloc(vertexCount,
                                                     sizeof(GLD_StageVertex));
        if (!result->vertices) goto fail;
        result->vertexCount = (unsigned int)vertexCount;
        result->primitiveMode = outputMode;
        for (record = 0; record < vertexCount; ++record) {
            GLD_StageVertex *vertex = &result->vertices[record];
            const unsigned char *recordData = raw + record *
                                              (size_t)program->captureStride;
            int fieldIndex;
            for (fieldIndex = 0; fieldIndex < program->captureFieldCount; ++fieldIndex) {
                GLS_CaptureField *field = &program->captureFields[fieldIndex];
                const unsigned char *fieldData = recordData + field->offset;
                int c;
                if (!strcmp(field->name, "gl_Position")) {
                    for (c = 0; c < field->components && c < 4; ++c)
                        vertex->position[c] = stageReadComponent(fieldData, field->type, c);
                } else if (!strcmp(field->name, "_gldPrimitiveSerial")) {
                    vertex->primitiveSerial = *(const GLuint *)fieldData;
                } else {
                    int varying;
                    for (varying = 0; varying < program->stageVaryingCount; ++varying) {
                        if (!stageFieldMatches(field->name,
                                               program->stageVaryings[varying].name)) continue;
                        for (c = 0; c < field->components && c < 4; ++c)
                            vertex->varying[varying][c] =
                                stageReadComponent(fieldData, field->type, c);
                        break;
                    }
                }
            }
            if (instanceCount > 1 && !program->geomShader &&
                !program->tessControlShader && !program->tessEvalShader &&
                vertexCount % (size_t)instanceCount == 0) {
                size_t perInstance = vertexCount / (size_t)instanceCount;
                if (perInstance)
                    vertex->primitiveSerial =
                        (unsigned int)(record / perInstance);
            }
        }
    }

    computeReadImages(state);
    for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
        computeReadBinding(GL_SHADER_STORAGE_BUFFER, &state->shaderStorageBindings[i]);
        computeReadBinding(GL_ATOMIC_COUNTER_BUFFER, &state->atomicCounterBindings[i]);
    }
    g_worker.UseProgram(0);
    g_worker.BindVertexArray(0);
    free(sentinel); free(raw);
    computeReleaseCurrent();
    computeLog(log, logSize, "");
    return TRUE;

fail:
    if (result && result->vertices) {
        free(result->vertices);
        memset(result, 0, sizeof(*result));
    }
    if (g_worker.Disable) g_worker.Disable(GL_RASTERIZER_DISCARD);
    if (g_worker.UseProgram) g_worker.UseProgram(0);
    free(sentinel); free(raw);
    computeReleaseCurrent();
    if (log && logSize > 0 && !log[0]) {
        _snprintf(log, logSize, "software graphics-stage draw failed");
        log[logSize - 1] = '\0';
    }
    return FALSE;
}

static void fragmentApplyState(const GLS_State *state)
{
    if (state->enableBlend) g_worker.Enable(GL_BLEND);
    else g_worker.Disable(GL_BLEND);
    g_worker.BlendFuncSeparate(state->blendSrcRGB, state->blendDstRGB,
                               state->blendSrcAlpha, state->blendDstAlpha);
    g_worker.BlendEquationSeparate(state->blendEquationRGB,
                                   state->blendEquationAlpha);
    g_worker.BlendColor(state->blendColorF[0], state->blendColorF[1],
                        state->blendColorF[2], state->blendColorF[3]);

    if (state->enableDepthTest) g_worker.Enable(GL_DEPTH_TEST);
    else g_worker.Disable(GL_DEPTH_TEST);
    g_worker.DepthFunc(state->depthFunc);
    g_worker.DepthMask(state->depthMask);
    g_worker.DepthRange(state->depthRangeNear, state->depthRangeFar);

    if (state->enableCullFace) g_worker.Enable(GL_CULL_FACE);
    else g_worker.Disable(GL_CULL_FACE);
    g_worker.CullFace(state->cullFaceMode);
    g_worker.FrontFace(state->frontFace);

    if (state->enableScissorTest) g_worker.Enable(GL_SCISSOR_TEST);
    else g_worker.Disable(GL_SCISSOR_TEST);
    g_worker.Scissor(state->scissorX, state->scissorY,
                     state->scissorW, state->scissorH);

    if (state->enableStencilTest) g_worker.Enable(GL_STENCIL_TEST);
    else g_worker.Disable(GL_STENCIL_TEST);
    g_worker.StencilFuncSeparate(GL_FRONT, state->stencilFunc,
                                 state->stencilRef, state->stencilMask);
    g_worker.StencilFuncSeparate(GL_BACK, state->stencilBackFunc,
                                 state->stencilBackRef,
                                 state->stencilBackMask);
    g_worker.StencilOpSeparate(GL_FRONT, state->stencilFail,
                               state->stencilZFail, state->stencilZPass);
    g_worker.StencilOpSeparate(GL_BACK, state->stencilBackFail,
                               state->stencilBackZFail,
                               state->stencilBackZPass);
    g_worker.StencilMaskSeparate(GL_FRONT, state->stencilWriteMask);
    g_worker.StencilMaskSeparate(GL_BACK, state->stencilBackWriteMask);

    if (state->enablePolygonOffsetFill) g_worker.Enable(GL_POLYGON_OFFSET_FILL);
    else g_worker.Disable(GL_POLYGON_OFFSET_FILL);
    g_worker.PolygonOffset(state->polygonOffsetFactor,
                           state->polygonOffsetUnits);
    g_worker.LineWidth(state->lineWidth);
    g_worker.ColorMask(state->colorMask[0], state->colorMask[1],
                       state->colorMask[2], state->colorMask[3]);
    g_worker.ClipControl(state->clipOrigin, state->clipDepthMode);
    g_worker.Viewport(state->viewportX, state->viewportY,
                      state->viewportW, state->viewportH);
}

static BOOL fragmentPrepareFramebuffer(int width, int height,
                                       const unsigned char *initialBGRA,
                                       char *log, int logSize)
{
    GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    BOOL resized = g_worker.fragmentWidth != width ||
                   g_worker.fragmentHeight != height;

    if (!g_worker.fragmentColor) g_worker.GenTextures(1, &g_worker.fragmentColor);
    if (!g_worker.fragmentFbo) g_worker.GenFramebuffers(1, &g_worker.fragmentFbo);
    if (!g_worker.fragmentDepthStencil)
        g_worker.GenRenderbuffers(1, &g_worker.fragmentDepthStencil);
    if (!g_worker.fragmentColor || !g_worker.fragmentFbo ||
        !g_worker.fragmentDepthStencil) {
        computeLog(log, logSize, "software fragment framebuffer allocation failed");
        return FALSE;
    }

    g_worker.ActiveTexture(GL_TEXTURE0);
    g_worker.BindTexture(GL_TEXTURE_2D, g_worker.fragmentColor);
    g_worker.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_worker.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_worker.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g_worker.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_worker.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_worker.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                        GL_BGRA, GL_UNSIGNED_BYTE, initialBGRA);

    g_worker.BindFramebuffer(GL_FRAMEBUFFER, g_worker.fragmentFbo);
    g_worker.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, g_worker.fragmentColor, 0);
    g_worker.BindRenderbuffer(GL_RENDERBUFFER, g_worker.fragmentDepthStencil);
    if (resized) {
        g_worker.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                     width, height);
        g_worker.fragmentWidth = width;
        g_worker.fragmentHeight = height;
    }
    g_worker.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     g_worker.fragmentDepthStencil);
    g_worker.DrawBuffers(1, &drawBuffer);
    g_worker.ReadBuffer(GL_COLOR_ATTACHMENT0);
    if (g_worker.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        computeLog(log, logSize, "software fragment framebuffer is incomplete");
        return FALSE;
    }
    if (resized) {
        g_worker.DepthMask(GL_TRUE);
        g_worker.StencilMaskSeparate(GL_FRONT, ~0u);
        g_worker.StencilMaskSeparate(GL_BACK, ~0u);
        g_worker.ClearDepth(1.0);
        g_worker.ClearStencil(0);
        g_worker.Disable(GL_SCISSOR_TEST);
        g_worker.Clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    return TRUE;
}

BOOL gldFragmentEmulatorDraw(GLS_Program *program, unsigned int mode,
                             int first, int count, unsigned int indexType,
                             const void *indices, int baseVertex,
                             int instanceCount, unsigned int baseInstance,
                             int width, int height,
                             const unsigned char *initialBGRA,
                             unsigned char *resultBGRA,
                             char *log, int logSize)
{
    GLS_State *state = glsGetState();
    GLS_VAO *vao = glsFindVAO(state->boundVAO);
    GLuint mesaProgram;
    GLenum error;
    int i;

    if (!program || !program->softwareFragmentExecution || !initialBGRA ||
        !resultBGRA || width <= 0 || height <= 0 || count <= 0 ||
        instanceCount <= 0) {
        computeLog(log, logSize, "invalid software fragment draw");
        return FALSE;
    }
    mesaProgram = g_worker.graphicsPrograms[program->id];
    if (!mesaProgram && !gldStageEmulatorLinkGraphics(program, log, logSize))
        return FALSE;
    mesaProgram = g_worker.graphicsPrograms[program->id];
    if (!computeMakeCurrent()) {
        computeLog(log, logSize, "software fragment worker unavailable");
        return FALSE;
    }
    if (!fragmentPrepareFramebuffer(width, height, initialBGRA, log, logSize))
        goto fail;

    stageConfigureVertexInput(state);
    for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
        computeMirrorBinding(GL_UNIFORM_BUFFER, (GLuint)i,
                             &state->uniformBindings[i]);
        computeMirrorBinding(GL_SHADER_STORAGE_BUFFER, (GLuint)i,
                             &state->shaderStorageBindings[i]);
        computeMirrorBinding(GL_ATOMIC_COUNTER_BUFFER, (GLuint)i,
                             &state->atomicCounterBindings[i]);
    }
    computeMirrorTextures(state);
    computeMirrorSamplers(state);
    computeMirrorImages(state);
    g_worker.BindFramebuffer(GL_FRAMEBUFFER, g_worker.fragmentFbo);
    fragmentApplyState(state);
    g_worker.UseProgram(mesaProgram);
    computeUploadUniformsTo(mesaProgram, program);
    if (program->tessControlShader || program->tessEvalShader) {
        g_worker.PatchParameteri(GL_PATCH_VERTICES, state->patchVertices);
        g_worker.PatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL,
                                  state->patchDefaultOuter);
        g_worker.PatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL,
                                  state->patchDefaultInner);
    }
    while (g_worker.GetError() != GL_NO_ERROR) { }

    if (indexType != 0) {
        GLuint elementName = vao ? vao->elementBuffer : state->boundElementBuffer;
        GLS_Buffer *element = glsFindBuffer(elementName);
        if (element) stageMirrorBuffer(GL_ELEMENT_ARRAY_BUFFER, element);
        else g_worker.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (instanceCount > 1 || baseInstance)
            g_worker.DrawElementsInstancedBaseVertexBaseInstance(
                mode, count, indexType, indices, instanceCount,
                baseVertex, baseInstance);
        else
            g_worker.DrawElementsBaseVertex(mode, count, indexType,
                                            indices, baseVertex);
    } else {
        if (instanceCount > 1 || baseInstance)
            g_worker.DrawArraysInstancedBaseInstance(mode, first, count,
                                                      instanceCount,
                                                      baseInstance);
        else
            g_worker.DrawArrays(mode, first, count);
    }
    g_worker.MemoryBarrier(GL_ALL_BARRIER_BITS);
    g_worker.Finish();
    g_worker.PixelStorei(GL_PACK_ALIGNMENT, 1);
    g_worker.ReadBuffer(GL_COLOR_ATTACHMENT0);
    g_worker.ReadPixels(0, 0, width, height, GL_BGRA,
                        GL_UNSIGNED_BYTE, resultBGRA);
    error = g_worker.GetError();
    if (error != GL_NO_ERROR) {
        char message[128];
        _snprintf(message, sizeof(message),
                  "software fragment draw failed with GL error 0x%04X", error);
        message[sizeof(message) - 1] = '\0';
        computeLog(log, logSize, message);
        goto fail;
    }

    computeReadImages(state);
    for (i = 0; i < GLS_MAX_BUFFER_BINDINGS; ++i) {
        computeReadBinding(GL_SHADER_STORAGE_BUFFER,
                           &state->shaderStorageBindings[i]);
        computeReadBinding(GL_ATOMIC_COUNTER_BUFFER,
                           &state->atomicCounterBindings[i]);
    }
    g_worker.UseProgram(0);
    g_worker.BindVertexArray(0);
    g_worker.BindFramebuffer(GL_FRAMEBUFFER, 0);
    computeReleaseCurrent();
    computeLog(log, logSize, "");
    return TRUE;

fail:
    if (g_worker.UseProgram) g_worker.UseProgram(0);
    if (g_worker.BindVertexArray) g_worker.BindVertexArray(0);
    if (g_worker.BindFramebuffer) g_worker.BindFramebuffer(GL_FRAMEBUFFER, 0);
    computeReleaseCurrent();
    if (log && logSize > 0 && !log[0])
        computeLog(log, logSize, "software fragment draw failed");
    return FALSE;
}

void gldFragmentEmulatorClear(unsigned int mask, float depth, int stencil)
{
    GLS_State *state = glsGetState();
    GLbitfield workerMask = mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (!workerMask || !g_worker.initialized || !g_worker.fragmentFbo ||
        !computeMakeCurrent()) return;
    g_worker.BindFramebuffer(GL_FRAMEBUFFER, g_worker.fragmentFbo);
    if (state->enableScissorTest) {
        g_worker.Enable(GL_SCISSOR_TEST);
        g_worker.Scissor(state->scissorX, state->scissorY,
                         state->scissorW, state->scissorH);
    } else {
        g_worker.Disable(GL_SCISSOR_TEST);
    }
    g_worker.DepthMask(state->depthMask);
    g_worker.StencilMaskSeparate(GL_FRONT, state->stencilWriteMask);
    g_worker.StencilMaskSeparate(GL_BACK, state->stencilBackWriteMask);
    g_worker.ClearDepth(depth);
    g_worker.ClearStencil(stencil);
    g_worker.Clear(workerMask);
    g_worker.BindFramebuffer(GL_FRAMEBUFFER, 0);
    computeReleaseCurrent();
}

void gldStageEmulatorFreeDraw(GLD_StageDraw *result)
{
    if (!result) return;
    free(result->vertices);
    memset(result, 0, sizeof(*result));
}

void gldComputeEmulatorShutdown(void)
{
    int i;
    if (g_worker.initialized && g_worker.wglMakeCurrent &&
        g_worker.wglMakeCurrent(g_worker.dc, g_worker.context)) {
        for (i = 0; i < GLS_MAX_PROGRAMS; ++i)
            if (g_worker.programs[i]) g_worker.DeleteProgram(g_worker.programs[i]);
        for (i = 0; i < GLS_MAX_PROGRAMS; ++i)
            if (g_worker.graphicsPrograms[i])
                g_worker.DeleteProgram(g_worker.graphicsPrograms[i]);
        for (i = 0; i < GLS_MAX_BUFFERS; ++i)
            if (g_worker.buffers[i]) g_worker.DeleteBuffers(1, &g_worker.buffers[i]);
        for (i = 0; i < GLS_MAX_TEXTURES; ++i)
            if (g_worker.textures[i]) g_worker.DeleteTextures(1, &g_worker.textures[i]);
        for (i = 0; i < GLS_MAX_SAMPLERS; ++i)
            if (g_worker.samplers[i]) g_worker.DeleteSamplers(1, &g_worker.samplers[i]);
        if (g_worker.stageCaptureBuffer)
            g_worker.DeleteBuffers(1, &g_worker.stageCaptureBuffer);
        if (g_worker.stageVao)
            g_worker.DeleteVertexArrays(1, &g_worker.stageVao);
        if (g_worker.fragmentDepthStencil)
            g_worker.DeleteRenderbuffers(1, &g_worker.fragmentDepthStencil);
        if (g_worker.fragmentFbo)
            g_worker.DeleteFramebuffers(1, &g_worker.fragmentFbo);
        if (g_worker.fragmentColor)
            g_worker.DeleteTextures(1, &g_worker.fragmentColor);
        g_worker.wglMakeCurrent(NULL, NULL);
    }
    if (g_worker.context && g_worker.wglDeleteContext)
        g_worker.wglDeleteContext(g_worker.context);
    if (g_worker.dc && g_worker.window) ReleaseDC(g_worker.window, g_worker.dc);
    if (g_worker.window) DestroyWindow(g_worker.window);
    if (g_worker.lockReady) DeleteCriticalSection(&g_worker.lock);
    if (g_worker.module) FreeLibrary(g_worker.module);
    ZeroMemory(&g_worker, sizeof(g_worker));
}
