/*********************************************************************************
*
*  gl_impl.c - GL function implementations using the state machine
*
*  These _gls* functions track GL state properly so games don't crash.
*  They don't produce correct D3D9 rendering yet, but they:
*  - Return valid IDs from glGen* functions
*  - Track object allocation/deallocation
*  - Track bindings
*  - Store data passed to glBufferData, glTexImage2D, etc.
*  - Track enable/disable state
*  - Track matrix operations
*  - Track immediate mode vertices
*  - Return proper values from glGet* queries
*  - Perform D3D9 clears when glClear is called
*  - Track viewport/scissor state
*
*********************************************************************************/

#include "gl_impl.h"
#include "gl_state.h"
#include "context_manager.h"
#include "gld_diag.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* GL enum constants we need — guarded against glad/gl.h */
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D           0x0DE1
#endif
#ifndef GL_TEXTURE_CUBE_MAP
#define GL_TEXTURE_CUBE_MAP     0x8513
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER         0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0             0x84C0
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER     0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER     0x8CA9
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER         0x8D41
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_DEPTH_TEST
#define GL_DEPTH_TEST           0x0B71
#endif
#ifndef GL_BLEND
#define GL_BLEND                0x0BE2
#endif
#ifndef GL_CULL_FACE
#define GL_CULL_FACE            0x0B44
#endif
#ifndef GL_SCISSOR_TEST
#define GL_SCISSOR_TEST         0x0C11
#endif
#ifndef GL_STENCIL_TEST
#define GL_STENCIL_TEST         0x0B90
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST           0x0BC0
#endif
#ifndef GL_FOG
#define GL_FOG                  0x0B60
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING             0x0B50
#endif
#ifndef GL_POLYGON_OFFSET_FILL
#define GL_POLYGON_OFFSET_FILL  0x8037
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE          0x809D
#endif
#ifndef GL_COLOR_MATERIAL
#define GL_COLOR_MATERIAL       0x0B57
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE            0x0BA1
#endif
#ifndef GL_LIGHT0
#define GL_LIGHT0               0x4000
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER   0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER   0x2800
#endif
#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S       0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T       0x2803
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R       0x8072
#endif
#ifndef GL_MODELVIEW
#define GL_MODELVIEW            0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION           0x1701
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE              0x1702
#endif
#ifndef GL_COLOR_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT     0x00004000
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT     0x00000100
#endif
#ifndef GL_STENCIL_BUFFER_BIT
#define GL_STENCIL_BUFFER_BIT   0x00000400
#endif
#ifndef GL_NO_ERROR
#define GL_NO_ERROR             0
#endif
#ifndef GL_TRUE
#define GL_TRUE                 1
#endif
#ifndef GL_FALSE
#define GL_FALSE                0
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER        0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER      0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS       0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS          0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH      0x8B84
#endif
#ifndef GL_SHADER_TYPE
#define GL_SHADER_TYPE          0x8B4F
#endif
#ifndef GL_DELETE_STATUS
#define GL_DELETE_STATUS        0x8B80
#endif
#ifndef GL_SHADER_SOURCE_LENGTH
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#endif

#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT     0x0CF5
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT       0x0D05
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH    0x0CF2
#endif
#ifndef GL_PACK_ROW_LENGTH
#define GL_PACK_ROW_LENGTH      0x0D02
#endif

/* GL format constants for texture creation */
#ifndef GL_RGBA
#define GL_RGBA                 0x1908
#endif
#ifndef GL_RGB
#define GL_RGB                  0x1907
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE            0x1909
#endif
#ifndef GL_ALPHA
#define GL_ALPHA                0x1906
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA      0x190A
#endif
#ifndef GL_BGRA
#define GL_BGRA                 0x80E1
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE        0x1401
#endif
#ifndef GL_UNSIGNED_SHORT
#define GL_UNSIGNED_SHORT       0x1403
#endif
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT         0x1405
#endif
#ifndef GL_FLOAT
#define GL_FLOAT                0x1406
#endif

/* GL filter/wrap constants */
#ifndef GL_NEAREST
#define GL_NEAREST              0x2600
#endif
#ifndef GL_LINEAR
#define GL_LINEAR               0x2601
#endif
#ifndef GL_NEAREST_MIPMAP_NEAREST
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#endif
#ifndef GL_LINEAR_MIPMAP_NEAREST
#define GL_LINEAR_MIPMAP_NEAREST  0x2701
#endif
#ifndef GL_NEAREST_MIPMAP_LINEAR
#define GL_NEAREST_MIPMAP_LINEAR  0x2702
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR   0x2703
#endif
#ifndef GL_REPEAT
#define GL_REPEAT               0x2901
#endif
#ifndef GL_CLAMP
#define GL_CLAMP                0x2900
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE        0x812F
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT      0x8370
#endif

/* GL blend factor constants */
#ifndef GL_ZERO
#define GL_ZERO                 0
#endif
#ifndef GL_ONE
#define GL_ONE                  1
#endif
#ifndef GL_SRC_COLOR
#define GL_SRC_COLOR            0x0300
#endif
#ifndef GL_ONE_MINUS_SRC_COLOR
#define GL_ONE_MINUS_SRC_COLOR  0x0301
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA            0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#endif
#ifndef GL_DST_ALPHA
#define GL_DST_ALPHA            0x0304
#endif
#ifndef GL_ONE_MINUS_DST_ALPHA
#define GL_ONE_MINUS_DST_ALPHA  0x0305
#endif
#ifndef GL_DST_COLOR
#define GL_DST_COLOR            0x0306
#endif
#ifndef GL_ONE_MINUS_DST_COLOR
#define GL_ONE_MINUS_DST_COLOR  0x0307
#endif
#ifndef GL_SRC_ALPHA_SATURATE
#define GL_SRC_ALPHA_SATURATE   0x0308
#endif
#ifndef GL_CONSTANT_COLOR
#define GL_CONSTANT_COLOR       0x8001
#endif
#ifndef GL_ONE_MINUS_CONSTANT_COLOR
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#endif
#ifndef GL_CONSTANT_ALPHA
#define GL_CONSTANT_ALPHA       0x8003
#endif
#ifndef GL_ONE_MINUS_CONSTANT_ALPHA
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#endif

/* GL compare function constants */
#ifndef GL_NEVER
#define GL_NEVER                0x0200
#endif
#ifndef GL_LESS
#define GL_LESS                 0x0201
#endif
#ifndef GL_EQUAL
#define GL_EQUAL                0x0202
#endif
#ifndef GL_LEQUAL
#define GL_LEQUAL               0x0203
#endif
#ifndef GL_GREATER
#define GL_GREATER              0x0204
#endif
#ifndef GL_NOTEQUAL
#define GL_NOTEQUAL             0x0205
#endif
#ifndef GL_GEQUAL
#define GL_GEQUAL               0x0206
#endif
#ifndef GL_ALWAYS
#define GL_ALWAYS               0x0207
#endif

/* GL cull face constants */
#ifndef GL_CW
#define GL_CW                   0x0900
#endif
#ifndef GL_CCW
#define GL_CCW                  0x0901
#endif

/* S3TC compressed texture format constants */
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

/* GL internal format aliases */
#ifndef GL_RGBA8
#define GL_RGBA8                0x8058
#endif
#ifndef GL_RGB8
#define GL_RGB8                 0x8051
#endif

/* ===================================================================
 *  D3D9 Helper Functions
 * =================================================================== */

D3DFORMAT _glsMapGLFormatToD3D(unsigned int internalformat)
{
    switch (internalformat) {
    case GL_RGBA:
    case GL_RGBA8:
    case 4:
        return D3DFMT_A8R8G8B8;
    case GL_RGB:
    case GL_RGB8:
    case 3:
        return D3DFMT_X8R8G8B8;
    case GL_LUMINANCE:
    case 1:
        return D3DFMT_L8;
    case GL_ALPHA:
        return D3DFMT_A8;
    case GL_LUMINANCE_ALPHA:
    case 2:
        return D3DFMT_A8L8;
    default:
        return D3DFMT_A8R8G8B8;
    }
}

D3DFORMAT _glsMapCompressedFormatToD3D(unsigned int internalformat)
{
    switch (internalformat) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return D3DFMT_DXT1;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return D3DFMT_DXT3;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return D3DFMT_DXT5;
    default:
        return D3DFMT_UNKNOWN;
    }
}

void _glsCopyPixelsToD3D(void *dst, const void *src, int width, int height,
                          unsigned int glFormat, unsigned int glType, int dstPitch)
{
    int y;
    if (!dst || !src) return;

    for (y = 0; y < height; y++) {
        unsigned char *dstRow = (unsigned char *)dst + y * dstPitch;
        int x;

        if ((glFormat == GL_RGBA || glFormat == GL_RGBA8 || glFormat == 4) && glType == GL_UNSIGNED_BYTE) {
            /* RGBA -> BGRA (D3D A8R8G8B8 is actually BGRA in memory) */
            const unsigned char *srcRow = (const unsigned char *)src + y * width * 4;
            for (x = 0; x < width; x++) {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2]; /* B <- R */
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; /* G <- G */
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0]; /* R <- B */
                dstRow[x * 4 + 3] = srcRow[x * 4 + 3]; /* A <- A */
            }
        } else if ((glFormat == GL_RGB || glFormat == GL_RGB8 || glFormat == 3) && glType == GL_UNSIGNED_BYTE) {
            /* RGB -> BGRX (D3D X8R8G8B8) */
            const unsigned char *srcRow = (const unsigned char *)src + y * width * 3;
            for (x = 0; x < width; x++) {
                dstRow[x * 4 + 0] = srcRow[x * 3 + 2]; /* B */
                dstRow[x * 4 + 1] = srcRow[x * 3 + 1]; /* G */
                dstRow[x * 4 + 2] = srcRow[x * 3 + 0]; /* R */
                dstRow[x * 4 + 3] = 0xFF;               /* X */
            }
        } else if (glFormat == GL_BGRA && glType == GL_UNSIGNED_BYTE) {
            /* BGRA -> BGRA (no swizzle needed) */
            const unsigned char *srcRow = (const unsigned char *)src + y * width * 4;
            memcpy(dstRow, srcRow, width * 4);
        } else if ((glFormat == GL_LUMINANCE || glFormat == 1) && glType == GL_UNSIGNED_BYTE) {
            /* L8 -> L8 */
            const unsigned char *srcRow = (const unsigned char *)src + y * width;
            memcpy(dstRow, srcRow, width);
        } else if (glFormat == GL_ALPHA && glType == GL_UNSIGNED_BYTE) {
            /* A8 -> A8 */
            const unsigned char *srcRow = (const unsigned char *)src + y * width;
            memcpy(dstRow, srcRow, width);
        } else if ((glFormat == GL_LUMINANCE_ALPHA || glFormat == 2) && glType == GL_UNSIGNED_BYTE) {
            /* LA -> A8L8 */
            const unsigned char *srcRow = (const unsigned char *)src + y * width * 2;
            memcpy(dstRow, srcRow, width * 2);
        } else {
            /* Fallback: assume RGBA byte order, do BGRA swizzle */
            const unsigned char *srcRow = (const unsigned char *)src + y * width * 4;
            for (x = 0; x < width; x++) {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2];
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0];
                dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
            }
        }
    }
}

D3DBLEND _glsMapBlendFactor(unsigned int glFactor)
{
    switch (glFactor) {
    case GL_ZERO:                    return D3DBLEND_ZERO;
    case GL_ONE:                     return D3DBLEND_ONE;
    case GL_SRC_COLOR:               return D3DBLEND_SRCCOLOR;
    case GL_ONE_MINUS_SRC_COLOR:     return D3DBLEND_INVSRCCOLOR;
    case GL_SRC_ALPHA:               return D3DBLEND_SRCALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:     return D3DBLEND_INVSRCALPHA;
    case GL_DST_ALPHA:               return D3DBLEND_DESTALPHA;
    case GL_ONE_MINUS_DST_ALPHA:     return D3DBLEND_INVDESTALPHA;
    case GL_DST_COLOR:               return D3DBLEND_DESTCOLOR;
    case GL_ONE_MINUS_DST_COLOR:     return D3DBLEND_INVDESTCOLOR;
    case GL_SRC_ALPHA_SATURATE:      return D3DBLEND_SRCALPHASAT;
    case GL_CONSTANT_COLOR:          return D3DBLEND_BLENDFACTOR;
    case GL_ONE_MINUS_CONSTANT_COLOR:return D3DBLEND_INVBLENDFACTOR;
    case GL_CONSTANT_ALPHA:          return D3DBLEND_BLENDFACTOR;
    case GL_ONE_MINUS_CONSTANT_ALPHA:return D3DBLEND_INVBLENDFACTOR;
    default:                         return D3DBLEND_ONE;
    }
}

D3DCMPFUNC _glsMapCompareFunc(unsigned int glFunc)
{
    switch (glFunc) {
    case GL_NEVER:    return D3DCMP_NEVER;
    case GL_LESS:     return D3DCMP_LESS;
    case GL_EQUAL:    return D3DCMP_EQUAL;
    case GL_LEQUAL:   return D3DCMP_LESSEQUAL;
    case GL_GREATER:  return D3DCMP_GREATER;
    case GL_NOTEQUAL: return D3DCMP_NOTEQUAL;
    case GL_GEQUAL:   return D3DCMP_GREATEREQUAL;
    case GL_ALWAYS:   return D3DCMP_ALWAYS;
    default:          return D3DCMP_LESSEQUAL;
    }
}

void _glsApplyD3DCullMode(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev) return;

    if (!s->enableCullFace) {
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);
    } else {
        /* D3D9 uses opposite winding convention from GL */
        if (s->cullFaceMode == GL_BACK) {
            /* GL_BACK + GL_CCW -> D3DCULL_CW, GL_BACK + GL_CW -> D3DCULL_CCW */
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE,
                (s->frontFace == GL_CCW) ? D3DCULL_CW : D3DCULL_CCW);
        } else if (s->cullFaceMode == GL_FRONT) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE,
                (s->frontFace == GL_CCW) ? D3DCULL_CCW : D3DCULL_CW);
        } else {
            /* GL_FRONT_AND_BACK — cull everything (no good D3D9 equivalent, use CW) */
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_CW);
        }
    }
}

static D3DTEXTUREFILTERTYPE _glsMapMinFilter(unsigned int glFilter)
{
    switch (glFilter) {
    case GL_NEAREST:                return D3DTEXF_POINT;
    case GL_LINEAR:                 return D3DTEXF_LINEAR;
    case GL_NEAREST_MIPMAP_NEAREST: return D3DTEXF_POINT;
    case GL_LINEAR_MIPMAP_NEAREST:  return D3DTEXF_LINEAR;
    case GL_NEAREST_MIPMAP_LINEAR:  return D3DTEXF_POINT;
    case GL_LINEAR_MIPMAP_LINEAR:   return D3DTEXF_LINEAR;
    default:                        return D3DTEXF_POINT;
    }
}

static D3DTEXTUREFILTERTYPE _glsMapMipFilter(unsigned int glFilter)
{
    switch (glFilter) {
    case GL_NEAREST:                return D3DTEXF_NONE;
    case GL_LINEAR:                 return D3DTEXF_NONE;
    case GL_NEAREST_MIPMAP_NEAREST: return D3DTEXF_POINT;
    case GL_LINEAR_MIPMAP_NEAREST:  return D3DTEXF_POINT;
    case GL_NEAREST_MIPMAP_LINEAR:  return D3DTEXF_LINEAR;
    case GL_LINEAR_MIPMAP_LINEAR:   return D3DTEXF_LINEAR;
    default:                        return D3DTEXF_NONE;
    }
}

static D3DTEXTUREFILTERTYPE _glsMapMagFilter(unsigned int glFilter)
{
    switch (glFilter) {
    case GL_NEAREST: return D3DTEXF_POINT;
    case GL_LINEAR:  return D3DTEXF_LINEAR;
    default:         return D3DTEXF_POINT;
    }
}

static D3DTEXTUREADDRESS _glsMapWrapMode(unsigned int glWrap)
{
    switch (glWrap) {
    case GL_REPEAT:          return D3DTADDRESS_WRAP;
    case GL_CLAMP:           return D3DTADDRESS_CLAMP;
    case GL_CLAMP_TO_EDGE:   return D3DTADDRESS_CLAMP;
    case GL_MIRRORED_REPEAT: return D3DTADDRESS_MIRROR;
    default:                 return D3DTADDRESS_WRAP;
    }
}

static void _glsTransposeMatrix(D3DMATRIX *dst, const float *src)
{
    /* GL is column-major, D3D9 is row-major — transpose */
    dst->_11 = src[0];  dst->_12 = src[4];  dst->_13 = src[8];  dst->_14 = src[12];
    dst->_21 = src[1];  dst->_22 = src[5];  dst->_23 = src[9];  dst->_24 = src[13];
    dst->_31 = src[2];  dst->_32 = src[6];  dst->_33 = src[10]; dst->_34 = src[14];
    dst->_41 = src[3];  dst->_42 = src[7];  dst->_43 = src[11]; dst->_44 = src[15];
}

/* ===================================================================
 *  SECTION 1: Object Management
 * =================================================================== */

void _glsGenTextures(int n, unsigned int *textures)
{
    GLS_State *s = glsGetState();
    int i;
    if (!textures || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextTexId++;
        if (id < GLS_MAX_TEXTURES) {
            memset(&s->textures[id], 0, sizeof(GLS_Texture));
            s->textures[id].id = id;
            s->textures[id].allocated = TRUE;
            s->textures[id].minFilter = GL_TEXTURE_MIN_FILTER;
            s->textures[id].magFilter = 0x2600;
            s->textures[id].wrapS = 0x2901;
            s->textures[id].wrapT = 0x2901;
            s->textures[id].wrapR = 0x2901;
        }
        textures[i] = id;
    }
    gldDiagLog("GL: glGenTextures(%d) -> first=%u", n, textures[0]);
}

void _glsDeleteTextures(int n, const unsigned int *textures)
{
    GLS_State *s = glsGetState();
    int i, u;
    if (!textures || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_Texture *tex = glsFindTexture(textures[i]);
        if (tex) {
            /* Release D3D9 texture */
            if (tex->pTex) {
                __try {
                    IDirect3DTexture9_Release(tex->pTex);
                } __except(EXCEPTION_EXECUTE_HANDLER) { }
                tex->pTex = NULL;
            }
            if (tex->pCubeTex) {
                __try {
                    IDirect3DCubeTexture9_Release(tex->pCubeTex);
                } __except(EXCEPTION_EXECUTE_HANDLER) { }
                tex->pCubeTex = NULL;
            }
            if (tex->pixelData) { free(tex->pixelData); tex->pixelData = NULL; }
            /* Unbind from any texture unit */
            for (u = 0; u < GLS_MAX_TEX_UNITS; u++) {
                if (s->boundTexture2D[u] == textures[i]) s->boundTexture2D[u] = 0;
                if (s->boundTextureCube[u] == textures[i]) s->boundTextureCube[u] = 0;
            }
            tex->allocated = FALSE;
        }
    }
}

void _glsBindTexture(unsigned int target, unsigned int texture)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;

    /* Auto-allocate if binding a non-zero texture that doesn't exist yet */
    if (texture != 0 && texture < GLS_MAX_TEXTURES && !s->textures[texture].allocated) {
        memset(&s->textures[texture], 0, sizeof(GLS_Texture));
        s->textures[texture].id = texture;
        s->textures[texture].target = target;
        s->textures[texture].allocated = TRUE;
        s->textures[texture].wrapS = 0x2901;
        s->textures[texture].wrapT = 0x2901;
        s->textures[texture].wrapR = 0x2901;
    }

    if (target == GL_TEXTURE_2D) {
        s->boundTexture2D[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = GL_TEXTURE_2D;
    } else if (target == GL_TEXTURE_CUBE_MAP) {
        s->boundTextureCube[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = GL_TEXTURE_CUBE_MAP;
    }

    /* Set D3D9 texture on the device */
    if (pDev && target == GL_TEXTURE_2D) {
        GLS_Texture *tex = (texture != 0) ? glsFindTexture(texture) : NULL;
        IDirect3DBaseTexture9 *baseTex = (tex && tex->pTex) ? (IDirect3DBaseTexture9 *)tex->pTex : NULL;
        __try {
            IDirect3DDevice9_SetTexture(pDev, unit, baseTex);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            /* Device lost or invalid — ignore */
        }
    }
}

void _glsGenBuffers(int n, unsigned int *buffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!buffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextBufId++;
        if (id < GLS_MAX_BUFFERS) {
            memset(&s->buffers[id], 0, sizeof(GLS_Buffer));
            s->buffers[id].id = id;
            s->buffers[id].allocated = TRUE;
        }
        buffers[i] = id;
    }
}

void _glsDeleteBuffers(int n, const unsigned int *buffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!buffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_Buffer *buf = glsFindBuffer(buffers[i]);
        if (buf) {
            if (buf->data) { free(buf->data); buf->data = NULL; }
            if (s->boundArrayBuffer == buffers[i]) s->boundArrayBuffer = 0;
            if (s->boundElementBuffer == buffers[i]) s->boundElementBuffer = 0;
            buf->allocated = FALSE;
        }
    }
}

void _glsBindBuffer(unsigned int target, unsigned int buffer)
{
    GLS_State *s = glsGetState();
    /* Auto-allocate if binding a non-zero buffer that doesn't exist yet */
    if (buffer != 0 && buffer < GLS_MAX_BUFFERS && !s->buffers[buffer].allocated) {
        memset(&s->buffers[buffer], 0, sizeof(GLS_Buffer));
        s->buffers[buffer].id = buffer;
        s->buffers[buffer].target = target;
        s->buffers[buffer].allocated = TRUE;
    }

    if (target == GL_ARRAY_BUFFER)
        s->boundArrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        s->boundElementBuffer = buffer;
    else if (target == 0x8F36) /* GL_COPY_READ_BUFFER */
        s->boundCopyReadBuffer = buffer;
    else if (target == 0x8F37) /* GL_COPY_WRITE_BUFFER */
        s->boundCopyWriteBuffer = buffer;
    else if (target == 0x8A11) /* GL_UNIFORM_BUFFER */
        s->boundUniformBuffer = buffer;
    else if (target == 0x8C8E) /* GL_TRANSFORM_FEEDBACK_BUFFER */
        s->boundTransformFeedbackBuffer = buffer;
}

void _glsGenVertexArrays(int n, unsigned int *arrays)
{
    GLS_State *s = glsGetState();
    int i;
    if (!arrays || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextVaoId++;
        if (id < GLS_MAX_VAOS) {
            memset(&s->vaos[id], 0, sizeof(GLS_VAO));
            s->vaos[id].id = id;
            s->vaos[id].allocated = TRUE;
        }
        arrays[i] = id;
    }
}

void _glsDeleteVertexArrays(int n, const unsigned int *arrays)
{
    GLS_State *s = glsGetState();
    int i;
    if (!arrays || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_VAO *vao = glsFindVAO(arrays[i]);
        if (vao) {
            if (s->boundVAO == arrays[i]) s->boundVAO = 0;
            vao->allocated = FALSE;
        }
    }
}

void _glsBindVertexArray(unsigned int array)
{
    GLS_State *s = glsGetState();
    s->boundVAO = array;
}

void _glsGenFramebuffers(int n, unsigned int *framebuffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!framebuffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextFboId++;
        if (id < GLS_MAX_FBOS) {
            memset(&s->fbos[id], 0, sizeof(GLS_FBO));
            s->fbos[id].id = id;
            s->fbos[id].allocated = TRUE;
        }
        framebuffers[i] = id;
    }
}

void _glsDeleteFramebuffers(int n, const unsigned int *framebuffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!framebuffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_FBO *fbo = glsFindFBO(framebuffers[i]);
        if (fbo) {
            if (s->boundFBO == framebuffers[i]) s->boundFBO = 0;
            fbo->allocated = FALSE;
        }
    }
}

void _glsBindFramebuffer(unsigned int target, unsigned int framebuffer)
{
    GLS_State *s = glsGetState();
    (void)target;
    s->boundFBO = framebuffer;
}

unsigned int _glsCheckFramebufferStatus(unsigned int target)
{
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

void _glsGenRenderbuffers(int n, unsigned int *renderbuffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!renderbuffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextRboId++;
        if (id < GLS_MAX_RBOS) {
            memset(&s->rbos[id], 0, sizeof(GLS_RBO));
            s->rbos[id].id = id;
            s->rbos[id].allocated = TRUE;
        }
        renderbuffers[i] = id;
    }
}

void _glsDeleteRenderbuffers(int n, const unsigned int *renderbuffers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!renderbuffers || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_RBO *rbo = glsFindRBO(renderbuffers[i]);
        if (rbo) {
            if (s->boundRBO == renderbuffers[i]) s->boundRBO = 0;
            rbo->allocated = FALSE;
        }
    }
}

void _glsBindRenderbuffer(unsigned int target, unsigned int renderbuffer)
{
    GLS_State *s = glsGetState();
    (void)target;
    s->boundRBO = renderbuffer;
}

void _glsRenderbufferStorage(unsigned int target, unsigned int internalformat, int width, int height)
{
    GLS_State *s = glsGetState();
    GLS_RBO *rbo = glsFindRBO(s->boundRBO);
    (void)target;
    if (rbo) {
        rbo->internalFormat = internalformat;
        rbo->width = width;
        rbo->height = height;
    }
}

void _glsGenQueries(int n, unsigned int *ids)
{
    GLS_State *s = glsGetState();
    int i;
    if (!ids || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextQueryId++;
        if (id < GLS_MAX_QUERIES) {
            memset(&s->queries[id], 0, sizeof(GLS_Query));
            s->queries[id].id = id;
            s->queries[id].allocated = TRUE;
        }
        ids[i] = id;
    }
}

void _glsDeleteQueries(int n, const unsigned int *ids)
{
    int i;
    if (!ids || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_Query *q = glsFindQuery(ids[i]);
        if (q) q->allocated = FALSE;
    }
}

void _glsGenSamplers(int count, unsigned int *samplers)
{
    GLS_State *s = glsGetState();
    int i;
    if (!samplers || count <= 0) return;
    for (i = 0; i < count; i++) {
        unsigned int id = s->nextSamplerId++;
        if (id < GLS_MAX_SAMPLERS) {
            memset(&s->samplers[id], 0, sizeof(GLS_Sampler));
            s->samplers[id].id = id;
            s->samplers[id].allocated = TRUE;
        }
        samplers[i] = id;
    }
}

void _glsDeleteSamplers(int count, const unsigned int *samplers)
{
    int i;
    if (!samplers || count <= 0) return;
    for (i = 0; i < count; i++) {
        GLS_Sampler *samp = glsFindSampler(samplers[i]);
        if (samp) samp->allocated = FALSE;
    }
}


/* ===================================================================
 *  SECTION 2: Texture Functions
 * =================================================================== */

void _glsTexImage2D(unsigned int target, int level, int internalformat,
                     int width, int height, int border,
                     unsigned int format, unsigned int type, const void *pixels)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    HRESULT hr;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    texId = s->boundTexture2D[unit];
    tex = glsFindTexture(texId);

    (void)border; (void)target;

    if (!tex || level != 0) return;

    tex->width = width;
    tex->height = height;
    tex->internalFormat = internalformat;

    /* Release old texture if any */
    if (tex->pTex) {
        IDirect3DTexture9_Release(tex->pTex);
        tex->pTex = NULL;
    }
    if (tex->pixelData) { free(tex->pixelData); tex->pixelData = NULL; tex->pixelDataSize = 0; }

    if (!pDev || width <= 0 || height <= 0) return;

    d3dFmt = _glsMapGLFormatToD3D(internalformat);

    __try {
        hr = IDirect3DDevice9_CreateTexture(pDev, width, height, 1, 0, d3dFmt,
                                             D3DPOOL_MANAGED, &tex->pTex, NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (FAILED(hr) || !tex->pTex) {
        gldDiagLog("GL: TexImage2D CreateTexture FAILED hr=0x%08X %dx%d fmt=%d", hr, width, height, d3dFmt);
        tex->pTex = NULL;
        return;
    }

    /* Copy pixel data if provided */
    if (pixels) {
        D3DLOCKED_RECT lr;
        __try {
            hr = IDirect3DTexture9_LockRect(tex->pTex, 0, &lr, NULL, 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            _glsCopyPixelsToD3D(lr.pBits, pixels, width, height, format, type, lr.Pitch);
            IDirect3DTexture9_UnlockRect(tex->pTex, 0);
        }
    }

    gldDiagLog("GL: TexImage2D tex=%u %dx%d fmt=0x%X d3d=%d pTex=%p", texId, width, height, internalformat, d3dFmt, tex->pTex);
}

void _glsTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset,
                        int width, int height, unsigned int format, unsigned int type,
                        const void *pixels)
{
    /* Just accept the data without crashing */
    (void)target; (void)level; (void)xoffset; (void)yoffset;
    (void)width; (void)height; (void)format; (void)type; (void)pixels;
}

void _glsTexParameteri(unsigned int target, unsigned int pname, int param)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;

    if (target == GL_TEXTURE_2D)
        texId = s->boundTexture2D[unit];
    else if (target == GL_TEXTURE_CUBE_MAP)
        texId = s->boundTextureCube[unit];
    else
        return;

    tex = glsFindTexture(texId);
    if (!tex) return;

    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: tex->minFilter = param; break;
    case GL_TEXTURE_MAG_FILTER: tex->magFilter = param; break;
    case GL_TEXTURE_WRAP_S:     tex->wrapS = param; break;
    case GL_TEXTURE_WRAP_T:     tex->wrapT = param; break;
    case GL_TEXTURE_WRAP_R:     tex->wrapR = param; break;
    }

    /* Apply to D3D9 sampler state */
    if (pDev) {
        __try {
            switch (pname) {
            case GL_TEXTURE_MIN_FILTER:
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MINFILTER, _glsMapMinFilter(param));
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MIPFILTER, _glsMapMipFilter(param));
                break;
            case GL_TEXTURE_MAG_FILTER:
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MAGFILTER, _glsMapMagFilter(param));
                break;
            case GL_TEXTURE_WRAP_S:
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSU, _glsMapWrapMode(param));
                break;
            case GL_TEXTURE_WRAP_T:
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSV, _glsMapWrapMode(param));
                break;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsTexParameterf(unsigned int target, unsigned int pname, float param)
{
    _glsTexParameteri(target, pname, (int)param);
}

void _glsCompressedTexImage2D(unsigned int target, int level, unsigned int internalformat,
                               int width, int height, int border, int imageSize, const void *data)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    HRESULT hr;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    texId = s->boundTexture2D[unit];
    tex = glsFindTexture(texId);

    (void)border; (void)target;

    if (!tex || level != 0) return;

    tex->width = width;
    tex->height = height;
    tex->internalFormat = internalformat;

    /* Release old texture if any */
    if (tex->pTex) {
        IDirect3DTexture9_Release(tex->pTex);
        tex->pTex = NULL;
    }
    if (tex->pixelData) { free(tex->pixelData); tex->pixelData = NULL; tex->pixelDataSize = 0; }

    if (!pDev || width <= 0 || height <= 0) return;

    d3dFmt = _glsMapCompressedFormatToD3D(internalformat);
    if (d3dFmt == D3DFMT_UNKNOWN) {
        gldDiagLog("GL: CompressedTexImage2D unknown format 0x%X", internalformat);
        return;
    }

    __try {
        hr = IDirect3DDevice9_CreateTexture(pDev, width, height, 1, 0, d3dFmt,
                                             D3DPOOL_MANAGED, &tex->pTex, NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (FAILED(hr) || !tex->pTex) {
        gldDiagLog("GL: CompressedTexImage2D CreateTexture FAILED hr=0x%08X", hr);
        tex->pTex = NULL;
        return;
    }

    /* Copy compressed data directly — no conversion needed for S3TC/DXT */
    if (data && imageSize > 0) {
        D3DLOCKED_RECT lr;
        __try {
            hr = IDirect3DTexture9_LockRect(tex->pTex, 0, &lr, NULL, 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            /* For compressed textures, copy row by row respecting pitch,
               or if pitch matches, copy the whole block */
            int blockHeight = (height + 3) / 4;
            int blockWidth = (width + 3) / 4;
            int blockSize = (d3dFmt == D3DFMT_DXT1) ? 8 : 16;
            int srcRowBytes = blockWidth * blockSize;
            int row;

            if (lr.Pitch == srcRowBytes) {
                memcpy(lr.pBits, data, imageSize);
            } else {
                for (row = 0; row < blockHeight; row++) {
                    int copySize = srcRowBytes;
                    if (copySize > lr.Pitch) copySize = lr.Pitch;
                    memcpy((unsigned char *)lr.pBits + row * lr.Pitch,
                           (const unsigned char *)data + row * srcRowBytes,
                           copySize);
                }
            }
            IDirect3DTexture9_UnlockRect(tex->pTex, 0);
        }
    }

    gldDiagLog("GL: CompressedTexImage2D tex=%u %dx%d fmt=0x%X d3d=%d pTex=%p", texId, width, height, internalformat, d3dFmt, tex->pTex);
}

void _glsActiveTexture(unsigned int texture)
{
    GLS_State *s = glsGetState();
    s->activeTexUnit = texture;
}

/* ===================================================================
 *  SECTION 3: Buffer Functions
 * =================================================================== */

static GLS_Buffer* _getBoundBuffer(unsigned int target)
{
    GLS_State *s = glsGetState();
    unsigned int id = 0;
    if (target == GL_ARRAY_BUFFER)
        id = s->boundArrayBuffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        id = s->boundElementBuffer;
    else if (target == 0x8F36) /* GL_COPY_READ_BUFFER */
        id = s->boundCopyReadBuffer;
    else if (target == 0x8F37) /* GL_COPY_WRITE_BUFFER */
        id = s->boundCopyWriteBuffer;
    else if (target == 0x8A11) /* GL_UNIFORM_BUFFER */
        id = s->boundUniformBuffer;
    else if (target == 0x8C8E) /* GL_TRANSFORM_FEEDBACK_BUFFER */
        id = s->boundTransformFeedbackBuffer;
    return glsFindBuffer(id);
}

void _glsBufferData(unsigned int target, ptrdiff_t size, const void *data, unsigned int usage)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf) return;

    buf->usage = usage;
    buf->size = size;

    if (buf->data) { free(buf->data); buf->data = NULL; }
    if (size > 0) {
        buf->data = malloc((size_t)size);
        if (buf->data && data) {
            memcpy(buf->data, data, (size_t)size);
        } else if (buf->data) {
            memset(buf->data, 0, (size_t)size);
        }
    }
}

void _glsBufferSubData(unsigned int target, ptrdiff_t offset, ptrdiff_t size, const void *data)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf || !buf->data || !data) return;
    if (offset + size <= buf->size) {
        memcpy((char*)buf->data + offset, data, (size_t)size);
    }
}

void *_glsMapBuffer(unsigned int target, unsigned int access)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    (void)access;
    if (!buf || !buf->data) return NULL;
    buf->mapped = TRUE;
    return buf->data;
}

unsigned char _glsUnmapBuffer(unsigned int target)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf) return 0;
    buf->mapped = FALSE;
    return 1; /* GL_TRUE */
}


/* ===================================================================
 *  SECTION 4: State Functions
 * =================================================================== */

static BOOL* _getEnableFlag(GLS_State *s, unsigned int cap)
{
    int unit;
    switch (cap) {
    case GL_DEPTH_TEST:          return &s->enableDepthTest;
    case GL_BLEND:               return &s->enableBlend;
    case GL_CULL_FACE:           return &s->enableCullFace;
    case GL_SCISSOR_TEST:        return &s->enableScissorTest;
    case GL_STENCIL_TEST:        return &s->enableStencilTest;
    case GL_ALPHA_TEST:          return &s->enableAlphaTest;
    case GL_FOG:                 return &s->enableFog;
    case GL_LIGHTING:            return &s->enableLighting;
    case GL_POLYGON_OFFSET_FILL: return &s->enablePolygonOffsetFill;
    case GL_MULTISAMPLE:         return &s->enableMultisample;
    case GL_COLOR_MATERIAL:      return &s->enableColorMaterial;
    case GL_NORMALIZE:           return &s->enableNormalize;
    case GL_TEXTURE_2D:
        unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
        if (unit >= 0 && unit < GLS_MAX_TEX_UNITS) return &s->enableTexture2D[unit];
        return NULL;
    case GL_TEXTURE_CUBE_MAP:
        unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
        if (unit >= 0 && unit < GLS_MAX_TEX_UNITS) return &s->enableTextureCubeMap[unit];
        return NULL;
    default:
        /* GL_LIGHT0..GL_LIGHT7 */
        if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + GLS_MAX_LIGHTS)
            return &s->lights[cap - GL_LIGHT0].enabled;
        return NULL;
    }
}

void _glsEnable(unsigned int cap)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    BOOL *flag = _getEnableFlag(s, cap);
    if (flag) *flag = TRUE;
    gldDiagLog("GL: glEnable(0x%X)", cap);

    /* Apply D3D9 render state */
    if (pDev) {
        __try {
            switch (cap) {
            case GL_DEPTH_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_TRUE);
                break;
            case GL_BLEND:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE, TRUE);
                break;
            case GL_CULL_FACE:
                _glsApplyD3DCullMode();
                break;
            case GL_SCISSOR_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_SCISSORTESTENABLE, TRUE);
                break;
            case GL_STENCIL_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILENABLE, TRUE);
                break;
            case GL_ALPHA_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHATESTENABLE, TRUE);
                break;
            case GL_LIGHTING:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);
                break;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsDisable(unsigned int cap)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    BOOL *flag = _getEnableFlag(s, cap);
    if (flag) *flag = FALSE;
    gldDiagLog("GL: glDisable(0x%X)", cap);

    /* Apply D3D9 render state */
    if (pDev) {
        __try {
            switch (cap) {
            case GL_DEPTH_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_FALSE);
                break;
            case GL_BLEND:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE, FALSE);
                break;
            case GL_CULL_FACE:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);
                break;
            case GL_SCISSOR_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_SCISSORTESTENABLE, FALSE);
                break;
            case GL_STENCIL_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILENABLE, FALSE);
                break;
            case GL_ALPHA_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHATESTENABLE, FALSE);
                break;
            case GL_LIGHTING:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
                break;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsBlendFunc(unsigned int sfactor, unsigned int dfactor)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->blendSrcRGB = s->blendSrcAlpha = sfactor;
    s->blendDstRGB = s->blendDstAlpha = dfactor;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SRCBLEND, _glsMapBlendFactor(sfactor));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_DESTBLEND, _glsMapBlendFactor(dfactor));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsDepthFunc(unsigned int func)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->depthFunc = func;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZFUNC, _glsMapCompareFunc(func));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsDepthMask(unsigned char flag)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->depthMask = flag;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZWRITEENABLE, flag ? TRUE : FALSE);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsCullFace(unsigned int mode)
{
    GLS_State *s = glsGetState();
    s->cullFaceMode = mode;
    _glsApplyD3DCullMode();
}

void _glsFrontFace(unsigned int mode)
{
    GLS_State *s = glsGetState();
    s->frontFace = mode;
    _glsApplyD3DCullMode();
}

void _glsColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GLS_State *s = glsGetState();
    s->colorMask[0] = r;
    s->colorMask[1] = g;
    s->colorMask[2] = b;
    s->colorMask[3] = a;
}

void _glsAlphaFunc(unsigned int func, float ref)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->alphaFunc = func;
    s->alphaRef = ref;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHAFUNC, _glsMapCompareFunc(func));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHAREF, (DWORD)(ref * 255.0f));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsStencilFunc(unsigned int func, int ref, unsigned int mask)
{
    GLS_State *s = glsGetState();
    s->stencilFunc = func;
    s->stencilRef = ref;
    s->stencilMask = mask;
}

void _glsStencilOp(unsigned int fail, unsigned int zfail, unsigned int zpass)
{
    GLS_State *s = glsGetState();
    s->stencilFail = fail;
    s->stencilZFail = zfail;
    s->stencilZPass = zpass;
}

void _glsPolygonMode(unsigned int face, unsigned int mode)
{
    GLS_State *s = glsGetState();
    if (face == 0x0404 || face == 0x0408) /* GL_FRONT or GL_FRONT_AND_BACK */
        s->polygonModeFront = mode;
    if (face == 0x0405 || face == 0x0408) /* GL_BACK or GL_FRONT_AND_BACK */
        s->polygonModeBack = mode;
}

void _glsPolygonOffset(float factor, float units)
{
    GLS_State *s = glsGetState();
    s->polygonOffsetFactor = factor;
    s->polygonOffsetUnits = units;
}

void _glsLineWidth(float width)
{
    GLS_State *s = glsGetState();
    s->lineWidth = width;
}

void _glsPointSize(float size)
{
    GLS_State *s = glsGetState();
    s->pointSize = size;
}

void _glsScissor(int x, int y, int width, int height)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->scissorX = x;
    s->scissorY = y;
    s->scissorW = width;
    s->scissorH = height;

    if (pDev) {
        RECT rc;
        rc.left = x;
        rc.top = y;
        rc.right = x + width;
        rc.bottom = y + height;
        __try {
            IDirect3DDevice9_SetScissorRect(pDev, &rc);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsViewport(int x, int y, int width, int height)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->viewportX = x;
    s->viewportY = y;
    s->viewportW = width;
    s->viewportH = height;
    gldDiagLog("GL: glViewport(%d, %d, %d, %d)", x, y, width, height);

    if (pDev && width > 0 && height > 0) {
        D3DVIEWPORT9 vp;
        vp.X = x;
        vp.Y = y;
        vp.Width = width;
        vp.Height = height;
        vp.MinZ = s->depthRangeNear;
        vp.MaxZ = s->depthRangeFar;
        __try {
            IDirect3DDevice9_SetViewport(pDev, &vp);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsDepthRange(double nearVal, double farVal)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->depthRangeNear = (float)nearVal;
    s->depthRangeFar = (float)farVal;

    /* Update D3D9 viewport with new depth range */
    if (pDev && s->viewportW > 0 && s->viewportH > 0) {
        D3DVIEWPORT9 vp;
        vp.X = s->viewportX;
        vp.Y = s->viewportY;
        vp.Width = s->viewportW;
        vp.Height = s->viewportH;
        vp.MinZ = (float)nearVal;
        vp.MaxZ = (float)farVal;
        __try {
            IDirect3DDevice9_SetViewport(pDev, &vp);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}


/* ===================================================================
 *  SECTION 5: Clear Functions
 * =================================================================== */

void _glsClearColor(float r, float g, float b, float a)
{
    GLS_State *s = glsGetState();
    s->clearColor[0] = r;
    s->clearColor[1] = g;
    s->clearColor[2] = b;
    s->clearColor[3] = a;
}

void _glsClearDepth(double depth)
{
    GLS_State *s = glsGetState();
    s->clearDepth = (float)depth;
}

void _glsClearStencil(int stencil)
{
    GLS_State *s = glsGetState();
    s->clearStencil = stencil;
}

void _glsClear(unsigned int mask)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD d3dFlags = 0;
    D3DCOLOR clearCol;

    gldDiagLog("GL: glClear(0x%X) pDev=%p", mask, (void*)pDev);

    if (!pDev) return;

    if (mask & GL_COLOR_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_TARGET;
    if (mask & GL_DEPTH_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_ZBUFFER;
    if (mask & GL_STENCIL_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_STENCIL;

    clearCol = D3DCOLOR_COLORVALUE(s->clearColor[0], s->clearColor[1], s->clearColor[2], s->clearColor[3]);

    IDirect3DDevice9_Clear(pDev, 0, NULL, d3dFlags, clearCol, s->clearDepth, s->clearStencil);
}

/* ===================================================================
 *  SECTION 6: Matrix Functions
 * =================================================================== */

void _glsMatrixMode(unsigned int mode)
{
    GLS_State *s = glsGetState();
    s->matrixMode = mode;
}

void _glsLoadIdentity(void)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (stack) glsMatrixIdentity(stack->stack[stack->top].m);
}

void _glsLoadMatrixf(const float *m)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (stack && m) memcpy(stack->stack[stack->top].m, m, 16 * sizeof(float));
}

void _glsLoadMatrixd(const double *m)
{
    float fm[16];
    int i;
    if (!m) return;
    for (i = 0; i < 16; i++) fm[i] = (float)m[i];
    _glsLoadMatrixf(fm);
}

void _glsMultMatrixf(const float *m)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (stack && m) {
        float *cur = stack->stack[stack->top].m;
        glsMatrixMultiply(cur, cur, m);
    }
}

void _glsMultMatrixd(const double *m)
{
    float fm[16];
    int i;
    if (!m) return;
    for (i = 0; i < 16; i++) fm[i] = (float)m[i];
    _glsMultMatrixf(fm);
}

void _glsPushMatrix(void)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (stack && stack->top < GLS_MAX_MATRIX_STACK - 1) {
        memcpy(stack->stack[stack->top + 1].m, stack->stack[stack->top].m, 16 * sizeof(float));
        stack->top++;
    }
}

void _glsPopMatrix(void)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (stack && stack->top > 0) {
        stack->top--;
    }
}

void _glsTranslatef(float x, float y, float z)
{
    float m[16];
    glsMatrixIdentity(m);
    m[12] = x; m[13] = y; m[14] = z;
    _glsMultMatrixf(m);
}

void _glsTranslated(double x, double y, double z)
{
    _glsTranslatef((float)x, (float)y, (float)z);
}

void _glsRotatef(float angle, float x, float y, float z)
{
    float m[16];
    float rad = angle * (float)(M_PI / 180.0);
    float c = (float)cos(rad);
    float s = (float)sin(rad);
    float len = (float)sqrt(x*x + y*y + z*z);
    float ic;

    if (len < 1e-6f) return;
    x /= len; y /= len; z /= len;
    ic = 1.0f - c;

    glsMatrixIdentity(m);
    m[0]  = x*x*ic + c;     m[4]  = x*y*ic - z*s;   m[8]  = x*z*ic + y*s;
    m[1]  = y*x*ic + z*s;   m[5]  = y*y*ic + c;      m[9]  = y*z*ic - x*s;
    m[2]  = z*x*ic - y*s;   m[6]  = z*y*ic + x*s;    m[10] = z*z*ic + c;

    _glsMultMatrixf(m);
}

void _glsRotated(double angle, double x, double y, double z)
{
    _glsRotatef((float)angle, (float)x, (float)y, (float)z);
}

void _glsScalef(float x, float y, float z)
{
    float m[16];
    glsMatrixIdentity(m);
    m[0] = x; m[5] = y; m[10] = z;
    _glsMultMatrixf(m);
}

void _glsScaled(double x, double y, double z)
{
    _glsScalef((float)x, (float)y, (float)z);
}

void _glsOrtho(double l, double r, double b, double t, double n, double f)
{
    float m[16];
    float rl = (float)(r - l);
    float tb = (float)(t - b);
    float fn = (float)(f - n);

    if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return;

    glsMatrixIdentity(m);
    m[0]  = 2.0f / rl;
    m[5]  = 2.0f / tb;
    m[10] = -2.0f / fn;
    m[12] = -(float)(r + l) / rl;
    m[13] = -(float)(t + b) / tb;
    m[14] = -(float)(f + n) / fn;

    _glsMultMatrixf(m);
}

void _glsFrustum(double l, double r, double b, double t, double n, double f)
{
    float m[16];
    float rl = (float)(r - l);
    float tb = (float)(t - b);
    float fn = (float)(f - n);
    float n2 = (float)(2.0 * n);

    if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return;

    memset(m, 0, sizeof(m));
    m[0]  = n2 / rl;
    m[5]  = n2 / tb;
    m[8]  = (float)(r + l) / rl;
    m[9]  = (float)(t + b) / tb;
    m[10] = -(float)(f + n) / fn;
    m[11] = -1.0f;
    m[14] = -(float)(2.0 * f * n) / fn;

    _glsMultMatrixf(m);
}


/* ===================================================================
 *  SECTION 7: Immediate Mode
 * =================================================================== */

static void _ensureImmCapacity(GLS_State *s)
{
    if (s->immVertexCount >= s->immVertexCapacity) {
        int newCap = s->immVertexCapacity * 2;
        GLS_ImmVertex *newBuf = (GLS_ImmVertex *)realloc(s->immVertices, newCap * sizeof(GLS_ImmVertex));
        if (newBuf) {
            s->immVertices = newBuf;
            s->immVertexCapacity = newCap;
        }
    }
}

void _glsBegin(unsigned int mode)
{
    GLS_State *s = glsGetState();
    s->inBeginEnd = TRUE;
    s->beginMode = mode;
    s->immVertexCount = 0;
}

void _glsEnd(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    int primCount;
    int i;

    s->inBeginEnd = FALSE;

    if (!pDev || s->immVertexCount == 0)
        return;

    /* Map GL primitive mode to D3D9 */
    switch (s->beginMode) {
    case 0x0000: /* GL_POINTS */        d3dPrimType = D3DPT_POINTLIST; primCount = s->immVertexCount; break;
    case 0x0001: /* GL_LINES */         d3dPrimType = D3DPT_LINELIST; primCount = s->immVertexCount / 2; break;
    case 0x0003: /* GL_LINE_STRIP */    d3dPrimType = D3DPT_LINESTRIP; primCount = s->immVertexCount - 1; break;
    case 0x0004: /* GL_TRIANGLES */     d3dPrimType = D3DPT_TRIANGLELIST; primCount = s->immVertexCount / 3; break;
    case 0x0005: /* GL_TRIANGLE_STRIP */d3dPrimType = D3DPT_TRIANGLESTRIP; primCount = s->immVertexCount - 2; break;
    case 0x0006: /* GL_TRIANGLE_FAN */  d3dPrimType = D3DPT_TRIANGLEFAN; primCount = s->immVertexCount - 2; break;
    case 0x0007: /* GL_QUADS */         d3dPrimType = D3DPT_TRIANGLELIST; primCount = (s->immVertexCount / 4) * 2; break;
    default: return;
    }

    if (primCount <= 0) return;

    /* Set modelview and projection matrices on D3D9 device (transposed: GL col-major -> D3D row-major) */
    {
        D3DMATRIX d3dWorld, d3dProj;
        _glsTransposeMatrix(&d3dWorld, s->modelviewStack.stack[s->modelviewStack.top].m);
        _glsTransposeMatrix(&d3dProj, s->projectionStack.stack[s->projectionStack.top].m);

        __try {
            IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &d3dWorld);
            IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &d3dProj);
            /* Disable lighting for immediate mode unless explicitly enabled */
            if (!s->enableLighting) {
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }

    /* Use untransformed vertex format: XYZ + DIFFUSE + TEX1 */
    {
        int vertCount;
        GLS_D3DVertex *verts;

        if (s->beginMode == 0x0007) { /* GL_QUADS */
            int quadCount = s->immVertexCount / 4;
            vertCount = quadCount * 6;
            verts = (GLS_D3DVertex *)malloc(vertCount * sizeof(GLS_D3DVertex));
            if (!verts) return;

            for (i = 0; i < quadCount; i++) {
                GLS_ImmVertex *q = &s->immVertices[i * 4];
                GLS_D3DVertex *t = &verts[i * 6];
                int j;
                /* Quad -> 2 triangles: 0-1-2, 0-2-3 */
                int indices[6] = {0, 1, 2, 0, 2, 3};
                for (j = 0; j < 6; j++) {
                    GLS_ImmVertex *sv = &q[indices[j]];
                    t[j].x = sv->pos[0];
                    t[j].y = sv->pos[1];
                    t[j].z = sv->pos[2];
                    t[j].color = D3DCOLOR_COLORVALUE(sv->color[0], sv->color[1], sv->color[2], sv->color[3]);
                    t[j].u = sv->texcoord[0][0];
                    t[j].v = sv->texcoord[0][1];
                }
            }
        } else {
            vertCount = s->immVertexCount;
            verts = (GLS_D3DVertex *)malloc(vertCount * sizeof(GLS_D3DVertex));
            if (!verts) return;

            for (i = 0; i < vertCount; i++) {
                GLS_ImmVertex *sv = &s->immVertices[i];
                verts[i].x = sv->pos[0];
                verts[i].y = sv->pos[1];
                verts[i].z = sv->pos[2];
                verts[i].color = D3DCOLOR_COLORVALUE(sv->color[0], sv->color[1], sv->color[2], sv->color[3]);
                verts[i].u = sv->texcoord[0][0];
                verts[i].v = sv->texcoord[0][1];
            }
        }

        /* Submit to D3D9 */
        __try {
            IDirect3DDevice9_SetFVF(pDev, GLS_D3DFVF);
            IDirect3DDevice9_DrawPrimitiveUP(pDev, d3dPrimType, primCount, verts, sizeof(GLS_D3DVertex));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }

        free(verts);
    }
}

static void _emitVertex(float x, float y, float z, float w)
{
    GLS_State *s = glsGetState();
    GLS_ImmVertex *v;
    int i;

    if (!s->inBeginEnd) return;
    _ensureImmCapacity(s);
    if (s->immVertexCount >= s->immVertexCapacity) return;

    v = &s->immVertices[s->immVertexCount++];
    v->pos[0] = x; v->pos[1] = y; v->pos[2] = z; v->pos[3] = w;
    v->color[0] = s->currentColor[0];
    v->color[1] = s->currentColor[1];
    v->color[2] = s->currentColor[2];
    v->color[3] = s->currentColor[3];
    v->normal[0] = s->currentNormal[0];
    v->normal[1] = s->currentNormal[1];
    v->normal[2] = s->currentNormal[2];
    for (i = 0; i < GLS_MAX_TEX_UNITS; i++) {
        v->texcoord[i][0] = s->currentTexCoord[i][0];
        v->texcoord[i][1] = s->currentTexCoord[i][1];
        v->texcoord[i][2] = s->currentTexCoord[i][2];
        v->texcoord[i][3] = s->currentTexCoord[i][3];
    }
}

void _glsVertex2f(float x, float y) { _emitVertex(x, y, 0.0f, 1.0f); }
void _glsVertex3f(float x, float y, float z) { _emitVertex(x, y, z, 1.0f); }
void _glsVertex4f(float x, float y, float z, float w) { _emitVertex(x, y, z, w); }

void _glsColor3f(float r, float g, float b)
{
    GLS_State *s = glsGetState();
    s->currentColor[0] = r; s->currentColor[1] = g;
    s->currentColor[2] = b; s->currentColor[3] = 1.0f;
}

void _glsColor4f(float r, float g, float b, float a)
{
    GLS_State *s = glsGetState();
    s->currentColor[0] = r; s->currentColor[1] = g;
    s->currentColor[2] = b; s->currentColor[3] = a;
}

void _glsColor3ub(unsigned char r, unsigned char g, unsigned char b)
{
    _glsColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
}

void _glsColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    _glsColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void _glsNormal3f(float nx, float ny, float nz)
{
    GLS_State *s = glsGetState();
    s->currentNormal[0] = nx; s->currentNormal[1] = ny; s->currentNormal[2] = nz;
}

void _glsTexCoord2f(float st, float tt)
{
    GLS_State *s = glsGetState();
    s->currentTexCoord[0][0] = st;
    s->currentTexCoord[0][1] = tt;
    s->currentTexCoord[0][2] = 0.0f;
    s->currentTexCoord[0][3] = 1.0f;
}

void _glsTexCoord3f(float st, float tt, float rt)
{
    GLS_State *s = glsGetState();
    s->currentTexCoord[0][0] = st;
    s->currentTexCoord[0][1] = tt;
    s->currentTexCoord[0][2] = rt;
    s->currentTexCoord[0][3] = 1.0f;
}

void _glsTexCoord4f(float st, float tt, float rt, float qt)
{
    GLS_State *s = glsGetState();
    s->currentTexCoord[0][0] = st;
    s->currentTexCoord[0][1] = tt;
    s->currentTexCoord[0][2] = rt;
    s->currentTexCoord[0][3] = qt;
}

/* ===================================================================
 *  SECTION 8: Shader Functions
 * =================================================================== */

unsigned int _glsCreateShader(unsigned int type)
{
    GLS_State *s = glsGetState();
    unsigned int id = s->nextShaderId++;
    if (id < GLS_MAX_SHADERS) {
        memset(&s->shaders[id], 0, sizeof(GLS_Shader));
        s->shaders[id].id = id;
        s->shaders[id].type = type;
        s->shaders[id].allocated = TRUE;
    }
    gldDiagLog("GL: glCreateShader(0x%X) -> %u", type, id);
    return id;
}

void _glsDeleteShader(unsigned int shader)
{
    GLS_Shader *sh = glsFindShader(shader);
    if (sh) {
        if (sh->source) { free(sh->source); sh->source = NULL; }
        sh->allocated = FALSE;
    }
}

void _glsShaderSource(unsigned int shader, int count, const char *const*string, const int *length)
{
    GLS_Shader *sh = glsFindShader(shader);
    int totalLen = 0;
    int i;
    char *combined;

    if (!sh || !string || count <= 0) return;

    /* Calculate total length */
    for (i = 0; i < count; i++) {
        if (length && length[i] >= 0)
            totalLen += length[i];
        else if (string[i])
            totalLen += (int)strlen(string[i]);
    }

    if (sh->source) { free(sh->source); sh->source = NULL; }
    combined = (char *)malloc(totalLen + 1);
    if (!combined) return;

    combined[0] = '\0';
    for (i = 0; i < count; i++) {
        if (string[i]) {
            if (length && length[i] >= 0) {
                int curLen = (int)strlen(combined);
                memcpy(combined + curLen, string[i], length[i]);
                combined[curLen + length[i]] = '\0';
            } else {
                strcat(combined, string[i]);
            }
        }
    }
    sh->source = combined;
}

void _glsCompileShader(unsigned int shader)
{
    GLS_Shader *sh = glsFindShader(shader);
    if (sh) sh->compiled = TRUE;
    gldDiagLog("GL: glCompileShader(%u) -> compiled=%d", shader, sh ? sh->compiled : -1);
}

void _glsGetShaderiv(unsigned int shader, unsigned int pname, int *params)
{
    GLS_Shader *sh = glsFindShader(shader);
    if (!params) return;

    if (!sh) {
        *params = 0;
        gldDiagLog("GL: glGetShaderiv(%u, 0x%X) -> 0 (NOT FOUND)", shader, pname);
        return;
    }

    switch (pname) {
    case GL_COMPILE_STATUS:       *params = sh->compiled ? GL_TRUE : GL_FALSE; break;
    case GL_SHADER_TYPE:          *params = sh->type; break;
    case 0x8B4E: /* GL_OBJECT_TYPE_ARB */ *params = sh->type; break;
    case GL_DELETE_STATUS:        *params = GL_FALSE; break;
    case GL_INFO_LOG_LENGTH:      *params = 0; break;
    case GL_SHADER_SOURCE_LENGTH: *params = sh->source ? (int)strlen(sh->source) + 1 : 0; break;
    default:                      *params = GL_TRUE; break;
    }
    gldDiagLog("GL: glGetShaderiv(%u, 0x%X) -> %d", shader, pname, *params);
}

void _glsGetShaderInfoLog(unsigned int shader, int bufSize, int *length, char *infoLog)
{
    (void)shader;
    if (length) *length = 0;
    if (infoLog && bufSize > 0) infoLog[0] = '\0';
}

unsigned int _glsCreateProgram(void)
{
    GLS_State *s = glsGetState();
    unsigned int id = s->nextProgramId++;
    if (id < GLS_MAX_PROGRAMS) {
        memset(&s->programs[id], 0, sizeof(GLS_Program));
        s->programs[id].id = id;
        s->programs[id].allocated = TRUE;
    }
    gldDiagLog("GL: glCreateProgram() -> %u", id);
    return id;
}

void _glsDeleteProgram(unsigned int program)
{
    GLS_State *s = glsGetState();
    GLS_Program *prog = glsFindProgram(program);
    if (prog) {
        if (s->boundProgram == program) s->boundProgram = 0;
        prog->allocated = FALSE;
    }
}

void _glsAttachShader(unsigned int program, unsigned int shader)
{
    GLS_Program *prog = glsFindProgram(program);
    GLS_Shader *sh = glsFindShader(shader);
    if (!prog || !sh) return;

    if (sh->type == GL_VERTEX_SHADER)
        prog->vertShader = shader;
    else if (sh->type == GL_FRAGMENT_SHADER)
        prog->fragShader = shader;
}

void _glsLinkProgram(unsigned int program)
{
    GLS_Program *prog = glsFindProgram(program);
    if (prog) prog->linked = TRUE;
    gldDiagLog("GL: glLinkProgram(%u) -> linked=%d", program, prog ? prog->linked : -1);
}

void _glsUseProgram(unsigned int program)
{
    GLS_State *s = glsGetState();
    s->boundProgram = program;
}

void _glsGetProgramiv(unsigned int program, unsigned int pname, int *params)
{
    GLS_Program *prog = glsFindProgram(program);
    if (!params) return;

    if (!prog && program != 0) {
        *params = 0;
        gldDiagLog("GL: glGetProgramiv(%u, 0x%X) -> 0 (NOT FOUND)", program, pname);
        return;
    }

    switch (pname) {
    case GL_LINK_STATUS:      *params = (prog && prog->linked) ? GL_TRUE : GL_FALSE; break;
    case GL_VALIDATE_STATUS:  *params = (prog && prog->validated) ? GL_TRUE : GL_FALSE; break;
    case GL_DELETE_STATUS:    *params = GL_FALSE; break;
    case GL_INFO_LOG_LENGTH:  *params = 0; break;
    case GL_ATTACHED_SHADERS: *params = (prog ? ((prog->vertShader ? 1 : 0) + (prog->fragShader ? 1 : 0)) : 0); break;
    case GL_ACTIVE_UNIFORMS:  *params = (prog ? prog->uniformCount : 0); break;
    case GL_ACTIVE_ATTRIBUTES: *params = 0; break;
    case 0x8B4E: /* GL_OBJECT_TYPE_ARB */ *params = 0x8B40; /* GL_PROGRAM_OBJECT_ARB */ break;
    case GL_COMPILE_STATUS:   *params = GL_TRUE; break;
    default:                  *params = GL_TRUE; break;
    }
    gldDiagLog("GL: glGetProgramiv(%u, 0x%X) -> %d", program, pname, *params);
}

void _glsGetProgramInfoLog(unsigned int program, int bufSize, int *length, char *infoLog)
{
    (void)program;
    if (length) *length = 0;
    if (infoLog && bufSize > 0) infoLog[0] = '\0';
}


/* ===================================================================
 *  SECTION 9: Pixel Store
 * =================================================================== */

void _glsPixelStorei(unsigned int pname, int param)
{
    GLS_State *s = glsGetState();
    switch (pname) {
    case GL_UNPACK_ALIGNMENT:  s->unpackAlignment = param; break;
    case GL_PACK_ALIGNMENT:    s->packAlignment = param; break;
    case GL_UNPACK_ROW_LENGTH: s->unpackRowLength = param; break;
    case GL_PACK_ROW_LENGTH:   s->packRowLength = param; break;
    }
}

/* ===================================================================
 *  SECTION 10: Get Functions
 * =================================================================== */

unsigned int _glsGetError(void)
{
    GLS_State *s = glsGetState();
    unsigned int err = s->lastError;
    s->lastError = GL_NO_ERROR;
    if (err != GL_NO_ERROR)
        gldDiagLog("GL: glGetError() -> 0x%X", err);
    return err;
}

unsigned char _glsIsEnabled(unsigned int cap)
{
    GLS_State *s = glsGetState();
    BOOL *flag = _getEnableFlag(s, cap);
    if (flag) return *flag ? 1 : 0;
    return 0;
}

void _glsGetBooleanv(unsigned int pname, unsigned char *params)
{
    GLS_State *s = glsGetState();
    if (!params) return;

    switch (pname) {
    case 0x0B72: /* GL_DEPTH_WRITEMASK */ *params = s->depthMask; break;
    case 0x0C23: /* GL_COLOR_WRITEMASK */
        params[0] = s->colorMask[0];
        params[1] = s->colorMask[1];
        params[2] = s->colorMask[2];
        params[3] = s->colorMask[3];
        break;
    default: *params = 0; break;
    }
}

void _glsGetFloatv(unsigned int pname, float *params)
{
    GLS_State *s = glsGetState();
    if (!params) return;

    switch (pname) {
    case 0x84FF: /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */ *params = 16.0f; break;
    case 0x0B21: /* GL_LINE_WIDTH */ *params = s->lineWidth; break;
    case 0x0B11: /* GL_POINT_SIZE */ *params = s->pointSize; break;
    case 0x846E: /* GL_ALIASED_POINT_SIZE_RANGE */ params[0] = 1.0f; params[1] = 64.0f; break;
    case 0x846D: /* GL_ALIASED_LINE_WIDTH_RANGE */ params[0] = 1.0f; params[1] = 10.0f; break;
    case 0x0C22: /* GL_COLOR_CLEAR_VALUE */
        params[0] = s->clearColor[0]; params[1] = s->clearColor[1];
        params[2] = s->clearColor[2]; params[3] = s->clearColor[3];
        break;
    case 0x0B73: /* GL_DEPTH_CLEAR_VALUE */ *params = s->clearDepth; break;
    case 0x0B70: /* GL_DEPTH_RANGE */
        params[0] = s->depthRangeNear; params[1] = s->depthRangeFar;
        break;
    case 0x0BA6: /* GL_MODELVIEW_MATRIX */
        memcpy(params, s->modelviewStack.stack[s->modelviewStack.top].m, 16 * sizeof(float));
        break;
    case 0x0BA7: /* GL_PROJECTION_MATRIX */
        memcpy(params, s->projectionStack.stack[s->projectionStack.top].m, 16 * sizeof(float));
        break;
    case 0x2503: /* GL_POLYGON_OFFSET_FACTOR */ *params = s->polygonOffsetFactor; break;
    case 0x2504: /* GL_POLYGON_OFFSET_UNITS */ *params = s->polygonOffsetUnits; break;
    default: *params = 0.0f; break;
    }
    gldDiagLog("GL: glGetFloatv(0x%X) -> %f", pname, *params);
}

void _glsGetIntegerv(unsigned int pname, int *params)
{
    GLS_State *s = glsGetState();
    if (!params) return;

    /* Log before the switch so we see what's being queried */

    switch (pname) {
    /* Version */
    case 0x821B: /* GL_MAJOR_VERSION */ *params = 4; break;
    case 0x821C: /* GL_MINOR_VERSION */ *params = 6; break;

    /* Texture limits */
    case 0x0D33: /* GL_MAX_TEXTURE_SIZE */ *params = 16384; break;
    case 0x851C: /* GL_MAX_TEXTURE_UNITS */ *params = 8; break;
    case 0x8824: /* GL_MAX_DRAW_BUFFERS */ *params = 8; break;
    case 0x8869: /* GL_MAX_VERTEX_ATTRIBS */ *params = 16; break;
    case 0x8872: /* GL_MAX_TEXTURE_IMAGE_UNITS */ *params = 16; break;
    case 0x8B4C: /* GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS */ *params = 16; break;
    case 0x8B49: /* GL_MAX_VERTEX_UNIFORM_COMPONENTS */ *params = 4096; break;
    case 0x8B4A: /* GL_MAX_FRAGMENT_UNIFORM_COMPONENTS */ *params = 4096; break;
    case 0x84E8: /* GL_MAX_RENDERBUFFER_SIZE */ *params = 16384; break;
    case 0x8CDF: /* GL_MAX_COLOR_ATTACHMENTS */ *params = 8; break;

    /* Legacy depth/stencil bits */
    case 0x0B71: /* GL_DEPTH_BITS */ *params = 24; break;
    case 0x0D55: /* GL_STENCIL_BITS */ *params = 8; break;

    /* Extensions */
    case 0x821D: /* GL_NUM_EXTENSIONS */ *params = 0; break;

    /* Legacy limits */
    case 0x0D32: /* GL_MAX_CLIP_PLANES */ *params = 6; break;
    case 0x0D31: /* GL_MAX_LIGHTS */ *params = 8; break;
    case 0x0D30: /* GL_MAX_MODELVIEW_STACK_DEPTH */ *params = 32; break;
    case 0x0D38: /* GL_MAX_PROJECTION_STACK_DEPTH */ *params = 4; break;
    case 0x0D39: /* GL_MAX_TEXTURE_STACK_DEPTH */ *params = 10; break;
    case 0x84E2: /* GL_MAX_TEXTURE_UNITS_ARB */ *params = 8; break;
    case 0x8073: /* GL_MAX_3D_TEXTURE_SIZE */ *params = 2048; break;
    case 0x88FF: /* GL_MAX_CUBE_MAP_TEXTURE_SIZE */ *params = 16384; break;
    case 0x8D57: /* GL_MAX_ARRAY_TEXTURE_LAYERS */ *params = 2048; break;
    case 0x8B4D: /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS */ *params = 32; break;
    case 0x8A2B: /* GL_MAX_UNIFORM_BUFFER_BINDINGS */ *params = 36; break;
    case 0x8A30: /* GL_MAX_UNIFORM_BLOCK_SIZE */ *params = 65536; break;
    case 0x8DFB: /* GL_MAX_VARYING_FLOATS */ *params = 64; break;
    case 0x8871: /* GL_MAX_TEXTURE_COORDS */ *params = 8; break;
    case 0x0BA0: /* GL_MAX_ATTRIB_STACK_DEPTH */ *params = 16; break;
    case 0x0BB0: /* GL_MAX_CLIENT_ATTRIB_STACK_DEPTH */ *params = 16; break;
    case 0x0D36: /* GL_MAX_PIXEL_MAP_TABLE */ *params = 256; break;
    case 0x0D37: /* GL_MAX_NAME_STACK_DEPTH */ *params = 128; break;
    case 0x0D34: /* GL_MAX_LIST_NESTING */ *params = 64; break;
    case 0x0D35: /* GL_MAX_EVAL_ORDER */ *params = 8; break;
    case 0x0D50: /* GL_SUBPIXEL_BITS */ *params = 8; break;

    /* Compressed texture formats */
    case 0x86A5: /* GL_NUM_COMPRESSED_TEXTURE_FORMATS */ *params = 4; break;
    case 0x86A3: /* GL_COMPRESSED_TEXTURE_FORMATS */
        params[0] = 0x83F0; params[1] = 0x83F1;
        params[2] = 0x83F2; params[3] = 0x83F3;
        break;

    /* Color bits */
    case 0x0BA2: /* GL_RED_BITS */ *params = 8; break;
    case 0x0BA3: /* GL_GREEN_BITS */ *params = 8; break;
    case 0x0D52: /* GL_BLUE_BITS */ *params = 8; break;
    case 0x0D54: /* GL_ALPHA_BITS */ *params = 8; break;
    case 0x0D56: /* GL_ACCUM_RED_BITS */ *params = 0; break;
    case 0x0D58: /* GL_ACCUM_BLUE_BITS */ *params = 0; break;
    case 0x0D5A: /* GL_ACCUM_ALPHA_BITS */ *params = 0; break;

    /* Pixel store */
    case GL_UNPACK_ALIGNMENT: *params = s->unpackAlignment; break;
    case GL_PACK_ALIGNMENT:   *params = s->packAlignment; break;

    /* Current state */
    case 0x0BA0 + 0x100: /* not a real enum, fallthrough */
    case 0x0B70: /* GL_DEPTH_FUNC */ *params = s->depthFunc; break;
    case 0x0B46: /* GL_FRONT_FACE */ *params = s->frontFace; break;
    case 0x0B45: /* GL_CULL_FACE_MODE */ *params = s->cullFaceMode; break;
    case 0x0BE0: /* GL_BLEND_SRC */ *params = s->blendSrcRGB; break;
    case 0x0BE1: /* GL_BLEND_DST */ *params = s->blendDstRGB; break;
    case 0x0B91: /* GL_STENCIL_FUNC */ *params = s->stencilFunc; break;
    case 0x0B97: /* GL_STENCIL_REF */ *params = s->stencilRef; break;
    case 0x0B92: /* GL_STENCIL_VALUE_MASK */ *params = s->stencilMask; break;
    case 0x0B94: /* GL_STENCIL_FAIL */ *params = s->stencilFail; break;
    case 0x0B95: /* GL_STENCIL_PASS_DEPTH_FAIL */ *params = s->stencilZFail; break;
    case 0x0B96: /* GL_STENCIL_PASS_DEPTH_PASS */ *params = s->stencilZPass; break;
    case 0x0B98: /* GL_STENCIL_WRITEMASK */ *params = s->stencilWriteMask; break;

    /* Viewport/scissor */
    case 0x0BA2 + 0x200: /* not real, fallthrough */
    case 0x0BA8: /* GL_MATRIX_MODE */ *params = s->matrixMode; break;
    case 0x0BA3 + 0x200: /* not real, fallthrough */
    case 0x84E1: /* GL_ACTIVE_TEXTURE */ *params = s->activeTexUnit; break;
    case 0x8074: /* GL_VIEWPORT */
        params[0] = s->viewportX; params[1] = s->viewportY;
        params[2] = s->viewportW; params[3] = s->viewportH;
        break;
    case 0x0C10: /* GL_SCISSOR_BOX */
        params[0] = s->scissorX; params[1] = s->scissorY;
        params[2] = s->scissorW; params[3] = s->scissorH;
        break;

    /* Current program */
    case 0x8B8D: /* GL_CURRENT_PROGRAM */ *params = s->boundProgram; break;

    /* Bound objects */
    case 0x8069: /* GL_TEXTURE_BINDING_2D */ {
        int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        *params = s->boundTexture2D[unit];
        break;
    }
    case 0x8894: /* GL_ARRAY_BUFFER_BINDING */ *params = s->boundArrayBuffer; break;
    case 0x8895: /* GL_ELEMENT_ARRAY_BUFFER_BINDING */ *params = s->boundElementBuffer; break;
    case 0x85B5: /* GL_VERTEX_ARRAY_BINDING */ *params = s->boundVAO; break;
    case 0x8CA6: /* GL_DRAW_FRAMEBUFFER_BINDING */ *params = s->boundFBO; break;
    case 0x8CA7: /* GL_RENDERBUFFER_BINDING */ *params = s->boundRBO; break;

    /* Values that MUST NOT be zero (games divide by these) */
    case 0x0D3A: /* GL_MAX_VIEWPORT_DIMS */ params[0] = 16384; params[1] = 16384; break;
    case 0x8D6B: /* GL_MAX_SAMPLES */ *params = 4; break;
    case 0x8DFD: /* GL_MAX_ELEMENTS_INDICES */ *params = 65536; break;
    case 0x8DFE: /* GL_MAX_ELEMENTS_VERTICES */ *params = 65536; break;
    case 0x8C2B: /* GL_MAX_TEXTURE_BUFFER_SIZE */ *params = 65536; break;
    case 0x8B4B: /* GL_MAX_VARYING_COMPONENTS */ *params = 64; break;
    case 0x8DE0: /* GL_MAX_GEOMETRY_UNIFORM_COMPONENTS */ *params = 1024; break;
    case 0x8DDF: /* GL_MAX_GEOMETRY_OUTPUT_VERTICES */ *params = 256; break;
    case 0x8C29: /* GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS */ *params = 16; break;
    case 0x9122: /* GL_MAX_COMBINED_UNIFORM_BLOCKS */ *params = 36; break;
    case 0x8A2D: /* GL_MAX_VERTEX_UNIFORM_BLOCKS */ *params = 12; break;
    case 0x8A2E: /* GL_MAX_GEOMETRY_UNIFORM_BLOCKS */ *params = 12; break;
    case 0x8A2F: /* GL_MAX_FRAGMENT_UNIFORM_BLOCKS */ *params = 12; break;
    case 0x8C8A: /* GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS */ *params = 64; break;
    case 0x8C8B: /* GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS */ *params = 4; break;
    case 0x8C80: /* GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS */ *params = 4; break;
    case 0x88FE: /* GL_MAX_VERTEX_ATTRIB_STRIDE */ *params = 2048; break;
    case 0x90D2: /* GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS */ *params = 8; break;
    case 0x90D3: /* GL_MAX_SHADER_STORAGE_BLOCK_SIZE */ *params = 65536; break;
    case 0x90D6: /* GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS */ *params = 8; break;
    case 0x91BB: /* GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS */ *params = 1024; break;
    case 0x8F38: /* GL_MAX_SERVER_WAIT_TIMEOUT */ *params = 0x7FFFFFFF; break;
    case 0x864B: /* GL_MAX_TEXTURE_LOD_BIAS */ *params = 16; break;

    default: *params = 0; break;
    }
    gldDiagLog("GL: glGetIntegerv(0x%X) -> %d", pname, *params);
}


/* ===================================================================
 *  SECTION 11: ARB Program (Assembly Shaders)
 * =================================================================== */

void _glsGenProgramsARB(int n, unsigned int *programs)
{
    /* Reuse shader IDs for ARB programs */
    GLS_State *s = glsGetState();
    int i;
    if (!programs || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextShaderId++;
        if (id < GLS_MAX_SHADERS) {
            memset(&s->shaders[id], 0, sizeof(GLS_Shader));
            s->shaders[id].id = id;
            s->shaders[id].type = 0x8620; /* GL_VERTEX_PROGRAM_ARB */
            s->shaders[id].allocated = TRUE;
        }
        programs[i] = id;
    }
}

void _glsBindProgramARB(unsigned int target, unsigned int program)
{
    GLS_State *s = glsGetState();
    (void)target;
    /* Auto-allocate if needed */
    if (program != 0 && program < GLS_MAX_SHADERS && !s->shaders[program].allocated) {
        memset(&s->shaders[program], 0, sizeof(GLS_Shader));
        s->shaders[program].id = program;
        s->shaders[program].type = target;
        s->shaders[program].allocated = TRUE;
    }
    /* Track as bound program */
    s->boundProgram = program;
}

void _glsProgramStringARB(unsigned int target, unsigned int format, int len, const void *string)
{
    /* Store the program string in the currently bound program's source */
    GLS_State *s = glsGetState();
    GLS_Shader *sh;
    (void)target; (void)format;

    sh = glsFindShader(s->boundProgram);
    if (!sh) return;

    if (sh->source) { free(sh->source); sh->source = NULL; }
    if (string && len > 0) {
        sh->source = (char *)malloc(len + 1);
        if (sh->source) {
            memcpy(sh->source, string, len);
            sh->source[len] = '\0';
        }
    }
    sh->compiled = TRUE;
}

void _glsProgramEnvParameter4fvARB(unsigned int target, unsigned int index, const float *params)
{
    /* Accept and discard — no D3D9 translation yet */
    (void)target; (void)index; (void)params;
}

void _glsProgramLocalParameter4fvARB(unsigned int target, unsigned int index, const float *params)
{
    (void)target; (void)index; (void)params;
}

/* ===================================================================
 *  SECTION 12: ARB Shader Object (handles both shaders and programs)
 * =================================================================== */

void _glsDeleteObjectARB(unsigned int obj)
{
    GLS_State *s = glsGetState();
    /* ARB_shader_objects uses a single namespace for shaders and programs */
    GLS_Shader *sh = glsFindShader(obj);
    if (sh) {
        if (sh->source) { free(sh->source); sh->source = NULL; }
        sh->allocated = FALSE;
        return;
    }
    GLS_Program *prog = glsFindProgram(obj);
    if (prog) {
        if (s->boundProgram == obj) s->boundProgram = 0;
        prog->allocated = FALSE;
    }
}

void _glsGetObjectParameterivARB(unsigned int obj, unsigned int pname, int *params)
{
    if (!params) return;

    /* Try as shader first */
    {
        GLS_Shader *sh = glsFindShader(obj);
        if (sh) {
            _glsGetShaderiv(obj, pname, params);
            return;
        }
    }
    /* Try as program */
    {
        GLS_Program *prog = glsFindProgram(obj);
        if (prog) {
            _glsGetProgramiv(obj, pname, params);
            return;
        }
    }
    *params = 0;
}

void _glsGetInfoLogARB(unsigned int obj, int maxLength, int *length, char *infoLog)
{
    /* Try shader first, then program */
    if (glsFindShader(obj)) {
        _glsGetShaderInfoLog(obj, maxLength, length, infoLog);
        return;
    }
    if (glsFindProgram(obj)) {
        _glsGetProgramInfoLog(obj, maxLength, length, infoLog);
        return;
    }
    if (length) *length = 0;
    if (infoLog && maxLength > 0) infoLog[0] = '\0';
}

/* ===================================================================
 *  SECTION 13: Multitexture
 * =================================================================== */

void _glsMultiTexCoord2fARB(unsigned int target, float s, float t)
{
    GLS_State *st = glsGetState();
    int unit = (target >= 0x84C0) ? (target - 0x84C0) : 0; /* GL_TEXTURE0 */
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;
    st->currentTexCoord[unit][0] = s;
    st->currentTexCoord[unit][1] = t;
    st->currentTexCoord[unit][2] = 0.0f;
    st->currentTexCoord[unit][3] = 1.0f;
}

void _glsMultiTexCoord2fvARB(unsigned int target, const float *v)
{
    if (v) _glsMultiTexCoord2fARB(target, v[0], v[1]);
}

/* ===================================================================
 *  SECTION 14: Stencil Two-Side
 * =================================================================== */

void _glsActiveStencilFaceEXT(unsigned int face)
{
    /* Track which face subsequent stencil ops apply to */
    (void)face;
}


/* ===================================================================
 *  SECTION 15: Uniform Functions
 * =================================================================== */

/* GL attachment/buffer enum constants — guard against glad/gl.h redefinition */
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0    0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT     0x8D00
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT   0x8D20
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_COLOR
#define GL_COLOR                0x1800
#endif
#ifndef GL_DEPTH
#define GL_DEPTH                0x1801
#endif
#ifndef GL_STENCIL
#define GL_STENCIL              0x1802
#endif
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL        0x84F9
#endif
#ifndef GL_FRONT
#define GL_FRONT                0x0404
#endif
#ifndef GL_BACK
#define GL_BACK                 0x0405
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK       0x0408
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED     0x911A
#endif
#ifndef GL_COPY_READ_BUFFER
#define GL_COPY_READ_BUFFER     0x8F36
#endif
#ifndef GL_COPY_WRITE_BUFFER
#define GL_COPY_WRITE_BUFFER    0x8F37
#endif
#ifndef GL_TRANSFORM_FEEDBACK_BUFFER
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8E
#endif
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER       0x8A11
#endif
#ifndef GL_VALIDATE_STATUS
#define GL_VALIDATE_STATUS      0x8B52
#endif
#ifndef GL_ATTACHED_SHADERS
#define GL_ATTACHED_SHADERS     0x8B85
#endif
#ifndef GL_ACTIVE_UNIFORMS
#define GL_ACTIVE_UNIFORMS      0x8B86
#endif
#ifndef GL_ACTIVE_ATTRIBUTES
#define GL_ACTIVE_ATTRIBUTES    0x8B89
#endif

static GLS_Uniform* _findOrCreateUniform(GLS_Program *prog, int location)
{
    int i;
    if (!prog) return NULL;
    /* Find existing */
    for (i = 0; i < prog->uniformCount; i++) {
        if (prog->uniforms[i].location == location)
            return &prog->uniforms[i];
    }
    /* Create new */
    if (prog->uniformCount < GLS_MAX_UNIFORMS) {
        GLS_Uniform *u = &prog->uniforms[prog->uniformCount++];
        memset(u, 0, sizeof(GLS_Uniform));
        u->location = location;
        u->set = TRUE;
        return u;
    }
    return NULL;
}

static GLS_Program* _getBoundProgram(void)
{
    GLS_State *s = glsGetState();
    return glsFindProgram(s->boundProgram);
}

void _glsUniform1i(int loc, int v0)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->set = TRUE; }
}

void _glsUniform2i(int loc, int v0, int v1)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->set = TRUE; }
}

void _glsUniform3i(int loc, int v0, int v1, int v2)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->data[2] = (float)v2; u->set = TRUE; }
}

void _glsUniform4i(int loc, int v0, int v1, int v2, int v3)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->data[2] = (float)v2; u->data[3] = (float)v3; u->set = TRUE; }
}

void _glsUniform1f(int loc, float v0)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 1; u->data[0] = v0; u->set = TRUE; }
}

void _glsUniform2f(int loc, float v0, float v1)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 2; u->data[0] = v0; u->data[1] = v1; u->set = TRUE; }
}

void _glsUniform3f(int loc, float v0, float v1, float v2)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 3; u->data[0] = v0; u->data[1] = v1; u->data[2] = v2; u->set = TRUE; }
}

void _glsUniform4f(int loc, float v0, float v1, float v2, float v3)
{
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 4; u->data[0] = v0; u->data[1] = v1; u->data[2] = v2; u->data[3] = v3; u->set = TRUE; }
}

void _glsUniform1iv(int loc, int count, const int *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v[0]; u->set = TRUE; }
}

void _glsUniform2iv(int loc, int count, const int *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v[0]; u->data[1] = (float)v[1]; u->set = TRUE; }
}

void _glsUniform3iv(int loc, int count, const int *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v[0]; u->data[1] = (float)v[1]; u->data[2] = (float)v[2]; u->set = TRUE; }
}

void _glsUniform4iv(int loc, int count, const int *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v[0]; u->data[1] = (float)v[1]; u->data[2] = (float)v[2]; u->data[3] = (float)v[3]; u->set = TRUE; }
}

void _glsUniform1fv(int loc, int count, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 1; u->data[0] = v[0]; u->set = TRUE; }
}

void _glsUniform2fv(int loc, int count, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 2; u->data[0] = v[0]; u->data[1] = v[1]; u->set = TRUE; }
}

void _glsUniform3fv(int loc, int count, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 3; u->data[0] = v[0]; u->data[1] = v[1]; u->data[2] = v[2]; u->set = TRUE; }
}

void _glsUniform4fv(int loc, int count, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 4; u->data[0] = v[0]; u->data[1] = v[1]; u->data[2] = v[2]; u->data[3] = v[3]; u->set = TRUE; }
}

void _glsUniformMatrix2fv(int loc, int count, unsigned char transpose, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 5; memcpy(u->data, v, 4 * sizeof(float)); u->set = TRUE; }
    (void)transpose;
}

void _glsUniformMatrix3fv(int loc, int count, unsigned char transpose, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 6; memcpy(u->data, v, 9 * sizeof(float)); u->set = TRUE; }
    (void)transpose;
}

void _glsUniformMatrix4fv(int loc, int count, unsigned char transpose, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 7; memcpy(u->data, v, 16 * sizeof(float)); u->set = TRUE; }
    (void)transpose;
}


/* ===================================================================
 *  SECTION 16: Vertex Attrib Functions
 * =================================================================== */

static GLS_VAO* _getBoundVAO(void)
{
    GLS_State *s = glsGetState();
    return glsFindVAO(s->boundVAO);
}

void _glsVertexAttrib1f(unsigned int index, float x)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].defaultValue[0] = x;
        vao->attribs[index].defaultValue[1] = 0.0f;
        vao->attribs[index].defaultValue[2] = 0.0f;
        vao->attribs[index].defaultValue[3] = 1.0f;
    }
}

void _glsVertexAttrib2f(unsigned int index, float x, float y)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].defaultValue[0] = x;
        vao->attribs[index].defaultValue[1] = y;
        vao->attribs[index].defaultValue[2] = 0.0f;
        vao->attribs[index].defaultValue[3] = 1.0f;
    }
}

void _glsVertexAttrib3f(unsigned int index, float x, float y, float z)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].defaultValue[0] = x;
        vao->attribs[index].defaultValue[1] = y;
        vao->attribs[index].defaultValue[2] = z;
        vao->attribs[index].defaultValue[3] = 1.0f;
    }
}

void _glsVertexAttrib4f(unsigned int index, float x, float y, float z, float w)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].defaultValue[0] = x;
        vao->attribs[index].defaultValue[1] = y;
        vao->attribs[index].defaultValue[2] = z;
        vao->attribs[index].defaultValue[3] = w;
    }
}

void _glsVertexAttribPointer(unsigned int index, int size, unsigned int type, unsigned char normalized, int stride, const void *pointer)
{
    GLS_State *s = glsGetState();
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        GLS_VertexAttrib *a = &vao->attribs[index];
        a->size = size;
        a->type = type;
        a->normalized = normalized;
        a->stride = stride;
        a->pointer = pointer;
        a->bufferBinding = s->boundArrayBuffer;
        a->integer = FALSE;
    }
}

void _glsEnableVertexAttribArray(unsigned int index)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].enabled = TRUE;
    }
}

void _glsDisableVertexAttribArray(unsigned int index)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].enabled = FALSE;
    }
}

void _glsVertexAttribIPointer(unsigned int index, int size, unsigned int type, int stride, const void *pointer)
{
    GLS_State *s = glsGetState();
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        GLS_VertexAttrib *a = &vao->attribs[index];
        a->size = size;
        a->type = type;
        a->normalized = FALSE;
        a->stride = stride;
        a->pointer = pointer;
        a->bufferBinding = s->boundArrayBuffer;
        a->integer = TRUE;
    }
}

void _glsVertexAttribDivisor(unsigned int index, unsigned int divisor)
{
    GLS_VAO *vao = _getBoundVAO();
    if (vao && index < GLS_MAX_VERTEX_ATTRIBS) {
        vao->attribs[index].divisor = divisor;
    }
}


/* ===================================================================
 *  SECTION 17: Blend/Stencil Separate
 * =================================================================== */

void _glsBlendEquationSeparate(unsigned int modeRGB, unsigned int modeAlpha)
{
    GLS_State *s = glsGetState();
    s->blendEquationRGB = modeRGB;
    s->blendEquationAlpha = modeAlpha;
}

void _glsBlendEquation(unsigned int mode)
{
    _glsBlendEquationSeparate(mode, mode);
}

void _glsBlendFuncSeparate(unsigned int sfRGB, unsigned int dfRGB, unsigned int sfA, unsigned int dfA)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    s->blendSrcRGB = sfRGB;
    s->blendDstRGB = dfRGB;
    s->blendSrcAlpha = sfA;
    s->blendDstAlpha = dfA;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SRCBLEND, _glsMapBlendFactor(sfRGB));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_DESTBLEND, _glsMapBlendFactor(dfRGB));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SRCBLENDALPHA, _glsMapBlendFactor(sfA));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_DESTBLENDALPHA, _glsMapBlendFactor(dfA));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsBlendColor(float r, float g, float b, float a)
{
    GLS_State *s = glsGetState();
    s->blendColorF[0] = r;
    s->blendColorF[1] = g;
    s->blendColorF[2] = b;
    s->blendColorF[3] = a;
}

void _glsStencilFuncSeparate(unsigned int face, unsigned int func, int ref, unsigned int mask)
{
    GLS_State *s = glsGetState();
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        s->stencilFunc = func;
        s->stencilRef = ref;
        s->stencilMask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        s->stencilBackFunc = func;
        s->stencilBackRef = ref;
        s->stencilBackMask = mask;
    }
}

void _glsStencilOpSeparate(unsigned int face, unsigned int sfail, unsigned int dpfail, unsigned int dppass)
{
    GLS_State *s = glsGetState();
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        s->stencilFail = sfail;
        s->stencilZFail = dpfail;
        s->stencilZPass = dppass;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        s->stencilBackFail = sfail;
        s->stencilBackZFail = dpfail;
        s->stencilBackZPass = dppass;
    }
}

void _glsStencilMaskSeparate(unsigned int face, unsigned int mask)
{
    GLS_State *s = glsGetState();
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        s->stencilWriteMask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        s->stencilBackWriteMask = mask;
    }
}

void _glsDrawBuffers(int n, const unsigned int *bufs)
{
    GLS_State *s = glsGetState();
    int i;
    if (!bufs || n <= 0) return;
    s->drawBufferCount = (n > GLS_MAX_DRAW_BUFFERS) ? GLS_MAX_DRAW_BUFFERS : n;
    for (i = 0; i < s->drawBufferCount; i++) {
        s->drawBuffers[i] = bufs[i];
    }
}


/* ===================================================================
 *  SECTION 18: FBO Attachments
 * =================================================================== */

static GLS_FBO* _getBoundFBO(void)
{
    GLS_State *s = glsGetState();
    return glsFindFBO(s->boundFBO);
}

static int _attachmentIndex(unsigned int attachment)
{
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + 4)
        return (int)(attachment - GL_COLOR_ATTACHMENT0);
    return -1;
}

void _glsFramebufferTexture2D(unsigned int target, unsigned int attachment, unsigned int textarget, unsigned int texture, int level)
{
    GLS_FBO *fbo = _getBoundFBO();
    int idx;
    (void)target;
    if (!fbo) return;

    idx = _attachmentIndex(attachment);
    if (idx >= 0) {
        fbo->colorAttachment[idx] = texture;
        fbo->colorAttachTarget[idx] = textarget;
        fbo->colorAttachLevel[idx] = level;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo->depthAttachment = texture;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo->stencilAttachment = texture;
    } else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        fbo->depthStencilAttachment = texture;
    }
}

void _glsFramebufferRenderbuffer(unsigned int target, unsigned int attachment, unsigned int rbtarget, unsigned int rb)
{
    GLS_FBO *fbo = _getBoundFBO();
    int idx;
    (void)target; (void)rbtarget;
    if (!fbo) return;

    idx = _attachmentIndex(attachment);
    if (idx >= 0) {
        fbo->colorAttachRB[idx] = rb;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo->depthAttachRB = rb;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo->stencilAttachRB = rb;
    } else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        fbo->depthStencilAttachRB = rb;
    }
}

void _glsBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1,
                          int dstX0, int dstY0, int dstX1, int dstY1,
                          unsigned int mask, unsigned int filter)
{
    /* Accept params, no-op for now */
    (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1;
    (void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1;
    (void)mask; (void)filter;
}

void _glsFramebufferTexture(unsigned int target, unsigned int attachment, unsigned int texture, int level)
{
    /* Treat like FramebufferTexture2D with textarget=GL_TEXTURE_2D */
    _glsFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);
}


/* ===================================================================
 *  SECTION 19: RBO Multisample
 * =================================================================== */

void _glsRenderbufferStorageMultisample(unsigned int target, int samples, unsigned int fmt, int w, int h)
{
    GLS_State *s = glsGetState();
    GLS_RBO *rbo = glsFindRBO(s->boundRBO);
    (void)target;
    if (rbo) {
        rbo->internalFormat = fmt;
        rbo->width = w;
        rbo->height = h;
        rbo->samples = samples;
    }
}

/* ===================================================================
 *  SECTION 20: Clear Buffers (GL 3.0)
 * =================================================================== */

void _glsClearBufferfv(unsigned int buffer, int drawbuffer, const float *value)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev || !value) return;

    if (buffer == GL_COLOR) {
        D3DCOLOR clearCol = D3DCOLOR_COLORVALUE(value[0], value[1], value[2], value[3]);
        IDirect3DDevice9_Clear(pDev, 0, NULL, D3DCLEAR_TARGET, clearCol, s->clearDepth, s->clearStencil);
    } else if (buffer == GL_DEPTH) {
        IDirect3DDevice9_Clear(pDev, 0, NULL, D3DCLEAR_ZBUFFER, 0, value[0], s->clearStencil);
    }
    (void)drawbuffer;
}

void _glsClearBufferiv(unsigned int buffer, int drawbuffer, const int *value)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev || !value) return;

    if (buffer == GL_STENCIL) {
        IDirect3DDevice9_Clear(pDev, 0, NULL, D3DCLEAR_STENCIL, 0, s->clearDepth, value[0]);
    }
    (void)drawbuffer;
}

void _glsClearBufferuiv(unsigned int buffer, int drawbuffer, const unsigned int *value)
{
    /* Same pattern as ClearBufferiv for stencil */
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev || !value) return;

    if (buffer == GL_STENCIL) {
        IDirect3DDevice9_Clear(pDev, 0, NULL, D3DCLEAR_STENCIL, 0, s->clearDepth, (int)value[0]);
    }
    (void)drawbuffer;
}

void _glsClearBufferfi(unsigned int buffer, int drawbuffer, float depth, int stencil)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev) return;

    if (buffer == GL_DEPTH_STENCIL) {
        IDirect3DDevice9_Clear(pDev, 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, depth, stencil);
    }
    (void)drawbuffer;
}


/* ===================================================================
 *  SECTION 21: Buffer Range / Mipmap / Buffer Bindings
 * =================================================================== */

void *_glsMapBufferRange(unsigned int target, ptrdiff_t offset, ptrdiff_t length, unsigned int access)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    (void)access; (void)length;
    if (!buf || !buf->data) return NULL;
    if (offset >= buf->size) return NULL;
    buf->mapped = TRUE;
    return (char*)buf->data + offset;
}

void _glsFlushMappedBufferRange(unsigned int target, ptrdiff_t offset, ptrdiff_t length)
{
    /* Accept, no-op */
    (void)target; (void)offset; (void)length;
}

void _glsGenerateMipmap(unsigned int target)
{
    /* Accept, no-op for now */
    (void)target;
}

void _glsBindBufferBase(unsigned int target, unsigned int index, unsigned int buffer)
{
    GLS_State *s = glsGetState();
    (void)index;
    if (target == GL_UNIFORM_BUFFER)
        s->boundUniformBuffer = buffer;
    else if (target == GL_TRANSFORM_FEEDBACK_BUFFER)
        s->boundTransformFeedbackBuffer = buffer;
    /* Also bind to the generic target */
    _glsBindBuffer(target, buffer);
}

void _glsBindBufferRange(unsigned int target, unsigned int index, unsigned int buffer, ptrdiff_t offset, ptrdiff_t size)
{
    (void)offset; (void)size;
    _glsBindBufferBase(target, index, buffer);
}

/* ===================================================================
 *  SECTION 22: Transform Feedback
 * =================================================================== */

void _glsBeginTransformFeedback(unsigned int primitiveMode)
{
    GLS_State *s = glsGetState();
    s->transformFeedbackActive = TRUE;
    s->transformFeedbackMode = primitiveMode;
}

void _glsEndTransformFeedback(void)
{
    GLS_State *s = glsGetState();
    s->transformFeedbackActive = FALSE;
}

void _glsTransformFeedbackVaryings(unsigned int program, int count, const char *const*varyings, unsigned int bufferMode)
{
    /* Accept and store — no rendering yet */
    (void)program; (void)count; (void)varyings; (void)bufferMode;
}

/* ===================================================================
 *  SECTION 23: Validate / Is / BindAttrib
 * =================================================================== */

void _glsValidateProgram(unsigned int program)
{
    GLS_Program *prog = glsFindProgram(program);
    if (prog) prog->validated = TRUE;
}

unsigned char _glsIsShader(unsigned int shader)
{
    GLS_Shader *sh = glsFindShader(shader);
    return (sh && sh->allocated) ? GL_TRUE : GL_FALSE;
}

unsigned char _glsIsProgram(unsigned int program)
{
    GLS_Program *prog = glsFindProgram(program);
    return (prog && prog->allocated) ? GL_TRUE : GL_FALSE;
}

void _glsBindAttribLocation(unsigned int program, unsigned int index, const char *name)
{
    GLS_Program *prog = glsFindProgram(program);
    if (!prog || !name) return;
    if (prog->attribBindingCount < GLS_MAX_ATTRIB_BINDINGS) {
        GLS_AttribBinding *ab = &prog->attribBindings[prog->attribBindingCount++];
        ab->index = index;
        strncpy(ab->name, name, sizeof(ab->name) - 1);
        ab->name[sizeof(ab->name) - 1] = '\0';
        ab->set = TRUE;
    }
}


/* ===================================================================
 *  SECTION 24: GL 3.1+ — Instanced Draw / CopyBuffer / TexBuffer / PrimitiveRestart
 * =================================================================== */

void _glsDrawArraysInstanced(unsigned int mode, int first, int count, int instancecount)
{
    /* Accept params, store state — no rendering yet */
    (void)mode; (void)first; (void)count; (void)instancecount;
}

void _glsDrawElementsInstanced(unsigned int mode, int count, unsigned int type, const void *indices, int instancecount)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)instancecount;
}

void _glsCopyBufferSubData(unsigned int readTarget, unsigned int writeTarget, ptrdiff_t readOffset, ptrdiff_t writeOffset, ptrdiff_t size)
{
    GLS_Buffer *readBuf, *writeBuf;
    GLS_State *s = glsGetState();

    /* Resolve read/write targets */
    if (readTarget == GL_COPY_READ_BUFFER) {
        readBuf = glsFindBuffer(s->boundCopyReadBuffer);
    } else {
        readBuf = _getBoundBuffer(readTarget);
    }
    if (writeTarget == GL_COPY_WRITE_BUFFER) {
        writeBuf = glsFindBuffer(s->boundCopyWriteBuffer);
    } else {
        writeBuf = _getBoundBuffer(writeTarget);
    }

    if (readBuf && writeBuf && readBuf->data && writeBuf->data) {
        if (readOffset + size <= readBuf->size && writeOffset + size <= writeBuf->size) {
            memmove((char*)writeBuf->data + writeOffset, (char*)readBuf->data + readOffset, (size_t)size);
        }
    }
}

void _glsTexBuffer(unsigned int target, unsigned int internalformat, unsigned int buffer)
{
    /* Accept params, no-op for now */
    (void)target; (void)internalformat; (void)buffer;
}

void _glsPrimitiveRestartIndex(unsigned int index)
{
    GLS_State *s = glsGetState();
    s->primitiveRestartIndex = index;
}

void _glsUniformBlockBinding(unsigned int program, unsigned int blockIndex, unsigned int blockBinding)
{
    /* Accept params, no-op for now */
    (void)program; (void)blockIndex; (void)blockBinding;
}

/* ===================================================================
 *  SECTION 25: GL 3.2 — Sync
 * =================================================================== */

void *_glsFenceSync(unsigned int condition, unsigned int flags)
{
    GLS_State *s = glsGetState();
    /* Return a dummy non-NULL handle */
    unsigned int id = ++s->nextSyncId;
    (void)condition; (void)flags;
    return (void*)(uintptr_t)id;
}

void _glsDeleteSync(void *sync)
{
    (void)sync;
}

unsigned int _glsClientWaitSync(void *sync, unsigned int flags, unsigned long long timeout)
{
    (void)sync; (void)flags; (void)timeout;
    return GL_ALREADY_SIGNALED;
}

void _glsWaitSync(void *sync, unsigned int flags, unsigned long long timeout)
{
    (void)sync; (void)flags; (void)timeout;
}

void _glsProvokingVertex(unsigned int mode)
{
    GLS_State *s = glsGetState();
    s->provokingVertexMode = mode;
}


/* ===================================================================
 *  SECTION 26: GL 3.3 — Samplers
 * =================================================================== */

void _glsBindSampler(unsigned int unit, unsigned int sampler)
{
    GLS_State *s = glsGetState();
    if (unit < GLS_MAX_TEX_UNITS) {
        s->boundSampler[unit] = sampler;
    }
}

static void _setSamplerParam(GLS_Sampler *samp, unsigned int pname, float value)
{
    if (!samp) return;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: samp->minFilter = (unsigned int)value; break;
    case GL_TEXTURE_MAG_FILTER: samp->magFilter = (unsigned int)value; break;
    case GL_TEXTURE_WRAP_S:     samp->wrapS = (unsigned int)value; break;
    case GL_TEXTURE_WRAP_T:     samp->wrapT = (unsigned int)value; break;
    case GL_TEXTURE_WRAP_R:     samp->wrapR = (unsigned int)value; break;
    case 0x84FE: /* GL_TEXTURE_MAX_ANISOTROPY_EXT */ samp->maxAnisotropy = value; break;
    case 0x884C: /* GL_TEXTURE_COMPARE_MODE */ samp->compareMode = (unsigned int)value; break;
    case 0x884D: /* GL_TEXTURE_COMPARE_FUNC */ samp->compareFunc = (unsigned int)value; break;
    case 0x813A: /* GL_TEXTURE_MIN_LOD */ samp->minLod = value; break;
    case 0x813B: /* GL_TEXTURE_MAX_LOD */ samp->maxLod = value; break;
    case 0x8501: /* GL_TEXTURE_LOD_BIAS */ samp->lodBias = value; break;
    }
}

void _glsSamplerParameteri(unsigned int sampler, unsigned int pname, int param)
{
    GLS_Sampler *samp = glsFindSampler(sampler);
    _setSamplerParam(samp, pname, (float)param);
}

void _glsSamplerParameterf(unsigned int sampler, unsigned int pname, float param)
{
    GLS_Sampler *samp = glsFindSampler(sampler);
    _setSamplerParam(samp, pname, param);
}

void _glsSamplerParameteriv(unsigned int sampler, unsigned int pname, const int *param)
{
    GLS_Sampler *samp = glsFindSampler(sampler);
    if (!param) return;
    if (pname == 0x1004) { /* GL_TEXTURE_BORDER_COLOR */
        if (samp) {
            samp->borderColor[0] = (float)param[0];
            samp->borderColor[1] = (float)param[1];
            samp->borderColor[2] = (float)param[2];
            samp->borderColor[3] = (float)param[3];
        }
    } else {
        _setSamplerParam(samp, pname, (float)param[0]);
    }
}

void _glsSamplerParameterfv(unsigned int sampler, unsigned int pname, const float *param)
{
    GLS_Sampler *samp = glsFindSampler(sampler);
    if (!param) return;
    if (pname == 0x1004) { /* GL_TEXTURE_BORDER_COLOR */
        if (samp) {
            samp->borderColor[0] = param[0];
            samp->borderColor[1] = param[1];
            samp->borderColor[2] = param[2];
            samp->borderColor[3] = param[3];
        }
    } else {
        _setSamplerParam(samp, pname, param[0]);
    }
}

/* ===================================================================
 *  SECTION 27: GL 4.x
 * =================================================================== */

void _glsTexStorage2D(unsigned int target, int levels, unsigned int internalformat, int width, int height)
{
    GLS_State *s = glsGetState();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    texId = s->boundTexture2D[unit];
    tex = glsFindTexture(texId);

    if (tex) {
        tex->width = width;
        tex->height = height;
        tex->internalFormat = internalformat;
    }
    (void)target; (void)levels;
}

void _glsTexStorage3D(unsigned int target, int levels, unsigned int internalformat, int width, int height, int depth)
{
    GLS_State *s = glsGetState();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    texId = s->boundTexture2D[unit];
    tex = glsFindTexture(texId);

    if (tex) {
        tex->width = width;
        tex->height = height;
        tex->depth = depth;
        tex->internalFormat = internalformat;
    }
    (void)target; (void)levels;
}

void _glsDispatchCompute(unsigned int x, unsigned int y, unsigned int z)
{
    /* Accept params, no-op */
    (void)x; (void)y; (void)z;
}

void _glsDebugMessageCallback(void *callback, const void *userParam)
{
    GLS_State *s = glsGetState();
    s->debugCallback = callback;
    s->debugUserParam = userParam;
}

void _glsClipControl(unsigned int origin, unsigned int depth)
{
    GLS_State *s = glsGetState();
    s->clipOrigin = origin;
    s->clipDepthMode = depth;
}


/* ===================================================================
 *  SECTION 28: Conditional Render / Indexed State
 * =================================================================== */

void _glsBeginConditionalRender(unsigned int id, unsigned int mode)
{
    (void)id; (void)mode;
}

void _glsEndConditionalRender(void)
{
}

void _glsColorMaski(unsigned int index, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    /* For index 0, store in main state */
    if (index == 0) {
        GLS_State *s = glsGetState();
        s->colorMask[0] = r;
        s->colorMask[1] = g;
        s->colorMask[2] = b;
        s->colorMask[3] = a;
    }
}

void _glsEnablei(unsigned int target, unsigned int index)
{
    (void)target; (void)index;
}

void _glsDisablei(unsigned int target, unsigned int index)
{
    (void)target; (void)index;
}

/* ===================================================================
 *  SECTION 29: Queries
 * =================================================================== */

void _glsBeginQuery(unsigned int target, unsigned int id)
{
    GLS_Query *q = glsFindQuery(id);
    if (q) {
        q->target = target;
        q->active = TRUE;
    }
}

void _glsEndQuery(unsigned int target)
{
    GLS_State *s = glsGetState();
    int i;
    for (i = 0; i < GLS_MAX_QUERIES; i++) {
        if (s->queries[i].allocated && s->queries[i].active && s->queries[i].target == target) {
            s->queries[i].active = FALSE;
            s->queries[i].result = 0;
            break;
        }
    }
}

void _glsQueryCounter(unsigned int id, unsigned int target)
{
    GLS_Query *q = glsFindQuery(id);
    if (q) {
        q->target = target;
        q->result = 0;
    }
}

/* ===================================================================
 *  SECTION 30: GL 3.2 Multisample Textures
 * =================================================================== */

void _glsTexImage2DMultisample(unsigned int target, int samples, unsigned int internalformat, int width, int height, unsigned char fixedsamplelocations)
{
    (void)target; (void)samples; (void)internalformat;
    (void)width; (void)height; (void)fixedsamplelocations;
}

void _glsTexImage3DMultisample(unsigned int target, int samples, unsigned int internalformat, int width, int height, int depth, unsigned char fixedsamplelocations)
{
    (void)target; (void)samples; (void)internalformat;
    (void)width; (void)height; (void)depth; (void)fixedsamplelocations;
}

void _glsSampleMaski(unsigned int maskNumber, unsigned int mask)
{
    (void)maskNumber; (void)mask;
}

/* ===================================================================
 *  SECTION 31: GL 4.x Misc
 * =================================================================== */

void _glsMinSampleShading(float value)
{
    (void)value;
}

void _glsBlendEquationi(unsigned int buf, unsigned int mode)
{
    /* For buf 0, store in main state */
    if (buf == 0) {
        GLS_State *s = glsGetState();
        s->blendEquationRGB = mode;
        s->blendEquationAlpha = mode;
    }
}

void _glsBlendFunci(unsigned int buf, unsigned int src, unsigned int dst)
{
    if (buf == 0) {
        GLS_State *s = glsGetState();
        s->blendSrcRGB = s->blendSrcAlpha = src;
        s->blendDstRGB = s->blendDstAlpha = dst;
    }
}

void _glsPatchParameteri(unsigned int pname, int value)
{
    (void)pname; (void)value;
}

void _glsMemoryBarrier(unsigned int barriers)
{
    (void)barriers;
}

void _glsBindImageTexture(unsigned int unit, unsigned int texture, int level, unsigned char layered, int layer, unsigned int access, unsigned int format)
{
    (void)unit; (void)texture; (void)level; (void)layered;
    (void)layer; (void)access; (void)format;
}

void _glsDebugMessageControl(unsigned int source, unsigned int type, unsigned int severity, int count, const unsigned int *ids, unsigned char enabled)
{
    (void)source; (void)type; (void)severity;
    (void)count; (void)ids; (void)enabled;
}

void _glsObjectLabel(unsigned int identifier, unsigned int name, int length, const char *label)
{
    (void)identifier; (void)name; (void)length; (void)label;
}

void _glsTextureBarrier(void)
{
}


/* ===================================================================
 *  SECTION 32: Texture 3D / Compressed / Misc Legacy
 * =================================================================== */

void _glsTexImage3D(unsigned int target, int level, int internalformat, int width, int height, int depth, int border, unsigned int format, unsigned int type, const void *pixels)
{
    (void)target; (void)level; (void)internalformat;
    (void)width; (void)height; (void)depth; (void)border;
    (void)format; (void)type; (void)pixels;
}

void _glsTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, unsigned int type, const void *pixels)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset;
    (void)width; (void)height; (void)depth; (void)format; (void)type; (void)pixels;
}

void _glsCompressedTexImage3D(unsigned int target, int level, unsigned int internalformat, int width, int height, int depth, int border, int imageSize, const void *data)
{
    (void)target; (void)level; (void)internalformat;
    (void)width; (void)height; (void)depth; (void)border;
    (void)imageSize; (void)data;
}

void _glsCompressedTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, int imageSize, const void *data)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset;
    (void)width; (void)height; (void)format; (void)imageSize; (void)data;
}

void _glsCompressedTexImage1D(unsigned int target, int level, unsigned int internalformat, int width, int border, int imageSize, const void *data)
{
    (void)target; (void)level; (void)internalformat;
    (void)width; (void)border; (void)imageSize; (void)data;
}

void _glsCompressedTexSubImage1D(unsigned int target, int level, int xoffset, int width, unsigned int format, int imageSize, const void *data)
{
    (void)target; (void)level; (void)xoffset;
    (void)width; (void)format; (void)imageSize; (void)data;
}

void _glsCompressedTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, int imageSize, const void *data)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset;
    (void)width; (void)height; (void)depth; (void)format; (void)imageSize; (void)data;
}

void _glsGetCompressedTexImage(unsigned int target, int level, void *img)
{
    (void)target; (void)level; (void)img;
}

void _glsCopyTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int x, int y, int width, int height)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset;
    (void)x; (void)y; (void)width; (void)height;
}

void _glsDrawRangeElements(unsigned int mode, unsigned int start, unsigned int end, int count, unsigned int type, const void *indices)
{
    (void)mode; (void)start; (void)end; (void)count; (void)type; (void)indices;
}

void _glsSampleCoverage(float value, unsigned char invert)
{
    (void)value; (void)invert;
}

void _glsClientActiveTexture(unsigned int texture)
{
    /* Track client active texture unit for legacy vertex arrays */
    (void)texture;
}

void _glsLoadTransposeMatrixf(const float *m)
{
    float t[16];
    int i, j;
    if (!m) return;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t[j * 4 + i] = m[i * 4 + j];
    _glsLoadMatrixf(t);
}

void _glsLoadTransposeMatrixd(const double *m)
{
    float t[16];
    int i, j;
    if (!m) return;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t[j * 4 + i] = (float)m[i * 4 + j];
    _glsLoadMatrixf(t);
}

void _glsMultTransposeMatrixf(const float *m)
{
    float t[16];
    int i, j;
    if (!m) return;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t[j * 4 + i] = m[i * 4 + j];
    _glsMultMatrixf(t);
}

void _glsMultTransposeMatrixd(const double *m)
{
    float t[16];
    int i, j;
    if (!m) return;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            t[j * 4 + i] = (float)m[i * 4 + j];
    _glsMultMatrixf(t);
}

void _glsFogCoordf(float coord)
{
    (void)coord;
}

void _glsFogCoordfv(const float *coord)
{
    (void)coord;
}

void _glsFogCoordd(double coord)
{
    (void)coord;
}

void _glsFogCoorddv(const double *coord)
{
    (void)coord;
}

void _glsFogCoordPointer(unsigned int type, int stride, const void *pointer)
{
    (void)type; (void)stride; (void)pointer;
}

void _glsSecondaryColor3f(float r, float g, float b)
{
    (void)r; (void)g; (void)b;
}

void _glsSecondaryColor3fv(const float *v)
{
    (void)v;
}

void _glsSecondaryColor3ub(unsigned char r, unsigned char g, unsigned char b)
{
    (void)r; (void)g; (void)b;
}

void _glsSecondaryColor3ubv(const unsigned char *v)
{
    (void)v;
}

void _glsSecondaryColorPointer(int size, unsigned int type, int stride, const void *pointer)
{
    (void)size; (void)type; (void)stride; (void)pointer;
}

void _glsMultiDrawArrays(unsigned int mode, const int *first, const int *count, int drawcount)
{
    (void)mode; (void)first; (void)count; (void)drawcount;
}

void _glsMultiDrawElements(unsigned int mode, const int *count, unsigned int type, const void *const*indices, int drawcount)
{
    (void)mode; (void)count; (void)type; (void)indices; (void)drawcount;
}

void _glsPointParameterf(unsigned int pname, float param)
{
    (void)pname; (void)param;
}

void _glsPointParameterfv(unsigned int pname, const float *params)
{
    (void)pname; (void)params;
}

void _glsPointParameteri(unsigned int pname, int param)
{
    (void)pname; (void)param;
}

void _glsPointParameteriv(unsigned int pname, const int *params)
{
    (void)pname; (void)params;
}

void _glsGetBufferSubData(unsigned int target, ptrdiff_t offset, ptrdiff_t size, void *data)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf || !buf->data || !data) return;
    if (offset + size <= buf->size) {
        memcpy(data, (char*)buf->data + offset, (size_t)size);
    }
}

/* ===================================================================
 *  SECTION: Fog state
 * =================================================================== */

#ifndef GL_FOG_MODE
#define GL_FOG_MODE             0x0B65
#endif
#ifndef GL_FOG_DENSITY
#define GL_FOG_DENSITY          0x0B62
#endif
#ifndef GL_FOG_START
#define GL_FOG_START            0x0B63
#endif
#ifndef GL_FOG_END
#define GL_FOG_END              0x0B64
#endif
#ifndef GL_FOG_COLOR
#define GL_FOG_COLOR            0x0B66
#endif
#ifndef GL_FOG_INDEX
#define GL_FOG_INDEX            0x0B61
#endif
#ifndef GL_FOG_COORD_SRC
#define GL_FOG_COORD_SRC        0x8450
#endif

static void _setFogParam(GLS_State *s, unsigned int pname, float value)
{
    switch (pname) {
    case GL_FOG_MODE:    s->fogMode = (GLenum_t)(int)value; break;
    case GL_FOG_DENSITY: s->fogDensity = value; break;
    case GL_FOG_START:   s->fogStart = value; break;
    case GL_FOG_END:     s->fogEnd = value; break;
    default: break;
    }
}

void _glsFogf(unsigned int pname, float param)
{
    GLS_State *s = glsGetState(); if (!s) return;
    _setFogParam(s, pname, param);
}

void _glsFogi(unsigned int pname, int param)
{
    GLS_State *s = glsGetState(); if (!s) return;
    _setFogParam(s, pname, (float)param);
}

void _glsFogfv(unsigned int pname, const float *params)
{
    GLS_State *s = glsGetState(); if (!s || !params) return;
    if (pname == GL_FOG_COLOR) {
        s->fogColor[0] = params[0];
        s->fogColor[1] = params[1];
        s->fogColor[2] = params[2];
        s->fogColor[3] = params[3];
    } else {
        _setFogParam(s, pname, params[0]);
    }
}

void _glsFogiv(unsigned int pname, const int *params)
{
    GLS_State *s = glsGetState(); if (!s || !params) return;
    if (pname == GL_FOG_COLOR) {
        s->fogColor[0] = params[0] / 2147483647.0f;
        s->fogColor[1] = params[1] / 2147483647.0f;
        s->fogColor[2] = params[2] / 2147483647.0f;
        s->fogColor[3] = params[3] / 2147483647.0f;
    } else {
        _setFogParam(s, pname, (float)params[0]);
    }
}

/* ===================================================================
 *  SECTION: Light state
 * =================================================================== */

#ifndef GL_AMBIENT
#define GL_AMBIENT              0x1200
#endif
#ifndef GL_DIFFUSE
#define GL_DIFFUSE              0x1201
#endif
#ifndef GL_SPECULAR
#define GL_SPECULAR             0x1202
#endif
#ifndef GL_POSITION
#define GL_POSITION             0x1203
#endif
#ifndef GL_SPOT_DIRECTION
#define GL_SPOT_DIRECTION       0x1204
#endif
#ifndef GL_SPOT_EXPONENT
#define GL_SPOT_EXPONENT        0x1205
#endif
#ifndef GL_SPOT_CUTOFF
#define GL_SPOT_CUTOFF          0x1206
#endif
#ifndef GL_CONSTANT_ATTENUATION
#define GL_CONSTANT_ATTENUATION 0x1207
#endif
#ifndef GL_LINEAR_ATTENUATION
#define GL_LINEAR_ATTENUATION   0x1208
#endif
#ifndef GL_QUADRATIC_ATTENUATION
#define GL_QUADRATIC_ATTENUATION 0x1209
#endif
#ifndef GL_LIGHT_MODEL_AMBIENT
#define GL_LIGHT_MODEL_AMBIENT  0x0B53
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#endif
#ifndef GL_LIGHT_MODEL_LOCAL_VIEWER
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#endif

static GLS_Light* _getLight(unsigned int light)
{
    GLS_State *s = glsGetState();
    if (!s) return NULL;
    unsigned int idx = light - GL_LIGHT0;
    if (idx >= GLS_MAX_LIGHTS) return NULL;
    return &s->lights[idx];
}

void _glsLightf(unsigned int light, unsigned int pname, float param)
{
    GLS_Light *l = _getLight(light);
    if (!l) return;
    switch (pname) {
    case GL_SPOT_EXPONENT:          l->spotExponent = param; break;
    case GL_SPOT_CUTOFF:            l->spotCutoff = param; break;
    case GL_CONSTANT_ATTENUATION:   l->constantAttenuation = param; break;
    case GL_LINEAR_ATTENUATION:     l->linearAttenuation = param; break;
    case GL_QUADRATIC_ATTENUATION:  l->quadraticAttenuation = param; break;
    default: break;
    }
}

void _glsLightfv(unsigned int light, unsigned int pname, const float *params)
{
    GLS_Light *l = _getLight(light);
    if (!l || !params) return;
    switch (pname) {
    case GL_AMBIENT:
        l->ambient[0] = params[0]; l->ambient[1] = params[1];
        l->ambient[2] = params[2]; l->ambient[3] = params[3]; break;
    case GL_DIFFUSE:
        l->diffuse[0] = params[0]; l->diffuse[1] = params[1];
        l->diffuse[2] = params[2]; l->diffuse[3] = params[3]; break;
    case GL_SPECULAR:
        l->specular[0] = params[0]; l->specular[1] = params[1];
        l->specular[2] = params[2]; l->specular[3] = params[3]; break;
    case GL_POSITION:
        l->position[0] = params[0]; l->position[1] = params[1];
        l->position[2] = params[2]; l->position[3] = params[3]; break;
    case GL_SPOT_DIRECTION:
        l->spotDirection[0] = params[0]; l->spotDirection[1] = params[1];
        l->spotDirection[2] = params[2]; break;
    default:
        _glsLightf(light, pname, params[0]); break;
    }
}

void _glsLightModelf(unsigned int pname, float param)
{
    GLS_State *s = glsGetState(); if (!s) return;
    switch (pname) {
    case GL_LIGHT_MODEL_TWO_SIDE:
        s->lightModelTwoSide = (param != 0.0f); break;
    default: break;
    }
}

void _glsLightModelfv(unsigned int pname, const float *params)
{
    GLS_State *s = glsGetState(); if (!s || !params) return;
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        s->lightModelAmbient[0] = params[0]; s->lightModelAmbient[1] = params[1];
        s->lightModelAmbient[2] = params[2]; s->lightModelAmbient[3] = params[3]; break;
    default:
        _glsLightModelf(pname, params[0]); break;
    }
}

void _glsGetLightfv(unsigned int light, unsigned int pname, float *params)
{
    GLS_Light *l = _getLight(light);
    if (!l || !params) return;
    switch (pname) {
    case GL_AMBIENT:  memcpy(params, l->ambient, 4*sizeof(float)); break;
    case GL_DIFFUSE:  memcpy(params, l->diffuse, 4*sizeof(float)); break;
    case GL_SPECULAR: memcpy(params, l->specular, 4*sizeof(float)); break;
    case GL_POSITION: memcpy(params, l->position, 4*sizeof(float)); break;
    case GL_SPOT_DIRECTION: memcpy(params, l->spotDirection, 3*sizeof(float)); break;
    case GL_SPOT_EXPONENT:  params[0] = l->spotExponent; break;
    case GL_SPOT_CUTOFF:    params[0] = l->spotCutoff; break;
    case GL_CONSTANT_ATTENUATION:  params[0] = l->constantAttenuation; break;
    case GL_LINEAR_ATTENUATION:    params[0] = l->linearAttenuation; break;
    case GL_QUADRATIC_ATTENUATION: params[0] = l->quadraticAttenuation; break;
    default: params[0] = 0.0f; break;
    }
}

void _glsGetLightiv(unsigned int light, unsigned int pname, int *params)
{
    if (!params) return;
    float fv[4] = {0};
    _glsGetLightfv(light, pname, fv);
    params[0] = (int)fv[0];
    if (pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR || pname == GL_POSITION) {
        params[1] = (int)fv[1]; params[2] = (int)fv[2]; params[3] = (int)fv[3];
    }
}

/* ===================================================================
 *  SECTION: Material state
 * =================================================================== */

#ifndef GL_EMISSION
#define GL_EMISSION             0x1600
#endif
#ifndef GL_SHININESS
#define GL_SHININESS            0x1601
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#define GL_AMBIENT_AND_DIFFUSE  0x1602
#endif
#ifndef GL_FRONT
#define GL_FRONT                0x0404
#endif
#ifndef GL_BACK
#define GL_BACK                 0x0405
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK       0x0408
#endif

static void _setMaterialParam(GLS_Material *mat, unsigned int pname, float param)
{
    if (!mat) return;
    if (pname == GL_SHININESS) mat->shininess = param;
}

static void _setMaterialParamv(GLS_Material *mat, unsigned int pname, const float *params)
{
    if (!mat || !params) return;
    switch (pname) {
    case GL_AMBIENT:
        memcpy(mat->ambient, params, 4*sizeof(float)); break;
    case GL_DIFFUSE:
        memcpy(mat->diffuse, params, 4*sizeof(float)); break;
    case GL_SPECULAR:
        memcpy(mat->specular, params, 4*sizeof(float)); break;
    case GL_EMISSION:
        memcpy(mat->emission, params, 4*sizeof(float)); break;
    case GL_SHININESS:
        mat->shininess = params[0]; break;
    case GL_AMBIENT_AND_DIFFUSE:
        memcpy(mat->ambient, params, 4*sizeof(float));
        memcpy(mat->diffuse, params, 4*sizeof(float)); break;
    default: break;
    }
}

void _glsMaterialf(unsigned int face, unsigned int pname, float param)
{
    GLS_State *s = glsGetState(); if (!s) return;
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
        _setMaterialParam(&s->materialFront, pname, param);
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
        _setMaterialParam(&s->materialBack, pname, param);
}

void _glsMaterialfv(unsigned int face, unsigned int pname, const float *params)
{
    GLS_State *s = glsGetState(); if (!s || !params) return;
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
        _setMaterialParamv(&s->materialFront, pname, params);
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
        _setMaterialParamv(&s->materialBack, pname, params);
}

static void _getMaterialParamv(const GLS_Material *mat, unsigned int pname, float *params)
{
    if (!mat || !params) return;
    switch (pname) {
    case GL_AMBIENT:  memcpy(params, mat->ambient, 4*sizeof(float)); break;
    case GL_DIFFUSE:  memcpy(params, mat->diffuse, 4*sizeof(float)); break;
    case GL_SPECULAR: memcpy(params, mat->specular, 4*sizeof(float)); break;
    case GL_EMISSION: memcpy(params, mat->emission, 4*sizeof(float)); break;
    case GL_SHININESS: params[0] = mat->shininess; break;
    default: params[0] = 0.0f; break;
    }
}

void _glsGetMaterialfv(unsigned int face, unsigned int pname, float *params)
{
    GLS_State *s = glsGetState(); if (!s || !params) return;
    if (face == GL_FRONT)
        _getMaterialParamv(&s->materialFront, pname, params);
    else
        _getMaterialParamv(&s->materialBack, pname, params);
}

void _glsGetMaterialiv(unsigned int face, unsigned int pname, int *params)
{
    if (!params) return;
    float fv[4] = {0};
    _glsGetMaterialfv(face, pname, fv);
    params[0] = (int)fv[0];
    if (pname != GL_SHININESS) {
        params[1] = (int)fv[1]; params[2] = (int)fv[2]; params[3] = (int)fv[3];
    }
}

/* ===================================================================
 *  SECTION: Display list tracking
 * =================================================================== */

/* Simple display list ID tracking — no command recording */
#define GLS_MAX_LISTS 4096

static struct {
    unsigned int allocated[GLS_MAX_LISTS]; /* 0 = free, nonzero = list ID */
    unsigned int listBase;
    unsigned int buildingList;             /* list ID currently being built, 0 = none */
    unsigned int nextListId;
} _glsListState = {0};

void _glsNewList(unsigned int list, unsigned int mode)
{
    (void)mode;
    _glsListState.buildingList = list;
    /* Mark as allocated */
    if (list > 0 && list <= GLS_MAX_LISTS)
        _glsListState.allocated[list - 1] = list;
}

void _glsEndList(void)
{
    _glsListState.buildingList = 0;
}

void _glsCallList(unsigned int list)
{
    (void)list; /* No-op — we track but don't record/replay */
}

void _glsCallLists(int n, unsigned int type, const void *lists)
{
    (void)n; (void)type; (void)lists;
}

unsigned int _glsGenLists(int range)
{
    if (range <= 0) return 0;
    unsigned int base = _glsListState.nextListId + 1;
    int i;
    for (i = 0; i < range && (base + (unsigned)i - 1) < GLS_MAX_LISTS; i++) {
        _glsListState.allocated[base + i - 1] = base + (unsigned)i;
    }
    _glsListState.nextListId = base + (unsigned)range - 1;
    return base;
}

void _glsDeleteLists(unsigned int list, int range)
{
    int i;
    for (i = 0; i < range; i++) {
        unsigned int id = list + (unsigned)i;
        if (id > 0 && id <= GLS_MAX_LISTS)
            _glsListState.allocated[id - 1] = 0;
    }
}

void _glsListBase(unsigned int base)
{
    _glsListState.listBase = base;
}

unsigned char _glsIsList(unsigned int list)
{
    if (list > 0 && list <= GLS_MAX_LISTS)
        return _glsListState.allocated[list - 1] != 0 ? 1 : 0;
    return 0;
}

/* ===================================================================
 *  SECTION: Clip plane state
 * =================================================================== */

static double _glsClipPlanes[GLS_MAX_CLIP_PLANES][4] = {{0}};

void _glsClipPlane(unsigned int plane, const double *equation)
{
    unsigned int idx = plane - 0x3000; /* GL_CLIP_PLANE0 = 0x3000 */
    if (idx < GLS_MAX_CLIP_PLANES && equation) {
        _glsClipPlanes[idx][0] = equation[0];
        _glsClipPlanes[idx][1] = equation[1];
        _glsClipPlanes[idx][2] = equation[2];
        _glsClipPlanes[idx][3] = equation[3];
    }
}

void _glsGetClipPlane(unsigned int plane, double *equation)
{
    unsigned int idx = plane - 0x3000;
    if (idx < GLS_MAX_CLIP_PLANES && equation) {
        equation[0] = _glsClipPlanes[idx][0];
        equation[1] = _glsClipPlanes[idx][1];
        equation[2] = _glsClipPlanes[idx][2];
        equation[3] = _glsClipPlanes[idx][3];
    }
}

/* ===================================================================
 *  SECTION: Misc legacy state
 * =================================================================== */

static GLenum_t _glsColorMaterialFace = 0x0408; /* GL_FRONT_AND_BACK */
static GLenum_t _glsColorMaterialMode = 0x1602; /* GL_AMBIENT_AND_DIFFUSE */
static GLenum_t _glsShadeModelMode = 0x1D01;    /* GL_SMOOTH */
static GLenum_t _glsLogicOpMode = 0x1503;       /* GL_COPY */
static GLenum_t _glsReadBufferMode = 0x0405;    /* GL_BACK */
static GLenum_t _glsDrawBufferMode = 0x0405;    /* GL_BACK */

void _glsColorMaterial(unsigned int face, unsigned int mode)
{
    _glsColorMaterialFace = face;
    _glsColorMaterialMode = mode;
}

void _glsShadeModel(unsigned int mode)
{
    _glsShadeModelMode = mode;
}

void _glsHint(unsigned int target, unsigned int mode)
{
    (void)target; (void)mode; /* Accept and ignore */
}

void _glsLogicOp(unsigned int opcode)
{
    _glsLogicOpMode = opcode;
}

void _glsReadBuffer(unsigned int mode)
{
    _glsReadBufferMode = mode;
}

void _glsDrawBuffer(unsigned int mode)
{
    _glsDrawBufferMode = mode;
}

/* ===================================================================
 *  SECTION: Attribute stack (accept params, no-op)
 * =================================================================== */

void _glsPushAttrib(unsigned int mask)
{
    (void)mask; /* Accept — no actual stack implementation needed yet */
}

void _glsPopAttrib(void)
{
    /* No-op */
}

void _glsPushClientAttrib(unsigned int mask)
{
    (void)mask;
}

void _glsPopClientAttrib(void)
{
    /* No-op */
}


/* ===================================================================
 *  SECTION 33: Draw Calls
 * =================================================================== */

void _glsDrawArrays(unsigned int mode, int first, int count)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    int primCount;
    GLS_VAO *vao;
    GLS_D3DVertex *verts;
    int i;

    if (!pDev || count <= 0) return;

    switch (mode) {
    case 0x0000: d3dPrimType = D3DPT_POINTLIST; primCount = count; break;
    case 0x0001: d3dPrimType = D3DPT_LINELIST; primCount = count / 2; break;
    case 0x0003: d3dPrimType = D3DPT_LINESTRIP; primCount = count - 1; break;
    case 0x0004: d3dPrimType = D3DPT_TRIANGLELIST; primCount = count / 3; break;
    case 0x0005: d3dPrimType = D3DPT_TRIANGLESTRIP; primCount = count - 2; break;
    case 0x0006: d3dPrimType = D3DPT_TRIANGLEFAN; primCount = count - 2; break;
    default: return;
    }

    if (primCount <= 0) return;

    /* Set transforms */
    {
        D3DMATRIX d3dWorld, d3dProj;
        _glsTransposeMatrix(&d3dWorld, s->modelviewStack.stack[s->modelviewStack.top].m);
        _glsTransposeMatrix(&d3dProj, s->projectionStack.stack[s->projectionStack.top].m);
        __try {
            IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &d3dWorld);
            IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &d3dProj);
            if (!s->enableLighting)
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
        } __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    }

    /* Build temporary vertex buffer from VAO attrib pointers */
    vao = glsFindVAO(s->boundVAO);
    verts = (GLS_D3DVertex *)malloc(count * sizeof(GLS_D3DVertex));
    if (!verts) return;

    for (i = 0; i < count; i++) {
        int vertIdx = first + i;
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float col[4];
        float tc[2] = {0.0f, 0.0f};

        col[0] = s->currentColor[0];
        col[1] = s->currentColor[1];
        col[2] = s->currentColor[2];
        col[3] = s->currentColor[3];

        /* Read position from attrib 0 */
        if (vao && vao->attribs[0].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[0];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + vertIdx * stride);
                if (a->size >= 1) pos[0] = p[0];
                if (a->size >= 2) pos[1] = p[1];
                if (a->size >= 3) pos[2] = p[2];
            }
        }

        /* Read color from attrib 3 */
        if (vao && vao->attribs[3].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[3];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                if (a->type == GL_FLOAT) {
                    const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + vertIdx * stride);
                    if (a->size >= 1) col[0] = p[0];
                    if (a->size >= 2) col[1] = p[1];
                    if (a->size >= 3) col[2] = p[2];
                    if (a->size >= 4) col[3] = p[3];
                } else if (a->type == GL_UNSIGNED_BYTE && a->normalized) {
                    const unsigned char *p = (const unsigned char *)((const char *)buf->data + (ptrdiff_t)a->pointer + vertIdx * (a->stride ? a->stride : a->size));
                    if (a->size >= 1) col[0] = p[0] / 255.0f;
                    if (a->size >= 2) col[1] = p[1] / 255.0f;
                    if (a->size >= 3) col[2] = p[2] / 255.0f;
                    if (a->size >= 4) col[3] = p[3] / 255.0f;
                }
            }
        }

        /* Read texcoord from attrib 8 */
        if (vao && vao->attribs[8].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[8];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + vertIdx * stride);
                if (a->size >= 1) tc[0] = p[0];
                if (a->size >= 2) tc[1] = p[1];
            }
        }

        verts[i].x = pos[0];
        verts[i].y = pos[1];
        verts[i].z = pos[2];
        verts[i].color = D3DCOLOR_COLORVALUE(col[0], col[1], col[2], col[3]);
        verts[i].u = tc[0];
        verts[i].v = tc[1];
    }

    __try {
        IDirect3DDevice9_SetFVF(pDev, GLS_D3DFVF);
        IDirect3DDevice9_DrawPrimitiveUP(pDev, d3dPrimType, primCount, verts, sizeof(GLS_D3DVertex));
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    free(verts);
}

void _glsDrawElements(unsigned int mode, int count, unsigned int type, const void *indices)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    int primCount;
    GLS_VAO *vao;
    GLS_Buffer *ibo;
    const void *indexData;
    int i;
    int maxVertIdx = 0;
    GLS_D3DVertex *verts;
    unsigned short *idx16 = NULL;

    if (!pDev || count <= 0) return;

    switch (mode) {
    case 0x0000: d3dPrimType = D3DPT_POINTLIST; primCount = count; break;
    case 0x0001: d3dPrimType = D3DPT_LINELIST; primCount = count / 2; break;
    case 0x0003: d3dPrimType = D3DPT_LINESTRIP; primCount = count - 1; break;
    case 0x0004: d3dPrimType = D3DPT_TRIANGLELIST; primCount = count / 3; break;
    case 0x0005: d3dPrimType = D3DPT_TRIANGLESTRIP; primCount = count - 2; break;
    case 0x0006: d3dPrimType = D3DPT_TRIANGLEFAN; primCount = count - 2; break;
    default: return;
    }

    if (primCount <= 0) return;

    /* Set transforms */
    {
        D3DMATRIX d3dWorld, d3dProj;
        _glsTransposeMatrix(&d3dWorld, s->modelviewStack.stack[s->modelviewStack.top].m);
        _glsTransposeMatrix(&d3dProj, s->projectionStack.stack[s->projectionStack.top].m);
        __try {
            IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &d3dWorld);
            IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &d3dProj);
            if (!s->enableLighting)
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
        } __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    }

    /* Resolve index data */
    ibo = glsFindBuffer(s->boundElementBuffer);
    indexData = indices;
    if (ibo && ibo->data) {
        indexData = (const char *)ibo->data + (ptrdiff_t)indices;
    }
    if (!indexData) return;

    /* Find max vertex index to know how many vertices to build */
    for (i = 0; i < count; i++) {
        int idx;
        switch (type) {
        case 0x1401: /* GL_UNSIGNED_BYTE */  idx = ((const unsigned char *)indexData)[i]; break;
        case 0x1403: /* GL_UNSIGNED_SHORT */ idx = ((const unsigned short *)indexData)[i]; break;
        case 0x1405: /* GL_UNSIGNED_INT */   idx = (int)((const unsigned int *)indexData)[i]; break;
        default: return;
        }
        if (idx > maxVertIdx) maxVertIdx = idx;
    }
    maxVertIdx++;

    /* Build vertex buffer */
    vao = glsFindVAO(s->boundVAO);
    verts = (GLS_D3DVertex *)malloc(maxVertIdx * sizeof(GLS_D3DVertex));
    if (!verts) return;

    for (i = 0; i < maxVertIdx; i++) {
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float col[4];
        float tc[2] = {0.0f, 0.0f};

        col[0] = s->currentColor[0];
        col[1] = s->currentColor[1];
        col[2] = s->currentColor[2];
        col[3] = s->currentColor[3];

        if (vao && vao->attribs[0].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[0];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + i * stride);
                if (a->size >= 1) pos[0] = p[0];
                if (a->size >= 2) pos[1] = p[1];
                if (a->size >= 3) pos[2] = p[2];
            }
        }

        if (vao && vao->attribs[3].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[3];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                if (a->type == GL_FLOAT) {
                    const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + i * stride);
                    if (a->size >= 1) col[0] = p[0];
                    if (a->size >= 2) col[1] = p[1];
                    if (a->size >= 3) col[2] = p[2];
                    if (a->size >= 4) col[3] = p[3];
                } else if (a->type == GL_UNSIGNED_BYTE && a->normalized) {
                    const unsigned char *p = (const unsigned char *)((const char *)buf->data + (ptrdiff_t)a->pointer + i * (a->stride ? a->stride : a->size));
                    if (a->size >= 1) col[0] = p[0] / 255.0f;
                    if (a->size >= 2) col[1] = p[1] / 255.0f;
                    if (a->size >= 3) col[2] = p[2] / 255.0f;
                    if (a->size >= 4) col[3] = p[3] / 255.0f;
                }
            }
        }

        if (vao && vao->attribs[8].enabled) {
            GLS_VertexAttrib *a = &vao->attribs[8];
            GLS_Buffer *buf = glsFindBuffer(a->bufferBinding);
            if (buf && buf->data) {
                int stride = a->stride ? a->stride : (a->size * (int)sizeof(float));
                const float *p = (const float *)((const char *)buf->data + (ptrdiff_t)a->pointer + i * stride);
                if (a->size >= 1) tc[0] = p[0];
                if (a->size >= 2) tc[1] = p[1];
            }
        }

        verts[i].x = pos[0];
        verts[i].y = pos[1];
        verts[i].z = pos[2];
        verts[i].color = D3DCOLOR_COLORVALUE(col[0], col[1], col[2], col[3]);
        verts[i].u = tc[0];
        verts[i].v = tc[1];
    }

    /* Convert indices to 16-bit if needed (D3D9 DrawIndexedPrimitiveUP needs INDEX16 or INDEX32) */
    {
        D3DFORMAT indexFormat;
        const void *d3dIndices = indexData;

        if (type == 0x1401) { /* GL_UNSIGNED_BYTE -> convert to 16-bit */
            idx16 = (unsigned short *)malloc(count * sizeof(unsigned short));
            if (!idx16) { free(verts); return; }
            for (i = 0; i < count; i++)
                idx16[i] = ((const unsigned char *)indexData)[i];
            d3dIndices = idx16;
            indexFormat = D3DFMT_INDEX16;
        } else if (type == 0x1403) { /* GL_UNSIGNED_SHORT */
            indexFormat = D3DFMT_INDEX16;
        } else { /* GL_UNSIGNED_INT */
            indexFormat = D3DFMT_INDEX32;
        }

        __try {
            IDirect3DDevice9_SetFVF(pDev, GLS_D3DFVF);
            IDirect3DDevice9_DrawIndexedPrimitiveUP(pDev, d3dPrimType,
                0, maxVertIdx, primCount,
                d3dIndices, indexFormat,
                verts, sizeof(GLS_D3DVertex));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }

        if (idx16) free(idx16);
    }

    free(verts);
}
