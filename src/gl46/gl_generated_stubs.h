/*********************************************************************************
*
*  gl_generated_stubs.h - exactly-typed entry points for every GL 1.0-4.6 name
*                         the wrapper did not already resolve.
*
*  GENERATED FILE - DO NOT EDIT BY HAND.
*      python tools/gen_gl_stubs.py
*  Inputs: tools/glmap.json, tools/gl_stub_classification.py,
*          tools/gl_dsa_mapping.py.  See tools/glmap.README.
*
*  823 entry points: 487 core names plus 336 extension aliases, every one with the
*  exact parameter list the Khronos registry gives it.  Before this file existed
*  gldGetProcAddress_GL46 answered these names with a no-op whose argument count
*  was guessed from the name, which corrupts the stack on x86 __stdcall whenever
*  the guess is wrong.
*
*  Every generated entry forwards to translator state, D3D9 work, or an
*  explicit emulation layer. Spec-defined discard hints are the only silent
*  operations; an unimplemented classification makes generation fail.
*
*********************************************************************************/

#ifndef GL_GENERATED_STUBS_H
#define GL_GENERATED_STUBS_H

#include <windows.h>
#include <string.h>
#include <glad/gl.h>
#include "gl_impl.h"
#include "gl_state.h"
#include "advanced_emulation.h"
#include "gl_modern_stubs.h"
#include "gld_diag.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Shared helpers for the generated bodies ===== */

/* A DSA texture command names the texture but not its target; the target is
 * whatever the texture was created with.  GLS_Texture records it. */
static GLenum _genDsaTexTarget(GLuint texture)
{
    GLS_Texture *t = glsFindTexture((GLuint_t)texture);
    return (t && t->target) ? (GLenum)t->target : (GLenum)GL_TEXTURE_2D;
}

static GLuint _genDsaTexBinding(GLenum target)
{
    GLS_State *s = glsGetState();
    int unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS)
        unit = 0;
    if (target == GL_TEXTURE_CUBE_MAP)
        return (GLuint)s->boundTextureCube[unit];
    if (target == GL_TEXTURE_3D)
        return (GLuint)s->boundTexture3D[unit];
    if (target == GL_TEXTURE_BUFFER)
        return (GLuint)s->boundTextureBuffer[unit];
    return (GLuint)s->boundTexture2D[unit];
}

/* glBindTextureUnit binds to an explicit unit rather than the active one. */
static void _genBindTextureUnit(GLuint unit, GLuint texture)
{
    GLS_State *s = glsGetState();
    GLenum prevUnit = (GLenum)s->activeTexUnit;
    GLenum tgt = _genDsaTexTarget(texture);
    _glsActiveTexture((unsigned int)(GL_TEXTURE0 + unit));
    _glsBindTexture((unsigned int)tgt, (unsigned int)texture);
    _glsActiveTexture((unsigned int)prevUnit);
}

static void _genBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                     int srcX0, int srcY0, int srcX1, int srcY1,
                                     int dstX0, int dstY0, int dstX1, int dstY1,
                                     unsigned int mask, unsigned int filter)
{
    GLS_State *s = glsGetState();
    GLuint_t prevRead = s->boundReadFBO;
    GLuint_t prevDraw = s->boundDrawFBO;
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)readFramebuffer);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)drawFramebuffer);
    _glsBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                        mask, filter);
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)prevRead);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)prevDraw);
}

/* glCreateTextures differs from glGenTextures in that the target is decided at
 * creation rather than at first bind, so record it now. */
static void _genCreateTextures(GLenum target, int n, unsigned int *textures)
{
    int i;
    _glsGenTextures(n, textures);
    if (!textures)
        return;
    for (i = 0; i < n; i++) {
        GLS_Texture *t = glsFindTexture((GLuint_t)textures[i]);
        if (t)
            t->target = (GLenum_t)target;
    }
}

static void _genCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer,
                                       ptrdiff_t readOffset, ptrdiff_t writeOffset,
                                       ptrdiff_t size)
{
    GLS_State *s = glsGetState();
    GLuint_t prevR = s->boundCopyReadBuffer;
    GLuint_t prevW = s->boundCopyWriteBuffer;
    s->boundCopyReadBuffer  = (GLuint_t)readBuffer;
    s->boundCopyWriteBuffer = (GLuint_t)writeBuffer;
    _glsCopyBufferSubData((unsigned int)GL_COPY_READ_BUFFER,
                          (unsigned int)GL_COPY_WRITE_BUFFER,
                          readOffset, writeOffset, size);
    s->boundCopyReadBuffer  = prevR;
    s->boundCopyWriteBuffer = prevW;
}

/* The scalar half of glGetVertexAttrib*: everything except
 * GL_CURRENT_VERTEX_ATTRIB, which is a vec4 and is handled by the caller. */
static GLint _genVaoAttribQuery(const GLS_VertexAttrib *a, GLenum pname)
{
    if (!a)
        return 0;
    switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:        return a->enabled ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:           return (GLint)a->size;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:         return (GLint)a->stride;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:           return (GLint)a->type;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:     return a->normalized ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:        return a->integer ? 1 : 0;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:        return (GLint)a->divisor;
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: return (GLint)a->bufferBinding;
    default:                                    return 0;
    }
}

static GLfloat _genSamplerParam(const GLS_Sampler *s, GLenum pname)
{
    if (!s)
        return 0.0f;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:   return (GLfloat)s->minFilter;
    case GL_TEXTURE_MAG_FILTER:   return (GLfloat)s->magFilter;
    case GL_TEXTURE_WRAP_S:       return (GLfloat)s->wrapS;
    case GL_TEXTURE_WRAP_T:       return (GLfloat)s->wrapT;
    case GL_TEXTURE_WRAP_R:       return (GLfloat)s->wrapR;
    case GL_TEXTURE_MIN_LOD:      return s->minLod;
    case GL_TEXTURE_MAX_LOD:      return s->maxLod;
    case GL_TEXTURE_LOD_BIAS:     return s->lodBias;
    case GL_TEXTURE_COMPARE_MODE: return (GLfloat)s->compareMode;
    case GL_TEXTURE_COMPARE_FUNC: return (GLfloat)s->compareFunc;
    case GL_TEXTURE_MAX_ANISOTROPY: return s->maxAnisotropy;
    default:                      return 0.0f;
    }
}

/* Uniform readback.  The wrapper keeps the last value written per location in
 * GLS_Program::uniforms, which is the only source glGetUniform* has. */
static int _genGetUniformValues(GLuint program, GLint location, float *out, int maxOut)
{
    GLS_Program *p = glsFindProgram((GLuint_t)program);
    int i, n;
    if (!p || !out || maxOut <= 0)
        return 0;
    for (i = 0; i < p->uniformCount && i < GLS_MAX_UNIFORMS; i++) {
        if (p->uniforms[i].location != location || !p->uniforms[i].set)
            continue;
        switch (p->uniforms[i].type) {
        case 0: case 1: n = 1;  break;
        case 2:         n = 2;  break;
        case 3:         n = 3;  break;
        case 4:         n = 4;  break;
        case 5:         n = 4;  break;
        case 6:         n = 9;  break;
        default:        n = 16; break;
        }
        if (n > maxOut)
            n = n > 16 ? 16 : maxOut;
        memcpy(out, p->uniforms[i].data, (size_t)n * sizeof(float));
        return n;
    }
    return 0;
}

static void _genD2F(float *dst, int cap, const GLdouble *src, int n)
{
    int i;
    if (!dst || !src)
        return;
    if (n > cap)
        n = cap;
    for (i = 0; i < n; i++)
        dst[i] = (float)src[i];
}

/*
 * GL_[UNSIGNED_]INT_2_10_10_10_REV unpack.
 *
 * Bit layout, least significant first: x[0..9] y[10..19] z[20..29] w[30..31].
 * The signed form is two's complement in 10 (and 2) bits, and GL clamps the
 * most negative representable value up to exactly -1.0 when normalising.
 */
static void _genUnpackP(GLenum type, GLuint packed, GLboolean normalized, float *out)
{
    int i;
    int raw[4];
    float scale[4];

    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;

    if (type == GL_INT_2_10_10_10_REV) {
        raw[0] = (int)((packed      ) & 0x3FFu);
        raw[1] = (int)((packed >> 10) & 0x3FFu);
        raw[2] = (int)((packed >> 20) & 0x3FFu);
        raw[3] = (int)((packed >> 30) & 0x3u);
        for (i = 0; i < 3; i++)
            if (raw[i] & 0x200) raw[i] -= 0x400;
        if (raw[3] & 0x2) raw[3] -= 0x4;
        scale[0] = scale[1] = scale[2] = 511.0f;
        scale[3] = 1.0f;
    } else if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        raw[0] = (int)((packed      ) & 0x3FFu);
        raw[1] = (int)((packed >> 10) & 0x3FFu);
        raw[2] = (int)((packed >> 20) & 0x3FFu);
        raw[3] = (int)((packed >> 30) & 0x3u);
        scale[0] = scale[1] = scale[2] = 1023.0f;
        scale[3] = 3.0f;
    } else {
        /* Not a packed type this entry point accepts. */
        return;
    }

    for (i = 0; i < 4; i++) {
        if (normalized) {
            out[i] = (float)raw[i] / scale[i];
            if (out[i] < -1.0f)
                out[i] = -1.0f;
        } else {
            out[i] = (float)raw[i];
        }
    }
}

/* ===== Generated entry points ===== */

static void APIENTRY _gen_glActiveShaderProgram(GLuint pipeline, GLuint program)
{
    (void)pipeline; (void)program;
    gldAdvActiveShaderProgram(pipeline, program);
}

static void APIENTRY _gen_glBeginQueryIndexed(GLenum target, GLuint index, GLuint id)
{
    (void)target; (void)index; (void)id;
    { if (index != 0) return; _glsBeginQuery((unsigned int)target, (unsigned int)id); }
}

static void APIENTRY _gen_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
    (void)target; (void)first; (void)count; (void)buffers;
    { GLsizei _genI; for (_genI = 0; _genI < count; _genI++) _glsBindBufferBase((unsigned int)target, (unsigned int)((GLsizei)first + _genI), (unsigned int)(buffers ? buffers[_genI] : 0)); }
}

static void APIENTRY _gen_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes)
{
    (void)target; (void)first; (void)count; (void)buffers; (void)offsets; (void)sizes;
    { GLsizei _genI; for (_genI = 0; _genI < count; _genI++) _glsBindBufferRange((unsigned int)target, (unsigned int)((GLsizei)first + _genI), (unsigned int)(buffers ? buffers[_genI] : 0), (ptrdiff_t)(offsets ? offsets[_genI] : 0), (ptrdiff_t)(sizes ? sizes[_genI] : 0)); }
}

static void APIENTRY _gen_glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name)
{
    (void)program; (void)color; (void)name;
    gldAdvBindFragDataLocation(program, color, name);
}

static void APIENTRY _gen_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar *name)
{
    (void)program; (void)colorNumber; (void)index; (void)name;
    gldAdvBindFragDataLocationIndexed(program, colorNumber, index, name);
}

static void APIENTRY _gen_glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
    (void)first; (void)count; (void)textures;
    { GLsizei _genI; for (_genI = 0; _genI < count; _genI++) _glsBindImageTexture((unsigned int)((GLsizei)first + _genI), (unsigned int)(textures ? textures[_genI] : 0), 0, 0, 0, (unsigned int)GL_READ_WRITE, (unsigned int)GL_RGBA8); }
}

static void APIENTRY _gen_glBindProgramPipeline(GLuint pipeline)
{
    (void)pipeline;
    gldAdvBindProgramPipeline(pipeline);
}

static void APIENTRY _gen_glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers)
{
    (void)first; (void)count; (void)samplers;
    { GLsizei _genI; for (_genI = 0; _genI < count; _genI++) _glsBindSampler((unsigned int)((GLsizei)first + _genI), (unsigned int)(samplers ? samplers[_genI] : 0)); }
}

static void APIENTRY _gen_glBindTextureUnit(GLuint unit, GLuint texture)
{
    (void)unit; (void)texture;
    _genBindTextureUnit((GLuint)unit, (GLuint)texture);
}

static void APIENTRY _gen_glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
    (void)first; (void)count; (void)textures;
    { GLsizei _genI; for (_genI = 0; _genI < count; _genI++) _genBindTextureUnit((GLuint)((GLsizei)first + _genI), (GLuint)(textures ? textures[_genI] : 0)); }
}

static void APIENTRY _gen_glBindTransformFeedback(GLenum target, GLuint id)
{
    (void)target; (void)id;
    gldAdvBindTransformFeedback(target, id);
}

static void APIENTRY _gen_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    (void)bindingindex; (void)buffer; (void)offset; (void)stride;
    gldAdvBindVertexBuffer(bindingindex, buffer, offset, stride);
}

static void APIENTRY _gen_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
    (void)first; (void)count; (void)buffers; (void)offsets; (void)strides;
    gldAdvBindVertexBuffers(first, count, buffers, offsets, strides);
}

static void APIENTRY _gen_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    (void)buf; (void)modeRGB; (void)modeAlpha;
    { if (buf != 0) return; _glsBlendEquationSeparate((unsigned int)modeRGB, (unsigned int)modeAlpha); }
}

static void APIENTRY _gen_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    (void)buf; (void)srcRGB; (void)dstRGB; (void)srcAlpha; (void)dstAlpha;
    { if (buf != 0) return; _glsBlendFuncSeparate((unsigned int)srcRGB, (unsigned int)dstRGB, (unsigned int)srcAlpha, (unsigned int)dstAlpha); }
}

static void APIENTRY _gen_glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    (void)readFramebuffer; (void)drawFramebuffer; (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1; (void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1; (void)mask; (void)filter;
    _genBlitNamedFramebuffer((GLuint)readFramebuffer, (GLuint)drawFramebuffer, (int)srcX0, (int)srcY0, (int)srcX1, (int)srcY1, (int)dstX0, (int)dstY0, (int)dstX1, (int)dstY1, (unsigned int)mask, (unsigned int)filter);
}

static void APIENTRY _gen_glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
    (void)target; (void)size; (void)data; (void)flags;
    gldAdvBufferStorage(target, size, data, flags);
}

static GLenum APIENTRY _gen_glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    GLenum _genRet;
    (void)framebuffer; (void)target;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _genRet = (GLenum)_glsCheckFramebufferStatus((unsigned int)target);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
    return _genRet;
}

static void APIENTRY _gen_glClampColor(GLenum target, GLenum clamp)
{
    (void)target; (void)clamp;
    (void)target; (void)clamp;
}

static void APIENTRY _gen_glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    (void)target; (void)internalformat; (void)format; (void)type; (void)data;
    gldAdvClearBufferData(target, internalformat, format, type, data);
}

static void APIENTRY _gen_glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    (void)target; (void)internalformat; (void)offset; (void)size; (void)format; (void)type; (void)data;
    gldAdvClearBufferSubData(target, internalformat, offset, size, format, type, data);
}

static void APIENTRY _gen_glClearDepthf(GLfloat d)
{
    (void)d;
    _glsClearDepth((double)d);
}

static void APIENTRY _gen_glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)internalformat; (void)format; (void)type; (void)data;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    gldAdvClearBufferData((GLenum)GL_ARRAY_BUFFER, (GLenum)internalformat, (GLenum)format, (GLenum)type, data);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)internalformat; (void)offset; (void)size; (void)format; (void)type; (void)data;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    gldAdvClearBufferSubData((GLenum)GL_ARRAY_BUFFER, (GLenum)internalformat, (GLintptr)offset, (GLsizeiptr)size, (GLenum)format, (GLenum)type, data);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)buffer; (void)drawbuffer; (void)depth; (void)stencil;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsClearBufferfi((unsigned int)buffer, (int)drawbuffer, (float)depth, (int)stencil);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)buffer; (void)drawbuffer; (void)value;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsClearBufferfv((unsigned int)buffer, (int)drawbuffer, (const float *)value);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)buffer; (void)drawbuffer; (void)value;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsClearBufferiv((unsigned int)buffer, (int)drawbuffer, (const int *)value);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)buffer; (void)drawbuffer; (void)value;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsClearBufferuiv((unsigned int)buffer, (int)drawbuffer, (const unsigned int *)value);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void *data)
{
    (void)texture; (void)level; (void)format; (void)type; (void)data;
    gldAdvClearTexImage(texture, level, format, type, data);
}

static void APIENTRY _gen_glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)type; (void)data;
    gldAdvClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
}

static void APIENTRY _gen_glColorP3ui(GLenum type, GLuint color)
{
    float _genV[4];
    (void)type; (void)color;
    _genUnpackP((GLenum)type, (GLuint)color, (GLboolean)GL_TRUE, _genV);
    _glsColor4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glColorP3uiv(GLenum type, const GLuint *color)
{
    float _genV[4];
    (void)type; (void)color;
    if (!color) return;
    _genUnpackP((GLenum)type, (GLuint)color[0], (GLboolean)GL_TRUE, _genV);
    _glsColor4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glColorP4ui(GLenum type, GLuint color)
{
    float _genV[4];
    (void)type; (void)color;
    _genUnpackP((GLenum)type, (GLuint)color, (GLboolean)GL_TRUE, _genV);
    _glsColor4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glColorP4uiv(GLenum type, const GLuint *color)
{
    float _genV[4];
    (void)type; (void)color;
    if (!color) return;
    _genUnpackP((GLenum)type, (GLuint)color[0], (GLboolean)GL_TRUE, _genV);
    _glsColor4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)width; (void)format; (void)imageSize; (void)data;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCompressedTexSubImage1D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)width, (unsigned int)format, (int)imageSize, data);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height; (void)format; (void)imageSize; (void)data;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCompressedTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)width, (int)height, (unsigned int)format, (int)imageSize, data);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)imageSize; (void)data;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCompressedTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)width, (int)height, (int)depth, (unsigned int)format, (int)imageSize, data);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
    (void)srcName; (void)srcTarget; (void)srcLevel; (void)srcX; (void)srcY; (void)srcZ; (void)dstName; (void)dstTarget; (void)dstLevel; (void)dstX; (void)dstY; (void)dstZ; (void)srcWidth; (void)srcHeight; (void)srcDepth;
    gldAdvCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

static void APIENTRY _gen_glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    (void)readBuffer; (void)writeBuffer; (void)readOffset; (void)writeOffset; (void)size;
    _genCopyNamedBufferSubData((GLuint)readBuffer, (GLuint)writeBuffer, (ptrdiff_t)readOffset, (ptrdiff_t)writeOffset, (ptrdiff_t)size);
}

static void APIENTRY _gen_glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)x; (void)y; (void)width;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCopyTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, 0, (int)x, (int)y, (int)width, 1);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)x; (void)y; (void)width; (void)height;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCopyTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)x, (int)y, (int)width, (int)height);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)x; (void)y; (void)width; (void)height;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsCopyTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)x, (int)y, (int)width, (int)height);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glCreateBuffers(GLsizei n, GLuint *buffers)
{
    (void)n; (void)buffers;
    _glsGenBuffers((int)n, (unsigned int *)buffers);
}

static void APIENTRY _gen_glCreateFramebuffers(GLsizei n, GLuint *framebuffers)
{
    (void)n; (void)framebuffers;
    _glsGenFramebuffers((int)n, (unsigned int *)framebuffers);
}

static void APIENTRY _gen_glCreateProgramPipelines(GLsizei n, GLuint *pipelines)
{
    (void)n; (void)pipelines;
    gldAdvGenProgramPipelines((GLsizei)n, pipelines);
}

static void APIENTRY _gen_glCreateQueries(GLenum target, GLsizei n, GLuint *ids)
{
    (void)target; (void)n; (void)ids;
    _glsGenQueries((int)n, (unsigned int *)ids);
}

static void APIENTRY _gen_glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    (void)n; (void)renderbuffers;
    _glsGenRenderbuffers((int)n, (unsigned int *)renderbuffers);
}

static void APIENTRY _gen_glCreateSamplers(GLsizei n, GLuint *samplers)
{
    (void)n; (void)samplers;
    _glsGenSamplers((int)n, (unsigned int *)samplers);
}

static GLuint APIENTRY _gen_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const*strings)
{
    (void)type; (void)count; (void)strings;
    return (GLuint)gldAdvCreateShaderProgramv(type, count, strings);
}

static void APIENTRY _gen_glCreateTextures(GLenum target, GLsizei n, GLuint *textures)
{
    (void)target; (void)n; (void)textures;
    _genCreateTextures((GLenum)target, (int)n, (unsigned int *)textures);
}

static void APIENTRY _gen_glCreateTransformFeedbacks(GLsizei n, GLuint *ids)
{
    (void)n; (void)ids;
    gldAdvGenTransformFeedbacks((GLsizei)n, ids);
}

static void APIENTRY _gen_glCreateVertexArrays(GLsizei n, GLuint *arrays)
{
    (void)n; (void)arrays;
    _glsGenVertexArrays((int)n, (unsigned int *)arrays);
}

static void APIENTRY _gen_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
    (void)source; (void)type; (void)id; (void)severity; (void)length; (void)buf;
    gldAdvDebugMessageInsert(source, type, id, severity, length, buf);
}

static void APIENTRY _gen_glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
    (void)n; (void)pipelines;
    gldAdvDeleteProgramPipelines(n, pipelines);
}

static void APIENTRY _gen_glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
    (void)n; (void)ids;
    gldAdvDeleteTransformFeedbacks(n, ids);
}

static void APIENTRY _gen_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
    (void)first; (void)count; (void)v;
    { if (first != 0 || count < 1 || !v) return; _glsDepthRange((double)v[0], (double)v[1]); }
}

static void APIENTRY _gen_glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f)
{
    (void)index; (void)n; (void)f;
    { if (index != 0) return; _glsDepthRange((double)n, (double)f); }
}

static void APIENTRY _gen_glDepthRangef(GLfloat n, GLfloat f)
{
    (void)n; (void)f;
    _glsDepthRange((double)n, (double)f);
}

static void APIENTRY _gen_glDisableVertexArrayAttrib(GLuint vaobj, GLuint index)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)index;
    _glsBindVertexArray((unsigned int)vaobj);
    _glsDisableVertexAttribArray((unsigned int)index);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glDispatchComputeIndirect(GLintptr indirect)
{
    (void)indirect;
    gldAdvDispatchComputeIndirect(indirect);
}

static void APIENTRY _gen_glDrawArraysIndirect(GLenum mode, const void *indirect)
{
    (void)mode; (void)indirect;
    gldAdvDrawArraysIndirect(mode, indirect);
}

static void APIENTRY _gen_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
    (void)mode; (void)first; (void)count; (void)instancecount; (void)baseinstance;
    gldAdvDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);
}

static void APIENTRY _gen_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)basevertex;
    gldAdvDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

static void APIENTRY _gen_glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    (void)mode; (void)type; (void)indirect;
    gldAdvDrawElementsIndirect(mode, type, indirect);
}

static void APIENTRY _gen_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)instancecount; (void)baseinstance;
    gldAdvDrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);
}

static void APIENTRY _gen_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)instancecount; (void)basevertex;
    gldAdvDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
}

static void APIENTRY _gen_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)instancecount; (void)basevertex; (void)baseinstance;
    gldAdvDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);
}

static void APIENTRY _gen_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    (void)mode; (void)start; (void)end; (void)count; (void)type; (void)indices; (void)basevertex;
    gldAdvDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
}

static void APIENTRY _gen_glDrawTransformFeedback(GLenum mode, GLuint id)
{
    (void)mode; (void)id;
    gldAdvDrawTransformFeedback(mode, id);
}

static void APIENTRY _gen_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount)
{
    (void)mode; (void)id; (void)instancecount;
    gldAdvDrawTransformFeedbackInstanced(mode, id, instancecount);
}

static void APIENTRY _gen_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
    (void)mode; (void)id; (void)stream;
    gldAdvDrawTransformFeedbackStream(mode, id, stream);
}

static void APIENTRY _gen_glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
    (void)mode; (void)id; (void)stream; (void)instancecount;
    gldAdvDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);
}

static void APIENTRY _gen_glEnableVertexArrayAttrib(GLuint vaobj, GLuint index)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)index;
    _glsBindVertexArray((unsigned int)vaobj);
    _glsEnableVertexAttribArray((unsigned int)index);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glEndQueryIndexed(GLenum target, GLuint index)
{
    (void)target; (void)index;
    { if (index != 0) return; _glsEndQuery((unsigned int)target); }
}

static void APIENTRY _gen_glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)offset; (void)length;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsFlushMappedBufferRange((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)length);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    (void)target; (void)pname; (void)param;
    gldAdvFramebufferParameteri(target, pname, param);
}

static void APIENTRY _gen_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    (void)target; (void)attachment; (void)texture; (void)level; (void)layer;
    _glsFramebufferTexture((unsigned int)target, (unsigned int)attachment, (unsigned int)texture, (int)level);
}

static void APIENTRY _gen_glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
    (void)n; (void)pipelines;
    gldAdvGenProgramPipelines(n, pipelines);
}

static void APIENTRY _gen_glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
    (void)n; (void)ids;
    gldAdvGenTransformFeedbacks(n, ids);
}

static void APIENTRY _gen_glGenerateTextureMipmap(GLuint texture)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGenerateMipmap((unsigned int)_genTgt);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname, GLint *params)
{
    (void)program; (void)bufferIndex; (void)pname; (void)params;
    gldAdvGetActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
}

static void APIENTRY _gen_glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    (void)program; (void)shadertype; (void)index; (void)bufSize; (void)length; (void)name;
    gldAdvGetActiveSubroutineName(program, shadertype, index, bufSize, length, name);
}

static void APIENTRY _gen_glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    (void)program; (void)shadertype; (void)index; (void)bufSize; (void)length; (void)name;
    gldAdvGetActiveSubroutineUniformName(program, shadertype, index, bufSize, length, name);
}

static void APIENTRY _gen_glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values)
{
    (void)program; (void)shadertype; (void)index; (void)pname; (void)values;
    gldAdvGetActiveSubroutineUniformiv(program, shadertype, index, pname, values);
}

static void APIENTRY _gen_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)
{
    (void)program; (void)uniformBlockIndex; (void)bufSize; (void)length; (void)uniformBlockName;
    gldAdvGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

static void APIENTRY _gen_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params)
{
    (void)program; (void)uniformBlockIndex; (void)pname; (void)params;
    gldAdvGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

static void APIENTRY _gen_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName)
{
    (void)program; (void)uniformIndex; (void)bufSize; (void)length; (void)uniformName;
    gldAdvGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

static void APIENTRY _gen_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)
{
    (void)program; (void)uniformCount; (void)uniformIndices; (void)pname; (void)params;
    gldAdvGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

static void APIENTRY _gen_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    GLS_Program *_genP = glsFindProgram((GLuint_t)program);
    GLsizei _genN = 0;
    (void)program; (void)maxCount; (void)count; (void)shaders;
    if (_genP && shaders) {
        if (_genP->vertShader && _genN < maxCount)
            shaders[_genN++] = (GLuint)_genP->vertShader;
        if (_genP->fragShader && _genN < maxCount)
            shaders[_genN++] = (GLuint)_genP->fragShader;
    }
    if (count) *count = _genN;
}

static void APIENTRY _gen_glGetBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    unsigned char _genB[16];
    int _genI;
    (void)target; (void)index; (void)data;
    if (!data) return;
    memset(_genB, 0, sizeof(_genB));
    if (index == 0)
        _glsGetBooleanv((unsigned int)target, _genB);
    for (_genI = 0; _genI < 4; _genI++)
        data[_genI] = (GLboolean)_genB[_genI];
}

static void APIENTRY _gen_glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void *pixels)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)bufSize; (void)pixels;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGetCompressedTexImage((unsigned int)_genTgt, (int)level, pixels);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels)
{
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)bufSize; (void)pixels;
    gldAdvGetCompressedTextureSubImage((GLuint)texture, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLint)zoffset, (GLsizei)width, (GLsizei)height, (GLsizei)depth, (GLsizei)bufSize, pixels);
}

static GLuint APIENTRY _gen_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog)
{
    (void)count; (void)bufSize; (void)sources; (void)types; (void)ids; (void)severities; (void)lengths; (void)messageLog;
    return (GLuint)gldAdvGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

static void APIENTRY _gen_glGetDoublei_v(GLenum target, GLuint index, GLdouble *data)
{
    float _genB[16];
    int _genI;
    (void)target; (void)index; (void)data;
    if (!data) return;
    memset(_genB, 0, sizeof(_genB));
    if (index == 0)
        _glsGetFloatv((unsigned int)target, _genB);
    for (_genI = 0; _genI < 4; _genI++)
        data[_genI] = (GLdouble)_genB[_genI];
}

static void APIENTRY _gen_glGetFloati_v(GLenum target, GLuint index, GLfloat *data)
{
    float _genB[16];
    int _genI;
    (void)target; (void)index; (void)data;
    if (!data) return;
    memset(_genB, 0, sizeof(_genB));
    if (index == 0)
        _glsGetFloatv((unsigned int)target, _genB);
    for (_genI = 0; _genI < 4; _genI++)
        data[_genI] = (GLfloat)_genB[_genI];
}

static GLint APIENTRY _gen_glGetFragDataIndex(GLuint program, const GLchar *name)
{
    (void)program; (void)name;
    return (GLint)gldAdvGetFragDataIndex(program, name);
}

static GLint APIENTRY _gen_glGetFragDataLocation(GLuint program, const GLchar *name)
{
    (void)program; (void)name;
    return (GLint)gldAdvGetFragDataLocation(program, name);
}

static void APIENTRY _gen_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    (void)target; (void)pname; (void)params;
    (void)target;
    _glsGetIntegerv((unsigned int)pname, (int *)params);
}

static GLenum APIENTRY _gen_glGetGraphicsResetStatus(void)
{
    return (GLenum)gldAdvGetGraphicsResetStatus();
}

static void APIENTRY _gen_glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data)
{
    int _genB[16];
    int _genI;
    (void)target; (void)index; (void)data;
    if (!data) return;
    memset(_genB, 0, sizeof(_genB));
    if (index == 0)
        _glsGetIntegerv((unsigned int)target, _genB);
    for (_genI = 0; _genI < 4; _genI++)
        data[_genI] = (GLint64)_genB[_genI];
}

static void APIENTRY _gen_glGetIntegeri_v(GLenum target, GLuint index, GLint *data)
{
    int _genB[16];
    int _genI;
    (void)target; (void)index; (void)data;
    if (!data) return;
    memset(_genB, 0, sizeof(_genB));
    if (index == 0)
        _glsGetIntegerv((unsigned int)target, _genB);
    for (_genI = 0; _genI < 4; _genI++)
        data[_genI] = (GLint)_genB[_genI];
}

static void APIENTRY _gen_glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64 *params)
{
    (void)target; (void)internalformat; (void)pname; (void)count; (void)params;
    gldAdvGetInternalformati64v(target, internalformat, pname, count, params);
}

static void APIENTRY _gen_glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint *params)
{
    (void)target; (void)internalformat; (void)pname; (void)count; (void)params;
    gldAdvGetInternalformativ(target, internalformat, pname, count, params);
}

static void APIENTRY _gen_glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)pname; (void)params;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    { GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetBufferParameteriv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, _genV); if (params) params[0] = (GLint64)_genV[0]; }
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint *params)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)pname; (void)params;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsGetBufferParameteriv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, (int *)params);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void **params)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)pname; (void)params;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsGetBufferPointerv((unsigned int)GL_ARRAY_BUFFER, (unsigned int)pname, (void **)params);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)offset; (void)size; (void)data;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsGetBufferSubData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)size, data);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)attachment; (void)pname; (void)params;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _stub_glGetFramebufferAttachmentParameteriv((GLenum)GL_DRAW_FRAMEBUFFER, (GLenum)attachment, (GLenum)pname, (GLint *)params);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint *param)
{
    (void)framebuffer; (void)pname; (void)param;
    gldAdvGetNamedFramebufferParameteriv((GLuint)framebuffer, (GLenum)pname, (GLint *)param);
}

static void APIENTRY _gen_glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint *params)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundRBO;
    (void)renderbuffer; (void)pname; (void)params;
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)renderbuffer);
    _stub_glGetRenderbufferParameteriv((GLenum)GL_RENDERBUFFER, (GLenum)pname, (GLint *)params);
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    (void)identifier; (void)name; (void)bufSize; (void)length; (void)label;
    gldAdvGetObjectLabel(identifier, name, bufSize, length, label);
}

static void APIENTRY _gen_glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    (void)ptr; (void)bufSize; (void)length; (void)label;
    gldAdvGetObjectPtrLabel(ptr, bufSize, length, label);
}

static void APIENTRY _gen_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)
{
    (void)program; (void)bufSize; (void)length; (void)binaryFormat; (void)binary;
    gldAdvGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

static void APIENTRY _gen_glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
    (void)program; (void)programInterface; (void)pname; (void)params;
    gldAdvGetProgramInterfaceiv(program, programInterface, pname, params);
}

static void APIENTRY _gen_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    (void)pipeline; (void)bufSize; (void)length; (void)infoLog;
    gldAdvGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

static void APIENTRY _gen_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    (void)pipeline; (void)pname; (void)params;
    gldAdvGetProgramPipelineiv(pipeline, pname, params);
}

static GLuint APIENTRY _gen_glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
    (void)program; (void)programInterface; (void)name;
    return (GLuint)gldAdvGetProgramResourceIndex(program, programInterface, name);
}

static GLint APIENTRY _gen_glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar *name)
{
    (void)program; (void)programInterface; (void)name;
    return (GLint)gldAdvGetProgramResourceLocation(program, programInterface, name);
}

static GLint APIENTRY _gen_glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
    (void)program; (void)programInterface; (void)name;
    return (GLint)gldAdvGetProgramResourceLocationIndex(program, programInterface, name);
}

static void APIENTRY _gen_glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    (void)program; (void)programInterface; (void)index; (void)bufSize; (void)length; (void)name;
    gldAdvGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

static void APIENTRY _gen_glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params)
{
    (void)program; (void)programInterface; (void)index; (void)propCount; (void)props; (void)count; (void)length; (void)params;
    gldAdvGetProgramResourceiv(program, programInterface, index, propCount, props, count, length, params);
}

static void APIENTRY _gen_glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values)
{
    (void)program; (void)shadertype; (void)pname; (void)values;
    gldAdvGetProgramStageiv(program, shadertype, pname, values);
}

static void APIENTRY _gen_glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    (void)id; (void)buffer; (void)pname; (void)offset;
    gldAdvGetQueryBufferObjecti64v((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset);
}

static void APIENTRY _gen_glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    (void)id; (void)buffer; (void)pname; (void)offset;
    gldAdvGetQueryBufferObjectiv((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset);
}

static void APIENTRY _gen_glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    (void)id; (void)buffer; (void)pname; (void)offset;
    gldAdvGetQueryBufferObjectui64v((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset);
}

static void APIENTRY _gen_glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    (void)id; (void)buffer; (void)pname; (void)offset;
    gldAdvGetQueryBufferObjectuiv((GLuint)id, (GLuint)buffer, (GLenum)pname, (GLintptr)offset);
}

static void APIENTRY _gen_glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params)
{
    (void)target; (void)index; (void)pname; (void)params;
    { if (index != 0) return; _glsGetQueryiv((unsigned int)target, (unsigned int)pname, (int *)params); }
}

static void APIENTRY _gen_glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params)
{
    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);
    (void)sampler; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLint)_genSamplerParam(_genSmp, (GLenum)pname);
}

static void APIENTRY _gen_glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params)
{
    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);
    (void)sampler; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLuint)_genSamplerParam(_genSmp, (GLenum)pname);
}

static void APIENTRY _gen_glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);
    (void)sampler; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLfloat)_genSamplerParam(_genSmp, (GLenum)pname);
}

static void APIENTRY _gen_glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
    GLS_Sampler *_genSmp = glsFindSampler((GLuint_t)sampler);
    (void)sampler; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLint)_genSamplerParam(_genSmp, (GLenum)pname);
}

static void APIENTRY _gen_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)
{
    (void)shadertype; (void)precisiontype; (void)range; (void)precision;
    (void)shadertype; (void)precisiontype;
    if (range) { range[0] = 127; range[1] = 127; }
    if (precision) precision[0] = 23;
}

static void APIENTRY _gen_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
    GLS_Shader *_genSh = glsFindShader((GLuint_t)shader);
    GLsizei _genLen = 0;
    (void)shader; (void)bufSize; (void)length; (void)source;
    if (_genSh && _genSh->source && source && bufSize > 0) {
        strncpy(source, _genSh->source, (size_t)bufSize - 1);
        source[bufSize - 1] = '\0';
        _genLen = (GLsizei)strlen(source);
    } else if (source && bufSize > 0) {
        source[0] = '\0';
    }
    if (length) *length = _genLen;
}

static GLuint APIENTRY _gen_glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name)
{
    (void)program; (void)shadertype; (void)name;
    return (GLuint)gldAdvGetSubroutineIndex(program, shadertype, name);
}

static GLint APIENTRY _gen_glGetSubroutineUniformLocation(GLuint program, GLenum shadertype, const GLchar *name)
{
    (void)program; (void)shadertype; (void)name;
    return (GLint)gldAdvGetSubroutineUniformLocation(program, shadertype, name);
}

static void APIENTRY _gen_glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values)
{
    (void)sync; (void)pname; (void)count; (void)length; (void)values;
    gldAdvGetSynciv(sync, pname, count, length, values);
}

static void APIENTRY _gen_glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params)
{
    (void)target; (void)pname; (void)params;
    _glsGetTexParameteriv((unsigned int)target, (unsigned int)pname, (int *)params);
}

static void APIENTRY _gen_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params)
{
    GLint _genV[4];
    (void)target; (void)pname; (void)params;
    memset(_genV, 0, sizeof(_genV));
    _glsGetTexParameteriv((unsigned int)target, (unsigned int)pname, _genV);
    if (params) params[0] = (GLuint)_genV[0];
}

static void APIENTRY _gen_glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)format; (void)type; (void)bufSize; (void)pixels;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGetTexImage((unsigned int)_genTgt, (int)level, (unsigned int)format, (unsigned int)type, pixels);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    { GLint _genV[16]; memset(_genV, 0, sizeof(_genV)); _glsGetTexLevelParameteriv((unsigned int)_genTgt, (int)level, (unsigned int)pname, _genV); if (params) params[0] = (GLfloat)_genV[0]; }
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGetTexLevelParameteriv((unsigned int)_genTgt, (int)level, (unsigned int)pname, (int *)params);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, (int *)params);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    { GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, _genV); if (params) params[0] = (GLuint)_genV[0]; }
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    { GLint _genV[4]; memset(_genV, 0, sizeof(_genV)); _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, _genV); if (params) params[0] = (GLfloat)_genV[0]; }
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsGetTexParameteriv((unsigned int)_genTgt, (unsigned int)pname, (int *)params);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)type; (void)bufSize; (void)pixels;
    gldAdvGetTextureSubImage((GLuint)texture, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLint)zoffset, (GLsizei)width, (GLsizei)height, (GLsizei)depth, (GLenum)format, (GLenum)type, (GLsizei)bufSize, pixels);
}

static void APIENTRY _gen_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)
{
    (void)program; (void)index; (void)bufSize; (void)length; (void)size; (void)type; (void)name;
    gldAdvGetTransformFeedbackVarying((GLuint)program, (GLuint)index, (GLsizei)bufSize, length, size, type, name);
}

static void APIENTRY _gen_glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
    (void)xfb; (void)pname; (void)index; (void)param;
    gldAdvGetTransformFeedbacki64_v((GLuint)xfb, (GLenum)pname, (GLuint)index, param);
}

static void APIENTRY _gen_glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
    (void)xfb; (void)pname; (void)index; (void)param;
    gldAdvGetTransformFeedbacki_v((GLuint)xfb, (GLenum)pname, (GLuint)index, param);
}

static void APIENTRY _gen_glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param)
{
    (void)xfb; (void)pname; (void)param;
    gldAdvGetTransformFeedbackiv((GLuint)xfb, (GLenum)pname, param);
}

static void APIENTRY _gen_glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices)
{
    (void)program; (void)uniformCount; (void)uniformNames; (void)uniformIndices;
    gldAdvGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

static void APIENTRY _gen_glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params)
{
    (void)shadertype; (void)location; (void)params;
    gldAdvGetUniformSubroutineuiv(shadertype, location, params);
}

static void APIENTRY _gen_glGetUniformdv(GLuint program, GLint location, GLdouble *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLdouble)_genB[_genI];
}

static void APIENTRY _gen_glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLfloat)_genB[_genI];
}

static void APIENTRY _gen_glGetUniformiv(GLuint program, GLint location, GLint *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLint)_genB[_genI];
}

static void APIENTRY _gen_glGetUniformuiv(GLuint program, GLint location, GLuint *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLuint)_genB[_genI];
}

static void APIENTRY _gen_glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64 *param)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)index; (void)pname; (void)param;
    _glsBindVertexArray((unsigned int)vaobj);
    { GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint64)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS) ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0); }
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)index; (void)pname; (void)param;
    _glsBindVertexArray((unsigned int)vaobj);
    { GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS) ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0); }
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param)
{
    (void)vaobj; (void)pname; (void)param;
    { GLS_VAO *_genVao = glsFindVAO((GLuint_t)vaobj); if (param) param[0] = (GLint)((_genVao && pname == GL_ELEMENT_ARRAY_BUFFER_BINDING) ? _genVao->elementBuffer : 0); }
}

static void APIENTRY _gen_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
    GLS_VAO *_genVao = glsFindVAO(glsGetState()->boundVAO);
    (void)index; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLint)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS)
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
    GLS_VAO *_genVao = glsFindVAO(glsGetState()->boundVAO);
    (void)index; (void)pname; (void)params;
    if (!params) return;
    params[0] = (GLuint)((_genVao && index < GLS_MAX_VERTEX_ATTRIBS)
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params)
{
    GLS_State *_genS = glsGetState();
    GLS_VAO *_genVao = glsFindVAO(_genS->boundVAO);
    int _genI;
    (void)index; (void)pname; (void)params;
    if (!params || index >= GLS_MAX_VERTEX_ATTRIBS) return;
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        for (_genI = 0; _genI < 4; _genI++)
            params[_genI] = (GLdouble)(_genVao
                ? _genVao->attribs[index].defaultValue[_genI] : 0.0f);
        return;
    }
    params[0] = (GLdouble)(_genVao
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    GLS_VAO *_genVao = glsFindVAO(glsGetState()->boundVAO);
    (void)index; (void)pname; (void)pointer;
    if (!pointer) return;
    (void)pname;
    pointer[0] = (_genVao && index < GLS_MAX_VERTEX_ATTRIBS)
        ? (void *)_genVao->attribs[index].pointer : NULL;
}

static void APIENTRY _gen_glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params)
{
    GLS_State *_genS = glsGetState();
    GLS_VAO *_genVao = glsFindVAO(_genS->boundVAO);
    int _genI;
    (void)index; (void)pname; (void)params;
    if (!params || index >= GLS_MAX_VERTEX_ATTRIBS) return;
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        for (_genI = 0; _genI < 4; _genI++)
            params[_genI] = (GLdouble)(_genVao
                ? _genVao->attribs[index].defaultValue[_genI] : 0.0f);
        return;
    }
    params[0] = (GLdouble)(_genVao
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    GLS_State *_genS = glsGetState();
    GLS_VAO *_genVao = glsFindVAO(_genS->boundVAO);
    int _genI;
    (void)index; (void)pname; (void)params;
    if (!params || index >= GLS_MAX_VERTEX_ATTRIBS) return;
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        for (_genI = 0; _genI < 4; _genI++)
            params[_genI] = (GLfloat)(_genVao
                ? _genVao->attribs[index].defaultValue[_genI] : 0.0f);
        return;
    }
    params[0] = (GLfloat)(_genVao
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    GLS_State *_genS = glsGetState();
    GLS_VAO *_genVao = glsFindVAO(_genS->boundVAO);
    int _genI;
    (void)index; (void)pname; (void)params;
    if (!params || index >= GLS_MAX_VERTEX_ATTRIBS) return;
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        for (_genI = 0; _genI < 4; _genI++)
            params[_genI] = (GLint)(_genVao
                ? _genVao->attribs[index].defaultValue[_genI] : 0.0f);
        return;
    }
    params[0] = (GLint)(_genVao
        ? _genVaoAttribQuery(&_genVao->attribs[index], (GLenum)pname) : 0);
}

static void APIENTRY _gen_glGetnColorTable(GLenum target, GLenum format, GLenum type, GLsizei bufSize, void *table)
{
    (void)target; (void)format; (void)type; (void)bufSize; (void)table;
}

static void APIENTRY _gen_glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
    (void)target; (void)lod; (void)bufSize; (void)pixels;
    _glsGetCompressedTexImage((unsigned int)target, (int)lod, pixels);
}

static void APIENTRY _gen_glGetnConvolutionFilter(GLenum target, GLenum format, GLenum type, GLsizei bufSize, void *image)
{
    (void)target; (void)format; (void)type; (void)bufSize; (void)image;
}

static void APIENTRY _gen_glGetnHistogram(GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void *values)
{
    (void)target; (void)reset; (void)format; (void)type; (void)bufSize; (void)values;
}

static void APIENTRY _gen_glGetnMapdv(GLenum target, GLenum query, GLsizei bufSize, GLdouble *v)
{
    float _genB[16];
    int _genI;
    int _genN = (int)bufSize;
    (void)target; (void)query; (void)bufSize; (void)v;
    if (!v || _genN <= 0) return;
    if (_genN > 16) _genN = 16;
    memset(_genB, 0, sizeof(_genB));
    _glsGetMapfv((unsigned int)target, (unsigned int)query, _genB);
    for (_genI = 0; _genI < _genN; _genI++) v[_genI] = (GLdouble)_genB[_genI];
}

static void APIENTRY _gen_glGetnMapfv(GLenum target, GLenum query, GLsizei bufSize, GLfloat *v)
{
    (void)target; (void)query; (void)bufSize; (void)v;
    if (bufSize < 1) return;
    _glsGetMapfv((unsigned int)target, (unsigned int)query, (float *)v);
}

static void APIENTRY _gen_glGetnMapiv(GLenum target, GLenum query, GLsizei bufSize, GLint *v)
{
    (void)target; (void)query; (void)bufSize; (void)v;
    if (bufSize < 1) return;
    _glsGetMapiv((unsigned int)target, (unsigned int)query, (int *)v);
}

static void APIENTRY _gen_glGetnMinmax(GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void *values)
{
    (void)target; (void)reset; (void)format; (void)type; (void)bufSize; (void)values;
}

static void APIENTRY _gen_glGetnPixelMapfv(GLenum map, GLsizei bufSize, GLfloat *values)
{
    (void)map; (void)bufSize; (void)values;
    if (bufSize < 32) return;
    _glsGetPixelMapfv((unsigned int)map, (float *)values);
}

static void APIENTRY _gen_glGetnPixelMapuiv(GLenum map, GLsizei bufSize, GLuint *values)
{
    (void)map; (void)bufSize; (void)values;
    if (bufSize < 32) return;
    _glsGetPixelMapuiv((unsigned int)map, (unsigned int *)values);
}

static void APIENTRY _gen_glGetnPixelMapusv(GLenum map, GLsizei bufSize, GLushort *values)
{
    (void)map; (void)bufSize; (void)values;
    if (bufSize < 32) return;
    _glsGetPixelMapusv((unsigned int)map, (unsigned short *)values);
}

static void APIENTRY _gen_glGetnPolygonStipple(GLsizei bufSize, GLubyte *pattern)
{
    (void)bufSize; (void)pattern;
    if (bufSize < 128) return;
    _glsGetPolygonStipple((unsigned char *)pattern);
}

static void APIENTRY _gen_glGetnSeparableFilter(GLenum target, GLenum format, GLenum type, GLsizei rowBufSize, void *row, GLsizei columnBufSize, void *column, void *span)
{
    (void)target; (void)format; (void)type; (void)rowBufSize; (void)row; (void)columnBufSize; (void)column; (void)span;
}

static void APIENTRY _gen_glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    (void)target; (void)level; (void)format; (void)type; (void)bufSize; (void)pixels;
    _glsGetTexImage((unsigned int)target, (int)level, (unsigned int)format, (unsigned int)type, pixels);
}

static void APIENTRY _gen_glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)bufSize; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    if (_genN > (int)(bufSize / (GLsizei)sizeof(GLdouble)))
        _genN = (int)(bufSize / (GLsizei)sizeof(GLdouble));
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLdouble)_genB[_genI];
}

static void APIENTRY _gen_glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)bufSize; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    if (_genN > (int)(bufSize / (GLsizei)sizeof(GLfloat)))
        _genN = (int)(bufSize / (GLsizei)sizeof(GLfloat));
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLfloat)_genB[_genI];
}

static void APIENTRY _gen_glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)bufSize; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    if (_genN > (int)(bufSize / (GLsizei)sizeof(GLint)))
        _genN = (int)(bufSize / (GLsizei)sizeof(GLint));
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLint)_genB[_genI];
}

static void APIENTRY _gen_glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
    float _genB[16];
    int _genI, _genN;
    (void)program; (void)location; (void)bufSize; (void)params;
    if (!params) return;
    _genN = _genGetUniformValues((GLuint)program, (GLint)location, _genB, 16);
    if (_genN > (int)(bufSize / (GLsizei)sizeof(GLuint)))
        _genN = (int)(bufSize / (GLsizei)sizeof(GLuint));
    for (_genI = 0; _genI < _genN; _genI++)
        params[_genI] = (GLuint)_genB[_genI];
}

static void APIENTRY _gen_glInvalidateBufferData(GLuint buffer)
{
    (void)buffer;
}

static void APIENTRY _gen_glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    (void)buffer; (void)offset; (void)length;
}

static void APIENTRY _gen_glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    (void)target; (void)numAttachments; (void)attachments;
}

static void APIENTRY _gen_glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments)
{
    (void)framebuffer; (void)numAttachments; (void)attachments;
}

static void APIENTRY _gen_glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    (void)framebuffer; (void)numAttachments; (void)attachments; (void)x; (void)y; (void)width; (void)height;
}

static void APIENTRY _gen_glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    (void)target; (void)numAttachments; (void)attachments; (void)x; (void)y; (void)width; (void)height;
}

static void APIENTRY _gen_glInvalidateTexImage(GLuint texture, GLint level)
{
    (void)texture; (void)level;
}

static void APIENTRY _gen_glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth)
{
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth;
}

static GLboolean APIENTRY _gen_glIsEnabledi(GLenum target, GLuint index)
{
    (void)target; (void)index;
    if (index != 0) return GL_FALSE;
    return (GLboolean)_glsIsEnabled((unsigned int)target);
}

static GLboolean APIENTRY _gen_glIsProgramPipeline(GLuint pipeline)
{
    (void)pipeline;
    return (GLboolean)gldAdvIsProgramPipeline(pipeline);
}

static GLboolean APIENTRY _gen_glIsQuery(GLuint id)
{
    (void)id;
    return glsFindQuery((GLuint_t)id) ? GL_TRUE : GL_FALSE;
}

static GLboolean APIENTRY _gen_glIsSampler(GLuint sampler)
{
    (void)sampler;
    return glsFindSampler((GLuint_t)sampler) ? GL_TRUE : GL_FALSE;
}

static GLboolean APIENTRY _gen_glIsSync(GLsync sync)
{
    (void)sync;
    return sync ? GL_TRUE : GL_FALSE;
}

static GLboolean APIENTRY _gen_glIsTransformFeedback(GLuint id)
{
    (void)id;
    return (GLboolean)gldAdvIsTransformFeedback(id);
}

static void * APIENTRY _gen_glMapNamedBuffer(GLuint buffer, GLenum access)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    void * _genRet;
    (void)buffer; (void)access;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _genRet = (void *)_glsMapBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)access);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
    return _genRet;
}

static void * APIENTRY _gen_glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    void * _genRet;
    (void)buffer; (void)offset; (void)length; (void)access;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _genRet = (void *)_glsMapBufferRange((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)length, (unsigned int)access);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
    return _genRet;
}

static void APIENTRY _gen_glMemoryBarrierByRegion(GLbitfield barriers)
{
    (void)barriers;
    _glsMemoryBarrier((unsigned int)barriers);
}

static void APIENTRY _gen_glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    (void)mode; (void)indirect; (void)drawcount; (void)stride;
    gldAdvMultiDrawArraysIndirect(mode, indirect, drawcount, stride);
}

static void APIENTRY _gen_glMultiDrawArraysIndirectCount(GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    (void)mode; (void)indirect; (void)drawcount; (void)maxdrawcount; (void)stride;
    gldAdvMultiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
}

static void APIENTRY _gen_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)drawcount; (void)basevertex;
    gldAdvMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
}

static void APIENTRY _gen_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    (void)mode; (void)type; (void)indirect; (void)drawcount; (void)stride;
    gldAdvMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
}

static void APIENTRY _gen_glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    (void)mode; (void)type; (void)indirect; (void)drawcount; (void)maxdrawcount; (void)stride;
    gldAdvMultiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}

static void APIENTRY _gen_glMultiTexCoordP1ui(GLenum texture, GLenum type, GLuint coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP1uiv(GLenum texture, GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP2ui(GLenum texture, GLenum type, GLuint coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP2uiv(GLenum texture, GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP3ui(GLenum texture, GLenum type, GLuint coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP3uiv(GLenum texture, GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glMultiTexCoordP4ui(GLenum texture, GLenum type, GLuint coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glMultiTexCoordP4uiv(GLenum texture, GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)texture; (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsMultiTexCoord4fARB((unsigned int)texture, _genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)size; (void)data; (void)usage;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsBufferData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)size, data, (unsigned int)usage);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)size; (void)data; (void)flags;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsBufferData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)size, data, (unsigned int)GL_STATIC_DRAW);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    (void)buffer; (void)offset; (void)size; (void)data;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _glsBufferSubData((unsigned int)GL_ARRAY_BUFFER, (ptrdiff_t)offset, (ptrdiff_t)size, data);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)buf;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsDrawBuffer((unsigned int)buf);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)n; (void)bufs;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsDrawBuffers((int)n, (const unsigned int *)bufs);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)pname; (void)param;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    gldAdvFramebufferParameteri((GLenum)GL_DRAW_FRAMEBUFFER, (GLenum)pname, (GLint)param);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundReadFBO;
    (void)framebuffer; (void)src;
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsReadBuffer((unsigned int)src);
    _glsBindFramebuffer((unsigned int)GL_READ_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)attachment; (void)renderbuffertarget; (void)renderbuffer;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsFramebufferRenderbuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)renderbuffertarget, (unsigned int)renderbuffer);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)attachment; (void)texture; (void)level;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsFramebufferTexture((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)texture, (int)level);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundDrawFBO;
    (void)framebuffer; (void)attachment; (void)texture; (void)level; (void)layer;
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)framebuffer);
    _glsFramebufferTexture((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)attachment, (unsigned int)texture, (int)level);
    _glsBindFramebuffer((unsigned int)GL_DRAW_FRAMEBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundRBO;
    (void)renderbuffer; (void)internalformat; (void)width; (void)height;
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)renderbuffer);
    _glsRenderbufferStorage((unsigned int)GL_RENDERBUFFER, (unsigned int)internalformat, (int)width, (int)height);
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundRBO;
    (void)renderbuffer; (void)samples; (void)internalformat; (void)width; (void)height;
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)renderbuffer);
    _glsRenderbufferStorageMultisample((unsigned int)GL_RENDERBUFFER, (int)samples, (unsigned int)internalformat, (int)width, (int)height);
    _glsBindRenderbuffer((unsigned int)GL_RENDERBUFFER, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glNormalP3ui(GLenum type, GLuint coords)
{
    float _genV[4];
    (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsNormal3f(_genV[0], _genV[1], _genV[2]);
}

static void APIENTRY _gen_glNormalP3uiv(GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsNormal3f(_genV[0], _genV[1], _genV[2]);
}

static void APIENTRY _gen_glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    (void)ptr; (void)length; (void)label;
    gldAdvObjectPtrLabel(ptr, length, label);
}

static void APIENTRY _gen_glPatchParameterfv(GLenum pname, const GLfloat *values)
{
    (void)pname; (void)values;
    gldAdvPatchParameterfv(pname, values);
}

static void APIENTRY _gen_glPauseTransformFeedback(void)
{
    gldAdvPauseTransformFeedback();
}

static void APIENTRY _gen_glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp)
{
    (void)factor; (void)units; (void)clamp;
    _glsPolygonOffset((float)factor, (float)units);
}

static void APIENTRY _gen_glPopDebugGroup(void)
{
    gldAdvPopDebugGroup();
}

static void APIENTRY _gen_glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    (void)program; (void)binaryFormat; (void)binary; (void)length;
    gldAdvProgramBinary(program, binaryFormat, binary, length);
}

static void APIENTRY _gen_glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    (void)program; (void)pname; (void)value;
    gldAdvProgramParameteri(program, pname, value);
}

static void APIENTRY _gen_glProgramUniform1d(GLuint program, GLint location, GLdouble v0)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0;
    _glsUseProgram((unsigned int)program);
    _glsUniform1f((int)location, (float)v0);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1dv(GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[256];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 256) _genN = 256;
    _genD2F(_genB, 256, value, _genN * 1);
    _glsUniform1fv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1f(GLuint program, GLint location, GLfloat v0)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0;
    _glsUseProgram((unsigned int)program);
    _glsUniform1f((int)location, (float)v0);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform1fv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0;
    _glsUseProgram((unsigned int)program);
    _glsUniform1i((int)location, (int)v0);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform1iv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1ui(GLuint program, GLint location, GLuint v0)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0;
    _glsUseProgram((unsigned int)program);
    _glsUniform1i((int)location, (int)v0);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 256) _genN = 256;
    for (_genI = 0; _genI < _genN * 1; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform1iv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1;
    _glsUseProgram((unsigned int)program);
    _glsUniform2f((int)location, (float)v0, (float)v1);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2dv(GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[256];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 128) _genN = 128;
    _genD2F(_genB, 256, value, _genN * 2);
    _glsUniform2fv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1;
    _glsUseProgram((unsigned int)program);
    _glsUniform2f((int)location, (float)v0, (float)v1);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform2fv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1;
    _glsUseProgram((unsigned int)program);
    _glsUniform2i((int)location, (int)v0, (int)v1);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform2iv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1;
    _glsUseProgram((unsigned int)program);
    _glsUniform2i((int)location, (int)v0, (int)v1);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 128) _genN = 128;
    for (_genI = 0; _genI < _genN * 2; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform2iv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2;
    _glsUseProgram((unsigned int)program);
    _glsUniform3f((int)location, (float)v0, (float)v1, (float)v2);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3dv(GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[256];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 85) _genN = 85;
    _genD2F(_genB, 256, value, _genN * 3);
    _glsUniform3fv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2;
    _glsUseProgram((unsigned int)program);
    _glsUniform3f((int)location, (float)v0, (float)v1, (float)v2);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform3fv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2;
    _glsUseProgram((unsigned int)program);
    _glsUniform3i((int)location, (int)v0, (int)v1, (int)v2);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform3iv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2;
    _glsUseProgram((unsigned int)program);
    _glsUniform3i((int)location, (int)v0, (int)v1, (int)v2);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 85) _genN = 85;
    for (_genI = 0; _genI < _genN * 3; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform3iv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2; (void)v3;
    _glsUseProgram((unsigned int)program);
    _glsUniform4f((int)location, (float)v0, (float)v1, (float)v2, (float)v3);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4dv(GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[256];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 64) _genN = 64;
    _genD2F(_genB, 256, value, _genN * 4);
    _glsUniform4fv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2; (void)v3;
    _glsUseProgram((unsigned int)program);
    _glsUniform4f((int)location, (float)v0, (float)v1, (float)v2, (float)v3);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform4fv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2; (void)v3;
    _glsUseProgram((unsigned int)program);
    _glsUniform4i((int)location, (int)v0, (int)v1, (int)v2, (int)v3);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniform4iv((int)location, (int)count, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)v0; (void)v1; (void)v2; (void)v3;
    _glsUseProgram((unsigned int)program);
    _glsUniform4i((int)location, (int)v0, (int)v1, (int)v2, (int)v3);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 64) _genN = 64;
    for (_genI = 0; _genI < _genN * 4; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform4iv((int)location, _genN, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 16) _genN = 16;
    _genD2F(_genB, 64, value, _genN * 4);
    _glsUniformMatrix2fv((int)location, _genN, (unsigned char)transpose, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniformMatrix2fv((int)location, (int)count, (unsigned char)transpose, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 2 + _genC]
                  : value[_genM * 6 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 2 + _genC]
                  : value[_genM * 6 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 2 + _genC]
                  : value[_genM * 8 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 2 + _genC]
                  : value[_genM * 8 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 7) _genN = 7;
    _genD2F(_genB, 64, value, _genN * 9);
    _glsUniformMatrix3fv((int)location, _genN, (unsigned char)transpose, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniformMatrix3fv((int)location, (int)count, (unsigned char)transpose, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 3 + _genC]
                  : value[_genM * 6 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 3 + _genC]
                  : value[_genM * 6 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 3 + _genC]
                  : value[_genM * 12 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 3 + _genC]
                  : value[_genM * 12 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 4) _genN = 4;
    _genD2F(_genB, 64, value, _genN * 16);
    _glsUniformMatrix4fv((int)location, _genN, (unsigned char)transpose, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    _glsUseProgram((unsigned int)program);
    _glsUniformMatrix4fv((int)location, (int)count, (unsigned char)transpose, value);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 4 + _genC]
                  : value[_genM * 8 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 4 + _genC]
                  : value[_genM * 8 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 4 + _genC]
                  : value[_genM * 12 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrevProg = _genS->boundProgram;
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)program; (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    _glsUseProgram((unsigned int)program);
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 4 + _genC]
                  : value[_genM * 12 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
    _glsUseProgram((unsigned int)_genPrevProg);
}

static void APIENTRY _gen_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    (void)source; (void)id; (void)length; (void)message;
    gldAdvPushDebugGroup(source, id, length, message);
}

static void APIENTRY _gen_glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)
{
    (void)x; (void)y; (void)width; (void)height; (void)format; (void)type; (void)bufSize; (void)data;
    _glsReadPixels((int)x, (int)y, (int)width, (int)height, (unsigned int)format, (unsigned int)type, data);
}

static void APIENTRY _gen_glReleaseShaderCompiler(void)
{
}

static void APIENTRY _gen_glResumeTransformFeedback(void)
{
    gldAdvResumeTransformFeedback();
}

static void APIENTRY _gen_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *param)
{
    (void)sampler; (void)pname; (void)param;
    _glsSamplerParameteri((unsigned int)sampler, (unsigned int)pname, param ? (int)param[0] : 0);
}

static void APIENTRY _gen_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *param)
{
    (void)sampler; (void)pname; (void)param;
    _glsSamplerParameteri((unsigned int)sampler, (unsigned int)pname, param ? (int)param[0] : 0);
}

static void APIENTRY _gen_glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
    (void)first; (void)count; (void)v;
    { if (first != 0 || count < 1 || !v) return; _glsScissor((int)v[0], (int)v[1], (int)v[2], (int)v[3]); }
}

static void APIENTRY _gen_glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    (void)index; (void)left; (void)bottom; (void)width; (void)height;
    { if (index != 0) return; _glsScissor((int)left, (int)bottom, (int)width, (int)height); }
}

static void APIENTRY _gen_glScissorIndexedv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    { if (index != 0 || !v) return; _glsScissor((int)v[0], (int)v[1], (int)v[2], (int)v[3]); }
}

static void APIENTRY _gen_glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f(((float)red / 127.0f < -1.0f ? -1.0f : (float)red / 127.0f), ((float)green / 127.0f < -1.0f ? -1.0f : (float)green / 127.0f), ((float)blue / 127.0f < -1.0f ? -1.0f : (float)blue / 127.0f));
}

static void APIENTRY _gen_glSecondaryColor3bv(const GLbyte *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f(((float)v[0] / 127.0f < -1.0f ? -1.0f : (float)v[0] / 127.0f), ((float)v[1] / 127.0f < -1.0f ? -1.0f : (float)v[1] / 127.0f), ((float)v[2] / 127.0f < -1.0f ? -1.0f : (float)v[2] / 127.0f));
}

static void APIENTRY _gen_glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f((float)red, (float)green, (float)blue);
}

static void APIENTRY _gen_glSecondaryColor3dv(const GLdouble *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f((float)v[0], (float)v[1], (float)v[2]);
}

static void APIENTRY _gen_glSecondaryColor3i(GLint red, GLint green, GLint blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f(((float)red / 2147483647.0f < -1.0f ? -1.0f : (float)red / 2147483647.0f), ((float)green / 2147483647.0f < -1.0f ? -1.0f : (float)green / 2147483647.0f), ((float)blue / 2147483647.0f < -1.0f ? -1.0f : (float)blue / 2147483647.0f));
}

static void APIENTRY _gen_glSecondaryColor3iv(const GLint *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f(((float)v[0] / 2147483647.0f < -1.0f ? -1.0f : (float)v[0] / 2147483647.0f), ((float)v[1] / 2147483647.0f < -1.0f ? -1.0f : (float)v[1] / 2147483647.0f), ((float)v[2] / 2147483647.0f < -1.0f ? -1.0f : (float)v[2] / 2147483647.0f));
}

static void APIENTRY _gen_glSecondaryColor3s(GLshort red, GLshort green, GLshort blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f(((float)red / 32767.0f < -1.0f ? -1.0f : (float)red / 32767.0f), ((float)green / 32767.0f < -1.0f ? -1.0f : (float)green / 32767.0f), ((float)blue / 32767.0f < -1.0f ? -1.0f : (float)blue / 32767.0f));
}

static void APIENTRY _gen_glSecondaryColor3sv(const GLshort *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f(((float)v[0] / 32767.0f < -1.0f ? -1.0f : (float)v[0] / 32767.0f), ((float)v[1] / 32767.0f < -1.0f ? -1.0f : (float)v[1] / 32767.0f), ((float)v[2] / 32767.0f < -1.0f ? -1.0f : (float)v[2] / 32767.0f));
}

static void APIENTRY _gen_glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f((float)red / 4294967295.0f, (float)green / 4294967295.0f, (float)blue / 4294967295.0f);
}

static void APIENTRY _gen_glSecondaryColor3uiv(const GLuint *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f((float)v[0] / 4294967295.0f, (float)v[1] / 4294967295.0f, (float)v[2] / 4294967295.0f);
}

static void APIENTRY _gen_glSecondaryColor3us(GLushort red, GLushort green, GLushort blue)
{
    (void)red; (void)green; (void)blue;
    _glsSecondaryColor3f((float)red / 65535.0f, (float)green / 65535.0f, (float)blue / 65535.0f);
}

static void APIENTRY _gen_glSecondaryColor3usv(const GLushort *v)
{
    (void)v;
    if (!v) return;
    _glsSecondaryColor3f((float)v[0] / 65535.0f, (float)v[1] / 65535.0f, (float)v[2] / 65535.0f);
}

static void APIENTRY _gen_glSecondaryColorP3ui(GLenum type, GLuint color)
{
    float _genV[4];
    (void)type; (void)color;
    _genUnpackP((GLenum)type, (GLuint)color, (GLboolean)GL_TRUE, _genV);
    _glsSecondaryColor3f(_genV[0], _genV[1], _genV[2]);
}

static void APIENTRY _gen_glSecondaryColorP3uiv(GLenum type, const GLuint *color)
{
    float _genV[4];
    (void)type; (void)color;
    if (!color) return;
    _genUnpackP((GLenum)type, (GLuint)color[0], (GLboolean)GL_TRUE, _genV);
    _glsSecondaryColor3f(_genV[0], _genV[1], _genV[2]);
}

static void APIENTRY _gen_glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)
{
    (void)count; (void)shaders; (void)binaryFormat; (void)binary; (void)length;
    gldAdvShaderBinary(count, shaders, binaryFormat, binary, length);
}

static void APIENTRY _gen_glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding)
{
    (void)program; (void)storageBlockIndex; (void)storageBlockBinding;
    gldAdvShaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding);
}

static void APIENTRY _gen_glSpecializeShader(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
    (void)shader; (void)pEntryPoint; (void)numSpecializationConstants; (void)pConstantIndex; (void)pConstantValue;
    gldAdvSpecializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

static void APIENTRY _gen_glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    (void)target; (void)internalformat; (void)buffer; (void)offset; (void)size;
    _glsTexBufferRange((unsigned int)target, (unsigned int)internalformat, (unsigned int)buffer, (ptrdiff_t)offset, (ptrdiff_t)size);
}

static void APIENTRY _gen_glTexCoordP1ui(GLenum type, GLuint coords)
{
    float _genV[4];
    (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glTexCoordP1uiv(GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glTexCoordP2ui(GLenum type, GLuint coords)
{
    float _genV[4];
    (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glTexCoordP2uiv(GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glTexCoordP3ui(GLenum type, GLuint coords)
{
    float _genV[4];
    (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glTexCoordP3uiv(GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glTexCoordP4ui(GLenum type, GLuint coords)
{
    float _genV[4];
    (void)type; (void)coords;
    _genUnpackP((GLenum)type, (GLuint)coords, (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glTexCoordP4uiv(GLenum type, const GLuint *coords)
{
    float _genV[4];
    (void)type; (void)coords;
    if (!coords) return;
    _genUnpackP((GLenum)type, (GLuint)coords[0], (GLboolean)GL_TRUE, _genV);
    _glsTexCoord4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glTexParameterIiv(GLenum target, GLenum pname, const GLint *params)
{
    (void)target; (void)pname; (void)params;
    _glsTexParameteri((unsigned int)target, (unsigned int)pname, params ? (int)params[0] : 0);
}

static void APIENTRY _gen_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params)
{
    (void)target; (void)pname; (void)params;
    _glsTexParameteri((unsigned int)target, (unsigned int)pname, params ? (int)params[0] : 0);
}

static void APIENTRY _gen_glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    (void)target; (void)levels; (void)internalformat; (void)width;
    _glsTexStorage2D((unsigned int)target, (int)levels, (unsigned int)internalformat, (int)width, 1);
}

static void APIENTRY _gen_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    (void)target; (void)samples; (void)internalformat; (void)width; (void)height; (void)fixedsamplelocations;
    _glsTexStorage2D((unsigned int)target, 1, (unsigned int)internalformat, (int)width, (int)height);
}

static void APIENTRY _gen_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    (void)target; (void)samples; (void)internalformat; (void)width; (void)height; (void)depth; (void)fixedsamplelocations;
    _glsTexStorage3D((unsigned int)target, 1, (unsigned int)internalformat, (int)width, (int)height, (int)depth);
}

static void APIENTRY _gen_glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)internalformat; (void)buffer;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexBuffer((unsigned int)_genTgt, (unsigned int)internalformat, (unsigned int)buffer);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)internalformat; (void)buffer; (void)offset; (void)size;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexBuffer((unsigned int)_genTgt, (unsigned int)internalformat, (unsigned int)buffer);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, params ? (int)params[0] : 0);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)params;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, params ? (int)params[0] : 0);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameterf(GLuint texture, GLenum pname, GLfloat param)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)param;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameterf((unsigned int)_genTgt, (unsigned int)pname, (float)param);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *param)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)param;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameterf((unsigned int)_genTgt, (unsigned int)pname, param ? (float)param[0] : 0.0f);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameteri(GLuint texture, GLenum pname, GLint param)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)param;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, (int)param);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureParameteriv(GLuint texture, GLenum pname, const GLint *param)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)pname; (void)param;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexParameteri((unsigned int)_genTgt, (unsigned int)pname, param ? (int)param[0] : 0);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)levels; (void)internalformat; (void)width;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexStorage2D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, 1);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)levels; (void)internalformat; (void)width; (void)height;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexStorage2D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, (int)height);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)samples; (void)internalformat; (void)width; (void)height; (void)fixedsamplelocations;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexStorage2D((unsigned int)_genTgt, 1, (unsigned int)internalformat, (int)width, (int)height);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)levels; (void)internalformat; (void)width; (void)height; (void)depth;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexStorage3D((unsigned int)_genTgt, (int)levels, (unsigned int)internalformat, (int)width, (int)height, (int)depth);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)samples; (void)internalformat; (void)width; (void)height; (void)depth; (void)fixedsamplelocations;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexStorage3D((unsigned int)_genTgt, 1, (unsigned int)internalformat, (int)width, (int)height, (int)depth);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)width; (void)format; (void)type; (void)pixels;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexSubImage1D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)width, (unsigned int)format, (unsigned int)type, pixels);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height; (void)format; (void)type; (void)pixels;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexSubImage2D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)width, (int)height, (unsigned int)format, (unsigned int)type, pixels);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    GLenum _genTgt = _genDsaTexTarget((GLuint)texture);
    GLuint _genPrev = _genDsaTexBinding(_genTgt);
    (void)texture; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)type; (void)pixels;
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)texture);
    _glsTexSubImage3D((unsigned int)_genTgt, (int)level, (int)xoffset, (int)yoffset, (int)zoffset, (int)width, (int)height, (int)depth, (unsigned int)format, (unsigned int)type, pixels);
    _glsBindTexture((unsigned int)_genTgt, (unsigned int)_genPrev);
}

static void APIENTRY _gen_glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
    (void)texture; (void)target; (void)origtexture; (void)internalformat; (void)minlevel; (void)numlevels; (void)minlayer; (void)numlayers;
    gldAdvTextureView((GLuint)texture, (GLenum)target, (GLuint)origtexture, (GLenum)internalformat, (GLuint)minlevel, (GLuint)numlevels, (GLuint)minlayer, (GLuint)numlayers);
}

static void APIENTRY _gen_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
    (void)xfb; (void)index; (void)buffer;
    gldAdvTransformFeedbackBufferBase((GLuint)xfb, (GLuint)index, (GLuint)buffer);
}

static void APIENTRY _gen_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    (void)xfb; (void)index; (void)buffer; (void)offset; (void)size;
    gldAdvTransformFeedbackBufferRange((GLuint)xfb, (GLuint)index, (GLuint)buffer, (GLintptr)offset, (GLsizeiptr)size);
}

static void APIENTRY _gen_glUniform1d(GLint location, GLdouble x)
{
    (void)location; (void)x;
    _glsUniform1f((int)location, (float)x);
}

static void APIENTRY _gen_glUniform1dv(GLint location, GLsizei count, const GLdouble *value)
{
    float _genB[256];
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 256) _genN = 256;
    _genD2F(_genB, 256, value, _genN * 1);
    _glsUniform1fv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform1ui(GLint location, GLuint v0)
{
    (void)location; (void)v0;
    _glsUniform1i((int)location, (int)v0);
}

static void APIENTRY _gen_glUniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 256) _genN = 256;
    for (_genI = 0; _genI < _genN * 1; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform1iv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform2d(GLint location, GLdouble x, GLdouble y)
{
    (void)location; (void)x; (void)y;
    _glsUniform2f((int)location, (float)x, (float)y);
}

static void APIENTRY _gen_glUniform2dv(GLint location, GLsizei count, const GLdouble *value)
{
    float _genB[256];
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 128) _genN = 128;
    _genD2F(_genB, 256, value, _genN * 2);
    _glsUniform2fv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
    (void)location; (void)v0; (void)v1;
    _glsUniform2i((int)location, (int)v0, (int)v1);
}

static void APIENTRY _gen_glUniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 128) _genN = 128;
    for (_genI = 0; _genI < _genN * 2; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform2iv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform3d(GLint location, GLdouble x, GLdouble y, GLdouble z)
{
    (void)location; (void)x; (void)y; (void)z;
    _glsUniform3f((int)location, (float)x, (float)y, (float)z);
}

static void APIENTRY _gen_glUniform3dv(GLint location, GLsizei count, const GLdouble *value)
{
    float _genB[256];
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 85) _genN = 85;
    _genD2F(_genB, 256, value, _genN * 3);
    _glsUniform3fv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    (void)location; (void)v0; (void)v1; (void)v2;
    _glsUniform3i((int)location, (int)v0, (int)v1, (int)v2);
}

static void APIENTRY _gen_glUniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 85) _genN = 85;
    for (_genI = 0; _genI < _genN * 3; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform3iv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform4d(GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    (void)location; (void)x; (void)y; (void)z; (void)w;
    _glsUniform4f((int)location, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glUniform4dv(GLint location, GLsizei count, const GLdouble *value)
{
    float _genB[256];
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 64) _genN = 64;
    _genD2F(_genB, 256, value, _genN * 4);
    _glsUniform4fv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    (void)location; (void)v0; (void)v1; (void)v2; (void)v3;
    _glsUniform4i((int)location, (int)v0, (int)v1, (int)v2, (int)v3);
}

static void APIENTRY _gen_glUniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
    int _genB[256];
    int _genI;
    int _genN = (int)count;
    (void)location; (void)count; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 64) _genN = 64;
    for (_genI = 0; _genI < _genN * 4; _genI++)
        _genB[_genI] = (int)value[_genI];
    _glsUniform4iv((int)location, _genN, _genB);
}

static void APIENTRY _gen_glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 16) _genN = 16;
    _genD2F(_genB, 64, value, _genN * 4);
    _glsUniformMatrix2fv((int)location, _genN, (unsigned char)transpose, _genB);
}

static void APIENTRY _gen_glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 2 + _genC]
                  : value[_genM * 6 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
}

static void APIENTRY _gen_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 2 + _genC]
                  : value[_genM * 6 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
}

static void APIENTRY _gen_glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 2 + _genC]
                  : value[_genM * 8 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
}

static void APIENTRY _gen_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 8) _genN = 8;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 2; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 2 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 2 + _genC]
                  : value[_genM * 8 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 2, _genB);
}

static void APIENTRY _gen_glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 7) _genN = 7;
    _genD2F(_genB, 64, value, _genN * 9);
    _glsUniformMatrix3fv((int)location, _genN, (unsigned char)transpose, _genB);
}

static void APIENTRY _gen_glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 3 + _genC]
                  : value[_genM * 6 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
}

static void APIENTRY _gen_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 6 + _genR * 3 + _genC]
                  : value[_genM * 6 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
}

static void APIENTRY _gen_glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 3 + _genC]
                  : value[_genM * 12 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
}

static void APIENTRY _gen_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 5) _genN = 5;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 3; _genC++)
            for (_genR = 0; _genR < 4; _genR++)
                _genB[(_genM * 3 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 3 + _genC]
                  : value[_genM * 12 + _genC * 4 + _genR]);
    _glsUniform4fv((int)location, _genN * 3, _genB);
}

static void APIENTRY _gen_glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 4) _genN = 4;
    _genD2F(_genB, 64, value, _genN * 16);
    _glsUniformMatrix4fv((int)location, _genN, (unsigned char)transpose, _genB);
}

static void APIENTRY _gen_glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 4 + _genC]
                  : value[_genM * 8 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
}

static void APIENTRY _gen_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 2; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 8 + _genR * 4 + _genC]
                  : value[_genM * 8 + _genC * 2 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
}

static void APIENTRY _gen_glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 4 + _genC]
                  : value[_genM * 12 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
}

static void APIENTRY _gen_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    float _genB[64];
    int _genM, _genC, _genR;
    int _genN = (int)count;
    (void)location; (void)count; (void)transpose; (void)value;
    if (!value || _genN <= 0) return;
    if (_genN > 4) _genN = 4;
    memset(_genB, 0, sizeof(_genB));
    for (_genM = 0; _genM < _genN; _genM++)
        for (_genC = 0; _genC < 4; _genC++)
            for (_genR = 0; _genR < 3; _genR++)
                _genB[(_genM * 4 + _genC) * 4 + _genR] = (float)(transpose ?
                    value[_genM * 12 + _genR * 4 + _genC]
                  : value[_genM * 12 + _genC * 3 + _genR]);
    _glsUniform4fv((int)location, _genN * 4, _genB);
}

static void APIENTRY _gen_glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices)
{
    (void)shadertype; (void)count; (void)indices;
    gldAdvUniformSubroutinesuiv(shadertype, count, indices);
}

static GLboolean APIENTRY _gen_glUnmapNamedBuffer(GLuint buffer)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundArrayBuffer;
    GLboolean _genRet;
    (void)buffer;
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)buffer);
    _genRet = (GLboolean)_glsUnmapBuffer((unsigned int)GL_ARRAY_BUFFER);
    _glsBindBuffer((unsigned int)GL_ARRAY_BUFFER, (unsigned int)_genPrev);
    return _genRet;
}

static void APIENTRY _gen_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    (void)pipeline; (void)stages; (void)program;
    gldAdvUseProgramStages(pipeline, stages, program);
}

static void APIENTRY _gen_glValidateProgramPipeline(GLuint pipeline)
{
    (void)pipeline;
    gldAdvValidateProgramPipeline(pipeline);
}

static void APIENTRY _gen_glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)attribindex; (void)bindingindex;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvVertexAttribBinding((GLuint)attribindex, (GLuint)bindingindex);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)attribindex; (void)size; (void)type; (void)normalized; (void)relativeoffset;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvVertexAttribFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLboolean)normalized, (GLuint)relativeoffset);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)attribindex; (void)size; (void)type; (void)relativeoffset;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvVertexAttribIFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLuint)relativeoffset);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)attribindex; (void)size; (void)type; (void)relativeoffset;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvVertexAttribLFormat((GLuint)attribindex, (GLint)size, (GLenum)type, (GLuint)relativeoffset);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)bindingindex; (void)divisor;
    _glsBindVertexArray((unsigned int)vaobj);
    _glsVertexAttribDivisor((unsigned int)bindingindex, (unsigned int)divisor);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)buffer;
    _glsBindVertexArray((unsigned int)vaobj);
    _glsBindBuffer((unsigned int)GL_ELEMENT_ARRAY_BUFFER, (unsigned int)buffer);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)bindingindex; (void)buffer; (void)offset; (void)stride;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvBindVertexBuffer((GLuint)bindingindex, (GLuint)buffer, (GLintptr)offset, (GLsizei)stride);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
    GLS_State *_genS = glsGetState();
    GLuint_t _genPrev = _genS->boundVAO;
    (void)vaobj; (void)first; (void)count; (void)buffers; (void)offsets; (void)strides;
    _glsBindVertexArray((unsigned int)vaobj);
    gldAdvBindVertexBuffers((GLuint)first, (GLsizei)count, buffers, offsets, strides);
    _glsBindVertexArray((unsigned int)_genPrev);
}

static void APIENTRY _gen_glVertexAttrib1d(GLuint index, GLdouble x)
{
    (void)index; (void)x;
    _glsVertexAttrib4f((unsigned int)index, (float)x, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib1dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib1fv(GLuint index, const GLfloat *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib1s(GLuint index, GLshort x)
{
    (void)index; (void)x;
    _glsVertexAttrib4f((unsigned int)index, (float)x, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib1sv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y)
{
    (void)index; (void)x; (void)y;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib2dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib2fv(GLuint index, const GLfloat *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib2s(GLuint index, GLshort x, GLshort y)
{
    (void)index; (void)x; (void)y;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib2sv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    (void)index; (void)x; (void)y; (void)z;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib3dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttrib3fv(GLuint index, const GLfloat *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z)
{
    (void)index; (void)x; (void)y; (void)z;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, 1.0f);
}

static void APIENTRY _gen_glVertexAttrib3sv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttrib4Nbv(GLuint index, const GLbyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, ((float)v[0] / 127.0f < -1.0f ? -1.0f : (float)v[0] / 127.0f), ((float)v[1] / 127.0f < -1.0f ? -1.0f : (float)v[1] / 127.0f), ((float)v[2] / 127.0f < -1.0f ? -1.0f : (float)v[2] / 127.0f), ((float)v[3] / 127.0f < -1.0f ? -1.0f : (float)v[3] / 127.0f));
}

static void APIENTRY _gen_glVertexAttrib4Niv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, ((float)v[0] / 2147483647.0f < -1.0f ? -1.0f : (float)v[0] / 2147483647.0f), ((float)v[1] / 2147483647.0f < -1.0f ? -1.0f : (float)v[1] / 2147483647.0f), ((float)v[2] / 2147483647.0f < -1.0f ? -1.0f : (float)v[2] / 2147483647.0f), ((float)v[3] / 2147483647.0f < -1.0f ? -1.0f : (float)v[3] / 2147483647.0f));
}

static void APIENTRY _gen_glVertexAttrib4Nsv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, ((float)v[0] / 32767.0f < -1.0f ? -1.0f : (float)v[0] / 32767.0f), ((float)v[1] / 32767.0f < -1.0f ? -1.0f : (float)v[1] / 32767.0f), ((float)v[2] / 32767.0f < -1.0f ? -1.0f : (float)v[2] / 32767.0f), ((float)v[3] / 32767.0f < -1.0f ? -1.0f : (float)v[3] / 32767.0f));
}

static void APIENTRY _gen_glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x / 255.0f, (float)y / 255.0f, (float)z / 255.0f, (float)w / 255.0f);
}

static void APIENTRY _gen_glVertexAttrib4Nubv(GLuint index, const GLubyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0] / 255.0f, (float)v[1] / 255.0f, (float)v[2] / 255.0f, (float)v[3] / 255.0f);
}

static void APIENTRY _gen_glVertexAttrib4Nuiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0] / 4294967295.0f, (float)v[1] / 4294967295.0f, (float)v[2] / 4294967295.0f, (float)v[3] / 4294967295.0f);
}

static void APIENTRY _gen_glVertexAttrib4Nusv(GLuint index, const GLushort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0] / 65535.0f, (float)v[1] / 65535.0f, (float)v[2] / 65535.0f, (float)v[3] / 65535.0f);
}

static void APIENTRY _gen_glVertexAttrib4bv(GLuint index, const GLbyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glVertexAttrib4dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4fv(GLuint index, const GLfloat *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4iv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glVertexAttrib4sv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4ubv(GLuint index, const GLubyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4uiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttrib4usv(GLuint index, const GLushort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
    (void)attribindex; (void)bindingindex;
    gldAdvVertexAttribBinding(attribindex, bindingindex);
}

static void APIENTRY _gen_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    (void)attribindex; (void)size; (void)type; (void)normalized; (void)relativeoffset;
    gldAdvVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

static void APIENTRY _gen_glVertexAttribI1i(GLuint index, GLint x)
{
    (void)index; (void)x;
    _glsVertexAttrib4f((unsigned int)index, (float)x, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI1iv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI1ui(GLuint index, GLuint x)
{
    (void)index; (void)x;
    _glsVertexAttrib4f((unsigned int)index, (float)x, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI1uiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI2i(GLuint index, GLint x, GLint y)
{
    (void)index; (void)x; (void)y;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI2iv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI2ui(GLuint index, GLuint x, GLuint y)
{
    (void)index; (void)x; (void)y;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI2uiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z)
{
    (void)index; (void)x; (void)y; (void)z;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI3iv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z)
{
    (void)index; (void)x; (void)y; (void)z;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, 1.0f);
}

static void APIENTRY _gen_glVertexAttribI3uiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttribI4bv(GLuint index, const GLbyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glVertexAttribI4iv(GLuint index, const GLint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribI4sv(GLuint index, const GLshort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribI4ubv(GLuint index, const GLubyte *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glVertexAttribI4uiv(GLuint index, const GLuint *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribI4usv(GLuint index, const GLushort *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    (void)attribindex; (void)size; (void)type; (void)relativeoffset;
    gldAdvVertexAttribIFormat(attribindex, size, type, relativeoffset);
}

static void APIENTRY _gen_glVertexAttribL1d(GLuint index, GLdouble x)
{
    (void)index; (void)x;
    _glsVertexAttrib4f((unsigned int)index, (float)x, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribL1dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y)
{
    (void)index; (void)x; (void)y;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribL2dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    (void)index; (void)x; (void)y; (void)z;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, 1.0f);
}

static void APIENTRY _gen_glVertexAttribL3dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    (void)index; (void)x; (void)y; (void)z; (void)w;
    _glsVertexAttrib4f((unsigned int)index, (float)x, (float)y, (float)z, (float)w);
}

static void APIENTRY _gen_glVertexAttribL4dv(GLuint index, const GLdouble *v)
{
    (void)index; (void)v;
    if (!v) return;
    _glsVertexAttrib4f((unsigned int)index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
}

static void APIENTRY _gen_glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    (void)attribindex; (void)size; (void)type; (void)relativeoffset;
    gldAdvVertexAttribLFormat(attribindex, size, type, relativeoffset);
}

static void APIENTRY _gen_glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    (void)index; (void)size; (void)type; (void)stride; (void)pointer;
    gldAdvVertexAttribLPointer(index, size, type, stride, pointer);
}

static void APIENTRY _gen_glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], 0.0f, 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    float _genV[4];
    (void)index; (void)type; (void)normalized; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)normalized, _genV);
    _glsVertexAttrib4f((unsigned int)index, _genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
    (void)bindingindex; (void)divisor;
    gldAdvVertexBindingDivisor(bindingindex, divisor);
}

static void APIENTRY _gen_glVertexP2ui(GLenum type, GLuint value)
{
    float _genV[4];
    (void)type; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexP2uiv(GLenum type, const GLuint *value)
{
    float _genV[4];
    (void)type; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], 0.0f, 1.0f);
}

static void APIENTRY _gen_glVertexP3ui(GLenum type, GLuint value)
{
    float _genV[4];
    (void)type; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glVertexP3uiv(GLenum type, const GLuint *value)
{
    float _genV[4];
    (void)type; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], _genV[2], 1.0f);
}

static void APIENTRY _gen_glVertexP4ui(GLenum type, GLuint value)
{
    float _genV[4];
    (void)type; (void)value;
    _genUnpackP((GLenum)type, (GLuint)value, (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glVertexP4uiv(GLenum type, const GLuint *value)
{
    float _genV[4];
    (void)type; (void)value;
    if (!value) return;
    _genUnpackP((GLenum)type, (GLuint)value[0], (GLboolean)GL_TRUE, _genV);
    _glsVertex4f(_genV[0], _genV[1], _genV[2], _genV[3]);
}

static void APIENTRY _gen_glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v)
{
    (void)first; (void)count; (void)v;
    { if (first != 0 || count < 1 || !v) return; _glsViewport((int)v[0], (int)v[1], (int)v[2], (int)v[3]); }
}

static void APIENTRY _gen_glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
    (void)index; (void)x; (void)y; (void)w; (void)h;
    { if (index != 0) return; _glsViewport((int)x, (int)y, (int)w, (int)h); }
}

static void APIENTRY _gen_glViewportIndexedfv(GLuint index, const GLfloat *v)
{
    (void)index; (void)v;
    { if (index != 0 || !v) return; _glsViewport((int)v[0], (int)v[1], (int)v[2], (int)v[3]); }
}

static void APIENTRY _gen_glWindowPos2d(GLdouble x, GLdouble y)
{
    (void)x; (void)y;
    _glsWindowPos3f((float)x, (float)y, 0.0f);
}

static void APIENTRY _gen_glWindowPos2dv(const GLdouble *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], 0.0f);
}

static void APIENTRY _gen_glWindowPos2f(GLfloat x, GLfloat y)
{
    (void)x; (void)y;
    _glsWindowPos3f((float)x, (float)y, 0.0f);
}

static void APIENTRY _gen_glWindowPos2fv(const GLfloat *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], 0.0f);
}

static void APIENTRY _gen_glWindowPos2i(GLint x, GLint y)
{
    (void)x; (void)y;
    _glsWindowPos3f((float)x, (float)y, 0.0f);
}

static void APIENTRY _gen_glWindowPos2iv(const GLint *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], 0.0f);
}

static void APIENTRY _gen_glWindowPos2s(GLshort x, GLshort y)
{
    (void)x; (void)y;
    _glsWindowPos3f((float)x, (float)y, 0.0f);
}

static void APIENTRY _gen_glWindowPos2sv(const GLshort *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], 0.0f);
}

static void APIENTRY _gen_glWindowPos3d(GLdouble x, GLdouble y, GLdouble z)
{
    (void)x; (void)y; (void)z;
    _glsWindowPos3f((float)x, (float)y, (float)z);
}

static void APIENTRY _gen_glWindowPos3dv(const GLdouble *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], (float)v[2]);
}

static void APIENTRY _gen_glWindowPos3f(GLfloat x, GLfloat y, GLfloat z)
{
    (void)x; (void)y; (void)z;
    _glsWindowPos3f((float)x, (float)y, (float)z);
}

static void APIENTRY _gen_glWindowPos3fv(const GLfloat *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], (float)v[2]);
}

static void APIENTRY _gen_glWindowPos3i(GLint x, GLint y, GLint z)
{
    (void)x; (void)y; (void)z;
    _glsWindowPos3f((float)x, (float)y, (float)z);
}

static void APIENTRY _gen_glWindowPos3iv(const GLint *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], (float)v[2]);
}

static void APIENTRY _gen_glWindowPos3s(GLshort x, GLshort y, GLshort z)
{
    (void)x; (void)y; (void)z;
    _glsWindowPos3f((float)x, (float)y, (float)z);
}

static void APIENTRY _gen_glWindowPos3sv(const GLshort *v)
{
    (void)v;
    if (!v) return;
    _glsWindowPos3f((float)v[0], (float)v[1], (float)v[2]);
}

static void APIENTRY _gen_glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    (void)target; (void)level; (void)internalformat; (void)x; (void)y; (void)width; (void)border;
    _glsCopyTexImage2D((unsigned int)target, (int)level, (unsigned int)internalformat, (int)x, (int)y, (int)width, 1, (int)border);
}

static void APIENTRY _gen_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    (void)target; (void)level; (void)internalformat; (void)x; (void)y; (void)width; (void)height; (void)border;
    _glsCopyTexImage2D((unsigned int)target, (int)level, (unsigned int)internalformat, (int)x, (int)y, (int)width, (int)height, (int)border);
}

static void APIENTRY _gen_glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    (void)target; (void)level; (void)xoffset; (void)x; (void)y; (void)width;
    _glsCopyTexSubImage2D((unsigned int)target, (int)level, (int)xoffset, 0, (int)x, (int)y, (int)width, 1);
}

static void APIENTRY _gen_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)x; (void)y; (void)width; (void)height;
    _glsCopyTexSubImage2D((unsigned int)target, (int)level, (int)xoffset, (int)yoffset, (int)x, (int)y, (int)width, (int)height);
}

static void APIENTRY _gen_glGetPointerv(GLenum pname, void **params)
{
    (void)pname; (void)params;
    _glsGetPointerv((unsigned int)pname, (void **)params);
}

static GLboolean APIENTRY _gen_glIsTexture(GLuint texture)
{
    (void)texture;
    return (GLboolean)_glsIsTexture((unsigned int)texture) ? GL_TRUE : GL_FALSE;
}

static void APIENTRY _gen_glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    (void)target; (void)level; (void)xoffset; (void)width; (void)format; (void)type; (void)pixels;
    _glsTexSubImage1D((unsigned int)target, (int)level, (int)xoffset, (int)width, (unsigned int)format, (unsigned int)type, pixels);
}

static void APIENTRY _gen_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height; (void)format; (void)type; (void)pixels;
    _glsTexSubImage2D((unsigned int)target, (int)level, (int)xoffset, (int)yoffset, (int)width, (int)height, (unsigned int)format, (unsigned int)type, pixels);
}

/* ===== Master table, scanned by gldGetProcAddress_GL46 ===== */

static const GLD_modernProcEntry g_generatedGL[] = {
    { "glActiveShaderProgram",                         (PROC)_gen_glActiveShaderProgram },
    { "glBeginConditionalRenderNV",                    (PROC)_stub_glBeginConditionalRender },
    { "glBeginQueryIndexed",                           (PROC)_gen_glBeginQueryIndexed },
    { "glBeginTransformFeedbackEXT",                   (PROC)_stub_glBeginTransformFeedback },
    { "glBeginTransformFeedbackNV",                    (PROC)_stub_glBeginTransformFeedback },
    { "glBindBufferBaseEXT",                           (PROC)_stub_glBindBufferBase },
    { "glBindBufferBaseNV",                            (PROC)_stub_glBindBufferBase },
    { "glBindBufferRangeEXT",                          (PROC)_stub_glBindBufferRange },
    { "glBindBufferRangeNV",                           (PROC)_stub_glBindBufferRange },
    { "glBindBuffersBase",                             (PROC)_gen_glBindBuffersBase },
    { "glBindBuffersRange",                            (PROC)_gen_glBindBuffersRange },
    { "glBindFragDataLocation",                        (PROC)_gen_glBindFragDataLocation },
    { "glBindFragDataLocationEXT",                     (PROC)_gen_glBindFragDataLocation },
    { "glBindFragDataLocationIndexed",                 (PROC)_gen_glBindFragDataLocationIndexed },
    { "glBindImageTextures",                           (PROC)_gen_glBindImageTextures },
    { "glBindProgramPipeline",                         (PROC)_gen_glBindProgramPipeline },
    { "glBindSamplers",                                (PROC)_gen_glBindSamplers },
    { "glBindTextureUnit",                             (PROC)_gen_glBindTextureUnit },
    { "glBindTextures",                                (PROC)_gen_glBindTextures },
    { "glBindTransformFeedback",                       (PROC)_gen_glBindTransformFeedback },
    { "glBindVertexBuffer",                            (PROC)_gen_glBindVertexBuffer },
    { "glBindVertexBuffers",                           (PROC)_gen_glBindVertexBuffers },
    { "glBlendEquationIndexedAMD",                     (PROC)_stub_glBlendEquationi },
    { "glBlendEquationSeparateEXT",                    (PROC)_stub_glBlendEquationSeparate },
    { "glBlendEquationSeparateIndexedAMD",             (PROC)_gen_glBlendEquationSeparatei },
    { "glBlendEquationSeparatei",                      (PROC)_gen_glBlendEquationSeparatei },
    { "glBlendEquationSeparateiARB",                   (PROC)_gen_glBlendEquationSeparatei },
    { "glBlendEquationiARB",                           (PROC)_stub_glBlendEquationi },
    { "glBlendFuncIndexedAMD",                         (PROC)_stub_glBlendFunci },
    { "glBlendFuncSeparateEXT",                        (PROC)_stub_glBlendFuncSeparate },
    { "glBlendFuncSeparateINGR",                       (PROC)_stub_glBlendFuncSeparate },
    { "glBlendFuncSeparateIndexedAMD",                 (PROC)_gen_glBlendFuncSeparatei },
    { "glBlendFuncSeparatei",                          (PROC)_gen_glBlendFuncSeparatei },
    { "glBlendFuncSeparateiARB",                       (PROC)_gen_glBlendFuncSeparatei },
    { "glBlendFunciARB",                               (PROC)_stub_glBlendFunci },
    { "glBlitNamedFramebuffer",                        (PROC)_gen_glBlitNamedFramebuffer },
    { "glBufferStorage",                               (PROC)_gen_glBufferStorage },
    { "glCheckNamedFramebufferStatus",                 (PROC)_gen_glCheckNamedFramebufferStatus },
    { "glClampColor",                                  (PROC)_gen_glClampColor },
    { "glClampColorARB",                               (PROC)_gen_glClampColor },
    { "glClearBufferData",                             (PROC)_gen_glClearBufferData },
    { "glClearBufferSubData",                          (PROC)_gen_glClearBufferSubData },
    { "glClearDepthf",                                 (PROC)_gen_glClearDepthf },
    { "glClearDepthfOES",                              (PROC)_gen_glClearDepthf },
    { "glClearNamedBufferData",                        (PROC)_gen_glClearNamedBufferData },
    { "glClearNamedBufferSubData",                     (PROC)_gen_glClearNamedBufferSubData },
    { "glClearNamedFramebufferfi",                     (PROC)_gen_glClearNamedFramebufferfi },
    { "glClearNamedFramebufferfv",                     (PROC)_gen_glClearNamedFramebufferfv },
    { "glClearNamedFramebufferiv",                     (PROC)_gen_glClearNamedFramebufferiv },
    { "glClearNamedFramebufferuiv",                    (PROC)_gen_glClearNamedFramebufferuiv },
    { "glClearTexImage",                               (PROC)_gen_glClearTexImage },
    { "glClearTexSubImage",                            (PROC)_gen_glClearTexSubImage },
    { "glColorMaskIndexedEXT",                         (PROC)_stub_glColorMaski },
    { "glColorP3ui",                                   (PROC)_gen_glColorP3ui },
    { "glColorP3uiv",                                  (PROC)_gen_glColorP3uiv },
    { "glColorP4ui",                                   (PROC)_gen_glColorP4ui },
    { "glColorP4uiv",                                  (PROC)_gen_glColorP4uiv },
    { "glCompressedTexImage1DARB",                     (PROC)_stub_glCompressedTexImage1D },
    { "glCompressedTexImage3DARB",                     (PROC)_stub_glCompressedTexImage3D },
    { "glCompressedTexSubImage1DARB",                  (PROC)_stub_glCompressedTexSubImage1D },
    { "glCompressedTexSubImage2DARB",                  (PROC)_stub_glCompressedTexSubImage2D },
    { "glCompressedTexSubImage3DARB",                  (PROC)_stub_glCompressedTexSubImage3D },
    { "glCompressedTextureSubImage1D",                 (PROC)_gen_glCompressedTextureSubImage1D },
    { "glCompressedTextureSubImage2D",                 (PROC)_gen_glCompressedTextureSubImage2D },
    { "glCompressedTextureSubImage3D",                 (PROC)_gen_glCompressedTextureSubImage3D },
    { "glCopyImageSubData",                            (PROC)_gen_glCopyImageSubData },
    { "glCopyNamedBufferSubData",                      (PROC)_gen_glCopyNamedBufferSubData },
    { "glCopyTexImage1DEXT",                           (PROC)_gen_glCopyTexImage1D },
    { "glCopyTexImage2DEXT",                           (PROC)_gen_glCopyTexImage2D },
    { "glCopyTexSubImage1DEXT",                        (PROC)_gen_glCopyTexSubImage1D },
    { "glCopyTexSubImage2DEXT",                        (PROC)_gen_glCopyTexSubImage2D },
    { "glCopyTextureSubImage1D",                       (PROC)_gen_glCopyTextureSubImage1D },
    { "glCopyTextureSubImage2D",                       (PROC)_gen_glCopyTextureSubImage2D },
    { "glCopyTextureSubImage3D",                       (PROC)_gen_glCopyTextureSubImage3D },
    { "glCreateBuffers",                               (PROC)_gen_glCreateBuffers },
    { "glCreateFramebuffers",                          (PROC)_gen_glCreateFramebuffers },
    { "glCreateProgramPipelines",                      (PROC)_gen_glCreateProgramPipelines },
    { "glCreateQueries",                               (PROC)_gen_glCreateQueries },
    { "glCreateRenderbuffers",                         (PROC)_gen_glCreateRenderbuffers },
    { "glCreateSamplers",                              (PROC)_gen_glCreateSamplers },
    { "glCreateShaderProgramv",                        (PROC)_gen_glCreateShaderProgramv },
    { "glCreateTextures",                              (PROC)_gen_glCreateTextures },
    { "glCreateTransformFeedbacks",                    (PROC)_gen_glCreateTransformFeedbacks },
    { "glCreateVertexArrays",                          (PROC)_gen_glCreateVertexArrays },
    { "glDebugMessageCallbackARB",                     (PROC)_stub_glDebugMessageCallback },
    { "glDebugMessageCallbackKHR",                     (PROC)_stub_glDebugMessageCallback },
    { "glDebugMessageControlARB",                      (PROC)_stub_glDebugMessageControl },
    { "glDebugMessageControlKHR",                      (PROC)_stub_glDebugMessageControl },
    { "glDebugMessageInsert",                          (PROC)_gen_glDebugMessageInsert },
    { "glDebugMessageInsertARB",                       (PROC)_gen_glDebugMessageInsert },
    { "glDebugMessageInsertKHR",                       (PROC)_gen_glDebugMessageInsert },
    { "glDeleteProgramPipelines",                      (PROC)_gen_glDeleteProgramPipelines },
    { "glDeleteQueriesARB",                            (PROC)_stub_glDeleteQueries },
    { "glDeleteTransformFeedbacks",                    (PROC)_gen_glDeleteTransformFeedbacks },
    { "glDeleteTransformFeedbacksNV",                  (PROC)_gen_glDeleteTransformFeedbacks },
    { "glDeleteVertexArraysAPPLE",                     (PROC)_stub_glDeleteVertexArrays },
    { "glDepthRangeArrayv",                            (PROC)_gen_glDepthRangeArrayv },
    { "glDepthRangeIndexed",                           (PROC)_gen_glDepthRangeIndexed },
    { "glDepthRangef",                                 (PROC)_gen_glDepthRangef },
    { "glDepthRangefOES",                              (PROC)_gen_glDepthRangef },
    { "glDisableIndexedEXT",                           (PROC)_stub_glDisablei },
    { "glDisableVertexArrayAttrib",                    (PROC)_gen_glDisableVertexArrayAttrib },
    { "glDispatchComputeIndirect",                     (PROC)_gen_glDispatchComputeIndirect },
    { "glDrawArraysIndirect",                          (PROC)_gen_glDrawArraysIndirect },
    { "glDrawArraysInstancedARB",                      (PROC)_stub_glDrawArraysInstanced },
    { "glDrawArraysInstancedBaseInstance",             (PROC)_gen_glDrawArraysInstancedBaseInstance },
    { "glDrawArraysInstancedEXT",                      (PROC)_stub_glDrawArraysInstanced },
    { "glDrawBuffersARB",                              (PROC)_stub_glDrawBuffers },
    { "glDrawBuffersATI",                              (PROC)_stub_glDrawBuffers },
    { "glDrawElementsBaseVertex",                      (PROC)_gen_glDrawElementsBaseVertex },
    { "glDrawElementsIndirect",                        (PROC)_gen_glDrawElementsIndirect },
    { "glDrawElementsInstancedARB",                    (PROC)_stub_glDrawElementsInstanced },
    { "glDrawElementsInstancedBaseInstance",           (PROC)_gen_glDrawElementsInstancedBaseInstance },
    { "glDrawElementsInstancedBaseVertex",             (PROC)_gen_glDrawElementsInstancedBaseVertex },
    { "glDrawElementsInstancedBaseVertexBaseInstance", (PROC)_gen_glDrawElementsInstancedBaseVertexBaseInstance },
    { "glDrawElementsInstancedEXT",                    (PROC)_stub_glDrawElementsInstanced },
    { "glDrawRangeElementsBaseVertex",                 (PROC)_gen_glDrawRangeElementsBaseVertex },
    { "glDrawTransformFeedback",                       (PROC)_gen_glDrawTransformFeedback },
    { "glDrawTransformFeedbackInstanced",              (PROC)_gen_glDrawTransformFeedbackInstanced },
    { "glDrawTransformFeedbackNV",                     (PROC)_gen_glDrawTransformFeedback },
    { "glDrawTransformFeedbackStream",                 (PROC)_gen_glDrawTransformFeedbackStream },
    { "glDrawTransformFeedbackStreamInstanced",        (PROC)_gen_glDrawTransformFeedbackStreamInstanced },
    { "glEnableIndexedEXT",                            (PROC)_stub_glEnablei },
    { "glEnableVertexArrayAttrib",                     (PROC)_gen_glEnableVertexArrayAttrib },
    { "glEndConditionalRenderNV",                      (PROC)_stub_glEndConditionalRender },
    { "glEndConditionalRenderNVX",                     (PROC)_stub_glEndConditionalRender },
    { "glEndQueryIndexed",                             (PROC)_gen_glEndQueryIndexed },
    { "glEndTransformFeedbackEXT",                     (PROC)_stub_glEndTransformFeedback },
    { "glEndTransformFeedbackNV",                      (PROC)_stub_glEndTransformFeedback },
    { "glFlushMappedBufferRangeAPPLE",                 (PROC)_stub_glFlushMappedBufferRange },
    { "glFlushMappedNamedBufferRange",                 (PROC)_gen_glFlushMappedNamedBufferRange },
    { "glFogCoorddEXT",                                (PROC)_stub_glFogCoordd },
    { "glFogCoorddvEXT",                               (PROC)_stub_glFogCoorddv },
    { "glFogCoordfvEXT",                               (PROC)_stub_glFogCoordfv },
    { "glFramebufferParameteri",                       (PROC)_gen_glFramebufferParameteri },
    { "glFramebufferTextureARB",                       (PROC)_stub_glFramebufferTexture },
    { "glFramebufferTextureEXT",                       (PROC)_stub_glFramebufferTexture },
    { "glFramebufferTextureLayer",                     (PROC)_gen_glFramebufferTextureLayer },
    { "glFramebufferTextureLayerARB",                  (PROC)_gen_glFramebufferTextureLayer },
    { "glFramebufferTextureLayerEXT",                  (PROC)_gen_glFramebufferTextureLayer },
    { "glGenProgramPipelines",                         (PROC)_gen_glGenProgramPipelines },
    { "glGenQueriesARB",                               (PROC)_stub_glGenQueries },
    { "glGenTransformFeedbacks",                       (PROC)_gen_glGenTransformFeedbacks },
    { "glGenTransformFeedbacksNV",                     (PROC)_gen_glGenTransformFeedbacks },
    { "glGenVertexArraysAPPLE",                        (PROC)_stub_glGenVertexArrays },
    { "glGenerateTextureMipmap",                       (PROC)_gen_glGenerateTextureMipmap },
    { "glGetActiveAtomicCounterBufferiv",              (PROC)_gen_glGetActiveAtomicCounterBufferiv },
    { "glGetActiveAttribARB",                          (PROC)_stub_glGetActiveAttrib },
    { "glGetActiveSubroutineName",                     (PROC)_gen_glGetActiveSubroutineName },
    { "glGetActiveSubroutineUniformName",              (PROC)_gen_glGetActiveSubroutineUniformName },
    { "glGetActiveSubroutineUniformiv",                (PROC)_gen_glGetActiveSubroutineUniformiv },
    { "glGetActiveUniformARB",                         (PROC)_stub_glGetActiveUniform },
    { "glGetActiveUniformBlockName",                   (PROC)_gen_glGetActiveUniformBlockName },
    { "glGetActiveUniformBlockiv",                     (PROC)_gen_glGetActiveUniformBlockiv },
    { "glGetActiveUniformName",                        (PROC)_gen_glGetActiveUniformName },
    { "glGetActiveUniformsiv",                         (PROC)_gen_glGetActiveUniformsiv },
    { "glGetAttachedShaders",                          (PROC)_gen_glGetAttachedShaders },
    { "glGetBooleanIndexedvEXT",                       (PROC)_gen_glGetBooleani_v },
    { "glGetBooleani_v",                               (PROC)_gen_glGetBooleani_v },
    { "glGetCompressedTextureImage",                   (PROC)_gen_glGetCompressedTextureImage },
    { "glGetCompressedTextureSubImage",                (PROC)_gen_glGetCompressedTextureSubImage },
    { "glGetDebugMessageLog",                          (PROC)_gen_glGetDebugMessageLog },
    { "glGetDebugMessageLogARB",                       (PROC)_gen_glGetDebugMessageLog },
    { "glGetDebugMessageLogKHR",                       (PROC)_gen_glGetDebugMessageLog },
    { "glGetDoubleIndexedvEXT",                        (PROC)_gen_glGetDoublei_v },
    { "glGetDoublei_v",                                (PROC)_gen_glGetDoublei_v },
    { "glGetDoublei_vEXT",                             (PROC)_gen_glGetDoublei_v },
    { "glGetFloatIndexedvEXT",                         (PROC)_gen_glGetFloati_v },
    { "glGetFloati_v",                                 (PROC)_gen_glGetFloati_v },
    { "glGetFloati_vEXT",                              (PROC)_gen_glGetFloati_v },
    { "glGetFragDataIndex",                            (PROC)_gen_glGetFragDataIndex },
    { "glGetFragDataLocation",                         (PROC)_gen_glGetFragDataLocation },
    { "glGetFragDataLocationEXT",                      (PROC)_gen_glGetFragDataLocation },
    { "glGetFramebufferParameteriv",                   (PROC)_gen_glGetFramebufferParameteriv },
    { "glGetGraphicsResetStatus",                      (PROC)_gen_glGetGraphicsResetStatus },
    { "glGetGraphicsResetStatusKHR",                   (PROC)_gen_glGetGraphicsResetStatus },
    { "glGetInteger64i_v",                             (PROC)_gen_glGetInteger64i_v },
    { "glGetIntegerIndexedvEXT",                       (PROC)_gen_glGetIntegeri_v },
    { "glGetIntegeri_v",                               (PROC)_gen_glGetIntegeri_v },
    { "glGetInternalformati64v",                       (PROC)_gen_glGetInternalformati64v },
    { "glGetInternalformativ",                         (PROC)_gen_glGetInternalformativ },
    { "glGetMultisamplefvNV",                          (PROC)_stub_glGetMultisamplefv },
    { "glGetNamedBufferParameteri64v",                 (PROC)_gen_glGetNamedBufferParameteri64v },
    { "glGetNamedBufferParameteriv",                   (PROC)_gen_glGetNamedBufferParameteriv },
    { "glGetNamedBufferPointerv",                      (PROC)_gen_glGetNamedBufferPointerv },
    { "glGetNamedBufferSubData",                       (PROC)_gen_glGetNamedBufferSubData },
    { "glGetNamedFramebufferAttachmentParameteriv",    (PROC)_gen_glGetNamedFramebufferAttachmentParameteriv },
    { "glGetNamedFramebufferParameteriv",              (PROC)_gen_glGetNamedFramebufferParameteriv },
    { "glGetNamedRenderbufferParameteriv",             (PROC)_gen_glGetNamedRenderbufferParameteriv },
    { "glGetObjectLabel",                              (PROC)_gen_glGetObjectLabel },
    { "glGetObjectLabelKHR",                           (PROC)_gen_glGetObjectLabel },
    { "glGetObjectPtrLabel",                           (PROC)_gen_glGetObjectPtrLabel },
    { "glGetObjectPtrLabelKHR",                        (PROC)_gen_glGetObjectPtrLabel },
    { "glGetPointervKHR",                              (PROC)_gen_glGetPointerv },
    { "glGetProgramBinary",                            (PROC)_gen_glGetProgramBinary },
    { "glGetProgramInterfaceiv",                       (PROC)_gen_glGetProgramInterfaceiv },
    { "glGetProgramPipelineInfoLog",                   (PROC)_gen_glGetProgramPipelineInfoLog },
    { "glGetProgramPipelineiv",                        (PROC)_gen_glGetProgramPipelineiv },
    { "glGetProgramResourceIndex",                     (PROC)_gen_glGetProgramResourceIndex },
    { "glGetProgramResourceLocation",                  (PROC)_gen_glGetProgramResourceLocation },
    { "glGetProgramResourceLocationIndex",             (PROC)_gen_glGetProgramResourceLocationIndex },
    { "glGetProgramResourceName",                      (PROC)_gen_glGetProgramResourceName },
    { "glGetProgramResourceiv",                        (PROC)_gen_glGetProgramResourceiv },
    { "glGetProgramStageiv",                           (PROC)_gen_glGetProgramStageiv },
    { "glGetQueryBufferObjecti64v",                    (PROC)_gen_glGetQueryBufferObjecti64v },
    { "glGetQueryBufferObjectiv",                      (PROC)_gen_glGetQueryBufferObjectiv },
    { "glGetQueryBufferObjectui64v",                   (PROC)_gen_glGetQueryBufferObjectui64v },
    { "glGetQueryBufferObjectuiv",                     (PROC)_gen_glGetQueryBufferObjectuiv },
    { "glGetQueryIndexediv",                           (PROC)_gen_glGetQueryIndexediv },
    { "glGetQueryObjecti64vEXT",                       (PROC)_stub_glGetQueryObjecti64v },
    { "glGetQueryObjectivARB",                         (PROC)_stub_glGetQueryObjectiv },
    { "glGetQueryObjectui64vEXT",                      (PROC)_stub_glGetQueryObjectui64v },
    { "glGetQueryObjectuivARB",                        (PROC)_stub_glGetQueryObjectuiv },
    { "glGetQueryivARB",                               (PROC)_stub_glGetQueryiv },
    { "glGetSamplerParameterIiv",                      (PROC)_gen_glGetSamplerParameterIiv },
    { "glGetSamplerParameterIuiv",                     (PROC)_gen_glGetSamplerParameterIuiv },
    { "glGetSamplerParameterfv",                       (PROC)_gen_glGetSamplerParameterfv },
    { "glGetSamplerParameteriv",                       (PROC)_gen_glGetSamplerParameteriv },
    { "glGetShaderPrecisionFormat",                    (PROC)_gen_glGetShaderPrecisionFormat },
    { "glGetShaderSource",                             (PROC)_gen_glGetShaderSource },
    { "glGetShaderSourceARB",                          (PROC)_gen_glGetShaderSource },
    { "glGetSubroutineIndex",                          (PROC)_gen_glGetSubroutineIndex },
    { "glGetSubroutineUniformLocation",                (PROC)_gen_glGetSubroutineUniformLocation },
    { "glGetSynciv",                                   (PROC)_gen_glGetSynciv },
    { "glGetTexParameterIiv",                          (PROC)_gen_glGetTexParameterIiv },
    { "glGetTexParameterIivEXT",                       (PROC)_gen_glGetTexParameterIiv },
    { "glGetTexParameterIuiv",                         (PROC)_gen_glGetTexParameterIuiv },
    { "glGetTexParameterIuivEXT",                      (PROC)_gen_glGetTexParameterIuiv },
    { "glGetTextureImage",                             (PROC)_gen_glGetTextureImage },
    { "glGetTextureLevelParameterfv",                  (PROC)_gen_glGetTextureLevelParameterfv },
    { "glGetTextureLevelParameteriv",                  (PROC)_gen_glGetTextureLevelParameteriv },
    { "glGetTextureParameterIiv",                      (PROC)_gen_glGetTextureParameterIiv },
    { "glGetTextureParameterIuiv",                     (PROC)_gen_glGetTextureParameterIuiv },
    { "glGetTextureParameterfv",                       (PROC)_gen_glGetTextureParameterfv },
    { "glGetTextureParameteriv",                       (PROC)_gen_glGetTextureParameteriv },
    { "glGetTextureSubImage",                          (PROC)_gen_glGetTextureSubImage },
    { "glGetTransformFeedbackVarying",                 (PROC)_gen_glGetTransformFeedbackVarying },
    { "glGetTransformFeedbackVaryingEXT",              (PROC)_gen_glGetTransformFeedbackVarying },
    { "glGetTransformFeedbacki64_v",                   (PROC)_gen_glGetTransformFeedbacki64_v },
    { "glGetTransformFeedbacki_v",                     (PROC)_gen_glGetTransformFeedbacki_v },
    { "glGetTransformFeedbackiv",                      (PROC)_gen_glGetTransformFeedbackiv },
    { "glGetUniformIndices",                           (PROC)_gen_glGetUniformIndices },
    { "glGetUniformSubroutineuiv",                     (PROC)_gen_glGetUniformSubroutineuiv },
    { "glGetUniformdv",                                (PROC)_gen_glGetUniformdv },
    { "glGetUniformfv",                                (PROC)_gen_glGetUniformfv },
    { "glGetUniformfvARB",                             (PROC)_gen_glGetUniformfv },
    { "glGetUniformiv",                                (PROC)_gen_glGetUniformiv },
    { "glGetUniformivARB",                             (PROC)_gen_glGetUniformiv },
    { "glGetUniformuiv",                               (PROC)_gen_glGetUniformuiv },
    { "glGetUniformuivEXT",                            (PROC)_gen_glGetUniformuiv },
    { "glGetVertexArrayIndexed64iv",                   (PROC)_gen_glGetVertexArrayIndexed64iv },
    { "glGetVertexArrayIndexediv",                     (PROC)_gen_glGetVertexArrayIndexediv },
    { "glGetVertexArrayiv",                            (PROC)_gen_glGetVertexArrayiv },
    { "glGetVertexAttribIiv",                          (PROC)_gen_glGetVertexAttribIiv },
    { "glGetVertexAttribIivEXT",                       (PROC)_gen_glGetVertexAttribIiv },
    { "glGetVertexAttribIuiv",                         (PROC)_gen_glGetVertexAttribIuiv },
    { "glGetVertexAttribIuivEXT",                      (PROC)_gen_glGetVertexAttribIuiv },
    { "glGetVertexAttribLdv",                          (PROC)_gen_glGetVertexAttribLdv },
    { "glGetVertexAttribLdvEXT",                       (PROC)_gen_glGetVertexAttribLdv },
    { "glGetVertexAttribPointerv",                     (PROC)_gen_glGetVertexAttribPointerv },
    { "glGetVertexAttribPointervARB",                  (PROC)_gen_glGetVertexAttribPointerv },
    { "glGetVertexAttribPointervNV",                   (PROC)_gen_glGetVertexAttribPointerv },
    { "glGetVertexAttribdv",                           (PROC)_gen_glGetVertexAttribdv },
    { "glGetVertexAttribdvARB",                        (PROC)_gen_glGetVertexAttribdv },
    { "glGetVertexAttribdvNV",                         (PROC)_gen_glGetVertexAttribdv },
    { "glGetVertexAttribfv",                           (PROC)_gen_glGetVertexAttribfv },
    { "glGetVertexAttribfvARB",                        (PROC)_gen_glGetVertexAttribfv },
    { "glGetVertexAttribfvNV",                         (PROC)_gen_glGetVertexAttribfv },
    { "glGetVertexAttribiv",                           (PROC)_gen_glGetVertexAttribiv },
    { "glGetVertexAttribivARB",                        (PROC)_gen_glGetVertexAttribiv },
    { "glGetVertexAttribivNV",                         (PROC)_gen_glGetVertexAttribiv },
    { "glGetnColorTable",                              (PROC)_gen_glGetnColorTable },
    { "glGetnCompressedTexImage",                      (PROC)_gen_glGetnCompressedTexImage },
    { "glGetnConvolutionFilter",                       (PROC)_gen_glGetnConvolutionFilter },
    { "glGetnHistogram",                               (PROC)_gen_glGetnHistogram },
    { "glGetnMapdv",                                   (PROC)_gen_glGetnMapdv },
    { "glGetnMapfv",                                   (PROC)_gen_glGetnMapfv },
    { "glGetnMapiv",                                   (PROC)_gen_glGetnMapiv },
    { "glGetnMinmax",                                  (PROC)_gen_glGetnMinmax },
    { "glGetnPixelMapfv",                              (PROC)_gen_glGetnPixelMapfv },
    { "glGetnPixelMapuiv",                             (PROC)_gen_glGetnPixelMapuiv },
    { "glGetnPixelMapusv",                             (PROC)_gen_glGetnPixelMapusv },
    { "glGetnPolygonStipple",                          (PROC)_gen_glGetnPolygonStipple },
    { "glGetnSeparableFilter",                         (PROC)_gen_glGetnSeparableFilter },
    { "glGetnTexImage",                                (PROC)_gen_glGetnTexImage },
    { "glGetnUniformdv",                               (PROC)_gen_glGetnUniformdv },
    { "glGetnUniformfv",                               (PROC)_gen_glGetnUniformfv },
    { "glGetnUniformfvKHR",                            (PROC)_gen_glGetnUniformfv },
    { "glGetnUniformiv",                               (PROC)_gen_glGetnUniformiv },
    { "glGetnUniformivKHR",                            (PROC)_gen_glGetnUniformiv },
    { "glGetnUniformuiv",                              (PROC)_gen_glGetnUniformuiv },
    { "glGetnUniformuivKHR",                           (PROC)_gen_glGetnUniformuiv },
    { "glInvalidateBufferData",                        (PROC)_gen_glInvalidateBufferData },
    { "glInvalidateBufferSubData",                     (PROC)_gen_glInvalidateBufferSubData },
    { "glInvalidateFramebuffer",                       (PROC)_gen_glInvalidateFramebuffer },
    { "glInvalidateNamedFramebufferData",              (PROC)_gen_glInvalidateNamedFramebufferData },
    { "glInvalidateNamedFramebufferSubData",           (PROC)_gen_glInvalidateNamedFramebufferSubData },
    { "glInvalidateSubFramebuffer",                    (PROC)_gen_glInvalidateSubFramebuffer },
    { "glInvalidateTexImage",                          (PROC)_gen_glInvalidateTexImage },
    { "glInvalidateTexSubImage",                       (PROC)_gen_glInvalidateTexSubImage },
    { "glIsEnabledIndexedEXT",                         (PROC)_gen_glIsEnabledi },
    { "glIsEnabledi",                                  (PROC)_gen_glIsEnabledi },
    { "glIsProgramPipeline",                           (PROC)_gen_glIsProgramPipeline },
    { "glIsQuery",                                     (PROC)_gen_glIsQuery },
    { "glIsQueryARB",                                  (PROC)_gen_glIsQuery },
    { "glIsSampler",                                   (PROC)_gen_glIsSampler },
    { "glIsSync",                                      (PROC)_gen_glIsSync },
    { "glIsTextureEXT",                                (PROC)_gen_glIsTexture },
    { "glIsTransformFeedback",                         (PROC)_gen_glIsTransformFeedback },
    { "glIsTransformFeedbackNV",                       (PROC)_gen_glIsTransformFeedback },
    { "glIsVertexArrayAPPLE",                          (PROC)_stub_glIsVertexArray },
    { "glLoadTransposeMatrixdARB",                     (PROC)_stub_glLoadTransposeMatrixd },
    { "glLoadTransposeMatrixfARB",                     (PROC)_stub_glLoadTransposeMatrixf },
    { "glMapNamedBuffer",                              (PROC)_gen_glMapNamedBuffer },
    { "glMapNamedBufferRange",                         (PROC)_gen_glMapNamedBufferRange },
    { "glMemoryBarrierByRegion",                       (PROC)_gen_glMemoryBarrierByRegion },
    { "glMemoryBarrierEXT",                            (PROC)_stub_glMemoryBarrier },
    { "glMinSampleShadingARB",                         (PROC)_stub_glMinSampleShading },
    { "glMultTransposeMatrixdARB",                     (PROC)_stub_glMultTransposeMatrixd },
    { "glMultTransposeMatrixfARB",                     (PROC)_stub_glMultTransposeMatrixf },
    { "glMultiDrawArraysEXT",                          (PROC)_stub_glMultiDrawArrays },
    { "glMultiDrawArraysIndirect",                     (PROC)_gen_glMultiDrawArraysIndirect },
    { "glMultiDrawArraysIndirectAMD",                  (PROC)_gen_glMultiDrawArraysIndirect },
    { "glMultiDrawArraysIndirectCount",                (PROC)_gen_glMultiDrawArraysIndirectCount },
    { "glMultiDrawArraysIndirectCountARB",             (PROC)_gen_glMultiDrawArraysIndirectCount },
    { "glMultiDrawElementsBaseVertex",                 (PROC)_gen_glMultiDrawElementsBaseVertex },
    { "glMultiDrawElementsIndirect",                   (PROC)_gen_glMultiDrawElementsIndirect },
    { "glMultiDrawElementsIndirectAMD",                (PROC)_gen_glMultiDrawElementsIndirect },
    { "glMultiDrawElementsIndirectCount",              (PROC)_gen_glMultiDrawElementsIndirectCount },
    { "glMultiDrawElementsIndirectCountARB",           (PROC)_gen_glMultiDrawElementsIndirectCount },
    { "glMultiTexCoordP1ui",                           (PROC)_gen_glMultiTexCoordP1ui },
    { "glMultiTexCoordP1uiv",                          (PROC)_gen_glMultiTexCoordP1uiv },
    { "glMultiTexCoordP2ui",                           (PROC)_gen_glMultiTexCoordP2ui },
    { "glMultiTexCoordP2uiv",                          (PROC)_gen_glMultiTexCoordP2uiv },
    { "glMultiTexCoordP3ui",                           (PROC)_gen_glMultiTexCoordP3ui },
    { "glMultiTexCoordP3uiv",                          (PROC)_gen_glMultiTexCoordP3uiv },
    { "glMultiTexCoordP4ui",                           (PROC)_gen_glMultiTexCoordP4ui },
    { "glMultiTexCoordP4uiv",                          (PROC)_gen_glMultiTexCoordP4uiv },
    { "glNamedBufferData",                             (PROC)_gen_glNamedBufferData },
    { "glNamedBufferStorage",                          (PROC)_gen_glNamedBufferStorage },
    { "glNamedBufferStorageEXT",                       (PROC)_gen_glNamedBufferStorage },
    { "glNamedBufferSubData",                          (PROC)_gen_glNamedBufferSubData },
    { "glNamedBufferSubDataEXT",                       (PROC)_gen_glNamedBufferSubData },
    { "glNamedFramebufferDrawBuffer",                  (PROC)_gen_glNamedFramebufferDrawBuffer },
    { "glNamedFramebufferDrawBuffers",                 (PROC)_gen_glNamedFramebufferDrawBuffers },
    { "glNamedFramebufferParameteri",                  (PROC)_gen_glNamedFramebufferParameteri },
    { "glNamedFramebufferReadBuffer",                  (PROC)_gen_glNamedFramebufferReadBuffer },
    { "glNamedFramebufferRenderbuffer",                (PROC)_gen_glNamedFramebufferRenderbuffer },
    { "glNamedFramebufferTexture",                     (PROC)_gen_glNamedFramebufferTexture },
    { "glNamedFramebufferTextureLayer",                (PROC)_gen_glNamedFramebufferTextureLayer },
    { "glNamedRenderbufferStorage",                    (PROC)_gen_glNamedRenderbufferStorage },
    { "glNamedRenderbufferStorageMultisample",         (PROC)_gen_glNamedRenderbufferStorageMultisample },
    { "glNormalP3ui",                                  (PROC)_gen_glNormalP3ui },
    { "glNormalP3uiv",                                 (PROC)_gen_glNormalP3uiv },
    { "glObjectLabelKHR",                              (PROC)_stub_glObjectLabel },
    { "glObjectPtrLabel",                              (PROC)_gen_glObjectPtrLabel },
    { "glObjectPtrLabelKHR",                           (PROC)_gen_glObjectPtrLabel },
    { "glPatchParameterfv",                            (PROC)_gen_glPatchParameterfv },
    { "glPauseTransformFeedback",                      (PROC)_gen_glPauseTransformFeedback },
    { "glPauseTransformFeedbackNV",                    (PROC)_gen_glPauseTransformFeedback },
    { "glPointParameterfARB",                          (PROC)_stub_glPointParameterf },
    { "glPointParameterfSGIS",                         (PROC)_stub_glPointParameterf },
    { "glPointParameterfvARB",                         (PROC)_stub_glPointParameterfv },
    { "glPointParameterfvSGIS",                        (PROC)_stub_glPointParameterfv },
    { "glPointParameteriNV",                           (PROC)_stub_glPointParameteri },
    { "glPointParameterivNV",                          (PROC)_stub_glPointParameteriv },
    { "glPolygonOffsetClamp",                          (PROC)_gen_glPolygonOffsetClamp },
    { "glPolygonOffsetClampEXT",                       (PROC)_gen_glPolygonOffsetClamp },
    { "glPopDebugGroup",                               (PROC)_gen_glPopDebugGroup },
    { "glPopDebugGroupKHR",                            (PROC)_gen_glPopDebugGroup },
    { "glProgramBinary",                               (PROC)_gen_glProgramBinary },
    { "glProgramParameteri",                           (PROC)_gen_glProgramParameteri },
    { "glProgramParameteriARB",                        (PROC)_gen_glProgramParameteri },
    { "glProgramParameteriEXT",                        (PROC)_gen_glProgramParameteri },
    { "glProgramUniform1d",                            (PROC)_gen_glProgramUniform1d },
    { "glProgramUniform1dv",                           (PROC)_gen_glProgramUniform1dv },
    { "glProgramUniform1f",                            (PROC)_gen_glProgramUniform1f },
    { "glProgramUniform1fEXT",                         (PROC)_gen_glProgramUniform1f },
    { "glProgramUniform1fv",                           (PROC)_gen_glProgramUniform1fv },
    { "glProgramUniform1fvEXT",                        (PROC)_gen_glProgramUniform1fv },
    { "glProgramUniform1i",                            (PROC)_gen_glProgramUniform1i },
    { "glProgramUniform1iEXT",                         (PROC)_gen_glProgramUniform1i },
    { "glProgramUniform1iv",                           (PROC)_gen_glProgramUniform1iv },
    { "glProgramUniform1ivEXT",                        (PROC)_gen_glProgramUniform1iv },
    { "glProgramUniform1ui",                           (PROC)_gen_glProgramUniform1ui },
    { "glProgramUniform1uiEXT",                        (PROC)_gen_glProgramUniform1ui },
    { "glProgramUniform1uiv",                          (PROC)_gen_glProgramUniform1uiv },
    { "glProgramUniform1uivEXT",                       (PROC)_gen_glProgramUniform1uiv },
    { "glProgramUniform2d",                            (PROC)_gen_glProgramUniform2d },
    { "glProgramUniform2dv",                           (PROC)_gen_glProgramUniform2dv },
    { "glProgramUniform2f",                            (PROC)_gen_glProgramUniform2f },
    { "glProgramUniform2fEXT",                         (PROC)_gen_glProgramUniform2f },
    { "glProgramUniform2fv",                           (PROC)_gen_glProgramUniform2fv },
    { "glProgramUniform2fvEXT",                        (PROC)_gen_glProgramUniform2fv },
    { "glProgramUniform2i",                            (PROC)_gen_glProgramUniform2i },
    { "glProgramUniform2iEXT",                         (PROC)_gen_glProgramUniform2i },
    { "glProgramUniform2iv",                           (PROC)_gen_glProgramUniform2iv },
    { "glProgramUniform2ivEXT",                        (PROC)_gen_glProgramUniform2iv },
    { "glProgramUniform2ui",                           (PROC)_gen_glProgramUniform2ui },
    { "glProgramUniform2uiEXT",                        (PROC)_gen_glProgramUniform2ui },
    { "glProgramUniform2uiv",                          (PROC)_gen_glProgramUniform2uiv },
    { "glProgramUniform2uivEXT",                       (PROC)_gen_glProgramUniform2uiv },
    { "glProgramUniform3d",                            (PROC)_gen_glProgramUniform3d },
    { "glProgramUniform3dv",                           (PROC)_gen_glProgramUniform3dv },
    { "glProgramUniform3f",                            (PROC)_gen_glProgramUniform3f },
    { "glProgramUniform3fEXT",                         (PROC)_gen_glProgramUniform3f },
    { "glProgramUniform3fv",                           (PROC)_gen_glProgramUniform3fv },
    { "glProgramUniform3fvEXT",                        (PROC)_gen_glProgramUniform3fv },
    { "glProgramUniform3i",                            (PROC)_gen_glProgramUniform3i },
    { "glProgramUniform3iEXT",                         (PROC)_gen_glProgramUniform3i },
    { "glProgramUniform3iv",                           (PROC)_gen_glProgramUniform3iv },
    { "glProgramUniform3ivEXT",                        (PROC)_gen_glProgramUniform3iv },
    { "glProgramUniform3ui",                           (PROC)_gen_glProgramUniform3ui },
    { "glProgramUniform3uiEXT",                        (PROC)_gen_glProgramUniform3ui },
    { "glProgramUniform3uiv",                          (PROC)_gen_glProgramUniform3uiv },
    { "glProgramUniform3uivEXT",                       (PROC)_gen_glProgramUniform3uiv },
    { "glProgramUniform4d",                            (PROC)_gen_glProgramUniform4d },
    { "glProgramUniform4dv",                           (PROC)_gen_glProgramUniform4dv },
    { "glProgramUniform4f",                            (PROC)_gen_glProgramUniform4f },
    { "glProgramUniform4fEXT",                         (PROC)_gen_glProgramUniform4f },
    { "glProgramUniform4fv",                           (PROC)_gen_glProgramUniform4fv },
    { "glProgramUniform4fvEXT",                        (PROC)_gen_glProgramUniform4fv },
    { "glProgramUniform4i",                            (PROC)_gen_glProgramUniform4i },
    { "glProgramUniform4iEXT",                         (PROC)_gen_glProgramUniform4i },
    { "glProgramUniform4iv",                           (PROC)_gen_glProgramUniform4iv },
    { "glProgramUniform4ivEXT",                        (PROC)_gen_glProgramUniform4iv },
    { "glProgramUniform4ui",                           (PROC)_gen_glProgramUniform4ui },
    { "glProgramUniform4uiEXT",                        (PROC)_gen_glProgramUniform4ui },
    { "glProgramUniform4uiv",                          (PROC)_gen_glProgramUniform4uiv },
    { "glProgramUniform4uivEXT",                       (PROC)_gen_glProgramUniform4uiv },
    { "glProgramUniformMatrix2dv",                     (PROC)_gen_glProgramUniformMatrix2dv },
    { "glProgramUniformMatrix2fv",                     (PROC)_gen_glProgramUniformMatrix2fv },
    { "glProgramUniformMatrix2fvEXT",                  (PROC)_gen_glProgramUniformMatrix2fv },
    { "glProgramUniformMatrix2x3dv",                   (PROC)_gen_glProgramUniformMatrix2x3dv },
    { "glProgramUniformMatrix2x3fv",                   (PROC)_gen_glProgramUniformMatrix2x3fv },
    { "glProgramUniformMatrix2x3fvEXT",                (PROC)_gen_glProgramUniformMatrix2x3fv },
    { "glProgramUniformMatrix2x4dv",                   (PROC)_gen_glProgramUniformMatrix2x4dv },
    { "glProgramUniformMatrix2x4fv",                   (PROC)_gen_glProgramUniformMatrix2x4fv },
    { "glProgramUniformMatrix2x4fvEXT",                (PROC)_gen_glProgramUniformMatrix2x4fv },
    { "glProgramUniformMatrix3dv",                     (PROC)_gen_glProgramUniformMatrix3dv },
    { "glProgramUniformMatrix3fv",                     (PROC)_gen_glProgramUniformMatrix3fv },
    { "glProgramUniformMatrix3fvEXT",                  (PROC)_gen_glProgramUniformMatrix3fv },
    { "glProgramUniformMatrix3x2dv",                   (PROC)_gen_glProgramUniformMatrix3x2dv },
    { "glProgramUniformMatrix3x2fv",                   (PROC)_gen_glProgramUniformMatrix3x2fv },
    { "glProgramUniformMatrix3x2fvEXT",                (PROC)_gen_glProgramUniformMatrix3x2fv },
    { "glProgramUniformMatrix3x4dv",                   (PROC)_gen_glProgramUniformMatrix3x4dv },
    { "glProgramUniformMatrix3x4fv",                   (PROC)_gen_glProgramUniformMatrix3x4fv },
    { "glProgramUniformMatrix3x4fvEXT",                (PROC)_gen_glProgramUniformMatrix3x4fv },
    { "glProgramUniformMatrix4dv",                     (PROC)_gen_glProgramUniformMatrix4dv },
    { "glProgramUniformMatrix4fv",                     (PROC)_gen_glProgramUniformMatrix4fv },
    { "glProgramUniformMatrix4fvEXT",                  (PROC)_gen_glProgramUniformMatrix4fv },
    { "glProgramUniformMatrix4x2dv",                   (PROC)_gen_glProgramUniformMatrix4x2dv },
    { "glProgramUniformMatrix4x2fv",                   (PROC)_gen_glProgramUniformMatrix4x2fv },
    { "glProgramUniformMatrix4x2fvEXT",                (PROC)_gen_glProgramUniformMatrix4x2fv },
    { "glProgramUniformMatrix4x3dv",                   (PROC)_gen_glProgramUniformMatrix4x3dv },
    { "glProgramUniformMatrix4x3fv",                   (PROC)_gen_glProgramUniformMatrix4x3fv },
    { "glProgramUniformMatrix4x3fvEXT",                (PROC)_gen_glProgramUniformMatrix4x3fv },
    { "glProvokingVertexEXT",                          (PROC)_stub_glProvokingVertex },
    { "glPushDebugGroup",                              (PROC)_gen_glPushDebugGroup },
    { "glPushDebugGroupKHR",                           (PROC)_gen_glPushDebugGroup },
    { "glReadnPixels",                                 (PROC)_gen_glReadnPixels },
    { "glReadnPixelsARB",                              (PROC)_gen_glReadnPixels },
    { "glReadnPixelsKHR",                              (PROC)_gen_glReadnPixels },
    { "glReleaseShaderCompiler",                       (PROC)_gen_glReleaseShaderCompiler },
    { "glResumeTransformFeedback",                     (PROC)_gen_glResumeTransformFeedback },
    { "glResumeTransformFeedbackNV",                   (PROC)_gen_glResumeTransformFeedback },
    { "glSampleCoverageARB",                           (PROC)_stub_glSampleCoverage },
    { "glSamplerParameterIiv",                         (PROC)_gen_glSamplerParameterIiv },
    { "glSamplerParameterIuiv",                        (PROC)_gen_glSamplerParameterIuiv },
    { "glScissorArrayv",                               (PROC)_gen_glScissorArrayv },
    { "glScissorIndexed",                              (PROC)_gen_glScissorIndexed },
    { "glScissorIndexedv",                             (PROC)_gen_glScissorIndexedv },
    { "glSecondaryColor3b",                            (PROC)_gen_glSecondaryColor3b },
    { "glSecondaryColor3bEXT",                         (PROC)_gen_glSecondaryColor3b },
    { "glSecondaryColor3bv",                           (PROC)_gen_glSecondaryColor3bv },
    { "glSecondaryColor3bvEXT",                        (PROC)_gen_glSecondaryColor3bv },
    { "glSecondaryColor3d",                            (PROC)_gen_glSecondaryColor3d },
    { "glSecondaryColor3dEXT",                         (PROC)_gen_glSecondaryColor3d },
    { "glSecondaryColor3dv",                           (PROC)_gen_glSecondaryColor3dv },
    { "glSecondaryColor3dvEXT",                        (PROC)_gen_glSecondaryColor3dv },
    { "glSecondaryColor3fEXT",                         (PROC)_stub_glSecondaryColor3f },
    { "glSecondaryColor3i",                            (PROC)_gen_glSecondaryColor3i },
    { "glSecondaryColor3iEXT",                         (PROC)_gen_glSecondaryColor3i },
    { "glSecondaryColor3iv",                           (PROC)_gen_glSecondaryColor3iv },
    { "glSecondaryColor3ivEXT",                        (PROC)_gen_glSecondaryColor3iv },
    { "glSecondaryColor3s",                            (PROC)_gen_glSecondaryColor3s },
    { "glSecondaryColor3sEXT",                         (PROC)_gen_glSecondaryColor3s },
    { "glSecondaryColor3sv",                           (PROC)_gen_glSecondaryColor3sv },
    { "glSecondaryColor3svEXT",                        (PROC)_gen_glSecondaryColor3sv },
    { "glSecondaryColor3ubEXT",                        (PROC)_stub_glSecondaryColor3ub },
    { "glSecondaryColor3ubvEXT",                       (PROC)_stub_glSecondaryColor3ubv },
    { "glSecondaryColor3ui",                           (PROC)_gen_glSecondaryColor3ui },
    { "glSecondaryColor3uiEXT",                        (PROC)_gen_glSecondaryColor3ui },
    { "glSecondaryColor3uiv",                          (PROC)_gen_glSecondaryColor3uiv },
    { "glSecondaryColor3uivEXT",                       (PROC)_gen_glSecondaryColor3uiv },
    { "glSecondaryColor3us",                           (PROC)_gen_glSecondaryColor3us },
    { "glSecondaryColor3usEXT",                        (PROC)_gen_glSecondaryColor3us },
    { "glSecondaryColor3usv",                          (PROC)_gen_glSecondaryColor3usv },
    { "glSecondaryColor3usvEXT",                       (PROC)_gen_glSecondaryColor3usv },
    { "glSecondaryColorP3ui",                          (PROC)_gen_glSecondaryColorP3ui },
    { "glSecondaryColorP3uiv",                         (PROC)_gen_glSecondaryColorP3uiv },
    { "glSecondaryColorPointerEXT",                    (PROC)_stub_glSecondaryColorPointer },
    { "glShaderBinary",                                (PROC)_gen_glShaderBinary },
    { "glShaderStorageBlockBinding",                   (PROC)_gen_glShaderStorageBlockBinding },
    { "glSpecializeShader",                            (PROC)_gen_glSpecializeShader },
    { "glSpecializeShaderARB",                         (PROC)_gen_glSpecializeShader },
    { "glStencilOpSeparateATI",                        (PROC)_stub_glStencilOpSeparate },
    { "glTexBufferARB",                                (PROC)_stub_glTexBuffer },
    { "glTexBufferEXT",                                (PROC)_stub_glTexBuffer },
    { "glTexBufferRange",                              (PROC)_gen_glTexBufferRange },
    { "glTexCoordP1ui",                                (PROC)_gen_glTexCoordP1ui },
    { "glTexCoordP1uiv",                               (PROC)_gen_glTexCoordP1uiv },
    { "glTexCoordP2ui",                                (PROC)_gen_glTexCoordP2ui },
    { "glTexCoordP2uiv",                               (PROC)_gen_glTexCoordP2uiv },
    { "glTexCoordP3ui",                                (PROC)_gen_glTexCoordP3ui },
    { "glTexCoordP3uiv",                               (PROC)_gen_glTexCoordP3uiv },
    { "glTexCoordP4ui",                                (PROC)_gen_glTexCoordP4ui },
    { "glTexCoordP4uiv",                               (PROC)_gen_glTexCoordP4uiv },
    { "glTexParameterIiv",                             (PROC)_gen_glTexParameterIiv },
    { "glTexParameterIivEXT",                          (PROC)_gen_glTexParameterIiv },
    { "glTexParameterIuiv",                            (PROC)_gen_glTexParameterIuiv },
    { "glTexParameterIuivEXT",                         (PROC)_gen_glTexParameterIuiv },
    { "glTexStorage1D",                                (PROC)_gen_glTexStorage1D },
    { "glTexStorage1DEXT",                             (PROC)_gen_glTexStorage1D },
    { "glTexStorage2DEXT",                             (PROC)_stub_glTexStorage2D },
    { "glTexStorage2DMultisample",                     (PROC)_gen_glTexStorage2DMultisample },
    { "glTexStorage3DEXT",                             (PROC)_stub_glTexStorage3D },
    { "glTexStorage3DMultisample",                     (PROC)_gen_glTexStorage3DMultisample },
    { "glTexSubImage1DEXT",                            (PROC)_gen_glTexSubImage1D },
    { "glTexSubImage2DEXT",                            (PROC)_gen_glTexSubImage2D },
    { "glTextureBuffer",                               (PROC)_gen_glTextureBuffer },
    { "glTextureBufferRange",                          (PROC)_gen_glTextureBufferRange },
    { "glTextureParameterIiv",                         (PROC)_gen_glTextureParameterIiv },
    { "glTextureParameterIuiv",                        (PROC)_gen_glTextureParameterIuiv },
    { "glTextureParameterf",                           (PROC)_gen_glTextureParameterf },
    { "glTextureParameterfv",                          (PROC)_gen_glTextureParameterfv },
    { "glTextureParameteri",                           (PROC)_gen_glTextureParameteri },
    { "glTextureParameteriv",                          (PROC)_gen_glTextureParameteriv },
    { "glTextureStorage1D",                            (PROC)_gen_glTextureStorage1D },
    { "glTextureStorage2D",                            (PROC)_gen_glTextureStorage2D },
    { "glTextureStorage2DMultisample",                 (PROC)_gen_glTextureStorage2DMultisample },
    { "glTextureStorage3D",                            (PROC)_gen_glTextureStorage3D },
    { "glTextureStorage3DMultisample",                 (PROC)_gen_glTextureStorage3DMultisample },
    { "glTextureSubImage1D",                           (PROC)_gen_glTextureSubImage1D },
    { "glTextureSubImage2D",                           (PROC)_gen_glTextureSubImage2D },
    { "glTextureSubImage3D",                           (PROC)_gen_glTextureSubImage3D },
    { "glTextureView",                                 (PROC)_gen_glTextureView },
    { "glTransformFeedbackBufferBase",                 (PROC)_gen_glTransformFeedbackBufferBase },
    { "glTransformFeedbackBufferRange",                (PROC)_gen_glTransformFeedbackBufferRange },
    { "glTransformFeedbackVaryingsEXT",                (PROC)_stub_glTransformFeedbackVaryings },
    { "glUniform1d",                                   (PROC)_gen_glUniform1d },
    { "glUniform1dv",                                  (PROC)_gen_glUniform1dv },
    { "glUniform1ivARB",                               (PROC)_stub_glUniform1iv },
    { "glUniform1ui",                                  (PROC)_gen_glUniform1ui },
    { "glUniform1uiEXT",                               (PROC)_gen_glUniform1ui },
    { "glUniform1uiv",                                 (PROC)_gen_glUniform1uiv },
    { "glUniform1uivEXT",                              (PROC)_gen_glUniform1uiv },
    { "glUniform2d",                                   (PROC)_gen_glUniform2d },
    { "glUniform2dv",                                  (PROC)_gen_glUniform2dv },
    { "glUniform2fARB",                                (PROC)_stub_glUniform2f },
    { "glUniform2iARB",                                (PROC)_stub_glUniform2i },
    { "glUniform2ivARB",                               (PROC)_stub_glUniform2iv },
    { "glUniform2ui",                                  (PROC)_gen_glUniform2ui },
    { "glUniform2uiEXT",                               (PROC)_gen_glUniform2ui },
    { "glUniform2uiv",                                 (PROC)_gen_glUniform2uiv },
    { "glUniform2uivEXT",                              (PROC)_gen_glUniform2uiv },
    { "glUniform3d",                                   (PROC)_gen_glUniform3d },
    { "glUniform3dv",                                  (PROC)_gen_glUniform3dv },
    { "glUniform3fARB",                                (PROC)_stub_glUniform3f },
    { "glUniform3iARB",                                (PROC)_stub_glUniform3i },
    { "glUniform3ivARB",                               (PROC)_stub_glUniform3iv },
    { "glUniform3ui",                                  (PROC)_gen_glUniform3ui },
    { "glUniform3uiEXT",                               (PROC)_gen_glUniform3ui },
    { "glUniform3uiv",                                 (PROC)_gen_glUniform3uiv },
    { "glUniform3uivEXT",                              (PROC)_gen_glUniform3uiv },
    { "glUniform4d",                                   (PROC)_gen_glUniform4d },
    { "glUniform4dv",                                  (PROC)_gen_glUniform4dv },
    { "glUniform4fARB",                                (PROC)_stub_glUniform4f },
    { "glUniform4iARB",                                (PROC)_stub_glUniform4i },
    { "glUniform4ivARB",                               (PROC)_stub_glUniform4iv },
    { "glUniform4ui",                                  (PROC)_gen_glUniform4ui },
    { "glUniform4uiEXT",                               (PROC)_gen_glUniform4ui },
    { "glUniform4uiv",                                 (PROC)_gen_glUniform4uiv },
    { "glUniform4uivEXT",                              (PROC)_gen_glUniform4uiv },
    { "glUniformMatrix2dv",                            (PROC)_gen_glUniformMatrix2dv },
    { "glUniformMatrix2fvARB",                         (PROC)_stub_glUniformMatrix2fv },
    { "glUniformMatrix2x3dv",                          (PROC)_gen_glUniformMatrix2x3dv },
    { "glUniformMatrix2x3fv",                          (PROC)_gen_glUniformMatrix2x3fv },
    { "glUniformMatrix2x4dv",                          (PROC)_gen_glUniformMatrix2x4dv },
    { "glUniformMatrix2x4fv",                          (PROC)_gen_glUniformMatrix2x4fv },
    { "glUniformMatrix3dv",                            (PROC)_gen_glUniformMatrix3dv },
    { "glUniformMatrix3fvARB",                         (PROC)_stub_glUniformMatrix3fv },
    { "glUniformMatrix3x2dv",                          (PROC)_gen_glUniformMatrix3x2dv },
    { "glUniformMatrix3x2fv",                          (PROC)_gen_glUniformMatrix3x2fv },
    { "glUniformMatrix3x4dv",                          (PROC)_gen_glUniformMatrix3x4dv },
    { "glUniformMatrix3x4fv",                          (PROC)_gen_glUniformMatrix3x4fv },
    { "glUniformMatrix4dv",                            (PROC)_gen_glUniformMatrix4dv },
    { "glUniformMatrix4fvARB",                         (PROC)_stub_glUniformMatrix4fv },
    { "glUniformMatrix4x2dv",                          (PROC)_gen_glUniformMatrix4x2dv },
    { "glUniformMatrix4x2fv",                          (PROC)_gen_glUniformMatrix4x2fv },
    { "glUniformMatrix4x3dv",                          (PROC)_gen_glUniformMatrix4x3dv },
    { "glUniformMatrix4x3fv",                          (PROC)_gen_glUniformMatrix4x3fv },
    { "glUniformSubroutinesuiv",                       (PROC)_gen_glUniformSubroutinesuiv },
    { "glUnmapNamedBuffer",                            (PROC)_gen_glUnmapNamedBuffer },
    { "glUseProgramStages",                            (PROC)_gen_glUseProgramStages },
    { "glValidateProgramARB",                          (PROC)_stub_glValidateProgram },
    { "glValidateProgramPipeline",                     (PROC)_gen_glValidateProgramPipeline },
    { "glVertexArrayAttribBinding",                    (PROC)_gen_glVertexArrayAttribBinding },
    { "glVertexArrayAttribFormat",                     (PROC)_gen_glVertexArrayAttribFormat },
    { "glVertexArrayAttribIFormat",                    (PROC)_gen_glVertexArrayAttribIFormat },
    { "glVertexArrayAttribLFormat",                    (PROC)_gen_glVertexArrayAttribLFormat },
    { "glVertexArrayBindingDivisor",                   (PROC)_gen_glVertexArrayBindingDivisor },
    { "glVertexArrayElementBuffer",                    (PROC)_gen_glVertexArrayElementBuffer },
    { "glVertexArrayVertexBuffer",                     (PROC)_gen_glVertexArrayVertexBuffer },
    { "glVertexArrayVertexBuffers",                    (PROC)_gen_glVertexArrayVertexBuffers },
    { "glVertexAttrib1d",                              (PROC)_gen_glVertexAttrib1d },
    { "glVertexAttrib1dARB",                           (PROC)_gen_glVertexAttrib1d },
    { "glVertexAttrib1dNV",                            (PROC)_gen_glVertexAttrib1d },
    { "glVertexAttrib1dv",                             (PROC)_gen_glVertexAttrib1dv },
    { "glVertexAttrib1dvARB",                          (PROC)_gen_glVertexAttrib1dv },
    { "glVertexAttrib1dvNV",                           (PROC)_gen_glVertexAttrib1dv },
    { "glVertexAttrib1fARB",                           (PROC)_stub_glVertexAttrib1f },
    { "glVertexAttrib1fNV",                            (PROC)_stub_glVertexAttrib1f },
    { "glVertexAttrib1fv",                             (PROC)_gen_glVertexAttrib1fv },
    { "glVertexAttrib1fvARB",                          (PROC)_gen_glVertexAttrib1fv },
    { "glVertexAttrib1fvNV",                           (PROC)_gen_glVertexAttrib1fv },
    { "glVertexAttrib1s",                              (PROC)_gen_glVertexAttrib1s },
    { "glVertexAttrib1sARB",                           (PROC)_gen_glVertexAttrib1s },
    { "glVertexAttrib1sNV",                            (PROC)_gen_glVertexAttrib1s },
    { "glVertexAttrib1sv",                             (PROC)_gen_glVertexAttrib1sv },
    { "glVertexAttrib1svARB",                          (PROC)_gen_glVertexAttrib1sv },
    { "glVertexAttrib1svNV",                           (PROC)_gen_glVertexAttrib1sv },
    { "glVertexAttrib2d",                              (PROC)_gen_glVertexAttrib2d },
    { "glVertexAttrib2dARB",                           (PROC)_gen_glVertexAttrib2d },
    { "glVertexAttrib2dNV",                            (PROC)_gen_glVertexAttrib2d },
    { "glVertexAttrib2dv",                             (PROC)_gen_glVertexAttrib2dv },
    { "glVertexAttrib2dvARB",                          (PROC)_gen_glVertexAttrib2dv },
    { "glVertexAttrib2dvNV",                           (PROC)_gen_glVertexAttrib2dv },
    { "glVertexAttrib2fARB",                           (PROC)_stub_glVertexAttrib2f },
    { "glVertexAttrib2fNV",                            (PROC)_stub_glVertexAttrib2f },
    { "glVertexAttrib2fv",                             (PROC)_gen_glVertexAttrib2fv },
    { "glVertexAttrib2fvARB",                          (PROC)_gen_glVertexAttrib2fv },
    { "glVertexAttrib2fvNV",                           (PROC)_gen_glVertexAttrib2fv },
    { "glVertexAttrib2s",                              (PROC)_gen_glVertexAttrib2s },
    { "glVertexAttrib2sARB",                           (PROC)_gen_glVertexAttrib2s },
    { "glVertexAttrib2sNV",                            (PROC)_gen_glVertexAttrib2s },
    { "glVertexAttrib2sv",                             (PROC)_gen_glVertexAttrib2sv },
    { "glVertexAttrib2svARB",                          (PROC)_gen_glVertexAttrib2sv },
    { "glVertexAttrib2svNV",                           (PROC)_gen_glVertexAttrib2sv },
    { "glVertexAttrib3d",                              (PROC)_gen_glVertexAttrib3d },
    { "glVertexAttrib3dARB",                           (PROC)_gen_glVertexAttrib3d },
    { "glVertexAttrib3dNV",                            (PROC)_gen_glVertexAttrib3d },
    { "glVertexAttrib3dv",                             (PROC)_gen_glVertexAttrib3dv },
    { "glVertexAttrib3dvARB",                          (PROC)_gen_glVertexAttrib3dv },
    { "glVertexAttrib3dvNV",                           (PROC)_gen_glVertexAttrib3dv },
    { "glVertexAttrib3fARB",                           (PROC)_stub_glVertexAttrib3f },
    { "glVertexAttrib3fNV",                            (PROC)_stub_glVertexAttrib3f },
    { "glVertexAttrib3fv",                             (PROC)_gen_glVertexAttrib3fv },
    { "glVertexAttrib3fvARB",                          (PROC)_gen_glVertexAttrib3fv },
    { "glVertexAttrib3fvNV",                           (PROC)_gen_glVertexAttrib3fv },
    { "glVertexAttrib3s",                              (PROC)_gen_glVertexAttrib3s },
    { "glVertexAttrib3sARB",                           (PROC)_gen_glVertexAttrib3s },
    { "glVertexAttrib3sNV",                            (PROC)_gen_glVertexAttrib3s },
    { "glVertexAttrib3sv",                             (PROC)_gen_glVertexAttrib3sv },
    { "glVertexAttrib3svARB",                          (PROC)_gen_glVertexAttrib3sv },
    { "glVertexAttrib3svNV",                           (PROC)_gen_glVertexAttrib3sv },
    { "glVertexAttrib4Nbv",                            (PROC)_gen_glVertexAttrib4Nbv },
    { "glVertexAttrib4NbvARB",                         (PROC)_gen_glVertexAttrib4Nbv },
    { "glVertexAttrib4Niv",                            (PROC)_gen_glVertexAttrib4Niv },
    { "glVertexAttrib4NivARB",                         (PROC)_gen_glVertexAttrib4Niv },
    { "glVertexAttrib4Nsv",                            (PROC)_gen_glVertexAttrib4Nsv },
    { "glVertexAttrib4NsvARB",                         (PROC)_gen_glVertexAttrib4Nsv },
    { "glVertexAttrib4Nub",                            (PROC)_gen_glVertexAttrib4Nub },
    { "glVertexAttrib4NubARB",                         (PROC)_gen_glVertexAttrib4Nub },
    { "glVertexAttrib4Nubv",                           (PROC)_gen_glVertexAttrib4Nubv },
    { "glVertexAttrib4NubvARB",                        (PROC)_gen_glVertexAttrib4Nubv },
    { "glVertexAttrib4Nuiv",                           (PROC)_gen_glVertexAttrib4Nuiv },
    { "glVertexAttrib4NuivARB",                        (PROC)_gen_glVertexAttrib4Nuiv },
    { "glVertexAttrib4Nusv",                           (PROC)_gen_glVertexAttrib4Nusv },
    { "glVertexAttrib4NusvARB",                        (PROC)_gen_glVertexAttrib4Nusv },
    { "glVertexAttrib4bv",                             (PROC)_gen_glVertexAttrib4bv },
    { "glVertexAttrib4bvARB",                          (PROC)_gen_glVertexAttrib4bv },
    { "glVertexAttrib4d",                              (PROC)_gen_glVertexAttrib4d },
    { "glVertexAttrib4dARB",                           (PROC)_gen_glVertexAttrib4d },
    { "glVertexAttrib4dNV",                            (PROC)_gen_glVertexAttrib4d },
    { "glVertexAttrib4dv",                             (PROC)_gen_glVertexAttrib4dv },
    { "glVertexAttrib4dvARB",                          (PROC)_gen_glVertexAttrib4dv },
    { "glVertexAttrib4dvNV",                           (PROC)_gen_glVertexAttrib4dv },
    { "glVertexAttrib4fARB",                           (PROC)_stub_glVertexAttrib4f },
    { "glVertexAttrib4fNV",                            (PROC)_stub_glVertexAttrib4f },
    { "glVertexAttrib4fv",                             (PROC)_gen_glVertexAttrib4fv },
    { "glVertexAttrib4fvARB",                          (PROC)_gen_glVertexAttrib4fv },
    { "glVertexAttrib4fvNV",                           (PROC)_gen_glVertexAttrib4fv },
    { "glVertexAttrib4iv",                             (PROC)_gen_glVertexAttrib4iv },
    { "glVertexAttrib4ivARB",                          (PROC)_gen_glVertexAttrib4iv },
    { "glVertexAttrib4s",                              (PROC)_gen_glVertexAttrib4s },
    { "glVertexAttrib4sARB",                           (PROC)_gen_glVertexAttrib4s },
    { "glVertexAttrib4sNV",                            (PROC)_gen_glVertexAttrib4s },
    { "glVertexAttrib4sv",                             (PROC)_gen_glVertexAttrib4sv },
    { "glVertexAttrib4svARB",                          (PROC)_gen_glVertexAttrib4sv },
    { "glVertexAttrib4svNV",                           (PROC)_gen_glVertexAttrib4sv },
    { "glVertexAttrib4ubNV",                           (PROC)_gen_glVertexAttrib4Nub },
    { "glVertexAttrib4ubv",                            (PROC)_gen_glVertexAttrib4ubv },
    { "glVertexAttrib4ubvARB",                         (PROC)_gen_glVertexAttrib4ubv },
    { "glVertexAttrib4ubvNV",                          (PROC)_gen_glVertexAttrib4Nubv },
    { "glVertexAttrib4uiv",                            (PROC)_gen_glVertexAttrib4uiv },
    { "glVertexAttrib4uivARB",                         (PROC)_gen_glVertexAttrib4uiv },
    { "glVertexAttrib4usv",                            (PROC)_gen_glVertexAttrib4usv },
    { "glVertexAttrib4usvARB",                         (PROC)_gen_glVertexAttrib4usv },
    { "glVertexAttribBinding",                         (PROC)_gen_glVertexAttribBinding },
    { "glVertexAttribDivisorARB",                      (PROC)_stub_glVertexAttribDivisor },
    { "glVertexAttribFormat",                          (PROC)_gen_glVertexAttribFormat },
    { "glVertexAttribI1i",                             (PROC)_gen_glVertexAttribI1i },
    { "glVertexAttribI1iEXT",                          (PROC)_gen_glVertexAttribI1i },
    { "glVertexAttribI1iv",                            (PROC)_gen_glVertexAttribI1iv },
    { "glVertexAttribI1ivEXT",                         (PROC)_gen_glVertexAttribI1iv },
    { "glVertexAttribI1ui",                            (PROC)_gen_glVertexAttribI1ui },
    { "glVertexAttribI1uiEXT",                         (PROC)_gen_glVertexAttribI1ui },
    { "glVertexAttribI1uiv",                           (PROC)_gen_glVertexAttribI1uiv },
    { "glVertexAttribI1uivEXT",                        (PROC)_gen_glVertexAttribI1uiv },
    { "glVertexAttribI2i",                             (PROC)_gen_glVertexAttribI2i },
    { "glVertexAttribI2iEXT",                          (PROC)_gen_glVertexAttribI2i },
    { "glVertexAttribI2iv",                            (PROC)_gen_glVertexAttribI2iv },
    { "glVertexAttribI2ivEXT",                         (PROC)_gen_glVertexAttribI2iv },
    { "glVertexAttribI2ui",                            (PROC)_gen_glVertexAttribI2ui },
    { "glVertexAttribI2uiEXT",                         (PROC)_gen_glVertexAttribI2ui },
    { "glVertexAttribI2uiv",                           (PROC)_gen_glVertexAttribI2uiv },
    { "glVertexAttribI2uivEXT",                        (PROC)_gen_glVertexAttribI2uiv },
    { "glVertexAttribI3i",                             (PROC)_gen_glVertexAttribI3i },
    { "glVertexAttribI3iEXT",                          (PROC)_gen_glVertexAttribI3i },
    { "glVertexAttribI3iv",                            (PROC)_gen_glVertexAttribI3iv },
    { "glVertexAttribI3ivEXT",                         (PROC)_gen_glVertexAttribI3iv },
    { "glVertexAttribI3ui",                            (PROC)_gen_glVertexAttribI3ui },
    { "glVertexAttribI3uiEXT",                         (PROC)_gen_glVertexAttribI3ui },
    { "glVertexAttribI3uiv",                           (PROC)_gen_glVertexAttribI3uiv },
    { "glVertexAttribI3uivEXT",                        (PROC)_gen_glVertexAttribI3uiv },
    { "glVertexAttribI4bv",                            (PROC)_gen_glVertexAttribI4bv },
    { "glVertexAttribI4bvEXT",                         (PROC)_gen_glVertexAttribI4bv },
    { "glVertexAttribI4i",                             (PROC)_gen_glVertexAttribI4i },
    { "glVertexAttribI4iEXT",                          (PROC)_gen_glVertexAttribI4i },
    { "glVertexAttribI4iv",                            (PROC)_gen_glVertexAttribI4iv },
    { "glVertexAttribI4ivEXT",                         (PROC)_gen_glVertexAttribI4iv },
    { "glVertexAttribI4sv",                            (PROC)_gen_glVertexAttribI4sv },
    { "glVertexAttribI4svEXT",                         (PROC)_gen_glVertexAttribI4sv },
    { "glVertexAttribI4ubv",                           (PROC)_gen_glVertexAttribI4ubv },
    { "glVertexAttribI4ubvEXT",                        (PROC)_gen_glVertexAttribI4ubv },
    { "glVertexAttribI4ui",                            (PROC)_gen_glVertexAttribI4ui },
    { "glVertexAttribI4uiEXT",                         (PROC)_gen_glVertexAttribI4ui },
    { "glVertexAttribI4uiv",                           (PROC)_gen_glVertexAttribI4uiv },
    { "glVertexAttribI4uivEXT",                        (PROC)_gen_glVertexAttribI4uiv },
    { "glVertexAttribI4usv",                           (PROC)_gen_glVertexAttribI4usv },
    { "glVertexAttribI4usvEXT",                        (PROC)_gen_glVertexAttribI4usv },
    { "glVertexAttribIFormat",                         (PROC)_gen_glVertexAttribIFormat },
    { "glVertexAttribIPointerEXT",                     (PROC)_stub_glVertexAttribIPointer },
    { "glVertexAttribL1d",                             (PROC)_gen_glVertexAttribL1d },
    { "glVertexAttribL1dEXT",                          (PROC)_gen_glVertexAttribL1d },
    { "glVertexAttribL1dv",                            (PROC)_gen_glVertexAttribL1dv },
    { "glVertexAttribL1dvEXT",                         (PROC)_gen_glVertexAttribL1dv },
    { "glVertexAttribL2d",                             (PROC)_gen_glVertexAttribL2d },
    { "glVertexAttribL2dEXT",                          (PROC)_gen_glVertexAttribL2d },
    { "glVertexAttribL2dv",                            (PROC)_gen_glVertexAttribL2dv },
    { "glVertexAttribL2dvEXT",                         (PROC)_gen_glVertexAttribL2dv },
    { "glVertexAttribL3d",                             (PROC)_gen_glVertexAttribL3d },
    { "glVertexAttribL3dEXT",                          (PROC)_gen_glVertexAttribL3d },
    { "glVertexAttribL3dv",                            (PROC)_gen_glVertexAttribL3dv },
    { "glVertexAttribL3dvEXT",                         (PROC)_gen_glVertexAttribL3dv },
    { "glVertexAttribL4d",                             (PROC)_gen_glVertexAttribL4d },
    { "glVertexAttribL4dEXT",                          (PROC)_gen_glVertexAttribL4d },
    { "glVertexAttribL4dv",                            (PROC)_gen_glVertexAttribL4dv },
    { "glVertexAttribL4dvEXT",                         (PROC)_gen_glVertexAttribL4dv },
    { "glVertexAttribLFormat",                         (PROC)_gen_glVertexAttribLFormat },
    { "glVertexAttribLPointer",                        (PROC)_gen_glVertexAttribLPointer },
    { "glVertexAttribLPointerEXT",                     (PROC)_gen_glVertexAttribLPointer },
    { "glVertexAttribP1ui",                            (PROC)_gen_glVertexAttribP1ui },
    { "glVertexAttribP1uiv",                           (PROC)_gen_glVertexAttribP1uiv },
    { "glVertexAttribP2ui",                            (PROC)_gen_glVertexAttribP2ui },
    { "glVertexAttribP2uiv",                           (PROC)_gen_glVertexAttribP2uiv },
    { "glVertexAttribP3ui",                            (PROC)_gen_glVertexAttribP3ui },
    { "glVertexAttribP3uiv",                           (PROC)_gen_glVertexAttribP3uiv },
    { "glVertexAttribP4ui",                            (PROC)_gen_glVertexAttribP4ui },
    { "glVertexAttribP4uiv",                           (PROC)_gen_glVertexAttribP4uiv },
    { "glVertexBindingDivisor",                        (PROC)_gen_glVertexBindingDivisor },
    { "glVertexP2ui",                                  (PROC)_gen_glVertexP2ui },
    { "glVertexP2uiv",                                 (PROC)_gen_glVertexP2uiv },
    { "glVertexP3ui",                                  (PROC)_gen_glVertexP3ui },
    { "glVertexP3uiv",                                 (PROC)_gen_glVertexP3uiv },
    { "glVertexP4ui",                                  (PROC)_gen_glVertexP4ui },
    { "glVertexP4uiv",                                 (PROC)_gen_glVertexP4uiv },
    { "glViewportArrayv",                              (PROC)_gen_glViewportArrayv },
    { "glViewportIndexedf",                            (PROC)_gen_glViewportIndexedf },
    { "glViewportIndexedfv",                           (PROC)_gen_glViewportIndexedfv },
    { "glWindowPos2d",                                 (PROC)_gen_glWindowPos2d },
    { "glWindowPos2dARB",                              (PROC)_gen_glWindowPos2d },
    { "glWindowPos2dv",                                (PROC)_gen_glWindowPos2dv },
    { "glWindowPos2dvARB",                             (PROC)_gen_glWindowPos2dv },
    { "glWindowPos2f",                                 (PROC)_gen_glWindowPos2f },
    { "glWindowPos2fARB",                              (PROC)_gen_glWindowPos2f },
    { "glWindowPos2fv",                                (PROC)_gen_glWindowPos2fv },
    { "glWindowPos2fvARB",                             (PROC)_gen_glWindowPos2fv },
    { "glWindowPos2i",                                 (PROC)_gen_glWindowPos2i },
    { "glWindowPos2iARB",                              (PROC)_gen_glWindowPos2i },
    { "glWindowPos2iv",                                (PROC)_gen_glWindowPos2iv },
    { "glWindowPos2ivARB",                             (PROC)_gen_glWindowPos2iv },
    { "glWindowPos2s",                                 (PROC)_gen_glWindowPos2s },
    { "glWindowPos2sARB",                              (PROC)_gen_glWindowPos2s },
    { "glWindowPos2sv",                                (PROC)_gen_glWindowPos2sv },
    { "glWindowPos2svARB",                             (PROC)_gen_glWindowPos2sv },
    { "glWindowPos3d",                                 (PROC)_gen_glWindowPos3d },
    { "glWindowPos3dARB",                              (PROC)_gen_glWindowPos3d },
    { "glWindowPos3dv",                                (PROC)_gen_glWindowPos3dv },
    { "glWindowPos3dvARB",                             (PROC)_gen_glWindowPos3dv },
    { "glWindowPos3f",                                 (PROC)_gen_glWindowPos3f },
    { "glWindowPos3fARB",                              (PROC)_gen_glWindowPos3f },
    { "glWindowPos3fv",                                (PROC)_gen_glWindowPos3fv },
    { "glWindowPos3fvARB",                             (PROC)_gen_glWindowPos3fv },
    { "glWindowPos3i",                                 (PROC)_gen_glWindowPos3i },
    { "glWindowPos3iARB",                              (PROC)_gen_glWindowPos3i },
    { "glWindowPos3iv",                                (PROC)_gen_glWindowPos3iv },
    { "glWindowPos3ivARB",                             (PROC)_gen_glWindowPos3iv },
    { "glWindowPos3s",                                 (PROC)_gen_glWindowPos3s },
    { "glWindowPos3sARB",                              (PROC)_gen_glWindowPos3s },
    { "glWindowPos3sv",                                (PROC)_gen_glWindowPos3sv },
    { "glWindowPos3svARB",                             (PROC)_gen_glWindowPos3sv },
    /* Sentinel */
    { NULL, NULL }
};

#ifdef __cplusplus
}
#endif

#endif /* GL_GENERATED_STUBS_H */
