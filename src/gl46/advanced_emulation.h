/*********************************************************************************
*
* advanced_emulation.h - GL 4.x state/resource operations implemented above the
*                        native D3D9 feature set.
*
* These entry points are called by the exactly-typed generated GL dispatchers.
* They deliberately keep the public GL ABI out of this module: the exported
* functions retain APIENTRY in gl_generated_stubs.h, while these are ordinary C
* helpers operating on GLDirect's context state and D3D9 resources.
*
*********************************************************************************/

#ifndef GLD_ADVANCED_EMULATION_H
#define GLD_ADVANCED_EMULATION_H

#include <glad/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void gldAdvReset(void);

void gldAdvActiveShaderProgram(GLuint pipeline, GLuint program);
void gldAdvBindFragDataLocation(GLuint program, GLuint color, const GLchar *name);
void gldAdvBindFragDataLocationIndexed(GLuint program, GLuint colorNumber,
                                       GLuint index, const GLchar *name);
void gldAdvBindProgramPipeline(GLuint pipeline);
void gldAdvGenProgramPipelines(GLsizei n, GLuint *pipelines);
void gldAdvDeleteProgramPipelines(GLsizei n, const GLuint *pipelines);
GLboolean gldAdvIsProgramPipeline(GLuint pipeline);
void gldAdvUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program);
void gldAdvValidateProgramPipeline(GLuint pipeline);
void gldAdvGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params);
void gldAdvGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize,
                                     GLsizei *length, GLchar *infoLog);

void gldAdvBindVertexBuffer(GLuint bindingindex, GLuint buffer,
                            GLintptr offset, GLsizei stride);
void gldAdvBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                             const GLintptr *offsets, const GLsizei *strides);
void gldAdvVertexAttribBinding(GLuint attribindex, GLuint bindingindex);
void gldAdvVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                              GLboolean normalized, GLuint relativeoffset);
void gldAdvVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                               GLuint relativeoffset);
void gldAdvVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                               GLuint relativeoffset);
void gldAdvVertexAttribLPointer(GLuint index, GLint size, GLenum type,
                                GLsizei stride, const void *pointer);
void gldAdvVertexBindingDivisor(GLuint bindingindex, GLuint divisor);

void gldAdvBufferStorage(GLenum target, GLsizeiptr size, const void *data,
                         GLbitfield flags);
void gldAdvClearBufferData(GLenum target, GLenum internalformat, GLenum format,
                           GLenum type, const void *data);
void gldAdvClearBufferSubData(GLenum target, GLenum internalformat,
                              GLintptr offset, GLsizeiptr size, GLenum format,
                              GLenum type, const void *data);
void gldAdvClearTexImage(GLuint texture, GLint level, GLenum format,
                         GLenum type, const void *data);
void gldAdvClearTexSubImage(GLuint texture, GLint level, GLint xoffset,
                            GLint yoffset, GLint zoffset, GLsizei width,
                            GLsizei height, GLsizei depth, GLenum format,
                            GLenum type, const void *data);
void gldAdvCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                            GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                            GLenum dstTarget, GLint dstLevel, GLint dstX,
                            GLint dstY, GLint dstZ, GLsizei srcWidth,
                            GLsizei srcHeight, GLsizei srcDepth);
void gldAdvGetTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                              GLint yoffset, GLint zoffset, GLsizei width,
                              GLsizei height, GLsizei depth, GLenum format,
                              GLenum type, GLsizei bufSize, void *pixels);
void gldAdvGetCompressedTextureSubImage(GLuint texture, GLint level,
                                        GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLsizei width,
                                        GLsizei height, GLsizei depth,
                                        GLsizei bufSize, void *pixels);
void gldAdvTextureView(GLuint texture, GLenum target, GLuint origtexture,
                       GLenum internalformat, GLuint minlevel,
                       GLuint numlevels, GLuint minlayer, GLuint numlayers);

void gldAdvDispatchComputeIndirect(GLintptr indirect);
void gldAdvDrawArraysIndirect(GLenum mode, const void *indirect);
void gldAdvDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect);
void gldAdvMultiDrawArraysIndirect(GLenum mode, const void *indirect,
                                   GLsizei drawcount, GLsizei stride);
void gldAdvMultiDrawArraysIndirectCount(GLenum mode, const void *indirect,
                                        GLintptr drawcount, GLsizei maxdrawcount,
                                        GLsizei stride);
void gldAdvMultiDrawElementsIndirect(GLenum mode, GLenum type,
                                     const void *indirect, GLsizei drawcount,
                                     GLsizei stride);
void gldAdvMultiDrawElementsIndirectCount(GLenum mode, GLenum type,
                                          const void *indirect,
                                          GLintptr drawcount,
                                          GLsizei maxdrawcount, GLsizei stride);
void gldAdvDrawArraysInstancedBaseInstance(GLenum mode, GLint first,
                                            GLsizei count,
                                            GLsizei instancecount,
                                            GLuint baseinstance);
void gldAdvDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                  const void *indices, GLint basevertex);
void gldAdvDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count,
                                              GLenum type, const void *indices,
                                              GLsizei instancecount,
                                              GLuint baseinstance);
void gldAdvDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count,
                                            GLenum type, const void *indices,
                                            GLsizei instancecount,
                                            GLint basevertex);
void gldAdvDrawElementsInstancedBaseVertexBaseInstance(
    GLenum mode, GLsizei count, GLenum type, const void *indices,
    GLsizei instancecount, GLint basevertex, GLuint baseinstance);
void gldAdvDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                       GLsizei count, GLenum type,
                                       const void *indices, GLint basevertex);
void gldAdvMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count,
                                       GLenum type, const void *const*indices,
                                       GLsizei drawcount,
                                       const GLint *basevertex);

void gldAdvBindTransformFeedback(GLenum target, GLuint id);
void gldAdvDeleteTransformFeedbacks(GLsizei n, const GLuint *ids);
void gldAdvGenTransformFeedbacks(GLsizei n, GLuint *ids);
GLboolean gldAdvIsTransformFeedback(GLuint id);
void gldAdvPauseTransformFeedback(void);
void gldAdvResumeTransformFeedback(void);
void gldAdvBeginTransformFeedback(GLenum mode);
void gldAdvEndTransformFeedback(void);
void gldAdvRecordTransformFeedbackDraw(GLenum mode, GLint first, GLsizei count);
void gldAdvDrawTransformFeedback(GLenum mode, GLuint id);
void gldAdvDrawTransformFeedbackInstanced(GLenum mode, GLuint id,
                                           GLsizei instancecount);
void gldAdvDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream);
void gldAdvDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id,
                                                 GLuint stream,
                                                 GLsizei instancecount);
void gldAdvTransformFeedbackVaryings(GLuint program, GLsizei count,
                                     const GLchar *const*varyings,
                                     GLenum bufferMode);
void gldAdvGetTransformFeedbackVarying(GLuint program, GLuint index,
                                       GLsizei bufSize, GLsizei *length,
                                       GLsizei *size, GLenum *type,
                                       GLchar *name);
void gldAdvTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer);
void gldAdvTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                        GLintptr offset, GLsizeiptr size);
void gldAdvGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param);
void gldAdvGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index,
                                   GLint *param);
void gldAdvGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index,
                                     GLint64 *param);

void gldAdvPatchParameterfv(GLenum pname, const GLfloat *values);
void gldAdvFramebufferParameteri(GLenum target, GLenum pname, GLint param);
void gldAdvGetSynciv(GLsync sync, GLenum pname, GLsizei count,
                     GLsizei *length, GLint *values);
GLenum gldAdvGetGraphicsResetStatus(void);
void gldAdvGetInternalformativ(GLenum target, GLenum internalformat,
                               GLenum pname, GLsizei count, GLint *params);
void gldAdvGetInternalformati64v(GLenum target, GLenum internalformat,
                                 GLenum pname, GLsizei count, GLint64 *params);
void gldAdvGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname,
                                          GLint *param);
void gldAdvGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname,
                                  GLintptr offset);
void gldAdvGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname,
                                   GLintptr offset);
void gldAdvGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname,
                                    GLintptr offset);
void gldAdvGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                     GLintptr offset);

void gldAdvObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                       const GLchar *label);
void gldAdvObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label);
void gldAdvGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize,
                          GLsizei *length, GLchar *label);
void gldAdvGetObjectPtrLabel(const void *ptr, GLsizei bufSize,
                             GLsizei *length, GLchar *label);

GLuint gldAdvCreateShaderProgramv(GLenum type, GLsizei count,
                                  const GLchar *const*strings);
void gldAdvProgramParameteri(GLuint program, GLenum pname, GLint value);
void gldAdvGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                            GLenum *binaryFormat, void *binary);
void gldAdvProgramBinary(GLuint program, GLenum binaryFormat,
                         const void *binary, GLsizei length);
void gldAdvShaderBinary(GLsizei count, const GLuint *shaders,
                        GLenum binaryFormat, const void *binary,
                        GLsizei length);
void gldAdvSpecializeShader(GLuint shader, const GLchar *entryPoint,
                            GLuint numConstants, const GLuint *constantIndex,
                            const GLuint *constantValue);

void gldAdvGetUniformIndices(GLuint program, GLsizei uniformCount,
                             const GLchar *const*uniformNames,
                             GLuint *uniformIndices);
void gldAdvGetActiveUniformName(GLuint program, GLuint uniformIndex,
                                GLsizei bufSize, GLsizei *length,
                                GLchar *uniformName);
void gldAdvGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                               const GLuint *uniformIndices, GLenum pname,
                               GLint *params);
void gldAdvGetProgramInterfaceiv(GLuint program, GLenum programInterface,
                                 GLenum pname, GLint *params);
GLuint gldAdvGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                     const GLchar *name);
GLint gldAdvGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                       const GLchar *name);
GLint gldAdvGetProgramResourceLocationIndex(GLuint program,
                                            GLenum programInterface,
                                            const GLchar *name);
void gldAdvGetProgramResourceName(GLuint program, GLenum programInterface,
                                  GLuint index, GLsizei bufSize,
                                  GLsizei *length, GLchar *name);
void gldAdvGetProgramResourceiv(GLuint program, GLenum programInterface,
                                GLuint index, GLsizei propCount,
                                const GLenum *props, GLsizei count,
                                GLsizei *length, GLint *params);
GLint gldAdvGetFragDataLocation(GLuint program, const GLchar *name);
GLint gldAdvGetFragDataIndex(GLuint program, const GLchar *name);

void gldAdvUniformBlockBinding(GLuint program, GLuint blockIndex,
                               GLuint blockBinding);
GLuint gldAdvGetUniformBlockIndex(GLuint program, const GLchar *name);
void gldAdvShaderStorageBlockBinding(GLuint program, GLuint blockIndex,
                                     GLuint blockBinding);
void gldAdvGetActiveUniformBlockName(GLuint program, GLuint blockIndex,
                                     GLsizei bufSize, GLsizei *length,
                                     GLchar *name);
void gldAdvGetActiveUniformBlockiv(GLuint program, GLuint blockIndex,
                                   GLenum pname, GLint *params);
void gldAdvGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                          GLenum pname, GLint *params);
void gldAdvGetProgramStageiv(GLuint program, GLenum shaderType, GLenum pname,
                             GLint *values);
GLuint gldAdvGetSubroutineIndex(GLuint program, GLenum shaderType,
                                const GLchar *name);
GLint gldAdvGetSubroutineUniformLocation(GLuint program, GLenum shaderType,
                                         const GLchar *name);
void gldAdvGetActiveSubroutineName(GLuint program, GLenum shaderType,
                                   GLuint index, GLsizei bufSize,
                                   GLsizei *length, GLchar *name);
void gldAdvGetActiveSubroutineUniformName(GLuint program, GLenum shaderType,
                                          GLuint index, GLsizei bufSize,
                                          GLsizei *length, GLchar *name);
void gldAdvGetActiveSubroutineUniformiv(GLuint program, GLenum shaderType,
                                        GLuint index, GLenum pname,
                                        GLint *values);
void gldAdvUniformSubroutinesuiv(GLenum shaderType, GLsizei count,
                                 const GLuint *indices);
void gldAdvGetUniformSubroutineuiv(GLenum shaderType, GLint location,
                                   GLuint *params);

void gldAdvDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                               GLsizei count, const GLuint *ids,
                               GLboolean enabled);
void gldAdvDebugMessageInsert(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message);
GLuint gldAdvGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                GLenum *types, GLuint *ids, GLenum *severities,
                                GLsizei *lengths, GLchar *messageLog);
void gldAdvPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                          const GLchar *message);
void gldAdvPopDebugGroup(void);

#ifdef __cplusplus
}
#endif

#endif
