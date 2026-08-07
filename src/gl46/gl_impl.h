/*********************************************************************************
*
*  gl_impl.h - Internal GL function implementations using the state machine
*
*  These _gls* functions are called by the exported stubs in gl_legacy_stubs.c
*  and gl_modern_stubs.h. They track GL state properly so games don't crash.
*
*********************************************************************************/

#ifndef GL_IMPL_H
#define GL_IMPL_H

#include "gl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Object management ===== */
void _glsGenTextures(int n, unsigned int *textures);
void _glsDeleteTextures(int n, const unsigned int *textures);
void _glsBindTexture(unsigned int target, unsigned int texture);
void _glsGenBuffers(int n, unsigned int *buffers);
void _glsDeleteBuffers(int n, const unsigned int *buffers);
void _glsBindBuffer(unsigned int target, unsigned int buffer);
void _glsGenVertexArrays(int n, unsigned int *arrays);
void _glsDeleteVertexArrays(int n, const unsigned int *arrays);
void _glsBindVertexArray(unsigned int array);
void _glsGenFramebuffers(int n, unsigned int *framebuffers);
void _glsDeleteFramebuffers(int n, const unsigned int *framebuffers);
void _glsBindFramebuffer(unsigned int target, unsigned int framebuffer);
unsigned int _glsCheckFramebufferStatus(unsigned int target);
void _glsGenRenderbuffers(int n, unsigned int *renderbuffers);
void _glsDeleteRenderbuffers(int n, const unsigned int *renderbuffers);
void _glsBindRenderbuffer(unsigned int target, unsigned int renderbuffer);
void _glsRenderbufferStorage(unsigned int target, unsigned int internalformat, int width, int height);
void _glsGenQueries(int n, unsigned int *ids);
void _glsGetQueryiv(unsigned int target, unsigned int pname, int *params);
void _glsGetQueryObjectiv(unsigned int id, unsigned int pname, int *params);
void _glsGetQueryObjectuiv(unsigned int id, unsigned int pname, unsigned int *params);
void _glsGetQueryObjecti64v(unsigned int id, unsigned int pname, __int64 *params);
void _glsGetQueryObjectui64v(unsigned int id, unsigned int pname, unsigned __int64 *params);
void _glsDeleteQueries(int n, const unsigned int *ids);
void _glsGenSamplers(int count, unsigned int *samplers);
void _glsDeleteSamplers(int count, const unsigned int *samplers);

/* ===== Texture functions ===== */
void _glsTexImage2D(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels);
void _glsTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, unsigned int type, const void *pixels);
void _glsTexParameteri(unsigned int target, unsigned int pname, int param);
void _glsTexParameterf(unsigned int target, unsigned int pname, float param);
void _glsCompressedTexImage2D(unsigned int target, int level, unsigned int internalformat, int width, int height, int border, int imageSize, const void *data);
void _glsActiveTexture(unsigned int texture);

/* ===== Buffer functions ===== */
void _glsBufferData(unsigned int target, ptrdiff_t size, const void *data, unsigned int usage);
void _glsBufferSubData(unsigned int target, ptrdiff_t offset, ptrdiff_t size, const void *data);
void *_glsMapBuffer(unsigned int target, unsigned int access);
unsigned char _glsUnmapBuffer(unsigned int target);

/* ===== State functions ===== */
void _glsEnable(unsigned int cap);
void _glsDisable(unsigned int cap);
void _glsBlendFunc(unsigned int sfactor, unsigned int dfactor);
void _glsDepthFunc(unsigned int func);
void _glsDepthMask(unsigned char flag);
void _glsCullFace(unsigned int mode);
void _glsFrontFace(unsigned int mode);
void _glsColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void _glsAlphaFunc(unsigned int func, float ref);
void _glsStencilFunc(unsigned int func, int ref, unsigned int mask);
void _glsStencilOp(unsigned int fail, unsigned int zfail, unsigned int zpass);
void _glsPolygonMode(unsigned int face, unsigned int mode);
void _glsPolygonOffset(float factor, float units);
void _glsLineWidth(float width);
void _glsPointSize(float size);
void _glsScissor(int x, int y, int width, int height);
void _glsViewport(int x, int y, int width, int height);
void _glsDepthRange(double nearVal, double farVal);

/* ===== Clear functions ===== */
void _glsClearColor(float r, float g, float b, float a);
void _glsClearDepth(double depth);
void _glsClearStencil(int s);
void _glsClear(unsigned int mask);

/* ===== Matrix functions ===== */
void _glsMatrixMode(unsigned int mode);
void _glsLoadIdentity(void);
void _glsLoadMatrixf(const float *m);
void _glsLoadMatrixd(const double *m);
void _glsMultMatrixf(const float *m);
void _glsMultMatrixd(const double *m);
void _glsPushMatrix(void);
void _glsPopMatrix(void);
void _glsTranslatef(float x, float y, float z);
void _glsTranslated(double x, double y, double z);
void _glsRotatef(float angle, float x, float y, float z);
void _glsRotated(double angle, double x, double y, double z);
void _glsScalef(float x, float y, float z);
void _glsScaled(double x, double y, double z);
void _glsOrtho(double l, double r, double b, double t, double n, double f);
void _glsFrustum(double l, double r, double b, double t, double n, double f);

/* ===== Immediate mode ===== */
void _glsBegin(unsigned int mode);
void _glsEnd(void);
void _glsVertex2f(float x, float y);
void _glsVertex3f(float x, float y, float z);
void _glsVertex4f(float x, float y, float z, float w);
void _glsColor3f(float r, float g, float b);
void _glsColor4f(float r, float g, float b, float a);
void _glsColor3ub(unsigned char r, unsigned char g, unsigned char b);
void _glsColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void _glsNormal3f(float nx, float ny, float nz);
void _glsTexCoord2f(float s, float t);
void _glsTexCoord3f(float s, float t, float r);
void _glsTexCoord4f(float s, float t, float r, float q);

/* ===== Shader functions ===== */
unsigned int _glsCreateShader(unsigned int type);
void _glsDeleteShader(unsigned int shader);
void _glsShaderSource(unsigned int shader, int count, const char *const*string, const int *length);
void _glsCompileShader(unsigned int shader);
void _glsGetShaderiv(unsigned int shader, unsigned int pname, int *params);
void _glsGetShaderInfoLog(unsigned int shader, int bufSize, int *length, char *infoLog);
unsigned int _glsCreateProgram(void);
void _glsDeleteProgram(unsigned int program);
void _glsAttachShader(unsigned int program, unsigned int shader);
void _glsDetachShader(unsigned int program, unsigned int shader);
unsigned char _glsIsVertexArray(unsigned int array);
void _glsGetBufferPointerv(unsigned int target, unsigned int pname, void **params);
void _glsLinkProgram(unsigned int program);
void _glsUseProgram(unsigned int program);
void _glsGetProgramiv(unsigned int program, unsigned int pname, int *params);
void _glsGetProgramInfoLog(unsigned int program, int bufSize, int *length, char *infoLog);

/* ===== Pixel store ===== */
void _glsPixelStorei(unsigned int pname, int param);

/* ===== Get functions ===== */
unsigned int _glsGetError(void);
unsigned char _glsIsEnabled(unsigned int cap);
void _glsGetBooleanv(unsigned int pname, unsigned char *params);
void _glsGetFloatv(unsigned int pname, float *params);
void _glsGetIntegerv(unsigned int pname, int *params);

/* ===== Uniform functions ===== */
void _glsUniform1i(int loc, int v0);
void _glsUniform2i(int loc, int v0, int v1);
void _glsUniform3i(int loc, int v0, int v1, int v2);
void _glsUniform4i(int loc, int v0, int v1, int v2, int v3);
void _glsUniform1f(int loc, float v0);
void _glsUniform2f(int loc, float v0, float v1);
void _glsUniform3f(int loc, float v0, float v1, float v2);
void _glsUniform4f(int loc, float v0, float v1, float v2, float v3);
void _glsUniform1iv(int loc, int count, const int *v);
void _glsUniform2iv(int loc, int count, const int *v);
void _glsUniform3iv(int loc, int count, const int *v);
void _glsUniform4iv(int loc, int count, const int *v);
void _glsUniform1fv(int loc, int count, const float *v);
void _glsUniform2fv(int loc, int count, const float *v);
void _glsUniform3fv(int loc, int count, const float *v);
void _glsUniform4fv(int loc, int count, const float *v);
void _glsUniformMatrix2fv(int loc, int count, unsigned char transpose, const float *v);
void _glsUniformMatrix3fv(int loc, int count, unsigned char transpose, const float *v);
void _glsUniformMatrix4fv(int loc, int count, unsigned char transpose, const float *v);

/* ===== Vertex attrib functions ===== */
void _glsVertexAttrib1f(unsigned int index, float x);
void _glsVertexAttrib2f(unsigned int index, float x, float y);
void _glsVertexAttrib3f(unsigned int index, float x, float y, float z);
void _glsVertexAttrib4f(unsigned int index, float x, float y, float z, float w);
void _glsVertexAttribPointer(unsigned int index, int size, unsigned int type, unsigned char normalized, int stride, const void *pointer);

/* ===== Deferred state application =====
 * Push accumulated GL state to the D3D9 device.  Called from the relevant
 * setters, and again when the corresponding capability is enabled, since
 * state set while a capability was off has never reached the device. */
void _glsApplyStencilState(void);
void _glsApplyFogState(void);

/* ===== Shader introspection ===== */
int _glsGetUniformLocation(unsigned int program, const char *name);
int _glsGetAttribLocation(unsigned int program, const char *name);

/* ===== Legacy client-side vertex arrays (GL 1.1) ===== */
void _glsVertexPointer(int size, unsigned int type, int stride, const void *pointer);
void _glsNormalPointer(unsigned int type, int stride, const void *pointer);
void _glsColorPointer(int size, unsigned int type, int stride, const void *pointer);
void _glsTexCoordPointer(int size, unsigned int type, int stride, const void *pointer);
void _glsClientActiveTexture(unsigned int texture);
void _glsEnableClientState(unsigned int array);
void _glsDisableClientState(unsigned int array);

void _glsArrayElement(int i);
void _glsInterleavedArrays(unsigned int format, int stride, const void *pointer);
void _glsIndexPointer(unsigned int type, int stride, const void *pointer);
void _glsLockArraysEXT(int first, int count);
void _glsUnlockArraysEXT(void);

/* ===== Program introspection ===== */
void _glsGetActiveUniform(unsigned int program, unsigned int index, int bufSize, int *length, int *size, unsigned int *type, char *name);
void _glsGetActiveAttrib(unsigned int program, unsigned int index, int bufSize, int *length, int *size, unsigned int *type, char *name);
unsigned char _glsIsBuffer(unsigned int buffer);
void _glsGetBufferParameteriv(unsigned int target, unsigned int pname, int *params);

void _glsEnableVertexAttribArray(unsigned int index);
void _glsDisableVertexAttribArray(unsigned int index);
void _glsVertexAttribIPointer(unsigned int index, int size, unsigned int type, int stride, const void *pointer);
void _glsVertexAttribDivisor(unsigned int index, unsigned int divisor);

/* ===== Blend/Stencil separate ===== */
void _glsBlendEquationSeparate(unsigned int modeRGB, unsigned int modeAlpha);
void _glsBlendFuncSeparate(unsigned int sfRGB, unsigned int dfRGB, unsigned int sfA, unsigned int dfA);
void _glsStencilFuncSeparate(unsigned int face, unsigned int func, int ref, unsigned int mask);
void _glsStencilOpSeparate(unsigned int face, unsigned int sfail, unsigned int dpfail, unsigned int dppass);
void _glsStencilMaskSeparate(unsigned int face, unsigned int mask);
void _glsDrawBuffers(int n, const unsigned int *bufs);
void _glsBlendEquation(unsigned int mode);
void _glsBlendColor(float r, float g, float b, float a);

/* ===== FBO attachments ===== */
void _glsFramebufferTexture2D(unsigned int target, unsigned int attachment, unsigned int textarget, unsigned int texture, int level);
void _glsFramebufferRenderbuffer(unsigned int target, unsigned int attachment, unsigned int rbtarget, unsigned int rb);
void _glsBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1, unsigned int mask, unsigned int filter);
void _glsFramebufferTexture(unsigned int target, unsigned int attachment, unsigned int texture, int level);

/* ===== RBO ===== */
void _glsRenderbufferStorageMultisample(unsigned int target, int samples, unsigned int fmt, int w, int h);

/* ===== Clear buffers (GL 3.0) ===== */
void _glsClearBufferfv(unsigned int buffer, int drawbuffer, const float *value);
void _glsClearBufferiv(unsigned int buffer, int drawbuffer, const int *value);
void _glsClearBufferuiv(unsigned int buffer, int drawbuffer, const unsigned int *value);
void _glsClearBufferfi(unsigned int buffer, int drawbuffer, float depth, int stencil);

/* ===== Buffer range ===== */
void *_glsMapBufferRange(unsigned int target, ptrdiff_t offset, ptrdiff_t length, unsigned int access);
void _glsFlushMappedBufferRange(unsigned int target, ptrdiff_t offset, ptrdiff_t length);
void _glsGenerateMipmap(unsigned int target);
void _glsBindBufferBase(unsigned int target, unsigned int index, unsigned int buffer);
void _glsBindBufferRange(unsigned int target, unsigned int index, unsigned int buffer, ptrdiff_t offset, ptrdiff_t size);

/* ===== Transform feedback ===== */
void _glsBeginTransformFeedback(unsigned int primitiveMode);
void _glsEndTransformFeedback(void);
void _glsTransformFeedbackVaryings(unsigned int program, int count, const char *const*varyings, unsigned int bufferMode);

/* ===== Validate / Is / BindAttrib ===== */
void _glsValidateProgram(unsigned int program);
unsigned char _glsIsShader(unsigned int shader);
unsigned char _glsIsProgram(unsigned int program);
void _glsBindAttribLocation(unsigned int program, unsigned int index, const char *name);

/* ===== GL 3.1+ ===== */
void _glsDrawArraysInstanced(unsigned int mode, int first, int count, int instancecount);
void _glsDrawElementsInstanced(unsigned int mode, int count, unsigned int type, const void *indices, int instancecount);
void _glsDrawArraysInstancedBaseInstance(unsigned int mode, int first, int count,
                                         int instancecount, unsigned int baseinstance);
void _glsDrawElementsInstancedBaseVertexBaseInstance(
    unsigned int mode, int count, unsigned int type, const void *indices,
    int instancecount, int basevertex, unsigned int baseinstance);
void _glsCopyBufferSubData(unsigned int readTarget, unsigned int writeTarget, ptrdiff_t readOffset, ptrdiff_t writeOffset, ptrdiff_t size);
void _glsTexBuffer(unsigned int target, unsigned int internalformat, unsigned int buffer);
void _glsTexBufferRange(unsigned int target, unsigned int internalformat,
                        unsigned int buffer, ptrdiff_t offset, ptrdiff_t size);
void _glsPrimitiveRestartIndex(unsigned int index);
void _glsUniformBlockBinding(unsigned int program, unsigned int blockIndex, unsigned int blockBinding);

/* ===== GL 3.2 — Sync ===== */
void *_glsFenceSync(unsigned int condition, unsigned int flags);
void _glsDeleteSync(void *sync);
unsigned int _glsClientWaitSync(void *sync, unsigned int flags, unsigned long long timeout);
void _glsWaitSync(void *sync, unsigned int flags, unsigned long long timeout);
void _glsProvokingVertex(unsigned int mode);

/* ===== GL 3.3 — Samplers ===== */
void _glsBindSampler(unsigned int unit, unsigned int sampler);
void _glsSamplerParameteri(unsigned int sampler, unsigned int pname, int param);
void _glsSamplerParameterf(unsigned int sampler, unsigned int pname, float param);
void _glsSamplerParameteriv(unsigned int sampler, unsigned int pname, const int *param);
void _glsSamplerParameterfv(unsigned int sampler, unsigned int pname, const float *param);

/* ===== GL 4.x ===== */
void _glsTexStorage2D(unsigned int target, int levels, unsigned int internalformat, int width, int height);
void _glsTexStorage3D(unsigned int target, int levels, unsigned int internalformat, int width, int height, int depth);
void _glsDispatchCompute(unsigned int x, unsigned int y, unsigned int z);
void _glsDebugMessageCallback(void *callback, const void *userParam);
void _glsClipControl(unsigned int origin, unsigned int depth);

/* ===== Conditional render ===== */
void _glsBeginConditionalRender(unsigned int id, unsigned int mode);
void _glsEndConditionalRender(void);

/* ===== Indexed state ===== */
void _glsColorMaski(unsigned int index, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void _glsEnablei(unsigned int target, unsigned int index);
void _glsDisablei(unsigned int target, unsigned int index);

/* ===== Queries ===== */
void _glsBeginQuery(unsigned int target, unsigned int id);
void _glsEndQuery(unsigned int target);
void _glsQueryCounter(unsigned int id, unsigned int target);

/* ===== GL 3.2 multisample textures ===== */
void _glsTexImage2DMultisample(unsigned int target, int samples, unsigned int internalformat, int width, int height, unsigned char fixedsamplelocations);
void _glsTexImage3DMultisample(unsigned int target, int samples, unsigned int internalformat, int width, int height, int depth, unsigned char fixedsamplelocations);
void _glsSampleMaski(unsigned int maskNumber, unsigned int mask);

/* ===== GL 4.x misc ===== */
void _glsMinSampleShading(float value);
void _glsBlendEquationi(unsigned int buf, unsigned int mode);
void _glsBlendFunci(unsigned int buf, unsigned int src, unsigned int dst);
void _glsPatchParameteri(unsigned int pname, int value);
void _glsMemoryBarrier(unsigned int barriers);
void _glsBindImageTexture(unsigned int unit, unsigned int texture, int level, unsigned char layered, int layer, unsigned int access, unsigned int format);
void _glsDebugMessageControl(unsigned int source, unsigned int type, unsigned int severity, int count, const unsigned int *ids, unsigned char enabled);
void _glsObjectLabel(unsigned int identifier, unsigned int name, int length, const char *label);
void _glsTextureBarrier(void);

/* ===== Texture 3D / misc ===== */
void _glsTexImage3D(unsigned int target, int level, int internalformat, int width, int height, int depth, int border, unsigned int format, unsigned int type, const void *pixels);
void _glsTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, unsigned int type, const void *pixels);

/* Private software-stage resource bridge.  It transfers one existing texture
 * level by object instead of by public binding, so the compute/programmable
 * stage worker can keep its context private without perturbing application GL
 * state.  Cube faces are selected with a face target; volume levels transfer
 * all slices. */
BOOL _glsTransferTextureLevel(GLS_Texture *texture, unsigned int target, int level,
                              unsigned int format, unsigned int type, void *pixels,
                              BOOL writeToTexture, int *width, int *height, int *depth);
BOOL _glsWriteBufferObject(GLS_Buffer *buffer, ptrdiff_t offset,
                           ptrdiff_t size, const void *data);
void _glsCompressedTexImage3D(unsigned int target, int level, unsigned int internalformat, int width, int height, int depth, int border, int imageSize, const void *data);
void _glsCompressedTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, int imageSize, const void *data);
void _glsCompressedTexImage1D(unsigned int target, int level, unsigned int internalformat, int width, int border, int imageSize, const void *data);
void _glsCompressedTexSubImage1D(unsigned int target, int level, int xoffset, int width, unsigned int format, int imageSize, const void *data);
void _glsCompressedTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, int imageSize, const void *data);
void _glsGetCompressedTexImage(unsigned int target, int level, void *img);
void _glsGetTexImage(unsigned int target, int level, unsigned int format, unsigned int type, void *pixels);
void _glsGetTexLevelParameteriv(unsigned int target, int level, unsigned int pname, int *params);
void _glsGetTexParameteriv(unsigned int target, unsigned int pname, int *params);
unsigned char _glsAreTexturesResident(int n, const unsigned int *textures, unsigned char *residences);
void _glsCopyTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int x, int y, int width, int height);
void _glsDrawRangeElements(unsigned int mode, unsigned int start, unsigned int end, int count, unsigned int type, const void *indices);
void _glsSampleCoverage(float value, unsigned char invert);
void _glsLoadTransposeMatrixf(const float *m);
void _glsLoadTransposeMatrixd(const double *m);
void _glsMultTransposeMatrixf(const float *m);
void _glsMultTransposeMatrixd(const double *m);
void _glsFogCoordf(float coord);
void _glsFogCoordfv(const float *coord);
void _glsFogCoordd(double coord);
void _glsFogCoorddv(const double *coord);
void _glsFogCoordPointer(unsigned int type, int stride, const void *pointer);
void _glsSecondaryColor3f(float r, float g, float b);
void _glsSecondaryColor3fv(const float *v);
void _glsSecondaryColor3ub(unsigned char r, unsigned char g, unsigned char b);
void _glsSecondaryColor3ubv(const unsigned char *v);
void _glsSecondaryColorPointer(int size, unsigned int type, int stride, const void *pointer);
void _glsMultiDrawArrays(unsigned int mode, const int *first, const int *count, int drawcount);
void _glsMultiDrawElements(unsigned int mode, const int *count, unsigned int type, const void *const*indices, int drawcount);
void _glsPointParameterf(unsigned int pname, float param);
void _glsPointParameterfv(unsigned int pname, const float *params);
void _glsPointParameteri(unsigned int pname, int param);
void _glsPointParameteriv(unsigned int pname, const int *params);
void _glsGetBufferSubData(unsigned int target, ptrdiff_t offset, ptrdiff_t size, void *data);

/* ===== ARB program (assembly shaders) ===== */
void _glsGenProgramsARB(int n, unsigned int *programs);
void _glsDeleteProgramsARB(int n, const unsigned int *programs);
void _glsBindProgramARB(unsigned int target, unsigned int program);
void _glsProgramStringARB(unsigned int target, unsigned int format, int len, const void *string);
void _glsProgramEnvParameter4fvARB(unsigned int target, unsigned int index, const float *params);
void _glsProgramLocalParameter4fvARB(unsigned int target, unsigned int index, const float *params);

/* ===== ARB shader object (handles both shaders and programs) ===== */
void _glsDeleteObjectARB(unsigned int obj);
void _glsGetObjectParameterivARB(unsigned int obj, unsigned int pname, int *params);
void _glsGetObjectParameterfvARB(unsigned int obj, unsigned int pname, float *params);
unsigned int _glsGetHandleARB(unsigned int pname);
void _glsGetAttachedObjectsARB(unsigned int containerObj, int maxCount, int *count, unsigned int *obj);
void _glsGetProgramEnvParameterfvARB(unsigned int target, unsigned int index, float *params);
void _glsGetProgramEnvParameterdvARB(unsigned int target, unsigned int index, double *params);
void _glsGetProgramLocalParameterfvARB(unsigned int target, unsigned int index, float *params);
void _glsGetProgramLocalParameterdvARB(unsigned int target, unsigned int index, double *params);
void _glsProgramEnvParameter4dvARB(unsigned int target, unsigned int index, const double *params);
void _glsProgramLocalParameter4dvARB(unsigned int target, unsigned int index, const double *params);
void _glsGetProgramivARB(unsigned int target, unsigned int pname, int *params);
void _glsGetProgramStringARB(unsigned int target, unsigned int pname, void *string);
unsigned char _glsIsProgramARB(unsigned int program);
void _glsGetInfoLogARB(unsigned int obj, int maxLength, int *length, char *infoLog);

/* ===== Multitexture =====
 *
 * All four arities, all eight units.  The scalar/vector spellings in the
 * exported surface (d/f/i/s and their v forms) all convert to float and land
 * here, matching how the existing ARB wrappers were already written.
 *
 * Note on reach: these record complete 4-component coordinates for every unit,
 * which is what glGet and any ARB/GLSL shader reading the attribute observe.
 * The fixed-function submission path narrows that to units 0-1 at two
 * components: GLS_D3DVertex carries four texcoord sets, but sets 2 and 3 are
 * reserved for ARB generic attributes 6/7 rather than texture units.  Giving
 * units 2-7 real coordinates is a separate change and is not done here. */
void _glsMultiTexCoord1fARB(unsigned int target, float s);
void _glsMultiTexCoord2fARB(unsigned int target, float s, float t);
void _glsMultiTexCoord3fARB(unsigned int target, float s, float t, float r);
void _glsMultiTexCoord4fARB(unsigned int target, float s, float t, float r, float q);
void _glsMultiTexCoord1fvARB(unsigned int target, const float *v);
void _glsMultiTexCoord2fvARB(unsigned int target, const float *v);
void _glsMultiTexCoord3fvARB(unsigned int target, const float *v);
void _glsMultiTexCoord4fvARB(unsigned int target, const float *v);

/* ===== Texture coordinate generation ===== */
void _glsTexGeni(unsigned int coord, unsigned int pname, int param);
void _glsTexGenfv(unsigned int coord, unsigned int pname, const float *params);
void _glsGetTexGenfv(unsigned int coord, unsigned int pname, float *params);
void _glsGetTexGeniv(unsigned int coord, unsigned int pname, int *params);
void _glsApplyTexGenState(int unit);

/* ===== Stencil two-side ===== */
void _glsActiveStencilFaceEXT(unsigned int face);

/* ===== Fog functions ===== */
void _glsFogf(unsigned int pname, float param);
void _glsFogi(unsigned int pname, int param);
void _glsFogfv(unsigned int pname, const float *params);
void _glsFogiv(unsigned int pname, const int *params);

/* ===== Light functions ===== */
void _glsLightf(unsigned int light, unsigned int pname, float param);
void _glsLightfv(unsigned int light, unsigned int pname, const float *params);
void _glsLightModelf(unsigned int pname, float param);
void _glsLightModelfv(unsigned int pname, const float *params);

/* ===== Material functions ===== */
void _glsMaterialf(unsigned int face, unsigned int pname, float param);
void _glsMaterialfv(unsigned int face, unsigned int pname, const float *params);

/* ===== Display list functions ===== */
void _glsNewList(unsigned int list, unsigned int mode);
void _glsEndList(void);
void _glsCallList(unsigned int list);
void _glsCallLists(int n, unsigned int type, const void *lists);
unsigned int _glsGenLists(int range);
void _glsDeleteLists(unsigned int list, int range);
void _glsListBase(unsigned int base);
unsigned char _glsIsList(unsigned int list);

/* ===== Clip plane ===== */
void _glsClipPlane(unsigned int plane, const double *equation);
void _glsGetClipPlane(unsigned int plane, double *equation);

/* ===== Misc legacy state ===== */
void _glsColorMaterial(unsigned int face, unsigned int mode);
void _glsShadeModel(unsigned int mode);
void _glsHint(unsigned int target, unsigned int mode);
void _glsLogicOp(unsigned int opcode);
void _glsReadBuffer(unsigned int mode);
void _glsDrawBuffer(unsigned int mode);
void _glsPushAttrib(unsigned int mask);
void _glsPopAttrib(void);
void _glsPushClientAttrib(unsigned int mask);
void _glsPopClientAttrib(void);

/* ===== Get legacy state ===== */
void _glsGetLightfv(unsigned int light, unsigned int pname, float *params);
void _glsGetLightiv(unsigned int light, unsigned int pname, int *params);
void _glsGetMaterialfv(unsigned int face, unsigned int pname, float *params);
void _glsGetMaterialiv(unsigned int face, unsigned int pname, int *params);

/* ===== D3D9 helper types =====
 *
 * One fat vertex format is used for every draw.  Member order is not free:
 * D3D9 requires FVF components to appear as position, normal, diffuse, then
 * texture coordinates, and the struct must match that order exactly.
 *
 * Normals and the second texcoord set are always present even when the
 * application supplies neither.  Selecting a narrower FVF per draw would save
 * bandwidth but requires a matching struct layout per permutation; a single
 * format keeps assembly correct, which matters more here than the few bytes.
 *
 * Texcoord sets 2 and 3 are not texture coordinates at all: they carry ARB
 * generic vertex attributes 6 and 7, the only two indices ARB_vertex_program
 * leaves without a conventional alias (every other index aliases position,
 * weight, normal, colour, fogcoord or a texcoord set).  A spare texcoord set
 * is the cheapest way to hand a vertex shader arbitrary per-vertex data under
 * an FVF.  They are 4-component because a generic attribute is a vec4 in
 * general and the translator always declares its inputs as float4; the
 * default 2-component texcoord size would silently truncate one.
 */
typedef struct {
    float x, y, z;
    float nx, ny, nz;
    DWORD color;
    DWORD specular;     /* glSecondaryColor; D3D9 mandates this order */
    float u0, v0;
    float u1, v1;
    float genericAttrib6[4];    /* texcoord set 2: ARB vertex.attrib[6] */
    float genericAttrib7[4];    /* texcoord set 3: ARB vertex.attrib[7] */
} GLS_D3DVertex;

#define GLS_D3DFVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | \
                    D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE4(2) | D3DFVF_TEXCOORDSIZE4(3))

/* ===== D3D9 helper functions ===== */
D3DFORMAT _glsMapGLFormatToD3D(unsigned int internalformat);
D3DFORMAT _glsMapCompressedFormatToD3D(unsigned int internalformat);
void _glsCopyPixelsToD3D(void *dst, const void *src, int width, int height, unsigned int glFormat, unsigned int glType, int dstPitch, D3DFORMAT dstFmt);
/* Mirror of the above.  `flipRows` reverses row order, which glReadPixels needs
 * because GL images run bottom-up and D3D9 surfaces top-down. */
void _glsCopyPixelsFromD3D(void *dst, const void *src, int width, int height, unsigned int glFormat, unsigned int glType, int srcPitch, D3DFORMAT srcFmt, int flipRows);
void _glsApplyD3DCullMode(void);
D3DBLEND _glsMapBlendFactor(unsigned int glFactor);
D3DCMPFUNC _glsMapCompareFunc(unsigned int glFunc);

/* ===== Pixel read-back / framebuffer-to-texture ===== */
void _glsReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void *pixels);
void _glsCopyTexImage2D(unsigned int target, int level, unsigned int internalformat, int x, int y, int width, int height, int border);
void _glsCopyTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int x, int y, int width, int height);
void _glsCopyPixels(int x, int y, int width, int height, unsigned int type);
void _glsDrawPixels(int width, int height, unsigned int format, unsigned int type, const void *pixels);

/* ===== Draw calls ===== */
void _glsDrawArrays(unsigned int mode, int first, int count);
void _glsDrawElements(unsigned int mode, int count, unsigned int type, const void *indices);
void _glsDrawElementsBaseVertex(unsigned int mode, int count, unsigned int type,
                                const void *indices, int basevertex);


/* ===== GL 1.x fixed-function paths (gl_legacy_impl.c) ===== */
void _glsApplyTexEnv(int unit);
void _glsInitTexEnvDefaults(void);
void _glsTexEnvf(unsigned int target, unsigned int pname, float param);
void _glsTexEnvfv(unsigned int target, unsigned int pname, const float *params);
void _glsTexEnvi(unsigned int target, unsigned int pname, int param);
void _glsTexEnviv(unsigned int target, unsigned int pname, const int *params);
void _glsGetTexEnvfv(unsigned int target, unsigned int pname, float *params);
void _glsGetTexEnviv(unsigned int target, unsigned int pname, int *params);
void _glsTexImage1D(unsigned int target, int level, int internalformat, int width, int border, unsigned int format, unsigned int type, const void *pixels);
void _glsTexSubImage1D(unsigned int target, int level, int xoffset, int width, unsigned int format, unsigned int type, const void *pixels);
void _glsRasterPos4f(float x, float y, float z, float w);
void _glsWindowPos3f(float x, float y, float z);
void _glsPixelZoom(float xfactor, float yfactor);
void _glsBitmap(int width, int height, float xorig, float yorig, float xmove, float ymove, const unsigned char *bitmap);
void _glsMap1f(unsigned int target, float u1, float u2, int stride, int order, const float *points);
void _glsMap2f(unsigned int target, float u1, float u2, int ustride, int uorder, float v1, float v2, int vstride, int vorder, const float *points);
void _glsEvalCoord1f(float u);
void _glsEvalCoord2f(float u, float v);
void _glsMapGrid1f(int un, float u1, float u2);
void _glsMapGrid2f(int un, float u1, float u2, int vn, float v1, float v2);
void _glsEvalMesh1(unsigned int mode, int i1, int i2);
void _glsEvalMesh2(unsigned int mode, int i1, int i2, int j1, int j2);
void _glsEvalPoint1(int i);
void _glsEvalPoint2(int i, int j);
void _glsGetMapfv(unsigned int target, unsigned int query, float *v);
BOOL _glsSetEvalEnable(unsigned int cap, BOOL enable);
void _glsSelectBuffer(int size, unsigned int *buffer);
void _glsFeedbackBuffer(int size, unsigned int type, float *buffer);
void _glsInitNames(void);
void _glsPushName(unsigned int name);
void _glsPopName(void);
void _glsLoadName(unsigned int name);
void _glsPassThrough(float token);
int  _glsRenderMode(unsigned int mode);
void _glsSelectRecordHit(float minZ, float maxZ);
void _glsPolygonStipple(const unsigned char *mask);
void _glsGetPolygonStipple(unsigned char *mask);
void _glsLineStipple(int factor, unsigned short pattern);
void _glsEdgeFlag(unsigned char flag);
void _glsIndexf(float c);
void _glsClearIndex(float c);
void _glsIndexMask(unsigned int mask);
void _glsPixelMapfv(unsigned int map, int mapsize, const float *values);
void _glsGetPixelMapfv(unsigned int map, float *values);
void _glsPixelTransferf(unsigned int pname, float param);
void _glsApplyPixelTransferRGBA(unsigned char *rgba, int count);
void _glsClearAccum(float r, float g, float b, float a);
void _glsAccum(unsigned int op, float value);

void _glsMap1d(unsigned int target, double u1, double u2, int stride, int order, const double *points);
void _glsMap2d(unsigned int target, double u1, double u2, int ustride, int uorder, double v1, double v2, int vstride, int vorder, const double *points);
void _glsGetMapiv(unsigned int target, unsigned int query, int *v);
void _glsPixelMapuiv(unsigned int map, int mapsize, const unsigned int *values);
void _glsPixelMapusv(unsigned int map, int mapsize, const unsigned short *values);
void _glsGetPixelMapuiv(unsigned int map, unsigned int *values);
void _glsGetPixelMapusv(unsigned int map, unsigned short *values);
void _glsPixelTransferi(unsigned int pname, int param);
void _glsPrioritizeTextures(int n, const unsigned int *textures, const float *priorities);

void _glsFinish(void);
void _glsFlush(void);
unsigned char _glsIsTexture(unsigned int texture);
void _glsGetPointerv(unsigned int pname, void **params);
void _glsEdgeFlagPointer(int stride, const void *pointer);
void _glsColorTableEXT(unsigned int target, unsigned int internalformat, int width, unsigned int format, unsigned int type, const void *table);
void _glsColorSubTableEXT(unsigned int target, int start, int count, unsigned int format, unsigned int type, const void *data);
void _glsGetColorTableEXT(unsigned int target, unsigned int format, unsigned int type, void *data);
void _glsGetColorTableParameterivEXT(unsigned int target, unsigned int pname, int *params);
void _glsGetColorTableParameterfvEXT(unsigned int target, unsigned int pname, float *params);
void _glsResizeBuffersMESA(void);

/* ===== Device lifetime =====
 *
 * g_glState is process-global and outlives any single GL context, so the two
 * moments where the D3D9 device changes underneath it have to be told about.
 *
 * _glsReleaseDeviceLosableResources: called immediately before
 * IDirect3DDevice9::Reset.  Releases only what a reset destroys and what has a
 * re-creation path; managed textures and shader objects are deliberately kept.
 *
 * _glsReleaseAllDeviceResources: called once at real process shutdown, just
 * before the device is released.  Releases everything.
 */
void _glsReleaseDeviceLosableResources(IDirect3DDevice9 *pDev);
void _glsReleaseAllDeviceResources(IDirect3DDevice9 *pDev);

#ifdef __cplusplus
}
#endif

#endif /* GL_IMPL_H */
