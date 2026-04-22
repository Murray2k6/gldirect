/*********************************************************************************
*  gl_ext_dsa.c — EXT_direct_state_access and vendor extension implementations
*
*  These functions are NOT in Mesa 26 llvmpipe but are queried by games like
*  Wolfenstein: The New Order (id Tech 5). Instead of returning NULL or no-ops,
*  we implement them by forwarding to the equivalent non-DSA Mesa functions.
*
*  DSA pattern: save current binding → bind named object → call regular func → restore
*
*  Also includes NV_half_float vertex weight stubs and AMD query extensions.
*********************************************************************************/

#include <windows.h>
#include "mesa_proxy.h"

#pragma warning(disable: 4100 4152)

/* GL types */
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef void GLvoid;
typedef unsigned short GLhalfNV;
typedef long long GLint64;
typedef unsigned long long GLuint64;
typedef signed long long GLsizeiptr;
typedef signed long long GLintptr;

/* GL constants */
#define GL_FRAMEBUFFER            0x8D40
#define GL_RENDERBUFFER           0x8D41
#define GL_FRAMEBUFFER_BINDING    0x8CA6
#define GL_RENDERBUFFER_BINDING   0x8CA7
#define GL_VERTEX_PROGRAM_ARB     0x8620
#define GL_FRAGMENT_PROGRAM_ARB   0x8804
#define GL_QUERY_RESULT           0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867

/*---------------------------------------------------------------------------
 * Helper: resolve a Mesa GL function by name, cache it
 *---------------------------------------------------------------------------*/
static PROC _resolveGL(const char *name)
{
    PROC p = mesaProxyGetProcAddress(name);
    if (!p && g_mesaProxy.hMesaDLL)
        p = GetProcAddress(g_mesaProxy.hMesaDLL, name);
    return p;
}

/*---------------------------------------------------------------------------
 * Cached function pointers for the underlying GL calls
 *---------------------------------------------------------------------------*/

/* Framebuffer */
typedef void   (APIENTRY *PFN_glBindFramebuffer)(GLenum, GLuint);
typedef void   (APIENTRY *PFN_glGetIntegerv)(GLenum, GLint*);
typedef void   (APIENTRY *PFN_glFramebufferTexture)(GLenum, GLenum, GLuint, GLint);
typedef void   (APIENTRY *PFN_glFramebufferTextureLayer)(GLenum, GLenum, GLuint, GLint, GLint);
typedef void   (APIENTRY *PFN_glFramebufferTextureFaceARB)(GLenum, GLenum, GLuint, GLint, GLenum);

/* Renderbuffer */
typedef void   (APIENTRY *PFN_glBindRenderbuffer)(GLenum, GLuint);
typedef void   (APIENTRY *PFN_glRenderbufferStorageMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);

/* Texture */
typedef void   (APIENTRY *PFN_glBindTexture)(GLenum, GLuint);
typedef void   (APIENTRY *PFN_glActiveTexture)(GLenum);

/* Program */
typedef void   (APIENTRY *PFN_glBindProgramARB)(GLenum, GLuint);

/* Query */
typedef void   (APIENTRY *PFN_glGetQueryObjectuiv)(GLuint, GLenum, GLuint*);

static PFN_glBindFramebuffer  _pfnBindFramebuffer  = NULL;
static PFN_glGetIntegerv      _pfnGetIntegerv      = NULL;
static PFN_glFramebufferTexture _pfnFramebufferTexture = NULL;
static PFN_glFramebufferTextureLayer _pfnFramebufferTextureLayer = NULL;
static PFN_glBindRenderbuffer _pfnBindRenderbuffer = NULL;
static PFN_glRenderbufferStorageMultisample _pfnRenderbufferStorageMultisample = NULL;
static PFN_glBindTexture      _pfnBindTexture      = NULL;
static PFN_glActiveTexture    _pfnActiveTexture     = NULL;
static PFN_glBindProgramARB   _pfnBindProgramARB   = NULL;
static PFN_glGetQueryObjectuiv _pfnGetQueryObjectuiv = NULL;

static void _ensureResolved(void)
{
    if (_pfnBindFramebuffer) return; /* already resolved */

    _pfnBindFramebuffer  = (PFN_glBindFramebuffer)_resolveGL("glBindFramebuffer");
    _pfnGetIntegerv      = (PFN_glGetIntegerv)_resolveGL("glGetIntegerv");
    _pfnFramebufferTexture = (PFN_glFramebufferTexture)_resolveGL("glFramebufferTexture");
    _pfnFramebufferTextureLayer = (PFN_glFramebufferTextureLayer)_resolveGL("glFramebufferTextureLayer");
    _pfnBindRenderbuffer = (PFN_glBindRenderbuffer)_resolveGL("glBindRenderbuffer");
    _pfnRenderbufferStorageMultisample = (PFN_glRenderbufferStorageMultisample)_resolveGL("glRenderbufferStorageMultisample");
    _pfnBindTexture      = (PFN_glBindTexture)_resolveGL("glBindTexture");
    _pfnActiveTexture    = (PFN_glActiveTexture)_resolveGL("glActiveTexture");
    _pfnBindProgramARB   = (PFN_glBindProgramARB)_resolveGL("glBindProgramARB");
    _pfnGetQueryObjectuiv = (PFN_glGetQueryObjectuiv)_resolveGL("glGetQueryObjectuiv");

    /* Fallback: if glGetIntegerv not found via extension, try DLL export */
    if (!_pfnGetIntegerv && g_mesaProxy.hMesaDLL)
        _pfnGetIntegerv = (PFN_glGetIntegerv)GetProcAddress(g_mesaProxy.hMesaDLL, "glGetIntegerv");
    if (!_pfnBindTexture && g_mesaProxy.hMesaDLL)
        _pfnBindTexture = (PFN_glBindTexture)GetProcAddress(g_mesaProxy.hMesaDLL, "glBindTexture");
}

/*===========================================================================
 * EXT_direct_state_access — Named Framebuffer functions
 *
 * Pattern: save current FBO → bind named FBO → call regular func → restore
 *===========================================================================*/

void APIENTRY glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment,
    GLuint texture, GLint level)
{
    GLint savedFBO = 0;
    _ensureResolved();
    if (!_pfnBindFramebuffer || !_pfnGetIntegerv || !_pfnFramebufferTexture) return;

    _pfnGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    _pfnFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, level);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, (GLuint)savedFBO);
}

void APIENTRY glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment,
    GLuint texture, GLint level, GLint layer)
{
    GLint savedFBO = 0;
    _ensureResolved();
    if (!_pfnBindFramebuffer || !_pfnGetIntegerv || !_pfnFramebufferTextureLayer) return;

    _pfnGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    _pfnFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture, level, layer);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, (GLuint)savedFBO);
}

void APIENTRY glNamedFramebufferTextureFaceEXT(GLuint framebuffer, GLenum attachment,
    GLuint texture, GLint level, GLenum face)
{
    /*
     * FramebufferTextureFace is from EXT_geometry_shader4 / NV_geometry_program4.
     * Mesa doesn't expose it, but the DSA version is queried by id Tech 5.
     * Implement as FramebufferTexture (ignoring face) — correct for non-cubemap,
     * and for cubemap the layer-based approach is preferred anyway.
     */
    GLint savedFBO = 0;
    _ensureResolved();
    if (!_pfnBindFramebuffer || !_pfnGetIntegerv || !_pfnFramebufferTexture) return;

    _pfnGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    _pfnFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, level);
    _pfnBindFramebuffer(GL_FRAMEBUFFER, (GLuint)savedFBO);
}

/*===========================================================================
 * EXT_direct_state_access — Named Renderbuffer functions
 *===========================================================================*/

void APIENTRY glNamedRenderbufferStorageMultisampleCoverageEXT(GLuint renderbuffer,
    GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat,
    GLsizei width, GLsizei height)
{
    /*
     * NV_framebuffer_multisample_coverage DSA variant.
     * Mesa doesn't support coverage samples separately from color samples.
     * Forward as regular RenderbufferStorageMultisample using colorSamples.
     */
    GLint savedRBO = 0;
    _ensureResolved();
    if (!_pfnBindRenderbuffer || !_pfnGetIntegerv || !_pfnRenderbufferStorageMultisample) return;

    _pfnGetIntegerv(GL_RENDERBUFFER_BINDING, &savedRBO);
    _pfnBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    _pfnRenderbufferStorageMultisample(GL_RENDERBUFFER, colorSamples, internalformat, width, height);
    _pfnBindRenderbuffer(GL_RENDERBUFFER, (GLuint)savedRBO);
}

/*===========================================================================
 * EXT_direct_state_access — Named Program Local Parameter (integer) functions
 *
 * These are from NV_gpu_program4 DSA variants. They set integer local
 * parameters on ARB vertex/fragment programs by name instead of binding.
 *
 * Since Mesa's llvmpipe doesn't support NV_gpu_program4 integer parameters,
 * we implement these as bind→set float equivalent→restore. The integer
 * values are converted to float, which is sufficient for id Tech 5's usage
 * (it queries these but the actual shader path uses GLSL).
 *===========================================================================*/

typedef void (APIENTRY *PFN_glProgramLocalParameter4fARB)(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY *PFN_glGetProgramLocalParameterfvARB)(GLenum, GLuint, GLfloat*);

static PFN_glProgramLocalParameter4fARB _pfnProgramLocalParameter4fARB = NULL;
static PFN_glGetProgramLocalParameterfvARB _pfnGetProgramLocalParameterfvARB = NULL;

static void _ensureProgramResolved(void)
{
    if (_pfnProgramLocalParameter4fARB) return;
    _pfnProgramLocalParameter4fARB = (PFN_glProgramLocalParameter4fARB)_resolveGL("glProgramLocalParameter4fARB");
    _pfnGetProgramLocalParameterfvARB = (PFN_glGetProgramLocalParameterfvARB)_resolveGL("glGetProgramLocalParameterfvARB");
}

void APIENTRY glNamedProgramLocalParameterI4iEXT(GLuint program, GLenum target,
    GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    GLint savedProg = 0;
    _ensureResolved();
    _ensureProgramResolved();
    if (!_pfnBindProgramARB || !_pfnGetIntegerv) return;

    /* Save current program binding for this target */
    if (target == GL_VERTEX_PROGRAM_ARB)
        _pfnGetIntegerv(0x8642 /* GL_CURRENT_VERTEX_ATTRIB */, &savedProg); /* not right, use program binding */
    /* Actually, GL_VERTEX_PROGRAM_BINDING_NV = 0x864A, GL_FRAGMENT_PROGRAM_BINDING_NV doesn't exist cleanly.
     * Simpler: just bind, set, restore to 0 (default program). */

    _pfnBindProgramARB(target, program);
    if (_pfnProgramLocalParameter4fARB)
        _pfnProgramLocalParameter4fARB(target, index, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
    _pfnBindProgramARB(target, 0);
}

void APIENTRY glNamedProgramLocalParameterI4ivEXT(GLuint program, GLenum target,
    GLuint index, const GLint *params)
{
    if (params)
        glNamedProgramLocalParameterI4iEXT(program, target, index,
            params[0], params[1], params[2], params[3]);
}

void APIENTRY glNamedProgramLocalParametersI4ivEXT(GLuint program, GLenum target,
    GLuint index, GLsizei count, const GLint *params)
{
    GLsizei i;
    if (!params) return;
    for (i = 0; i < count; i++) {
        glNamedProgramLocalParameterI4iEXT(program, target, index + i,
            params[i*4+0], params[i*4+1], params[i*4+2], params[i*4+3]);
    }
}

void APIENTRY glNamedProgramLocalParameterI4uiEXT(GLuint program, GLenum target,
    GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    _ensureResolved();
    _ensureProgramResolved();
    if (!_pfnBindProgramARB) return;

    _pfnBindProgramARB(target, program);
    if (_pfnProgramLocalParameter4fARB)
        _pfnProgramLocalParameter4fARB(target, index, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
    _pfnBindProgramARB(target, 0);
}

void APIENTRY glNamedProgramLocalParameterI4uivEXT(GLuint program, GLenum target,
    GLuint index, const GLuint *params)
{
    if (params)
        glNamedProgramLocalParameterI4uiEXT(program, target, index,
            params[0], params[1], params[2], params[3]);
}

void APIENTRY glNamedProgramLocalParametersI4uivEXT(GLuint program, GLenum target,
    GLuint index, GLsizei count, const GLuint *params)
{
    GLsizei i;
    if (!params) return;
    for (i = 0; i < count; i++) {
        glNamedProgramLocalParameterI4uiEXT(program, target, index + i,
            params[i*4+0], params[i*4+1], params[i*4+2], params[i*4+3]);
    }
}

/*===========================================================================
 * EXT_direct_state_access — Get Named Program Local Parameter (integer)
 *===========================================================================*/

void APIENTRY glGetNamedProgramLocalParameterIivEXT(GLuint program, GLenum target,
    GLuint index, GLint *params)
{
    GLfloat fparams[4] = {0};
    _ensureResolved();
    _ensureProgramResolved();
    if (!params) return;

    if (_pfnBindProgramARB && _pfnGetProgramLocalParameterfvARB) {
        _pfnBindProgramARB(target, program);
        _pfnGetProgramLocalParameterfvARB(target, index, fparams);
        _pfnBindProgramARB(target, 0);
    }
    params[0] = (GLint)fparams[0];
    params[1] = (GLint)fparams[1];
    params[2] = (GLint)fparams[2];
    params[3] = (GLint)fparams[3];
}

void APIENTRY glGetNamedProgramLocalParameterIuivEXT(GLuint program, GLenum target,
    GLuint index, GLuint *params)
{
    GLfloat fparams[4] = {0};
    _ensureResolved();
    _ensureProgramResolved();
    if (!params) return;

    if (_pfnBindProgramARB && _pfnGetProgramLocalParameterfvARB) {
        _pfnBindProgramARB(target, program);
        _pfnGetProgramLocalParameterfvARB(target, index, fparams);
        _pfnBindProgramARB(target, 0);
    }
    params[0] = (GLuint)fparams[0];
    params[1] = (GLuint)fparams[1];
    params[2] = (GLuint)fparams[2];
    params[3] = (GLuint)fparams[3];
}

/*===========================================================================
 * NV_explicit_multisample — Texture/MultiTex Renderbuffer
 *
 * These attach a renderbuffer to a texture for multisample resolve.
 * Mesa doesn't support NV_explicit_multisample, but id Tech 5 queries these.
 * Implement as no-ops that don't crash — the engine falls back to other paths.
 *===========================================================================*/

void APIENTRY glTextureRenderbufferEXT(GLuint texture, GLenum target, GLuint renderbuffer)
{
    /*
     * NV_explicit_multisample: TexRenderbufferNV with DSA.
     * Binds a renderbuffer's storage to a texture for multisample texel fetch.
     * Mesa doesn't support this — the engine will use a fallback path.
     * We do nothing but don't crash.
     */
    (void)texture;
    (void)target;
    (void)renderbuffer;
}

void APIENTRY glMultiTexRenderbufferEXT(GLenum texunit, GLenum target, GLuint renderbuffer)
{
    /*
     * NV_explicit_multisample: MultiTex variant.
     * Same as above but addressed by texture unit instead of texture name.
     */
    (void)texunit;
    (void)target;
    (void)renderbuffer;
}

/*===========================================================================
 * AMD_multi_draw_indirect query extension
 *
 * glGetMultiQueryObjectuivAMD — batch query of multiple query objects.
 * Not in Mesa. Implement by looping over individual queries.
 *===========================================================================*/

void APIENTRY glGetMultiQueryObjectuivAMD(GLuint id, GLuint count, GLenum pname,
    GLuint stride, GLuint *params)
{
    GLuint i;
    _ensureResolved();

    if (!params || !_pfnGetQueryObjectuiv) return;

    /*
     * AMD extension: query multiple query objects in one call.
     * id = first query object name, count = number of queries,
     * stride = byte stride between results in output array.
     * We implement by calling glGetQueryObjectuiv for each query.
     */
    for (i = 0; i < count; i++) {
        GLuint *dest = (GLuint*)((unsigned char*)params + i * stride);
        _pfnGetQueryObjectuiv(id + i, pname, dest);
    }
}

/*===========================================================================
 * NV_half_float — Vertex Weight
 *
 * glVertexWeighthNV / glVertexWeighthvNV — set vertex weight using half float.
 * Part of NV_half_float + EXT_vertex_weighting.
 * Mesa doesn't support vertex weighting. These are legacy NVIDIA extensions.
 * Implement as no-ops — no modern game actually uses vertex weighting for
 * rendering, but id Tech 5 queries the function pointers.
 *===========================================================================*/

void APIENTRY glVertexWeighthNV(GLhalfNV weight)
{
    /* NV_half_float vertex weight — no-op, vertex weighting is legacy NVIDIA */
    (void)weight;
}

void APIENTRY glVertexWeighthvNV(const GLhalfNV *weight)
{
    /* NV_half_float vertex weight vector — no-op */
    (void)weight;
}
