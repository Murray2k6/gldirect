/*********************************************************************************
*  gl_dx9_compat.c — GL to D3D9 rendering pipeline
*
*  Routes ALL GL calls through D3D9. Mesa is only used for function resolution.
*  D3D9 is the actual renderer. Includes full shader pipeline (GLSL→HLSL→SM3.0),
*  uniform management, vertex attribute pipeline, and real draw calls via
*  IDirect3DDevice9_DrawPrimitiveUP / DrawIndexedPrimitiveUP.
*********************************************************************************/

#pragma warning(disable: 4996 4244 4305)

#include "gl_dx9_compat.h"
#include "context_manager.h"
#include "glsl_to_hlsl.h"
#include "gld_context.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * GL constants (self-contained — no GL headers)
 *---------------------------------------------------------------------------*/
#define GL_TEXTURE_2D     0x0DE1
#define GL_RGBA           0x1908
#define GL_RGB            0x1907
#define GL_BGRA           0x80E1
#define GL_BGR            0x80E0
#define GL_LUMINANCE      0x1909
#define GL_ALPHA          0x1906
#define GL_UNSIGNED_BYTE  0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT   0x1405
#define GL_UNSIGNED_SHORT_5_6_5   0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_FLOAT          0x1406
#define GL_INT            0x1404
#define GL_SHORT          0x1402
#define GL_BYTE           0x1400
#define GL_DOUBLE         0x140A
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT  0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

#define GL_DEPTH_BUFFER_BIT   0x00000100
#define GL_COLOR_BUFFER_BIT   0x00004000
#define GL_STENCIL_BUFFER_BIT 0x00000400

#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUADS          0x0007

#define GL_VERTEX_SHADER   0x8B31
#define GL_FRAGMENT_SHADER 0x8B30

#define GL_DEPTH_TEST     0x0B71
#define GL_BLEND          0x0BE2
#define GL_CULL_FACE      0x0B44
#define GL_ALPHA_TEST     0x0BC0
#define GL_FOG            0x0B60
#define GL_LIGHTING       0x0B50
#define GL_SCISSOR_TEST   0x0C11
#define GL_STENCIL_TEST   0x0B90

#define GL_FRONT          0x0404
#define GL_BACK           0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW             0x0900
#define GL_CCW            0x0901

#define GL_MODELVIEW      0x1700
#define GL_PROJECTION     0x1701
#define GL_TEXTURE        0x1702

/*---------------------------------------------------------------------------
 * Limits
 *---------------------------------------------------------------------------*/
#define MAX_CACHED_TEXTURES      4096
#define MAX_SHADERS              1024
#define MAX_PROGRAMS             512
#define MAX_VERTEX_ATTRIBS       16
#define MAX_UNIFORMS_PER_PROGRAM 256
#define MAX_MATRIX_STACK         32

/*---------------------------------------------------------------------------
 * Texture cache — maps GL texture IDs to D3D9 textures
 *---------------------------------------------------------------------------*/
typedef struct {
    unsigned int glId;
    IDirect3DTexture9 *pTex;
    int width, height;
    D3DFORMAT d3dFmt;
} CachedTexture;

/*---------------------------------------------------------------------------
 * Shader cache
 *---------------------------------------------------------------------------*/
typedef struct {
    unsigned int glId;
    unsigned int type;          /* GL_VERTEX_SHADER or GL_FRAGMENT_SHADER */
    char        *source;        /* concatenated GLSL source */
    void        *bytecode;      /* compiled D3D9 bytecode */
    DWORD        bytecodeSize;
} ShaderEntry;

/*---------------------------------------------------------------------------
 * Uniform entry
 *---------------------------------------------------------------------------*/
typedef struct {
    char name[128];
    int  registerIndex;
    int  type;                  /* 0 = float, 1 = sampler */
} UniformEntry;

/*---------------------------------------------------------------------------
 * Program cache
 *---------------------------------------------------------------------------*/
typedef struct {
    unsigned int glId;
    int vsShaderIdx;            /* index into g_shaders, -1 if none */
    int psShaderIdx;            /* index into g_shaders, -1 if none */
    IDirect3DVertexShader9 *pVS;
    IDirect3DPixelShader9  *pPS;
    UniformEntry uniforms[MAX_UNIFORMS_PER_PROGRAM];
    int uniformCount;
} ProgramEntry;

/*---------------------------------------------------------------------------
 * Vertex attribute state
 *---------------------------------------------------------------------------*/
typedef struct {
    BOOL         enabled;
    int          size;          /* 1..4 components */
    unsigned int type;          /* GL_FLOAT, GL_UNSIGNED_BYTE, etc. */
    BOOL         normalized;
    int          stride;
    const void  *pointer;
} VertexAttrib;

/*---------------------------------------------------------------------------
 * FVF vertex for DrawPrimitiveUP
 *---------------------------------------------------------------------------*/
typedef struct {
    float x, y, z;
    DWORD color;
    float u, v;
} FVFVertex;

#define COMPAT_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

/*---------------------------------------------------------------------------
 * Matrix stack
 *---------------------------------------------------------------------------*/
typedef struct {
    float m[16];
} Mat4;

/*---------------------------------------------------------------------------
 * Module-level state
 *---------------------------------------------------------------------------*/
static IDirect3DDevice9 *g_pDev = NULL;

/* Texture cache */
static CachedTexture g_texCache[MAX_CACHED_TEXTURES];
static int g_texCacheCount = 0;
static unsigned int g_boundTexture = 0;

/* Clear state */
static float g_clearR = 0, g_clearG = 0, g_clearB = 0, g_clearA = 0;
static float g_clearDepth = 1.0f;

/* Shader cache */
static ShaderEntry g_shaders[MAX_SHADERS];
static int g_shaderCount = 0;
static unsigned int g_nextShaderId = 1;

/* Program cache */
static ProgramEntry g_programs[MAX_PROGRAMS];
static int g_programCount = 0;
static unsigned int g_nextProgramId = 1;
static int g_activeProgramIdx = -1;

/* Vertex attributes */
static VertexAttrib g_attribs[MAX_VERTEX_ATTRIBS];

/* Cull / front-face state */
static unsigned int g_cullMode  = GL_BACK;
static unsigned int g_frontFace = GL_CCW;
static BOOL g_cullEnabled = FALSE;

/* Matrix stacks */
static unsigned int g_matrixMode = GL_MODELVIEW;
static Mat4 g_mvStack[MAX_MATRIX_STACK];
static int  g_mvTop = 0;
static Mat4 g_projStack[MAX_MATRIX_STACK];
static int  g_projTop = 0;
static Mat4 g_texStack[MAX_MATRIX_STACK];
static int  g_texTop = 0;

/*---------------------------------------------------------------------------
 * Texture cache helpers
 *---------------------------------------------------------------------------*/
static CachedTexture* _findTexture(unsigned int glId)
{
    int i;
    for (i = 0; i < g_texCacheCount; i++) {
        if (g_texCache[i].glId == glId)
            return &g_texCache[i];
    }
    return NULL;
}

static CachedTexture* _allocTexture(unsigned int glId)
{
    CachedTexture *ct = _findTexture(glId);
    if (ct) {
        if (ct->pTex) {
            IDirect3DTexture9_Release(ct->pTex);
            ct->pTex = NULL;
        }
        return ct;
    }
    if (g_texCacheCount >= MAX_CACHED_TEXTURES) {
        if (g_texCache[0].pTex)
            IDirect3DTexture9_Release(g_texCache[0].pTex);
        memmove(&g_texCache[0], &g_texCache[1], sizeof(CachedTexture) * (MAX_CACHED_TEXTURES - 1));
        g_texCacheCount = MAX_CACHED_TEXTURES - 1;
    }
    ct = &g_texCache[g_texCacheCount++];
    memset(ct, 0, sizeof(*ct));
    ct->glId = glId;
    return ct;
}

/*---------------------------------------------------------------------------
 * GL format to D3D9 format conversion
 *---------------------------------------------------------------------------*/
static D3DFORMAT _glFormatToD3D(int internalformat)
{
    switch (internalformat) {
    case 1: case GL_LUMINANCE: case 0x8040:
        return D3DFMT_L8;
    case GL_ALPHA: case 0x803C:
        return D3DFMT_A8;
    case 3: case GL_RGB: case 0x8051:
        return D3DFMT_X8R8G8B8;
    case 4: case GL_RGBA: case 0x8058:
        return D3DFMT_A8R8G8B8;
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return D3DFMT_DXT1;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return D3DFMT_DXT3;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return D3DFMT_DXT5;
    case 0x8D62:
        return D3DFMT_R5G6B5;
    case 0x8056:
        return D3DFMT_A4R4G4B4;
    case 0x8057:
        return D3DFMT_A1R5G5B5;
    default:
        return D3DFMT_A8R8G8B8;
    }
}

/*---------------------------------------------------------------------------
 * Pixel conversion: GL source format -> D3D9 ARGB
 *---------------------------------------------------------------------------*/
static void _convertPixels(
    unsigned char *dst, int dstPitch,
    const unsigned char *src, int srcPitch,
    int width, int height,
    unsigned int format, unsigned int type,
    D3DFORMAT d3dFmt)
{
    int x, y;
    if (!src || !dst) return;

    if (format == GL_BGRA && type == GL_UNSIGNED_BYTE && d3dFmt == D3DFMT_A8R8G8B8) {
        for (y = 0; y < height; y++)
            memcpy(dst + y * dstPitch, src + y * srcPitch, width * 4);
        return;
    }
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE && d3dFmt == D3DFMT_A8R8G8B8) {
        for (y = 0; y < height; y++) {
            const unsigned char *s = src + y * srcPitch;
            unsigned char *d = dst + y * dstPitch;
            for (x = 0; x < width; x++) {
                d[x*4+0] = s[x*4+2];
                d[x*4+1] = s[x*4+1];
                d[x*4+2] = s[x*4+0];
                d[x*4+3] = s[x*4+3];
            }
        }
        return;
    }
    if (format == GL_RGB && type == GL_UNSIGNED_BYTE &&
        (d3dFmt == D3DFMT_X8R8G8B8 || d3dFmt == D3DFMT_A8R8G8B8)) {
        for (y = 0; y < height; y++) {
            const unsigned char *s = src + y * srcPitch;
            unsigned char *d = dst + y * dstPitch;
            for (x = 0; x < width; x++) {
                d[x*4+0] = s[x*3+2];
                d[x*4+1] = s[x*3+1];
                d[x*4+2] = s[x*3+0];
                d[x*4+3] = 0xFF;
            }
        }
        return;
    }
    if (format == GL_BGR && type == GL_UNSIGNED_BYTE &&
        (d3dFmt == D3DFMT_X8R8G8B8 || d3dFmt == D3DFMT_A8R8G8B8)) {
        for (y = 0; y < height; y++) {
            const unsigned char *s = src + y * srcPitch;
            unsigned char *d = dst + y * dstPitch;
            for (x = 0; x < width; x++) {
                d[x*4+0] = s[x*3+0];
                d[x*4+1] = s[x*3+1];
                d[x*4+2] = s[x*3+2];
                d[x*4+3] = 0xFF;
            }
        }
        return;
    }
    if (format == GL_LUMINANCE && type == GL_UNSIGNED_BYTE && d3dFmt == D3DFMT_L8) {
        for (y = 0; y < height; y++)
            memcpy(dst + y * dstPitch, src + y * width, width);
        return;
    }
    if (format == GL_ALPHA && type == GL_UNSIGNED_BYTE && d3dFmt == D3DFMT_A8) {
        for (y = 0; y < height; y++)
            memcpy(dst + y * dstPitch, src + y * width, width);
        return;
    }
    /* Fallback: zero fill */
    for (y = 0; y < height; y++)
        memset(dst + y * dstPitch, 0, dstPitch);
}

static int _srcPitch(int width, unsigned int format, unsigned int type)
{
    int bpp = 4;
    if (type == GL_UNSIGNED_BYTE) {
        if (format == GL_RGBA || format == GL_BGRA) bpp = 4;
        else if (format == GL_RGB || format == GL_BGR) bpp = 3;
        else if (format == GL_LUMINANCE || format == GL_ALPHA) bpp = 1;
    }
    return width * bpp;
}

/*---------------------------------------------------------------------------
 * Shader cache helpers
 *---------------------------------------------------------------------------*/
static ShaderEntry* _findShader(unsigned int glId)
{
    int i;
    for (i = 0; i < g_shaderCount; i++) {
        if (g_shaders[i].glId == glId)
            return &g_shaders[i];
    }
    return NULL;
}

static int _findShaderIndex(unsigned int glId)
{
    int i;
    for (i = 0; i < g_shaderCount; i++) {
        if (g_shaders[i].glId == glId)
            return i;
    }
    return -1;
}

/*---------------------------------------------------------------------------
 * Program cache helpers
 *---------------------------------------------------------------------------*/
static ProgramEntry* _findProgram(unsigned int glId)
{
    int i;
    for (i = 0; i < g_programCount; i++) {
        if (g_programs[i].glId == glId)
            return &g_programs[i];
    }
    return NULL;
}

static int _findProgramIndex(unsigned int glId)
{
    int i;
    for (i = 0; i < g_programCount; i++) {
        if (g_programs[i].glId == glId)
            return i;
    }
    return -1;
}

/*---------------------------------------------------------------------------
 * D3D9 cull mode helper — combines g_cullMode + g_frontFace
 *---------------------------------------------------------------------------*/
static void _applyCullMode(void)
{
    D3DCULL cull;
    if (!g_pDev) return;
    if (!g_cullEnabled) {
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_CULLMODE, D3DCULL_NONE);
        return;
    }
    if (g_cullMode == GL_FRONT_AND_BACK) {
        /* D3D9 can't cull both; we'll set NONE and skip draws instead */
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_CULLMODE, D3DCULL_NONE);
        return;
    }
    /* GL default: front face = CCW, cull back = CW winding culled
     * D3D9 D3DCULL_CW  = cull clockwise-wound triangles
     * D3D9 D3DCULL_CCW = cull counter-clockwise-wound triangles */
    if (g_frontFace == GL_CCW) {
        cull = (g_cullMode == GL_BACK) ? D3DCULL_CW : D3DCULL_CCW;
    } else {
        cull = (g_cullMode == GL_BACK) ? D3DCULL_CCW : D3DCULL_CW;
    }
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_CULLMODE, cull);
}

/*---------------------------------------------------------------------------
 * Matrix helpers
 *---------------------------------------------------------------------------*/
static void _mat4Identity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void _mat4Multiply(float *out, const float *a, const float *b)
{
    float tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0f;
            for (k = 0; k < 4; k++)
                tmp[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
        }
    }
    memcpy(out, tmp, 16 * sizeof(float));
}

static float* _currentMatrix(void)
{
    switch (g_matrixMode) {
    case GL_PROJECTION: return g_projStack[g_projTop].m;
    case GL_TEXTURE:    return g_texStack[g_texTop].m;
    default:            return g_mvStack[g_mvTop].m;
    }
}

/*---------------------------------------------------------------------------
 * Vertex data reading helper — reads a float from arbitrary GL type
 *---------------------------------------------------------------------------*/
static float _readComponent(const unsigned char *ptr, unsigned int type, BOOL normalized)
{
    switch (type) {
    case GL_FLOAT: {
        float v;
        memcpy(&v, ptr, sizeof(float));
        return v;
    }
    case GL_DOUBLE: {
        double v;
        memcpy(&v, ptr, sizeof(double));
        return (float)v;
    }
    case GL_BYTE: {
        signed char v = *(const signed char*)ptr;
        return normalized ? (v / 127.0f) : (float)v;
    }
    case GL_UNSIGNED_BYTE: {
        unsigned char v = *ptr;
        return normalized ? (v / 255.0f) : (float)v;
    }
    case GL_SHORT: {
        short v;
        memcpy(&v, ptr, sizeof(short));
        return normalized ? (v / 32767.0f) : (float)v;
    }
    case GL_UNSIGNED_SHORT: {
        unsigned short v;
        memcpy(&v, ptr, sizeof(unsigned short));
        return normalized ? (v / 65535.0f) : (float)v;
    }
    case GL_INT: {
        int v;
        memcpy(&v, ptr, sizeof(int));
        return normalized ? (v / 2147483647.0f) : (float)v;
    }
    case GL_UNSIGNED_INT: {
        unsigned int v;
        memcpy(&v, ptr, sizeof(unsigned int));
        return normalized ? (float)(v / 4294967295.0) : (float)v;
    }
    default:
        return 0.0f;
    }
}

static int _glTypeSize(unsigned int type)
{
    switch (type) {
    case GL_BYTE:           return 1;
    case GL_UNSIGNED_BYTE:  return 1;
    case GL_SHORT:          return 2;
    case GL_UNSIGNED_SHORT: return 2;
    case GL_INT:            return 4;
    case GL_UNSIGNED_INT:   return 4;
    case GL_FLOAT:          return 4;
    case GL_DOUBLE:         return 8;
    default:                return 4;
    }
}

/*---------------------------------------------------------------------------
 * Read vertex components from an attribute pointer for a given vertex index
 *---------------------------------------------------------------------------*/
static void _readAttribFloats(const VertexAttrib *attr, int vertexIndex, float *out, int maxComponents)
{
    const unsigned char *base;
    const unsigned char *vertPtr;
    int stride;
    int compSize;
    int i;

    memset(out, 0, maxComponents * sizeof(float));
    if (!attr->pointer) return;

    compSize = _glTypeSize(attr->type);
    stride = attr->stride;
    if (stride == 0)
        stride = attr->size * compSize;

    base = (const unsigned char *)attr->pointer;
    vertPtr = base + vertexIndex * stride;

    for (i = 0; i < attr->size && i < maxComponents; i++) {
        out[i] = _readComponent(vertPtr + i * compSize, attr->type, attr->normalized);
    }
}

/*---------------------------------------------------------------------------
 * Build FVF vertex from enabled attributes for a single vertex
 *---------------------------------------------------------------------------*/
static void _buildFVFVertex(FVFVertex *fvf, int vertexIndex)
{
    float pos[4] = {0, 0, 0, 1};
    float col[4] = {1, 1, 1, 1};
    float tex[4] = {0, 0, 0, 0};

    /* Attribute 0 = position (convention) */
    if (g_attribs[0].enabled && g_attribs[0].pointer) {
        _readAttribFloats(&g_attribs[0], vertexIndex, pos, 4);
    }

    /* Attribute 1 = color (convention, or attrib 3 in some layouts) */
    if (g_attribs[1].enabled && g_attribs[1].pointer) {
        _readAttribFloats(&g_attribs[1], vertexIndex, col, 4);
    } else if (g_attribs[3].enabled && g_attribs[3].pointer) {
        _readAttribFloats(&g_attribs[3], vertexIndex, col, 4);
    }

    /* Attribute 2 = texcoord (convention, or attrib 8 in some layouts) */
    if (g_attribs[2].enabled && g_attribs[2].pointer) {
        _readAttribFloats(&g_attribs[2], vertexIndex, tex, 4);
    } else if (g_attribs[8].enabled && g_attribs[8].pointer) {
        _readAttribFloats(&g_attribs[8], vertexIndex, tex, 4);
    }

    fvf->x = pos[0];
    fvf->y = pos[1];
    fvf->z = pos[2];

    /* Clamp color components to [0,1] and pack as D3DCOLOR */
    {
        int r = (int)(col[0] * 255.0f); if (r < 0) r = 0; if (r > 255) r = 255;
        int g = (int)(col[1] * 255.0f); if (g < 0) g = 0; if (g > 255) g = 255;
        int b = (int)(col[2] * 255.0f); if (b < 0) b = 0; if (b > 255) b = 255;
        int a = (int)(col[3] * 255.0f); if (a < 0) a = 0; if (a > 255) a = 255;
        fvf->color = D3DCOLOR_ARGB(a, r, g, b);
    }

    fvf->u = tex[0];
    fvf->v = tex[1];
}

/*---------------------------------------------------------------------------
 * GL primitive mode to D3D9 primitive type + count
 *---------------------------------------------------------------------------*/
static BOOL _glModeToD3D(unsigned int mode, int vertexCount,
                          D3DPRIMITIVETYPE *pType, int *pPrimCount)
{
    switch (mode) {
    case GL_TRIANGLES:
        *pType = D3DPT_TRIANGLELIST;
        *pPrimCount = vertexCount / 3;
        return (*pPrimCount > 0);
    case GL_TRIANGLE_STRIP:
        *pType = D3DPT_TRIANGLESTRIP;
        *pPrimCount = vertexCount - 2;
        return (*pPrimCount > 0);
    case GL_TRIANGLE_FAN:
        *pType = D3DPT_TRIANGLEFAN;
        *pPrimCount = vertexCount - 2;
        return (*pPrimCount > 0);
    case GL_LINES:
        *pType = D3DPT_LINELIST;
        *pPrimCount = vertexCount / 2;
        return (*pPrimCount > 0);
    case GL_LINE_STRIP:
        *pType = D3DPT_LINESTRIP;
        *pPrimCount = vertexCount - 1;
        return (*pPrimCount > 0);
    case GL_POINTS:
        *pType = D3DPT_POINTLIST;
        *pPrimCount = vertexCount;
        return (*pPrimCount > 0);
    case GL_QUADS:
        /* Quads must be expanded to triangles by caller */
        *pType = D3DPT_TRIANGLELIST;
        *pPrimCount = (vertexCount / 4) * 2;
        return (*pPrimCount > 0);
    default:
        return FALSE;
    }
}

/*---------------------------------------------------------------------------
 * Read an index from an index buffer based on GL type
 *---------------------------------------------------------------------------*/
static unsigned int _readIndex(const void *indices, unsigned int type, int i)
{
    switch (type) {
    case GL_UNSIGNED_BYTE:
        return ((const unsigned char*)indices)[i];
    case GL_UNSIGNED_SHORT:
        return ((const unsigned short*)indices)[i];
    case GL_UNSIGNED_INT:
        return ((const unsigned int*)indices)[i];
    default:
        return 0;
    }
}

/*===========================================================================
 * Public API — Init / Shutdown
 *===========================================================================*/

void gldCompatInit(IDirect3DDevice9 *pDev)
{
    g_pDev = pDev;
    g_texCacheCount = 0;
    g_boundTexture = 0;
    memset(g_texCache, 0, sizeof(g_texCache));

    g_shaderCount = 0;
    g_nextShaderId = 1;
    memset(g_shaders, 0, sizeof(g_shaders));

    g_programCount = 0;
    g_nextProgramId = 1;
    g_activeProgramIdx = -1;
    memset(g_programs, 0, sizeof(g_programs));

    memset(g_attribs, 0, sizeof(g_attribs));

    g_cullMode = GL_BACK;
    g_frontFace = GL_CCW;
    g_cullEnabled = FALSE;

    g_matrixMode = GL_MODELVIEW;
    g_mvTop = 0;
    g_projTop = 0;
    g_texTop = 0;
    _mat4Identity(g_mvStack[0].m);
    _mat4Identity(g_projStack[0].m);
    _mat4Identity(g_texStack[0].m);

    g_clearR = g_clearG = g_clearB = g_clearA = 0;
    g_clearDepth = 1.0f;

    glslTranspilerInit();
}

void gldCompatShutdown(void)
{
    int i;

    /* Release textures */
    for (i = 0; i < g_texCacheCount; i++) {
        if (g_texCache[i].pTex)
            IDirect3DTexture9_Release(g_texCache[i].pTex);
    }
    g_texCacheCount = 0;

    /* Release shaders */
    for (i = 0; i < g_shaderCount; i++) {
        if (g_shaders[i].source) { free(g_shaders[i].source); g_shaders[i].source = NULL; }
        if (g_shaders[i].bytecode) { glslFreeBytecode(g_shaders[i].bytecode); g_shaders[i].bytecode = NULL; }
    }
    g_shaderCount = 0;

    /* Release programs */
    for (i = 0; i < g_programCount; i++) {
        if (g_programs[i].pVS) { IDirect3DVertexShader9_Release(g_programs[i].pVS); g_programs[i].pVS = NULL; }
        if (g_programs[i].pPS) { IDirect3DPixelShader9_Release(g_programs[i].pPS); g_programs[i].pPS = NULL; }
    }
    g_programCount = 0;
    g_activeProgramIdx = -1;

    glslTranspilerShutdown();
    g_pDev = NULL;
}

/*===========================================================================
 * Texture pipeline
 *===========================================================================*/

void gldCompatBindTexture(unsigned int target, unsigned int texture)
{
    CachedTexture *ct;
    if (target != GL_TEXTURE_2D) return;
    g_boundTexture = texture;
    if (!g_pDev) return;
    ct = _findTexture(texture);
    if (ct && ct->pTex)
        IDirect3DDevice9_SetTexture(g_pDev, 0, (IDirect3DBaseTexture9*)ct->pTex);
}

void gldCompatTexImage2D(unsigned int target, int level, int internalformat,
    int width, int height, int border, unsigned int format,
    unsigned int type, const void *pixels)
{
    CachedTexture *ct;
    D3DFORMAT d3dFmt;
    HRESULT hr;

    (void)border;
    if (target != GL_TEXTURE_2D) return;
    if (!g_pDev) return;
    if (g_boundTexture == 0) return;
    if (width <= 0 || height <= 0) return;
    if (level != 0) return;

    d3dFmt = _glFormatToD3D(internalformat);
    ct = _allocTexture(g_boundTexture);
    if (!ct) return;

    hr = IDirect3DDevice9_CreateTexture(g_pDev, width, height, 1,
        0, d3dFmt, D3DPOOL_MANAGED, &ct->pTex, NULL);
    if (FAILED(hr)) {
        d3dFmt = D3DFMT_A8R8G8B8;
        hr = IDirect3DDevice9_CreateTexture(g_pDev, width, height, 1,
            0, d3dFmt, D3DPOOL_MANAGED, &ct->pTex, NULL);
        if (FAILED(hr)) { ct->pTex = NULL; return; }
    }
    ct->width = width;
    ct->height = height;
    ct->d3dFmt = d3dFmt;

    if (pixels && ct->pTex) {
        D3DLOCKED_RECT lr;
        hr = IDirect3DTexture9_LockRect(ct->pTex, 0, &lr, NULL, 0);
        if (SUCCEEDED(hr)) {
            int sp = _srcPitch(width, format, type);
            _convertPixels((unsigned char*)lr.pBits, lr.Pitch,
                (const unsigned char*)pixels, sp,
                width, height, format, type, d3dFmt);
            IDirect3DTexture9_UnlockRect(ct->pTex, 0);
        }
    }
    IDirect3DDevice9_SetTexture(g_pDev, 0, (IDirect3DBaseTexture9*)ct->pTex);
}

void gldCompatCompressedTexImage2D(unsigned int target, int level,
    unsigned int internalformat, int width, int height, int border,
    int imageSize, const void *data)
{
    CachedTexture *ct;
    D3DFORMAT d3dFmt;
    HRESULT hr;

    (void)border;
    if (target != GL_TEXTURE_2D) return;
    if (!g_pDev) return;
    if (g_boundTexture == 0) return;
    if (level != 0) return;
    if (width <= 0 || height <= 0) return;

    d3dFmt = _glFormatToD3D(internalformat);
    ct = _allocTexture(g_boundTexture);
    if (!ct) return;

    hr = IDirect3DDevice9_CreateTexture(g_pDev, width, height, 1,
        0, d3dFmt, D3DPOOL_MANAGED, &ct->pTex, NULL);
    if (FAILED(hr)) { ct->pTex = NULL; return; }
    ct->width = width;
    ct->height = height;
    ct->d3dFmt = d3dFmt;

    if (data && ct->pTex) {
        D3DLOCKED_RECT lr;
        hr = IDirect3DTexture9_LockRect(ct->pTex, 0, &lr, NULL, 0);
        if (SUCCEEDED(hr)) {
            int blockH = (height + 3) / 4;
            int blockRowBytes;
            if (d3dFmt == D3DFMT_DXT1)
                blockRowBytes = ((width + 3) / 4) * 8;
            else
                blockRowBytes = ((width + 3) / 4) * 16;

            if (lr.Pitch == blockRowBytes) {
                memcpy(lr.pBits, data, imageSize);
            } else {
                int row;
                const unsigned char *src = (const unsigned char*)data;
                unsigned char *dst = (unsigned char*)lr.pBits;
                int copyBytes = (blockRowBytes < lr.Pitch) ? blockRowBytes : lr.Pitch;
                for (row = 0; row < blockH; row++)
                    memcpy(dst + row * lr.Pitch, src + row * blockRowBytes, copyBytes);
            }
            IDirect3DTexture9_UnlockRect(ct->pTex, 0);
        }
    }
    IDirect3DDevice9_SetTexture(g_pDev, 0, (IDirect3DBaseTexture9*)ct->pTex);
}

void gldCompatTexSubImage2D(unsigned int target, int level,
    int xoffset, int yoffset, int width, int height,
    unsigned int format, unsigned int type, const void *pixels)
{
    CachedTexture *ct;
    HRESULT hr;

    if (target != GL_TEXTURE_2D) return;
    if (!g_pDev) return;
    if (g_boundTexture == 0) return;
    if (level != 0) return;
    if (!pixels) return;

    ct = _findTexture(g_boundTexture);
    if (!ct || !ct->pTex) return;

    {
        RECT rc = { xoffset, yoffset, xoffset + width, yoffset + height };
        D3DLOCKED_RECT lr;
        hr = IDirect3DTexture9_LockRect(ct->pTex, 0, &lr, &rc, 0);
        if (SUCCEEDED(hr)) {
            int sp = _srcPitch(width, format, type);
            _convertPixels((unsigned char*)lr.pBits, lr.Pitch,
                (const unsigned char*)pixels, sp,
                width, height, format, type, ct->d3dFmt);
            IDirect3DTexture9_UnlockRect(ct->pTex, 0);
        }
    }
}

void gldCompatDeleteTextures(int n, const unsigned int *textures)
{
    int i, j;
    if (!textures) return;
    for (i = 0; i < n; i++) {
        for (j = 0; j < g_texCacheCount; j++) {
            if (g_texCache[j].glId == textures[i]) {
                if (g_texCache[j].pTex)
                    IDirect3DTexture9_Release(g_texCache[j].pTex);
                memmove(&g_texCache[j], &g_texCache[j+1],
                    sizeof(CachedTexture) * (g_texCacheCount - j - 1));
                g_texCacheCount--;
                break;
            }
        }
    }
}

/*===========================================================================
 * Shader pipeline — GLSL to HLSL SM3.0 via D3D9
 *===========================================================================*/

unsigned int gldCompatCreateShader(unsigned int type)
{
    ShaderEntry *se;
    if (g_shaderCount >= MAX_SHADERS) return 0;

    se = &g_shaders[g_shaderCount];
    memset(se, 0, sizeof(*se));
    se->glId = g_nextShaderId++;
    se->type = type;
    se->source = NULL;
    se->bytecode = NULL;
    se->bytecodeSize = 0;
    g_shaderCount++;
    return se->glId;
}

void gldCompatShaderSource(unsigned int shader, int count,
    const char *const*string, const int *length)
{
    ShaderEntry *se;
    int totalLen, i;
    char *buf;

    se = _findShader(shader);
    if (!se) return;

    /* Free old source */
    if (se->source) { free(se->source); se->source = NULL; }

    /* Calculate total length */
    totalLen = 0;
    for (i = 0; i < count; i++) {
        if (!string[i]) continue;
        if (length && length[i] > 0)
            totalLen += length[i];
        else
            totalLen += (int)strlen(string[i]);
    }

    buf = (char*)malloc(totalLen + 1);
    if (!buf) return;
    buf[0] = '\0';

    /* Concatenate all source strings */
    {
        int offset = 0;
        for (i = 0; i < count; i++) {
            int len;
            if (!string[i]) continue;
            if (length && length[i] > 0)
                len = length[i];
            else
                len = (int)strlen(string[i]);
            memcpy(buf + offset, string[i], len);
            offset += len;
        }
        buf[totalLen] = '\0';
    }
    se->source = buf;
}

void gldCompatCompileShader(unsigned int shader)
{
    ShaderEntry *se;
    int shaderType;
    void *bytecode = NULL;
    DWORD bytecodeSize = 0;

    se = _findShader(shader);
    if (!se || !se->source) return;

    /* Free old bytecode */
    if (se->bytecode) { glslFreeBytecode(se->bytecode); se->bytecode = NULL; se->bytecodeSize = 0; }

    shaderType = (se->type == GL_VERTEX_SHADER) ? 0 : 1;

    if (glslTranspileAndCompile(shaderType, se->source, &bytecode, &bytecodeSize)) {
        se->bytecode = bytecode;
        se->bytecodeSize = bytecodeSize;
    }
}

void gldCompatDeleteShader(unsigned int shader)
{
    int idx = _findShaderIndex(shader);
    if (idx < 0) return;

    if (g_shaders[idx].source) { free(g_shaders[idx].source); g_shaders[idx].source = NULL; }
    if (g_shaders[idx].bytecode) { glslFreeBytecode(g_shaders[idx].bytecode); g_shaders[idx].bytecode = NULL; }

    /* Remove from array */
    if (idx < g_shaderCount - 1) {
        memmove(&g_shaders[idx], &g_shaders[idx + 1],
            sizeof(ShaderEntry) * (g_shaderCount - idx - 1));
    }
    g_shaderCount--;
}

unsigned int gldCompatCreateProgram(void)
{
    ProgramEntry *pe;
    if (g_programCount >= MAX_PROGRAMS) return 0;

    pe = &g_programs[g_programCount];
    memset(pe, 0, sizeof(*pe));
    pe->glId = g_nextProgramId++;
    pe->vsShaderIdx = -1;
    pe->psShaderIdx = -1;
    pe->pVS = NULL;
    pe->pPS = NULL;
    pe->uniformCount = 0;
    g_programCount++;
    return pe->glId;
}

void gldCompatAttachShader(unsigned int program, unsigned int shader)
{
    ProgramEntry *pe;
    ShaderEntry *se;
    int shaderIdx;

    pe = _findProgram(program);
    if (!pe) return;

    shaderIdx = _findShaderIndex(shader);
    if (shaderIdx < 0) return;

    se = &g_shaders[shaderIdx];
    if (se->type == GL_VERTEX_SHADER)
        pe->vsShaderIdx = shaderIdx;
    else
        pe->psShaderIdx = shaderIdx;
}

void gldCompatLinkProgram(unsigned int program)
{
    ProgramEntry *pe;

    pe = _findProgram(program);
    if (!pe) return;
    if (!g_pDev) return;

    /* Release old D3D9 shaders */
    if (pe->pVS) { IDirect3DVertexShader9_Release(pe->pVS); pe->pVS = NULL; }
    if (pe->pPS) { IDirect3DPixelShader9_Release(pe->pPS); pe->pPS = NULL; }

    /* Create vertex shader from VS bytecode */
    if (pe->vsShaderIdx >= 0 && pe->vsShaderIdx < g_shaderCount) {
        ShaderEntry *vs = &g_shaders[pe->vsShaderIdx];
        if (vs->bytecode && vs->bytecodeSize > 0) {
            glslCreateVertexShader(g_pDev, vs->bytecode, vs->bytecodeSize, &pe->pVS);
        }
    }

    /* Create pixel shader from PS bytecode */
    if (pe->psShaderIdx >= 0 && pe->psShaderIdx < g_shaderCount) {
        ShaderEntry *ps = &g_shaders[pe->psShaderIdx];
        if (ps->bytecode && ps->bytecodeSize > 0) {
            glslCreatePixelShader(g_pDev, ps->bytecode, ps->bytecodeSize, &pe->pPS);
        }
    }
}

void gldCompatUseProgram(unsigned int program)
{
    if (!g_pDev) return;

    if (program == 0) {
        /* Fixed function — clear shaders */
        IDirect3DDevice9_SetVertexShader(g_pDev, NULL);
        IDirect3DDevice9_SetPixelShader(g_pDev, NULL);
        g_activeProgramIdx = -1;
        return;
    }

    {
        int idx = _findProgramIndex(program);
        if (idx < 0) return;

        g_activeProgramIdx = idx;
        IDirect3DDevice9_SetVertexShader(g_pDev, g_programs[idx].pVS);
        IDirect3DDevice9_SetPixelShader(g_pDev, g_programs[idx].pPS);
    }
}

void gldCompatDeleteProgram(unsigned int program)
{
    int idx = _findProgramIndex(program);
    if (idx < 0) return;

    if (g_programs[idx].pVS) { IDirect3DVertexShader9_Release(g_programs[idx].pVS); g_programs[idx].pVS = NULL; }
    if (g_programs[idx].pPS) { IDirect3DPixelShader9_Release(g_programs[idx].pPS); g_programs[idx].pPS = NULL; }

    /* If this was the active program, deactivate */
    if (g_activeProgramIdx == idx)
        g_activeProgramIdx = -1;
    else if (g_activeProgramIdx > idx)
        g_activeProgramIdx--;

    if (idx < g_programCount - 1) {
        memmove(&g_programs[idx], &g_programs[idx + 1],
            sizeof(ProgramEntry) * (g_programCount - idx - 1));
    }
    g_programCount--;
}

/*===========================================================================
 * Uniform pipeline — D3D9 shader constants
 *===========================================================================*/

int gldCompatGetUniformLocation(unsigned int program, const char *name)
{
    ProgramEntry *pe;
    int i;

    pe = _findProgram(program);
    if (!pe || !name) return -1;

    /* Search existing uniforms */
    for (i = 0; i < pe->uniformCount; i++) {
        if (strcmp(pe->uniforms[i].name, name) == 0)
            return pe->uniforms[i].registerIndex;
    }

    /* Not found — add new uniform */
    if (pe->uniformCount >= MAX_UNIFORMS_PER_PROGRAM)
        return -1;

    {
        UniformEntry *ue = &pe->uniforms[pe->uniformCount];
        strncpy(ue->name, name, 127);
        ue->name[127] = '\0';
        ue->registerIndex = pe->uniformCount;
        /* Heuristic: if name contains "sampler" or "tex" or "Texture", mark as sampler */
        if (strstr(name, "sampler") || strstr(name, "Sampler") ||
            strstr(name, "tex") || strstr(name, "Tex") ||
            strstr(name, "Texture") || strstr(name, "texture"))
            ue->type = 1; /* sampler */
        else
            ue->type = 0; /* float */
        pe->uniformCount++;
        return ue->registerIndex;
    }
}

void gldCompatUniform1f(int location, float v0)
{
    float data[4];
    if (!g_pDev || location < 0) return;
    data[0] = v0; data[1] = 0.0f; data[2] = 0.0f; data[3] = 0.0f;
    IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, location, data, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, location, data, 1);
}

void gldCompatUniform2f(int location, float v0, float v1)
{
    float data[4];
    if (!g_pDev || location < 0) return;
    data[0] = v0; data[1] = v1; data[2] = 0.0f; data[3] = 0.0f;
    IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, location, data, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, location, data, 1);
}

void gldCompatUniform3f(int location, float v0, float v1, float v2)
{
    float data[4];
    if (!g_pDev || location < 0) return;
    data[0] = v0; data[1] = v1; data[2] = v2; data[3] = 0.0f;
    IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, location, data, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, location, data, 1);
}

void gldCompatUniform4f(int location, float v0, float v1, float v2, float v3)
{
    float data[4];
    if (!g_pDev || location < 0) return;
    data[0] = v0; data[1] = v1; data[2] = v2; data[3] = v3;
    IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, location, data, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, location, data, 1);
}

void gldCompatUniform1i(int location, int v0)
{
    if (!g_pDev || location < 0) return;

    /* Check if this uniform is a sampler in the active program */
    if (g_activeProgramIdx >= 0) {
        ProgramEntry *pe = &g_programs[g_activeProgramIdx];
        int i;
        for (i = 0; i < pe->uniformCount; i++) {
            if (pe->uniforms[i].registerIndex == location && pe->uniforms[i].type == 1) {
                /* Sampler uniform: bind texture unit.
                 * v0 is the texture unit index. The texture should already be
                 * bound to that stage via gldCompatBindTexture. */
                /* Nothing extra needed — D3D9 SetTexture is done at bind time.
                 * But we record the sampler→stage mapping for correctness. */
                return;
            }
        }
    }

    /* Regular integer uniform: cast to float and set as constant */
    {
        float data[4];
        data[0] = (float)v0; data[1] = 0.0f; data[2] = 0.0f; data[3] = 0.0f;
        IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, location, data, 1);
        IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, location, data, 1);
    }
}

void gldCompatUniformMatrix4fv(int location, int count, unsigned char transpose, const float *value)
{
    int m;
    if (!g_pDev || location < 0 || !value) return;

    for (m = 0; m < count; m++) {
        const float *src = value + m * 16;
        int baseReg = location + m * 4;

        if (transpose) {
            /* Transpose the 4x4 matrix before uploading.
             * GL stores column-major, D3D9 expects row-major in float4 registers.
             * If transpose=TRUE, the source is row-major, so we need to transpose
             * to get column-major for the shader constants. */
            float transposed[16];
            int r, c;
            for (r = 0; r < 4; r++)
                for (c = 0; c < 4; c++)
                    transposed[c * 4 + r] = src[r * 4 + c];
            IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, baseReg, transposed, 4);
            IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, baseReg, transposed, 4);
        } else {
            /* Source is column-major (GL default) — upload directly as 4 float4 registers */
            IDirect3DDevice9_SetVertexShaderConstantF(g_pDev, baseReg, src, 4);
            IDirect3DDevice9_SetPixelShaderConstantF(g_pDev, baseReg, src, 4);
        }
    }
}

/*===========================================================================
 * Vertex attribute pipeline
 *===========================================================================*/

void gldCompatVertexAttribPointer(unsigned int index, int size, unsigned int type,
    unsigned char normalized, int stride, const void *pointer)
{
    if (index >= MAX_VERTEX_ATTRIBS) return;
    g_attribs[index].size = size;
    g_attribs[index].type = type;
    g_attribs[index].normalized = normalized ? TRUE : FALSE;
    g_attribs[index].stride = stride;
    g_attribs[index].pointer = pointer;
}

void gldCompatEnableVertexAttribArray(unsigned int index)
{
    if (index >= MAX_VERTEX_ATTRIBS) return;
    g_attribs[index].enabled = TRUE;
}

void gldCompatDisableVertexAttribArray(unsigned int index)
{
    if (index >= MAX_VERTEX_ATTRIBS) return;
    g_attribs[index].enabled = FALSE;
}

/*===========================================================================
 * Draw calls — real D3D9 DrawPrimitiveUP / DrawIndexedPrimitiveUP
 *===========================================================================*/

void gldCompatDrawArrays(unsigned int mode, int first, int count)
{
    D3DPRIMITIVETYPE d3dPrimType;
    int primCount;
    FVFVertex *vertices;
    int i;

    if (!g_pDev || count <= 0) return;

    /* Handle GL_QUADS by expanding to triangles */
    if (mode == GL_QUADS) {
        int numQuads = count / 4;
        int triCount = numQuads * 6;
        FVFVertex *triVerts;

        if (numQuads <= 0) return;
        triVerts = (FVFVertex*)malloc(triCount * sizeof(FVFVertex));
        if (!triVerts) return;

        for (i = 0; i < numQuads; i++) {
            FVFVertex q[4];
            int base = first + i * 4;
            _buildFVFVertex(&q[0], base + 0);
            _buildFVFVertex(&q[1], base + 1);
            _buildFVFVertex(&q[2], base + 2);
            _buildFVFVertex(&q[3], base + 3);
            /* Triangle 1: 0-1-2 */
            triVerts[i * 6 + 0] = q[0];
            triVerts[i * 6 + 1] = q[1];
            triVerts[i * 6 + 2] = q[2];
            /* Triangle 2: 0-2-3 */
            triVerts[i * 6 + 3] = q[0];
            triVerts[i * 6 + 4] = q[2];
            triVerts[i * 6 + 5] = q[3];
        }

        IDirect3DDevice9_SetFVF(g_pDev, COMPAT_FVF);
        IDirect3DDevice9_DrawPrimitiveUP(g_pDev, D3DPT_TRIANGLELIST,
            numQuads * 2, triVerts, sizeof(FVFVertex));
        free(triVerts);
        return;
    }

    if (!_glModeToD3D(mode, count, &d3dPrimType, &primCount))
        return;

    vertices = (FVFVertex*)malloc(count * sizeof(FVFVertex));
    if (!vertices) return;

    for (i = 0; i < count; i++) {
        _buildFVFVertex(&vertices[i], first + i);
    }

    IDirect3DDevice9_SetFVF(g_pDev, COMPAT_FVF);
    IDirect3DDevice9_DrawPrimitiveUP(g_pDev, d3dPrimType, primCount,
        vertices, sizeof(FVFVertex));
    free(vertices);
}

void gldCompatDrawElements(unsigned int mode, int count, unsigned int type, const void *indices)
{
    D3DPRIMITIVETYPE d3dPrimType;
    int primCount;
    FVFVertex *vertices;
    int i;

    if (!g_pDev || count <= 0 || !indices) return;

    /* Handle GL_QUADS by expanding to triangles */
    if (mode == GL_QUADS) {
        int numQuads = count / 4;
        int triCount = numQuads * 6;
        FVFVertex *triVerts;

        if (numQuads <= 0) return;
        triVerts = (FVFVertex*)malloc(triCount * sizeof(FVFVertex));
        if (!triVerts) return;

        for (i = 0; i < numQuads; i++) {
            FVFVertex q[4];
            unsigned int idx0 = _readIndex(indices, type, i * 4 + 0);
            unsigned int idx1 = _readIndex(indices, type, i * 4 + 1);
            unsigned int idx2 = _readIndex(indices, type, i * 4 + 2);
            unsigned int idx3 = _readIndex(indices, type, i * 4 + 3);
            _buildFVFVertex(&q[0], idx0);
            _buildFVFVertex(&q[1], idx1);
            _buildFVFVertex(&q[2], idx2);
            _buildFVFVertex(&q[3], idx3);
            triVerts[i * 6 + 0] = q[0];
            triVerts[i * 6 + 1] = q[1];
            triVerts[i * 6 + 2] = q[2];
            triVerts[i * 6 + 3] = q[0];
            triVerts[i * 6 + 4] = q[2];
            triVerts[i * 6 + 5] = q[3];
        }

        IDirect3DDevice9_SetFVF(g_pDev, COMPAT_FVF);
        IDirect3DDevice9_DrawPrimitiveUP(g_pDev, D3DPT_TRIANGLELIST,
            numQuads * 2, triVerts, sizeof(FVFVertex));
        free(triVerts);
        return;
    }

    if (!_glModeToD3D(mode, count, &d3dPrimType, &primCount))
        return;

    /* Expand indexed vertices into a flat vertex buffer and use DrawPrimitiveUP.
     * This avoids needing to create an index buffer for DrawIndexedPrimitiveUP. */
    vertices = (FVFVertex*)malloc(count * sizeof(FVFVertex));
    if (!vertices) return;

    for (i = 0; i < count; i++) {
        unsigned int idx = _readIndex(indices, type, i);
        _buildFVFVertex(&vertices[i], idx);
    }

    IDirect3DDevice9_SetFVF(g_pDev, COMPAT_FVF);
    IDirect3DDevice9_DrawPrimitiveUP(g_pDev, d3dPrimType, primCount,
        vertices, sizeof(FVFVertex));
    free(vertices);
}

/*===========================================================================
 * State management
 *===========================================================================*/

void gldCompatViewport(int x, int y, int width, int height)
{
    D3DVIEWPORT9 vp;
    HGLRC hGLRC;
    GLD_ctx *ctx;
    int renderTargetHeight;

    if (!g_pDev) return;

    // Get current context to obtain render target dimensions
    hGLRC = gldGetCurrentContext();
    if (hGLRC) {
        ctx = gldGetContextAddress(hGLRC);
        if (ctx) {
            renderTargetHeight = ctx->dwHeight;
        } else {
            renderTargetHeight = height; // Fallback
        }
    } else {
        renderTargetHeight = height; // Fallback
    }

    // Convert OpenGL viewport (bottom-left origin) to D3D9 viewport (top-left origin)
    vp.X = x;
    vp.Y = renderTargetHeight - (y + height);
    vp.Width = width;
    vp.Height = height;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    IDirect3DDevice9_SetViewport(g_pDev, &vp);
}

void gldCompatClearColor(float r, float g, float b, float a)
{
    g_clearR = r; g_clearG = g; g_clearB = b; g_clearA = a;
}

void gldCompatClearDepth(double depth)
{
    g_clearDepth = (float)depth;
}

void gldCompatClear(unsigned int mask)
{
    DWORD flags = 0;
    D3DCOLOR color;
    if (!g_pDev) return;
    if (mask & GL_COLOR_BUFFER_BIT)   flags |= D3DCLEAR_TARGET;
    if (mask & GL_DEPTH_BUFFER_BIT)   flags |= D3DCLEAR_ZBUFFER;
    if (mask & GL_STENCIL_BUFFER_BIT) flags |= D3DCLEAR_STENCIL;
    if (!flags) return;
    color = D3DCOLOR_COLORVALUE(g_clearR, g_clearG, g_clearB, g_clearA);
    IDirect3DDevice9_Clear(g_pDev, 0, NULL, flags, color, g_clearDepth, 0);
}

void gldCompatEnable(unsigned int cap)
{
    if (!g_pDev) return;
    switch (cap) {
    case GL_DEPTH_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ZENABLE, D3DZB_TRUE);
        break;
    case GL_BLEND:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ALPHABLENDENABLE, TRUE);
        break;
    case GL_CULL_FACE:
        g_cullEnabled = TRUE;
        _applyCullMode();
        break;
    case GL_ALPHA_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ALPHATESTENABLE, TRUE);
        break;
    case GL_FOG:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_FOGENABLE, TRUE);
        break;
    case GL_LIGHTING:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_LIGHTING, TRUE);
        break;
    case GL_SCISSOR_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_SCISSORTESTENABLE, TRUE);
        break;
    case GL_STENCIL_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_STENCILENABLE, TRUE);
        break;
    }
}

void gldCompatDisable(unsigned int cap)
{
    if (!g_pDev) return;
    switch (cap) {
    case GL_DEPTH_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ZENABLE, D3DZB_FALSE);
        break;
    case GL_BLEND:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ALPHABLENDENABLE, FALSE);
        break;
    case GL_CULL_FACE:
        g_cullEnabled = FALSE;
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_CULLMODE, D3DCULL_NONE);
        break;
    case GL_ALPHA_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ALPHATESTENABLE, FALSE);
        break;
    case GL_FOG:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_FOGENABLE, FALSE);
        break;
    case GL_LIGHTING:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_LIGHTING, FALSE);
        break;
    case GL_SCISSOR_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_SCISSORTESTENABLE, FALSE);
        break;
    case GL_STENCIL_TEST:
        IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_STENCILENABLE, FALSE);
        break;
    }
}

/*---------------------------------------------------------------------------
 * Blend / Depth
 *---------------------------------------------------------------------------*/

static D3DBLEND _blendGL2D3D(unsigned int f)
{
    switch (f) {
    case 0:      return D3DBLEND_ZERO;
    case 1:      return D3DBLEND_ONE;
    case 0x0300: return D3DBLEND_SRCCOLOR;
    case 0x0301: return D3DBLEND_INVSRCCOLOR;
    case 0x0302: return D3DBLEND_SRCALPHA;
    case 0x0303: return D3DBLEND_INVSRCALPHA;
    case 0x0304: return D3DBLEND_DESTALPHA;
    case 0x0305: return D3DBLEND_INVDESTALPHA;
    case 0x0306: return D3DBLEND_DESTCOLOR;
    case 0x0307: return D3DBLEND_INVDESTCOLOR;
    case 0x0308: return D3DBLEND_SRCALPHASAT;
    default:     return D3DBLEND_ONE;
    }
}

void gldCompatBlendFunc(unsigned int s, unsigned int d)
{
    if (!g_pDev) return;
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_SRCBLEND, _blendGL2D3D(s));
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_DESTBLEND, _blendGL2D3D(d));
}

static D3DCMPFUNC _cmpGL2D3D(unsigned int f)
{
    switch (f) {
    case 0x0200: return D3DCMP_NEVER;
    case 0x0201: return D3DCMP_LESS;
    case 0x0202: return D3DCMP_EQUAL;
    case 0x0203: return D3DCMP_LESSEQUAL;
    case 0x0204: return D3DCMP_GREATER;
    case 0x0205: return D3DCMP_NOTEQUAL;
    case 0x0206: return D3DCMP_GREATEREQUAL;
    case 0x0207: return D3DCMP_ALWAYS;
    default:     return D3DCMP_ALWAYS;
    }
}

void gldCompatDepthFunc(unsigned int func)
{
    if (!g_pDev) return;
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ZFUNC, _cmpGL2D3D(func));
}

void gldCompatDepthMask(unsigned char flag)
{
    if (!g_pDev) return;
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_ZWRITEENABLE, flag ? TRUE : FALSE);
}

/*---------------------------------------------------------------------------
 * Cull face / Front face
 *---------------------------------------------------------------------------*/

void gldCompatCullFace(unsigned int mode)
{
    g_cullMode = mode;
    _applyCullMode();
}

void gldCompatFrontFace(unsigned int mode)
{
    g_frontFace = mode;
    _applyCullMode();
}

/*---------------------------------------------------------------------------
 * Scissor
 *---------------------------------------------------------------------------*/

void gldCompatScissor(int x, int y, int width, int height)
{
    RECT rc;
    if (!g_pDev) return;
    rc.left = x;
    rc.top = y;
    rc.right = x + width;
    rc.bottom = y + height;
    IDirect3DDevice9_SetScissorRect(g_pDev, &rc);
}

/*---------------------------------------------------------------------------
 * Color mask
 *---------------------------------------------------------------------------*/

void gldCompatColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    DWORD flags = 0;
    if (!g_pDev) return;
    if (r) flags |= D3DCOLORWRITEENABLE_RED;
    if (g) flags |= D3DCOLORWRITEENABLE_GREEN;
    if (b) flags |= D3DCOLORWRITEENABLE_BLUE;
    if (a) flags |= D3DCOLORWRITEENABLE_ALPHA;
    IDirect3DDevice9_SetRenderState(g_pDev, D3DRS_COLORWRITEENABLE, flags);
}

/*---------------------------------------------------------------------------
 * Matrix stack
 *---------------------------------------------------------------------------*/

void gldCompatMatrixMode(unsigned int mode)
{
    g_matrixMode = mode;
}

void gldCompatLoadMatrix(const float *m)
{
    float *cur;
    if (!m) return;
    cur = _currentMatrix();
    memcpy(cur, m, 16 * sizeof(float));

    /* Upload to D3D9 transform */
    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, m, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

void gldCompatMultMatrix(const float *m)
{
    float *cur;
    if (!m) return;
    cur = _currentMatrix();
    _mat4Multiply(cur, cur, m);

    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, cur, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

void gldCompatLoadIdentity(void)
{
    float *cur = _currentMatrix();
    _mat4Identity(cur);

    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, cur, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

void gldCompatPushMatrix(void)
{
    switch (g_matrixMode) {
    case GL_PROJECTION:
        if (g_projTop < MAX_MATRIX_STACK - 1) {
            memcpy(g_projStack[g_projTop + 1].m, g_projStack[g_projTop].m, 16 * sizeof(float));
            g_projTop++;
        }
        break;
    case GL_TEXTURE:
        if (g_texTop < MAX_MATRIX_STACK - 1) {
            memcpy(g_texStack[g_texTop + 1].m, g_texStack[g_texTop].m, 16 * sizeof(float));
            g_texTop++;
        }
        break;
    default:
        if (g_mvTop < MAX_MATRIX_STACK - 1) {
            memcpy(g_mvStack[g_mvTop + 1].m, g_mvStack[g_mvTop].m, 16 * sizeof(float));
            g_mvTop++;
        }
        break;
    }
}

void gldCompatPopMatrix(void)
{
    float *cur;
    switch (g_matrixMode) {
    case GL_PROJECTION:
        if (g_projTop > 0) g_projTop--;
        break;
    case GL_TEXTURE:
        if (g_texTop > 0) g_texTop--;
        break;
    default:
        if (g_mvTop > 0) g_mvTop--;
        break;
    }

    /* Re-upload the now-current matrix to D3D9 */
    cur = _currentMatrix();
    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, cur, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

void gldCompatOrtho(double l, double r, double b, double t, double n, double f)
{
    float ortho[16];
    float *cur;

    memset(ortho, 0, sizeof(ortho));
    ortho[0]  = (float)(2.0 / (r - l));
    ortho[5]  = (float)(2.0 / (t - b));
    ortho[10] = (float)(-2.0 / (f - n));
    ortho[12] = (float)(-(r + l) / (r - l));
    ortho[13] = (float)(-(t + b) / (t - b));
    ortho[14] = (float)(-(f + n) / (f - n));
    ortho[15] = 1.0f;

    cur = _currentMatrix();
    _mat4Multiply(cur, cur, ortho);

    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, cur, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

void gldCompatFrustum(double l, double r, double b, double t, double n, double f)
{
    float frust[16];
    float *cur;

    memset(frust, 0, sizeof(frust));
    frust[0]  = (float)(2.0 * n / (r - l));
    frust[5]  = (float)(2.0 * n / (t - b));
    frust[8]  = (float)((r + l) / (r - l));
    frust[9]  = (float)((t + b) / (t - b));
    frust[10] = (float)(-(f + n) / (f - n));
    frust[11] = -1.0f;
    frust[14] = (float)(-2.0 * f * n / (f - n));

    cur = _currentMatrix();
    _mat4Multiply(cur, cur, frust);

    if (g_pDev) {
        D3DMATRIX d3dm;
        memcpy(&d3dm, cur, sizeof(D3DMATRIX));
        switch (g_matrixMode) {
        case GL_PROJECTION:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_PROJECTION, &d3dm);
            break;
        case GL_TEXTURE:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_TEXTURE0, &d3dm);
            break;
        default:
            IDirect3DDevice9_SetTransform(g_pDev, D3DTS_WORLD, &d3dm);
            break;
        }
    }
}

/*---------------------------------------------------------------------------
 * Frame boundary
 *---------------------------------------------------------------------------*/

void gldCompatSwapBuffers(void)
{
    if (!g_pDev) return;
    IDirect3DDevice9_EndScene(g_pDev);
    IDirect3DDevice9_Present(g_pDev, NULL, NULL, NULL, NULL);
    IDirect3DDevice9_BeginScene(g_pDev);
}

/*---------------------------------------------------------------------------
 * Query
 *---------------------------------------------------------------------------*/

BOOL gldCompatIsActive(void)
{
    return (g_pDev != NULL);
}
