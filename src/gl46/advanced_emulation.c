/*********************************************************************************
*
* advanced_emulation.c - GL operations whose state model is newer than D3D9.
*
* D3D9 supplies the storage and draw engine.  This module supplies the missing
* GL object model on the CPU, expands indirect/instanced commands into ordinary
* draws, maintains split vertex bindings and implements resource operations
* against the wrapper's CPU shadows before synchronising them to D3D9.
*
*********************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "advanced_emulation.h"
#include "gl_state.h"
#include "gl_impl.h"
#include "context_manager.h"
#include "gld_diag.h"

#define ADV_MAX_PIPELINES       256
#define ADV_MAX_TF_OBJECTS      256
#define ADV_MAX_LABELS          256
#define ADV_MAX_FRAG_BINDINGS   16
#define ADV_MAX_BLOCKS          16
#define ADV_MAX_SUBROUTINES     32
#define ADV_MAX_DEBUG_MESSAGES  64
#define ADV_BINARY_FORMAT       0x474C4439u /* 'GLD9' */
#define ADV_BINARY_VERSION      1u

typedef struct {
    BOOL allocated;
    BOOL validated;
    GLuint activeProgram;
    GLuint vertexProgram;
    GLuint fragmentProgram;
    GLuint geometryProgram;
    GLuint tessControlProgram;
    GLuint tessEvalProgram;
    GLuint computeProgram;
    char infoLog[256];
} AdvPipeline;

typedef struct {
    BOOL allocated;
    BOOL paused;
    GLenum mode;
    GLint first;
    GLsizei count;
    GLS_IndexedBufferBinding bindings[GLS_MAX_BUFFER_BINDINGS];
} AdvTransformFeedback;

typedef struct {
    BOOL used;
    GLenum identifier;
    GLuint name;
    const void *ptr;
    char text[256];
} AdvLabel;

typedef struct {
    BOOL used;
    GLuint color;
    GLuint index;
    char name[64];
} AdvFragBinding;

typedef struct {
    AdvFragBinding frag[ADV_MAX_FRAG_BINDINGS];
    GLint fragCount;
    GLuint uniformBlockBinding[ADV_MAX_BLOCKS];
    GLuint storageBlockBinding[ADV_MAX_BLOCKS];
    GLuint subroutineSelection[6][ADV_MAX_SUBROUTINES];
    GLenum transformFeedbackMode;
    GLint transformFeedbackCount;
    char transformFeedbackVaryings[GLS_MAX_VERTEX_ATTRIBS][64];
} AdvProgramInfo;

typedef struct {
    GLenum source;
    GLenum type;
    GLuint id;
    GLenum severity;
    GLsizei length;
    char message[512];
} AdvDebugMessage;

typedef struct {
    DWORD magic;
    DWORD version;
    DWORD vertexLength;
    DWORD fragmentLength;
} AdvProgramBinaryHeader;

typedef struct {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
} AdvDrawArraysIndirectCommand;

typedef struct {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLint  baseVertex;
    GLuint baseInstance;
} AdvDrawElementsIndirectCommand;

typedef struct {
    GLuint x;
    GLuint y;
    GLuint z;
} AdvDispatchIndirectCommand;

static AdvPipeline g_pipelines[ADV_MAX_PIPELINES];
static AdvTransformFeedback g_tfObjects[ADV_MAX_TF_OBJECTS];
static AdvLabel g_labels[ADV_MAX_LABELS];
static AdvProgramInfo g_programInfo[GLS_MAX_PROGRAMS];
static GLuint g_nextPipeline = 1;
static GLuint g_nextTransformFeedback = 1;
static GLuint g_boundTransformFeedback = 0;
static AdvDebugMessage g_debugMessages[ADV_MAX_DEBUG_MESSAGES];
static GLuint g_debugRead = 0;
static GLuint g_debugWrite = 0;
static GLuint g_debugCount = 0;
static BOOL g_debugEnabled = TRUE;
static GLuint g_debugGroupDepth = 0;

static void advSetError(GLenum error)
{
    GLS_State *s = glsGetState();
    if (s->lastError == GL_NO_ERROR)
        s->lastError = (GLenum_t)error;
}

static void advCopyString(char *dst, int dstSize, const char *src, int srcLength)
{
    int n;
    if (!dst || dstSize <= 0) return;
    dst[0] = '\0';
    if (!src) return;
    n = srcLength < 0 ? (int)strlen(src) : srcLength;
    if (n > dstSize - 1) n = dstSize - 1;
    if (n > 0) memcpy(dst, src, (size_t)n);
    dst[n] = '\0';
}

static void advReturnString(const char *src, GLsizei bufSize,
                            GLsizei *length, GLchar *dst)
{
    int n = src ? (int)strlen(src) : 0;
    if (length) *length = 0;
    if (!dst || bufSize <= 0) return;
    if (n > bufSize - 1) n = bufSize - 1;
    if (n > 0) memcpy(dst, src, (size_t)n);
    dst[n] = '\0';
    if (length) *length = n;
}

static GLS_Buffer *advBoundBuffer(GLenum target)
{
    GLS_State *s = glsGetState();
    GLuint id = 0;
    switch (target) {
    case GL_ARRAY_BUFFER:              id = s->boundArrayBuffer; break;
    case GL_ELEMENT_ARRAY_BUFFER:      id = s->boundElementBuffer; break;
    case GL_TEXTURE_BUFFER:            id = s->boundTextureBufferObject; break;
    case GL_PIXEL_PACK_BUFFER:         id = s->boundPixelPackBuffer; break;
    case GL_PIXEL_UNPACK_BUFFER:       id = s->boundPixelUnpackBuffer; break;
    case GL_COPY_READ_BUFFER:          id = s->boundCopyReadBuffer; break;
    case GL_COPY_WRITE_BUFFER:         id = s->boundCopyWriteBuffer; break;
    case GL_UNIFORM_BUFFER:            id = s->boundUniformBuffer; break;
    case GL_TRANSFORM_FEEDBACK_BUFFER: id = s->boundTransformFeedbackBuffer; break;
    case GL_SHADER_STORAGE_BUFFER:     id = s->boundShaderStorageBuffer; break;
    case GL_ATOMIC_COUNTER_BUFFER:     id = s->boundAtomicCounterBuffer; break;
    case GL_DRAW_INDIRECT_BUFFER:      id = s->boundDrawIndirectBuffer; break;
    case GL_DISPATCH_INDIRECT_BUFFER:  id = s->boundDispatchIndirectBuffer; break;
#ifdef GL_PARAMETER_BUFFER_ARB
    case GL_PARAMETER_BUFFER_ARB:      id = s->boundParameterBuffer; break;
#endif
    default: break;
    }
    return glsFindBuffer(id);
}

static const void *advIndirectPointer(GLuint buffer, const void *offset,
                                      size_t bytes)
{
    GLS_Buffer *b;
    size_t off;
    if (!buffer) return offset;
    b = glsFindBuffer(buffer);
    if (!b || !b->data) {
        advSetError(GL_INVALID_OPERATION);
        return NULL;
    }
    off = (size_t)(UINT_PTR)offset;
    if (off > (size_t)b->size || bytes > (size_t)b->size - off) {
        advSetError(GL_INVALID_OPERATION);
        return NULL;
    }
    return (const unsigned char *)b->data + off;
}

static size_t advTypeSize(GLenum type)
{
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT: return 2;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT: return 4;
    case GL_DOUBLE: return 8;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV: return 2;
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV: return 4;
    default: return 0;
    }
}

static int advFormatComponents(GLenum format)
{
    switch (format) {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
    case GL_STENCIL_INDEX: return 1;
    case GL_RG:
    case GL_LUMINANCE_ALPHA:
    case GL_DEPTH_STENCIL: return 2;
    case GL_RGB:
    case GL_BGR: return 3;
    case GL_RGBA:
    case GL_BGRA: return 4;
    default: return 1;
    }
}

static size_t advPixelSize(GLenum format, GLenum type)
{
    size_t ts = advTypeSize(type);
    switch (type) {
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
        return ts;
    default:
        return ts * (size_t)advFormatComponents(format);
    }
}

static double advReadComponent(const void *data, GLenum type, int index)
{
    switch (type) {
    case GL_BYTE:           return ((const signed char *)data)[index] / 127.0;
    case GL_UNSIGNED_BYTE:  return ((const unsigned char *)data)[index] / 255.0;
    case GL_SHORT:          return ((const short *)data)[index] / 32767.0;
    case GL_UNSIGNED_SHORT: return ((const unsigned short *)data)[index] / 65535.0;
    case GL_INT:            return ((const int *)data)[index] / 2147483647.0;
    case GL_UNSIGNED_INT:   return ((const unsigned int *)data)[index] / 4294967295.0;
    case GL_FLOAT:          return ((const float *)data)[index];
    case GL_DOUBLE:         return ((const double *)data)[index];
    default:                return 0.0;
    }
}

static unsigned char advByte(double v)
{
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return (unsigned char)(v * 255.0 + 0.5);
}

static void advPixelRGBA8(GLenum format, GLenum type, const void *data,
                          unsigned char rgba[4])
{
    double c[4] = { 0.0, 0.0, 0.0, 1.0 };
    int n;
    if (!data) {
        memset(rgba, 0, 4);
        return;
    }

    if (advPixelSize(format, type) == advTypeSize(type) &&
        advFormatComponents(format) > 1) {
        /* Packed formats are copied as a normalized 32-bit value.  The common
         * clear value is zero; non-zero packed clears retain all source bits
         * through buffer clears and are approximated here for texture upload. */
        unsigned int packed = 0;
        size_t sz = advTypeSize(type);
        memcpy(&packed, data, sz > sizeof(packed) ? sizeof(packed) : sz);
        c[0] = ((packed >> 0)  & 0xFFu) / 255.0;
        c[1] = ((packed >> 8)  & 0xFFu) / 255.0;
        c[2] = ((packed >> 16) & 0xFFu) / 255.0;
        c[3] = ((packed >> 24) & 0xFFu) / 255.0;
    } else {
        n = advFormatComponents(format);
        if (n > 4) n = 4;
        while (n-- > 0) c[n] = advReadComponent(data, type, n);
    }

    switch (format) {
    case GL_RED:       rgba[0] = advByte(c[0]); rgba[1] = 0; rgba[2] = 0; rgba[3] = 255; break;
    case GL_GREEN:     rgba[0] = 0; rgba[1] = advByte(c[0]); rgba[2] = 0; rgba[3] = 255; break;
    case GL_BLUE:      rgba[0] = 0; rgba[1] = 0; rgba[2] = advByte(c[0]); rgba[3] = 255; break;
    case GL_ALPHA:     rgba[0] = 0; rgba[1] = 0; rgba[2] = 0; rgba[3] = advByte(c[0]); break;
    case GL_RG:        rgba[0] = advByte(c[0]); rgba[1] = advByte(c[1]); rgba[2] = 0; rgba[3] = 255; break;
    case GL_RGB:       rgba[0] = advByte(c[0]); rgba[1] = advByte(c[1]); rgba[2] = advByte(c[2]); rgba[3] = 255; break;
    case GL_BGR:       rgba[0] = advByte(c[2]); rgba[1] = advByte(c[1]); rgba[2] = advByte(c[0]); rgba[3] = 255; break;
    case GL_BGRA:      rgba[0] = advByte(c[2]); rgba[1] = advByte(c[1]); rgba[2] = advByte(c[0]); rgba[3] = advByte(c[3]); break;
    case GL_LUMINANCE: rgba[0] = rgba[1] = rgba[2] = advByte(c[0]); rgba[3] = 255; break;
    case GL_LUMINANCE_ALPHA: rgba[0] = rgba[1] = rgba[2] = advByte(c[0]); rgba[3] = advByte(c[1]); break;
    default:           rgba[0] = advByte(c[0]); rgba[1] = advByte(c[1]); rgba[2] = advByte(c[2]); rgba[3] = advByte(c[3]); break;
    }
}

static GLS_VAO *advCurrentVAO(void)
{
    return glsFindVAO(glsGetState()->boundVAO);
}

static void advResolveVertexBinding(GLS_VAO *vao, GLuint attribindex)
{
    GLS_VertexAttrib *a;
    GLS_VertexBinding *b;
    if (!vao || attribindex >= GLS_MAX_VERTEX_ATTRIBS) return;
    a = &vao->attribs[attribindex];
    if (a->bindingIndex >= GLS_MAX_VERTEX_ATTRIBS) return;
    b = &vao->bindings[a->bindingIndex];
    a->bufferBinding = b->buffer;
    a->pointer = (const void *)(UINT_PTR)(b->offset + (GLintptr_t)a->relativeOffset);
    a->stride = b->stride;
    a->divisor = b->divisor;
}

static AdvPipeline *advPipeline(GLuint id)
{
    if (!id || id >= ADV_MAX_PIPELINES || !g_pipelines[id].allocated)
        return NULL;
    return &g_pipelines[id];
}

static void advApplyPipeline(AdvPipeline *p)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *dev = gldGetD3DDevice46();
    GLS_Program *vp = p ? glsFindProgram(p->vertexProgram) : NULL;
    GLS_Program *fp = p ? glsFindProgram(p->fragmentProgram) : NULL;

    s->pipelineActiveProgram = p ? p->activeProgram : 0;
    s->pipelineVertexProgram = p ? p->vertexProgram : 0;
    s->pipelineFragmentProgram = p ? p->fragmentProgram : 0;
    s->pipelineComputeProgram = p ? p->computeProgram : 0;

    if (!dev) return;
    __try {
        /* RTX Remix intercepts programmable D3D9 draws as well as fixed-
         * function draws.  Preserve the application's translated pipeline;
         * forcing NULL shaders here corrupts every separable-program draw. */
        IDirect3DDevice9_SetVertexShader(dev, vp && vp->linked ? vp->pVS : NULL);
        IDirect3DDevice9_SetPixelShader(dev, fp && fp->linked ? fp->pPS : NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

static AdvProgramInfo *advProgramInfo(GLuint program)
{
    if (!program || program >= GLS_MAX_PROGRAMS || !glsFindProgram(program))
        return NULL;
    return &g_programInfo[program];
}

void gldAdvReset(void)
{
    memset(g_pipelines, 0, sizeof(g_pipelines));
    memset(g_tfObjects, 0, sizeof(g_tfObjects));
    memset(g_labels, 0, sizeof(g_labels));
    memset(g_programInfo, 0, sizeof(g_programInfo));
    g_nextPipeline = 1;
    g_nextTransformFeedback = 1;
    g_boundTransformFeedback = 0;
    memset(g_debugMessages, 0, sizeof(g_debugMessages));
    g_debugRead = g_debugWrite = g_debugCount = 0;
    g_debugEnabled = TRUE;
    g_debugGroupDepth = 0;
}

/* ------------------------- Program pipelines ------------------------- */

void gldAdvGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
    GLsizei i;
    if (n < 0 || (!pipelines && n)) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; ++i) {
        GLuint id;
        for (id = g_nextPipeline; id < ADV_MAX_PIPELINES; ++id)
            if (!g_pipelines[id].allocated) break;
        if (id >= ADV_MAX_PIPELINES) { pipelines[i] = 0; advSetError(GL_OUT_OF_MEMORY); continue; }
        memset(&g_pipelines[id], 0, sizeof(g_pipelines[id]));
        g_pipelines[id].allocated = TRUE;
        pipelines[i] = id;
        g_nextPipeline = id + 1;
        if (g_nextPipeline >= ADV_MAX_PIPELINES) g_nextPipeline = 1;
    }
}

void gldAdvDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
    GLS_State *s = glsGetState();
    GLsizei i;
    if (n < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!pipelines) return;
    for (i = 0; i < n; ++i) {
        GLuint id = pipelines[i];
        if (id && id < ADV_MAX_PIPELINES) {
            memset(&g_pipelines[id], 0, sizeof(g_pipelines[id]));
            if (s->boundProgramPipeline == id) {
                s->boundProgramPipeline = 0;
                advApplyPipeline(NULL);
            }
        }
    }
}

GLboolean gldAdvIsProgramPipeline(GLuint pipeline)
{
    return advPipeline(pipeline) ? GL_TRUE : GL_FALSE;
}

void gldAdvBindProgramPipeline(GLuint pipeline)
{
    GLS_State *s = glsGetState();
    AdvPipeline *p = NULL;
    if (pipeline) {
        if (pipeline >= ADV_MAX_PIPELINES) { advSetError(GL_INVALID_VALUE); return; }
        if (!g_pipelines[pipeline].allocated) {
            memset(&g_pipelines[pipeline], 0, sizeof(g_pipelines[pipeline]));
            g_pipelines[pipeline].allocated = TRUE;
        }
        p = &g_pipelines[pipeline];
    }
    s->boundProgramPipeline = pipeline;
    if (s->boundProgram == 0) advApplyPipeline(p);
}

void gldAdvUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    AdvPipeline *p = advPipeline(pipeline);
    GLS_Program *prog = program ? glsFindProgram(program) : NULL;
    if (!p) { advSetError(GL_INVALID_OPERATION); return; }
    if (program && (!prog || !prog->linked)) { advSetError(GL_INVALID_OPERATION); return; }
    if (stages & GL_VERTEX_SHADER_BIT) p->vertexProgram = program;
    if (stages & GL_FRAGMENT_SHADER_BIT) p->fragmentProgram = program;
    if (stages & GL_GEOMETRY_SHADER_BIT) p->geometryProgram = program;
    if (stages & GL_TESS_CONTROL_SHADER_BIT) p->tessControlProgram = program;
    if (stages & GL_TESS_EVALUATION_SHADER_BIT) p->tessEvalProgram = program;
    if (stages & GL_COMPUTE_SHADER_BIT) p->computeProgram = program;
    p->validated = FALSE;
    if (glsGetState()->boundProgramPipeline == pipeline &&
        glsGetState()->boundProgram == 0)
        advApplyPipeline(p);
}

void gldAdvActiveShaderProgram(GLuint pipeline, GLuint program)
{
    AdvPipeline *p = advPipeline(pipeline);
    if (!p) { advSetError(GL_INVALID_OPERATION); return; }
    if (program && !glsFindProgram(program)) { advSetError(GL_INVALID_VALUE); return; }
    p->activeProgram = program;
    if (glsGetState()->boundProgramPipeline == pipeline)
        glsGetState()->pipelineActiveProgram = program;
}

void gldAdvValidateProgramPipeline(GLuint pipeline)
{
    AdvPipeline *p = advPipeline(pipeline);
    GLS_Program *vp;
    GLS_Program *fp;
    if (!p) { advSetError(GL_INVALID_OPERATION); return; }
    vp = p->vertexProgram ? glsFindProgram(p->vertexProgram) : NULL;
    fp = p->fragmentProgram ? glsFindProgram(p->fragmentProgram) : NULL;
    p->infoLog[0] = '\0';
    p->validated = TRUE;
    if (p->vertexProgram && (!vp || !vp->linked)) {
        p->validated = FALSE;
        strcpy(p->infoLog, "vertex-stage program is not linked");
    } else if (p->fragmentProgram && (!fp || !fp->linked)) {
        p->validated = FALSE;
        strcpy(p->infoLog, "fragment-stage program is not linked");
    }
}

void gldAdvGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    AdvPipeline *p = advPipeline(pipeline);
    if (!params) return;
    *params = 0;
    if (!p) { advSetError(GL_INVALID_OPERATION); return; }
    switch (pname) {
    case GL_ACTIVE_PROGRAM: *params = (GLint)p->activeProgram; break;
    case GL_VERTEX_SHADER: *params = (GLint)p->vertexProgram; break;
    case GL_FRAGMENT_SHADER: *params = (GLint)p->fragmentProgram; break;
    case GL_GEOMETRY_SHADER: *params = (GLint)p->geometryProgram; break;
    case GL_TESS_CONTROL_SHADER: *params = (GLint)p->tessControlProgram; break;
    case GL_TESS_EVALUATION_SHADER: *params = (GLint)p->tessEvalProgram; break;
    case GL_COMPUTE_SHADER: *params = (GLint)p->computeProgram; break;
    case GL_VALIDATE_STATUS: *params = p->validated ? GL_TRUE : GL_FALSE; break;
    case GL_INFO_LOG_LENGTH: *params = (GLint)strlen(p->infoLog) + 1; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

void gldAdvGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize,
                                     GLsizei *length, GLchar *infoLog)
{
    AdvPipeline *p = advPipeline(pipeline);
    if (!p) { advSetError(GL_INVALID_OPERATION); advReturnString("", bufSize, length, infoLog); return; }
    advReturnString(p->infoLog, bufSize, length, infoLog);
}

/* ---------------------- Fragment output bindings --------------------- */

void gldAdvBindFragDataLocationIndexed(GLuint program, GLuint colorNumber,
                                       GLuint index, const GLchar *name)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    GLint i;
    if (!pi || !name) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < pi->fragCount; ++i)
        if (strcmp(pi->frag[i].name, name) == 0) break;
    if (i == pi->fragCount) {
        if (i >= ADV_MAX_FRAG_BINDINGS) { advSetError(GL_INVALID_VALUE); return; }
        ++pi->fragCount;
    }
    pi->frag[i].used = TRUE;
    pi->frag[i].color = colorNumber;
    pi->frag[i].index = index;
    advCopyString(pi->frag[i].name, sizeof(pi->frag[i].name), name, -1);
}

void gldAdvBindFragDataLocation(GLuint program, GLuint color, const GLchar *name)
{
    gldAdvBindFragDataLocationIndexed(program, color, 0, name);
}

GLint gldAdvGetFragDataLocation(GLuint program, const GLchar *name)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    GLint i;
    if (!pi || !name) return -1;
    for (i = 0; i < pi->fragCount; ++i)
        if (pi->frag[i].used && strcmp(pi->frag[i].name, name) == 0)
            return (GLint)pi->frag[i].color;
    if (strcmp(name, "gl_FragColor") == 0 || strcmp(name, "gl_FragData[0]") == 0)
        return 0;
    return -1;
}

GLint gldAdvGetFragDataIndex(GLuint program, const GLchar *name)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    GLint i;
    if (!pi || !name) return -1;
    for (i = 0; i < pi->fragCount; ++i)
        if (pi->frag[i].used && strcmp(pi->frag[i].name, name) == 0)
            return (GLint)pi->frag[i].index;
    return gldAdvGetFragDataLocation(program, name) >= 0 ? 0 : -1;
}

/* ------------------------- Vertex bindings --------------------------- */

void gldAdvBindVertexBuffer(GLuint bindingindex, GLuint buffer,
                            GLintptr offset, GLsizei stride)
{
    GLS_VAO *vao = advCurrentVAO();
    GLuint i;
    if (!vao || bindingindex >= GLS_MAX_VERTEX_ATTRIBS || offset < 0 || stride < 0) {
        advSetError(GL_INVALID_VALUE); return;
    }
    if (buffer && !glsFindBuffer(buffer)) { advSetError(GL_INVALID_OPERATION); return; }
    vao->bindings[bindingindex].buffer = buffer;
    vao->bindings[bindingindex].offset = (GLintptr_t)offset;
    vao->bindings[bindingindex].stride = stride;
    for (i = 0; i < GLS_MAX_VERTEX_ATTRIBS; ++i)
        if (vao->attribs[i].bindingIndex == bindingindex)
            advResolveVertexBinding(vao, i);
}

void gldAdvBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                             const GLintptr *offsets, const GLsizei *strides)
{
    GLsizei i;
    if (count < 0 || first + (GLuint)count > GLS_MAX_VERTEX_ATTRIBS) {
        advSetError(GL_INVALID_VALUE); return;
    }
    for (i = 0; i < count; ++i)
        gldAdvBindVertexBuffer(first + (GLuint)i, buffers ? buffers[i] : 0,
                               offsets ? offsets[i] : 0, strides ? strides[i] : 16);
}

void gldAdvVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
    GLS_VAO *vao = advCurrentVAO();
    if (!vao || attribindex >= GLS_MAX_VERTEX_ATTRIBS ||
        bindingindex >= GLS_MAX_VERTEX_ATTRIBS) {
        advSetError(GL_INVALID_VALUE); return;
    }
    vao->attribs[attribindex].bindingIndex = bindingindex;
    advResolveVertexBinding(vao, attribindex);
}

void gldAdvVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                              GLboolean normalized, GLuint relativeoffset)
{
    GLS_VAO *vao = advCurrentVAO();
    GLS_VertexAttrib *a;
    if (!vao || attribindex >= GLS_MAX_VERTEX_ATTRIBS || size < 1 || size > 4) {
        advSetError(GL_INVALID_VALUE); return;
    }
    a = &vao->attribs[attribindex];
    a->size = size;
    a->type = type;
    a->normalized = normalized;
    a->integer = FALSE;
    a->relativeOffset = relativeoffset;
    advResolveVertexBinding(vao, attribindex);
}

void gldAdvVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                               GLuint relativeoffset)
{
    GLS_VAO *vao;
    gldAdvVertexAttribFormat(attribindex, size, type, GL_FALSE, relativeoffset);
    vao = advCurrentVAO();
    if (vao && attribindex < GLS_MAX_VERTEX_ATTRIBS)
        vao->attribs[attribindex].integer = TRUE;
}

void gldAdvVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                               GLuint relativeoffset)
{
    if (type != GL_DOUBLE) { advSetError(GL_INVALID_ENUM); return; }
    gldAdvVertexAttribFormat(attribindex, size, type, GL_FALSE, relativeoffset);
}

void gldAdvVertexAttribLPointer(GLuint index, GLint size, GLenum type,
                                GLsizei stride, const void *pointer)
{
    if (type != GL_DOUBLE) { advSetError(GL_INVALID_ENUM); return; }
    _glsVertexAttribPointer(index, size, type, GL_FALSE, stride, pointer);
}

void gldAdvVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
    GLS_VAO *vao = advCurrentVAO();
    GLuint i;
    if (!vao || bindingindex >= GLS_MAX_VERTEX_ATTRIBS) { advSetError(GL_INVALID_VALUE); return; }
    vao->bindings[bindingindex].divisor = divisor;
    for (i = 0; i < GLS_MAX_VERTEX_ATTRIBS; ++i)
        if (vao->attribs[i].bindingIndex == bindingindex)
            vao->attribs[i].divisor = divisor;
}

/* ----------------------- Buffer/resource storage --------------------- */

void gldAdvBufferStorage(GLenum target, GLsizeiptr size, const void *data,
                         GLbitfield flags)
{
    GLS_Buffer *b = advBoundBuffer(target);
    if (!b || size < 0) { advSetError(b ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return; }
    if (b->immutable) { advSetError(GL_INVALID_OPERATION); return; }
    _glsBufferData(target, (ptrdiff_t)size, data, GL_STATIC_DRAW);
    b = advBoundBuffer(target);
    if (b) { b->immutable = TRUE; b->storageFlags = flags; }
}

void gldAdvClearBufferSubData(GLenum target, GLenum internalformat,
                              GLintptr offset, GLsizeiptr size, GLenum format,
                              GLenum type, const void *data)
{
    GLS_Buffer *b = advBoundBuffer(target);
    size_t pixelSize = advPixelSize(format, type);
    size_t pos;
    (void)internalformat;
    if (!b || !b->data) { advSetError(GL_INVALID_OPERATION); return; }
    if (offset < 0 || size < 0 || (GLsizeiptr)offset > b->size ||
        size > b->size - (GLsizeiptr)offset || !pixelSize ||
        ((size_t)offset % pixelSize) || ((size_t)size % pixelSize)) {
        advSetError(GL_INVALID_VALUE); return;
    }
    if (!data) {
        memset((unsigned char *)b->data + offset, 0, (size_t)size);
        return;
    }
    for (pos = 0; pos < (size_t)size; pos += pixelSize)
        memcpy((unsigned char *)b->data + offset + pos, data, pixelSize);
}

void gldAdvClearBufferData(GLenum target, GLenum internalformat, GLenum format,
                           GLenum type, const void *data)
{
    GLS_Buffer *b = advBoundBuffer(target);
    if (!b) { advSetError(GL_INVALID_OPERATION); return; }
    gldAdvClearBufferSubData(target, internalformat, 0, b->size, format, type, data);
}

static GLuint advBoundTextureForTarget(GLS_State *s, GLenum target, int unit)
{
    if (target == GL_TEXTURE_3D) return s->boundTexture3D[unit];
    if (target == GL_TEXTURE_CUBE_MAP) return s->boundTextureCube[unit];
    return s->boundTexture2D[unit];
}

static void advClearTextureRegion(GLuint texture, GLint level, GLint xoffset,
                                  GLint yoffset, GLint zoffset, GLsizei width,
                                  GLsizei height, GLsizei depth, GLenum format,
                                  GLenum type, const void *data)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex = glsFindTexture(texture);
    GLenum target;
    GLenum bindTarget;
    GLuint old;
    int unit;
    size_t pixels;
    unsigned char rgba[4];
    unsigned char *image;
    size_t i;
    int face;

    if (!tex || level < 0 || width < 0 || height < 0 || depth < 0) {
        advSetError(GL_INVALID_VALUE); return;
    }
    target = (GLenum)tex->target;
    bindTarget = target;
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
        target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        bindTarget = GL_TEXTURE_CUBE_MAP;
    unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    old = advBoundTextureForTarget(s, bindTarget, unit);

    if ((size_t)width > SIZE_MAX / (size_t)(height ? height : 1) ||
        (size_t)width * (size_t)height > SIZE_MAX / (size_t)(depth ? depth : 1) ||
        (size_t)width * (size_t)height * (size_t)depth > SIZE_MAX / 4) {
        advSetError(GL_OUT_OF_MEMORY); return;
    }
    pixels = (size_t)width * (size_t)height * (size_t)depth;
    image = (unsigned char *)malloc(pixels * 4);
    if (!image && pixels) { advSetError(GL_OUT_OF_MEMORY); return; }
    advPixelRGBA8(format, type, data, rgba);
    for (i = 0; i < pixels; ++i) memcpy(image + i * 4, rgba, 4);

    _glsBindTexture(bindTarget, texture);
    if (bindTarget == GL_TEXTURE_3D) {
        _glsTexSubImage3D(bindTarget, level, xoffset, yoffset, zoffset,
                          width, height, depth, GL_RGBA, GL_UNSIGNED_BYTE, image);
    } else if (bindTarget == GL_TEXTURE_CUBE_MAP && target == GL_TEXTURE_CUBE_MAP) {
        for (face = 0; face < 6; ++face)
            _glsTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level,
                              xoffset, yoffset, width, height,
                              GL_RGBA, GL_UNSIGNED_BYTE, image);
    } else {
        _glsTexSubImage2D(target, level, xoffset, yoffset, width, height,
                          GL_RGBA, GL_UNSIGNED_BYTE, image);
    }
    _glsBindTexture(bindTarget, old);
    free(image);
}

void gldAdvClearTexSubImage(GLuint texture, GLint level, GLint xoffset,
                            GLint yoffset, GLint zoffset, GLsizei width,
                            GLsizei height, GLsizei depth, GLenum format,
                            GLenum type, const void *data)
{
    advClearTextureRegion(texture, level, xoffset, yoffset, zoffset,
                          width, height, depth, format, type, data);
}

void gldAdvClearTexImage(GLuint texture, GLint level, GLenum format,
                         GLenum type, const void *data)
{
    GLS_Texture *tex = glsFindTexture(texture);
    GLsizei w, h, d;
    if (!tex) { advSetError(GL_INVALID_VALUE); return; }
    w = tex->width >> level; if (w < 1) w = 1;
    h = tex->height >> level; if (h < 1) h = 1;
    d = tex->depth >> level; if (d < 1) d = 1;
    advClearTextureRegion(texture, level, 0, 0, 0, w, h, d, format, type, data);
}

void gldAdvCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                            GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                            GLenum dstTarget, GLint dstLevel, GLint dstX,
                            GLint dstY, GLint dstZ, GLsizei srcWidth,
                            GLsizei srcHeight, GLsizei srcDepth)
{
    GLS_State *s = glsGetState();
    GLS_Texture *src = glsFindTexture(srcName);
    GLS_Texture *dst = glsFindTexture(dstName);
    int unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    GLuint oldSrc, oldDst;
    unsigned char *whole = NULL;
    unsigned char *region = NULL;
    size_t wholePixels, regionPixels;
    int y;
    (void)srcZ; (void)dstZ;
    if (!src || !dst || srcDepth != 1 || srcWidth < 0 || srcHeight < 0) {
        advSetError(GL_INVALID_VALUE); return;
    }
    if (srcTarget == GL_TEXTURE_3D || dstTarget == GL_TEXTURE_3D) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    if (srcX < 0 || srcY < 0 || srcX + srcWidth > src->width ||
        srcY + srcHeight > src->height) { advSetError(GL_INVALID_VALUE); return; }
    wholePixels = (size_t)src->width * (size_t)src->height;
    regionPixels = (size_t)srcWidth * (size_t)srcHeight;
    if (wholePixels > SIZE_MAX / 4 || regionPixels > SIZE_MAX / 4) {
        advSetError(GL_OUT_OF_MEMORY); return;
    }
    whole = (unsigned char *)malloc(wholePixels * 4);
    region = (unsigned char *)malloc(regionPixels * 4);
    if ((!whole && wholePixels) || (!region && regionPixels)) {
        free(whole); free(region); advSetError(GL_OUT_OF_MEMORY); return;
    }
    oldSrc = advBoundTextureForTarget(s, srcTarget, unit);
    _glsBindTexture(srcTarget, srcName);
    _glsGetTexImage(srcTarget, srcLevel, GL_RGBA, GL_UNSIGNED_BYTE, whole);
    for (y = 0; y < srcHeight; ++y)
        memcpy(region + (size_t)y * srcWidth * 4,
               whole + ((size_t)(srcY + y) * src->width + srcX) * 4,
               (size_t)srcWidth * 4);
    _glsBindTexture(srcTarget, oldSrc);

    oldDst = advBoundTextureForTarget(s, dstTarget, unit);
    _glsBindTexture(dstTarget, dstName);
    _glsTexSubImage2D(dstTarget, dstLevel, dstX, dstY, srcWidth, srcHeight,
                      GL_RGBA, GL_UNSIGNED_BYTE, region);
    _glsBindTexture(dstTarget, oldDst);
    free(region);
    free(whole);
}

void gldAdvGetTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                              GLint yoffset, GLint zoffset, GLsizei width,
                              GLsizei height, GLsizei depth, GLenum format,
                              GLenum type, GLsizei bufSize, void *pixels)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex = glsFindTexture(texture);
    GLenum target;
    int unit;
    GLuint old;
    int fullWidth, fullHeight;
    size_t bpp, fullBytes, outBytes;
    unsigned char *full;
    int y;
    if (!tex || !pixels || level < 0 || xoffset < 0 || yoffset < 0 ||
        zoffset != 0 || depth != 1 || width < 0 || height < 0) {
        advSetError(GL_INVALID_VALUE); return;
    }
    target = (GLenum)tex->target;
    if (target == GL_TEXTURE_3D) { advSetError(GL_INVALID_OPERATION); return; }
    fullWidth = tex->width >> level; if (fullWidth < 1) fullWidth = 1;
    fullHeight = tex->height >> level; if (fullHeight < 1) fullHeight = 1;
    if (xoffset + width > fullWidth || yoffset + height > fullHeight) {
        advSetError(GL_INVALID_VALUE); return;
    }
    bpp = advPixelSize(format, type);
    if (!bpp || (size_t)fullWidth > SIZE_MAX / (size_t)fullHeight ||
        (size_t)fullWidth * (size_t)fullHeight > SIZE_MAX / bpp) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    fullBytes = (size_t)fullWidth * (size_t)fullHeight * bpp;
    outBytes = (size_t)width * (size_t)height * bpp;
    if (bufSize < 0 || (size_t)bufSize < outBytes) { advSetError(GL_INVALID_OPERATION); return; }
    full = (unsigned char *)malloc(fullBytes);
    if (!full && fullBytes) { advSetError(GL_OUT_OF_MEMORY); return; }
    unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    old = advBoundTextureForTarget(s, target, unit);
    _glsBindTexture(target, texture);
    _glsGetTexImage(target, level, format, type, full);
    _glsBindTexture(target, old);
    for (y = 0; y < height; ++y)
        memcpy((unsigned char *)pixels + (size_t)y * width * bpp,
               full + ((size_t)(yoffset + y) * fullWidth + xoffset) * bpp,
               (size_t)width * bpp);
    free(full);
}

void gldAdvGetCompressedTextureSubImage(GLuint texture, GLint level,
                                        GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLsizei width,
                                        GLsizei height, GLsizei depth,
                                        GLsizei bufSize, void *pixels)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex = glsFindTexture(texture);
    GLenum target;
    int unit, fullWidth, fullHeight, fullBlocksX, blockSize, bx, by, bw, bh, row;
    GLuint old;
    size_t fullBytes, outBytes;
    unsigned char *full;
    if (!tex || !pixels || level < 0 || zoffset != 0 || depth != 1) {
        advSetError(GL_INVALID_VALUE); return;
    }
    switch (tex->internalFormat) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: blockSize = 8; break;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: blockSize = 16; break;
    default: advSetError(GL_INVALID_OPERATION); return;
    }
    fullWidth = tex->width >> level; if (fullWidth < 1) fullWidth = 1;
    fullHeight = tex->height >> level; if (fullHeight < 1) fullHeight = 1;
    if (xoffset < 0 || yoffset < 0 || width < 0 || height < 0 ||
        xoffset + width > fullWidth || yoffset + height > fullHeight ||
        (xoffset & 3) || (yoffset & 3)) { advSetError(GL_INVALID_VALUE); return; }
    fullBlocksX = (fullWidth + 3) / 4;
    bx = xoffset / 4; by = yoffset / 4;
    bw = (width + 3) / 4; bh = (height + 3) / 4;
    fullBytes = (size_t)fullBlocksX * (size_t)((fullHeight + 3) / 4) * blockSize;
    outBytes = (size_t)bw * (size_t)bh * blockSize;
    if (bufSize < 0 || (size_t)bufSize < outBytes) { advSetError(GL_INVALID_OPERATION); return; }
    full = (unsigned char *)malloc(fullBytes);
    if (!full && fullBytes) { advSetError(GL_OUT_OF_MEMORY); return; }
    target = (GLenum)tex->target;
    unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    old = advBoundTextureForTarget(s, target, unit);
    _glsBindTexture(target, texture);
    _glsGetCompressedTexImage(target, level, full);
    _glsBindTexture(target, old);
    for (row = 0; row < bh; ++row)
        memcpy((unsigned char *)pixels + (size_t)row * bw * blockSize,
               full + ((size_t)(by + row) * fullBlocksX + bx) * blockSize,
               (size_t)bw * blockSize);
    free(full);
}

void gldAdvTextureView(GLuint texture, GLenum target, GLuint origtexture,
                       GLenum internalformat, GLuint minlevel,
                       GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
    GLS_State *s = glsGetState();
    GLS_Texture *orig = glsFindTexture(origtexture);
    GLS_Texture *view;
    int unit, width, height;
    GLuint oldOrig, oldView, level;
    unsigned char *pixels;
    size_t bytes;
    if (!orig || !texture || texture >= GLS_MAX_TEXTURES || !numlevels ||
        minlayer != 0 || numlayers != 1 ||
        (target != GL_TEXTURE_2D && target != GL_TEXTURE_1D) ||
        (orig->target != GL_TEXTURE_2D && orig->target != GL_TEXTURE_1D)) {
        advSetError(GL_INVALID_VALUE); return;
    }
    unit = (int)(s->activeTexUnit - GL_TEXTURE0);
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    oldView = advBoundTextureForTarget(s, target, unit);
    _glsBindTexture(target, texture);
    view = glsFindTexture(texture);
    if (!view) { advSetError(GL_OUT_OF_MEMORY); return; }
    width = orig->width >> minlevel; if (width < 1) width = 1;
    height = orig->height >> minlevel; if (height < 1) height = 1;
    _glsTexStorage2D(target, (int)numlevels, internalformat, width, height);
    oldOrig = advBoundTextureForTarget(s, (GLenum)orig->target, unit);
    for (level = 0; level < numlevels; ++level) {
        int w = width >> level; int h = height >> level;
        if (w < 1) w = 1; if (h < 1) h = 1;
        bytes = (size_t)w * (size_t)h * 4;
        pixels = (unsigned char *)malloc(bytes);
        if (!pixels) { advSetError(GL_OUT_OF_MEMORY); break; }
        _glsBindTexture((GLenum)orig->target, origtexture);
        _glsGetTexImage((GLenum)orig->target, (int)(minlevel + level),
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        _glsBindTexture(target, texture);
        _glsTexSubImage2D(target, (int)level, 0, 0, w, h,
                          GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        free(pixels);
    }
    _glsBindTexture((GLenum)orig->target, oldOrig);
    _glsBindTexture(target, oldView);
}

/* ---------------------- Draw command expansion ----------------------- */

void gldAdvDrawArraysInstancedBaseInstance(GLenum mode, GLint first,
                                            GLsizei count,
                                            GLsizei instancecount,
                                            GLuint baseinstance)
{
    if (instancecount < 0) { advSetError(GL_INVALID_VALUE); return; }
    _glsDrawArraysInstancedBaseInstance(mode, first, count,
                                        instancecount, baseinstance);
}

void gldAdvDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                  const void *indices, GLint basevertex)
{
    _glsDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void gldAdvDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count,
                                              GLenum type, const void *indices,
                                              GLsizei instancecount,
                                              GLuint baseinstance)
{
    if (instancecount < 0) { advSetError(GL_INVALID_VALUE); return; }
    _glsDrawElementsInstancedBaseVertexBaseInstance(
        mode, count, type, indices, instancecount, 0, baseinstance);
}

void gldAdvDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count,
                                            GLenum type, const void *indices,
                                            GLsizei instancecount,
                                            GLint basevertex)
{
    if (instancecount < 0) { advSetError(GL_INVALID_VALUE); return; }
    _glsDrawElementsInstancedBaseVertexBaseInstance(
        mode, count, type, indices, instancecount, basevertex, 0);
}

void gldAdvDrawElementsInstancedBaseVertexBaseInstance(
    GLenum mode, GLsizei count, GLenum type, const void *indices,
    GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
    if (instancecount < 0) { advSetError(GL_INVALID_VALUE); return; }
    _glsDrawElementsInstancedBaseVertexBaseInstance(
        mode, count, type, indices, instancecount, basevertex, baseinstance);
}

void gldAdvDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
                                       GLsizei count, GLenum type,
                                       const void *indices, GLint basevertex)
{
    (void)start; (void)end;
    _glsDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void gldAdvMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count,
                                       GLenum type, const void *const*indices,
                                       GLsizei drawcount,
                                       const GLint *basevertex)
{
    GLsizei i;
    if (drawcount < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!count || !indices) return;
    for (i = 0; i < drawcount; ++i)
        _glsDrawElementsBaseVertex(mode, count[i], type, indices[i],
                                   basevertex ? basevertex[i] : 0);
}

void gldAdvDrawArraysIndirect(GLenum mode, const void *indirect)
{
    GLS_State *s = glsGetState();
    const AdvDrawArraysIndirectCommand *cmd =
        (const AdvDrawArraysIndirectCommand *)advIndirectPointer(
            s->boundDrawIndirectBuffer, indirect, sizeof(*cmd));
    if (!cmd) return;
    gldAdvDrawArraysInstancedBaseInstance(mode, (GLint)cmd->first,
                                          (GLsizei)cmd->count,
                                          (GLsizei)cmd->instanceCount,
                                          cmd->baseInstance);
}

void gldAdvDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    GLS_State *s = glsGetState();
    const AdvDrawElementsIndirectCommand *cmd =
        (const AdvDrawElementsIndirectCommand *)advIndirectPointer(
            s->boundDrawIndirectBuffer, indirect, sizeof(*cmd));
    size_t indexSize = advTypeSize(type);
    const void *indices;
    if (!cmd || !indexSize) { if (!indexSize) advSetError(GL_INVALID_ENUM); return; }
    indices = (const void *)(UINT_PTR)((size_t)cmd->firstIndex * indexSize);
    gldAdvDrawElementsInstancedBaseVertexBaseInstance(
        mode, (GLsizei)cmd->count, type, indices,
        (GLsizei)cmd->instanceCount, cmd->baseVertex, cmd->baseInstance);
}

void gldAdvMultiDrawArraysIndirect(GLenum mode, const void *indirect,
                                   GLsizei drawcount, GLsizei stride)
{
    GLS_State *s = glsGetState();
    GLsizei i;
    size_t step = stride ? (size_t)stride : sizeof(AdvDrawArraysIndirectCommand);
    if (drawcount < 0 || stride < 0) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; ++i) {
        const void *p = (const void *)((const unsigned char *)indirect + (size_t)i * step);
        if (s->boundDrawIndirectBuffer)
            p = (const void *)((UINT_PTR)indirect + (size_t)i * step);
        gldAdvDrawArraysIndirect(mode, p);
    }
}

void gldAdvMultiDrawElementsIndirect(GLenum mode, GLenum type,
                                     const void *indirect, GLsizei drawcount,
                                     GLsizei stride)
{
    GLS_State *s = glsGetState();
    GLsizei i;
    size_t step = stride ? (size_t)stride : sizeof(AdvDrawElementsIndirectCommand);
    if (drawcount < 0 || stride < 0) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; ++i) {
        const void *p = (const void *)((const unsigned char *)indirect + (size_t)i * step);
        if (s->boundDrawIndirectBuffer)
            p = (const void *)((UINT_PTR)indirect + (size_t)i * step);
        gldAdvDrawElementsIndirect(mode, type, p);
    }
}

static GLsizei advIndirectDrawCount(GLintptr offset, GLsizei maximum)
{
    GLS_State *s = glsGetState();
    const GLuint *p = (const GLuint *)advIndirectPointer(
        s->boundParameterBuffer, (const void *)(UINT_PTR)offset, sizeof(GLuint));
    GLuint n;
    if (!p) return 0;
    n = *p;
    if (n > (GLuint)maximum) n = (GLuint)maximum;
    return (GLsizei)n;
}

void gldAdvMultiDrawArraysIndirectCount(GLenum mode, const void *indirect,
                                        GLintptr drawcount, GLsizei maxdrawcount,
                                        GLsizei stride)
{
    gldAdvMultiDrawArraysIndirect(mode, indirect,
                                  advIndirectDrawCount(drawcount, maxdrawcount), stride);
}

void gldAdvMultiDrawElementsIndirectCount(GLenum mode, GLenum type,
                                          const void *indirect,
                                          GLintptr drawcount,
                                          GLsizei maxdrawcount, GLsizei stride)
{
    gldAdvMultiDrawElementsIndirect(mode, type, indirect,
                                    advIndirectDrawCount(drawcount, maxdrawcount), stride);
}

void gldAdvDispatchComputeIndirect(GLintptr indirect)
{
    GLS_State *s = glsGetState();
    const AdvDispatchIndirectCommand *cmd =
        (const AdvDispatchIndirectCommand *)advIndirectPointer(
            s->boundDispatchIndirectBuffer, (const void *)(UINT_PTR)indirect,
            sizeof(*cmd));
    if (cmd) _glsDispatchCompute(cmd->x, cmd->y, cmd->z);
}

/* ----------------------- Transform feedback -------------------------- */

void gldAdvGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
    GLsizei i;
    if (n < 0 || (!ids && n)) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; ++i) {
        GLuint id;
        for (id = g_nextTransformFeedback; id < ADV_MAX_TF_OBJECTS; ++id)
            if (!g_tfObjects[id].allocated) break;
        if (id >= ADV_MAX_TF_OBJECTS) { ids[i] = 0; advSetError(GL_OUT_OF_MEMORY); continue; }
        memset(&g_tfObjects[id], 0, sizeof(g_tfObjects[id]));
        g_tfObjects[id].allocated = TRUE;
        ids[i] = id;
        g_nextTransformFeedback = id + 1;
        if (g_nextTransformFeedback >= ADV_MAX_TF_OBJECTS) g_nextTransformFeedback = 1;
    }
}

void gldAdvDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
    GLsizei i;
    if (n < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!ids) return;
    for (i = 0; i < n; ++i) {
        if (ids[i] && ids[i] < ADV_MAX_TF_OBJECTS) {
            memset(&g_tfObjects[ids[i]], 0, sizeof(g_tfObjects[ids[i]]));
            if (g_boundTransformFeedback == ids[i]) g_boundTransformFeedback = 0;
        }
    }
}

GLboolean gldAdvIsTransformFeedback(GLuint id)
{
    return (id && id < ADV_MAX_TF_OBJECTS && g_tfObjects[id].allocated)
         ? GL_TRUE : GL_FALSE;
}

void gldAdvBindTransformFeedback(GLenum target, GLuint id)
{
    if (target != GL_TRANSFORM_FEEDBACK) { advSetError(GL_INVALID_ENUM); return; }
    if (id >= ADV_MAX_TF_OBJECTS) { advSetError(GL_INVALID_VALUE); return; }
    if (id && !g_tfObjects[id].allocated) {
        memset(&g_tfObjects[id], 0, sizeof(g_tfObjects[id]));
        g_tfObjects[id].allocated = TRUE;
    }
    if (glsGetState()->transformFeedbackActive && g_boundTransformFeedback != id) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    g_boundTransformFeedback = id;
}

void gldAdvPauseTransformFeedback(void)
{
    if (!glsGetState()->transformFeedbackActive || !g_boundTransformFeedback) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    g_tfObjects[g_boundTransformFeedback].paused = TRUE;
}

void gldAdvBeginTransformFeedback(GLenum mode)
{
    AdvTransformFeedback *tf;
    if (!g_boundTransformFeedback) return;
    tf = &g_tfObjects[g_boundTransformFeedback];
    if (!tf->allocated) return;
    tf->paused = FALSE;
    tf->mode = mode;
    tf->first = 0;
    tf->count = 0;
}

void gldAdvEndTransformFeedback(void)
{
    if (g_boundTransformFeedback &&
        g_boundTransformFeedback < ADV_MAX_TF_OBJECTS)
        g_tfObjects[g_boundTransformFeedback].paused = FALSE;
}

void gldAdvResumeTransformFeedback(void)
{
    if (!glsGetState()->transformFeedbackActive || !g_boundTransformFeedback) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    g_tfObjects[g_boundTransformFeedback].paused = FALSE;
}

void gldAdvRecordTransformFeedbackDraw(GLenum mode, GLint first, GLsizei count)
{
    AdvTransformFeedback *tf;
    if (!glsGetState()->transformFeedbackActive || !g_boundTransformFeedback)
        return;
    tf = &g_tfObjects[g_boundTransformFeedback];
    if (!tf->allocated || tf->paused) return;
    tf->mode = mode;
    tf->first = first;
    tf->count += count;
}

void gldAdvDrawTransformFeedback(GLenum mode, GLuint id)
{
    AdvTransformFeedback *tf;
    if (!id || id >= ADV_MAX_TF_OBJECTS || !g_tfObjects[id].allocated) {
        advSetError(GL_INVALID_VALUE); return;
    }
    tf = &g_tfObjects[id];
    _glsDrawArrays(mode, tf->first, tf->count);
}

void gldAdvDrawTransformFeedbackInstanced(GLenum mode, GLuint id,
                                           GLsizei instancecount)
{
    GLsizei i;
    if (instancecount < 0) { advSetError(GL_INVALID_VALUE); return; }
    for (i = 0; i < instancecount; ++i) gldAdvDrawTransformFeedback(mode, id);
}

void gldAdvDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
    if (stream != 0) { advSetError(GL_INVALID_VALUE); return; }
    gldAdvDrawTransformFeedback(mode, id);
}

void gldAdvDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id,
                                                 GLuint stream,
                                                 GLsizei instancecount)
{
    if (stream != 0) { advSetError(GL_INVALID_VALUE); return; }
    gldAdvDrawTransformFeedbackInstanced(mode, id, instancecount);
}

void gldAdvTransformFeedbackVaryings(GLuint program, GLsizei count,
                                     const GLchar *const*varyings,
                                     GLenum bufferMode)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    GLsizei i;
    if (!pi || count < 0 || count > GLS_MAX_VERTEX_ATTRIBS ||
        (bufferMode != GL_INTERLEAVED_ATTRIBS && bufferMode != GL_SEPARATE_ATTRIBS)) {
        advSetError(GL_INVALID_VALUE); return;
    }
    pi->transformFeedbackCount = count;
    pi->transformFeedbackMode = bufferMode;
    for (i = 0; i < count; ++i)
        advCopyString(pi->transformFeedbackVaryings[i], 64,
                      varyings ? varyings[i] : "", -1);
}

void gldAdvGetTransformFeedbackVarying(GLuint program, GLuint index,
                                       GLsizei bufSize, GLsizei *length,
                                       GLsizei *size, GLenum *type,
                                       GLchar *name)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    if (size) *size = 0;
    if (type) *type = 0;
    if (!pi || index >= (GLuint)pi->transformFeedbackCount) {
        advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return;
    }
    advReturnString(pi->transformFeedbackVaryings[index], bufSize, length, name);
    if (size) *size = 1;
    if (type) *type = GL_FLOAT_VEC4;
}

void gldAdvTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                        GLintptr offset, GLsizeiptr size)
{
    AdvTransformFeedback *tf;
    GLS_Buffer *b = buffer ? glsFindBuffer(buffer) : NULL;
    if (!xfb || xfb >= ADV_MAX_TF_OBJECTS || !g_tfObjects[xfb].allocated ||
        index >= GLS_MAX_BUFFER_BINDINGS || offset < 0 || size < 0 ||
        (b && (offset > b->size || size > b->size - offset))) {
        advSetError(GL_INVALID_VALUE); return;
    }
    tf = &g_tfObjects[xfb];
    tf->bindings[index].buffer = buffer;
    tf->bindings[index].offset = offset;
    tf->bindings[index].size = size;
}

void gldAdvTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
    GLS_Buffer *b = buffer ? glsFindBuffer(buffer) : NULL;
    gldAdvTransformFeedbackBufferRange(xfb, index, buffer, 0, b ? b->size : 0);
}

void gldAdvGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param)
{
    AdvTransformFeedback *tf;
    if (!param) return;
    *param = 0;
    if (!xfb || xfb >= ADV_MAX_TF_OBJECTS || !g_tfObjects[xfb].allocated) {
        advSetError(GL_INVALID_VALUE); return;
    }
    tf = &g_tfObjects[xfb];
    switch (pname) {
    case GL_TRANSFORM_FEEDBACK_PAUSED: *param = tf->paused ? GL_TRUE : GL_FALSE; break;
    case GL_TRANSFORM_FEEDBACK_ACTIVE:
        *param = (glsGetState()->transformFeedbackActive && g_boundTransformFeedback == xfb)
               ? GL_TRUE : GL_FALSE; break;
    case GL_TRANSFORM_FEEDBACK_BINDING: *param = (GLint)g_boundTransformFeedback; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

void gldAdvGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index,
                                   GLint *param)
{
    AdvTransformFeedback *tf;
    if (!param) return;
    *param = 0;
    if (!xfb || xfb >= ADV_MAX_TF_OBJECTS || !g_tfObjects[xfb].allocated ||
        index >= GLS_MAX_BUFFER_BINDINGS) { advSetError(GL_INVALID_VALUE); return; }
    tf = &g_tfObjects[xfb];
    if (pname == GL_TRANSFORM_FEEDBACK_BUFFER_BINDING)
        *param = (GLint)tf->bindings[index].buffer;
    else
        advSetError(GL_INVALID_ENUM);
}

void gldAdvGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index,
                                     GLint64 *param)
{
    AdvTransformFeedback *tf;
    if (!param) return;
    *param = 0;
    if (!xfb || xfb >= ADV_MAX_TF_OBJECTS || !g_tfObjects[xfb].allocated ||
        index >= GLS_MAX_BUFFER_BINDINGS) { advSetError(GL_INVALID_VALUE); return; }
    tf = &g_tfObjects[xfb];
    if (pname == GL_TRANSFORM_FEEDBACK_BUFFER_START)
        *param = (GLint64)tf->bindings[index].offset;
    else if (pname == GL_TRANSFORM_FEEDBACK_BUFFER_SIZE)
        *param = (GLint64)tf->bindings[index].size;
    else
        advSetError(GL_INVALID_ENUM);
}

/* ------------------------ Miscellaneous state ------------------------ */

void gldAdvPatchParameterfv(GLenum pname, const GLfloat *values)
{
    GLS_State *s = glsGetState();
    if (!values) return;
    if (pname == GL_PATCH_DEFAULT_OUTER_LEVEL)
        memcpy(s->patchDefaultOuter, values, sizeof(s->patchDefaultOuter));
    else if (pname == GL_PATCH_DEFAULT_INNER_LEVEL)
        memcpy(s->patchDefaultInner, values, sizeof(s->patchDefaultInner));
    else
        advSetError(GL_INVALID_ENUM);
}

void gldAdvFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    (void)target;
    /* D3D9 default-framebuffer dimensions are owned by the swap chain.  A
     * non-default FBO's effective size comes from its attachments, so the
     * width/height/layers/samples hints are retained only for validation. */
    switch (pname) {
    case GL_FRAMEBUFFER_DEFAULT_WIDTH:
    case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
    case GL_FRAMEBUFFER_DEFAULT_LAYERS:
    case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
    case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
        if (param < 0) advSetError(GL_INVALID_VALUE);
        break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

void gldAdvGetSynciv(GLsync sync, GLenum pname, GLsizei count,
                     GLsizei *length, GLint *values)
{
    IDirect3DQuery9 *q = (IDirect3DQuery9 *)sync;
    HRESULT hr;
    if (length) *length = 0;
    if (!values || count <= 0) return;
    if (!q) { advSetError(GL_INVALID_VALUE); return; }
    switch (pname) {
    case GL_OBJECT_TYPE: values[0] = GL_SYNC_FENCE; break;
    case GL_SYNC_CONDITION: values[0] = GL_SYNC_GPU_COMMANDS_COMPLETE; break;
    case GL_SYNC_FLAGS: values[0] = 0; break;
    case GL_SYNC_STATUS:
        hr = IDirect3DQuery9_GetData(q, NULL, 0, 0);
        values[0] = (hr == S_OK) ? GL_SIGNALED : GL_UNSIGNALED;
        break;
    default: advSetError(GL_INVALID_ENUM); return;
    }
    if (length) *length = 1;
}

GLenum gldAdvGetGraphicsResetStatus(void)
{
    IDirect3DDevice9 *dev = gldGetD3DDevice46();
    HRESULT hr;
    if (!dev) return GL_UNKNOWN_CONTEXT_RESET;
    hr = IDirect3DDevice9_TestCooperativeLevel(dev);
    return (hr == D3D_OK || hr == D3DERR_DEVICENOTRESET || hr == D3DERR_DEVICELOST)
         ? GL_NO_ERROR : GL_UNKNOWN_CONTEXT_RESET;
}

void gldAdvGetInternalformativ(GLenum target, GLenum internalformat,
                               GLenum pname, GLsizei count, GLint *params)
{
    GLint v = 0;
    GLsizei i;
    (void)target; (void)internalformat;
    if (count < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!params) return;
    switch (pname) {
    case GL_INTERNALFORMAT_SUPPORTED: v = GL_TRUE; break;
    case GL_INTERNALFORMAT_PREFERRED: v = (GLint)internalformat; break;
    case GL_COLOR_COMPONENTS:
        v = (internalformat != GL_DEPTH_COMPONENT &&
             internalformat != GL_DEPTH_STENCIL &&
             internalformat != GL_STENCIL_INDEX) ? GL_TRUE : GL_FALSE;
        break;
    case GL_DEPTH_COMPONENTS: v = (internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_STENCIL) ? GL_TRUE : GL_FALSE; break;
    case GL_STENCIL_COMPONENTS: v = (internalformat == GL_STENCIL_INDEX || internalformat == GL_DEPTH_STENCIL) ? GL_TRUE : GL_FALSE; break;
    case GL_FRAMEBUFFER_RENDERABLE:
    case GL_FRAMEBUFFER_BLEND: v = GL_FULL_SUPPORT; break;
    case GL_READ_PIXELS:
    case GL_TEXTURE_IMAGE_FORMAT:
    case GL_GET_TEXTURE_IMAGE_FORMAT: v = GL_RGBA; break;
    case GL_READ_PIXELS_FORMAT:
    case GL_TEXTURE_IMAGE_TYPE:
    case GL_GET_TEXTURE_IMAGE_TYPE: v = GL_UNSIGNED_BYTE; break;
    case GL_NUM_SAMPLE_COUNTS: v = 1; break;
    case GL_SAMPLES: v = 1; break;
    case GL_MAX_WIDTH:
    case GL_MAX_HEIGHT:
    case GL_MAX_DEPTH:
    case GL_MAX_LAYERS: v = 4096; break;
    default: v = 0; break;
    }
    for (i = 0; i < count; ++i) params[i] = (i == 0) ? v : 0;
}

void gldAdvGetInternalformati64v(GLenum target, GLenum internalformat,
                                 GLenum pname, GLsizei count, GLint64 *params)
{
    GLint values32[64];
    GLsizei i, n = count > 64 ? 64 : count;
    if (!params) return;
    gldAdvGetInternalformativ(target, internalformat, pname, n, values32);
    for (i = 0; i < n; ++i) params[i] = (GLint64)values32[i];
    for (; i < count; ++i) params[i] = 0;
}

void gldAdvGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname,
                                          GLint *param)
{
    GLS_FBO *fbo = framebuffer ? glsFindFBO(framebuffer) : NULL;
    if (!param) return;
    *param = 0;
    if (framebuffer && !fbo) { advSetError(GL_INVALID_OPERATION); return; }
    switch (pname) {
    case GL_FRAMEBUFFER_DEFAULT_WIDTH:
        if (fbo && fbo->colorAttachment[0]) {
            GLS_Texture *t = glsFindTexture(fbo->colorAttachment[0]);
            *param = t ? t->width : 0;
        }
        break;
    case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
        if (fbo && fbo->colorAttachment[0]) {
            GLS_Texture *t = glsFindTexture(fbo->colorAttachment[0]);
            *param = t ? t->height : 0;
        }
        break;
    case GL_FRAMEBUFFER_DEFAULT_LAYERS: *param = 1; break;
    case GL_FRAMEBUFFER_DEFAULT_SAMPLES: *param = 0; break;
    case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS: *param = GL_TRUE; break;
    case GL_DOUBLEBUFFER: *param = framebuffer ? GL_FALSE : GL_TRUE; break;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT: *param = GL_RGBA; break;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE: *param = GL_UNSIGNED_BYTE; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

static void advWriteQueryBuffer(GLuint id, GLuint buffer, GLenum pname,
                                GLintptr offset, int kind)
{
    GLS_Buffer *b = glsFindBuffer(buffer);
    size_t bytes = (kind >= 2) ? sizeof(GLuint64) : sizeof(GLuint);
    if (!b || !b->data || offset < 0 || (size_t)offset > (size_t)b->size ||
        bytes > (size_t)b->size - (size_t)offset) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    if (kind == 0) {
        GLint value = 0; _glsGetQueryObjectiv(id, pname, &value);
        memcpy((unsigned char *)b->data + offset, &value, sizeof(value));
    } else if (kind == 1) {
        GLuint value = 0; _glsGetQueryObjectuiv(id, pname, &value);
        memcpy((unsigned char *)b->data + offset, &value, sizeof(value));
    } else if (kind == 2) {
        GLint64 value = 0; _glsGetQueryObjecti64v(id, pname, &value);
        memcpy((unsigned char *)b->data + offset, &value, sizeof(value));
    } else {
        GLuint64 value = 0; _glsGetQueryObjectui64v(id, pname, &value);
        memcpy((unsigned char *)b->data + offset, &value, sizeof(value));
    }
}

void gldAdvGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname,
                                  GLintptr offset)
{ advWriteQueryBuffer(id, buffer, pname, offset, 0); }

void gldAdvGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname,
                                   GLintptr offset)
{ advWriteQueryBuffer(id, buffer, pname, offset, 1); }

void gldAdvGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname,
                                    GLintptr offset)
{ advWriteQueryBuffer(id, buffer, pname, offset, 2); }

void gldAdvGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                     GLintptr offset)
{ advWriteQueryBuffer(id, buffer, pname, offset, 3); }

/* ---------------------------- Labels -------------------------------- */

static AdvLabel *advFindLabel(GLenum identifier, GLuint name, const void *ptr,
                              BOOL create)
{
    int i;
    AdvLabel *empty = NULL;
    for (i = 0; i < ADV_MAX_LABELS; ++i) {
        if (!g_labels[i].used) { if (!empty) empty = &g_labels[i]; continue; }
        if (ptr ? (g_labels[i].ptr == ptr)
                : (g_labels[i].identifier == identifier && g_labels[i].name == name))
            return &g_labels[i];
    }
    if (create && empty) { memset(empty, 0, sizeof(*empty)); empty->used = TRUE; return empty; }
    return NULL;
}

void gldAdvObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                       const GLchar *label)
{
    AdvLabel *slot = advFindLabel(identifier, name, NULL, TRUE);
    if (!slot) { advSetError(GL_OUT_OF_MEMORY); return; }
    slot->identifier = identifier;
    slot->name = name;
    slot->ptr = NULL;
    advCopyString(slot->text, sizeof(slot->text), label, length);
}

void gldAdvObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    AdvLabel *slot;
    if (!ptr) { advSetError(GL_INVALID_VALUE); return; }
    slot = advFindLabel(0, 0, ptr, TRUE);
    if (!slot) { advSetError(GL_OUT_OF_MEMORY); return; }
    slot->ptr = ptr;
    advCopyString(slot->text, sizeof(slot->text), label, length);
}

void gldAdvGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize,
                          GLsizei *length, GLchar *label)
{
    AdvLabel *slot = advFindLabel(identifier, name, NULL, FALSE);
    advReturnString(slot ? slot->text : "", bufSize, length, label);
}

void gldAdvGetObjectPtrLabel(const void *ptr, GLsizei bufSize,
                             GLsizei *length, GLchar *label)
{
    AdvLabel *slot = advFindLabel(0, 0, ptr, FALSE);
    advReturnString(slot ? slot->text : "", bufSize, length, label);
}

/* --------------------- Program source binaries ----------------------- */

GLuint gldAdvCreateShaderProgramv(GLenum type, GLsizei count,
                                  const GLchar *const*strings)
{
    GLuint shader = _glsCreateShader(type);
    GLuint program = _glsCreateProgram();
    _glsShaderSource(shader, count, strings, NULL);
    _glsCompileShader(shader);
    _glsAttachShader(program, shader);
    gldAdvProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
    _glsLinkProgram(program);
    return program;
}

void gldAdvProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    GLS_Program *p = glsFindProgram(program);
    if (!p) { advSetError(GL_INVALID_VALUE); return; }
    if (pname == GL_PROGRAM_SEPARABLE) p->separable = value ? TRUE : FALSE;
    else if (pname == GL_PROGRAM_BINARY_RETRIEVABLE_HINT) p->binaryRetrievable = value ? TRUE : FALSE;
    else advSetError(GL_INVALID_ENUM);
}

void gldAdvGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                            GLenum *binaryFormat, void *binary)
{
    GLS_Program *p = glsFindProgram(program);
    GLS_Shader *vs;
    GLS_Shader *fs;
    AdvProgramBinaryHeader h;
    size_t total;
    unsigned char *out = (unsigned char *)binary;
    if (length) *length = 0;
    if (!p || !p->linked) { advSetError(GL_INVALID_OPERATION); return; }
    vs = glsFindShader(p->vertShader);
    fs = glsFindShader(p->fragShader);
    h.magic = ADV_BINARY_FORMAT;
    h.version = ADV_BINARY_VERSION;
    h.vertexLength = (vs && vs->source) ? (DWORD)strlen(vs->source) + 1 : 0;
    h.fragmentLength = (fs && fs->source) ? (DWORD)strlen(fs->source) + 1 : 0;
    total = sizeof(h) + h.vertexLength + h.fragmentLength;
    if (!binary || bufSize < 0 || (size_t)bufSize < total) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    memcpy(out, &h, sizeof(h)); out += sizeof(h);
    if (h.vertexLength) { memcpy(out, vs->source, h.vertexLength); out += h.vertexLength; }
    if (h.fragmentLength) memcpy(out, fs->source, h.fragmentLength);
    if (binaryFormat) *binaryFormat = ADV_BINARY_FORMAT;
    if (length) *length = (GLsizei)total;
}

void gldAdvProgramBinary(GLuint program, GLenum binaryFormat,
                         const void *binary, GLsizei length)
{
    GLS_Program *p = glsFindProgram(program);
    const AdvProgramBinaryHeader *h = (const AdvProgramBinaryHeader *)binary;
    const char *src;
    size_t total;
    if (!p) { advSetError(GL_INVALID_VALUE); return; }
    p->linked = FALSE;
    if (binaryFormat != ADV_BINARY_FORMAT || !binary ||
        length < (GLsizei)sizeof(*h) || h->magic != ADV_BINARY_FORMAT ||
        h->version != ADV_BINARY_VERSION) { advSetError(GL_INVALID_ENUM); return; }
    total = sizeof(*h) + (size_t)h->vertexLength + (size_t)h->fragmentLength;
    if (total > (size_t)length) { advSetError(GL_INVALID_VALUE); return; }
    src = (const char *)(h + 1);
    if (h->vertexLength) {
        GLuint sh = _glsCreateShader(GL_VERTEX_SHADER);
        const char *one = src;
        GLint len = (GLint)h->vertexLength - 1;
        _glsShaderSource(sh, 1, &one, &len); _glsCompileShader(sh); _glsAttachShader(program, sh);
        src += h->vertexLength;
    }
    if (h->fragmentLength) {
        GLuint sh = _glsCreateShader(GL_FRAGMENT_SHADER);
        const char *one = src;
        GLint len = (GLint)h->fragmentLength - 1;
        _glsShaderSource(sh, 1, &one, &len); _glsCompileShader(sh); _glsAttachShader(program, sh);
    }
    _glsLinkProgram(program);
}

void gldAdvShaderBinary(GLsizei count, const GLuint *shaders,
                        GLenum binaryFormat, const void *binary,
                        GLsizei length)
{
    GLsizei i;
    const char *src = (const char *)binary;
    if (count < 0 || length < 0 || (!shaders && count)) { advSetError(GL_INVALID_VALUE); return; }
    if (binaryFormat != ADV_BINARY_FORMAT || !binary) { advSetError(GL_INVALID_ENUM); return; }
    for (i = 0; i < count; ++i) {
        const char *one = src;
        _glsShaderSource(shaders[i], 1, &one, &length);
    }
}

void gldAdvSpecializeShader(GLuint shader, const GLchar *entryPoint,
                            GLuint numConstants, const GLuint *constantIndex,
                            const GLuint *constantValue)
{
    GLS_Shader *sh = glsFindShader(shader);
    (void)constantIndex; (void)constantValue;
    if (!sh || !entryPoint || numConstants) {
        advSetError(sh ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return;
    }
    if (strcmp(entryPoint, "main") != 0) { advSetError(GL_INVALID_VALUE); return; }
    _glsCompileShader(shader);
}

/* --------------------- Uniform/resource reflection ------------------- */

static int advResourceCount(GLS_Program *p, GLenum iface)
{
    AdvProgramInfo *pi;
    if (!p) return 0;
    switch (iface) {
    case GL_UNIFORM: return p->resolvedCount;
    case GL_PROGRAM_INPUT: return p->activeAttribCount;
    case GL_PROGRAM_OUTPUT:
        pi = advProgramInfo(p->id);
        return pi ? pi->fragCount : 0;
    default: return 0;
    }
}

static const char *advResourceName(GLS_Program *p, GLenum iface, GLuint index)
{
    AdvProgramInfo *pi;
    if (!p) return NULL;
    if (iface == GL_UNIFORM && index < (GLuint)p->resolvedCount)
        return p->resolved[index].name;
    if (iface == GL_PROGRAM_INPUT && index < (GLuint)p->activeAttribCount)
        return p->activeAttribs[index].name;
    if (iface == GL_PROGRAM_OUTPUT) {
        pi = advProgramInfo(p->id);
        if (pi && index < (GLuint)pi->fragCount) return pi->frag[index].name;
    }
    return NULL;
}

void gldAdvGetUniformIndices(GLuint program, GLsizei uniformCount,
                             const GLchar *const*uniformNames,
                             GLuint *uniformIndices)
{
    GLS_Program *p = glsFindProgram(program);
    GLsizei i;
    int j;
    if (!p || uniformCount < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!uniformNames || !uniformIndices) return;
    for (i = 0; i < uniformCount; ++i) {
        uniformIndices[i] = GL_INVALID_INDEX;
        for (j = 0; j < p->resolvedCount; ++j)
            if (uniformNames[i] && strcmp(p->resolved[j].name, uniformNames[i]) == 0) {
                uniformIndices[i] = (GLuint)j; break;
            }
    }
}

void gldAdvGetActiveUniformName(GLuint program, GLuint uniformIndex,
                                GLsizei bufSize, GLsizei *length,
                                GLchar *uniformName)
{
    GLS_Program *p = glsFindProgram(program);
    if (!p || uniformIndex >= (GLuint)p->resolvedCount) {
        advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, uniformName); return;
    }
    advReturnString(p->resolved[uniformIndex].name, bufSize, length, uniformName);
}

void gldAdvGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                               const GLuint *uniformIndices, GLenum pname,
                               GLint *params)
{
    GLS_Program *p = glsFindProgram(program);
    GLsizei i;
    if (!p || uniformCount < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!uniformIndices || !params) return;
    for (i = 0; i < uniformCount; ++i) {
        GLuint idx = uniformIndices[i];
        if (idx >= (GLuint)p->resolvedCount) { params[i] = 0; advSetError(GL_INVALID_VALUE); continue; }
        switch (pname) {
        case GL_UNIFORM_TYPE: params[i] = GL_FLOAT_VEC4; break;
        case GL_UNIFORM_SIZE: params[i] = p->resolved[idx].registerCount > 0 ? p->resolved[idx].registerCount : 1; break;
        case GL_UNIFORM_NAME_LENGTH: params[i] = (GLint)strlen(p->resolved[idx].name) + 1; break;
        case GL_UNIFORM_BLOCK_INDEX: params[i] = -1; break;
        case GL_UNIFORM_OFFSET: params[i] = -1; break;
        case GL_UNIFORM_ARRAY_STRIDE:
        case GL_UNIFORM_MATRIX_STRIDE: params[i] = 0; break;
        case GL_UNIFORM_IS_ROW_MAJOR: params[i] = GL_FALSE; break;
        default: params[i] = 0; advSetError(GL_INVALID_ENUM); break;
        }
    }
}

void gldAdvGetProgramInterfaceiv(GLuint program, GLenum programInterface,
                                 GLenum pname, GLint *params)
{
    GLS_Program *p = glsFindProgram(program);
    int i, n, maxLen = 0;
    if (!params) return;
    *params = 0;
    if (!p) { advSetError(GL_INVALID_OPERATION); return; }
    n = advResourceCount(p, programInterface);
    switch (pname) {
    case GL_ACTIVE_RESOURCES: *params = n; break;
    case GL_MAX_NAME_LENGTH:
        for (i = 0; i < n; ++i) {
            const char *name = advResourceName(p, programInterface, (GLuint)i);
            int len = name ? (int)strlen(name) + 1 : 0;
            if (len > maxLen) maxLen = len;
        }
        *params = maxLen;
        break;
    case GL_MAX_NUM_ACTIVE_VARIABLES: *params = n; break;
    case GL_MAX_NUM_COMPATIBLE_SUBROUTINES: *params = 0; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

GLuint gldAdvGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                     const GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    int i, n = advResourceCount(p, programInterface);
    if (!p || !name) return GL_INVALID_INDEX;
    for (i = 0; i < n; ++i) {
        const char *candidate = advResourceName(p, programInterface, (GLuint)i);
        if (candidate && strcmp(candidate, name) == 0) return (GLuint)i;
    }
    return GL_INVALID_INDEX;
}

GLint gldAdvGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                       const GLchar *name)
{
    GLuint idx = gldAdvGetProgramResourceIndex(program, programInterface, name);
    if (idx == GL_INVALID_INDEX) return -1;
    if (programInterface == GL_PROGRAM_OUTPUT) return gldAdvGetFragDataLocation(program, name);
    return (GLint)idx;
}

GLint gldAdvGetProgramResourceLocationIndex(GLuint program,
                                            GLenum programInterface,
                                            const GLchar *name)
{
    if (programInterface != GL_PROGRAM_OUTPUT) return -1;
    return gldAdvGetFragDataIndex(program, name);
}

void gldAdvGetProgramResourceName(GLuint program, GLenum programInterface,
                                  GLuint index, GLsizei bufSize,
                                  GLsizei *length, GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    const char *src = advResourceName(p, programInterface, index);
    if (!p || !src) { advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return; }
    advReturnString(src, bufSize, length, name);
}

void gldAdvGetProgramResourceiv(GLuint program, GLenum programInterface,
                                GLuint index, GLsizei propCount,
                                const GLenum *props, GLsizei count,
                                GLsizei *length, GLint *params)
{
    GLS_Program *p = glsFindProgram(program);
    const char *name = advResourceName(p, programInterface, index);
    GLsizei i, written = 0;
    if (length) *length = 0;
    if (!p || !name || propCount < 0 || count < 0) { advSetError(GL_INVALID_VALUE); return; }
    if (!props || !params) return;
    for (i = 0; i < propCount && written < count; ++i, ++written) {
        switch (props[i]) {
        case GL_NAME_LENGTH: params[written] = (GLint)strlen(name) + 1; break;
        case GL_TYPE:
            params[written] = (programInterface == GL_PROGRAM_INPUT && index < (GLuint)p->activeAttribCount)
                            ? (GLint)p->activeAttribs[index].type : GL_FLOAT_VEC4;
            break;
        case GL_ARRAY_SIZE: params[written] = 1; break;
        case GL_LOCATION: params[written] = gldAdvGetProgramResourceLocation(program, programInterface, name); break;
        case GL_LOCATION_INDEX: params[written] = gldAdvGetProgramResourceLocationIndex(program, programInterface, name); break;
        case GL_REFERENCED_BY_VERTEX_SHADER: params[written] = (programInterface != GL_PROGRAM_OUTPUT); break;
        case GL_REFERENCED_BY_FRAGMENT_SHADER: params[written] = (programInterface != GL_PROGRAM_INPUT); break;
        default: params[written] = 0; break;
        }
    }
    if (length) *length = written;
}

/* ---------------- Uniform blocks, atomic counters and subroutines -------- */

static GLS_Shader *advStageShader(GLS_Program *p, GLenum shaderType)
{
    GLuint id = 0;
    if (!p) return NULL;
    switch (shaderType) {
    case GL_VERTEX_SHADER: id = p->vertShader; break;
    case GL_FRAGMENT_SHADER: id = p->fragShader; break;
    case GL_GEOMETRY_SHADER: id = p->geomShader; break;
    case GL_TESS_CONTROL_SHADER: id = p->tessControlShader; break;
    case GL_TESS_EVALUATION_SHADER: id = p->tessEvalShader; break;
    case GL_COMPUTE_SHADER: id = p->computeShader; break;
    default: break;
    }
    return glsFindShader(id);
}

static const char *advSkipSpace(const char *p)
{
    while (p && *p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    return p;
}

static const char *advToken(const char *p, char *out, int outSize)
{
    int n = 0;
    p = advSkipSpace(p);
    if (!p) { if (outSize) out[0] = '\0'; return NULL; }
    while ((*p == '_' || (*p >= 'A' && *p <= 'Z') ||
            (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) &&
           n < outSize - 1)
        out[n++] = *p++;
    out[n] = '\0';
    return p;
}

static int advCollectUniformBlocks(GLS_Program *program,
                                   char names[ADV_MAX_BLOCKS][64])
{
    GLuint stages[6];
    int stage, count = 0;
    stages[0] = program ? program->vertShader : 0;
    stages[1] = program ? program->fragShader : 0;
    stages[2] = program ? program->geomShader : 0;
    stages[3] = program ? program->tessControlShader : 0;
    stages[4] = program ? program->tessEvalShader : 0;
    stages[5] = program ? program->computeShader : 0;
    for (stage = 0; stage < 6; ++stage) {
        GLS_Shader *sh = glsFindShader(stages[stage]);
        const char *p;
        if (!sh || !sh->source) continue;
        p = sh->source;
        while ((p = strstr(p, "uniform")) != NULL) {
            char block[64];
            const char *after;
            int i, duplicate = 0;
            if ((p > sh->source && ((p[-1] >= 'A' && p[-1] <= 'Z') ||
                (p[-1] >= 'a' && p[-1] <= 'z') || p[-1] == '_'))) { p += 7; continue; }
            after = advToken(p + 7, block, sizeof(block));
            after = advSkipSpace(after);
            if (!block[0] || !after || *after != '{') { p += 7; continue; }
            for (i = 0; i < count; ++i)
                if (strcmp(names[i], block) == 0) duplicate = 1;
            if (!duplicate && count < ADV_MAX_BLOCKS) {
                advCopyString(names[count], 64, block, -1);
                ++count;
            }
            p = after + 1;
        }
    }
    return count;
}

GLuint gldAdvGetUniformBlockIndex(GLuint program, const GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_BLOCKS][64];
    int i, count;
    if (!p || !name) {
        advSetError(GL_INVALID_VALUE);
        return GL_INVALID_INDEX;
    }
    count = advCollectUniformBlocks(p, names);
    for (i = 0; i < count; ++i)
        if (strcmp(names[i], name) == 0) return (GLuint)i;
    return GL_INVALID_INDEX;
}

void gldAdvUniformBlockBinding(GLuint program, GLuint blockIndex,
                               GLuint blockBinding)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    char names[ADV_MAX_BLOCKS][64];
    int count;
    if (!pi) { advSetError(GL_INVALID_VALUE); return; }
    count = advCollectUniformBlocks(glsFindProgram(program), names);
    if (blockIndex >= (GLuint)count || blockIndex >= ADV_MAX_BLOCKS) {
        advSetError(GL_INVALID_VALUE); return;
    }
    pi->uniformBlockBinding[blockIndex] = blockBinding;
}

void gldAdvShaderStorageBlockBinding(GLuint program, GLuint blockIndex,
                                     GLuint blockBinding)
{
    AdvProgramInfo *pi = advProgramInfo(program);
    if (!pi || blockIndex >= ADV_MAX_BLOCKS) { advSetError(GL_INVALID_VALUE); return; }
    pi->storageBlockBinding[blockIndex] = blockBinding;
}

void gldAdvGetActiveUniformBlockName(GLuint program, GLuint blockIndex,
                                     GLsizei bufSize, GLsizei *length,
                                     GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_BLOCKS][64];
    int count;
    if (!p) { advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return; }
    count = advCollectUniformBlocks(p, names);
    if (blockIndex >= (GLuint)count) {
        advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return;
    }
    advReturnString(names[blockIndex], bufSize, length, name);
}

void gldAdvGetActiveUniformBlockiv(GLuint program, GLuint blockIndex,
                                   GLenum pname, GLint *params)
{
    GLS_Program *p = glsFindProgram(program);
    AdvProgramInfo *pi = advProgramInfo(program);
    char names[ADV_MAX_BLOCKS][64];
    int blockCount;
    GLuint binding;
    GLS_Buffer *buffer;
    if (!params) return;
    *params = 0;
    if (!p || !pi) { advSetError(GL_INVALID_VALUE); return; }
    blockCount = advCollectUniformBlocks(p, names);
    if (blockIndex >= (GLuint)blockCount) { advSetError(GL_INVALID_VALUE); return; }
    binding = pi->uniformBlockBinding[blockIndex];
    buffer = binding < GLS_MAX_BUFFER_BINDINGS
           ? glsFindBuffer(glsGetState()->uniformBindings[binding].buffer) : NULL;
    switch (pname) {
    case GL_UNIFORM_BLOCK_BINDING: *params = (GLint)binding; break;
    case GL_UNIFORM_BLOCK_DATA_SIZE: *params = buffer ? (GLint)buffer->size : 0; break;
    case GL_UNIFORM_BLOCK_NAME_LENGTH: *params = (GLint)strlen(names[blockIndex]) + 1; break;
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS: *params = 0; break;
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: *params = p->vertShader ? GL_TRUE : GL_FALSE; break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: *params = p->fragShader ? GL_TRUE : GL_FALSE; break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER: *params = p->geomShader ? GL_TRUE : GL_FALSE; break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER: *params = p->tessControlShader ? GL_TRUE : GL_FALSE; break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER: *params = p->tessEvalShader ? GL_TRUE : GL_FALSE; break;
    case GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER: *params = p->computeShader ? GL_TRUE : GL_FALSE; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

void gldAdvGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                          GLenum pname, GLint *params)
{
    GLS_State *s = glsGetState();
    GLS_IndexedBufferBinding *binding;
    GLS_Buffer *buffer;
    (void)program;
    if (!params) return;
    *params = 0;
    if (bufferIndex >= GLS_MAX_BUFFER_BINDINGS) { advSetError(GL_INVALID_VALUE); return; }
    binding = &s->atomicCounterBindings[bufferIndex];
    buffer = glsFindBuffer(binding->buffer);
    switch (pname) {
    case GL_ATOMIC_COUNTER_BUFFER_BINDING: *params = (GLint)binding->buffer; break;
    case GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE: *params = buffer ? (GLint)(binding->size ? binding->size : buffer->size) : 0; break;
    case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS: *params = buffer ? (GLint)(buffer->size / sizeof(GLuint)) : 0; break;
    case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES: break;
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER:
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER:
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER:
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER:
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER:
    case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER:
        *params = buffer ? GL_TRUE : GL_FALSE; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

static int advCollectSubroutines(GLS_Program *program, GLenum shaderType,
                                 BOOL uniforms,
                                 char names[ADV_MAX_SUBROUTINES][64])
{
    GLS_Shader *sh = advStageShader(program, shaderType);
    const char *p;
    int count = 0;
    if (!sh || !sh->source) return 0;
    p = sh->source;
    while ((p = strstr(p, "subroutine")) != NULL && count < ADV_MAX_SUBROUTINES) {
        const char *q = advSkipSpace(p + 10);
        char token[64];
        char name[64];
        if (uniforms) {
            if (strncmp(q, "uniform", 7) != 0) { p += 10; continue; }
            q = advToken(q + 7, token, sizeof(token));
            q = advToken(q, name, sizeof(name));
        } else {
            if (*q != '(') { p += 10; continue; }
            q = strchr(q, ')');
            if (!q) break;
            q = advToken(q + 1, token, sizeof(token));
            q = advToken(q, name, sizeof(name));
        }
        if (name[0]) advCopyString(names[count++], 64, name, -1);
        p = q ? q : p + 10;
    }
    return count;
}

void gldAdvGetProgramStageiv(GLuint program, GLenum shaderType, GLenum pname,
                             GLint *values)
{
    GLS_Program *p = glsFindProgram(program);
    char subs[ADV_MAX_SUBROUTINES][64];
    char uniforms[ADV_MAX_SUBROUTINES][64];
    int nSubs, nUniforms, i, maxLen = 0;
    if (!values) return;
    *values = 0;
    if (!p) { advSetError(GL_INVALID_VALUE); return; }
    nSubs = advCollectSubroutines(p, shaderType, FALSE, subs);
    nUniforms = advCollectSubroutines(p, shaderType, TRUE, uniforms);
    switch (pname) {
    case GL_ACTIVE_SUBROUTINES: *values = nSubs; break;
    case GL_ACTIVE_SUBROUTINE_UNIFORMS: *values = nUniforms; break;
    case GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS: *values = nUniforms; break;
    case GL_ACTIVE_SUBROUTINE_MAX_LENGTH:
        for (i = 0; i < nSubs; ++i) if ((int)strlen(subs[i]) + 1 > maxLen) maxLen = (int)strlen(subs[i]) + 1;
        *values = maxLen; break;
    case GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH:
        for (i = 0; i < nUniforms; ++i) if ((int)strlen(uniforms[i]) + 1 > maxLen) maxLen = (int)strlen(uniforms[i]) + 1;
        *values = maxLen; break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

GLuint gldAdvGetSubroutineIndex(GLuint program, GLenum shaderType,
                                const GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_SUBROUTINES][64];
    int i, count = advCollectSubroutines(p, shaderType, FALSE, names);
    if (!p || !name) return GL_INVALID_INDEX;
    for (i = 0; i < count; ++i) if (strcmp(names[i], name) == 0) return (GLuint)i;
    return GL_INVALID_INDEX;
}

GLint gldAdvGetSubroutineUniformLocation(GLuint program, GLenum shaderType,
                                         const GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_SUBROUTINES][64];
    int i, count = advCollectSubroutines(p, shaderType, TRUE, names);
    if (!p || !name) return -1;
    for (i = 0; i < count; ++i) if (strcmp(names[i], name) == 0) return i;
    return -1;
}

void gldAdvGetActiveSubroutineName(GLuint program, GLenum shaderType,
                                   GLuint index, GLsizei bufSize,
                                   GLsizei *length, GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_SUBROUTINES][64];
    int count = advCollectSubroutines(p, shaderType, FALSE, names);
    if (!p || index >= (GLuint)count) { advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return; }
    advReturnString(names[index], bufSize, length, name);
}

void gldAdvGetActiveSubroutineUniformName(GLuint program, GLenum shaderType,
                                          GLuint index, GLsizei bufSize,
                                          GLsizei *length, GLchar *name)
{
    GLS_Program *p = glsFindProgram(program);
    char names[ADV_MAX_SUBROUTINES][64];
    int count = advCollectSubroutines(p, shaderType, TRUE, names);
    if (!p || index >= (GLuint)count) { advSetError(GL_INVALID_VALUE); advReturnString("", bufSize, length, name); return; }
    advReturnString(names[index], bufSize, length, name);
}

void gldAdvGetActiveSubroutineUniformiv(GLuint program, GLenum shaderType,
                                        GLuint index, GLenum pname,
                                        GLint *values)
{
    GLS_Program *p = glsFindProgram(program);
    char uniformNames[ADV_MAX_SUBROUTINES][64];
    char subroutineNames[ADV_MAX_SUBROUTINES][64];
    int count = advCollectSubroutines(p, shaderType, TRUE, uniformNames);
    int subCount;
    if (!values) return;
    *values = 0;
    if (!p || index >= (GLuint)count) { advSetError(GL_INVALID_VALUE); return; }
    subCount = advCollectSubroutines(p, shaderType, FALSE, subroutineNames);
    switch (pname) {
    case GL_NUM_COMPATIBLE_SUBROUTINES: *values = subCount; break;
    case GL_COMPATIBLE_SUBROUTINES: {
        int i; for (i = 0; i < subCount; ++i) values[i] = i; break;
    }
    case GL_UNIFORM_SIZE: *values = 1; break;
    case GL_UNIFORM_NAME_LENGTH:
        *values = (GLint)strlen(uniformNames[index]) + 1;
        break;
    default: advSetError(GL_INVALID_ENUM); break;
    }
}

static int advStageSlot(GLenum shaderType)
{
    switch (shaderType) {
    case GL_VERTEX_SHADER: return 0;
    case GL_FRAGMENT_SHADER: return 1;
    case GL_GEOMETRY_SHADER: return 2;
    case GL_TESS_CONTROL_SHADER: return 3;
    case GL_TESS_EVALUATION_SHADER: return 4;
    case GL_COMPUTE_SHADER: return 5;
    default: return -1;
    }
}

void gldAdvUniformSubroutinesuiv(GLenum shaderType, GLsizei count,
                                 const GLuint *indices)
{
    GLS_State *s = glsGetState();
    GLuint program = s->boundProgram ? s->boundProgram : s->pipelineActiveProgram;
    AdvProgramInfo *pi = advProgramInfo(program);
    int stage = advStageSlot(shaderType);
    GLsizei i;
    if (!pi || stage < 0 || count < 0 || count > ADV_MAX_SUBROUTINES) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    if (!indices && count) return;
    for (i = 0; i < count; ++i) pi->subroutineSelection[stage][i] = indices[i];
}

void gldAdvGetUniformSubroutineuiv(GLenum shaderType, GLint location,
                                   GLuint *params)
{
    GLS_State *s = glsGetState();
    GLuint program = s->boundProgram ? s->boundProgram : s->pipelineActiveProgram;
    AdvProgramInfo *pi = advProgramInfo(program);
    int stage = advStageSlot(shaderType);
    if (!params) return;
    *params = 0;
    if (!pi || stage < 0 || location < 0 || location >= ADV_MAX_SUBROUTINES) {
        advSetError(GL_INVALID_OPERATION); return;
    }
    *params = pi->subroutineSelection[stage][location];
}

/* --------------------------- Debug output ------------------------------- */

static void advQueueDebug(GLenum source, GLenum type, GLuint id,
                          GLenum severity, const char *message, int length)
{
    GLS_State *s = glsGetState();
    AdvDebugMessage *m;
    typedef void (APIENTRY *AdvDebugCallback)(GLenum, GLenum, GLuint, GLenum,
                                               GLsizei, const GLchar *,
                                               const void *);
    if (!g_debugEnabled || !message) return;
    m = &g_debugMessages[g_debugWrite];
    m->source = source; m->type = type; m->id = id; m->severity = severity;
    advCopyString(m->message, sizeof(m->message), message, length);
    m->length = (GLsizei)strlen(m->message);
    g_debugWrite = (g_debugWrite + 1) % ADV_MAX_DEBUG_MESSAGES;
    if (g_debugCount < ADV_MAX_DEBUG_MESSAGES) ++g_debugCount;
    else g_debugRead = (g_debugRead + 1) % ADV_MAX_DEBUG_MESSAGES;
    if (s->debugCallback)
        ((AdvDebugCallback)s->debugCallback)(source, type, id, severity,
                                             m->length, m->message,
                                             s->debugUserParam);
}

void gldAdvDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                               GLsizei count, const GLuint *ids,
                               GLboolean enabled)
{
    (void)source; (void)type; (void)severity; (void)count; (void)ids;
    g_debugEnabled = enabled ? TRUE : FALSE;
}

void gldAdvDebugMessageInsert(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message)
{
    if (length < 0 && message) length = (GLsizei)strlen(message);
    advQueueDebug(source, type, id, severity, message, length);
}

GLuint gldAdvGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                GLenum *types, GLuint *ids, GLenum *severities,
                                GLsizei *lengths, GLchar *messageLog)
{
    GLuint written = 0;
    GLsizei used = 0;
    while (written < count && g_debugCount) {
        AdvDebugMessage *m = &g_debugMessages[g_debugRead];
        GLsizei bytes = m->length + 1;
        if (messageLog && used + bytes > bufSize) break;
        if (sources) sources[written] = m->source;
        if (types) types[written] = m->type;
        if (ids) ids[written] = m->id;
        if (severities) severities[written] = m->severity;
        if (lengths) lengths[written] = bytes;
        if (messageLog) memcpy(messageLog + used, m->message, (size_t)bytes);
        used += bytes;
        g_debugRead = (g_debugRead + 1) % ADV_MAX_DEBUG_MESSAGES;
        --g_debugCount;
        ++written;
    }
    return written;
}

void gldAdvPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                          const GLchar *message)
{
    ++g_debugGroupDepth;
    advQueueDebug(source, GL_DEBUG_TYPE_PUSH_GROUP, id,
                  GL_DEBUG_SEVERITY_NOTIFICATION, message, length);
}

void gldAdvPopDebugGroup(void)
{
    if (!g_debugGroupDepth) { advSetError(GL_STACK_UNDERFLOW); return; }
    --g_debugGroupDepth;
    advQueueDebug(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_POP_GROUP, 0,
                  GL_DEBUG_SEVERITY_NOTIFICATION, "debug group end", -1);
}
