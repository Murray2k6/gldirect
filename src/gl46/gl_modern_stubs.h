/*********************************************************************************
*
*  gl_modern_stubs.h - No-op stubs for GL 2.0+ functions
*
*  Apps query these via wglGetProcAddress. Returning a non-NULL function pointer
*  tells the app the function is "supported". The actual DX9 translation will
*  replace these stubs as each function is implemented.
*
*  Covers: GL 2.0, 2.1, 3.0, 3.1, 3.2, 3.3, 4.0-4.6 core functions.
*
*********************************************************************************/

#ifndef GL_MODERN_STUBS_H
#define GL_MODERN_STUBS_H

#include <windows.h>
#include <glad/gl.h>
#include "gl_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== GL 2.0 — Shaders ===== */
static GLuint  APIENTRY _stub_glCreateShader(GLenum type) { return _glsCreateShader(type); }
static void    APIENTRY _stub_glDeleteShader(GLuint shader) { _glsDeleteShader(shader); }
static void    APIENTRY _stub_glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) { _glsShaderSource(shader, count, string, length); }
static void    APIENTRY _stub_glCompileShader(GLuint shader) { _glsCompileShader(shader); }
static void    APIENTRY _stub_glGetShaderiv(GLuint shader, GLenum pname, GLint *params) { _glsGetShaderiv(shader, pname, params); }
static void    APIENTRY _stub_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) { _glsGetShaderInfoLog(shader, bufSize, length, infoLog); }
static GLuint  APIENTRY _stub_glCreateProgram(void) { return _glsCreateProgram(); }
static void    APIENTRY _stub_glDeleteProgram(GLuint program) { _glsDeleteProgram(program); }
static void    APIENTRY _stub_glAttachShader(GLuint program, GLuint shader) { _glsAttachShader(program, shader); }
static void    APIENTRY _stub_glDetachShader(GLuint program, GLuint shader) { (void)program; (void)shader; }
static void    APIENTRY _stub_glLinkProgram(GLuint program) { _glsLinkProgram(program); }
static void    APIENTRY _stub_glUseProgram(GLuint program) { _glsUseProgram(program); }
static void    APIENTRY _stub_glGetProgramiv(GLuint program, GLenum pname, GLint *params) { _glsGetProgramiv(program, pname, params); }
static void    APIENTRY _stub_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) { _glsGetProgramInfoLog(program, bufSize, length, infoLog); }
static void    APIENTRY _stub_glValidateProgram(GLuint program) { _glsValidateProgram(program); }
static GLboolean APIENTRY _stub_glIsShader(GLuint shader) { return _glsIsShader(shader); }
static GLboolean APIENTRY _stub_glIsProgram(GLuint program) { return _glsIsProgram(program); }
static GLint   APIENTRY _stub_glGetUniformLocation(GLuint program, const GLchar *name) { return -1; }
static GLint   APIENTRY _stub_glGetAttribLocation(GLuint program, const GLchar *name) { return -1; }
static void    APIENTRY _stub_glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) { _glsBindAttribLocation(program, index, name); }
static void    APIENTRY _stub_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) { if(length) *length=0; }
static void    APIENTRY _stub_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) { if(length) *length=0; }

/* ===== GL 2.0 — Uniforms ===== */
static void APIENTRY _stub_glUniform1i(GLint loc, GLint v0) { _glsUniform1i(loc, v0); }
static void APIENTRY _stub_glUniform2i(GLint loc, GLint v0, GLint v1) { _glsUniform2i(loc, v0, v1); }
static void APIENTRY _stub_glUniform3i(GLint loc, GLint v0, GLint v1, GLint v2) { _glsUniform3i(loc, v0, v1, v2); }
static void APIENTRY _stub_glUniform4i(GLint loc, GLint v0, GLint v1, GLint v2, GLint v3) { _glsUniform4i(loc, v0, v1, v2, v3); }
static void APIENTRY _stub_glUniform1f(GLint loc, GLfloat v0) { _glsUniform1f(loc, v0); }
static void APIENTRY _stub_glUniform2f(GLint loc, GLfloat v0, GLfloat v1) { _glsUniform2f(loc, v0, v1); }
static void APIENTRY _stub_glUniform3f(GLint loc, GLfloat v0, GLfloat v1, GLfloat v2) { _glsUniform3f(loc, v0, v1, v2); }
static void APIENTRY _stub_glUniform4f(GLint loc, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { _glsUniform4f(loc, v0, v1, v2, v3); }
static void APIENTRY _stub_glUniform1iv(GLint loc, GLsizei count, const GLint *v) { _glsUniform1iv(loc, count, v); }
static void APIENTRY _stub_glUniform2iv(GLint loc, GLsizei count, const GLint *v) { _glsUniform2iv(loc, count, v); }
static void APIENTRY _stub_glUniform3iv(GLint loc, GLsizei count, const GLint *v) { _glsUniform3iv(loc, count, v); }
static void APIENTRY _stub_glUniform4iv(GLint loc, GLsizei count, const GLint *v) { _glsUniform4iv(loc, count, v); }
static void APIENTRY _stub_glUniform1fv(GLint loc, GLsizei count, const GLfloat *v) { _glsUniform1fv(loc, count, v); }
static void APIENTRY _stub_glUniform2fv(GLint loc, GLsizei count, const GLfloat *v) { _glsUniform2fv(loc, count, v); }
static void APIENTRY _stub_glUniform3fv(GLint loc, GLsizei count, const GLfloat *v) { _glsUniform3fv(loc, count, v); }
static void APIENTRY _stub_glUniform4fv(GLint loc, GLsizei count, const GLfloat *v) { _glsUniform4fv(loc, count, v); }
static void APIENTRY _stub_glUniformMatrix2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v) { _glsUniformMatrix2fv(loc, count, transpose, v); }
static void APIENTRY _stub_glUniformMatrix3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v) { _glsUniformMatrix3fv(loc, count, transpose, v); }
static void APIENTRY _stub_glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v) { _glsUniformMatrix4fv(loc, count, transpose, v); }

/* ===== GL 2.0 — Vertex attribs ===== */
static void APIENTRY _stub_glVertexAttrib1f(GLuint index, GLfloat x) { _glsVertexAttrib1f(index, x); }
static void APIENTRY _stub_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { _glsVertexAttrib2f(index, x, y); }
static void APIENTRY _stub_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { _glsVertexAttrib3f(index, x, y, z); }
static void APIENTRY _stub_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { _glsVertexAttrib4f(index, x, y, z, w); }
static void APIENTRY _stub_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) { _glsVertexAttribPointer(index, size, type, normalized, stride, pointer); }
static void APIENTRY _stub_glEnableVertexAttribArray(GLuint index) { _glsEnableVertexAttribArray(index); }
static void APIENTRY _stub_glDisableVertexAttribArray(GLuint index) { _glsDisableVertexAttribArray(index); }

/* ===== GL 2.0 — Other ===== */
static void APIENTRY _stub_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) { _glsBlendEquationSeparate(modeRGB, modeAlpha); }
static void APIENTRY _stub_glBlendFuncSeparate(GLenum sfRGB, GLenum dfRGB, GLenum sfA, GLenum dfA) { _glsBlendFuncSeparate(sfRGB, dfRGB, sfA, dfA); }
static void APIENTRY _stub_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) { _glsStencilFuncSeparate(face, func, ref, mask); }
static void APIENTRY _stub_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) { _glsStencilOpSeparate(face, sfail, dpfail, dppass); }
static void APIENTRY _stub_glStencilMaskSeparate(GLenum face, GLuint mask) { _glsStencilMaskSeparate(face, mask); }
static void APIENTRY _stub_glDrawBuffers(GLsizei n, const GLenum *bufs) { _glsDrawBuffers(n, bufs); }

/* ===== GL 1.5 / 2.0 — Buffer Objects ===== */
static void    APIENTRY _stub_glGenBuffers(GLsizei n, GLuint *buffers) { _glsGenBuffers(n, buffers); }
static void    APIENTRY _stub_glDeleteBuffers(GLsizei n, const GLuint *buffers) { _glsDeleteBuffers(n, buffers); }
static void    APIENTRY _stub_glBindBuffer(GLenum target, GLuint buffer) { _glsBindBuffer(target, buffer); }
static void    APIENTRY _stub_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { _glsBufferData(target, size, data, usage); }
static void    APIENTRY _stub_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { _glsBufferSubData(target, offset, size, data); }
static void*   APIENTRY _stub_glMapBuffer(GLenum target, GLenum access) { return _glsMapBuffer(target, access); }
static GLboolean APIENTRY _stub_glUnmapBuffer(GLenum target) { return _glsUnmapBuffer(target); }
static GLboolean APIENTRY _stub_glIsBuffer(GLuint buffer) { return GL_FALSE; }
static void    APIENTRY _stub_glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) { if(params) *params=0; }

/* ===== GL 3.0 — VAO ===== */
static void    APIENTRY _stub_glGenVertexArrays(GLsizei n, GLuint *arrays) { _glsGenVertexArrays(n, arrays); }
static void    APIENTRY _stub_glDeleteVertexArrays(GLsizei n, const GLuint *arrays) { _glsDeleteVertexArrays(n, arrays); }
static void    APIENTRY _stub_glBindVertexArray(GLuint array) { _glsBindVertexArray(array); }
static GLboolean APIENTRY _stub_glIsVertexArray(GLuint array) { return GL_FALSE; }

/* ===== GL 3.0 — FBO ===== */
static void    APIENTRY _stub_glGenFramebuffers(GLsizei n, GLuint *framebuffers) { _glsGenFramebuffers(n, framebuffers); }
static void    APIENTRY _stub_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) { _glsDeleteFramebuffers(n, framebuffers); }
static void    APIENTRY _stub_glBindFramebuffer(GLenum target, GLuint framebuffer) { _glsBindFramebuffer(target, framebuffer); }
static GLenum  APIENTRY _stub_glCheckFramebufferStatus(GLenum target) { return _glsCheckFramebufferStatus(target); }
static void    APIENTRY _stub_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) { _glsFramebufferTexture2D(target, attachment, textarget, texture, level); }
static void    APIENTRY _stub_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) { _glsFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer); }
static GLboolean APIENTRY _stub_glIsFramebuffer(GLuint framebuffer) { return GL_FALSE; }
static void    APIENTRY _stub_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) { _glsBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter); }

/* ===== GL 3.0 — RBO ===== */
static void    APIENTRY _stub_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) { _glsGenRenderbuffers(n, renderbuffers); }
static void    APIENTRY _stub_glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) { _glsDeleteRenderbuffers(n, renderbuffers); }
static void    APIENTRY _stub_glBindRenderbuffer(GLenum target, GLuint renderbuffer) { _glsBindRenderbuffer(target, renderbuffer); }
static void    APIENTRY _stub_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) { _glsRenderbufferStorage(target, internalformat, width, height); }
static void    APIENTRY _stub_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) { _glsRenderbufferStorageMultisample(target, samples, internalformat, width, height); }
static void    APIENTRY _stub_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) { if(params) *params=0; }

/* ===== GL 3.0 — Misc ===== */
static const GLubyte* APIENTRY _stub_glGetStringi(GLenum name, GLuint index) { return (const GLubyte*)""; }
static void    APIENTRY _stub_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value) { _glsClearBufferfv(buffer, drawbuffer, value); }
static void    APIENTRY _stub_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value) { _glsClearBufferiv(buffer, drawbuffer, value); }
static void    APIENTRY _stub_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value) { _glsClearBufferuiv(buffer, drawbuffer, value); }
static void    APIENTRY _stub_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) { _glsClearBufferfi(buffer, drawbuffer, depth, stencil); }
static void*   APIENTRY _stub_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) { return _glsMapBufferRange(target, offset, length, access); }
static void    APIENTRY _stub_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) { _glsFlushMappedBufferRange(target, offset, length); }
static void    APIENTRY _stub_glGenerateMipmap(GLenum target) { _glsGenerateMipmap(target); }
static void    APIENTRY _stub_glBindBufferBase(GLenum target, GLuint index, GLuint buffer) { _glsBindBufferBase(target, index, buffer); }
static void    APIENTRY _stub_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) { _glsBindBufferRange(target, index, buffer, offset, size); }
static void    APIENTRY _stub_glBeginTransformFeedback(GLenum primitiveMode) { _glsBeginTransformFeedback(primitiveMode); }
static void    APIENTRY _stub_glEndTransformFeedback(void) { _glsEndTransformFeedback(); }
static void    APIENTRY _stub_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode) { _glsTransformFeedbackVaryings(program, count, varyings, bufferMode); }
static void    APIENTRY _stub_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) { _glsVertexAttribIPointer(index, size, type, stride, pointer); }
static void    APIENTRY _stub_glBeginConditionalRender(GLuint id, GLenum mode) { _glsBeginConditionalRender(id, mode); }
static void    APIENTRY _stub_glEndConditionalRender(void) { _glsEndConditionalRender(); }
static void    APIENTRY _stub_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) { _glsColorMaski(index, r, g, b, a); }
static void    APIENTRY _stub_glEnablei(GLenum target, GLuint index) { _glsEnablei(target, index); }
static void    APIENTRY _stub_glDisablei(GLenum target, GLuint index) { _glsDisablei(target, index); }

/* ===== GL 3.0 — Queries ===== */
static void    APIENTRY _stub_glGenQueries(GLsizei n, GLuint *ids) { _glsGenQueries(n, ids); }
static void    APIENTRY _stub_glDeleteQueries(GLsizei n, const GLuint *ids) { _glsDeleteQueries(n, ids); }
static void    APIENTRY _stub_glBeginQuery(GLenum target, GLuint id) { _glsBeginQuery(target, id); }
static void    APIENTRY _stub_glEndQuery(GLenum target) { _glsEndQuery(target); }
static void    APIENTRY _stub_glGetQueryiv(GLenum target, GLenum pname, GLint *params) { if(params) *params=0; }
static void    APIENTRY _stub_glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params) { if(params) *params=0; }
static void    APIENTRY _stub_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params) { if(params) *params=0; }

/* ===== GL 3.1 — UBO / TBO / CopyBuffer / DrawInstanced ===== */
static GLuint  APIENTRY _stub_glGetUniformBlockIndex(GLuint program, const GLchar *name) { return 0xFFFFFFFF; }
static void    APIENTRY _stub_glUniformBlockBinding(GLuint program, GLuint blockIndex, GLuint blockBinding) { _glsUniformBlockBinding(program, blockIndex, blockBinding); }
static void    APIENTRY _stub_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) { _glsDrawArraysInstanced(mode, first, count, instancecount); }
static void    APIENTRY _stub_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount) { _glsDrawElementsInstanced(mode, count, type, indices, instancecount); }
static void    APIENTRY _stub_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) { _glsCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size); }
static void    APIENTRY _stub_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) { _glsTexBuffer(target, internalformat, buffer); }
static void    APIENTRY _stub_glPrimitiveRestartIndex(GLuint index) { _glsPrimitiveRestartIndex(index); }

/* ===== GL 3.2 — Sync / Geometry Shader ===== */
static GLsync  APIENTRY _stub_glFenceSync(GLenum condition, GLbitfield flags) { return (GLsync)_glsFenceSync(condition, flags); }
static void    APIENTRY _stub_glDeleteSync(GLsync sync) { _glsDeleteSync(sync); }
static GLenum  APIENTRY _stub_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) { return _glsClientWaitSync(sync, flags, timeout); }
static void    APIENTRY _stub_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) { _glsWaitSync(sync, flags, timeout); }
static void    APIENTRY _stub_glGetInteger64v(GLenum pname, GLint64 *data) { if(data) *data=0; }
static void    APIENTRY _stub_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) { _glsFramebufferTexture(target, attachment, texture, level); }
static void    APIENTRY _stub_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params) { if(params) *params=0; }
static void    APIENTRY _stub_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) { _glsTexImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations); }
static void    APIENTRY _stub_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) { _glsTexImage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations); }
static void    APIENTRY _stub_glSampleMaski(GLuint maskNumber, GLbitfield mask) { _glsSampleMaski(maskNumber, mask); }
static void    APIENTRY _stub_glProvokingVertex(GLenum mode) { _glsProvokingVertex(mode); }

/* ===== GL 3.3 — Samplers / Timer queries ===== */
static void    APIENTRY _stub_glGenSamplers(GLsizei count, GLuint *samplers) { _glsGenSamplers(count, samplers); }
static void    APIENTRY _stub_glDeleteSamplers(GLsizei count, const GLuint *samplers) { _glsDeleteSamplers(count, samplers); }
static void    APIENTRY _stub_glBindSampler(GLuint unit, GLuint sampler) { _glsBindSampler(unit, sampler); }
static void    APIENTRY _stub_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) { _glsSamplerParameteri(sampler, pname, param); }
static void    APIENTRY _stub_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) { _glsSamplerParameterf(sampler, pname, param); }
static void    APIENTRY _stub_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param) { _glsSamplerParameteriv(sampler, pname, param); }
static void    APIENTRY _stub_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param) { _glsSamplerParameterfv(sampler, pname, param); }
static void    APIENTRY _stub_glQueryCounter(GLuint id, GLenum target) { _glsQueryCounter(id, target); }
static void    APIENTRY _stub_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params) { if(params) *params=0; }
static void    APIENTRY _stub_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params) { if(params) *params=0; }
static void    APIENTRY _stub_glVertexAttribDivisor(GLuint index, GLuint divisor) { _glsVertexAttribDivisor(index, divisor); }

/* ===== GL 4.x — Commonly queried ===== */
static void    APIENTRY _stub_glMinSampleShading(GLfloat value) { _glsMinSampleShading(value); }
static void    APIENTRY _stub_glBlendEquationi(GLuint buf, GLenum mode) { _glsBlendEquationi(buf, mode); }
static void    APIENTRY _stub_glBlendFunci(GLuint buf, GLenum src, GLenum dst) { _glsBlendFunci(buf, src, dst); }
static void    APIENTRY _stub_glPatchParameteri(GLenum pname, GLint value) { _glsPatchParameteri(pname, value); }
static void    APIENTRY _stub_glMemoryBarrier(GLbitfield barriers) { _glsMemoryBarrier(barriers); }
static void    APIENTRY _stub_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) { _glsTexStorage2D(target, levels, internalformat, width, height); }
static void    APIENTRY _stub_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) { _glsTexStorage3D(target, levels, internalformat, width, height, depth); }
static void    APIENTRY _stub_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) { _glsBindImageTexture(unit, texture, level, layered, layer, access, format); }
static void    APIENTRY _stub_glDispatchCompute(GLuint x, GLuint y, GLuint z) { _glsDispatchCompute(x, y, z); }
static void    APIENTRY _stub_glDebugMessageCallback(void *callback, const void *userParam) { _glsDebugMessageCallback(callback, userParam); }
static void    APIENTRY _stub_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled) { _glsDebugMessageControl(source, type, severity, count, ids, enabled); }
static void    APIENTRY _stub_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label) { _glsObjectLabel(identifier, name, length, label); }
static void    APIENTRY _stub_glClipControl(GLenum origin, GLenum depth) { _glsClipControl(origin, depth); }
static void    APIENTRY _stub_glTextureBarrier(void) { _glsTextureBarrier(); }

/* ===== GL 2.0 — Texture3D (core) ===== */
static void    APIENTRY _stub_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) { _glsTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels); }
static void    APIENTRY _stub_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) { _glsTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels); }
static void    APIENTRY _stub_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) { _glsCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data); }
static void    APIENTRY _stub_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data) { _glsCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data); }
static void    APIENTRY _stub_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) { _glsCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data); }
static void    APIENTRY _stub_glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data) { _glsCompressedTexImage1D(target, level, internalformat, width, border, imageSize, data); }
static void    APIENTRY _stub_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data) { _glsCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data); }
static void    APIENTRY _stub_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data) { _glsCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data); }
static void    APIENTRY _stub_glGetCompressedTexImage(GLenum target, GLint level, void *img) { _glsGetCompressedTexImage(target, level, img); }
static void    APIENTRY _stub_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) { _glsCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height); }
static void    APIENTRY _stub_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices) { _glsDrawRangeElements(mode, start, end, count, type, indices); }
static void    APIENTRY _stub_glSampleCoverage(GLfloat value, GLboolean invert) { _glsSampleCoverage(value, invert); }
static void    APIENTRY _stub_glClientActiveTexture(GLenum texture) { _glsClientActiveTexture(texture); }
static void    APIENTRY _stub_glLoadTransposeMatrixf(const GLfloat *m) { _glsLoadTransposeMatrixf(m); }
static void    APIENTRY _stub_glLoadTransposeMatrixd(const GLdouble *m) { _glsLoadTransposeMatrixd(m); }
static void    APIENTRY _stub_glMultTransposeMatrixf(const GLfloat *m) { _glsMultTransposeMatrixf(m); }
static void    APIENTRY _stub_glMultTransposeMatrixd(const GLdouble *m) { _glsMultTransposeMatrixd(m); }
static void    APIENTRY _stub_glFogCoordf(GLfloat coord) { _glsFogCoordf(coord); }
static void    APIENTRY _stub_glFogCoordfv(const GLfloat *coord) { _glsFogCoordfv(coord); }
static void    APIENTRY _stub_glFogCoordd(GLdouble coord) { _glsFogCoordd(coord); }
static void    APIENTRY _stub_glFogCoorddv(const GLdouble *coord) { _glsFogCoorddv(coord); }
static void    APIENTRY _stub_glFogCoordPointer(GLenum type, GLsizei stride, const void *pointer) { _glsFogCoordPointer(type, stride, pointer); }
static void    APIENTRY _stub_glSecondaryColor3f(GLfloat r, GLfloat g, GLfloat b) { _glsSecondaryColor3f(r, g, b); }
static void    APIENTRY _stub_glSecondaryColor3fv(const GLfloat *v) { _glsSecondaryColor3fv(v); }
static void    APIENTRY _stub_glSecondaryColor3ub(GLubyte r, GLubyte g, GLubyte b) { _glsSecondaryColor3ub(r, g, b); }
static void    APIENTRY _stub_glSecondaryColor3ubv(const GLubyte *v) { _glsSecondaryColor3ubv(v); }
static void    APIENTRY _stub_glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) { _glsSecondaryColorPointer(size, type, stride, pointer); }
static void    APIENTRY _stub_glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount) { _glsMultiDrawArrays(mode, first, count, drawcount); }
static void    APIENTRY _stub_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount) { _glsMultiDrawElements(mode, count, type, indices, drawcount); }
static void    APIENTRY _stub_glPointParameterf(GLenum pname, GLfloat param) { _glsPointParameterf(pname, param); }
static void    APIENTRY _stub_glPointParameterfv(GLenum pname, const GLfloat *params) { _glsPointParameterfv(pname, params); }
static void    APIENTRY _stub_glPointParameteri(GLenum pname, GLint param) { _glsPointParameteri(pname, param); }
static void    APIENTRY _stub_glPointParameteriv(GLenum pname, const GLint *params) { _glsPointParameteriv(pname, params); }
static void    APIENTRY _stub_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data) { _glsGetBufferSubData(target, offset, size, data); }
static void    APIENTRY _stub_glGetBufferPointerv(GLenum target, GLenum pname, void **params) { if(params) *params=NULL; }
static void    APIENTRY _stub_glActiveTexture(GLenum texture) { _glsActiveTexture(texture); }
static void    APIENTRY _stub_glBlendEquation(GLenum mode) { _glsBlendEquation(mode); }
static void    APIENTRY _stub_glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { _glsBlendColor(r, g, b, a); }

/* ARB program wrappers with correct signatures */
static void    APIENTRY _stub_glGenProgramsARB(GLenum target, GLsizei n, GLuint *programs) { (void)target; _glsGenProgramsARB(n, programs); }
static void    APIENTRY _stub_glGenProgramsARB2(GLsizei n, GLuint *programs) { _glsGenProgramsARB(n, programs); }
static void    APIENTRY _stub_glBindProgramARB(GLenum target, GLuint program) { _glsBindProgramARB(target, program); }
static void    APIENTRY _stub_glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string) { _glsProgramStringARB(target, format, len, string); }
static void    APIENTRY _stub_glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat *params) { _glsProgramEnvParameter4fvARB(target, index, params); }
static void    APIENTRY _stub_glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat *params) { _glsProgramLocalParameter4fvARB(target, index, params); }
static void    APIENTRY _stub_glDeleteObjectARB(GLuint obj) { _glsDeleteObjectARB(obj); }
static void    APIENTRY _stub_glGetObjectParameterivARB(GLuint obj, GLenum pname, GLint *params) { _glsGetObjectParameterivARB(obj, pname, params); }
static void    APIENTRY _stub_glGetInfoLogARB(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog) { _glsGetInfoLogARB(obj, maxLength, length, infoLog); }
static void    APIENTRY _stub_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) { _glsMultiTexCoord2fARB(target, s, t); }
static void    APIENTRY _stub_glMultiTexCoord2fvARB(GLenum target, const GLfloat *v) { _glsMultiTexCoord2fvARB(target, v); }
static void    APIENTRY _stub_glActiveStencilFaceEXT(GLenum face) { _glsActiveStencilFaceEXT(face); }

/* ===== Master proc table ===== */

typedef struct { const char *name; PROC proc; } GLD_modernProcEntry;

static const GLD_modernProcEntry g_modernGL[] = {
    /* GL 2.0 Shaders */
    { "glCreateShader",             (PROC)_stub_glCreateShader },
    { "glDeleteShader",             (PROC)_stub_glDeleteShader },
    { "glShaderSource",             (PROC)_stub_glShaderSource },
    { "glCompileShader",            (PROC)_stub_glCompileShader },
    { "glGetShaderiv",              (PROC)_stub_glGetShaderiv },
    { "glGetShaderInfoLog",         (PROC)_stub_glGetShaderInfoLog },
    { "glCreateProgram",            (PROC)_stub_glCreateProgram },
    { "glDeleteProgram",            (PROC)_stub_glDeleteProgram },
    { "glAttachShader",             (PROC)_stub_glAttachShader },
    { "glDetachShader",             (PROC)_stub_glDetachShader },
    { "glLinkProgram",              (PROC)_stub_glLinkProgram },
    { "glUseProgram",               (PROC)_stub_glUseProgram },
    { "glGetProgramiv",             (PROC)_stub_glGetProgramiv },
    { "glGetProgramInfoLog",        (PROC)_stub_glGetProgramInfoLog },
    { "glValidateProgram",          (PROC)_stub_glValidateProgram },
    { "glIsShader",                 (PROC)_stub_glIsShader },
    { "glIsProgram",                (PROC)_stub_glIsProgram },
    { "glGetUniformLocation",       (PROC)_stub_glGetUniformLocation },
    { "glGetAttribLocation",        (PROC)_stub_glGetAttribLocation },
    { "glBindAttribLocation",       (PROC)_stub_glBindAttribLocation },
    { "glGetActiveUniform",         (PROC)_stub_glGetActiveUniform },
    { "glGetActiveAttrib",          (PROC)_stub_glGetActiveAttrib },
    /* GL 2.0 Uniforms */
    { "glUniform1i",                (PROC)_stub_glUniform1i },
    { "glUniform2i",                (PROC)_stub_glUniform2i },
    { "glUniform3i",                (PROC)_stub_glUniform3i },
    { "glUniform4i",                (PROC)_stub_glUniform4i },
    { "glUniform1f",                (PROC)_stub_glUniform1f },
    { "glUniform2f",                (PROC)_stub_glUniform2f },
    { "glUniform3f",                (PROC)_stub_glUniform3f },
    { "glUniform4f",                (PROC)_stub_glUniform4f },
    { "glUniform1iv",               (PROC)_stub_glUniform1iv },
    { "glUniform2iv",               (PROC)_stub_glUniform2iv },
    { "glUniform3iv",               (PROC)_stub_glUniform3iv },
    { "glUniform4iv",               (PROC)_stub_glUniform4iv },
    { "glUniform1fv",               (PROC)_stub_glUniform1fv },
    { "glUniform2fv",               (PROC)_stub_glUniform2fv },
    { "glUniform3fv",               (PROC)_stub_glUniform3fv },
    { "glUniform4fv",               (PROC)_stub_glUniform4fv },
    { "glUniformMatrix2fv",         (PROC)_stub_glUniformMatrix2fv },
    { "glUniformMatrix3fv",         (PROC)_stub_glUniformMatrix3fv },
    { "glUniformMatrix4fv",         (PROC)_stub_glUniformMatrix4fv },
    /* GL 2.0 Vertex attribs */
    { "glVertexAttrib1f",           (PROC)_stub_glVertexAttrib1f },
    { "glVertexAttrib2f",           (PROC)_stub_glVertexAttrib2f },
    { "glVertexAttrib3f",           (PROC)_stub_glVertexAttrib3f },
    { "glVertexAttrib4f",           (PROC)_stub_glVertexAttrib4f },
    { "glVertexAttribPointer",      (PROC)_stub_glVertexAttribPointer },
    { "glEnableVertexAttribArray",  (PROC)_stub_glEnableVertexAttribArray },
    { "glDisableVertexAttribArray", (PROC)_stub_glDisableVertexAttribArray },
    /* GL 2.0 Other */
    { "glBlendEquationSeparate",    (PROC)_stub_glBlendEquationSeparate },
    { "glBlendFuncSeparate",        (PROC)_stub_glBlendFuncSeparate },
    { "glStencilFuncSeparate",      (PROC)_stub_glStencilFuncSeparate },
    { "glStencilOpSeparate",        (PROC)_stub_glStencilOpSeparate },
    { "glStencilMaskSeparate",      (PROC)_stub_glStencilMaskSeparate },
    { "glDrawBuffers",              (PROC)_stub_glDrawBuffers },
    /* GL 1.5/2.0 Buffers */
    { "glGenBuffers",               (PROC)_stub_glGenBuffers },
    { "glDeleteBuffers",            (PROC)_stub_glDeleteBuffers },
    { "glBindBuffer",               (PROC)_stub_glBindBuffer },
    { "glBufferData",               (PROC)_stub_glBufferData },
    { "glBufferSubData",            (PROC)_stub_glBufferSubData },
    { "glMapBuffer",                (PROC)_stub_glMapBuffer },
    { "glUnmapBuffer",              (PROC)_stub_glUnmapBuffer },
    { "glIsBuffer",                 (PROC)_stub_glIsBuffer },
    { "glGetBufferParameteriv",     (PROC)_stub_glGetBufferParameteriv },
    /* GL 3.0 VAO */
    { "glGenVertexArrays",          (PROC)_stub_glGenVertexArrays },
    { "glDeleteVertexArrays",       (PROC)_stub_glDeleteVertexArrays },
    { "glBindVertexArray",          (PROC)_stub_glBindVertexArray },
    { "glIsVertexArray",            (PROC)_stub_glIsVertexArray },
    /* GL 3.0 FBO */
    { "glGenFramebuffers",          (PROC)_stub_glGenFramebuffers },
    { "glDeleteFramebuffers",       (PROC)_stub_glDeleteFramebuffers },
    { "glBindFramebuffer",          (PROC)_stub_glBindFramebuffer },
    { "glCheckFramebufferStatus",   (PROC)_stub_glCheckFramebufferStatus },
    { "glFramebufferTexture2D",     (PROC)_stub_glFramebufferTexture2D },
    { "glFramebufferRenderbuffer",  (PROC)_stub_glFramebufferRenderbuffer },
    { "glIsFramebuffer",            (PROC)_stub_glIsFramebuffer },
    { "glBlitFramebuffer",          (PROC)_stub_glBlitFramebuffer },
    /* GL 3.0 RBO */
    { "glGenRenderbuffers",         (PROC)_stub_glGenRenderbuffers },
    { "glDeleteRenderbuffers",      (PROC)_stub_glDeleteRenderbuffers },
    { "glBindRenderbuffer",         (PROC)_stub_glBindRenderbuffer },
    { "glRenderbufferStorage",      (PROC)_stub_glRenderbufferStorage },
    { "glRenderbufferStorageMultisample", (PROC)_stub_glRenderbufferStorageMultisample },
    { "glGetRenderbufferParameteriv", (PROC)_stub_glGetRenderbufferParameteriv },
    /* GL 3.0 Misc */
    { "glGetStringi",               (PROC)_stub_glGetStringi },
    { "glClearBufferfv",            (PROC)_stub_glClearBufferfv },
    { "glClearBufferiv",            (PROC)_stub_glClearBufferiv },
    { "glClearBufferuiv",           (PROC)_stub_glClearBufferuiv },
    { "glClearBufferfi",            (PROC)_stub_glClearBufferfi },
    { "glMapBufferRange",           (PROC)_stub_glMapBufferRange },
    { "glFlushMappedBufferRange",   (PROC)_stub_glFlushMappedBufferRange },
    { "glGenerateMipmap",           (PROC)_stub_glGenerateMipmap },
    { "glBindBufferBase",           (PROC)_stub_glBindBufferBase },
    { "glBindBufferRange",          (PROC)_stub_glBindBufferRange },
    { "glBeginTransformFeedback",   (PROC)_stub_glBeginTransformFeedback },
    { "glEndTransformFeedback",     (PROC)_stub_glEndTransformFeedback },
    { "glTransformFeedbackVaryings",(PROC)_stub_glTransformFeedbackVaryings },
    { "glVertexAttribIPointer",     (PROC)_stub_glVertexAttribIPointer },
    { "glBeginConditionalRender",   (PROC)_stub_glBeginConditionalRender },
    { "glEndConditionalRender",     (PROC)_stub_glEndConditionalRender },
    { "glColorMaski",               (PROC)_stub_glColorMaski },
    { "glEnablei",                  (PROC)_stub_glEnablei },
    { "glDisablei",                 (PROC)_stub_glDisablei },
    /* GL 3.0 Queries */
    { "glGenQueries",               (PROC)_stub_glGenQueries },
    { "glDeleteQueries",            (PROC)_stub_glDeleteQueries },
    { "glBeginQuery",               (PROC)_stub_glBeginQuery },
    { "glEndQuery",                 (PROC)_stub_glEndQuery },
    { "glGetQueryiv",               (PROC)_stub_glGetQueryiv },
    { "glGetQueryObjectiv",         (PROC)_stub_glGetQueryObjectiv },
    { "glGetQueryObjectuiv",        (PROC)_stub_glGetQueryObjectuiv },
    /* GL 3.1 */
    { "glGetUniformBlockIndex",     (PROC)_stub_glGetUniformBlockIndex },
    { "glUniformBlockBinding",      (PROC)_stub_glUniformBlockBinding },
    { "glDrawArraysInstanced",      (PROC)_stub_glDrawArraysInstanced },
    { "glDrawElementsInstanced",    (PROC)_stub_glDrawElementsInstanced },
    { "glCopyBufferSubData",        (PROC)_stub_glCopyBufferSubData },
    { "glTexBuffer",                (PROC)_stub_glTexBuffer },
    { "glPrimitiveRestartIndex",    (PROC)_stub_glPrimitiveRestartIndex },
    /* GL 3.2 */
    { "glFenceSync",                (PROC)_stub_glFenceSync },
    { "glDeleteSync",               (PROC)_stub_glDeleteSync },
    { "glClientWaitSync",           (PROC)_stub_glClientWaitSync },
    { "glWaitSync",                 (PROC)_stub_glWaitSync },
    { "glGetInteger64v",            (PROC)_stub_glGetInteger64v },
    { "glFramebufferTexture",       (PROC)_stub_glFramebufferTexture },
    { "glGetBufferParameteri64v",   (PROC)_stub_glGetBufferParameteri64v },
    { "glTexImage2DMultisample",    (PROC)_stub_glTexImage2DMultisample },
    { "glTexImage3DMultisample",    (PROC)_stub_glTexImage3DMultisample },
    { "glSampleMaski",              (PROC)_stub_glSampleMaski },
    { "glProvokingVertex",          (PROC)_stub_glProvokingVertex },
    /* GL 3.3 */
    { "glGenSamplers",              (PROC)_stub_glGenSamplers },
    { "glDeleteSamplers",           (PROC)_stub_glDeleteSamplers },
    { "glBindSampler",              (PROC)_stub_glBindSampler },
    { "glSamplerParameteri",        (PROC)_stub_glSamplerParameteri },
    { "glSamplerParameterf",        (PROC)_stub_glSamplerParameterf },
    { "glSamplerParameteriv",       (PROC)_stub_glSamplerParameteriv },
    { "glSamplerParameterfv",       (PROC)_stub_glSamplerParameterfv },
    { "glQueryCounter",             (PROC)_stub_glQueryCounter },
    { "glGetQueryObjecti64v",       (PROC)_stub_glGetQueryObjecti64v },
    { "glGetQueryObjectui64v",      (PROC)_stub_glGetQueryObjectui64v },
    { "glVertexAttribDivisor",      (PROC)_stub_glVertexAttribDivisor },
    /* GL 4.x commonly queried */
    { "glMinSampleShading",         (PROC)_stub_glMinSampleShading },
    { "glBlendEquationi",           (PROC)_stub_glBlendEquationi },
    { "glBlendFunci",               (PROC)_stub_glBlendFunci },
    { "glPatchParameteri",          (PROC)_stub_glPatchParameteri },
    { "glMemoryBarrier",            (PROC)_stub_glMemoryBarrier },
    { "glTexStorage2D",             (PROC)_stub_glTexStorage2D },
    { "glTexStorage3D",             (PROC)_stub_glTexStorage3D },
    { "glBindImageTexture",         (PROC)_stub_glBindImageTexture },
    { "glDispatchCompute",          (PROC)_stub_glDispatchCompute },
    { "glDebugMessageCallback",     (PROC)_stub_glDebugMessageCallback },
    { "glDebugMessageControl",      (PROC)_stub_glDebugMessageControl },
    { "glObjectLabel",              (PROC)_stub_glObjectLabel },
    { "glClipControl",              (PROC)_stub_glClipControl },
    { "glTextureBarrier",           (PROC)_stub_glTextureBarrier },
    /* Texture / misc */
    { "glTexImage3D",               (PROC)_stub_glTexImage3D },
    { "glTexSubImage3D",            (PROC)_stub_glTexSubImage3D },
    { "glCompressedTexImage2D",     (PROC)_stub_glCompressedTexImage2D },
    { "glCompressedTexImage3D",     (PROC)_stub_glCompressedTexImage3D },
    { "glCompressedTexImage1D",     (PROC)_stub_glCompressedTexImage1D },
    { "glCompressedTexSubImage2D",  (PROC)_stub_glCompressedTexSubImage2D },
    { "glCompressedTexSubImage3D",  (PROC)_stub_glCompressedTexSubImage3D },
    { "glCompressedTexSubImage1D",  (PROC)_stub_glCompressedTexSubImage1D },
    { "glGetCompressedTexImage",    (PROC)_stub_glGetCompressedTexImage },
    { "glCopyTexSubImage3D",        (PROC)_stub_glCopyTexSubImage3D },
    { "glDrawRangeElements",        (PROC)_stub_glDrawRangeElements },
    { "glSampleCoverage",           (PROC)_stub_glSampleCoverage },
    { "glClientActiveTexture",      (PROC)_stub_glClientActiveTexture },
    { "glLoadTransposeMatrixf",     (PROC)_stub_glLoadTransposeMatrixf },
    { "glLoadTransposeMatrixd",     (PROC)_stub_glLoadTransposeMatrixd },
    { "glMultTransposeMatrixf",     (PROC)_stub_glMultTransposeMatrixf },
    { "glMultTransposeMatrixd",     (PROC)_stub_glMultTransposeMatrixd },
    { "glFogCoordf",                (PROC)_stub_glFogCoordf },
    { "glFogCoordfv",               (PROC)_stub_glFogCoordfv },
    { "glFogCoordd",                (PROC)_stub_glFogCoordd },
    { "glFogCoorddv",               (PROC)_stub_glFogCoorddv },
    { "glFogCoordPointer",          (PROC)_stub_glFogCoordPointer },
    { "glSecondaryColor3f",         (PROC)_stub_glSecondaryColor3f },
    { "glSecondaryColor3fv",        (PROC)_stub_glSecondaryColor3fv },
    { "glSecondaryColor3ub",        (PROC)_stub_glSecondaryColor3ub },
    { "glSecondaryColor3ubv",       (PROC)_stub_glSecondaryColor3ubv },
    { "glSecondaryColorPointer",    (PROC)_stub_glSecondaryColorPointer },
    { "glMultiDrawArrays",          (PROC)_stub_glMultiDrawArrays },
    { "glMultiDrawElements",        (PROC)_stub_glMultiDrawElements },
    { "glPointParameterf",          (PROC)_stub_glPointParameterf },
    { "glPointParameterfv",         (PROC)_stub_glPointParameterfv },
    { "glPointParameteri",          (PROC)_stub_glPointParameteri },
    { "glPointParameteriv",         (PROC)_stub_glPointParameteriv },
    { "glGetBufferSubData",         (PROC)_stub_glGetBufferSubData },
    { "glGetBufferPointerv",        (PROC)_stub_glGetBufferPointerv },
    { "glActiveTexture",            (PROC)_stub_glActiveTexture },
    { "glBlendEquation",            (PROC)_stub_glBlendEquation },
    { "glBlendColor",               (PROC)_stub_glBlendColor },
    /* ARB shader object aliases — Quake 4 and other id Tech games use these */
    { "glCreateShaderObjectARB",    (PROC)_stub_glCreateShader },
    { "glCreateProgramObjectARB",   (PROC)_stub_glCreateProgram },
    { "glDeleteObjectARB",          (PROC)_stub_glDeleteObjectARB },
    { "glShaderSourceARB",          (PROC)_stub_glShaderSource },
    { "glCompileShaderARB",         (PROC)_stub_glCompileShader },
    { "glGetObjectParameterivARB",  (PROC)_stub_glGetObjectParameterivARB },
    { "glAttachObjectARB",          (PROC)_stub_glAttachShader },
    { "glDetachObjectARB",          (PROC)_stub_glDetachShader },
    { "glLinkProgramARB",           (PROC)_stub_glLinkProgram },
    { "glUseProgramObjectARB",      (PROC)_stub_glUseProgram },
    { "glGetInfoLogARB",            (PROC)_stub_glGetInfoLogARB },
    { "glGetUniformLocationARB",    (PROC)_stub_glGetUniformLocation },
    { "glUniform1fARB",             (PROC)_stub_glUniform1f },
    { "glUniform1iARB",             (PROC)_stub_glUniform1i },
    { "glUniform1fvARB",            (PROC)_stub_glUniform1fv },
    { "glUniform2fvARB",            (PROC)_stub_glUniform2fv },
    { "glUniform3fvARB",            (PROC)_stub_glUniform3fv },
    { "glUniform4fvARB",            (PROC)_stub_glUniform4fv },
    { "glGetAttribLocationARB",     (PROC)_stub_glGetAttribLocation },
    { "glBindAttribLocationARB",    (PROC)_stub_glBindAttribLocation },
    { "glVertexAttribPointerARB",   (PROC)_stub_glVertexAttribPointer },
    { "glEnableVertexAttribArrayARB",  (PROC)_stub_glEnableVertexAttribArray },
    { "glDisableVertexAttribArrayARB", (PROC)_stub_glDisableVertexAttribArray },
    /* ARB vertex/fragment program (assembly shaders) */
    { "glGenProgramsARB",           (PROC)_stub_glGenProgramsARB2 },
    { "glBindProgramARB",           (PROC)_stub_glBindProgramARB },
    { "glProgramStringARB",         (PROC)_stub_glProgramStringARB },
    { "glProgramEnvParameter4fvARB",(PROC)_stub_glProgramEnvParameter4fvARB },
    { "glProgramLocalParameter4fvARB",(PROC)_stub_glProgramLocalParameter4fvARB },
    /* ARB buffer object aliases */
    { "glBindBufferARB",            (PROC)_stub_glBindBuffer },
    { "glDeleteBuffersARB",         (PROC)_stub_glDeleteBuffers },
    { "glGenBuffersARB",            (PROC)_stub_glGenBuffers },
    { "glIsBufferARB",              (PROC)_stub_glIsBuffer },
    { "glBufferDataARB",            (PROC)_stub_glBufferData },
    { "glBufferSubDataARB",         (PROC)_stub_glBufferSubData },
    { "glGetBufferSubDataARB",      (PROC)_stub_glGetBufferSubData },
    { "glMapBufferARB",             (PROC)_stub_glMapBuffer },
    { "glUnmapBufferARB",           (PROC)_stub_glUnmapBuffer },
    { "glGetBufferParameterivARB",  (PROC)_stub_glGetBufferParameteriv },
    { "glGetBufferPointervARB",     (PROC)_stub_glGetBufferPointerv },
    /* ARB multitexture */
    { "glMultiTexCoord2fARB",       (PROC)_stub_glMultiTexCoord2fARB },
    { "glMultiTexCoord2fvARB",      (PROC)_stub_glMultiTexCoord2fvARB },
    { "glActiveTextureARB",         (PROC)_stub_glActiveTexture },
    { "glClientActiveTextureARB",   (PROC)_stub_glClientActiveTexture },
    /* ARB texture compression */
    { "glCompressedTexImage2DARB",  (PROC)_stub_glCompressedTexImage2D },
    { "glGetCompressedTexImageARB", (PROC)_stub_glGetCompressedTexImage },
    /* EXT aliases */
    { "glDrawRangeElementsEXT",     (PROC)_stub_glDrawRangeElements },
    { "glBlendEquationEXT",         (PROC)_stub_glBlendEquation },
    { "glActiveStencilFaceEXT",     (PROC)_stub_glActiveStencilFaceEXT },
    /* Sentinel */
    { NULL, NULL }
};

#ifdef __cplusplus
}
#endif

#endif /* GL_MODERN_STUBS_H */
