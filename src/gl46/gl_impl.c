/*********************************************************************************
*
*  gl_impl.c - GL function implementations using the state machine
*
*  These _gls* functions own the direct OpenGL state machine and translate
*  resources, state, shaders, and draws to D3D9. They:
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
#include "advanced_emulation.h"
#include "compute_emulator.h"
#include "display_list_emulator.h"
#include "glsl_to_hlsl.h"
#include "arb_asm_translator.h"
#include "context_manager.h"
#include "gld_diag.h"
#include "gld_context.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static BOOL _glsRunInstancedStageDraw(unsigned int mode, int first, int count,
                                      unsigned int indexType, const void *indices,
                                      int baseVertex, int instanceCount,
                                      unsigned int baseInstance);
static void _applyTextureObjectSamplingToD3D(unsigned int unit,
                                             const GLS_Texture *tex);
static BOOL _glsBuildClippedViewport(IDirect3DDevice9 *pDev, GLS_State *s,
                                     D3DVIEWPORT9 *vp, float adjust[4]);

/* Display lists retain evaluated command arguments.  The callbacks below
 * replay canonical GLDirect operations, so list playback follows the same
 * D3D9 translation path as an immediate call. */
typedef struct { unsigned int u[2]; } GLS_DLUInt2;
typedef struct { float f[16]; } GLS_DLFloat16;
typedef struct {
    unsigned int u[4];
    int i[4];
    float f[4];
} GLS_DLStateArgs;

static BOOL _glsDLRecord(GLD_dlCommandFunc func, const void *data, int size)
{
    if (!gldDLIsRecording()) return FALSE;
    if (!gldDLRecordCommand(func, data, size)) {
        glsGetState()->lastError = GL_OUT_OF_MEMORY;
        return TRUE;
    }
    return gldDLGetRecordingMode() == GL_COMPILE;
}

static void _glsDLBegin(const void *p) { _glsBegin(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLEnd(const void *p) { (void)p; _glsEnd(); }
static void _glsDLVertex4(const void *p) { const float *v = ((const GLS_DLFloat16 *)p)->f; _glsVertex4f(v[0], v[1], v[2], v[3]); }
static void _glsDLColor4(const void *p) { const float *v = ((const GLS_DLFloat16 *)p)->f; _glsColor4f(v[0], v[1], v[2], v[3]); }
static void _glsDLNormal3(const void *p) { const float *v = ((const GLS_DLFloat16 *)p)->f; _glsNormal3f(v[0], v[1], v[2]); }
static void _glsDLTexCoord4(const void *p) { const float *v = ((const GLS_DLFloat16 *)p)->f; _glsTexCoord4f(v[0], v[1], v[2], v[3]); }
static void _glsDLMatrixMode(const void *p) { _glsMatrixMode(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLLoadIdentity(const void *p) { (void)p; _glsLoadIdentity(); }
static void _glsDLLoadMatrix(const void *p) { _glsLoadMatrixf(((const GLS_DLFloat16 *)p)->f); }
static void _glsDLMultMatrix(const void *p) { _glsMultMatrixf(((const GLS_DLFloat16 *)p)->f); }
static void _glsDLPushMatrix(const void *p) { (void)p; _glsPushMatrix(); }
static void _glsDLPopMatrix(const void *p) { (void)p; _glsPopMatrix(); }
static void _glsDLCallList(const void *p) { gldCallList46(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLEnable(const void *p) { _glsEnable(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLDisable(const void *p) { _glsDisable(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLBindTexture(const void *p) { const GLS_DLUInt2 *a = (const GLS_DLUInt2 *)p; _glsBindTexture(a->u[0], a->u[1]); }
static void _glsDLTexParameteri(const void *p) { const GLS_DLStateArgs *a = (const GLS_DLStateArgs *)p; _glsTexParameteri(a->u[0], a->u[1], a->i[0]); }
static void _glsDLBlendFunc(const void *p) { const GLS_DLUInt2 *a = (const GLS_DLUInt2 *)p; _glsBlendFunc(a->u[0], a->u[1]); }
static void _glsDLDepthFunc(const void *p) { _glsDepthFunc(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLDepthMask(const void *p) { _glsDepthMask((unsigned char)((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLCullFace(const void *p) { _glsCullFace(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLFrontFace(const void *p) { _glsFrontFace(((const GLS_DLUInt2 *)p)->u[0]); }
static void _glsDLColorMask(const void *p) { const GLS_DLUInt2 *a = (const GLS_DLUInt2 *)p; _glsColorMask((unsigned char)(a->u[0] & 0xFF), (unsigned char)((a->u[0] >> 8) & 0xFF), (unsigned char)((a->u[0] >> 16) & 0xFF), (unsigned char)((a->u[0] >> 24) & 0xFF)); }
static void _glsDLPolygonMode(const void *p) { const GLS_DLUInt2 *a = (const GLS_DLUInt2 *)p; _glsPolygonMode(a->u[0], a->u[1]); }

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
#ifndef GL_TEXTURE_1D
#define GL_TEXTURE_1D           0x0DE0
#endif
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D           0x806F
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
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT         0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS         0x8CD9
#endif
#ifndef GL_FRAMEBUFFER_UNDEFINED
#define GL_FRAMEBUFFER_UNDEFINED                     0x8219
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
#ifndef GL_NONE
#define GL_NONE                 0
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
#ifndef GL_INVALID_ENUM
#define GL_INVALID_ENUM         0x0500
#endif
#ifndef GL_INVALID_VALUE
#define GL_INVALID_VALUE        0x0501
#endif
#ifndef GL_INVALID_OPERATION
#define GL_INVALID_OPERATION    0x0502
#endif
#ifndef GL_VERTEX_PROGRAM_ARB
#define GL_VERTEX_PROGRAM_ARB   0x8620
#endif
#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB 0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#endif
#ifndef GL_S
#define GL_S                    0x2000
#endif
#ifndef GL_TEXTURE_GEN_MODE
#define GL_TEXTURE_GEN_MODE     0x2500
#endif
#ifndef GL_OBJECT_PLANE
#define GL_OBJECT_PLANE         0x2501
#endif
#ifndef GL_EYE_PLANE
#define GL_EYE_PLANE            0x2502
#endif
#ifndef GL_EYE_LINEAR
#define GL_EYE_LINEAR           0x2400
#endif
#ifndef GL_OBJECT_LINEAR
#define GL_OBJECT_LINEAR        0x2401
#endif
#ifndef GL_SPHERE_MAP
#define GL_SPHERE_MAP           0x2402
#endif
#ifndef GL_NORMAL_MAP
#define GL_NORMAL_MAP           0x8511
#endif
#ifndef GL_REFLECTION_MAP
#define GL_REFLECTION_MAP       0x8512
#endif
#ifndef GL_TEXTURE_GEN_S
#define GL_TEXTURE_GEN_S        0x0C60
#endif
#ifndef GL_TEXTURE_GEN_T
#define GL_TEXTURE_GEN_T        0x0C61
#endif
#ifndef GL_TEXTURE_GEN_R
#define GL_TEXTURE_GEN_R        0x0C62
#endif
#ifndef GL_TEXTURE_GEN_Q
#define GL_TEXTURE_GEN_Q        0x0C63
#endif

/* Working buffer for the GLSL an ARB assembly program is lowered to.  Sized
 * for the largest real ARB programs (a few hundred instructions, each becoming
 * one long GLSL statement) with a wide margin. */
#define GLS_ARB_GLSL_BUFFER     (192 * 1024)

/* Defined with the rest of the ARB program code; the draw path calls it to
 * refresh the state.* bindings an ARB program reads. */
static void _glsApplyARBStateParams(void);
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
/* The SM4+-only stages.  Named here so a program that attaches one gets an
 * explained outcome in the diagnostic log rather than a silent one; D3D9
 * predates all four and has no execution path for any of them. */
#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER      0x8DD9
#endif
#ifndef GL_TESS_CONTROL_SHADER
#define GL_TESS_CONTROL_SHADER  0x8E88
#endif
#ifndef GL_TESS_EVALUATION_SHADER
#define GL_TESS_EVALUATION_SHADER 0x8E87
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER       0x91B9
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
#ifndef GL_COLOR
#define GL_COLOR                0x1800
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
#ifndef GL_BYTE
#define GL_BYTE                 0x1400
#endif
#ifndef GL_SHORT
#define GL_SHORT                0x1402
#endif
#ifndef GL_INT
#define GL_INT                  0x1404
#endif
#ifndef GL_DOUBLE
#define GL_DOUBLE               0x140A
#endif

/* Legacy client-array capability selectors (glEnableClientState) */
#ifndef GL_VERTEX_ARRAY
#define GL_VERTEX_ARRAY         0x8074
#endif
#ifndef GL_NORMAL_ARRAY
#define GL_NORMAL_ARRAY         0x8075
#endif
#ifndef GL_COLOR_ARRAY
#define GL_COLOR_ARRAY          0x8076
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#define GL_TEXTURE_COORD_ARRAY  0x8078
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
#ifndef GL_RG
#define GL_RG                   0x8227
#endif
#ifndef GL_RG8
#define GL_RG8                  0x822B
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

    /* Two-channel red/green.  D3D9 has no RG8, and A8L8 is the two-channel
     * 8-bit-per-component surface it does have; format_mapper.c already
     * reports A8L8 back to the application as GL_RG8 with an (R,R,R,G)
     * swizzle, so this is the same pairing read the other way round.  Without
     * a case here these fell to the A8R8G8B8 default, which is not wrong
     * enough to crash but quadruples the memory and, more importantly for
     * glTexStorage2D, is a format nothing else in this file agrees with. */
    case GL_RG:
    case GL_RG8:
        return D3DFMT_A8L8;

    /* Sized single- and dual-component formats.  id Tech 4 asks for these by
     * name; without them they fell through to A8R8G8B8, quadrupling the
     * memory for what should be an 8-bit surface. */
    case 0x803B: case 0x803C:                     /* GL_ALPHA4/8      */
    case 0x803D: case 0x803E:                     /* GL_ALPHA12/16    */
        return D3DFMT_A8;
    case 0x803F: case 0x8040:                     /* GL_LUMINANCE4/8  */
    case 0x8041: case 0x8042:                     /* GL_LUMINANCE12/16*/
    case 0x804B: case 0x804C:                     /* GL_INTENSITY8/12 */
        return D3DFMT_L8;
    case 0x8043: case 0x8044: case 0x8045:        /* GL_LUMINANCE*_ALPHA* */
    case 0x8046: case 0x8047:
        return D3DFMT_A8L8;

    /* Sized colour formats. */
    case 0x8050:                                  /* GL_RGB5   */
        return D3DFMT_R5G6B5;
    case 0x8056:                                  /* GL_RGBA4  */
        return D3DFMT_A4R4G4B4;
    case 0x8057:                                  /* GL_RGB5_A1*/
        return D3DFMT_A1R5G5B5;
    case 0x8052: case 0x8053: case 0x8054:        /* GL_RGB10/12/16 */
        return D3DFMT_X8R8G8B8;

    /* Compressed internal formats requested through the *uncompressed*
     * glTexImage2D entry point: the application hands over raw pixels and
     * asks the driver to compress.  There is no compressor here, so the
     * data is kept uncompressed — but the surface must still carry alpha
     * only when the requested format does. */
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:         /* 0x83F0 */
        return D3DFMT_X8R8G8B8;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:        /* 0x83F1 */
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:        /* 0x83F2 */
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:        /* 0x83F3 */
        return D3DFMT_A8R8G8B8;

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

/* Does this destination surface format carry an alpha channel?  Decides
 * which universal format a substitution has to use, so alpha data is never
 * silently dropped. */
static BOOL _glsFormatHasAlpha(D3DFORMAT fmt)
{
    switch (fmt) {
    case D3DFMT_A8R8G8B8: case D3DFMT_A8:       case D3DFMT_A8L8:
    case D3DFMT_A4R4G4B4: case D3DFMT_A1R5G5B5: return TRUE;
    default:                                    return FALSE;
    }
}

/*
 * Pick a D3DFORMAT the device has confirmed it can create.
 *
 * Why the device is asked first rather than CreateTexture being called and
 * the failure reacted to: on some d3d9.dll implementations one failed
 * CreateTexture (observed as fmt=28/D3DFMT_A8, hr=0x80004005) leaves the
 * device in a state where *every* later CreateTexture and
 * CreateCubeTexture fails as well — including the D3DFMT_A8R8G8B8 retry a
 * reactive fallback would make.  A reactive fallback cannot recover from a
 * state the first failed call already created, so the only place left to
 * intervene is before that call is ever made.
 *
 * Uncompressed formats only.  Compressed (DXT1/3/5) data must never be
 * substituted: the bytes are block-compressed and cannot be reinterpreted
 * as raw ARGB pixels without a decompressor this wrapper does not have.
 * _glsCompressedTexImage2D therefore gates on support and skips the
 * texture instead of calling this.
 *
 * The fallback chain is: requested format, then an ordered list of
 * candidates, then D3DFMT_UNKNOWN, which means "no creatable format, do not
 * call CreateTexture at all".
 *
 * The list has to be a list rather than the single alternate this used to
 * offer.  A request that already *was* A8R8G8B8 (or X8R8G8B8) and came back
 * unsupported was answered by proposing itself, so the second probe asked the
 * same question, got the same answer, and the texture was abandoned with
 * "fallback D3DFMT=22 also unsupported" naming the format that had just been
 * rejected — no alternative was ever tried.
 *
 * Ordering keeps alpha when the request had alpha: every alpha-capable
 * candidate is exhausted before an opaque one is accepted, and dropping alpha
 * is logged, because a texture silently losing its alpha channel is a
 * rendering bug that looks like an art bug.  A request without alpha prefers
 * opaque formats for the same reason in reverse — no point spending an alpha
 * channel nothing will read — but will still take an alpha-capable format
 * over failing to create the texture at all.
 *
 * Each rung costs one lookup in gldIsTextureFormatSupported46's per-format
 * cache, not a driver round trip, so a deeper chain is close to free after
 * the first texture.
 */
static D3DFORMAT _glsResolveTextureFormat(D3DFORMAT wanted, BOOL cubeMap)
{
    /* Alpha-capable rungs, widest first. */
    static const D3DFORMAT alphaChain[] = {
        D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_A8L8
    };
    /* Opaque rungs, widest first. */
    static const D3DFORMAT opaqueChain[] = {
        D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5
    };

    const D3DFORMAT *first, *second;
    int firstCount, secondCount;
    BOOL wantAlpha;
    D3DFORMAT tried[8];
    int triedCount = 0;
    int stage, i, j;
    char attempted[128];
    int off = 0;

    if (gldIsTextureFormatSupported46(wanted, cubeMap))
        return wanted;

    wantAlpha = _glsFormatHasAlpha(wanted);

    if (wantAlpha) {
        first  = alphaChain;  firstCount  = (int)(sizeof(alphaChain) / sizeof(alphaChain[0]));
        second = opaqueChain; secondCount = (int)(sizeof(opaqueChain) / sizeof(opaqueChain[0]));
    } else {
        first  = opaqueChain; firstCount  = (int)(sizeof(opaqueChain) / sizeof(opaqueChain[0]));
        second = alphaChain;  secondCount = (int)(sizeof(alphaChain) / sizeof(alphaChain[0]));
    }

    for (stage = 0; stage < 2; stage++) {
        const D3DFORMAT *chain = stage == 0 ? first : second;
        int count             = stage == 0 ? firstCount : secondCount;

        for (i = 0; i < count; i++) {
            D3DFORMAT cand = chain[i];
            BOOL seen = FALSE;

            if (cand == wanted)
                continue;
            for (j = 0; j < triedCount; j++)
                if (tried[j] == cand) { seen = TRUE; break; }
            if (seen)
                continue;

            if (triedCount < (int)(sizeof(tried) / sizeof(tried[0])))
                tried[triedCount++] = cand;

            if (!gldIsTextureFormatSupported46(cand, cubeMap))
                continue;

            /* Kept in the shape the first rung has always logged in, so
             * anything reading these logs still recognises the line. */
            gldDiagLog("GL: TexImage2D D3DFMT=%d unsupported by device - falling back to D3DFMT=%d",
                       (int)wanted, (int)cand);

            if (wantAlpha && !_glsFormatHasAlpha(cand))
                gldDiagLog("GL: TexImage2D fallback D3DFMT=%d has no alpha channel - "
                           "the requested format's alpha is dropped", (int)cand);

            return cand;
        }
    }

    /* Name every rung that was tried.  A genuine "this driver creates none of
     * these" then reads differently from a chain that never ran, which is the
     * distinction the single-alternate version could not express. */
    attempted[0] = '\0';
    for (j = 0; j < triedCount; j++) {
        char num[16];
        int v = (int)tried[j];
        int k = 0, m;

        if (off > 0 && off < (int)sizeof(attempted) - 1)
            attempted[off++] = ',';
        do {
            num[k++] = (char)('0' + (v % 10));
            v /= 10;
        } while (v > 0 && k < (int)sizeof(num));
        for (m = k - 1; m >= 0 && off < (int)sizeof(attempted) - 1; m--)
            attempted[off++] = num[m];
        attempted[off] = '\0';
    }

    gldDiagLog("GL: TexImage2D D3DFMT=%d and every fallback (%s) unsupported - "
               "no creatable format", (int)wanted, attempted);
    return D3DFMT_UNKNOWN;
}

/* Components in a GL pixel format, or 0 if unknown. */
static int _glsFormatComponents(unsigned int glFormat)
{
    switch (glFormat) {
    case GL_RGBA: case GL_BGRA: case GL_RGBA8: case 4:      return 4;
    case GL_RGB:  case GL_BGR:  case GL_RGB8:  case 3:      return 3;
    case GL_LUMINANCE_ALPHA: case GL_RG: case GL_RG8: case 2: return 2;
    case GL_LUMINANCE: case GL_ALPHA: case GL_RED:
    case GL_GREEN: case GL_BLUE: case GL_INTENSITY:
    case GL_DEPTH_COMPONENT: case 1:                        return 1;
    default:                                                return 0;
    }
}

/*
 * Bytes occupied by one source pixel, or 0 when the combination is not
 * understood.
 *
 * Returning 0 matters: the caller must then skip the copy entirely rather
 * than guess a size.  Guessing 4 bytes per pixel — as this code used to —
 * reads three bytes past the end of every pixel of a single-component
 * image, which on a 1024-wide alpha texture runs ~3 MB off the end of the
 * application's buffer and crashes.
 */
static int _glsSourceBytesPerPixel(unsigned int glFormat, unsigned int glType)
{
    int comps;

    switch (glType) {
    /* Packed types encode a whole pixel, so the component count is implied. */
    case 0x8363: /* GL_UNSIGNED_SHORT_5_6_5      */
    case 0x8364: /* GL_UNSIGNED_SHORT_5_6_5_REV  */
    case 0x8033: /* GL_UNSIGNED_SHORT_4_4_4_4    */
    case 0x8365: /* GL_UNSIGNED_SHORT_4_4_4_4_REV*/
    case 0x8034: /* GL_UNSIGNED_SHORT_5_5_5_1    */
    case 0x8366: /* GL_UNSIGNED_SHORT_1_5_5_5_REV*/
        return 2;
    case 0x8035: /* GL_UNSIGNED_INT_8_8_8_8      */
    case 0x8367: /* GL_UNSIGNED_INT_8_8_8_8_REV  */
    case 0x8036: /* GL_UNSIGNED_INT_10_10_10_2   */
    case 0x8368: /* GL_UNSIGNED_INT_2_10_10_10_REV*/
        return 4;
    default:
        break;
    }

    comps = _glsFormatComponents(glFormat);
    if (comps == 0) return 0;

    switch (glType) {
    case GL_BYTE: case GL_UNSIGNED_BYTE:   return comps * 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: return comps * 2;
    case GL_INT: case GL_UNSIGNED_INT:
    case GL_FLOAT:                         return comps * 4;
    default:                               return 0;
    }
}

/* Bytes one destination pixel occupies, or 0 if this surface format is not
 * one this converter knows how to write. */
static int _glsDestBytesPerPixel(D3DFORMAT fmt)
{
    switch (fmt) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: return 4;
    case D3DFMT_R5G6B5:   case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4: case D3DFMT_A8L8:     return 2;
    case D3DFMT_A8:       case D3DFMT_L8:       return 1;
    default:                                    return 0;
    }
}

/* Expand one GL_UNSIGNED_BYTE source pixel to RGBA8. */
static void _glsDecodePixelUB(const unsigned char *p, unsigned int glFormat,
                              int comps, unsigned char rgba[4])
{
    switch (comps) {
    case 4:
        if (glFormat == GL_BGRA) {
            rgba[0] = p[2]; rgba[1] = p[1]; rgba[2] = p[0];
        } else {
            rgba[0] = p[0]; rgba[1] = p[1]; rgba[2] = p[2];
        }
        rgba[3] = p[3];
        break;
    case 3:
        if (glFormat == GL_BGR) {
            rgba[0] = p[2]; rgba[1] = p[1]; rgba[2] = p[0];
        } else {
            rgba[0] = p[0]; rgba[1] = p[1]; rgba[2] = p[2];
        }
        rgba[3] = 0xFF;
        break;
    case 2:                                  /* luminance + alpha */
        rgba[0] = rgba[1] = rgba[2] = p[0];
        rgba[3] = p[1];
        break;
    default:
        if (glFormat == GL_ALPHA) {          /* colour undefined, alpha carried */
            rgba[0] = rgba[1] = rgba[2] = 0xFF;
            rgba[3] = p[0];
        } else if (glFormat == GL_INTENSITY) {
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = p[0];
        } else {                             /* luminance / red */
            rgba[0] = rgba[1] = rgba[2] = p[0];
            rgba[3] = 0xFF;
        }
        break;
    }
}

/* Write one RGBA8 pixel in the destination surface's own layout. */
static void _glsEncodePixel(unsigned char *d, D3DFORMAT fmt,
                            const unsigned char rgba[4])
{
    unsigned short v;

    switch (fmt) {
    case D3DFMT_A8R8G8B8:                    /* BGRA in memory */
        d[0] = rgba[2]; d[1] = rgba[1]; d[2] = rgba[0]; d[3] = rgba[3];
        break;
    case D3DFMT_X8R8G8B8:
        d[0] = rgba[2]; d[1] = rgba[1]; d[2] = rgba[0]; d[3] = 0xFF;
        break;
    case D3DFMT_A8:
        d[0] = rgba[3];
        break;
    case D3DFMT_L8:
        d[0] = rgba[0];
        break;
    case D3DFMT_A8L8:                        /* L low byte, A high byte */
        d[0] = rgba[0]; d[1] = rgba[3];
        break;
    case D3DFMT_R5G6B5:
        v = (unsigned short)(((rgba[0] >> 3) << 11) |
                             ((rgba[1] >> 2) <<  5) |
                              (rgba[2] >> 3));
        memcpy(d, &v, 2);
        break;
    case D3DFMT_A1R5G5B5:
        v = (unsigned short)(((rgba[3] >= 128 ? 1 : 0) << 15) |
                             ((rgba[0] >> 3) << 10) |
                             ((rgba[1] >> 3) <<  5) |
                              (rgba[2] >> 3));
        memcpy(d, &v, 2);
        break;
    case D3DFMT_A4R4G4B4:
        v = (unsigned short)(((rgba[3] >> 4) << 12) |
                             ((rgba[0] >> 4) <<  8) |
                             ((rgba[1] >> 4) <<  4) |
                              (rgba[2] >> 4));
        memcpy(d, &v, 2);
        break;
    default:
        break;
    }
}

/*
 * Copy application pixel data into a locked D3D9 surface.
 *
 * The destination format is a parameter because it is not implied by the
 * source: glTexImage2D(GL_ALPHA8, ..., GL_RGBA, GL_UNSIGNED_BYTE, p) hands
 * over 4-byte pixels for a surface holding 1-byte pixels.  Deciding the
 * write width from the source format alone overran every row by 4x and
 * corrupted D3D9's managed-pool heap, after which every later CreateTexture
 * returned E_FAIL.  Convert through RGBA8 and let the destination decide
 * how many bytes it takes.
 *
 * Honours GL_UNPACK_ALIGNMENT and GL_UNPACK_ROW_LENGTH: GL pads each source
 * row up to the alignment (4 by default), so a 2-pixel-wide alpha texture
 * has 4-byte rows, not 2.
 */
void _glsCopyPixelsToD3D(void *dst, const void *src, int width, int height,
                          unsigned int glFormat, unsigned int glType,
                          int dstPitch, D3DFORMAT dstFmt)
{
    GLS_State *s = glsGetState();
    int srcBpp, dstBpp, comps, rowPixels, rowBytes, srcStride, alignment;
    int y, x;

    if (!dst || !src || width <= 0 || height <= 0) return;

    srcBpp = _glsSourceBytesPerPixel(glFormat, glType);
    dstBpp = _glsDestBytesPerPixel(dstFmt);
    comps  = _glsFormatComponents(glFormat);

    if (srcBpp == 0 || dstBpp == 0) {
        /* Leave the level as allocated rather than read or write memory whose
         * layout we cannot determine. */
        gldDiagLog("GL: CopyPixels unsupported fmt=0x%X type=0x%X d3d=%d - level skipped",
                   glFormat, glType, (int)dstFmt);
        return;
    }

    /* Never write more than the surface row actually holds. */
    if (dstPitch > 0 && width * dstBpp > dstPitch)
        width = dstPitch / dstBpp;
    if (width <= 0) return;

    rowPixels = (s && s->unpackRowLength > 0) ? s->unpackRowLength : width;
    alignment = (s && s->unpackAlignment > 0) ? s->unpackAlignment : 4;
    rowBytes  = rowPixels * srcBpp;
    srcStride = ((rowBytes + alignment - 1) / alignment) * alignment;

    for (y = 0; y < height; y++) {
        const unsigned char *srcRow = (const unsigned char *)src + (ptrdiff_t)y * srcStride;
        unsigned char *dstRow = (unsigned char *)dst + (ptrdiff_t)y * dstPitch;

        /* Fast path: identical layout, no per-pixel work. */
        if (glType == GL_UNSIGNED_BYTE && dstFmt == D3DFMT_A8R8G8B8 &&
            glFormat == GL_BGRA) {
            memcpy(dstRow, srcRow, (size_t)width * 4);
            continue;
        }
        if (glType == GL_UNSIGNED_BYTE && dstBpp == 1 && comps == 1) {
            /* Single-component source into a single-component surface: the
             * one byte per pixel is the same byte either way. */
            memcpy(dstRow, srcRow, (size_t)width);
            continue;
        }

        if (glType == GL_UNSIGNED_BYTE) {
            for (x = 0; x < width; x++) {
                unsigned char rgba[4];
                _glsDecodePixelUB(srcRow + (ptrdiff_t)x * srcBpp, glFormat, comps, rgba);
                _glsEncodePixel(dstRow + (ptrdiff_t)x * dstBpp, dstFmt, rgba);
            }
        } else if (srcBpp == dstBpp) {
            /* Packed or wide types whose size already matches the surface. */
            memcpy(dstRow, srcRow, (size_t)width * dstBpp);
        } else {
            gldDiagLog("GL: CopyPixels no conversion fmt=0x%X type=0x%X d3d=%d - level skipped",
                       glFormat, glType, (int)dstFmt);
            return;
        }
    }
}

/* Expand one pixel of a D3D9 surface to RGBA8 — the inverse of
 * _glsEncodePixel, used by every read-back path. */
static void _glsDecodeSurfacePixel(const unsigned char *p, D3DFORMAT fmt,
                                   unsigned char rgba[4])
{
    unsigned short v;

    switch (fmt) {
    case D3DFMT_A8R8G8B8:                    /* BGRA in memory */
        rgba[0] = p[2]; rgba[1] = p[1]; rgba[2] = p[0]; rgba[3] = p[3];
        break;
    case D3DFMT_X8R8G8B8:
        rgba[0] = p[2]; rgba[1] = p[1]; rgba[2] = p[0]; rgba[3] = 0xFF;
        break;
    case D3DFMT_A8:
        rgba[0] = rgba[1] = rgba[2] = 0xFF; rgba[3] = p[0];
        break;
    case D3DFMT_L8:
        rgba[0] = rgba[1] = rgba[2] = p[0]; rgba[3] = 0xFF;
        break;
    case D3DFMT_A8L8:
        rgba[0] = rgba[1] = rgba[2] = p[0]; rgba[3] = p[1];
        break;
    case D3DFMT_R5G6B5:
        memcpy(&v, p, 2);
        rgba[0] = (unsigned char)(((v >> 11) & 0x1F) * 255 / 31);
        rgba[1] = (unsigned char)(((v >>  5) & 0x3F) * 255 / 63);
        rgba[2] = (unsigned char)(( v        & 0x1F) * 255 / 31);
        rgba[3] = 0xFF;
        break;
    case D3DFMT_A1R5G5B5:
        memcpy(&v, p, 2);
        rgba[0] = (unsigned char)(((v >> 10) & 0x1F) * 255 / 31);
        rgba[1] = (unsigned char)(((v >>  5) & 0x1F) * 255 / 31);
        rgba[2] = (unsigned char)(( v        & 0x1F) * 255 / 31);
        rgba[3] = (v & 0x8000) ? 0xFF : 0x00;
        break;
    case D3DFMT_A4R4G4B4:
        memcpy(&v, p, 2);
        rgba[0] = (unsigned char)(((v >> 8) & 0xF) * 17);
        rgba[1] = (unsigned char)(((v >> 4) & 0xF) * 17);
        rgba[2] = (unsigned char)(( v       & 0xF) * 17);
        rgba[3] = (unsigned char)(((v >> 12) & 0xF) * 17);
        break;
    default:
        rgba[0] = rgba[1] = rgba[2] = 0; rgba[3] = 0xFF;
        break;
    }
}

/* Write one RGBA8 pixel in a GL client format — the inverse of
 * _glsDecodePixelUB. */
static void _glsEncodePixelUB(unsigned char *d, unsigned int glFormat, int comps,
                              const unsigned char rgba[4])
{
    switch (comps) {
    case 4:
        if (glFormat == GL_BGRA) {
            d[0] = rgba[2]; d[1] = rgba[1]; d[2] = rgba[0];
        } else {
            d[0] = rgba[0]; d[1] = rgba[1]; d[2] = rgba[2];
        }
        d[3] = rgba[3];
        break;
    case 3:
        if (glFormat == GL_BGR) {
            d[0] = rgba[2]; d[1] = rgba[1]; d[2] = rgba[0];
        } else {
            d[0] = rgba[0]; d[1] = rgba[1]; d[2] = rgba[2];
        }
        break;
    case 2:                                  /* luminance + alpha */
        d[0] = rgba[0]; d[1] = rgba[3];
        break;
    default:
        if (glFormat == GL_ALPHA)      d[0] = rgba[3];
        else if (glFormat == GL_INTENSITY) d[0] = rgba[3];
        else                           d[0] = rgba[0];
        break;
    }
}

/*
 * Copy a locked D3D9 surface back out into an application pixel buffer.
 *
 * The mirror of _glsCopyPixelsToD3D, and a parameter-for-parameter one: the
 * source surface format is explicit rather than inferred, for the same reason
 * given there.  Two things differ from the write direction:
 *
 *  - GL_PACK_ALIGNMENT / GL_PACK_ROW_LENGTH apply instead of the unpack pair.
 *  - `flipRows` exists at all.  GL images run bottom-up and D3D9 surfaces
 *    top-down; the write direction never had to care because it only ever
 *    filled a surface from a buffer laid out the same way it would read it
 *    back, but glReadPixels hands the result to an application that expects
 *    GL's own row order.
 */
void _glsCopyPixelsFromD3D(void *dst, const void *src, int width, int height,
                           unsigned int glFormat, unsigned int glType,
                           int srcPitch, D3DFORMAT srcFmt, int flipRows)
{
    GLS_State *s = glsGetState();
    int dstBpp, srcBpp, comps, rowPixels, rowBytes, dstStride, alignment;
    int y, x;

    if (!dst || !src || width <= 0 || height <= 0) return;

    dstBpp = _glsSourceBytesPerPixel(glFormat, glType);
    srcBpp = _glsDestBytesPerPixel(srcFmt);
    comps  = _glsFormatComponents(glFormat);

    if (dstBpp == 0 || srcBpp == 0) {
        gldDiagLog("GL: CopyPixelsFromD3D unsupported fmt=0x%X type=0x%X d3d=%d - skipped",
                   glFormat, glType, (int)srcFmt);
        return;
    }

    rowPixels = (s && s->packRowLength > 0) ? s->packRowLength : width;
    alignment = (s && s->packAlignment > 0) ? s->packAlignment : 4;
    rowBytes  = rowPixels * dstBpp;
    dstStride = ((rowBytes + alignment - 1) / alignment) * alignment;

    for (y = 0; y < height; y++) {
        int srcY = flipRows ? (height - 1 - y) : y;
        const unsigned char *srcRow = (const unsigned char *)src + (ptrdiff_t)srcY * srcPitch;
        unsigned char *dstRow = (unsigned char *)dst + (ptrdiff_t)y * dstStride;

        if (glType == GL_UNSIGNED_BYTE && srcFmt == D3DFMT_A8R8G8B8 &&
            glFormat == GL_BGRA) {
            memcpy(dstRow, srcRow, (size_t)width * 4);
            continue;
        }

        if (glType == GL_UNSIGNED_BYTE) {
            for (x = 0; x < width; x++) {
                unsigned char rgba[4];
                _glsDecodeSurfacePixel(srcRow + (ptrdiff_t)x * srcBpp, srcFmt, rgba);
                _glsEncodePixelUB(dstRow + (ptrdiff_t)x * dstBpp, glFormat, comps, rgba);
            }
        } else if (srcBpp == dstBpp) {
            memcpy(dstRow, srcRow, (size_t)width * dstBpp);
        } else {
            gldDiagLog("GL: CopyPixelsFromD3D no conversion fmt=0x%X type=0x%X d3d=%d - skipped",
                       glFormat, glType, (int)srcFmt);
            return;
        }
    }
}

/* ===== D3D9 surface reference tracking =====
 *
 * Every IDirect3DSurface9 this file obtains is short-lived and released before
 * the function that took it returns, so the balance should be zero whenever the
 * wrapper is idle. It is not: RTX Remix reports live objects in its Resource
 * map at module eviction, and a surface that outlives its function keeps the
 * device alive too, which is why the device and its swapchain appear alongside.
 *
 * Reading all thirty-one release sites by eye found nothing twice over, so the
 * balance is counted instead. Acquire and release are both recorded with the
 * pointer, so an unmatched acquire can be identified by its address rather than
 * inferred, and the running total is reported at shutdown.
 */
static LONG g_surfLive = 0;

static void _glsSurfAcquired(void *p, const char *site)
{
    if (!p) return;
    gldDiagLogV("GL: surf +1 %p  %s  (live=%ld)", p, site, InterlockedIncrement(&g_surfLive));
}

/* Drop-in for IDirect3DSurface9_Release: same NULL tolerance, same return. */
static ULONG _glsSurfRel(IDirect3DSurface9 *p)
{
    ULONG refs;
    if (!p) return 0;
    refs = IDirect3DSurface9_Release(p);
    gldDiagLogV("GL: surf -1 %p  refs=%lu  (live=%ld)",
               (void *)p, (unsigned long)refs, InterlockedDecrement(&g_surfLive));
    return refs;
}

LONG _glsSurfaceBalance(void)
{
    return InterlockedCompareExchange(&g_surfLive, 0, 0);
}

/*
 * Take a CPU-readable copy of the current colour render target.
 *
 * GetRenderTargetData is the only GPU->CPU path D3D9 offers: StretchRect
 * cannot write into D3DPOOL_SYSTEMMEM, and a D3DPOOL_DEFAULT render target is
 * not lockable.  It also insists the destination match the source exactly, so
 * the whole target is copied and the caller locks the sub-rectangle it wants.
 *
 * The caller releases *ppSysMem.
 */
static BOOL _glsReadRenderTarget(IDirect3DSurface9 **ppSysMem, D3DSURFACE_DESC *pDesc)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DSurface9 *pRT = NULL;
    IDirect3DSurface9 *pSys = NULL;
    HRESULT hr;

    if (!pDev || !ppSysMem || !pDesc) return FALSE;
    *ppSysMem = NULL;

    __try {
        hr = IDirect3DDevice9_GetRenderTarget(pDev, 0, &pRT);
        if (SUCCEEDED(hr)) _glsSurfAcquired(pRT, "ReadRenderTarget/GetRenderTarget");
        if (FAILED(hr) || !pRT) {
            gldDiagLog("GL: ReadRenderTarget GetRenderTarget failed (hr=0x%08X)", (unsigned)hr);
            return FALSE;
        }
        hr = IDirect3DSurface9_GetDesc(pRT, pDesc);
        if (FAILED(hr)) {
            _glsSurfRel(pRT); pRT = NULL;
            gldDiagLog("GL: ReadRenderTarget GetDesc failed (hr=0x%08X)", (unsigned)hr);
            return FALSE;
        }
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDev, pDesc->Width, pDesc->Height,
                                                          pDesc->Format, D3DPOOL_SYSTEMMEM,
                                                          &pSys, NULL);
                                                          if (SUCCEEDED(hr)) _glsSurfAcquired(pSys, "ReadRenderTarget/OffscreenPlain");
        if (FAILED(hr) || !pSys) {
            _glsSurfRel(pRT); pRT = NULL;
            gldDiagLog("GL: ReadRenderTarget CreateOffscreenPlainSurface failed (hr=0x%08X)",
                       (unsigned)hr);
            return FALSE;
        }
        hr = IDirect3DDevice9_GetRenderTargetData(pDev, pRT, pSys);
        _glsSurfRel(pRT); pRT = NULL;
        if (FAILED(hr)) {
            _glsSurfRel(pSys); pSys = NULL;
            gldDiagLog("GL: ReadRenderTarget GetRenderTargetData failed (hr=0x%08X)",
                       (unsigned)hr);
            return FALSE;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        /* Whatever was acquired before the fault is still held; the normal-path
         * Release below it never ran.  These are D3DPOOL_DEFAULT/SYSTEMMEM
         * surfaces, so swallowing the fault without releasing them leaks a
         * device resource that then blocks a clean device teardown. */
        if (pRT)  { _glsSurfRel(pRT);  pRT  = NULL; }
        if (pSys) { _glsSurfRel(pSys); pSys = NULL; }
        gldDiagLogV("GL: ReadRenderTarget faulted inside D3D9");
        return FALSE;
    }

    *ppSysMem = pSys;
    return TRUE;
}

/*
 * Resolve where a pixel-pack operation should write.
 *
 * With a buffer bound to GL_PIXEL_PACK_BUFFER the `pixels` argument is a byte
 * offset into that buffer rather than a client address.
 */
static void *_glsResolvePackTarget(GLS_State *s, void *pixels)
{
    if (s->boundPixelPackBuffer) {
        GLS_Buffer *buf = glsFindBuffer(s->boundPixelPackBuffer);
        if (!buf || !buf->data) {
            gldDiagLog("GL: pixel pack buffer %u has no storage", s->boundPixelPackBuffer);
            return NULL;
        }
        return (unsigned char *)buf->data + (ptrdiff_t)pixels;
    }
    return pixels;
}

/*
 * Resolve where a pixel-unpack operation should read from, mirroring
 * _glsResolvePackTarget for GL_PIXEL_UNPACK_BUFFER.
 */
static const void *_glsResolveUnpackSource(GLS_State *s, const void *pixels)
{
    if (s->boundPixelUnpackBuffer) {
        GLS_Buffer *buf = glsFindBuffer(s->boundPixelUnpackBuffer);
        if (!buf || !buf->data) {
            gldDiagLog("GL: pixel unpack buffer %u has no storage", s->boundPixelUnpackBuffer);
            return NULL;
        }
        return (const unsigned char *)buf->data + (ptrdiff_t)pixels;
    }
    return pixels;
}

/*
 * glReadPixels — colour buffer -> client memory (or a pixel pack buffer).
 */
void _glsReadPixels(int x, int y, int width, int height,
                    unsigned int format, unsigned int type, void *pixels)
{
    GLS_State *s = glsGetState();
    IDirect3DSurface9 *pSys = NULL;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    RECT rc;
    void *dst;
    HRESULT hr;

    if (width <= 0 || height <= 0) return;

    dst = _glsResolvePackTarget(s, pixels);
    if (!dst) return;

    if (!_glsReadRenderTarget(&pSys, &desc)) {
        gldDiagLogV("GL: glReadPixels(%d,%d,%d,%d) — render target not readable, no data written",
                   x, y, width, height);
        return;
    }

    /* GL counts rows from the bottom, D3D9 from the top. */
    rc.left   = x;
    rc.right  = x + width;
    rc.bottom = (LONG)desc.Height - y;
    rc.top    = rc.bottom - height;
    if (rc.left < 0) rc.left = 0;
    if (rc.top  < 0) rc.top  = 0;
    if (rc.right  > (LONG)desc.Width)  rc.right  = (LONG)desc.Width;
    if (rc.bottom > (LONG)desc.Height) rc.bottom = (LONG)desc.Height;
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        _glsSurfRel(pSys);
        gldDiagLogV("GL: glReadPixels rectangle lies outside the render target");
        return;
    }

    __try {
        hr = IDirect3DSurface9_LockRect(pSys, &lr, &rc, D3DLOCK_READONLY);
        if (SUCCEEDED(hr)) {
            _glsCopyPixelsFromD3D(dst, lr.pBits, (int)(rc.right - rc.left),
                                  (int)(rc.bottom - rc.top), format, type,
                                  lr.Pitch, desc.Format, 1);
            IDirect3DSurface9_UnlockRect(pSys);
        } else {
            gldDiagLog("GL: glReadPixels LockRect failed (hr=0x%08X)", (unsigned)hr);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        gldDiagLogV("GL: glReadPixels faulted inside D3D9");
    }

    _glsSurfRel(pSys);
    gldDiagLogV("GL: glReadPixels(%d,%d,%d,%d fmt=0x%X type=0x%X)",
               x, y, width, height, format, type);
}

/*
 * Read a framebuffer rectangle into a freshly allocated GL_BGRA/GL_UNSIGNED_BYTE
 * staging buffer, laid out exactly as an application would have passed it to
 * glTexImage2D.  Feeding that to the existing upload path is what lets
 * glCopyTexImage reuse all of its mip-chain, cube-face and pool handling.
 *
 * Caller frees the returned block.
 */
static unsigned char *_glsGrabFramebufferRect(int x, int y, int width, int height)
{
    IDirect3DSurface9 *pSys = NULL;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    RECT rc;
    unsigned char *buf;
    GLS_State *s = glsGetState();
    int savedPackAlign, savedPackRow;
    HRESULT hr;

    if (width <= 0 || height <= 0) return NULL;
    if (!_glsReadRenderTarget(&pSys, &desc)) return NULL;

    rc.left   = x;
    rc.right  = x + width;
    rc.bottom = (LONG)desc.Height - y;
    rc.top    = rc.bottom - height;
    if (rc.left < 0) rc.left = 0;
    if (rc.top  < 0) rc.top  = 0;
    if (rc.right  > (LONG)desc.Width)  rc.right  = (LONG)desc.Width;
    if (rc.bottom > (LONG)desc.Height) rc.bottom = (LONG)desc.Height;
    if (rc.right - rc.left != width || rc.bottom - rc.top != height) {
        _glsSurfRel(pSys);
        gldDiagLogV("GL: framebuffer rect (%d,%d %dx%d) does not fit the render target",
                   x, y, width, height);
        return NULL;
    }

    buf = (unsigned char *)malloc((size_t)width * height * 4);
    if (!buf) { _glsSurfRel(pSys); return NULL; }

    /* The staging buffer is tightly packed, whatever the application's own
     * pack state happens to be. */
    savedPackAlign = s->packAlignment;
    savedPackRow   = s->packRowLength;
    s->packAlignment = 1;
    s->packRowLength = 0;

    __try {
        hr = IDirect3DSurface9_LockRect(pSys, &lr, &rc, D3DLOCK_READONLY);
        if (SUCCEEDED(hr)) {
            _glsCopyPixelsFromD3D(buf, lr.pBits, width, height, GL_BGRA,
                                  GL_UNSIGNED_BYTE, lr.Pitch, desc.Format, 1);
            IDirect3DSurface9_UnlockRect(pSys);
        } else {
            gldDiagLog("GL: framebuffer rect LockRect failed (hr=0x%08X)", (unsigned)hr);
            free(buf);
            buf = NULL;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        gldDiagLogV("GL: framebuffer rect read faulted inside D3D9");
        free(buf);
        buf = NULL;
    }

    s->packAlignment = savedPackAlign;
    s->packRowLength = savedPackRow;
    _glsSurfRel(pSys);
    return buf;
}

void _glsCopyTexImage2D(unsigned int target, int level, unsigned int internalformat,
                        int x, int y, int width, int height, int border)
{
    GLS_State *s = glsGetState();
    unsigned char *buf;
    int savedUnpackAlign, savedUnpackRow;

    buf = _glsGrabFramebufferRect(x, y, width, height);
    if (!buf) {
        gldDiagLogV("GL: glCopyTexImage2D(level=%d, %d,%d %dx%d) — framebuffer not readable, "
                   "texture level left unchanged", level, x, y, width, height);
        return;
    }

    savedUnpackAlign = s->unpackAlignment;
    savedUnpackRow   = s->unpackRowLength;
    s->unpackAlignment = 1;
    s->unpackRowLength = 0;
    _glsTexImage2D(target, level, (int)internalformat, width, height, border,
                   GL_BGRA, GL_UNSIGNED_BYTE, buf);
    s->unpackAlignment = savedUnpackAlign;
    s->unpackRowLength = savedUnpackRow;

    free(buf);
    gldDiagLogV("GL: glCopyTexImage2D(target=0x%X level=%d %dx%d)", target, level, width, height);
}

void _glsCopyTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset,
                           int x, int y, int width, int height)
{
    GLS_State *s = glsGetState();
    unsigned char *buf;
    int savedUnpackAlign, savedUnpackRow;

    buf = _glsGrabFramebufferRect(x, y, width, height);
    if (!buf) {
        gldDiagLogV("GL: glCopyTexSubImage2D(level=%d, %d,%d %dx%d) — framebuffer not readable, "
                   "texture level left unchanged", level, x, y, width, height);
        return;
    }

    savedUnpackAlign = s->unpackAlignment;
    savedUnpackRow   = s->unpackRowLength;
    s->unpackAlignment = 1;
    s->unpackRowLength = 0;
    _glsTexSubImage2D(target, level, xoffset, yoffset, width, height,
                      GL_BGRA, GL_UNSIGNED_BYTE, buf);
    s->unpackAlignment = savedUnpackAlign;
    s->unpackRowLength = savedUnpackRow;

    free(buf);
    gldDiagLogV("GL: glCopyTexSubImage2D(target=0x%X level=%d %dx%d)", target, level, width, height);
}

/*
 * glCopyPixels — framebuffer rectangle to framebuffer rectangle.
 *
 * D3D9 refuses a StretchRect whose source and destination are the same
 * surface, so the rectangle goes via a D3DPOOL_DEFAULT scratch surface.
 */
void _glsCopyPixels(int x, int y, int width, int height, unsigned int type)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DSurface9 *pRT = NULL;
    IDirect3DSurface9 *pTmp = NULL;
    D3DSURFACE_DESC desc;
    RECT srcRect, tmpRect, dstRect;
    HRESULT hr;

    if (!pDev || width <= 0 || height <= 0) return;
    if (!s->rasterPosValid) {
        gldDiagLog("GL: glCopyPixels with an invalid raster position - discarded");
        return;
    }

    if (type != GL_COLOR) {
        gldDiagLog("GL: glCopyPixels type 0x%X (depth/stencil) has no D3D9 "
                   "surface-to-surface path here, skipped", type);
        return;
    }

    gldDiagLogV("GL: glCopyPixels(%d,%d %dx%d) -> (%.1f,%.1f)",
                x, y, width, height, s->rasterPos[0], s->rasterPos[1]);

    __try {
        hr = IDirect3DDevice9_GetRenderTarget(pDev, 0, &pRT);
        if (SUCCEEDED(hr)) _glsSurfAcquired(pRT, "CopyPixels/GetRenderTarget");
        if (FAILED(hr) || !pRT) return;
        if (FAILED(IDirect3DSurface9_GetDesc(pRT, &desc))) {
            _glsSurfRel(pRT); pRT = NULL;
            return;
        }
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDev, width, height, desc.Format,
                                                          D3DPOOL_DEFAULT, &pTmp, NULL);
                                                          if (SUCCEEDED(hr)) _glsSurfAcquired(pTmp, "CopyPixels/OffscreenPlain");
        if (FAILED(hr) || !pTmp) {
            _glsSurfRel(pRT); pRT = NULL;
            gldDiagLog("GL: glCopyPixels scratch surface creation failed (hr=0x%08X)",
                       (unsigned)hr);
            return;
        }

        srcRect.left = x; srcRect.right = x + width;
        srcRect.bottom = (LONG)desc.Height - y;
        srcRect.top    = srcRect.bottom - height;
        tmpRect.left = 0; tmpRect.top = 0;
        tmpRect.right = width; tmpRect.bottom = height;
        {
            float zx = s->pixelZoomX;
            float zy = s->pixelZoomY;
            LONG dw, dh, rx, ry, botGL, topGL;

            if (zx == 0.0f || zy == 0.0f) {
                _glsSurfRel(pTmp); pTmp = NULL;
                _glsSurfRel(pRT);  pRT  = NULL;
                return;
            }

            dw = (LONG)((float)width  * (zx < 0.0f ? -zx : zx));
            dh = (LONG)((float)height * (zy < 0.0f ? -zy : zy));
            rx = (LONG)s->rasterPos[0];
            ry = (LONG)s->rasterPos[1];
            if (dw < 1) dw = 1;
            if (dh < 1) dh = 1;

            botGL = (zy < 0.0f) ? (ry - dh) : ry;
            topGL = botGL + dh;
            dstRect.left   = (zx < 0.0f) ? (rx - dw) : rx;
            dstRect.right  = dstRect.left + dw;
            dstRect.bottom = (LONG)desc.Height - botGL;
            dstRect.top    = (LONG)desc.Height - topGL;

            if (dstRect.left < 0) dstRect.left = 0;
            if (dstRect.top < 0) dstRect.top = 0;
            if (dstRect.right > (LONG)desc.Width) dstRect.right = (LONG)desc.Width;
            if (dstRect.bottom > (LONG)desc.Height) dstRect.bottom = (LONG)desc.Height;
            if (dstRect.right <= dstRect.left || dstRect.bottom <= dstRect.top) {
                _glsSurfRel(pTmp); pTmp = NULL;
                _glsSurfRel(pRT);  pRT  = NULL;
                return;
            }
        }

        hr = IDirect3DDevice9_StretchRect(pDev, pRT, &srcRect, pTmp, &tmpRect, D3DTEXF_POINT);
        if (SUCCEEDED(hr))
            hr = IDirect3DDevice9_StretchRect(pDev, pTmp, &tmpRect, pRT, &dstRect, D3DTEXF_POINT);
        if (FAILED(hr))
            gldDiagLog("GL: glCopyPixels StretchRect failed (hr=0x%08X)", (unsigned)hr);

        _glsSurfRel(pTmp); pTmp = NULL;
        _glsSurfRel(pRT);  pRT  = NULL;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        /* The scratch surface is D3DPOOL_DEFAULT; swallowing the fault without
         * releasing it leaks a device resource and blocks a clean teardown. */
        if (pTmp) { _glsSurfRel(pTmp); pTmp = NULL; }
        if (pRT)  { _glsSurfRel(pRT);  pRT  = NULL; }
        gldDiagLogV("GL: glCopyPixels faulted inside D3D9");
    }
}

/*
 * glDrawPixels — client image straight into the colour buffer.
 *
 * A D3DPOOL_DEFAULT offscreen plain surface is lockable and is a legal
 * StretchRect source, which is what makes a CPU-side image reachable from a
 * render target that cannot itself be locked.
 */
void _glsDrawPixels(int width, int height, unsigned int format, unsigned int type,
                    const void *pixels)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DSurface9 *pRT = NULL;
    IDirect3DSurface9 *pTmp = NULL;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    RECT srcRect, dstRect;
    const void *src;
    HRESULT hr;

    if (!pDev || width <= 0 || height <= 0) return;

    src = _glsResolveUnpackSource(s, pixels);
    if (!src) return;

    if (!s->rasterPosValid) {
        /* GL discards the whole primitive when the raster position is invalid. */
        gldDiagLog("GL: glDrawPixels with an invalid raster position - discarded");
        return;
    }
    gldDiagLogV("GL: glDrawPixels(%dx%d fmt=0x%X type=0x%X) -> (%.1f,%.1f)",
               width, height, format, type, s->rasterPos[0], s->rasterPos[1]);

    __try {
        hr = IDirect3DDevice9_GetRenderTarget(pDev, 0, &pRT);
        if (SUCCEEDED(hr)) _glsSurfAcquired(pRT, "DrawPixels/GetRenderTarget");
        if (FAILED(hr) || !pRT) return;
        if (FAILED(IDirect3DSurface9_GetDesc(pRT, &desc))) {
            _glsSurfRel(pRT); pRT = NULL;
            return;
        }
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDev, width, height, desc.Format,
                                                          D3DPOOL_DEFAULT, &pTmp, NULL);
                                                          if (SUCCEEDED(hr)) _glsSurfAcquired(pTmp, "DrawPixels/OffscreenPlain");
        if (FAILED(hr) || !pTmp) {
            _glsSurfRel(pRT); pRT = NULL;
            gldDiagLog("GL: glDrawPixels scratch surface creation failed (hr=0x%08X)",
                       (unsigned)hr);
            return;
        }

        hr = IDirect3DSurface9_LockRect(pTmp, &lr, NULL, 0);
        if (SUCCEEDED(hr)) {
            /* The image arrives bottom-up; the surface is top-down. */
            int row;
            for (row = 0; row < height; row++) {
                const unsigned char *srcRow;
                int srcBpp = _glsSourceBytesPerPixel(format, type);
                int rowPixels = (s->unpackRowLength > 0) ? s->unpackRowLength : width;
                int align = (s->unpackAlignment > 0) ? s->unpackAlignment : 4;
                int stride = ((rowPixels * srcBpp + align - 1) / align) * align;
                if (srcBpp == 0) break;
                srcRow = (const unsigned char *)src + (ptrdiff_t)(height - 1 - row) * stride;
                _glsCopyPixelsToD3D((unsigned char *)lr.pBits + (ptrdiff_t)row * lr.Pitch,
                                    srcRow, width, 1, format, type, lr.Pitch, desc.Format);
            }
            IDirect3DSurface9_UnlockRect(pTmp);

            srcRect.left = 0; srcRect.top = 0;
            srcRect.right = width; srcRect.bottom = height;
            {
                /* glPixelZoom scales the image about the raster position; a
                 * negative factor mirrors it, which StretchRect expresses by
                 * ordering the destination edges. */
                float zx = s->pixelZoomX;
                float zy = s->pixelZoomY;
                LONG dw = (LONG)((float)width  * (zx < 0.0f ? -zx : zx));
                LONG dh = (LONG)((float)height * (zy < 0.0f ? -zy : zy));
                LONG rx = (LONG)s->rasterPos[0];
                LONG ry = (LONG)s->rasterPos[1];
                LONG topGL, botGL;

                if (zx == 0.0f || zy == 0.0f) {
                    _glsSurfRel(pTmp); pTmp = NULL;
                    _glsSurfRel(pRT);  pRT  = NULL;
                    return;
                }
                if (dw < 1) dw = 1;
                if (dh < 1) dh = 1;

                /* GL window coordinates count up from the bottom. */
                botGL = (zy < 0.0f) ? (ry - dh) : ry;
                topGL = botGL + dh;

                dstRect.left   = (zx < 0.0f) ? (rx - dw) : rx;
                dstRect.right  = dstRect.left + dw;
                dstRect.bottom = (LONG)desc.Height - botGL;
                dstRect.top    = (LONG)desc.Height - topGL;

                if (dstRect.left < 0) dstRect.left = 0;
                if (dstRect.top  < 0) dstRect.top  = 0;
                if (dstRect.right  > (LONG)desc.Width)  dstRect.right  = (LONG)desc.Width;
                if (dstRect.bottom > (LONG)desc.Height) dstRect.bottom = (LONG)desc.Height;
                if (dstRect.right <= dstRect.left || dstRect.bottom <= dstRect.top) {
                    gldDiagLog("GL: glDrawPixels destination is fully clipped - nothing drawn");
                    _glsSurfRel(pTmp); pTmp = NULL;
                    _glsSurfRel(pRT);  pRT  = NULL;
                    return;
                }
            }
            hr = IDirect3DDevice9_StretchRect(pDev, pTmp, &srcRect, pRT, &dstRect, D3DTEXF_POINT);
            if (FAILED(hr))
                gldDiagLog("GL: glDrawPixels StretchRect failed (hr=0x%08X)", (unsigned)hr);
        } else {
            gldDiagLog("GL: glDrawPixels LockRect failed (hr=0x%08X)", (unsigned)hr);
        }

        _glsSurfRel(pTmp); pTmp = NULL;
        _glsSurfRel(pRT);  pRT  = NULL;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        /* The scratch surface is D3DPOOL_DEFAULT; swallowing the fault without
         * releasing it leaks a device resource and blocks a clean teardown. */
        if (pTmp) { _glsSurfRel(pTmp); pTmp = NULL; }
        if (pRT)  { _glsSurfRel(pRT);  pRT  = NULL; }
        gldDiagLogV("GL: glDrawPixels faulted inside D3D9");
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

/*
 * Convert a GL matrix to a D3D9 matrix.
 *
 * This is a straight copy, not an element-wise transpose.  The two APIs
 * differ in *both* storage order and vector convention, and the two
 * differences cancel: GL stores column-major and multiplies M*v with column
 * vectors, D3D9 stores row-major and multiplies v*M with row vectors, so the
 * same transform has an identical byte layout in both.
 *
 * Transposing element-wise here — as this function previously did — moves
 * the translation out of the last row into the last column, which silently
 * discards all translation: glTranslatef(10,20,30) applied to the origin
 * came out as (0,0,0), so nothing could be positioned correctly.
 */
static void _glsGLMatrixToD3D(D3DMATRIX *dst, const float *src)
{
    memcpy(dst, src, 16 * sizeof(float));
}

/*
 * Build the D3D9 projection matrix from GL's projection matrix.
 *
 * Two conventions differ between the APIs and both must be corrected here,
 * or geometry is silently destroyed before it ever reaches the rasteriser:
 *
 * 1. Clip-space depth.  GL projects z into NDC [-1,+1]; D3D9 clips against
 *    0 <= z <= w.  Handing a GL matrix straight to D3D9 therefore throws
 *    away the entire near half of every frustum.  D3D9 uses row vectors
 *    (clip = v * M), so the z output is column 3 and w is column 4, and the
 *    remap z' = (z + w) / 2 is applied by folding column 4 into column 3.
 *
 * 2. Pixel centre.  GL samples pixel centres at (i+0.5, j+0.5); D3D9's
 *    rasteriser effectively samples at integer coordinates, which shows up
 *    as a half-pixel smear on all texturing and a visible offset on 2D/UI.
 *    Correcting by a half pixel means shifting NDC x by -1/width and y by
 *    +1/height.  That shift has to scale with w to survive the perspective
 *    divide, so it is applied as column_x -= column_w/width rather than as
 *    a constant added to the translation row (which would only be correct
 *    for orthographic projections).
 */
static void _glsBuildD3DProjection(D3DMATRIX *dst, const float *glProj,
                                   const float adjust[4])
{
    float x1, x2, x3, x4, y1, y2, y3, y4;
    _glsGLMatrixToD3D(dst, glProj);

    /* 1. GL [-1,1] -> D3D [0,1] depth range */
    dst->_13 = 0.5f * (dst->_13 + dst->_14);
    dst->_23 = 0.5f * (dst->_23 + dst->_24);
    dst->_33 = 0.5f * (dst->_33 + dst->_34);
    dst->_43 = 0.5f * (dst->_43 + dst->_44);

    /* 2. Preserve the GL viewport transform after clipping it to D3D9's
     * render-target bounds; the offsets also contain the half-pixel fix. */
    x1 = dst->_11; x2 = dst->_21; x3 = dst->_31; x4 = dst->_41;
    y1 = dst->_12; y2 = dst->_22; y3 = dst->_32; y4 = dst->_42;
    dst->_11 = adjust[0] * x1 + adjust[2] * dst->_14;
    dst->_21 = adjust[0] * x2 + adjust[2] * dst->_24;
    dst->_31 = adjust[0] * x3 + adjust[2] * dst->_34;
    dst->_41 = adjust[0] * x4 + adjust[2] * dst->_44;
    dst->_12 = adjust[1] * y1 + adjust[3] * dst->_14;
    dst->_22 = adjust[1] * y2 + adjust[3] * dst->_24;
    dst->_32 = adjust[1] * y3 + adjust[3] * dst->_34;
    dst->_42 = adjust[1] * y4 + adjust[3] * dst->_44;
}

/*
 * Push the current GL modelview/projection onto the D3D9 device.
 *
 * GL keeps a single combined modelview matrix, so it goes to D3DTS_WORLD and
 * D3DTS_VIEW is held at identity.  Shared by the immediate-mode, DrawArrays
 * and DrawElements paths so the clip-space correction cannot be applied in
 * one and forgotten in another.
 *
 * Returns FALSE if the transforms could not be set and the draw should be
 * abandoned.
 */

/*
 * RTX Remix compatibility.
 *
 * Remix reconstructs a scene from intercepted D3D9 state.  It accepts both
 * fixed-function and programmable D3D9 draws, and shader-era
 * games must keep their translated shaders bound; removing them changes the
 * draw and used to make every modern GL program render as fixed function.
 * The compatibility requirement is instead to keep D3D transforms, textures
 * and render state current alongside the shaders, which the draw path does.
 */
static BOOL _glsShadersUsable(void)
{
    static BOOL logged = FALSE;

    if (gldIsRemixDetected() && !logged) {
        logged = TRUE;
        gldDiagLog("GL: RTX Remix active - preserving translated D3D9 shaders and "
                   "publishing transforms, textures, render state and UP geometry per draw");
    }
    return TRUE;
}

static BOOL _glsApplyTransforms(IDirect3DDevice9 *pDev, GLS_State *s)
{
    D3DMATRIX d3dWorld, d3dView, d3dProj;
    D3DVIEWPORT9 viewport;
    float adjust[4];

    if (!_glsBuildClippedViewport(pDev, s, &viewport, adjust))
        return FALSE;

    _glsGLMatrixToD3D(&d3dWorld, s->modelviewStack.stack[s->modelviewStack.top].m);
    _glsBuildD3DProjection(&d3dProj,
                           s->projectionStack.stack[s->projectionStack.top].m,
                           adjust);

    memset(&d3dView, 0, sizeof(d3dView));
    d3dView._11 = d3dView._22 = d3dView._33 = d3dView._44 = 1.0f;

    __try {
        IDirect3DDevice9_SetViewport(pDev, &viewport);
        IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &d3dWorld);
        IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &d3dView);
        IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &d3dProj);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING,
                                        s->enableLighting ? TRUE : FALSE);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }

    /* An ARB assembly program reads the same matrices through state.matrix.*,
     * which are constants on the device rather than fixed-function transforms,
     * so they have to be re-pushed alongside them. */
    _glsApplyARBStateParams();

    return TRUE;
}

/* ===================================================================
 *  SECTION 0b: Vertex array assembly
 *
 *  Shared by the DrawArrays and DrawElements paths.  Reads whatever the
 *  application supplied — generic vertex attributes on a VAO (GL 2.0+) or
 *  legacy client-side arrays (GL 1.1) — and assembles the single fat
 *  GLS_D3DVertex format the fixed-function submission path uses.
 * =================================================================== */

/*
 * Resolve an array's base address.
 *
 * GL overloads `pointer`: if a buffer object was bound when the pointer was
 * set it is a byte offset into that buffer, otherwise it is a raw client
 * memory address.  Returns NULL when the source is unusable.
 */
static const unsigned char *_glsResolveArrayBase(GLuint_t bufferBinding, const void *pointer)
{
    if (bufferBinding) {
        GLS_Buffer *buf = glsFindBuffer(bufferBinding);
        if (!buf || !buf->data) return NULL;
        return (const unsigned char *)buf->data + (ptrdiff_t)pointer;
    }
    return (const unsigned char *)pointer;
}

/* Bytes occupied by one component of a GL array type. */
static int _glsTypeSize(unsigned int type)
{
    switch (type) {
    case GL_BYTE:  case GL_UNSIGNED_BYTE:  return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
    case GL_INT:   case GL_UNSIGNED_INT:
    case GL_FLOAT:                         return 4;
    case GL_DOUBLE:                        return 8;
    default:                               return 4;
    }
}

/*
 * Read one element of a typed GL vertex array into out[4].
 *
 * `normalized` follows GL's rule: integer components are scaled to [0,1]
 * when unsigned and [-1,1] when signed; floats are always passed through.
 * Components the array does not supply keep the caller's defaults, which is
 * what makes a 2-component glTexCoordPointer or 3-component colour work.
 */
static void _glsReadArrayElement(const unsigned char *base, int stride, int index,
                                 int size, unsigned int type, BOOL normalized,
                                 float *out)
{
    const unsigned char *p;
    int i;

    if (!base || size <= 0) return;
    if (stride <= 0) stride = size * _glsTypeSize(type);
    if (size > 4) size = 4;
    p = base + (ptrdiff_t)index * stride;

    for (i = 0; i < size; i++) {
        switch (type) {
        case GL_FLOAT:
            out[i] = ((const float *)p)[i];
            break;
        case GL_DOUBLE:
            out[i] = (float)((const double *)p)[i];
            break;
        case GL_BYTE: {
            signed char v = ((const signed char *)p)[i];
            out[i] = normalized ? (float)v / 127.0f : (float)v;
            break;
        }
        case GL_UNSIGNED_BYTE: {
            unsigned char v = p[i];
            out[i] = normalized ? (float)v / 255.0f : (float)v;
            break;
        }
        case GL_SHORT: {
            short v = ((const short *)p)[i];
            out[i] = normalized ? (float)v / 32767.0f : (float)v;
            break;
        }
        case GL_UNSIGNED_SHORT: {
            unsigned short v = ((const unsigned short *)p)[i];
            out[i] = normalized ? (float)v / 65535.0f : (float)v;
            break;
        }
        case GL_INT: {
            int v = ((const int *)p)[i];
            out[i] = normalized ? (float)v / 2147483647.0f : (float)v;
            break;
        }
        case GL_UNSIGNED_INT: {
            unsigned int v = ((const unsigned int *)p)[i];
            out[i] = normalized ? (float)v / 4294967295.0f : (float)v;
            break;
        }
        default:
            break;
        }
    }
}

/* Pack a float colour into D3DCOLOR, clamping first.  GL allows colours
 * outside [0,1]; D3DCOLOR_COLORVALUE truncates to DWORD without clamping,
 * so an out-of-range channel would wrap and invert the colour. */
static DWORD _glsPackColor(const float *c)
{
    float r = c[0] < 0.0f ? 0.0f : (c[0] > 1.0f ? 1.0f : c[0]);
    float g = c[1] < 0.0f ? 0.0f : (c[1] > 1.0f ? 1.0f : c[1]);
    float b = c[2] < 0.0f ? 0.0f : (c[2] > 1.0f ? 1.0f : c[2]);
    float a = c[3] < 0.0f ? 0.0f : (c[3] > 1.0f ? 1.0f : c[3]);

    return D3DCOLOR_ARGB((DWORD)(a * 255.0f + 0.5f),
                         (DWORD)(r * 255.0f + 0.5f),
                         (DWORD)(g * 255.0f + 0.5f),
                         (DWORD)(b * 255.0f + 0.5f));
}

/* Where one vertex semantic is read from, reduced from either a generic
 * vertex attrib or a legacy client array. */
typedef struct {
    const unsigned char *base;
    int          stride;
    int          size;
    unsigned int type;
    BOOL         normalized;
    BOOL         present;
} GLS_AttribSource;

typedef struct {
    GLS_AttribSource pos, normal, color, tex0, tex1;
    GLS_AttribSource generic6, generic7;
} GLS_VertexSources;

static void _glsSourceFromAttrib(GLS_AttribSource *src, const GLS_VertexAttrib *a)
{
    src->present = FALSE;
    if (!a || !a->enabled) return;
    src->base = _glsResolveArrayBase(a->bufferBinding, a->pointer);
    if (!src->base) return;
    src->stride     = a->stride;
    src->size       = a->size;
    src->type       = a->type;
    src->normalized = a->normalized ? TRUE : FALSE;
    src->present    = TRUE;
}

static void _glsSourceFromClientArray(GLS_AttribSource *src, const GLS_ClientArray *c,
                                      BOOL normalized)
{
    src->present = FALSE;
    if (!c || !c->enabled) return;
    src->base = _glsResolveArrayBase(c->bufferBinding, c->pointer);
    if (!src->base) return;
    src->stride     = c->stride;
    src->size       = c->size;
    src->type       = c->type;
    src->normalized = normalized;
    src->present    = TRUE;
}

/*
 * Work out where each vertex semantic comes from for this draw.
 *
 * Generic attributes on the bound VAO win, using the classic fixed-function
 * aliasing (0 = position, 2 = normal, 3 = colour, 8/9 = texcoord 0/1).
 * Anything the VAO does not supply falls back to the GL 1.1 client arrays,
 * so both eras draw through one path.
 *
 * Attributes 6 and 7 are the exception: ARB_vertex_program leaves them with no
 * conventional alias, so they resolve directly to their own vertex-format
 * fields and have no legacy client array to fall back to (there never was a
 * glTangentPointer).  When the application binds neither, `present` stays
 * FALSE and _glsBuildVertex supplies GL's default attribute value.
 */
static void _glsResolveVertexSources(GLS_State *s, GLS_VertexSources *out)
{
    GLS_VAO *vao = glsFindVAO(s->boundVAO);

    memset(out, 0, sizeof(*out));

    if (vao) {
        _glsSourceFromAttrib(&out->pos,    &vao->attribs[0]);
        _glsSourceFromAttrib(&out->normal, &vao->attribs[2]);
        _glsSourceFromAttrib(&out->color,  &vao->attribs[3]);
        _glsSourceFromAttrib(&out->tex0,   &vao->attribs[8]);
        _glsSourceFromAttrib(&out->tex1,   &vao->attribs[9]);
        _glsSourceFromAttrib(&out->generic6, &vao->attribs[6]);
        _glsSourceFromAttrib(&out->generic7, &vao->attribs[7]);
    }

    /* Integer colours and normals are normalised by definition in the legacy
     * entry points: glColorPointer(GL_UNSIGNED_BYTE) means 0..255 -> 0..1. */
    if (!out->pos.present)
        _glsSourceFromClientArray(&out->pos,    &s->clientVertexArray,      FALSE);
    if (!out->normal.present)
        _glsSourceFromClientArray(&out->normal, &s->clientNormalArray,      TRUE);
    if (!out->color.present)
        _glsSourceFromClientArray(&out->color,  &s->clientColorArray,       TRUE);
    if (!out->tex0.present)
        _glsSourceFromClientArray(&out->tex0,   &s->clientTexCoordArray[0], FALSE);
    if (!out->tex1.present)
        _glsSourceFromClientArray(&out->tex1,   &s->clientTexCoordArray[1], FALSE);
}

/* Assemble one output vertex from the resolved sources. */
static void _glsBuildVertex(GLS_State *s, const GLS_VertexSources *src,
                            int index, GLS_D3DVertex *out)
{
    float pos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float nrm[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    float t0[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
    float t1[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
    /* GL's default value for an unbound generic vertex attribute. */
    float g6[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
    float g7[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
    float col[4];

    /* Absent colour array means the vertex takes glColor's current value. */
    col[0] = s->currentColor[0];
    col[1] = s->currentColor[1];
    col[2] = s->currentColor[2];
    col[3] = s->currentColor[3];

    if (src->pos.present)
        _glsReadArrayElement(src->pos.base, src->pos.stride, index,
                             src->pos.size, src->pos.type, src->pos.normalized, pos);
    if (src->normal.present)
        _glsReadArrayElement(src->normal.base, src->normal.stride, index,
                             src->normal.size, src->normal.type, src->normal.normalized, nrm);
    if (src->color.present)
        _glsReadArrayElement(src->color.base, src->color.stride, index,
                             src->color.size, src->color.type, src->color.normalized, col);
    if (src->tex0.present)
        _glsReadArrayElement(src->tex0.base, src->tex0.stride, index,
                             src->tex0.size, src->tex0.type, src->tex0.normalized, t0);
    if (src->tex1.present)
        _glsReadArrayElement(src->tex1.base, src->tex1.stride, index,
                             src->tex1.size, src->tex1.type, src->tex1.normalized, t1);
    if (src->generic6.present)
        _glsReadArrayElement(src->generic6.base, src->generic6.stride, index,
                             src->generic6.size, src->generic6.type,
                             src->generic6.normalized, g6);
    if (src->generic7.present)
        _glsReadArrayElement(src->generic7.base, src->generic7.stride, index,
                             src->generic7.size, src->generic7.type,
                             src->generic7.normalized, g7);

    out->x  = pos[0]; out->y  = pos[1]; out->z  = pos[2];
    out->nx = nrm[0]; out->ny = nrm[1]; out->nz = nrm[2];
    out->color = _glsPackColor(col);
    {
        float sc[4];
        sc[0] = s->secondaryColor[0]; sc[1] = s->secondaryColor[1];
        sc[2] = s->secondaryColor[2]; sc[3] = 0.0f;
        out->specular = _glsPackColor(sc);
    }
    out->u0 = t0[0]; out->v0 = t0[1];
    out->u1 = t1[0]; out->v1 = t1[1];
    memcpy(out->genericAttrib6, g6, sizeof(g6));
    memcpy(out->genericAttrib7, g7, sizeof(g7));
}

/*
 * Number of indices needed to express `count` vertices of `mode` as a D3D9
 * primitive.  Returns 0 for modes that cannot be drawn.
 */
static int _glsExpandedIndexCount(unsigned int mode, int count)
{
    switch (mode) {
    case 0x0000: return count;                  /* GL_POINTS */
    case 0x0001: return (count / 2) * 2;        /* GL_LINES */
    case 0x0002: return count >= 2 ? count + 1 : 0; /* GL_LINE_LOOP -> closed strip */
    case 0x0003: return count >= 2 ? count : 0; /* GL_LINE_STRIP */
    case 0x0004: return (count / 3) * 3;        /* GL_TRIANGLES */
    case 0x0005:                                /* GL_TRIANGLE_STRIP */
    case 0x0006: return count >= 3 ? count : 0; /* GL_TRIANGLE_FAN */
    case 0x0007: return (count / 4) * 6;        /* GL_QUADS -> 2 tris each */
    case 0x0008: return count >= 4 ? (count / 2) * 2 : 0; /* GL_QUAD_STRIP */
    case 0x0009: return count >= 3 ? count : 0; /* GL_POLYGON -> fan */
    default:     return 0;
    }
}

/*
 * Expand a GL primitive mode into a D3D9 primitive type plus an index list
 * over the assembled vertices.
 *
 * D3D9 has no GL_QUADS, GL_POLYGON or GL_LINE_LOOP, so those are tessellated
 * here.  GL_QUAD_STRIP needs no work: its vertex order already matches a
 * D3D triangle strip.  Modes that map one-to-one still generate an identity
 * index list so every draw can use a single indexed submission path.
 *
 * `out` must hold _glsExpandedIndexCount(mode, count) entries.  Returns the
 * primitive count, or 0 if the mode cannot be drawn.
 */
static int _glsExpandPrimitive(unsigned int mode, int count,
                               D3DPRIMITIVETYPE *outType, unsigned int *out)
{
    int i, n = 0;

    switch (mode) {
    case 0x0000: /* GL_POINTS */
        *outType = D3DPT_POINTLIST;
        for (i = 0; i < count; i++) out[n++] = i;
        return count;

    case 0x0001: /* GL_LINES */
        *outType = D3DPT_LINELIST;
        for (i = 0; i + 1 < count; i += 2) { out[n++] = i; out[n++] = i + 1; }
        return count / 2;

    case 0x0002: /* GL_LINE_LOOP — D3D has no loop, close the strip manually */
        if (count < 2) return 0;
        *outType = D3DPT_LINESTRIP;
        for (i = 0; i < count; i++) out[n++] = i;
        out[n++] = 0;
        return count;

    case 0x0003: /* GL_LINE_STRIP */
        if (count < 2) return 0;
        *outType = D3DPT_LINESTRIP;
        for (i = 0; i < count; i++) out[n++] = i;
        return count - 1;

    case 0x0004: /* GL_TRIANGLES */
        *outType = D3DPT_TRIANGLELIST;
        for (i = 0; i + 2 < count; i += 3) { out[n++] = i; out[n++] = i + 1; out[n++] = i + 2; }
        return count / 3;

    case 0x0005: /* GL_TRIANGLE_STRIP */
        if (count < 3) return 0;
        *outType = D3DPT_TRIANGLESTRIP;
        for (i = 0; i < count; i++) out[n++] = i;
        return count - 2;

    case 0x0006: /* GL_TRIANGLE_FAN */
        if (count < 3) return 0;
        *outType = D3DPT_TRIANGLEFAN;
        for (i = 0; i < count; i++) out[n++] = i;
        return count - 2;

    case 0x0007: /* GL_QUADS — split each quad into triangles 0-1-2 and 0-2-3 */
        *outType = D3DPT_TRIANGLELIST;
        for (i = 0; i + 3 < count; i += 4) {
            out[n++] = i;     out[n++] = i + 1; out[n++] = i + 2;
            out[n++] = i;     out[n++] = i + 2; out[n++] = i + 3;
        }
        return (count / 4) * 2;

    case 0x0008: /* GL_QUAD_STRIP — same winding as a D3D triangle strip */
        if (count < 4) return 0;
        *outType = D3DPT_TRIANGLESTRIP;
        count = (count / 2) * 2;  /* trailing odd vertex cannot form a quad */
        for (i = 0; i < count; i++) out[n++] = i;
        return count - 2;

    case 0x0009: /* GL_POLYGON — convex by GL's definition, so a fan is exact */
        if (count < 3) return 0;
        *outType = D3DPT_TRIANGLEFAN;
        for (i = 0; i < count; i++) out[n++] = i;
        return count - 2;

    default:
        return 0;
    }
}

/*
 * Submit assembled vertices as one indexed D3D9 draw.
 *
 * 16-bit indices are used when the vertex count allows, since D3DFMT_INDEX32
 * is an optional capability that some drivers refuse.
 */
static void _glsSubmitIndexed(IDirect3DDevice9 *pDev, D3DPRIMITIVETYPE primType,
                              int primCount, const GLS_D3DVertex *verts, int vertCount,
                              const unsigned int *indices, int indexCount)
{
    if (primCount <= 0 || vertCount <= 0 || indexCount <= 0) return;

    __try {
        IDirect3DDevice9_SetFVF(pDev, GLS_D3DFVF);

        if (vertCount <= 65536) {
            unsigned short *idx16 = (unsigned short *)malloc(indexCount * sizeof(unsigned short));
            int i;
            if (!idx16) return;
            for (i = 0; i < indexCount; i++) idx16[i] = (unsigned short)indices[i];
            IDirect3DDevice9_DrawIndexedPrimitiveUP(pDev, primType, 0, vertCount,
                                                    primCount, idx16, D3DFMT_INDEX16,
                                                    verts, sizeof(GLS_D3DVertex));
            free(idx16);
        } else {
            IDirect3DDevice9_DrawIndexedPrimitiveUP(pDev, primType, 0, vertCount,
                                                    primCount, indices, D3DFMT_INDEX32,
                                                    verts, sizeof(GLS_D3DVertex));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

typedef struct {
    float position[4];
    float varying[GLS_MAX_STAGE_VARYINGS][4];
} GLS_PostStageVertex;

static IDirect3DVertexShader9 *g_postStageVS;
static IDirect3DVertexDeclaration9 *g_postStageDecl;
static IDirect3DDevice9 *g_postStageDevice;

static void _glsReleasePostStagePipeline(void)
{
    if (g_postStageVS) {
        IDirect3DVertexShader9_Release(g_postStageVS);
        g_postStageVS = NULL;
    }
    if (g_postStageDecl) {
        IDirect3DVertexDeclaration9_Release(g_postStageDecl);
        g_postStageDecl = NULL;
    }
    g_postStageDevice = NULL;
}

static BOOL _glsEnsurePostStagePipeline(IDirect3DDevice9 *pDev)
{
    static const char source[] =
        "#version 330 core\n"
        "in vec4 gldPos_SEM_POSITION;\n"
        "in vec4 gldV0_SEM_TEXCOORD0;\n" "in vec4 gldV1_SEM_TEXCOORD1;\n"
        "in vec4 gldV2_SEM_TEXCOORD2;\n" "in vec4 gldV3_SEM_TEXCOORD3;\n"
        "in vec4 gldV4_SEM_TEXCOORD4;\n" "in vec4 gldV5_SEM_TEXCOORD5;\n"
        "in vec4 gldV6_SEM_TEXCOORD6;\n" "in vec4 gldV7_SEM_TEXCOORD7;\n"
        "out vec4 gldO0_SEM_TEXCOORD0;\n" "out vec4 gldO1_SEM_TEXCOORD1;\n"
        "out vec4 gldO2_SEM_TEXCOORD2;\n" "out vec4 gldO3_SEM_TEXCOORD3;\n"
        "out vec4 gldO4_SEM_TEXCOORD4;\n" "out vec4 gldO5_SEM_TEXCOORD5;\n"
        "out vec4 gldO6_SEM_TEXCOORD6;\n" "out vec4 gldO7_SEM_TEXCOORD7;\n"
        "void main(){ gl_Position=gldPos_SEM_POSITION; "
        "gldO0_SEM_TEXCOORD0=gldV0_SEM_TEXCOORD0; "
        "gldO1_SEM_TEXCOORD1=gldV1_SEM_TEXCOORD1; "
        "gldO2_SEM_TEXCOORD2=gldV2_SEM_TEXCOORD2; "
        "gldO3_SEM_TEXCOORD3=gldV3_SEM_TEXCOORD3; "
        "gldO4_SEM_TEXCOORD4=gldV4_SEM_TEXCOORD4; "
        "gldO5_SEM_TEXCOORD5=gldV5_SEM_TEXCOORD5; "
        "gldO6_SEM_TEXCOORD6=gldV6_SEM_TEXCOORD6; "
        "gldO7_SEM_TEXCOORD7=gldV7_SEM_TEXCOORD7; }\n";
    D3DVERTEXELEMENT9 decl[2 + GLS_MAX_STAGE_VARYINGS];
    void *code = NULL;
    DWORD codeSize = 0;
    HRESULT hr;
    int i;

    if (g_postStageDevice == pDev && g_postStageVS && g_postStageDecl)
        return TRUE;
    _glsReleasePostStagePipeline();
    if (!glslTranspileAndCompile(0, source, &code, &codeSize)) return FALSE;
    if (!glslCreateVertexShader(pDev, code, codeSize, &g_postStageVS)) {
        glslFreeBytecode(code);
        return FALSE;
    }
    glslFreeBytecode(code);

    decl[0].Stream = 0; decl[0].Offset = 0; decl[0].Type = D3DDECLTYPE_FLOAT4;
    decl[0].Method = D3DDECLMETHOD_DEFAULT; decl[0].Usage = D3DDECLUSAGE_POSITION;
    decl[0].UsageIndex = 0;
    for (i = 0; i < GLS_MAX_STAGE_VARYINGS; ++i) {
        decl[i + 1].Stream = 0;
        decl[i + 1].Offset = (WORD)(sizeof(float) * 4 * (i + 1));
        decl[i + 1].Type = D3DDECLTYPE_FLOAT4;
        decl[i + 1].Method = D3DDECLMETHOD_DEFAULT;
        decl[i + 1].Usage = D3DDECLUSAGE_TEXCOORD;
        decl[i + 1].UsageIndex = (BYTE)i;
    }
    decl[GLS_MAX_STAGE_VARYINGS + 1] = (D3DVERTEXELEMENT9)D3DDECL_END();
    hr = IDirect3DDevice9_CreateVertexDeclaration(pDev, decl, &g_postStageDecl);
    if (FAILED(hr)) {
        _glsReleasePostStagePipeline();
        return FALSE;
    }
    g_postStageDevice = pDev;
    return TRUE;
}

/*
 * glArrayElement — draw one logical vertex from the enabled client arrays.
 *
 * Everything else in the immediate-mode path reaches the vertex buffer through
 * the glColor/glNormal/glTexCoord/glVertex entry points, so replaying an array
 * element through those same calls keeps a glBegin block that mixes
 * glArrayElement with explicit vertices consistent.  Vertex position goes last
 * because it is what commits the vertex.
 */
void _glsArrayElement(int i)
{
    GLS_State *s = glsGetState();
    GLS_VertexSources src;
    float v[4];
    int u;

    if (i < 0) return;
    _glsResolveVertexSources(s, &src);

    if (src.normal.present) {
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 1.0f; v[3] = 0.0f;
        _glsReadArrayElement(src.normal.base, src.normal.stride, i,
                             src.normal.size, src.normal.type, src.normal.normalized, v);
        _glsNormal3f(v[0], v[1], v[2]);
    }
    if (src.color.present) {
        v[0] = v[1] = v[2] = v[3] = 1.0f;
        _glsReadArrayElement(src.color.base, src.color.stride, i,
                             src.color.size, src.color.type, src.color.normalized, v);
        _glsColor4f(v[0], v[1], v[2], v[3]);
    }
    for (u = 0; u < GLS_MAX_TEX_UNITS; u++) {
        const GLS_ClientArray *c = &s->clientTexCoordArray[u];
        const unsigned char *base;
        if (!c->enabled) continue;
        base = _glsResolveArrayBase(c->bufferBinding, c->pointer);
        if (!base) continue;
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f; v[3] = 1.0f;
        _glsReadArrayElement(base, c->stride, i, c->size, c->type, FALSE, v);
        _glsMultiTexCoord4fARB((unsigned int)(GL_TEXTURE0 + u), v[0], v[1], v[2], v[3]);
    }
    if (src.pos.present) {
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f; v[3] = 1.0f;
        _glsReadArrayElement(src.pos.base, src.pos.stride, i,
                             src.pos.size, src.pos.type, src.pos.normalized, v);
        _glsVertex4f(v[0], v[1], v[2], v[3]);
    }
}

/*
 * glInterleavedArrays — one packed array standing in for several pointers.
 *
 * The GL specification defines each format as an exact table of component
 * counts, types and byte offsets; this reproduces that table and then calls
 * the ordinary pointer entry points, so there is only one array-reading path
 * to keep correct.
 */
void _glsInterleavedArrays(unsigned int format, int stride, const void *pointer)
{
    const int f = (int)sizeof(float);
    const int c = 4;                /* 4 GLubytes, padded to a float boundary */
    int et = 0, ec = 0, en = 0;     /* which arrays the format supplies */
    int st = 0, sc = 0, sv = 0;     /* component counts                 */
    unsigned int tc = GL_FLOAT;     /* colour component type            */
    int pc = 0, pn = 0, pv = 0;     /* byte offsets                     */
    int defStride = 0;
    const unsigned char *base = (const unsigned char *)pointer;

    switch (format) {
    case 0x2A20: /* GL_V2F */              sv = 2; pv = 0;   defStride = 2*f; break;
    case 0x2A21: /* GL_V3F */              sv = 3; pv = 0;   defStride = 3*f; break;
    case 0x2A22: /* GL_C4UB_V2F */         ec = 1; sc = 4; tc = GL_UNSIGNED_BYTE;
                                           pc = 0; sv = 2; pv = c; defStride = c + 2*f; break;
    case 0x2A23: /* GL_C4UB_V3F */         ec = 1; sc = 4; tc = GL_UNSIGNED_BYTE;
                                           pc = 0; sv = 3; pv = c; defStride = c + 3*f; break;
    case 0x2A24: /* GL_C3F_V3F */          ec = 1; sc = 3; pc = 0;
                                           sv = 3; pv = 3*f; defStride = 6*f; break;
    case 0x2A25: /* GL_N3F_V3F */          en = 1; pn = 0;
                                           sv = 3; pv = 3*f; defStride = 6*f; break;
    case 0x2A26: /* GL_C4F_N3F_V3F */      ec = 1; sc = 4; pc = 0; en = 1; pn = 4*f;
                                           sv = 3; pv = 7*f; defStride = 10*f; break;
    case 0x2A27: /* GL_T2F_V3F */          et = 1; st = 2;
                                           sv = 3; pv = 2*f; defStride = 5*f; break;
    case 0x2A28: /* GL_T4F_V4F */          et = 1; st = 4;
                                           sv = 4; pv = 4*f; defStride = 8*f; break;
    case 0x2A29: /* GL_T2F_C4UB_V3F */     et = 1; st = 2; ec = 1; sc = 4;
                                           tc = GL_UNSIGNED_BYTE; pc = 2*f;
                                           sv = 3; pv = 2*f + c; defStride = 2*f + c + 3*f; break;
    case 0x2A2A: /* GL_T2F_C3F_V3F */      et = 1; st = 2; ec = 1; sc = 3; pc = 2*f;
                                           sv = 3; pv = 5*f; defStride = 8*f; break;
    case 0x2A2B: /* GL_T2F_N3F_V3F */      et = 1; st = 2; en = 1; pn = 2*f;
                                           sv = 3; pv = 5*f; defStride = 8*f; break;
    case 0x2A2C: /* GL_T2F_C4F_N3F_V3F */  et = 1; st = 2; ec = 1; sc = 4; pc = 2*f;
                                           en = 1; pn = 6*f;
                                           sv = 3; pv = 9*f; defStride = 12*f; break;
    case 0x2A2D: /* GL_T4F_C4F_N3F_V4F */  et = 1; st = 4; ec = 1; sc = 4; pc = 4*f;
                                           en = 1; pn = 8*f;
                                           sv = 4; pv = 11*f; defStride = 15*f; break;
    default:
        gldDiagLogV("GL: glInterleavedArrays unknown format 0x%X, arrays unchanged", format);
        glsGetState()->lastError = GL_INVALID_ENUM;
        return;
    }

    if (stride <= 0) stride = defStride;

    /* GL_TEXTURE_COORD_ARRAY / GL_COLOR_ARRAY / GL_NORMAL_ARRAY / GL_VERTEX_ARRAY */
    if (et) { _glsEnableClientState(0x8078);  _glsTexCoordPointer(st, GL_FLOAT, stride, base); }
    else      _glsDisableClientState(0x8078);
    if (ec) { _glsEnableClientState(0x8076);  _glsColorPointer(sc, tc, stride, base + pc); }
    else      _glsDisableClientState(0x8076);
    if (en) { _glsEnableClientState(0x8075);  _glsNormalPointer(GL_FLOAT, stride, base + pn); }
    else      _glsDisableClientState(0x8075);
    _glsEnableClientState(0x8074);
    _glsVertexPointer(sv, GL_FLOAT, stride, base + pv);

    gldDiagLogV("GL: glInterleavedArrays(0x%X stride=%d)", format, stride);
}

/*
 * glIndexPointer — colour-index mode.
 *
 * This pipeline is RGBA only: D3D9 has no palettised colour path for vertex
 * colours, and there is no index-to-RGBA lookup to route the values through.
 * Rather than record a pointer nothing will ever read, this says so once and
 * skips.  Colour-index rendering is intentionally unsupported.
 */
void _glsIndexPointer(unsigned int type, int stride, const void *pointer)
{
    static BOOL reported = FALSE;
    (void)stride; (void)pointer;
    if (!reported) {
        reported = TRUE;
        gldDiagLog("GL: glIndexPointer(type=0x%X) — colour-index mode has no D3D9 "
                   "equivalent in this RGBA-only pipeline, intentionally unsupported", type);
    }
}

/*
 * glLockArraysEXT / glUnlockArraysEXT (EXT_compiled_vertex_array).
 *
 * The extension is explicitly a performance hint: an implementation is free to
 * do nothing observable, and this one does exactly that — but it records the
 * locked range so the state is queryable and so a later array change that
 * contradicts the lock can be reported.  This is a legitimately empty
 * operation, not an unimplemented one.
 */
void _glsLockArraysEXT(int first, int count)
{
    GLS_State *s = glsGetState();
    s->lockedArrayFirst = first;
    s->lockedArrayCount = count;
    s->arraysLocked = TRUE;
    gldDiagLogV("GL: glLockArraysEXT(first=%d count=%d) — recorded as a hint", first, count);
}

void _glsUnlockArraysEXT(void)
{
    GLS_State *s = glsGetState();
    s->arraysLocked = FALSE;
    s->lockedArrayFirst = 0;
    s->lockedArrayCount = 0;
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
            s->textures[id].minFilter = GL_NEAREST_MIPMAP_LINEAR;
            s->textures[id].magFilter = GL_LINEAR;
            s->textures[id].wrapS = 0x2901;
            s->textures[id].wrapT = 0x2901;
            s->textures[id].wrapR = 0x2901;
        }
        textures[i] = id;
    }
    gldDiagLogV("GL: glGenTextures(%d) -> first=%u", n, textures[0]);
}

/* Defined with the rest of the texture code, needed here by glDeleteTextures
 * which sits ahead of that section. */
static void _glsReleaseTextureResources(GLS_Texture *tex);

void _glsDeleteTextures(int n, const unsigned int *textures)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int i, u;
    if (!textures || n <= 0) return;
    for (i = 0; i < n; i++) {
        GLS_Texture *tex = glsFindTexture(textures[i]);
        if (tex) {
            /* Unbind from any texture unit first, on the device as well as in
             * the state table: releasing a texture the device is still bound to
             * leaves D3D9 holding the last reference, which is exactly how a
             * texture outlives the delete that was supposed to free it. */
            for (u = 0; u < GLS_MAX_TEX_UNITS; u++) {
                BOOL bound2D = (s->boundTexture2D[u] == textures[i]);
                if (s->boundTexture2D[u] == textures[i]) s->boundTexture2D[u] = 0;
                if (s->boundTextureCube[u] == textures[i]) s->boundTextureCube[u] = 0;
                if (s->boundTexture3D[u] == textures[i]) s->boundTexture3D[u] = 0;
                if (s->boundTextureBuffer[u] == textures[i]) s->boundTextureBuffer[u] = 0;
                if (pDev && bound2D) {
                    __try {
                        IDirect3DDevice9_SetTexture(pDev, (DWORD)u, NULL);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        /* Device lost or invalid — ignore */
                    }
                }
            }
            /* Releases pTex, pCubeTex *and* pVolTex, and frees the CPU copy. */
            __try {
                _glsReleaseTextureResources(tex);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            tex->allocated = FALSE;
        }
    }
}

void _glsBindTexture(unsigned int target, unsigned int texture)
{
    GLS_DLUInt2 a = {{ target, texture }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    if (_glsDLRecord(_glsDLBindTexture, &a, sizeof(a))) return;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;

    /* Auto-allocate if binding a non-zero texture that doesn't exist yet */
    if (texture != 0 && texture < GLS_MAX_TEXTURES && !s->textures[texture].allocated) {
        memset(&s->textures[texture], 0, sizeof(GLS_Texture));
        s->textures[texture].id = texture;
        s->textures[texture].target = target;
        s->textures[texture].allocated = TRUE;
        s->textures[texture].minFilter = GL_NEAREST_MIPMAP_LINEAR;
        s->textures[texture].magFilter = GL_LINEAR;
        s->textures[texture].wrapS = 0x2901;
        s->textures[texture].wrapT = 0x2901;
        s->textures[texture].wrapR = 0x2901;
    }

    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_1D ||
        target == GL_TEXTURE_2D_MULTISAMPLE) {
        /* A 1D texture is stored as a 2D texture one row high, so the two
         * targets share this binding point. */
        s->boundTexture2D[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = target;
    } else if (target == GL_TEXTURE_CUBE_MAP) {
        s->boundTextureCube[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = GL_TEXTURE_CUBE_MAP;
    } else if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        s->boundTexture3D[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = GL_TEXTURE_3D;
    } else if (target == GL_TEXTURE_BUFFER) {
        s->boundTextureBuffer[unit] = texture;
        if (texture && texture < GLS_MAX_TEXTURES)
            s->textures[texture].target = GL_TEXTURE_BUFFER;
    }

    /* Set D3D9 texture on the device */
    if (pDev && (target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_MULTISAMPLE ||
                 target == GL_TEXTURE_BUFFER)) {
        GLS_Texture *tex = (texture != 0) ? glsFindTexture(texture) : NULL;
        IDirect3DBaseTexture9 *baseTex = (tex && tex->pTex) ? (IDirect3DBaseTexture9 *)tex->pTex : NULL;
        __try {
            IDirect3DDevice9_SetTexture(pDev, unit, baseTex);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            /* Device lost or invalid — ignore */
        }
        if (!s->boundSampler[unit])
            _applyTextureObjectSamplingToD3D((unsigned int)unit, tex);
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
            if (s->boundTextureBufferObject == buffers[i]) s->boundTextureBufferObject = 0;
            if (s->boundPixelPackBuffer == buffers[i]) s->boundPixelPackBuffer = 0;
            if (s->boundPixelUnpackBuffer == buffers[i]) s->boundPixelUnpackBuffer = 0;
            if (s->boundCopyReadBuffer == buffers[i]) s->boundCopyReadBuffer = 0;
            if (s->boundCopyWriteBuffer == buffers[i]) s->boundCopyWriteBuffer = 0;
            if (s->boundUniformBuffer == buffers[i]) s->boundUniformBuffer = 0;
            if (s->boundTransformFeedbackBuffer == buffers[i]) s->boundTransformFeedbackBuffer = 0;
            if (s->boundShaderStorageBuffer == buffers[i]) s->boundShaderStorageBuffer = 0;
            if (s->boundAtomicCounterBuffer == buffers[i]) s->boundAtomicCounterBuffer = 0;
            if (s->boundDrawIndirectBuffer == buffers[i]) s->boundDrawIndirectBuffer = 0;
            if (s->boundDispatchIndirectBuffer == buffers[i]) s->boundDispatchIndirectBuffer = 0;
            if (s->boundParameterBuffer == buffers[i]) s->boundParameterBuffer = 0;
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
    else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        GLS_VAO *vao;
        s->boundElementBuffer = buffer;
        vao = glsFindVAO(s->boundVAO);
        if (vao) vao->elementBuffer = buffer;
    }
    else if (target == 0x88EB) /* GL_PIXEL_PACK_BUFFER */
        s->boundPixelPackBuffer = buffer;
    else if (target == GL_TEXTURE_BUFFER)
        s->boundTextureBufferObject = buffer;
    else if (target == 0x88EC) /* GL_PIXEL_UNPACK_BUFFER */
        s->boundPixelUnpackBuffer = buffer;
    else if (target == 0x8F36) /* GL_COPY_READ_BUFFER */
        s->boundCopyReadBuffer = buffer;
    else if (target == 0x8F37) /* GL_COPY_WRITE_BUFFER */
        s->boundCopyWriteBuffer = buffer;
    else if (target == 0x8A11) /* GL_UNIFORM_BUFFER */
        s->boundUniformBuffer = buffer;
    else if (target == 0x8C8E) /* GL_TRANSFORM_FEEDBACK_BUFFER */
        s->boundTransformFeedbackBuffer = buffer;
    else if (target == 0x90D2) /* GL_SHADER_STORAGE_BUFFER */
        s->boundShaderStorageBuffer = buffer;
    else if (target == 0x92C0) /* GL_ATOMIC_COUNTER_BUFFER */
        s->boundAtomicCounterBuffer = buffer;
    else if (target == 0x8F3F) /* GL_DRAW_INDIRECT_BUFFER */
        s->boundDrawIndirectBuffer = buffer;
    else if (target == 0x90EE) /* GL_DISPATCH_INDIRECT_BUFFER */
        s->boundDispatchIndirectBuffer = buffer;
    else if (target == 0x80EE) /* GL_PARAMETER_BUFFER_ARB */
        s->boundParameterBuffer = buffer;
}

void _glsGenVertexArrays(int n, unsigned int *arrays)
{
    GLS_State *s = glsGetState();
    int i;
    if (!arrays || n <= 0) return;
    for (i = 0; i < n; i++) {
        unsigned int id = s->nextVaoId++;
        if (id < GLS_MAX_VAOS) {
            int j;
            memset(&s->vaos[id], 0, sizeof(GLS_VAO));
            s->vaos[id].id = id;
            s->vaos[id].allocated = TRUE;
            for (j = 0; j < GLS_MAX_VERTEX_ATTRIBS; ++j)
                s->vaos[id].attribs[j].defaultValue[3] = 1.0f;
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
        GLS_VAO *vao;
        /* GL silently ignores name 0 in a delete list. glsFindVAO(0) returns
         * the always-allocated default VAO, so without this guard a delete
         * list containing a 0 would permanently free it. */
        if (arrays[i] == 0) continue;
        vao = glsFindVAO(arrays[i]);
        if (vao) {
            if (s->boundVAO == arrays[i]) s->boundVAO = 0;
            vao->allocated = FALSE;
        }
    }
}

void _glsBindVertexArray(unsigned int array)
{
    GLS_State *s = glsGetState();
    GLS_VAO *vao;
    s->boundVAO = array;
    vao = glsFindVAO(array);
    if (vao) s->boundElementBuffer = vao->elementBuffer;
}

unsigned char _glsIsVertexArray(unsigned int array)
{
    GLS_VAO *v;
    if (array == 0) return GL_FALSE;
    v = glsFindVAO(array);
    return (v && v->allocated) ? GL_TRUE : GL_FALSE;
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
            if (s->boundReadFBO == framebuffers[i]) s->boundReadFBO = 0;
            if (s->boundDrawFBO == framebuffers[i]) s->boundDrawFBO = 0;
            fbo->allocated = FALSE;
        }
    }
}

void _glsBindFramebuffer(unsigned int target, unsigned int framebuffer)
{
    GLS_State *s = glsGetState();

    /* GL_FRAMEBUFFER means both; the read and draw targets are otherwise
     * independent bindings. */
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        s->boundReadFBO = framebuffer;
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        s->boundDrawFBO = framebuffer;
}

/*
 * glCheckFramebufferStatus — answered from the recorded attachments.
 *
 * The default framebuffer (name 0) is complete by definition.  For a
 * user framebuffer the two conditions this wrapper can actually check are
 * "has at least one attachment" and "all attachments are the same size";
 * anything it cannot determine is reported complete rather than invented,
 * because a false incomplete would stop an application dead.
 */
unsigned int _glsCheckFramebufferStatus(unsigned int target)
{
    GLS_State *s = glsGetState();
    GLuint_t name = (target == GL_READ_FRAMEBUFFER) ? s->boundReadFBO : s->boundDrawFBO;
    GLS_FBO *fbo;
    int i, count = 0;
    int w = -1, h = -1;

    if (name == 0)
        return GL_FRAMEBUFFER_COMPLETE;

    fbo = glsFindFBO(name);
    if (!fbo) {
        gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> framebuffer %u does not exist",
                   target, name);
        return GL_FRAMEBUFFER_UNDEFINED;
    }

    for (i = 0; i < 4; i++) {
        int aw = -1, ah = -1;
        if (fbo->colorAttachment[i]) {
            GLS_Texture *tex = glsFindTexture(fbo->colorAttachment[i]);
            if (!tex) {
                gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> colour attachment %d "
                           "names texture %u, which does not exist",
                           target, i, fbo->colorAttachment[i]);
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
            aw = tex->width; ah = tex->height;
        } else if (fbo->colorAttachRB[i]) {
            GLS_RBO *rbo = glsFindRBO(fbo->colorAttachRB[i]);
            if (!rbo) {
                gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> colour attachment %d "
                           "names renderbuffer %u, which does not exist",
                           target, i, fbo->colorAttachRB[i]);
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
            aw = rbo->width; ah = rbo->height;
        } else {
            continue;
        }
        count++;
        if (w < 0) { w = aw; h = ah; }
        else if (aw != w || ah != h) {
            gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> attachment sizes differ "
                       "(%dx%d vs %dx%d)", target, w, h, aw, ah);
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if (fbo->depthAttachRB || fbo->depthStencilAttachRB || fbo->stencilAttachRB) {
        GLuint_t rbName = fbo->depthAttachRB ? fbo->depthAttachRB
                        : (fbo->depthStencilAttachRB ? fbo->depthStencilAttachRB
                                                     : fbo->stencilAttachRB);
        GLS_RBO *rbo = glsFindRBO(rbName);
        if (!rbo) {
            gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> depth/stencil renderbuffer %u "
                       "does not exist", target, rbName);
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        count++;
        if (w < 0) { w = rbo->width; h = rbo->height; }
        else if (rbo->width != w || rbo->height != h) {
            gldDiagLogV("GL: CheckFramebufferStatus(0x%X) -> depth/stencil size %dx%d "
                       "differs from colour %dx%d",
                       target, rbo->width, rbo->height, w, h);
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    } else if (fbo->depthAttachment || fbo->depthStencilAttachment || fbo->stencilAttachment) {
        count++;
    }

    if (count == 0) {
        gldDiagLog("GL: CheckFramebufferStatus(0x%X) -> framebuffer %u has no attachments",
                   target, name);
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

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
        if (!q) continue;
        if (q->pQuery) {
            IDirect3DQuery9_Release(q->pQuery);
            q->pQuery = NULL;
        }
        q->resultReady = FALSE;
        q->active      = FALSE;
        q->allocated   = FALSE;
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
            s->samplers[id].minFilter = GL_NEAREST_MIPMAP_LINEAR;
            s->samplers[id].magFilter = GL_LINEAR;
            s->samplers[id].wrapS = GL_REPEAT;
            s->samplers[id].wrapT = GL_REPEAT;
            s->samplers[id].wrapR = GL_REPEAT;
            s->samplers[id].minLod = -1000.0f;
            s->samplers[id].maxLod = 1000.0f;
            s->samplers[id].compareMode = GL_NONE;
            s->samplers[id].compareFunc = GL_LEQUAL;
            s->samplers[id].maxAnisotropy = 1.0f;
            s->samplers[id].allocated = TRUE;
        }
        samplers[i] = id;
    }
}

void _glsDeleteSamplers(int count, const unsigned int *samplers)
{
    GLS_State *s = glsGetState();
    int i, unit;
    if (!samplers || count <= 0) return;
    for (i = 0; i < count; i++) {
        GLS_Sampler *samp = glsFindSampler(samplers[i]);
        if (samp) {
            for (unit = 0; unit < GLS_MAX_TEX_UNITS; ++unit)
                if (s->boundSampler[unit] == samplers[i])
                    s->boundSampler[unit] = 0;
            samp->allocated = FALSE;
        }
    }
}


/* ===================================================================
 *  SECTION 2: Texture Functions
 * =================================================================== */

/* Cube map face targets are consecutive from GL_TEXTURE_CUBE_MAP_POSITIVE_X
 * and appear in the same order as D3DCUBEMAP_FACES, so the face index is a
 * plain subtraction.  Returns -1 for non-cube targets. */
#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_X
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X  0x8515
#endif
#ifndef GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  0x851A
#endif

static int _glsCubeFaceFromTarget(unsigned int target)
{
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
        target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        return (int)(target - GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    return -1;
}

/* Number of mip levels in a full chain down to 1x1. */
static int _glsMipLevelCount(int width, int height)
{
    int levels = 1;
    while (width > 1 || height > 1) {
        if (width  > 1) width  >>= 1;
        if (height > 1) height >>= 1;
        levels++;
    }
    return levels;
}

/* Look up the texture object bound to the active unit for `target`. */
static GLS_Texture *_glsBoundTextureForTarget(unsigned int target, int *outUnit)
{
    GLS_State *s = glsGetState();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (int)(s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;

    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    if (outUnit) *outUnit = unit;

    if (_glsCubeFaceFromTarget(target) >= 0 || target == GL_TEXTURE_CUBE_MAP)
        texId = s->boundTextureCube[unit];
    else if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        texId = s->boundTexture3D[unit];
    else if (target == GL_TEXTURE_BUFFER)
        texId = s->boundTextureBuffer[unit];
    else
        texId = s->boundTexture2D[unit];

    return glsFindTexture(texId);
}

/* Drop whatever D3D9 resources a texture object currently holds. */
static void _glsReleaseTextureResources(GLS_Texture *tex)
{
    if (tex->pTex)     { IDirect3DTexture9_Release(tex->pTex);         tex->pTex = NULL; }
    if (tex->pCubeTex) { IDirect3DCubeTexture9_Release(tex->pCubeTex); tex->pCubeTex = NULL; }
    if (tex->pVolTex)  { IDirect3DVolumeTexture9_Release(tex->pVolTex); tex->pVolTex = NULL; }
    if (tex->pixelData) { free(tex->pixelData); tex->pixelData = NULL; tex->pixelDataSize = 0; }
}

/*
 * Lock one mip level of a 2D or cube texture.
 *
 * `pRect` may be NULL to lock the whole level.  Returns FALSE if the level
 * does not exist, which is the normal outcome when an application uploads
 * more mip levels than the allocated chain holds.
 */
static BOOL _glsLockTexLevel(GLS_Texture *tex, unsigned int target, int level,
                             const RECT *pRect, D3DLOCKED_RECT *out)
{
    int face = _glsCubeFaceFromTarget(target);
    HRESULT hr = E_FAIL;

    __try {
        if (face >= 0 && tex->pCubeTex)
            hr = IDirect3DCubeTexture9_LockRect(tex->pCubeTex, (D3DCUBEMAP_FACES)face,
                                                level, out, pRect, 0);
        else if (tex->pTex)
            hr = IDirect3DTexture9_LockRect(tex->pTex, level, out, pRect, 0);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    return SUCCEEDED(hr);
}

static void _glsUnlockTexLevel(GLS_Texture *tex, unsigned int target, int level)
{
    int face = _glsCubeFaceFromTarget(target);

    __try {
        if (face >= 0 && tex->pCubeTex)
            IDirect3DCubeTexture9_UnlockRect(tex->pCubeTex, (D3DCUBEMAP_FACES)face, level);
        else if (tex->pTex)
            IDirect3DTexture9_UnlockRect(tex->pTex, level);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/*
 * glTexImage2D — define one mip level of a 2D or cube map texture.
 *
 * GL defines levels one call at a time and gives no advance notice of how
 * many are coming, so the storage is allocated on the level 0 call as a full
 * mip chain.  Levels the application never supplies simply stay empty, and
 * are only sampled if it also asks for a mipmapping min filter.
 *
 * A level > 0 arriving for an already-allocated texture writes into the
 * existing chain rather than being discarded, which is what makes
 * pre-generated mipmaps work.
 */
void _glsTexImage2D(unsigned int target, int level, int internalformat,
                     int width, int height, int border,
                     unsigned int format, unsigned int type, const void *pixels)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    D3DLOCKED_RECT lr;
    D3DSURFACE_DESC desc;
    int face, unit;
    HRESULT hr;

    (void)border;

    /* Logged on entry, not just on success: everything else here records
     * only after the work is done, so a call that faults leaves no trace of
     * itself and the crash appears to happen "after" the previous call. */
    gldDiagLogV("GL: -> TexImage2D target=0x%X level=%d %dx%d int=0x%X fmt=0x%X type=0x%X px=%p",
               target, level, width, height, internalformat, format, type, pixels);

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;
    if (!pDev || width <= 0 || height <= 0) return;

    face   = _glsCubeFaceFromTarget(target);
    d3dFmt = _glsMapGLFormatToD3D(internalformat);

    if (level == 0) {
        int levels;

        /* A cube map is defined face by face; only allocate on the first
         * face, otherwise each face would destroy the previous one. */
        if (face > 0 && tex->pCubeTex) {
            /* storage already exists — fall through to the upload below */
        } else {
            _glsReleaseTextureResources(tex);

            tex->width          = width;
            tex->height         = height;
            tex->internalFormat = internalformat;
            tex->target         = target;

            levels = _glsMipLevelCount(width, height);

            /* Never hand CreateTexture a format the device has not already
             * confirmed it can create — see _glsResolveTextureFormat. */
            d3dFmt = _glsResolveTextureFormat(d3dFmt, face >= 0);
            if (d3dFmt == D3DFMT_UNKNOWN) {
                gldDiagLog("GL: TexImage2D no creatable format for int=0x%X (tex=%u) %dx%d - texture left without data",
                           internalformat, tex->id, width, height);
                tex->pTex = NULL;
                tex->pCubeTex = NULL;
                return;
            }

            __try {
                if (face >= 0)
                    hr = IDirect3DDevice9_CreateCubeTexture(pDev, width, levels, 0,
                                                            d3dFmt, D3DPOOL_MANAGED,
                                                            &tex->pCubeTex, NULL);
                else
                    hr = IDirect3DDevice9_CreateTexture(pDev, width, height, levels, 0,
                                                        d3dFmt, D3DPOOL_MANAGED,
                                                        &tex->pTex, NULL);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                hr = E_FAIL;
            }

            if (FAILED(hr)) {
                gldDiagLog("GL: TexImage2D Create%sTexture FAILED hr=0x%08X %dx%d fmt=%d",
                           face >= 0 ? "Cube" : "", hr, width, height, d3dFmt);
                tex->pTex = NULL;
                tex->pCubeTex = NULL;
                return;
            }
        }
    }

    if (!tex->pTex && !tex->pCubeTex) {
        /* Level > 0 arrived before level 0 — nothing to write into. */
        gldDiagLog("GL: TexImage2D level %d with no storage allocated", level);
        return;
    }

    /* Ask the surface what it really is rather than trusting the local: the
     * storage may have been created with a substituted format, and on a
     * cube face > 0 or a level > 0 upload nothing resolved d3dFmt on this
     * call at all — it still holds the raw mapper output.  Same lookup
     * _glsTexSubImage2D performs for the same reason. */
    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: TexImage2D GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, hr);
        return;
    }

    if (pixels) {
        /* Copy no more rows than the level actually has.
         *
         * The size asked for and the size created are not always the same:
         * CreateTexture is free to adjust for power-of-two or square
         * requirements and to clamp to the device's maximum, and the caps
         * differ from one d3d9.dll to the next. desc holds what was really
         * created, and up to now only its format was consulted while the copy
         * was still driven by the requested dimensions.
         *
         * Getting that wrong writes height*Pitch bytes into a buffer holding
         * desc.Height*Pitch. Rows past the end land outside the allocation,
         * and for a MANAGED or SYSTEMMEM texture that is process heap - so the
         * damage surfaces much later as a fault inside ntdll walking a heap
         * block whose links were overwritten, with nothing to connect it back
         * to the texture upload that caused it.
         *
         * Width is left alone here; _glsCopyPixelsToD3D already clamps each
         * row against the real pitch. */
        int copyH = height;
        if ((UINT)copyH > desc.Height) {
            gldDiagLog("GL: TexImage2D level %d created %ux%u but %dx%d was requested "
                       "- copying %u rows to stay inside the allocation (tex=%u)",
                       level, desc.Width, desc.Height, width, height, desc.Height, tex->id);
            copyH = (int)desc.Height;
        }

        if (_glsLockTexLevel(tex, target, level, NULL, &lr)) {
            _glsCopyPixelsToD3D(lr.pBits, pixels, width, copyH, format, type, lr.Pitch, desc.Format);
            _glsUnlockTexLevel(tex, target, level);
        } else {
            gldDiagLog("GL: TexImage2D lock failed level=%d", level);
        }
    }

    gldDiagLogV("GL: TexImage2D tex=%u target=0x%X level=%d %dx%d fmt=0x%X d3d=%d",
               tex->id, target, level, width, height, internalformat, desc.Format);
}

/*
 * glTexSubImage2D — replace a rectangle of an existing mip level.
 *
 * Locking only the destination rectangle lets the source rows be copied
 * straight in: within a locked sub-rect, pBits points at the sub-rect's
 * first texel, so the existing row conversion works unchanged.
 */
void _glsTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset,
                        int width, int height, unsigned int format, unsigned int type,
                        const void *pixels)
{
    GLS_Texture *tex;
    D3DLOCKED_RECT lr;
    D3DSURFACE_DESC desc;
    RECT rect;
    int unit;
    HRESULT hr;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || !pixels || level < 0) return;
    if (width <= 0 || height <= 0) return;
    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: TexSubImage2D with no storage (tex=%u)", tex->id);
        return;
    }

    /* Ask the surface itself rather than re-deriving from internalFormat: the
     * created format is what the copy has to write, and D3D may have given us
     * something other than what was requested. */
    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: TexSubImage2D GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, hr);
        return;
    }

    /* The sub-rectangle has to be inside the level before it is locked.
     *
     * GL calls this GL_INVALID_VALUE and ignores the call, but D3D9 does not
     * check: LockRect with a rectangle larger than the level can still succeed,
     * hand back a pointer into a smaller allocation, and leave the copy below
     * writing rows past the end of it. For a MANAGED or SYSTEMMEM texture that
     * allocation is ordinary process heap, so the overrun silently smashes heap
     * metadata and the process dies much later inside ntdll, walking a block
     * whose links have been overwritten - nowhere near the code at fault.
     *
     * Mip levels are where this bites: the tail of a chain is 2x2 and 1x1, and
     * a caller that keeps halving its own idea of the size while D3D has
     * already clamped to 1 will ask to write outside the level. */
    if (xoffset < 0 || yoffset < 0 ||
        (UINT)(xoffset + width)  > desc.Width ||
        (UINT)(yoffset + height) > desc.Height) {
        gldDiagLog("GL: TexSubImage2D rejected out-of-range rect tex=%u level=%d "
                   "(%d,%d %dx%d) into %ux%u - would overrun the level",
                   tex->id, level, xoffset, yoffset, width, height,
                   desc.Width, desc.Height);
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }

    rect.left   = xoffset;
    rect.top    = yoffset;
    rect.right  = xoffset + width;
    rect.bottom = yoffset + height;

    if (!_glsLockTexLevel(tex, target, level, &rect, &lr)) {
        gldDiagLog("GL: TexSubImage2D lock failed tex=%u level=%d", tex->id, level);
        return;
    }

    _glsCopyPixelsToD3D(lr.pBits, pixels, width, height, format, type, lr.Pitch, desc.Format);
    _glsUnlockTexLevel(tex, target, level);

    gldDiagLogV("GL: TexSubImage2D tex=%u level=%d (%d,%d) %dx%d",
               tex->id, level, xoffset, yoffset, width, height);
}

void _glsTexParameteri(unsigned int target, unsigned int pname, int param)
{
    GLS_DLStateArgs args;
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (s->activeTexUnit - GL_TEXTURE0) : 0;
    unsigned int texId;
    GLS_Texture *tex;

    memset(&args, 0, sizeof(args));
    args.u[0] = target;
    args.u[1] = pname;
    args.i[0] = param;
    if (_glsDLRecord(_glsDLTexParameteri, &args, sizeof(args))) return;

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

/*
 * glCompressedTexImage2D — upload pre-compressed S3TC/DXT data.
 *
 * The block layout is identical between GL and D3D9, so the data is copied
 * verbatim; only the row pitch has to be honoured, and a "row" here is a row
 * of 4x4 blocks rather than of texels.  Levels beyond 0 write into the chain
 * allocated by the level 0 call, which is how compressed mipmaps in DDS-style
 * assets get through.
 */
void _glsCompressedTexImage2D(unsigned int target, int level, unsigned int internalformat,
                               int width, int height, int border, int imageSize, const void *data)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    D3DLOCKED_RECT lr;
    int face, unit;
    HRESULT hr;

    (void)border;

    gldDiagLogV("GL: -> CompressedTexImage2D target=0x%X level=%d %dx%d int=0x%X size=%d data=%p",
               target, level, width, height, internalformat, imageSize, data);

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;
    if (!pDev || width <= 0 || height <= 0) return;

    face   = _glsCubeFaceFromTarget(target);
    d3dFmt = _glsMapCompressedFormatToD3D(internalformat);
    if (d3dFmt == D3DFMT_UNKNOWN) {
        gldDiagLogV("GL: CompressedTexImage2D unknown format 0x%X", internalformat);
        return;
    }

    if (level == 0 && !(face > 0 && tex->pCubeTex)) {
        int levels = _glsMipLevelCount(width, height);

        _glsReleaseTextureResources(tex);

        tex->width          = width;
        tex->height         = height;
        tex->internalFormat = internalformat;
        tex->target         = target;

        /* Supported-or-skip, never substitute: the bytes handed to this
         * function are DXT block-compressed and cannot be reinterpreted as
         * raw ARGB pixels without a decompressor this wrapper does not
         * have.  The texture is left with no D3D resource, which every
         * later call for it already guards against, and the surfaces using
         * it come out untextured instead of crashing or scrambled. */
        if (!gldIsTextureFormatSupported46(d3dFmt, face >= 0)) {
            gldDiagLog("GL: CompressedTexImage2D D3DFMT=%d (tex=%u) unsupported by device; no software decompressor - texture left without data",
                       (int)d3dFmt, tex->id);
            tex->pTex = NULL;
            tex->pCubeTex = NULL;
            return;
        }

        __try {
            if (face >= 0)
                hr = IDirect3DDevice9_CreateCubeTexture(pDev, width, levels, 0,
                                                        d3dFmt, D3DPOOL_MANAGED,
                                                        &tex->pCubeTex, NULL);
            else
                hr = IDirect3DDevice9_CreateTexture(pDev, width, height, levels, 0,
                                                    d3dFmt, D3DPOOL_MANAGED,
                                                    &tex->pTex, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }

        if (FAILED(hr)) {
            gldDiagLog("GL: CompressedTexImage2D Create FAILED hr=0x%08X %dx%d fmt=%d",
                       hr, width, height, d3dFmt);
            tex->pTex = NULL;
            tex->pCubeTex = NULL;
            return;
        }
    }

    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: CompressedTexImage2D level %d with no storage", level);
        return;
    }

    if (data && imageSize > 0) {
        if (_glsLockTexLevel(tex, target, level, NULL, &lr)) {
            int blockWidth  = (width  + 3) / 4;
            int blockHeight = (height + 3) / 4;
            int blockSize   = (d3dFmt == D3DFMT_DXT1) ? 8 : 16;
            int srcRowBytes = blockWidth * blockSize;
            int row;

            if (lr.Pitch == srcRowBytes) {
                memcpy(lr.pBits, data, (size_t)imageSize);
            } else {
                for (row = 0; row < blockHeight; row++) {
                    int copySize = srcRowBytes;
                    if (copySize > lr.Pitch) copySize = lr.Pitch;
                    memcpy((unsigned char *)lr.pBits + (ptrdiff_t)row * lr.Pitch,
                           (const unsigned char *)data + (ptrdiff_t)row * srcRowBytes,
                           (size_t)copySize);
                }
            }

            _glsUnlockTexLevel(tex, target, level);
        } else {
            gldDiagLog("GL: CompressedTexImage2D lock failed level=%d", level);
        }
    }

    gldDiagLogV("GL: CompressedTexImage2D tex=%u target=0x%X level=%d %dx%d fmt=0x%X size=%d",
               tex->id, target, level, width, height, internalformat, imageSize);
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
    else if (target == 0x88EB) /* GL_PIXEL_PACK_BUFFER */
        id = s->boundPixelPackBuffer;
    else if (target == GL_TEXTURE_BUFFER)
        id = s->boundTextureBufferObject;
    else if (target == 0x88EC) /* GL_PIXEL_UNPACK_BUFFER */
        id = s->boundPixelUnpackBuffer;
    else if (target == 0x8F36) /* GL_COPY_READ_BUFFER */
        id = s->boundCopyReadBuffer;
    else if (target == 0x8F37) /* GL_COPY_WRITE_BUFFER */
        id = s->boundCopyWriteBuffer;
    else if (target == 0x8A11) /* GL_UNIFORM_BUFFER */
        id = s->boundUniformBuffer;
    else if (target == 0x8C8E) /* GL_TRANSFORM_FEEDBACK_BUFFER */
        id = s->boundTransformFeedbackBuffer;
    else if (target == 0x90D2) /* GL_SHADER_STORAGE_BUFFER */
        id = s->boundShaderStorageBuffer;
    else if (target == 0x92C0) /* GL_ATOMIC_COUNTER_BUFFER */
        id = s->boundAtomicCounterBuffer;
    else if (target == 0x8F3F) /* GL_DRAW_INDIRECT_BUFFER */
        id = s->boundDrawIndirectBuffer;
    else if (target == 0x90EE) /* GL_DISPATCH_INDIRECT_BUFFER */
        id = s->boundDispatchIndirectBuffer;
    else if (target == 0x80EE) /* GL_PARAMETER_BUFFER_ARB */
        id = s->boundParameterBuffer;
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

BOOL _glsWriteBufferObject(GLS_Buffer *buf, ptrdiff_t offset,
                           ptrdiff_t size, const void *data)
{
    void *dst = NULL;
    HRESULT hr;
    if (!buf || !buf->data || !data || offset < 0 || size < 0 ||
        offset > buf->size || size > buf->size - offset) return FALSE;
    memcpy((unsigned char *)buf->data + offset, data, (size_t)size);

    if (buf->pVB) {
        hr = IDirect3DVertexBuffer9_Lock(buf->pVB, (UINT)offset, (UINT)size,
                                         &dst, 0);
        if (SUCCEEDED(hr)) {
            memcpy(dst, data, (size_t)size);
            IDirect3DVertexBuffer9_Unlock(buf->pVB);
        }
    }
    if (buf->pIB) {
        dst = NULL;
        hr = IDirect3DIndexBuffer9_Lock(buf->pIB, (UINT)offset, (UINT)size,
                                        &dst, 0);
        if (SUCCEEDED(hr)) {
            memcpy(dst, data, (size_t)size);
            IDirect3DIndexBuffer9_Unlock(buf->pIB);
        }
    }
    return TRUE;
}

void *_glsMapBuffer(unsigned int target, unsigned int access)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    unsigned int flags;
    if (!buf) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    if (buf->mapped) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    if (access == 0x88B8) flags = 0x0001;      /* GL_READ_ONLY / GL_MAP_READ_BIT */
    else if (access == 0x88B9) flags = 0x0002; /* GL_WRITE_ONLY / GL_MAP_WRITE_BIT */
    else if (access == 0x88BA) flags = 0x0003; /* GL_READ_WRITE */
    else {
        glsGetState()->lastError = GL_INVALID_ENUM;
        return NULL;
    }
    if (!buf->data || buf->size <= 0) return NULL;
    buf->mapped = TRUE;
    buf->mapOffset = 0;
    buf->mapLength = buf->size;
    buf->mapAccess = flags;
    return buf->data;
}

unsigned char _glsUnmapBuffer(unsigned int target)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf || !buf->mapped) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return 0;
    }
    buf->mapped = FALSE;
    buf->mapOffset = 0;
    buf->mapLength = 0;
    buf->mapAccess = 0;
    return 1; /* GL_TRUE */
}

/* glIsBuffer — a name is a buffer once glBindBuffer or glGenBuffers made it one. */
unsigned char _glsIsBuffer(unsigned int buffer)
{
    GLS_Buffer *buf = glsFindBuffer(buffer);
    return (buf && buf->allocated) ? GL_TRUE : GL_FALSE;
}

/* glGetBufferParameteriv — answered from the tracked buffer object. */
void _glsGetBufferParameteriv(unsigned int target, unsigned int pname, int *params)
{
    GLS_Buffer *buf = _getBoundBuffer(target);

    if (!params) return;
    *params = 0;
    if (!buf) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }

    switch (pname) {
    case 0x8764: /* GL_BUFFER_SIZE   */ *params = (int)buf->size; break;
    case 0x8765: /* GL_BUFFER_USAGE  */ *params = (int)buf->usage; break;
    case 0x88BB: /* GL_BUFFER_ACCESS */ *params = 0x88BA; /* GL_READ_WRITE */ break;
    case 0x88BC: /* GL_BUFFER_MAPPED */ *params = buf->mapped ? GL_TRUE : GL_FALSE; break;
    default:                            *params = 0; break;
    }
}

void _glsGetBufferPointerv(unsigned int target, unsigned int pname, void **params)
{
    GLS_Buffer *buf = _getBoundBuffer(target);

    if (!params) return;
    *params = NULL;

    if (!buf) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }

    /* GL_BUFFER_MAP_POINTER: defined to be NULL unless the buffer is
     * currently mapped, in which case it is the pointer glMapBuffer returned. */
    if (pname == 0x88BD && buf->mapped)
        *params = buf->data;
}


/* ===================================================================
 *  SECTION 4: State Functions
 * =================================================================== */

/* Defined with the rest of the ARB program code, needed here and by
 * glDeleteShader / glUseProgram, which sit ahead of that section. */
static void _glsApplyARBProgramBinding(unsigned int target);
static void _glsReleaseARBProgram(GLS_Shader *sh);

static BOOL* _getEnableFlag(GLS_State *s, unsigned int cap)
{
    int unit;
    switch (cap) {
    case 0x8910: /* GL_STENCIL_TEST_TWO_SIDE_EXT */
        return &s->enableStencilTestTwoSide;
    case GL_VERTEX_PROGRAM_ARB:   return &s->enableVertexProgramARB;
    case GL_FRAGMENT_PROGRAM_ARB: return &s->enableFragmentProgramARB;
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_Q:
        unit = (s->activeTexUnit >= GL_TEXTURE0) ? (int)(s->activeTexUnit - GL_TEXTURE0) : 0;
        if (unit >= 0 && unit < GLS_MAX_TEX_UNITS)
            return &s->texGenEnabled[unit][cap - GL_TEXTURE_GEN_S];
        return NULL;
    case GL_DEPTH_TEST:          return &s->enableDepthTest;
    case GL_BLEND:               return &s->enableBlend;
    case GL_CULL_FACE:           return &s->enableCullFace;
    case GL_SCISSOR_TEST:        return &s->enableScissorTest;
    case GL_STENCIL_TEST:        return &s->enableStencilTest;
    case GL_RASTERIZER_DISCARD:  return &s->enableRasterizerDiscard;
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
    GLS_DLUInt2 a = {{ cap, 0 }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    BOOL *flag = _getEnableFlag(s, cap);
    if (_glsDLRecord(_glsDLEnable, &a, sizeof(a))) return;
    if (flag) *flag = TRUE;
    gldDiagLogV("GL: glEnable(0x%X)", cap);

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
                /* Enabling the test alone is not enough — the func/op state
                 * accumulated while it was off has never been pushed. */
                _glsApplyStencilState();
                break;
            case GL_ALPHA_TEST:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHATESTENABLE, TRUE);
                break;
            case GL_LIGHTING:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);
                break;
            case GL_FOG:
                _glsApplyFogState();
                break;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    /* Binding an ARB program while its target was disabled never reached the
     * device; enabling the target is what puts it there. */
    if (cap == GL_VERTEX_PROGRAM_ARB || cap == GL_FRAGMENT_PROGRAM_ARB)
        _glsApplyARBProgramBinding(cap);

    /* Same reasoning for texgen: the mode set while it was off never went up. */
    if (cap >= GL_TEXTURE_GEN_S && cap <= GL_TEXTURE_GEN_Q)
        _glsApplyTexGenState((s->activeTexUnit >= GL_TEXTURE0)
                             ? (int)(s->activeTexUnit - GL_TEXTURE0) : 0);
}

void _glsDisable(unsigned int cap)
{
    GLS_DLUInt2 a = {{ cap, 0 }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    BOOL *flag = _getEnableFlag(s, cap);
    if (_glsDLRecord(_glsDLDisable, &a, sizeof(a))) return;
    if (flag) *flag = FALSE;
    gldDiagLogV("GL: glDisable(0x%X)", cap);

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
            case GL_FOG:
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGENABLE, FALSE);
                break;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    /* Returns the stage to fixed function (or to the GLSL program in use). */
    if (cap == GL_VERTEX_PROGRAM_ARB || cap == GL_FRAGMENT_PROGRAM_ARB)
        _glsApplyARBProgramBinding(cap);

    if (cap >= GL_TEXTURE_GEN_S && cap <= GL_TEXTURE_GEN_Q)
        _glsApplyTexGenState((s->activeTexUnit >= GL_TEXTURE0)
                             ? (int)(s->activeTexUnit - GL_TEXTURE0) : 0);
}

void _glsBlendFunc(unsigned int sfactor, unsigned int dfactor)
{
    GLS_DLUInt2 a = {{ sfactor, dfactor }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (_glsDLRecord(_glsDLBlendFunc, &a, sizeof(a))) return;
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
    GLS_DLUInt2 a = {{ func, 0 }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (_glsDLRecord(_glsDLDepthFunc, &a, sizeof(a))) return;
    s->depthFunc = func;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZFUNC, _glsMapCompareFunc(func));
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsDepthMask(unsigned char flag)
{
    GLS_DLUInt2 a = {{ flag, 0 }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (_glsDLRecord(_glsDLDepthMask, &a, sizeof(a))) return;
    s->depthMask = flag;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZWRITEENABLE, flag ? TRUE : FALSE);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

void _glsCullFace(unsigned int mode)
{
    GLS_DLUInt2 a = {{ mode, 0 }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLCullFace, &a, sizeof(a))) return;
    s->cullFaceMode = mode;
    _glsApplyD3DCullMode();
}

void _glsFrontFace(unsigned int mode)
{
    GLS_DLUInt2 a = {{ mode, 0 }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLFrontFace, &a, sizeof(a))) return;
    s->frontFace = mode;
    _glsApplyD3DCullMode();
}

void _glsColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GLS_DLUInt2 args = {{ (unsigned int)r | ((unsigned int)g << 8) |
                          ((unsigned int)b << 16) | ((unsigned int)a << 24), 0 }};
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD mask = 0;
    if (_glsDLRecord(_glsDLColorMask, &args, sizeof(args))) return;

    s->colorMask[0] = r;
    s->colorMask[1] = g;
    s->colorMask[2] = b;
    s->colorMask[3] = a;

    if (r) mask |= D3DCOLORWRITEENABLE_RED;
    if (g) mask |= D3DCOLORWRITEENABLE_GREEN;
    if (b) mask |= D3DCOLORWRITEENABLE_BLUE;
    if (a) mask |= D3DCOLORWRITEENABLE_ALPHA;

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE, mask);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
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

/* GL stencil operation -> D3DSTENCILOP.
 *
 * GL_INCR/GL_DECR saturate at the maximum/minimum representable value, so
 * they map to the *SAT variants; the WRAP forms are the ones that map to
 * D3D's plain INCR/DECR. */
static D3DSTENCILOP _glsMapStencilOp(unsigned int op)
{
    switch (op) {
    case 0x1E00: return D3DSTENCILOP_KEEP;      /* GL_KEEP */
    case 0x0000: return D3DSTENCILOP_ZERO;      /* GL_ZERO */
    case 0x1E01: return D3DSTENCILOP_REPLACE;   /* GL_REPLACE */
    case 0x1E02: return D3DSTENCILOP_INCRSAT;   /* GL_INCR */
    case 0x1E03: return D3DSTENCILOP_DECRSAT;   /* GL_DECR */
    case 0x150A: return D3DSTENCILOP_INVERT;    /* GL_INVERT */
    case 0x8507: return D3DSTENCILOP_INCR;      /* GL_INCR_WRAP */
    case 0x8508: return D3DSTENCILOP_DECR;      /* GL_DECR_WRAP */
    default:     return D3DSTENCILOP_KEEP;
    }
}

/*
 * Push the whole stencil state to D3D9.
 *
 * Called from every stencil setter rather than each one writing its own
 * render states, because two-sided stencil has to be enabled or disabled
 * based on whether the front and back state actually differ — a decision
 * that needs all of it visible at once.
 */
void _glsApplyStencilState(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    BOOL twoSided;

    if (!pDev) return;

    twoSided = (s->stencilBackFunc  != s->stencilFunc)  ||
               (s->stencilBackRef   != s->stencilRef)   ||
               (s->stencilBackMask  != s->stencilMask)  ||
               (s->stencilBackFail  != s->stencilFail)  ||
               (s->stencilBackZFail != s->stencilZFail) ||
               (s->stencilBackZPass != s->stencilZPass);

    __try {
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILFUNC, _glsMapCompareFunc(s->stencilFunc));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILREF,  (DWORD)s->stencilRef);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILMASK, (DWORD)s->stencilMask);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILWRITEMASK, (DWORD)s->stencilWriteMask);

        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILFAIL,  _glsMapStencilOp(s->stencilFail));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILZFAIL, _glsMapStencilOp(s->stencilZFail));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILPASS,  _glsMapStencilOp(s->stencilZPass));

        IDirect3DDevice9_SetRenderState(pDev, D3DRS_TWOSIDEDSTENCILMODE, twoSided ? TRUE : FALSE);
        if (twoSided) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CCW_STENCILFUNC,
                                            _glsMapCompareFunc(s->stencilBackFunc));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CCW_STENCILFAIL,
                                            _glsMapStencilOp(s->stencilBackFail));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CCW_STENCILZFAIL,
                                            _glsMapStencilOp(s->stencilBackZFail));
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_CCW_STENCILPASS,
                                            _glsMapStencilOp(s->stencilBackZPass));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

void _glsStencilFunc(unsigned int func, int ref, unsigned int mask)
{
    GLS_State *s = glsGetState();

    /* glStencilFunc sets both faces unless EXT_stencil_two_side is enabled and
     * glActiveStencilFaceEXT has singled one out.  The recorded active face is
     * not itself altered here — it stays queryable. */
    GLenum_t face = s->enableStencilTestTwoSide ? s->activeStencilFace : GL_FRONT_AND_BACK;

    if (face != GL_BACK) {
        s->stencilFunc = func;
        s->stencilRef = ref;
        s->stencilMask = mask;
    }
    if (face != GL_FRONT) {
        s->stencilBackFunc = func;
        s->stencilBackRef = ref;
        s->stencilBackMask = mask;
    }

    _glsApplyStencilState();
}

void _glsStencilOp(unsigned int fail, unsigned int zfail, unsigned int zpass)
{
    GLS_State *s = glsGetState();

    GLenum_t face = s->enableStencilTestTwoSide ? s->activeStencilFace : GL_FRONT_AND_BACK;

    if (face != GL_BACK) {
        s->stencilFail = fail;
        s->stencilZFail = zfail;
        s->stencilZPass = zpass;
    }
    if (face != GL_FRONT) {
        s->stencilBackFail = fail;
        s->stencilBackZFail = zfail;
        s->stencilBackZPass = zpass;
    }

    _glsApplyStencilState();
}

void _glsPolygonMode(unsigned int face, unsigned int mode)
{
    GLS_DLUInt2 dlArgs = {{ face, mode }};
    if (_glsDLRecord(_glsDLPolygonMode, &dlArgs, sizeof(dlArgs))) return;
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD fill;

    if (face == 0x0404 || face == 0x0408) /* GL_FRONT or GL_FRONT_AND_BACK */
        s->polygonModeFront = mode;
    if (face == 0x0405 || face == 0x0408) /* GL_BACK or GL_FRONT_AND_BACK */
        s->polygonModeBack = mode;

    /* D3D9 has a single fill mode, with no front/back distinction; the front
     * setting wins because that is what an application changing only one face
     * almost always means to affect. */
    switch (s->polygonModeFront) {
    case 0x1B00: fill = D3DFILL_POINT;     break;  /* GL_POINT */
    case 0x1B01: fill = D3DFILL_WIREFRAME; break;  /* GL_LINE */
    default:     fill = D3DFILL_SOLID;     break;  /* GL_FILL */
    }

    if (pDev) {
        __try {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_FILLMODE, fill);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
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
    HGLRC hGLRC;
    GLD_ctx *ctx;
    int renderTargetHeight;

    s->scissorX = x;
    s->scissorY = y;
    s->scissorW = width;
    s->scissorH = height;

    if (pDev) {
        RECT rc;

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

        // Convert OpenGL scissor (bottom-left origin) to D3D9 scissor (top-left origin)
        rc.left = x;
        rc.top = renderTargetHeight - (y + height);
        rc.right = x + width;
        rc.bottom = renderTargetHeight - y;
        __try {
            IDirect3DDevice9_SetScissorRect(pDev, &rc);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

static BOOL _glsBuildClippedViewport(IDirect3DDevice9 *pDev, GLS_State *s,
                                     D3DVIEWPORT9 *vp, float adjust[4])
{
    IDirect3DSurface9 *surface = NULL;
    D3DSURFACE_DESC desc;
    GLD_ctx *ctx = NULL;
    HGLRC hGLRC;
    int rtWidth = 0, rtHeight = 0;
    int64_t desiredLeft, desiredTop, desiredRight, desiredBottom;
    int64_t clippedLeft, clippedTop, clippedRight, clippedBottom;

    if (!pDev || !s || !vp || s->viewportW <= 0 || s->viewportH <= 0)
        return FALSE;
    if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(pDev, 0, &surface)) && surface) {
        if (SUCCEEDED(IDirect3DSurface9_GetDesc(surface, &desc))) {
            rtWidth = (int)desc.Width;
            rtHeight = (int)desc.Height;
        }
        IDirect3DSurface9_Release(surface);
    }
    if (rtWidth <= 0 || rtHeight <= 0) {
        hGLRC = gldGetCurrentContext();
        if (hGLRC) ctx = gldGetContextAddress(hGLRC);
        if (ctx) {
            rtWidth = (int)ctx->dwWidth;
            rtHeight = (int)ctx->dwHeight;
        }
    }
    if (rtWidth <= 0 || rtHeight <= 0) return FALSE;

    desiredLeft = s->viewportX;
    desiredTop = (int64_t)rtHeight -
                 ((int64_t)s->viewportY + s->viewportH);
    desiredRight = desiredLeft + s->viewportW;
    desiredBottom = desiredTop + s->viewportH;
    clippedLeft = desiredLeft < 0 ? 0 : desiredLeft;
    clippedTop = desiredTop < 0 ? 0 : desiredTop;
    clippedRight = desiredRight > rtWidth ? rtWidth : desiredRight;
    clippedBottom = desiredBottom > rtHeight ? rtHeight : desiredBottom;
    if (clippedRight <= clippedLeft || clippedBottom <= clippedTop)
        return FALSE;

    vp->X = (DWORD)clippedLeft;
    vp->Y = (DWORD)clippedTop;
    vp->Width = (DWORD)(clippedRight - clippedLeft);
    vp->Height = (DWORD)(clippedBottom - clippedTop);
    vp->MinZ = s->depthRangeNear;
    vp->MaxZ = s->depthRangeFar;

    if (adjust) {
        adjust[0] = (float)s->viewportW / (float)vp->Width;
        adjust[1] = (float)s->viewportH / (float)vp->Height;
        adjust[2] = (float)(2 * desiredLeft + s->viewportW -
                            2 * clippedLeft - (int64_t)vp->Width) /
                    (float)vp->Width - 1.0f / (float)vp->Width;
        adjust[3] = (float)(2 * desiredTop + s->viewportH -
                            2 * clippedTop - (int64_t)vp->Height) /
                    (float)vp->Height + 1.0f / (float)vp->Height;
    }
    return TRUE;
}

void _glsViewport(int x, int y, int width, int height)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    if (width < 0 || height < 0) {
        s->lastError = GL_INVALID_VALUE;
        return;
    }

    s->viewportX = x;
    s->viewportY = y;
    s->viewportW = width;
    s->viewportH = height;
    gldDiagLogV("GL: glViewport(%d, %d, %d, %d)", x, y, width, height);

    if (pDev && width > 0 && height > 0) {
        D3DVIEWPORT9 vp;
        if (_glsBuildClippedViewport(pDev, s, &vp, NULL)) {
            __try { IDirect3DDevice9_SetViewport(pDev, &vp); }
            __except(EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
}

void _glsDepthRange(double nearVal, double farVal)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    if (nearVal < 0.0) nearVal = 0.0;
    if (nearVal > 1.0) nearVal = 1.0;
    if (farVal < 0.0) farVal = 0.0;
    if (farVal > 1.0) farVal = 1.0;
    s->depthRangeNear = (float)nearVal;
    s->depthRangeFar = (float)farVal;

    /* Update D3D9 viewport with new depth range */
    if (pDev && s->viewportW > 0 && s->viewportH > 0) {
        D3DVIEWPORT9 vp;
        if (_glsBuildClippedViewport(pDev, s, &vp, NULL)) {
            __try { IDirect3DDevice9_SetViewport(pDev, &vp); }
            __except(EXCEPTION_EXECUTE_HANDLER) { }
        }
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

    gldDiagLogV("GL: glClear(0x%X) pDev=%p", mask, (void*)pDev);

    if (!pDev) return;

    if (mask & GL_COLOR_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_TARGET;
    if (mask & GL_DEPTH_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_ZBUFFER;
    if (mask & GL_STENCIL_BUFFER_BIT)
        d3dFlags |= D3DCLEAR_STENCIL;

    clearCol = D3DCOLOR_COLORVALUE(s->clearColor[0], s->clearColor[1], s->clearColor[2], s->clearColor[3]);

    IDirect3DDevice9_Clear(pDev, 0, NULL, d3dFlags, clearCol, s->clearDepth, s->clearStencil);
    gldFragmentEmulatorClear(mask, s->clearDepth, s->clearStencil);
}

/* ===================================================================
 *  SECTION 6: Matrix Functions
 * =================================================================== */

void _glsMatrixMode(unsigned int mode)
{
    GLS_DLUInt2 a = {{ mode, 0 }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLMatrixMode, &a, sizeof(a))) return;
    s->matrixMode = mode;
}

void _glsLoadIdentity(void)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (_glsDLRecord(_glsDLLoadIdentity, NULL, 0)) return;
    if (stack) glsMatrixIdentity(stack->stack[stack->top].m);
}

void _glsLoadMatrixf(const float *m)
{
    GLS_DLFloat16 a;
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (!m) return;
    memcpy(a.f, m, sizeof(a.f));
    if (_glsDLRecord(_glsDLLoadMatrix, &a, sizeof(a))) return;
    if (stack) memcpy(stack->stack[stack->top].m, m, 16 * sizeof(float));
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
    GLS_DLFloat16 a;
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (!m) return;
    memcpy(a.f, m, sizeof(a.f));
    if (_glsDLRecord(_glsDLMultMatrix, &a, sizeof(a))) return;
    if (stack) {
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
    if (_glsDLRecord(_glsDLPushMatrix, NULL, 0)) return;
    if (stack && stack->top < GLS_MAX_MATRIX_STACK - 1) {
        memcpy(stack->stack[stack->top + 1].m, stack->stack[stack->top].m, 16 * sizeof(float));
        stack->top++;
    }
}

void _glsPopMatrix(void)
{
    GLS_MatrixStack *stack = glsGetCurrentMatrixStack();
    if (_glsDLRecord(_glsDLPopMatrix, NULL, 0)) return;
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
    GLS_DLUInt2 a = {{ mode, 0 }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLBegin, &a, sizeof(a))) return;
    s->inBeginEnd = TRUE;
    s->beginMode = mode;
    s->immVertexCount = 0;
}

/*
 * glEnd — flush the vertices accumulated since glBegin.
 *
 * Shares the primitive expansion used by the vertex-array paths, so
 * GL_LINE_LOOP, GL_QUAD_STRIP and GL_POLYGON work here too rather than
 * being silently dropped.
 */
void _glsEnd(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    GLS_D3DVertex *verts;
    unsigned int *idx;
    int indexCount, primCount, count, i;

    if (_glsDLRecord(_glsDLEnd, NULL, 0)) return;
    s->inBeginEnd = FALSE;

    count = s->immVertexCount;
    if (!pDev || count <= 0)
        return;

    indexCount = _glsExpandedIndexCount(s->beginMode, count);
    if (indexCount <= 0) {
        gldDiagLog("GL: glEnd unsupported mode 0x%X", s->beginMode);
        return;
    }

    if (!_glsApplyTransforms(pDev, s))
        return;

    verts = (GLS_D3DVertex *)malloc((size_t)count * sizeof(GLS_D3DVertex));
    if (!verts) return;

    idx = (unsigned int *)malloc((size_t)indexCount * sizeof(unsigned int));
    if (!idx) { free(verts); return; }

    for (i = 0; i < count; i++) {
        GLS_ImmVertex *sv = &s->immVertices[i];
        verts[i].x  = sv->pos[0];
        verts[i].y  = sv->pos[1];
        verts[i].z  = sv->pos[2];
        verts[i].nx = sv->normal[0];
        verts[i].ny = sv->normal[1];
        verts[i].nz = sv->normal[2];
        verts[i].color = _glsPackColor(sv->color);
        {
            float sc[4];
            sc[0] = s->secondaryColor[0]; sc[1] = s->secondaryColor[1];
            sc[2] = s->secondaryColor[2]; sc[3] = 0.0f;
            verts[i].specular = _glsPackColor(sc);
        }
        verts[i].u0 = sv->texcoord[0][0];
        verts[i].v0 = sv->texcoord[0][1];
        verts[i].u1 = sv->texcoord[1][0];
        verts[i].v1 = sv->texcoord[1][1];
        /* GLS_ImmVertex captures no generic attributes, so 6/7 take GL's
         * default attribute value rather than whatever malloc handed back. */
        verts[i].genericAttrib6[0] = 0.0f; verts[i].genericAttrib6[1] = 0.0f;
        verts[i].genericAttrib6[2] = 0.0f; verts[i].genericAttrib6[3] = 1.0f;
        verts[i].genericAttrib7[0] = 0.0f; verts[i].genericAttrib7[1] = 0.0f;
        verts[i].genericAttrib7[2] = 0.0f; verts[i].genericAttrib7[3] = 1.0f;
    }

    primCount = _glsExpandPrimitive(s->beginMode, count, &d3dPrimType, idx);
    if (primCount > 0)
        _glsSubmitIndexed(pDev, d3dPrimType, primCount, verts, count, idx, indexCount);

    free(idx);
    free(verts);
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

void _glsVertex2f(float x, float y) { _glsVertex4f(x, y, 0.0f, 1.0f); }
void _glsVertex3f(float x, float y, float z) { _glsVertex4f(x, y, z, 1.0f); }
void _glsVertex4f(float x, float y, float z, float w)
{
    GLS_DLFloat16 a = {{ x, y, z, w }};
    if (_glsDLRecord(_glsDLVertex4, &a, 4 * (int)sizeof(float))) return;
    _emitVertex(x, y, z, w);
}

void _glsColor3f(float r, float g, float b)
{
    _glsColor4f(r, g, b, 1.0f);
}

void _glsColor4f(float r, float g, float b, float a)
{
    GLS_DLFloat16 args = {{ r, g, b, a }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLColor4, &args, 4 * (int)sizeof(float))) return;
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
    GLS_DLFloat16 a = {{ nx, ny, nz }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLNormal3, &a, 3 * (int)sizeof(float))) return;
    s->currentNormal[0] = nx; s->currentNormal[1] = ny; s->currentNormal[2] = nz;
}

void _glsTexCoord2f(float st, float tt)
{
    _glsTexCoord4f(st, tt, 0.0f, 1.0f);
}

void _glsTexCoord3f(float st, float tt, float rt)
{
    _glsTexCoord4f(st, tt, rt, 1.0f);
}

void _glsTexCoord4f(float st, float tt, float rt, float qt)
{
    GLS_DLFloat16 a = {{ st, tt, rt, qt }};
    GLS_State *s = glsGetState();
    if (_glsDLRecord(_glsDLTexCoord4, &a, 4 * (int)sizeof(float))) return;
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
    gldDiagLogV("GL: glCreateShader(0x%X) -> %u", type, id);
    return id;
}

void _glsDeleteShader(unsigned int shader)
{
    GLS_State *s = glsGetState();
    GLS_Shader *sh = glsFindShader(shader);
    if (sh) {
        if (sh->source) { free(sh->source); sh->source = NULL; }
        _glsReleaseARBProgram(sh);
        sh->allocated = FALSE;
    }
    if (s->boundVertexProgramARB == shader)   s->boundVertexProgramARB = 0;
    if (s->boundFragmentProgramARB == shader) s->boundFragmentProgramARB = 0;
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
    gldDiagLogV("GL: glCompileShader(%u) -> compiled=%d", shader, sh ? sh->compiled : -1);
}

void _glsGetShaderiv(unsigned int shader, unsigned int pname, int *params)
{
    GLS_Shader *sh = glsFindShader(shader);
    if (!params) return;

    if (!sh) {
        *params = 0;
        gldDiagLogV("GL: glGetShaderiv(%u, 0x%X) -> 0 (NOT FOUND)", shader, pname);
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
    gldDiagLogV("GL: glGetShaderiv(%u, 0x%X) -> %d", shader, pname, *params);
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
    gldDiagLogV("GL: glCreateProgram() -> %u", id);
    return id;
}

/* Defined with the rest of the program code, needed here by glDeleteProgram
 * which sits ahead of that definition. */
static void _glsReleaseProgramShaders(GLS_Program *prog);

void _glsDeleteProgram(unsigned int program)
{
    GLS_State *s = glsGetState();
    GLS_Program *prog = glsFindProgram(program);
    if (prog) {
        if (s->boundProgram == program) s->boundProgram = 0;
        /* The D3D9 vertex/pixel shader objects the link created are owned by
         * the program; marking the slot free without releasing them strands
         * them on the device for the life of the process. */
        _glsReleaseProgramShaders(prog);
        prog->allocated = FALSE;
    }
}

void _glsAttachShader(unsigned int program, unsigned int shader)
{
    GLS_Program *prog = glsFindProgram(program);
    GLS_Shader *sh = glsFindShader(shader);
    if (!prog || !sh) return;

    if (sh->type == GL_VERTEX_SHADER) {
        prog->vertShader = shader;
    } else if (sh->type == GL_FRAGMENT_SHADER) {
        prog->fragShader = shader;
    } else if (sh->type == GL_GEOMETRY_SHADER) {
        prog->geomShader = shader;
    } else if (sh->type == GL_TESS_CONTROL_SHADER) {
        prog->tessControlShader = shader;
    } else if (sh->type == GL_TESS_EVALUATION_SHADER) {
        prog->tessEvalShader = shader;
    } else if (sh->type == GL_COMPUTE_SHADER) {
        prog->computeShader = shader;
    } else {
        /* Any other stage is silently dropped, as it always has been — the
         * program still links from whatever vertex/fragment shaders it has.
         * What is new is saying so once: a program whose geometry or compute
         * stage simply never ran is otherwise indistinguishable from one that
         * ran and produced nothing. */
        static BOOL warned = FALSE;
        if (!warned) {
            const char *stage =
                (sh->type == GL_GEOMETRY_SHADER)        ? "geometry" :
                (sh->type == GL_TESS_CONTROL_SHADER)    ? "tessellation control" :
                (sh->type == GL_TESS_EVALUATION_SHADER) ? "tessellation evaluation" :
                (sh->type == GL_COMPUTE_SHADER)         ? "compute" : "unrecognised";
            warned = TRUE;
            gldDiagLog("GL: glAttachShader - %s shader (type 0x%X) attached to program %u; "
                       "D3D9 has no such stage and no lowering exists, so it is ignored "
                       "and the program links from its vertex/fragment stages only",
                       stage, sh->type, program);
        }
    }
}

void _glsDetachShader(unsigned int program, unsigned int shader)
{
    GLS_Program *p = glsFindProgram(program);

    if (!p) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }

    /* A program here holds one vertex and one fragment shader rather than a
     * list, so detaching is clearing whichever slot names this shader. */
    if (p->vertShader == shader) p->vertShader = 0;
    if (p->fragShader == shader) p->fragShader = 0;
    if (p->geomShader == shader) p->geomShader = 0;
    if (p->tessControlShader == shader) p->tessControlShader = 0;
    if (p->tessEvalShader == shader) p->tessEvalShader = 0;
    if (p->computeShader == shader) p->computeShader = 0;
}

/*
 * Merge one shader's reflected constant table into the program's resolved
 * uniform list, matching entries by name across the two shader stages.
 */
static void _glsMergeUniformMap(GLS_Program *prog, const glslUniformMap *map,
                                int count, BOOL isVertex)
{
    int i, j;

    for (i = 0; i < count; i++) {
        GLS_ResolvedUniform *slot = NULL;
        const char *name = map[i].name;

        /* The transpiler renames a uniform whose GLSL name is an HLSL keyword
         * to _glsl_kw_<name>, so that is what comes back from the constant
         * table.  The application only ever knows the name it wrote, so the
         * GL-visible name has to be the original one. */
        if (strncmp(name, "_glsl_kw_", 9) == 0)
            name += 9;

        for (j = 0; j < prog->resolvedCount; j++) {
            if (strcmp(prog->resolved[j].name, name) == 0) {
                slot = &prog->resolved[j];
                break;
            }
        }

        if (!slot) {
            if (prog->resolvedCount >= GLS_MAX_UNIFORMS)
                continue;
            slot = &prog->resolved[prog->resolvedCount++];
            strncpy(slot->name, name, sizeof(slot->name) - 1);
            slot->name[sizeof(slot->name) - 1] = '\0';
            slot->vsRegister = -1;
            slot->psRegister = -1;
        }

        slot->registerCount = map[i].registerCount;
        slot->registerSet   = map[i].registerSet;
        if (isVertex) slot->vsRegister = map[i].registerIndex;
        else          slot->psRegister = map[i].registerIndex;
    }
}

/*
 * Give every uniform the shader *declares* a location, not only those that
 * survived optimisation.
 *
 * D3DCompile removes any uniform that cannot affect the output, so it never
 * appears in the constant table this program's map was built from.  GL permits
 * glGetUniformLocation to answer -1 for such an inactive uniform, but engines
 * that resolve a fixed parameter list after linking read that as a broken
 * program: id Tech 5 logs "No address, error: %d while loading renderProg %s"
 * and then fails the link outright, so a shader is rejected for a uniform the
 * application can see in the source it just handed us.
 *
 * Registering the declared name with no register attached costs nothing at draw
 * time - _glsUploadUniform writes only to stages whose register is >= 0, so an
 * upload here is accepted and dropped, which is the behaviour GL specifies for
 * an inactive uniform anyway.
 */
static void _glsRegisterDeclaredUniforms(GLS_Program *prog, const char *source,
                                         const char *stage)
{
    glslUniformMap *decl;
    int count, i, j, added = 0;

    if (!prog || !source) return;

    decl = (glslUniformMap *)malloc(sizeof(glslUniformMap) * GLSL_MAX_UNIFORM_MAP);
    if (!decl) return;

    count = glslReflectDeclaredUniforms(source, decl, GLSL_MAX_UNIFORM_MAP);

    for (i = 0; i < count; i++) {
        GLS_ResolvedUniform *slot;
        BOOL found = FALSE;

        for (j = 0; j < prog->resolvedCount; j++) {
            if (strcmp(prog->resolved[j].name, decl[i].name) == 0) {
                found = TRUE;
                break;
            }
        }
        if (found) continue;                     /* already has a register */

        if (prog->resolvedCount >= GLS_MAX_UNIFORMS)
            break;

        slot = &prog->resolved[prog->resolvedCount++];
        strncpy(slot->name, decl[i].name, sizeof(slot->name) - 1);
        slot->name[sizeof(slot->name) - 1] = '\0';
        slot->vsRegister    = -1;
        slot->psRegister    = -1;
        slot->registerSet   = decl[i].registerSet;
        slot->registerCount = decl[i].registerCount;
        added++;
    }

    if (added)
        gldDiagLog("GL: program %u %s - %d declared uniform(s) the compiler "
                   "optimised out kept a location so lookups still resolve",
                   prog->id, stage, added);

    free(decl);
}

/*
 * Pair up the transpiler's synthesized "_glsl_texdim_<sampler>" uniforms with
 * the samplers they describe.
 *
 * The transpiler emits one of these for every sampler whose texelFetch or
 * textureSize it lowered; it is an ordinary named uniform, so it arrives here
 * through the same CTAB reflection as any other and needs no new reflection
 * code.  All that is left is to remember which constant register it landed on
 * and which sampler's texture supplies its value, so the draw path can fill it
 * in.  A sampler that cannot be found is skipped rather than guessed at.
 */
#define GLS_TEXDIM_PREFIX     "_glsl_texdim_"
#define GLS_TEXDIM_PREFIX_LEN 13

static void _glsResolveTexDimBindings(GLS_Program *prog)
{
    int i, j;

    prog->texDimCount = 0;

    for (i = 0; i < prog->resolvedCount; i++) {
        const char *name = prog->resolved[i].name;
        const char *sampler;

        if (strncmp(name, GLS_TEXDIM_PREFIX, GLS_TEXDIM_PREFIX_LEN) != 0)
            continue;
        sampler = name + GLS_TEXDIM_PREFIX_LEN;
        if (!*sampler) continue;

        for (j = 0; j < prog->resolvedCount; j++) {
            if (prog->resolved[j].registerSet != GLSL_RS_SAMPLER) continue;
            if (strcmp(prog->resolved[j].name, sampler) != 0) continue;

            if (prog->texDimCount < GLS_MAX_TEXDIM) {
                GLS_TexDimBinding *b = &prog->texDim[prog->texDimCount++];
                b->vsRegister        = prog->resolved[i].vsRegister;
                b->psRegister        = prog->resolved[i].psRegister;
                b->samplerPsRegister = prog->resolved[j].psRegister;
                gldDiagLogV("GL: program %u texdim '%s' vs=%d ps=%d <- sampler '%s' stage %d",
                           prog->id, name, b->vsRegister, b->psRegister,
                           sampler, b->samplerPsRegister);
            }
            break;
        }
    }
}

static void _glsResolveViewportBinding(GLS_Program *prog)
{
    int i;
    prog->viewportRegister = -1;
    for (i = 0; i < prog->resolvedCount; ++i) {
        if (!strcmp(prog->resolved[i].name, "_glsl_viewportAdjust")) {
            prog->viewportRegister = prog->resolved[i].vsRegister;
            break;
        }
    }
}

/* Release any D3D9 shader objects a program is holding. */
static void _glsReleaseProgramShaders(GLS_Program *prog)
{
    if (prog->pVS) { IDirect3DVertexShader9_Release(prog->pVS); prog->pVS = NULL; }
    if (prog->pPS) { IDirect3DPixelShader9_Release(prog->pPS);  prog->pPS = NULL; }
}

/*
 * glLinkProgram — transpile the attached GLSL to HLSL, compile it to Shader
 * Model 3 bytecode and create the D3D9 shader objects.
 *
 * This is where a GL 2.0+ program actually becomes something D3D9 can run.
 * Failure leaves linked = FALSE with a message in the info log, which is what
 * an application's glGetProgramiv(GL_LINK_STATUS) check expects to see.
 */
void _glsLinkProgram(unsigned int program)
{
    GLS_Program *prog = glsFindProgram(program);
    GLS_Shader *vs, *fs, *cs;
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    void *vsCode = NULL, *psCode = NULL;
    DWORD vsSize = 0, psSize = 0;
    glslUniformMap map[GLSL_MAX_UNIFORM_MAP];
    int mapCount;

    if (!prog) {
        gldDiagLogV("GL: glLinkProgram(%u) -> program not found", program);
        return;
    }

    prog->linked        = FALSE;
    prog->resolvedCount = 0;
    prog->texDimCount   = 0;
    prog->viewportRegister = -1;
    prog->infoLog[0]    = '\0';
    _glsReleaseProgramShaders(prog);

    vs = glsFindShader(prog->vertShader);
    fs = glsFindShader(prog->fragShader);
    cs = glsFindShader(prog->computeShader);
    prog->softwareVertexExecution = FALSE;
    prog->softwareFragmentExecution = FALSE;
    if (vs && vs->source &&
        (strstr(vs->source, "gl_VertexID") ||
         strstr(vs->source, "gl_InstanceID") ||
         strstr(vs->source, "gl_BaseInstance") ||
         strstr(vs->source, "gl_DrawID")))
        prog->softwareVertexExecution = TRUE;

    /* Compute does not require a D3D9 device: it runs in the private software
     * worker and writes the CPU buffer shadows that later D3D9 draws consume. */
    if (cs) {
        if (vs || fs || prog->geomShader || prog->tessControlShader ||
            prog->tessEvalShader) {
            strcpy(prog->infoLog, "a compute program cannot contain graphics stages");
            return;
        }
        _glsRegisterDeclaredUniforms(prog, cs->source, "compute");
        if (!gldComputeEmulatorLink(prog, prog->infoLog, sizeof(prog->infoLog))) {
            gldDiagLog("GL: glLinkProgram(%u) compute emulation link failed: %s",
                       program, prog->infoLog);
            return;
        }
        prog->linked = TRUE;
        prog->validated = TRUE;
        gldDiagLogV("GL: glLinkProgram(%u) -> software compute program linked", program);
        return;
    }

    if (!pDev) {
        strcpy(prog->infoLog, "no D3D9 device");
        gldDiagLog("GL: glLinkProgram(%u) -> no D3D9 device", program);
        return;
    }

    if (!glslTranspilerInit()) {
        strcpy(prog->infoLog, "GLSL transpiler unavailable (d3dcompiler_47.dll missing)");
        gldDiagLogV("GL: glLinkProgram(%u) -> transpiler unavailable", program);
        return;
    }

    /* Never claim a successful link after dropping an attached programmable
     * stage.  Compute is executed above by the software worker; arbitrary
     * geometry and tessellation shaders still need a graphics-stage emulator. */
    /* Vertex stage */
    if (vs && vs->source && !prog->geomShader &&
        !prog->tessControlShader && !prog->tessEvalShader &&
        !prog->softwareVertexExecution) {
        if (!glslTranspileAndCompile(0, vs->source, &vsCode, &vsSize)) {
            /* Shader Model 3 rejection is not a GL link failure: execute the
             * vertex stage in the private GL 4.6 worker and hand its outputs
             * to the fixed post-stage D3D9 vertex shader instead. */
            prog->softwareVertexExecution = TRUE;
            prog->infoLog[0] = '\0';
            gldDiagLog("GL: glLinkProgram(%u) -> VS requires software GL 4.6 execution",
                       program);
        } else {
            if (_glsShadersUsable()) {
                if (!glslCreateVertexShader(pDev, vsCode, vsSize, &prog->pVS)) {
                    glslFreeBytecode(vsCode);
                    vsCode = NULL;
                    prog->softwareVertexExecution = TRUE;
                    gldDiagLog("GL: glLinkProgram(%u) -> D3D9 rejected VS; using software GL 4.6 execution",
                               program);
                }
            }
            if (vsCode) {
                mapCount = glslReflectConstants(vsCode, vsSize, map, GLSL_MAX_UNIFORM_MAP);
                _glsMergeUniformMap(prog, map, mapCount, TRUE);
                glslFreeBytecode(vsCode);
                vsCode = NULL;
            }
        }
    }

    /* Fragment stage */
    if (fs && fs->source) {
        if (!glslTranspileAndCompile(1, fs->source, &psCode, &psSize)) {
            /* Shader Model 3 is the native fast path, not the feature limit.
             * Execute the original GLSL fragment stage in the private 4.6
             * context and copy its framebuffer result back to D3D9. */
            prog->softwareFragmentExecution = TRUE;
            prog->infoLog[0] = '\0';
            gldDiagLog("GL: glLinkProgram(%u) -> PS requires software GL 4.6 execution",
                       program);
        } else if (_glsShadersUsable()) {
            if (!glslCreatePixelShader(pDev, psCode, psSize, &prog->pPS)) {
                prog->softwareFragmentExecution = TRUE;
                gldDiagLog("GL: glLinkProgram(%u) -> D3D9 rejected PS; using software GL 4.6 execution",
                           program);
            }
        }
        if (psCode) {
            mapCount = glslReflectConstants(psCode, psSize, map, GLSL_MAX_UNIFORM_MAP);
            _glsMergeUniformMap(prog, map, mapCount, FALSE);
            glslFreeBytecode(psCode);
            psCode = NULL;
        }
    }

    /* Both stages' constant tables are merged by now, so the synthesized
     * texture-dimension uniforms can be matched to their samplers. */
    _glsResolveTexDimBindings(prog);
    _glsResolveViewportBinding(prog);

    /* Anything the two stages declared but the compiler dropped still needs a
     * location, or an engine that resolves its parameter list after linking
     * sees a shader it wrote uniforms into report that they do not exist. */
    if (vs && vs->source) _glsRegisterDeclaredUniforms(prog, vs->source, "vertex");
    if (fs && fs->source) _glsRegisterDeclaredUniforms(prog, fs->source, "fragment");

    if (prog->softwareVertexExecution || prog->softwareFragmentExecution ||
        prog->geomShader ||
        prog->tessControlShader || prog->tessEvalShader ||
        prog->transformFeedbackCount > 0) {
        if (!gldStageEmulatorLinkGraphics(prog, prog->infoLog,
                                          sizeof(prog->infoLog))) {
            _glsReleaseProgramShaders(prog);
            gldDiagLog("GL: glLinkProgram(%u) software graphics stages failed: %s",
                       program, prog->infoLog);
            return;
        }
    }

    /* Attribute names vanish during compilation, so capture them from the
     * source while it is still available — glGetActiveAttrib has no other
     * source for them. */
    prog->activeAttribCount = 0;
    if (vs && vs->source) {
        glslAttribInfo attribs[GLSL_MAX_ATTRIB_INFO];
        int n = glslReflectAttributes(vs->source, attribs, GLSL_MAX_ATTRIB_INFO);
        int i;
        for (i = 0; i < n && i < GLS_MAX_ATTRIB_BINDINGS; i++) {
            strncpy(prog->activeAttribs[i].name, attribs[i].name,
                    sizeof(prog->activeAttribs[i].name) - 1);
            prog->activeAttribs[i].name[sizeof(prog->activeAttribs[i].name) - 1] = '\0';
            prog->activeAttribs[i].type = (GLenum_t)attribs[i].glType;
            prog->activeAttribs[i].size = attribs[i].arraySize;
            prog->activeAttribCount++;
        }
    }

    prog->linked = TRUE;
    gldDiagLogV("GL: glLinkProgram(%u) -> linked (VS=%p PS=%p, %d uniforms, %d attributes)",
               program, prog->pVS, prog->pPS, prog->resolvedCount, prog->activeAttribCount);
}

/*
 * glGetActiveUniform — described from the reflected constant table, which
 * already carries every uniform's name and register footprint.
 */
void _glsGetActiveUniform(unsigned int program, unsigned int index, int bufSize,
                          int *length, int *size, unsigned int *type, char *name)
{
    GLS_Program *prog = glsFindProgram(program);
    GLS_ResolvedUniform *u;
    int n;

    if (length) *length = 0;
    if (size)   *size = 0;
    if (type)   *type = 0;
    if (name && bufSize > 0) name[0] = '\0';

    if (!prog || (int)index >= prog->resolvedCount) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }
    u = &prog->resolved[index];

    if (name && bufSize > 0) {
        n = (int)strlen(u->name);
        if (n > bufSize - 1) n = bufSize - 1;
        memcpy(name, u->name, (size_t)n);
        name[n] = '\0';
        if (length) *length = n;
    }
    if (size) *size = (u->registerCount > 0) ? u->registerCount : 1;
    if (type) {
        if (u->registerSet == GLSL_RS_SAMPLER)      *type = 0x8B5E; /* GL_SAMPLER_2D  */
        else if (u->registerCount == 4)             *type = 0x8B5C; /* GL_FLOAT_MAT4  */
        else if (u->registerCount == 3)             *type = 0x8B5B; /* GL_FLOAT_MAT3  */
        else if (u->registerSet == GLSL_RS_INT4)    *type = 0x8B55; /* GL_INT_VEC4    */
        else if (u->registerSet == GLSL_RS_BOOL)    *type = 0x8B56; /* GL_BOOL        */
        else                                        *type = 0x8B52; /* GL_FLOAT_VEC4  */
    }
}

/* glGetActiveAttrib — from the names captured at link time. */
void _glsGetActiveAttrib(unsigned int program, unsigned int index, int bufSize,
                         int *length, int *size, unsigned int *type, char *name)
{
    GLS_Program *prog = glsFindProgram(program);
    int n;

    if (length) *length = 0;
    if (size)   *size = 0;
    if (type)   *type = 0;
    if (name && bufSize > 0) name[0] = '\0';

    if (!prog || (int)index >= prog->activeAttribCount) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }

    if (name && bufSize > 0) {
        n = (int)strlen(prog->activeAttribs[index].name);
        if (n > bufSize - 1) n = bufSize - 1;
        memcpy(name, prog->activeAttribs[index].name, (size_t)n);
        name[n] = '\0';
        if (length) *length = n;
    }
    if (size) *size = prog->activeAttribs[index].size;
    if (type) *type = prog->activeAttribs[index].type;
}

/*
 * glUseProgram — bind the program's compiled shaders on the device.
 *
 * Program 0 unbinds both stages, which returns D3D9 to the fixed-function
 * pipeline that the immediate-mode and legacy array paths rely on.
 */
void _glsUseProgram(unsigned int program)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Program *prog;

    s->boundProgram = program;

    if (!pDev) return;

    if (program == 0) {
        if (s->boundProgramPipeline) {
            gldAdvBindProgramPipeline(s->boundProgramPipeline);
            gldDiagLogV("GL: glUseProgram(0) -> program pipeline %u",
                        s->boundProgramPipeline);
            return;
        }
        __try {
            IDirect3DDevice9_SetVertexShader(pDev, NULL);
            IDirect3DDevice9_SetPixelShader(pDev, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
        gldDiagLogV("GL: glUseProgram(0) -> fixed function");
        /* An ARB program that was masked by the GLSL program takes the stage
         * back now that nothing outranks it. */
        _glsApplyARBProgramBinding(GL_VERTEX_PROGRAM_ARB);
        _glsApplyARBProgramBinding(GL_FRAGMENT_PROGRAM_ARB);
        return;
    }

    prog = glsFindProgram(program);
    if (!prog || !prog->linked) {
        gldDiagLogV("GL: glUseProgram(%u) -> not linked, staying on fixed function", program);
        return;
    }

    if (!_glsShadersUsable()) {
        __try {
            IDirect3DDevice9_SetVertexShader(pDev, NULL);
            IDirect3DDevice9_SetPixelShader(pDev, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
        return;
    }

    __try {
        IDirect3DDevice9_SetVertexShader(pDev, prog->pVS);
        IDirect3DDevice9_SetPixelShader(pDev, prog->pPS);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    gldDiagLogV("GL: glUseProgram(%u) -> VS=%p PS=%p", program, prog->pVS, prog->pPS);
}

void _glsGetProgramiv(unsigned int program, unsigned int pname, int *params)
{
    GLS_Program *prog = glsFindProgram(program);
    if (!params) return;

    if (!prog && program != 0) {
        *params = 0;
        gldDiagLogV("GL: glGetProgramiv(%u, 0x%X) -> 0 (NOT FOUND)", program, pname);
        return;
    }

    switch (pname) {
    case GL_LINK_STATUS:      *params = (prog && prog->linked) ? GL_TRUE : GL_FALSE; break;
    case GL_VALIDATE_STATUS:  *params = (prog && prog->validated) ? GL_TRUE : GL_FALSE; break;
    case GL_DELETE_STATUS:    *params = GL_FALSE; break;
    case GL_INFO_LOG_LENGTH:  *params = 0; break;
    case GL_ATTACHED_SHADERS: *params = (prog ?
        ((prog->vertShader ? 1 : 0) + (prog->fragShader ? 1 : 0) +
         (prog->geomShader ? 1 : 0) + (prog->tessControlShader ? 1 : 0) +
         (prog->tessEvalShader ? 1 : 0) + (prog->computeShader ? 1 : 0)) : 0); break;
    case GL_ACTIVE_UNIFORMS:  *params = (prog ? prog->uniformCount : 0); break;
    case GL_ACTIVE_ATTRIBUTES: *params = 0; break;
    case 0x8B4E: /* GL_OBJECT_TYPE_ARB */ *params = 0x8B40; /* GL_PROGRAM_OBJECT_ARB */ break;
    case GL_COMPILE_STATUS:   *params = GL_TRUE; break;
    default:                  *params = GL_TRUE; break;
    }
    gldDiagLogV("GL: glGetProgramiv(%u, 0x%X) -> %d", program, pname, *params);
}

void _glsGetProgramInfoLog(unsigned int program, int bufSize, int *length, char *infoLog)
{
    GLS_Program *prog = glsFindProgram(program);
    int n;

    if (infoLog && bufSize > 0) infoLog[0] = '\0';
    if (length) *length = 0;

    if (!prog || !infoLog || bufSize <= 0) return;

    /* Report why linking failed — applications print this on link failure,
     * and a silent empty log makes a shader problem impossible to diagnose. */
    strncpy(infoLog, prog->infoLog, (size_t)bufSize - 1);
    infoLog[bufSize - 1] = '\0';

    n = (int)strlen(infoLog);
    if (length) *length = n;
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
    case 0x0B00: /* GL_CURRENT_COLOR */
        memcpy(params, s->currentColor, 4 * sizeof(float));
        break;
    case 0x0B02: /* GL_CURRENT_NORMAL */
        memcpy(params, s->currentNormal, 3 * sizeof(float));
        break;
    case 0x0B03: { /* GL_CURRENT_TEXTURE_COORDS */
        int unit = (int)(s->activeTexUnit - GL_TEXTURE0);
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        memcpy(params, s->currentTexCoord[unit], 4 * sizeof(float));
        break;
    }
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
    case 0x0BA8: { /* GL_TEXTURE_MATRIX */
        int unit = (int)(s->activeTexUnit - GL_TEXTURE0);
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        memcpy(params, s->textureStack[unit].stack[s->textureStack[unit].top].m,
               16 * sizeof(float));
        break;
    }
    case 0x2503: /* GL_POLYGON_OFFSET_FACTOR */ *params = s->polygonOffsetFactor; break;
    case 0x2504: /* GL_POLYGON_OFFSET_UNITS */ *params = s->polygonOffsetUnits; break;
    default: *params = 0.0f; break;
    }
    gldDiagLogV("GL: glGetFloatv(0x%X) -> %f", pname, *params);
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
    case 0x851C: /* GL_MAX_CUBE_MAP_TEXTURE_SIZE */ *params = 16384; break;
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
    case 0x821D: /* GL_NUM_EXTENSIONS */ {
        extern int _gldGetExtensionCount(void);
        *params = _gldGetExtensionCount();
        break;
    }

    /* Legacy limits */
    case 0x0D32: /* GL_MAX_CLIP_PLANES */ *params = 6; break;
    case 0x0D31: /* GL_MAX_LIGHTS */ *params = 8; break;
    case 0x0D30: /* GL_MAX_MODELVIEW_STACK_DEPTH */ *params = 32; break;
    case 0x0D38: /* GL_MAX_PROJECTION_STACK_DEPTH */ *params = 4; break;
    case 0x0D39: /* GL_MAX_TEXTURE_STACK_DEPTH */ *params = 10; break;
    case 0x84E2: /* GL_MAX_TEXTURE_UNITS_ARB */ *params = 8; break;
    case 0x8073: /* GL_MAX_3D_TEXTURE_SIZE */ *params = 2048; break;
    case 0x88FF: /* GL_MAX_ARRAY_TEXTURE_LAYERS */ *params = 2048; break;
    case 0x8D57: /* GL_MAX_SAMPLES */ *params = 4; break;
    case 0x910E: /* GL_MAX_COLOR_TEXTURE_SAMPLES */ *params = 4; break;
    case 0x910F: /* GL_MAX_DEPTH_TEXTURE_SAMPLES */ *params = 4; break;
    case 0x9110: /* GL_MAX_INTEGER_SAMPLES */ *params = 4; break;
    case 0x84FF: /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */ *params = 16; break;
    case 0x8B4D: /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS */ *params = 32; break;
    case 0x8A2B: /* GL_MAX_UNIFORM_BUFFER_BINDINGS */ *params = 36; break;
    case 0x8A30: /* GL_MAX_UNIFORM_BLOCK_SIZE */ *params = 65536; break;
    case 0x8A34: /* GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT */ *params = 256; break;
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
    case 0x88ED: /* GL_PIXEL_PACK_BUFFER_BINDING */ *params = s->boundPixelPackBuffer; break;
    case 0x88EF: /* GL_PIXEL_UNPACK_BUFFER_BINDING */ *params = s->boundPixelUnpackBuffer; break;
    case 0x85B5: /* GL_VERTEX_ARRAY_BINDING */ *params = s->boundVAO; break;
    case 0x8CA6: /* GL_DRAW_FRAMEBUFFER_BINDING */ *params = s->boundDrawFBO; break;
    case 0x8CAA: /* GL_READ_FRAMEBUFFER_BINDING */ *params = s->boundReadFBO; break;
    case 0x8CA7: /* GL_RENDERBUFFER_BINDING */ *params = s->boundRBO; break;

    /* Values that MUST NOT be zero (games divide by these) */
    case 0x0D3A: /* GL_MAX_VIEWPORT_DIMS */ params[0] = 16384; params[1] = 16384; break;
    case 0x8D6B: /* GL_MAX_ELEMENT_INDEX */ *params = 0x7FFFFFFF; break;
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

    /* NVIDIA GPU memory information, reported in KiB. */
    case 0x9047: /* GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX */
    case 0x9048: /* GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX */
    case 0x9049: /* GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX */
        *params = (int)gldGetAvailableVideoMemoryKB46();
        break;
    case 0x904A: /* GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX */
    case 0x904B: /* GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX */
        *params = 0;
        break;

    default: *params = 0; break;
    }
    gldDiagLogV("GL: glGetIntegerv(0x%X) -> %d", pname, *params);
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
        unsigned int id = s->nextShaderId;

        /* Handing back an id with no slot behind it is worse than handing back
         * nothing: every later glProgramStringARB and glBindProgramARB on it
         * finds no program and silently does nothing, so the application
         * renders with shaders it believes it created.  id Tech 4 generates
         * these on every level load and never stops, so the table does run out.
         *
         * GL says glGenProgramsARB returns *unused* names; 0 is never a valid
         * name, so it is the correct way to say "none left". */
        if (id >= GLS_MAX_SHADERS) {
            static BOOL warned = FALSE;
            if (!warned) {
                warned = TRUE;
                gldDiagLog("GL: glGenProgramsARB - the %d-entry shader table is full; "
                           "returning 0 for the rest. Programs are being generated "
                           "faster than they are deleted.", GLS_MAX_SHADERS);
            }
            programs[i] = 0;
            continue;
        }

        s->nextShaderId++;
        memset(&s->shaders[id], 0, sizeof(GLS_Shader));
        s->shaders[id].id = id;
        s->shaders[id].type = 0x8620; /* GL_VERTEX_PROGRAM_ARB */
        s->shaders[id].allocated = TRUE;
        programs[i] = id;
    }
}

/*
 * glDeleteProgramsARB — the counterpart to glGenProgramsARB.
 *
 * ARB programs live in the shader-ID space (see _glsGenProgramsARB above), so
 * deleting one is exactly what glDeleteShader does to a shader object: free the
 * source text, release the D3D9 shader objects and parameter block the program
 * holds, drop any binding naming it, and free the slot.  Without this the
 * assembly programs an id Tech 4 title generates every level load accumulate
 * D3D9 vertex/pixel shaders for the life of the process.
 */
void _glsDeleteProgramsARB(int n, const unsigned int *programs)
{
    GLS_State *s = glsGetState();
    int i;

    if (!programs || n <= 0) return;

    for (i = 0; i < n; i++) {
        GLS_Shader *sh = glsFindShader(programs[i]);
        if (sh) {
            if (sh->source) { free(sh->source); sh->source = NULL; }
            _glsReleaseARBProgram(sh);
            sh->allocated = FALSE;
        }
        if (s->boundVertexProgramARB == programs[i])   s->boundVertexProgramARB = 0;
        if (s->boundFragmentProgramARB == programs[i]) s->boundFragmentProgramARB = 0;
    }
}

/* Which of the two ARB program bindings a target enum selects. */
static GLuint_t *_glsARBBindingFor(GLS_State *s, unsigned int target)
{
    if (target == GL_VERTEX_PROGRAM_ARB)   return &s->boundVertexProgramARB;
    if (target == GL_FRAGMENT_PROGRAM_ARB) return &s->boundFragmentProgramARB;
    return NULL;
}

static BOOL _glsARBStageEnabled(GLS_State *s, unsigned int target)
{
    if (target == GL_VERTEX_PROGRAM_ARB)   return s->enableVertexProgramARB;
    if (target == GL_FRAGMENT_PROGRAM_ARB) return s->enableFragmentProgramARB;
    return FALSE;
}

/* Release the D3D9 objects and parameter storage an ARB program holds. */
static void _glsReleaseARBProgram(GLS_Shader *sh)
{
    if (!sh || !sh->arb) return;
    if (sh->arb->pVS) { IDirect3DVertexShader9_Release(sh->arb->pVS); sh->arb->pVS = NULL; }
    if (sh->arb->pPS) { IDirect3DPixelShader9_Release(sh->arb->pPS);  sh->arb->pPS = NULL; }
    free(sh->arb);
    sh->arb = NULL;
}

/*
 * Drop every reference the device is holding to an object this wrapper owns.
 *
 * Ordering copied from gldDestroyDrawable_DX (src/dx9/gld5_wgl.c): unbind the
 * stages first, then release, so D3D9 is never the last owner of something the
 * caller believes it has freed.
 */
static void _glsUnbindDeviceStages(IDirect3DDevice9 *pDev)
{
    int u;

    if (!pDev) return;

    for (u = 0; u < GLS_MAX_TEX_UNITS; u++) {
        __try {
            IDirect3DDevice9_SetTexture(pDev, (DWORD)u, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
    __try {
        IDirect3DDevice9_SetVertexShader(pDev, NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
    __try {
        IDirect3DDevice9_SetPixelShader(pDev, NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
    __try {
        IDirect3DDevice9_SetStreamSource(pDev, 0, NULL, 0, 0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/*
 * Prepare the device for IDirect3DDevice9::Reset.
 *
 * Reset fails with D3DERR_INVALIDCALL while anything the runtime destroys on
 * reset is still alive, so everything in that class has to be released first
 * and recreated (or lazily re-created on next use) afterwards.  What that
 * covers here, from an audit of every D3D9 object src/gl46 creates:
 *
 *   - D3DPOOL_DEFAULT resources: the only two allocations in this backend are
 *     the scratch offscreen-plain surfaces in _glsCopyPixels and _glsDrawPixels,
 *     both function-local and released on every exit path before their caller
 *     returns.  Nothing in g_glState holds one, so there is nothing to release
 *     here — but this is the function to extend if that ever changes.
 *   - Query objects.  These are not pool-scoped but are documented as needing
 *     release across a reset, and glBeginQuery already re-creates a query whose
 *     pQuery is NULL, so dropping them is free.
 *   - Everything else this wrapper owns survives a Reset by contract:
 *     D3DPOOL_MANAGED textures (every CreateTexture/CreateCubeTexture/
 *     CreateVolumeTexture call site passes D3DPOOL_MANAGED) and vertex/pixel
 *     shader objects, which are not pool resources.  Those must NOT be released
 *     here: they hold the texture and shader content the application has
 *     already uploaded and will not upload again.
 *
 * The device is also unbound from every stage so it is not the last owner of a
 * managed texture while the reset runs.
 */
void _glsReleaseDeviceLosableResources(IDirect3DDevice9 *pDev)
{
    GLS_State *s = glsGetState();
    int i;
    int queries = 0;

    _glsUnbindDeviceStages(pDev);

    for (i = 0; i < GLS_MAX_QUERIES; i++) {
        if (s->queries[i].pQuery) {
            __try {
                IDirect3DQuery9_Release(s->queries[i].pQuery);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            s->queries[i].pQuery = NULL;
            s->queries[i].active = FALSE;
            queries++;
        }
    }

    gldDiagLog("GL: pre-Reset release — %d query object(s) dropped, "
               "managed textures and shader objects kept", queries);
}

/*
 * Release every D3D9 object g_glState is holding, ahead of releasing the device
 * itself.
 *
 * g_glState is process-global and outlives any individual GL context, so this
 * is the one place that can return it to a state with no D3D9 pointers in it.
 * Called from gldShutdownContext46, immediately before the device goes away.
 */
void _glsReleaseAllDeviceResources(IDirect3DDevice9 *pDev)
{
    GLS_State *s = glsGetState();
    int i;
    int textures = 0, programs = 0, shaders = 0, queries = 0;

    _glsUnbindDeviceStages(pDev);
    _glsReleasePostStagePipeline();

    for (i = 0; i < GLS_MAX_TEXTURES; i++) {
        GLS_Texture *tex = &s->textures[i];
        if (tex->pTex || tex->pCubeTex || tex->pVolTex || tex->pixelData) {
            __try {
                _glsReleaseTextureResources(tex);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            textures++;
        }
        tex->allocated = FALSE;
    }

    for (i = 0; i < GLS_MAX_PROGRAMS; i++) {
        GLS_Program *prog = &s->programs[i];
        if (prog->pVS || prog->pPS) {
            __try {
                _glsReleaseProgramShaders(prog);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            programs++;
        }
        prog->allocated = FALSE;
    }

    /* Plain shader objects and ARB assembly programs share this array. */
    for (i = 0; i < GLS_MAX_SHADERS; i++) {
        GLS_Shader *sh = &s->shaders[i];
        if (sh->arb) {
            __try {
                _glsReleaseARBProgram(sh);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            shaders++;
        }
        if (sh->source) { free(sh->source); sh->source = NULL; }
        sh->allocated = FALSE;
    }

    for (i = 0; i < GLS_MAX_QUERIES; i++) {
        if (s->queries[i].pQuery) {
            __try {
                IDirect3DQuery9_Release(s->queries[i].pQuery);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            s->queries[i].pQuery = NULL;
            queries++;
        }
        s->queries[i].allocated = FALSE;
    }

    /* Releases d3dcompiler_47.dll; nothing else reaches this in a live build. */
    glslTranspilerShutdown();

    gldDiagLog("GL: shutdown sweep released %d texture(s), %d program shader "
               "pair(s), %d ARB program(s), %d query object(s)",
               textures, programs, shaders, queries);

    /* Any surviving surface keeps the device alive, which is why Remix reports
     * a device and a swapchain beside them at module eviction.  Zero here means
     * this file is not the source of those entries. */
    {
        LONG live = _glsSurfaceBalance();
        if (live != 0)
            gldDiagLog("GL: %ld D3D9 surface(s) still outstanding at shutdown - "
                       "match the 'surf +1' lines against 'surf -1' to find which",
                       live);
        else
            gldDiagLog("GL: 0 D3D9 surfaces outstanding at shutdown");
    }
}

/*
 * Push an ARB program's whole env/local parameter block into the D3D9
 * constant file.
 *
 * Constants live on the device, not on the shader object, so everything the
 * program reads has to be (re)uploaded whenever it becomes the active shader —
 * anything set through glProgramEnvParameter while a different program was
 * bound never reached these registers.
 */
static void _glsUploadARBParams(GLS_Shader *sh, BOOL isVertexStage)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_ARBProgram *p;
    if (!pDev || !sh || !sh->arb) return;
    p = sh->arb;

    __try {
        if (p->envBaseReg >= 0 && p->envRegCount > 0) {
            if (isVertexStage)
                IDirect3DDevice9_SetVertexShaderConstantF(pDev, p->envBaseReg,
                                                          &p->envParams[0][0], p->envRegCount);
            else
                IDirect3DDevice9_SetPixelShaderConstantF(pDev, p->envBaseReg,
                                                         &p->envParams[0][0], p->envRegCount);
        }
        if (p->localBaseReg >= 0 && p->localRegCount > 0) {
            if (isVertexStage)
                IDirect3DDevice9_SetVertexShaderConstantF(pDev, p->localBaseReg,
                                                          &p->localParams[0][0], p->localRegCount);
            else
                IDirect3DDevice9_SetPixelShaderConstantF(pDev, p->localBaseReg,
                                                         &p->localParams[0][0], p->localRegCount);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/*
 * Make the device agree with the ARB binding + enable state for one target.
 *
 * A linked GLSL program in use wins: glUseProgram and the ARB program targets
 * are separate mechanisms and GL gives the former precedence, so an enabled
 * ARB program must not quietly replace it.
 */
static void _glsApplyARBProgramBinding(unsigned int target)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLuint_t *binding = _glsARBBindingFor(s, target);
    GLS_Shader *sh;
    BOOL isVertexStage = (target == GL_VERTEX_PROGRAM_ARB);

    if (!pDev || !binding) return;

    if (s->boundProgram != 0) {
        GLS_Program *glslProg = glsFindProgram(s->boundProgram);
        if (glslProg && glslProg->linked) {
            gldDiagLogV("GL: ARB program target 0x%X not applied — GLSL program %u is in use",
                       target, s->boundProgram);
            return;
        }
    }

    sh = glsFindShader(*binding);

    if (!_glsShadersUsable() || !_glsARBStageEnabled(s, target) ||
        !sh || !sh->arb || !sh->compiled) {
        __try {
            if (isVertexStage) IDirect3DDevice9_SetVertexShader(pDev, NULL);
            else               IDirect3DDevice9_SetPixelShader(pDev, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
        return;
    }

    __try {
        if (isVertexStage) IDirect3DDevice9_SetVertexShader(pDev, sh->arb->pVS);
        else               IDirect3DDevice9_SetPixelShader(pDev, sh->arb->pPS);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    _glsUploadARBParams(sh, isVertexStage);
    gldDiagLogV("GL: ARB program %u applied to %s stage",
               *binding, isVertexStage ? "vertex" : "pixel");
}

void _glsBindProgramARB(unsigned int target, unsigned int program)
{
    GLS_State *s = glsGetState();
    GLuint_t *binding = _glsARBBindingFor(s, target);

    gldDiagLogV("GL: -> BindProgramARB target=0x%X program=%u", target, program);

    if (!binding) {
        gldDiagLog("GL: BindProgramARB unknown target 0x%X, ignored", target);
        return;
    }

    /* Auto-allocate if needed */
    if (program != 0 && program < GLS_MAX_SHADERS && !s->shaders[program].allocated) {
        memset(&s->shaders[program], 0, sizeof(GLS_Shader));
        s->shaders[program].id = program;
        s->shaders[program].type = target;
        s->shaders[program].allocated = TRUE;
    }

    *binding = program;
    _glsApplyARBProgramBinding(target);
}

/*
 * Find the constant register a named uniform array from the translated program
 * landed on.  The HLSL compiler allocates registers itself, so the mapping is
 * only knowable by reflecting the compiled bytecode.
 */
static void _glsFindARBArray(const glslUniformMap *map, int mapCount, const char *name,
                             int *pBase, int *pCount)
{
    int i;
    *pBase = -1;
    *pCount = 0;
    for (i = 0; i < mapCount; i++) {
        if (strcmp(map[i].name, name) == 0) {
            *pBase  = map[i].registerIndex;
            *pCount = map[i].registerCount;
            return;
        }
    }
}

/*
 * glProgramStringARB — translate ARB assembly to GLSL, then run it through the
 * same GLSL -> HLSL -> Shader Model 3 pipeline glLinkProgram uses.
 *
 * compiled means the string translated and compiled to Shader Model 3
 * bytecode and a matching D3D9 shader object was created. RTX Remix consumes
 * programmable D3D9 draws, so the translated object remains bound there too.
 *
 * Anything that cannot be translated is named in the log and leaves the
 * program uncompiled, so draws fall back to fixed function instead of
 * rendering with a shader that was never built.
 */
void _glsProgramStringARB(unsigned int target, unsigned int format, int len, const void *string)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLuint_t *binding = _glsARBBindingFor(s, target);
    GLS_Shader *sh;
    ARBTranslation info;
    char *glsl = NULL;
    void *code = NULL;
    DWORD codeSize = 0;
    glslUniformMap map[GLSL_MAX_UNIFORM_MAP];
    int mapCount = 0;
    BOOL isVertexStage = (target == GL_VERTEX_PROGRAM_ARB);

    gldDiagLogV("GL: -> ProgramStringARB target=0x%X format=0x%X len=%d", target, format, len);

    if (!binding) {
        gldDiagLogV("GL: ProgramStringARB unknown target 0x%X", target);
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    if (format != GL_PROGRAM_FORMAT_ASCII_ARB) {
        gldDiagLogV("GL: ProgramStringARB format 0x%X is not ASCII assembly", format);
        s->lastError = GL_INVALID_ENUM;
        return;
    }

    sh = glsFindShader(*binding);
    if (!sh) {
        gldDiagLogV("GL: ProgramStringARB with no program bound to target 0x%X", target);
        s->lastError = GL_INVALID_OPERATION;
        return;
    }

    /* Keep the source for glGetProgramStringARB-style queries and for logging. */
    if (sh->source) { free(sh->source); sh->source = NULL; }
    if (string && len > 0) {
        sh->source = (char *)malloc((size_t)len + 1);
        if (sh->source) {
            memcpy(sh->source, string, (size_t)len);
            sh->source[len] = '\0';
        }
    }

    /* A retranslation replaces whatever the previous string produced. */
    _glsReleaseARBProgram(sh);
    sh->compiled = FALSE;
    sh->type = target;

    if (!string || len <= 0) {
        gldDiagLog("GL: ProgramStringARB translate failed: empty program string");
        s->lastError = GL_INVALID_OPERATION;
        return;
    }
    if (!pDev) {
        gldDiagLog("GL: ProgramStringARB translate failed: no D3D9 device");
        return;
    }
    if (!glslTranspilerInit()) {
        gldDiagLog("GL: ProgramStringARB translate failed: "
                   "shader compiler unavailable (d3dcompiler_47.dll missing)");
        return;
    }

    glsl = (char *)malloc(GLS_ARB_GLSL_BUFFER);
    if (!glsl) return;

    if (!arbTranslateProgram((const char *)string, len, glsl, GLS_ARB_GLSL_BUFFER, &info)) {
        gldDiagLog("GL: ProgramStringARB translate failed: %s", info.error);
        if (info.notes[0]) gldDiagLogV("GL: ProgramStringARB notes: %s", info.notes);
        s->lastError = GL_INVALID_OPERATION;
        free(glsl);
        return;
    }
    if (info.notes[0])
        gldDiagLogV("GL: ProgramStringARB notes: %s", info.notes);

    if ((info.target == ARB_TARGET_VERTEX) != (isVertexStage ? TRUE : FALSE)) {
        gldDiagLog("GL: ProgramStringARB translate failed: program header is a %s "
                   "program but the bound target is 0x%X",
                   info.target == ARB_TARGET_VERTEX ? "vertex" : "fragment", target);
        s->lastError = GL_INVALID_OPERATION;
        free(glsl);
        return;
    }

    if (!glslTranspileAndCompile(isVertexStage ? 0 : 1, glsl, &code, &codeSize)) {
        gldDiagLog("GL: ProgramStringARB translate failed: "
                   "generated GLSL did not compile to Shader Model 3 "
                   "(see gldirect.log for the transpiler/D3DCompile diagnostic)");
        gldDiagLogV("GL: --- generated GLSL for %s program %u ---",
                   isVertexStage ? "vertex" : "fragment", *binding);
        gldDiagLogV("%.2000s", glsl ? glsl : "(null)");
        gldDiagLogV("GL: --- end generated GLSL ---");
        s->lastError = GL_INVALID_OPERATION;
        free(glsl);
        return;
    }

    sh->arb = (GLS_ARBProgram *)calloc(1, sizeof(GLS_ARBProgram));
    if (!sh->arb) { glslFreeBytecode(code); free(glsl); return; }
    sh->arb->envBaseReg = sh->arb->localBaseReg = sh->arb->stateBaseReg = -1;

    if (isVertexStage) {
        if (!glslCreateVertexShader(pDev, code, codeSize, &sh->arb->pVS)) {
            gldDiagLog("GL: ProgramStringARB translate failed: CreateVertexShader rejected the bytecode");
            glslFreeBytecode(code);
            _glsReleaseARBProgram(sh);
            free(glsl);
            return;
        }
    } else {
        if (!glslCreatePixelShader(pDev, code, codeSize, &sh->arb->pPS)) {
            gldDiagLog("GL: ProgramStringARB translate failed: CreatePixelShader rejected the bytecode");
            glslFreeBytecode(code);
            _glsReleaseARBProgram(sh);
            free(glsl);
            return;
        }
    }

    mapCount = glslReflectConstants(code, codeSize, map, GLSL_MAX_UNIFORM_MAP);
    _glsFindARBArray(map, mapCount, ARB_ENV_UNIFORM_NAME,
                     &sh->arb->envBaseReg, &sh->arb->envRegCount);
    _glsFindARBArray(map, mapCount, ARB_LOCAL_UNIFORM_NAME,
                     &sh->arb->localBaseReg, &sh->arb->localRegCount);
    _glsFindARBArray(map, mapCount, ARB_STATE_UNIFORM_NAME,
                     &sh->arb->stateBaseReg, &sh->arb->stateRegCount);
    sh->arb->usesStateMatrices = info.usesStateMatrices;
    sh->arb->usesStateLight    = info.usesStateLight;
    sh->arb->usesStateFog      = info.usesStateFog;

    glslFreeBytecode(code);
    free(glsl);

    sh->compiled = TRUE;
    gldDiagLogV("GL: ProgramStringARB program %u -> %s shader %p "
               "(env base=%d count=%d, local base=%d count=%d, state base=%d count=%d)",
               *binding, isVertexStage ? "vertex" : "pixel",
               isVertexStage ? (void *)sh->arb->pVS : (void *)sh->arb->pPS,
               sh->arb->envBaseReg, sh->arb->envRegCount,
               sh->arb->localBaseReg, sh->arb->localRegCount,
               sh->arb->stateBaseReg, sh->arb->stateRegCount);

    if (_glsARBStageEnabled(s, target))
        _glsApplyARBProgramBinding(target);
}

/* ---- state.* bindings: GL state -> ARB constant registers ---------------- */

/*
 * Write one GL column-major 4x4 into four consecutive float4 registers as the
 * matrix's *rows*, which is what state.matrix.<m>.row[i] means: an ARB program
 * transforms a position with four DP4s against those rows.
 */
static void _glsMatrixRowsToRegs(float *dst, const float *m)
{
    int r, cidx;
    for (r = 0; r < 4; r++)
        for (cidx = 0; cidx < 4; cidx++)
            dst[r * 4 + cidx] = m[cidx * 4 + r];
}

static void _glsMatrixTranspose(float *out, const float *m)
{
    int r, cidx;
    for (cidx = 0; cidx < 4; cidx++)
        for (r = 0; r < 4; r++)
            out[cidx * 4 + r] = m[r * 4 + cidx];
}

/* General 4x4 inverse.  Falls back to the identity for a singular matrix,
 * which is the only defined answer available and keeps the program running. */
static void _glsMatrixInverse(float *out, const float *m)
{
    float inv[16], det;
    int i;

    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0.0f) { glsMatrixIdentity(out); return; }
    det = 1.0f / det;
    for (i = 0; i < 16; i++) out[i] = inv[i] * det;
}

/* Fill the four register slots of one matrix family: base, inverse,
 * transpose, inverse-transpose — the four spellings ARB allows. */
static void _glsFillMatrixFamily(float *regs, int slot, const float *m)
{
    float tmp[16];
    _glsMatrixRowsToRegs(regs + (slot + 0) * 16, m);
    _glsMatrixInverse(tmp, m);
    _glsMatrixRowsToRegs(regs + (slot + 1) * 16, tmp);
    _glsMatrixTranspose(tmp, m);
    _glsMatrixRowsToRegs(regs + (slot + 2) * 16, tmp);
    _glsMatrixInverse(tmp, m);
    {
        float tmp2[16];
        _glsMatrixTranspose(tmp2, tmp);
        _glsMatrixRowsToRegs(regs + (slot + 3) * 16, tmp2);
    }
}

/*
 * Build the whole arb_state[] block from tracked GL state.
 *
 * These are not source-level uniforms an application ever names, so there is
 * nothing for the constant-table reflection to match them against by name;
 * they are computed here and pushed at the register the translator's fixed
 * layout assigns, in the same place the fixed-function transforms go up.
 */
static void _glsBuildARBStateRegs(GLS_State *s, float *regs)
{
    float mvp[16];
    int i;

    memset(regs, 0, ARB_STATE_REG_COUNT * 4 * sizeof(float));

    glsMatrixMultiply(mvp, s->projectionStack.stack[s->projectionStack.top].m,
                      s->modelviewStack.stack[s->modelviewStack.top].m);

    _glsFillMatrixFamily(regs, ARB_STATE_MAT_MVP,  mvp);
    _glsFillMatrixFamily(regs, ARB_STATE_MAT_MV,   s->modelviewStack.stack[s->modelviewStack.top].m);
    _glsFillMatrixFamily(regs, ARB_STATE_MAT_PROJ, s->projectionStack.stack[s->projectionStack.top].m);

    for (i = 0; i < GLS_MAX_TEX_UNITS && ARB_STATE_MAT_TEX0 + i < ARB_STATE_MAT_COUNT; i++)
        _glsMatrixRowsToRegs(regs + (ARB_STATE_MAT_TEX0 + i) * 16,
                             s->textureStack[i].stack[s->textureStack[i].top].m);

    for (i = 0; i < GLS_MAX_LIGHTS; i++) {
        memcpy(regs + (ARB_STATE_LIGHT0_POS + i) * 4,      s->lights[i].position, 4 * sizeof(float));
        memcpy(regs + (ARB_STATE_LIGHT0_AMBIENT + i) * 4,  s->lights[i].ambient,  4 * sizeof(float));
        memcpy(regs + (ARB_STATE_LIGHT0_DIFFUSE + i) * 4,  s->lights[i].diffuse,  4 * sizeof(float));
        memcpy(regs + (ARB_STATE_LIGHT0_SPECULAR + i) * 4, s->lights[i].specular, 4 * sizeof(float));
    }

    memcpy(regs + ARB_STATE_MAT_F_AMBIENT  * 4, s->materialFront.ambient,  4 * sizeof(float));
    memcpy(regs + ARB_STATE_MAT_F_DIFFUSE  * 4, s->materialFront.diffuse,  4 * sizeof(float));
    memcpy(regs + ARB_STATE_MAT_F_SPECULAR * 4, s->materialFront.specular, 4 * sizeof(float));
    memcpy(regs + ARB_STATE_MAT_F_EMISSION * 4, s->materialFront.emission, 4 * sizeof(float));
    regs[ARB_STATE_MAT_F_SHININESS * 4 + 0] = s->materialFront.shininess;

    memcpy(regs + ARB_STATE_FOG_COLOR * 4, s->fogColor, 4 * sizeof(float));
    regs[ARB_STATE_FOG_PARAMS * 4 + 0] = s->fogDensity;
    regs[ARB_STATE_FOG_PARAMS * 4 + 1] = s->fogStart;
    regs[ARB_STATE_FOG_PARAMS * 4 + 2] = s->fogEnd;
    regs[ARB_STATE_FOG_PARAMS * 4 + 3] =
        (s->fogEnd != s->fogStart) ? 1.0f / (s->fogEnd - s->fogStart) : 0.0f;

    memcpy(regs + ARB_STATE_LIGHTMODEL_AMB * 4, s->lightModelAmbient, 4 * sizeof(float));
}

static void _glsApplyARBStateParams(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Shader *vp, *fp;
    BOOL needVS, needFS;
    float regs[ARB_STATE_REG_COUNT * 4];

    if (!pDev) return;

    vp = s->enableVertexProgramARB   ? glsFindShader(s->boundVertexProgramARB)   : NULL;
    fp = s->enableFragmentProgramARB ? glsFindShader(s->boundFragmentProgramARB) : NULL;

    needVS = (vp && vp->arb && vp->compiled && vp->arb->stateBaseReg >= 0);
    needFS = (fp && fp->arb && fp->compiled && fp->arb->stateBaseReg >= 0);
    if (!needVS && !needFS) return;

    _glsBuildARBStateRegs(s, regs);

    __try {
        if (needVS)
            IDirect3DDevice9_SetVertexShaderConstantF(pDev, vp->arb->stateBaseReg,
                                                      regs, vp->arb->stateRegCount);
        if (needFS)
            IDirect3DDevice9_SetPixelShaderConstantF(pDev, fp->arb->stateBaseReg,
                                                     regs, fp->arb->stateRegCount);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/* Shared body for the env and local parameter setters. */
static void _glsSetARBParam(unsigned int target, unsigned int index,
                            const float *params, BOOL isEnv)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLuint_t *binding = _glsARBBindingFor(s, target);
    GLS_Shader *sh;
    BOOL isVertexStage = (target == GL_VERTEX_PROGRAM_ARB);
    int baseReg, regCount;
    float *slot;

    if (!params) return;
    if (!binding) {
        gldDiagLogV("GL: Program%sParameter4fv unknown target 0x%X",
                   isEnv ? "Env" : "Local", target);
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    if (index >= GLS_MAX_PROGRAM_PARAMS) {
        gldDiagLogV("GL: Program%sParameter4fv index %u exceeds the %d guaranteed by ARB",
                   isEnv ? "Env" : "Local", index, GLS_MAX_PROGRAM_PARAMS);
        s->lastError = GL_INVALID_VALUE;
        return;
    }

    sh = glsFindShader(*binding);
    if (!sh || !sh->arb) {
        /* Nothing to store it on yet.  Programs are routinely parameterised
         * before their string is supplied, so this is not an error — but the
         * value genuinely is not retained, and saying so beats pretending. */
        gldDiagLog("GL: Program%sParameter4fv(%u) ignored — no translated program "
                   "bound to target 0x%X", isEnv ? "Env" : "Local", index, target);
        return;
    }

    slot = isEnv ? sh->arb->envParams[index] : sh->arb->localParams[index];
    slot[0] = params[0]; slot[1] = params[1];
    slot[2] = params[2]; slot[3] = params[3];

    baseReg  = isEnv ? sh->arb->envBaseReg  : sh->arb->localBaseReg;
    regCount = isEnv ? sh->arb->envRegCount : sh->arb->localRegCount;

    /* Push immediately only while this program is the active device shader;
     * otherwise it goes up with the rest of the block at bind/enable time. */
    if (!pDev || baseReg < 0 || (int)index >= regCount) return;
    if (!_glsARBStageEnabled(s, target) || !sh->compiled) return;

    __try {
        if (isVertexStage)
            IDirect3DDevice9_SetVertexShaderConstantF(pDev, baseReg + (int)index, slot, 1);
        else
            IDirect3DDevice9_SetPixelShaderConstantF(pDev, baseReg + (int)index, slot, 1);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

void _glsProgramEnvParameter4fvARB(unsigned int target, unsigned int index, const float *params)
{
    _glsSetARBParam(target, index, params, TRUE);
}

void _glsProgramLocalParameter4fvARB(unsigned int target, unsigned int index, const float *params)
{
    _glsSetARBParam(target, index, params, FALSE);
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
        _glsReleaseARBProgram(sh);
        sh->allocated = FALSE;
        if (s->boundVertexProgramARB == obj)   s->boundVertexProgramARB = 0;
        if (s->boundFragmentProgramARB == obj) s->boundFragmentProgramARB = 0;
        return;
    }
    GLS_Program *prog = glsFindProgram(obj);
    if (prog) {
        if (s->boundProgram == obj) s->boundProgram = 0;
        _glsReleaseProgramShaders(prog);
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

/*
 * glMultiTexCoord{1,3,4}f — the same unit decode as the 2-component form.
 *
 * Components the caller does not supply take GL's defaults (r = 0, q = 1), so
 * every arity writes a complete 4-vector and glGet and any shader reading the
 * attribute see spec-correct values for all eight units.
 *
 * The fixed-function rasterizer still only surfaces units 0 and 1 at two
 * components each: the two further texcoord sets GLS_D3DVertex carries are
 * reserved for ARB generic attributes 6/7 — see the note in gl_impl.h.  That
 * ceiling is unchanged by these entry points; it is the vertex format, not the
 * state capture, that limits it.
 */
void _glsMultiTexCoord1fARB(unsigned int target, float s)
{
    GLS_State *st = glsGetState();
    int unit = (target >= 0x84C0) ? (int)(target - 0x84C0) : 0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;
    st->currentTexCoord[unit][0] = s;
    st->currentTexCoord[unit][1] = 0.0f;
    st->currentTexCoord[unit][2] = 0.0f;
    st->currentTexCoord[unit][3] = 1.0f;
}

void _glsMultiTexCoord3fARB(unsigned int target, float s, float t, float r)
{
    GLS_State *st = glsGetState();
    int unit = (target >= 0x84C0) ? (int)(target - 0x84C0) : 0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;
    st->currentTexCoord[unit][0] = s;
    st->currentTexCoord[unit][1] = t;
    st->currentTexCoord[unit][2] = r;
    st->currentTexCoord[unit][3] = 1.0f;
}

void _glsMultiTexCoord4fARB(unsigned int target, float s, float t, float r, float q)
{
    GLS_State *st = glsGetState();
    int unit = (target >= 0x84C0) ? (int)(target - 0x84C0) : 0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;
    st->currentTexCoord[unit][0] = s;
    st->currentTexCoord[unit][1] = t;
    st->currentTexCoord[unit][2] = r;
    st->currentTexCoord[unit][3] = q;
}

void _glsMultiTexCoord1fvARB(unsigned int target, const float *v)
{
    if (v) _glsMultiTexCoord1fARB(target, v[0]);
}

void _glsMultiTexCoord3fvARB(unsigned int target, const float *v)
{
    if (v) _glsMultiTexCoord3fARB(target, v[0], v[1], v[2]);
}

void _glsMultiTexCoord4fvARB(unsigned int target, const float *v)
{
    if (v) _glsMultiTexCoord4fARB(target, v[0], v[1], v[2], v[3]);
}

/* ===================================================================
 *  SECTION 13b: Texture coordinate generation
 * =================================================================== */

/* GL_S/T/R/Q -> 0..3, or -1 for anything else. */
static int _glsTexGenCoordIndex(unsigned int coord)
{
    if (coord >= GL_S && coord <= GL_S + 3) return (int)(coord - GL_S);
    return -1;
}

static int _glsActiveTexUnit(GLS_State *s)
{
    int unit = (s->activeTexUnit >= GL_TEXTURE0) ? (int)(s->activeTexUnit - GL_TEXTURE0) : 0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    return unit;
}

/*
 * Push the texgen state of one unit onto the device.
 *
 * D3D9's fixed function generates texture coordinates through
 * D3DTSS_TEXCOORDINDEX, which offers exactly three generated sources: a sphere
 * map, the camera-space normal and the camera-space reflection vector.  GL's
 * GL_OBJECT_LINEAR and GL_EYE_LINEAR are arbitrary plane equations with no
 * fixed-function counterpart at all — they need a vertex shader — so they are
 * reported and the unit is left passing its own coordinates through.
 */
void _glsApplyTexGenState(int unit)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD tci;
    GLenum_t mode;
    BOOL anyEnabled;

    if (!pDev || unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;

    anyEnabled = (s->texGenEnabled[unit][0] || s->texGenEnabled[unit][1] ||
                  s->texGenEnabled[unit][2] || s->texGenEnabled[unit][3]);
    mode = s->texGenMode[unit][0];

    if (!anyEnabled) {
        tci = (DWORD)unit;                                  /* D3DTSS_TCI_PASSTHRU */
    } else {
        switch (mode) {
        case GL_SPHERE_MAP:
            tci = (DWORD)unit | D3DTSS_TCI_SPHEREMAP;
            break;
        case GL_NORMAL_MAP:
            tci = (DWORD)unit | D3DTSS_TCI_CAMERASPACENORMAL;
            break;
        case GL_REFLECTION_MAP:
            tci = (DWORD)unit | D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
            break;
        default:
            gldDiagLog("GL: TexGen mode 0x%X has no fixed-function D3D9 equivalent, "
                       "unit %d texcoord left unmodified", mode, unit);
            tci = (DWORD)unit;
            break;
        }
    }

    __try {
        IDirect3DDevice9_SetTextureStageState(pDev, (DWORD)unit, D3DTSS_TEXCOORDINDEX, tci);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

void _glsTexGeni(unsigned int coord, unsigned int pname, int param)
{
    GLS_State *s = glsGetState();
    int unit = _glsActiveTexUnit(s);
    int c = _glsTexGenCoordIndex(coord);

    if (c < 0) {
        gldDiagLogV("GL: glTexGen unknown coordinate 0x%X", coord);
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    if (pname != GL_TEXTURE_GEN_MODE) {
        gldDiagLogV("GL: glTexGeni pname 0x%X is not GL_TEXTURE_GEN_MODE", pname);
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    s->texGenMode[unit][c] = (GLenum_t)param;
    _glsApplyTexGenState(unit);
}

void _glsTexGenfv(unsigned int coord, unsigned int pname, const float *params)
{
    GLS_State *s = glsGetState();
    int unit = _glsActiveTexUnit(s);
    int c = _glsTexGenCoordIndex(coord);

    if (c < 0 || !params) {
        if (c < 0) s->lastError = GL_INVALID_ENUM;
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        s->texGenMode[unit][c] = (GLenum_t)params[0];
        _glsApplyTexGenState(unit);
        break;
    case GL_OBJECT_PLANE:
        memcpy(s->texGenObjectPlane[unit][c], params, 4 * sizeof(float));
        break;
    case GL_EYE_PLANE:
        memcpy(s->texGenEyePlane[unit][c], params, 4 * sizeof(float));
        break;
    default:
        gldDiagLogV("GL: glTexGenfv unknown pname 0x%X", pname);
        s->lastError = GL_INVALID_ENUM;
        break;
    }
}

void _glsGetTexGenfv(unsigned int coord, unsigned int pname, float *params)
{
    GLS_State *s = glsGetState();
    int unit = _glsActiveTexUnit(s);
    int c = _glsTexGenCoordIndex(coord);

    if (!params) return;
    if (c < 0) { s->lastError = GL_INVALID_ENUM; return; }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        params[0] = (float)s->texGenMode[unit][c];
        break;
    case GL_OBJECT_PLANE:
        memcpy(params, s->texGenObjectPlane[unit][c], 4 * sizeof(float));
        break;
    case GL_EYE_PLANE:
        memcpy(params, s->texGenEyePlane[unit][c], 4 * sizeof(float));
        break;
    default:
        params[0] = 0.0f;
        s->lastError = GL_INVALID_ENUM;
        break;
    }
}

void _glsGetTexGeniv(unsigned int coord, unsigned int pname, int *params)
{
    float f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int i, n;
    if (!params) return;
    _glsGetTexGenfv(coord, pname, f);
    n = (pname == GL_TEXTURE_GEN_MODE) ? 1 : 4;
    for (i = 0; i < n; i++) params[i] = (int)f[i];
}

/* ===================================================================
 *  SECTION 14: Stencil Two-Side
 * =================================================================== */

/*
 * glActiveStencilFaceEXT — choose which face the single-face stencil entry
 * points address from here on.
 *
 * GL_FRONT_AND_BACK (or never having called this at all) means both, which is
 * the plain GL 1.0 behaviour.  The device state itself is refreshed because
 * _glsApplyStencilState decides D3DRS_TWOSIDEDSTENCILMODE from whether the two
 * faces still agree.
 */
void _glsActiveStencilFaceEXT(unsigned int face)
{
    GLS_State *s = glsGetState();

    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
        gldDiagLog("GL: glActiveStencilFaceEXT unknown face 0x%X, ignored", face);
        s->lastError = GL_INVALID_ENUM;
        return;
    }

    s->activeStencilFace = face;
    _glsApplyStencilState();
    gldDiagLogV("GL: glActiveStencilFaceEXT(0x%X)", face);
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
    GLuint_t program = s->boundProgram;
    if (!program && s->boundProgramPipeline) {
        program = s->pipelineActiveProgram;
        if (!program) program = s->pipelineVertexProgram;
        if (!program) program = s->pipelineFragmentProgram;
        if (!program) program = s->pipelineComputeProgram;
    }
    return glsFindProgram(program);
}

/*
 * glGetUniformLocation — resolve a uniform name to a location.
 *
 * The location is an index into the program's resolved uniform table built
 * at link time from the shader constant tables, not a D3D9 register: the
 * same name usually occupies different registers in the vertex and pixel
 * shaders, so the indirection is what lets one glUniform call update both.
 */
int _glsGetUniformLocation(unsigned int program, const char *name)
{
    GLS_Program *prog = glsFindProgram(program);
    int i;

    if (!prog || !name) return -1;

    for (i = 0; i < prog->resolvedCount; i++) {
        if (strcmp(prog->resolved[i].name, name) == 0) {
            gldDiagLogV("GL: glGetUniformLocation(%u, \"%s\") -> %d (vs=%d ps=%d)",
                       program, name, i,
                       prog->resolved[i].vsRegister, prog->resolved[i].psRegister);
            return i;
        }
    }

    gldDiagLogV("GL: glGetUniformLocation(%u, \"%s\") -> -1 (not found)", program, name);
    return -1;
}

/*
 * glGetAttribLocation — resolve a vertex attribute name to its index.
 *
 * Honours any glBindAttribLocation the application made; otherwise falls back
 * to the fixed-function aliasing the vertex assembly path reads from.
 */
int _glsGetAttribLocation(unsigned int program, const char *name)
{
    GLS_Program *prog = glsFindProgram(program);
    int i;

    if (!prog || !name) return -1;

    for (i = 0; i < prog->attribBindingCount; i++) {
        if (prog->attribBindings[i].set &&
            strcmp(prog->attribBindings[i].name, name) == 0)
            return (int)prog->attribBindings[i].index;
    }

    if (strstr(name, "ertex") || strstr(name, "osition")) return 0;
    if (strstr(name, "ormal"))                            return 2;
    if (strstr(name, "olor") || strstr(name, "olour"))    return 3;
    if (strstr(name, "exCoord") || strstr(name, "excoord")) return 8;

    return -1;
}

/*
 * Push a uniform's value into the D3D9 constant registers of whichever
 * stages declare it.
 *
 * Sampler uniforms are handled separately: D3D9 pins a sampler to its
 * register, so glUniform1i on a sampler is recorded as a stage->GL-unit
 * mapping rather than uploaded as a constant.
 */
static void _glsUploadUniform(int loc, const float *data, int vec4Count)
{
    GLS_State *s = glsGetState();
    GLS_Program *prog = _getBoundProgram();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_ResolvedUniform *u;

    if (!prog || !pDev || loc < 0 || loc >= prog->resolvedCount) return;
    if (!data || vec4Count <= 0) return;

    u = &prog->resolved[loc];

    if (u->registerSet == GLSL_RS_SAMPLER) {
        int unit = (int)data[0];
        if (u->psRegister >= 0 && u->psRegister < GLS_MAX_TEX_UNITS &&
            unit >= 0 && unit < GLS_MAX_TEX_UNITS)
            s->samplerStageUnit[u->psRegister] = unit;
        gldDiagLogV("GL: uniform sampler loc=%d stage=%d <- GL unit %d",
                   loc, u->psRegister, unit);
        return;
    }

    /* Never write past the registers this uniform actually owns.
     *
     * registerCount comes from the compiled shader's constant table, so it is
     * what the shader really reserved; the count on this call comes from the
     * application, which is free to pass a larger one — glUniform4fv with a
     * count the shader disagrees with is an application error GL simply
     * ignores, not something GL is allowed to answer by overwriting the next
     * uniform's registers.  Conditioned on registerCount > 0 so a uniform the
     * reflection never resolved is left exactly as it behaves today. */
    if (u->registerCount > 0 && vec4Count > u->registerCount) {
        gldDiagLogV("GL: uniform loc=%d upload of %d vec4 clamped to %d (shader's "
                    "register count)", loc, vec4Count, u->registerCount);
        vec4Count = u->registerCount;
    }

    __try {
        if (u->vsRegister >= 0)
            IDirect3DDevice9_SetVertexShaderConstantF(pDev, u->vsRegister, data, vec4Count);
        if (u->psRegister >= 0)
            IDirect3DDevice9_SetPixelShaderConstantF(pDev, u->psRegister, data, vec4Count);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/* Upload `count` scalars/vectors of `components` each, padding to float4
 * because D3D9 constant registers are always four floats wide. */
static void _glsUploadScalars(int loc, const float *v, int count, int components)
{
    float packed[4 * 64];
    int i, c, n;

    if (!v || count <= 0) return;
    if (count > 64) count = 64;

    n = count * 4;
    memset(packed, 0, (size_t)n * sizeof(float));

    for (i = 0; i < count; i++)
        for (c = 0; c < components; c++)
            packed[i * 4 + c] = v[i * components + c];

    _glsUploadUniform(loc, packed, count);
}

/* ===== glUniform* =====
 *
 * Each entry records the value for glGetUniform queries and uploads it to
 * the bound program's D3D9 constant registers.  Integer uniforms are
 * converted to float because D3D9's integer constant file is tiny and
 * rarely supported; the only integers that matter in practice are sampler
 * bindings, which _glsUploadUniform intercepts.
 */

void _glsUniform1i(int loc, int v0)
{
    float f[1];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->set = TRUE; }
    f[0] = (float)v0;
    _glsUploadScalars(loc, f, 1, 1);
}

void _glsUniform2i(int loc, int v0, int v1)
{
    float f[2];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->set = TRUE; }
    f[0] = (float)v0; f[1] = (float)v1;
    _glsUploadScalars(loc, f, 1, 2);
}

void _glsUniform3i(int loc, int v0, int v1, int v2)
{
    float f[3];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->data[2] = (float)v2; u->set = TRUE; }
    f[0] = (float)v0; f[1] = (float)v1; f[2] = (float)v2;
    _glsUploadScalars(loc, f, 1, 3);
}

void _glsUniform4i(int loc, int v0, int v1, int v2, int v3)
{
    float f[4];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 0; u->data[0] = (float)v0; u->data[1] = (float)v1; u->data[2] = (float)v2; u->data[3] = (float)v3; u->set = TRUE; }
    f[0] = (float)v0; f[1] = (float)v1; f[2] = (float)v2; f[3] = (float)v3;
    _glsUploadScalars(loc, f, 1, 4);
}

void _glsUniform1f(int loc, float v0)
{
    float f[1];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 1; u->data[0] = v0; u->set = TRUE; }
    f[0] = v0;
    _glsUploadScalars(loc, f, 1, 1);
}

void _glsUniform2f(int loc, float v0, float v1)
{
    float f[2];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 2; u->data[0] = v0; u->data[1] = v1; u->set = TRUE; }
    f[0] = v0; f[1] = v1;
    _glsUploadScalars(loc, f, 1, 2);
}

void _glsUniform3f(int loc, float v0, float v1, float v2)
{
    float f[3];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 3; u->data[0] = v0; u->data[1] = v1; u->data[2] = v2; u->set = TRUE; }
    f[0] = v0; f[1] = v1; f[2] = v2;
    _glsUploadScalars(loc, f, 1, 3);
}

void _glsUniform4f(int loc, float v0, float v1, float v2, float v3)
{
    float f[4];
    GLS_Uniform *u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 4; u->data[0] = v0; u->data[1] = v1; u->data[2] = v2; u->data[3] = v3; u->set = TRUE; }
    f[0] = v0; f[1] = v1; f[2] = v2; f[3] = v3;
    _glsUploadScalars(loc, f, 1, 4);
}

void _glsUniform1iv(int loc, int count, const int *v)
{
    float f[64];
    int i, n;
    if (!v || count <= 0) return;
    n = count > 64 ? 64 : count;
    for (i = 0; i < n; i++) f[i] = (float)v[i];
    _glsUploadScalars(loc, f, n, 1);
}

void _glsUniform2iv(int loc, int count, const int *v)
{
    float f[128];
    int i, n;
    if (!v || count <= 0) return;
    n = count > 64 ? 64 : count;
    for (i = 0; i < n * 2; i++) f[i] = (float)v[i];
    _glsUploadScalars(loc, f, n, 2);
}

void _glsUniform3iv(int loc, int count, const int *v)
{
    float f[192];
    int i, n;
    if (!v || count <= 0) return;
    n = count > 64 ? 64 : count;
    for (i = 0; i < n * 3; i++) f[i] = (float)v[i];
    _glsUploadScalars(loc, f, n, 3);
}

void _glsUniform4iv(int loc, int count, const int *v)
{
    float f[256];
    int i, n;
    if (!v || count <= 0) return;
    n = count > 64 ? 64 : count;
    for (i = 0; i < n * 4; i++) f[i] = (float)v[i];
    _glsUploadScalars(loc, f, n, 4);
}

void _glsUniform1fv(int loc, int count, const float *v)
{
    if (!v || count <= 0) return;
    _glsUploadScalars(loc, v, count, 1);
}

void _glsUniform2fv(int loc, int count, const float *v)
{
    if (!v || count <= 0) return;
    _glsUploadScalars(loc, v, count, 2);
}

void _glsUniform3fv(int loc, int count, const float *v)
{
    if (!v || count <= 0) return;
    _glsUploadScalars(loc, v, count, 3);
}

void _glsUniform4fv(int loc, int count, const float *v)
{
    /* Already float4-aligned — upload straight through. */
    if (!v || count <= 0) return;
    _glsUploadUniform(loc, v, count);
}

/*
 * Matrix uniforms.
 *
 * GL hands over column-major data when transpose is FALSE, which is exactly
 * what HLSL's default column-major packing expects, so that case is a
 * straight copy.  transpose = TRUE means the application supplied row-major
 * data and asked the driver to flip it.
 *
 * Each matrix occupies one constant register per column, and a mat3 still
 * costs three float4 registers because registers cannot be split.
 */
static void _glsUploadMatrices(int loc, int count, unsigned char transpose,
                               const float *v, int dim)
{
    float packed[4 * 16];
    int m, r, c, reg = 0;

    if (!v || count <= 0) return;
    if (count > 16 / dim) count = 16 / dim;

    memset(packed, 0, sizeof(packed));

    for (m = 0; m < count; m++) {
        const float *src = v + m * dim * dim;
        for (c = 0; c < dim; c++) {
            for (r = 0; r < dim; r++) {
                /* src is column-major unless the caller asked for a flip */
                packed[reg * 4 + r] = transpose ? src[r * dim + c]
                                                : src[c * dim + r];
            }
            reg++;
        }
    }

    _glsUploadUniform(loc, packed, reg);
}

void _glsUniformMatrix2fv(int loc, int count, unsigned char transpose, const float *v)
{
    _glsUploadMatrices(loc, count, transpose, v, 2);
}

void _glsUniformMatrix3fv(int loc, int count, unsigned char transpose, const float *v)
{
    _glsUploadMatrices(loc, count, transpose, v, 3);
}

void _glsUniformMatrix4fv(int loc, int count, unsigned char transpose, const float *v)
{
    GLS_Uniform *u;
    if (!v || count <= 0) return;
    u = _findOrCreateUniform(_getBoundProgram(), loc);
    if (u) { u->type = 7; memcpy(u->data, v, 16 * sizeof(float)); u->set = TRUE; }
    _glsUploadMatrices(loc, count, transpose, v, 4);
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

/* ===== Legacy client-side vertex arrays (GL 1.1) =====
 *
 * These record where the application's arrays live; _glsResolveVertexSources
 * picks them up at draw time when the bound VAO does not supply the same
 * semantic.  A buffer object bound to GL_ARRAY_BUFFER at the time of the call
 * makes `pointer` an offset into it, matching the VBO rules for generic
 * attributes.
 */

static void _glsSetClientArray(GLS_ClientArray *arr, int size, unsigned int type,
                               int stride, const void *pointer)
{
    GLS_State *s = glsGetState();
    arr->size          = size;
    arr->type          = type;
    arr->stride        = stride;
    arr->pointer       = pointer;
    arr->bufferBinding = s->boundArrayBuffer;
}

void _glsVertexPointer(int size, unsigned int type, int stride, const void *pointer)
{
    _glsSetClientArray(&glsGetState()->clientVertexArray, size, type, stride, pointer);
    gldDiagLogV("GL: glVertexPointer(%d, 0x%X, %d, %p)", size, type, stride, pointer);
}

void _glsNormalPointer(unsigned int type, int stride, const void *pointer)
{
    /* glNormalPointer has no size argument — normals are always 3 components. */
    _glsSetClientArray(&glsGetState()->clientNormalArray, 3, type, stride, pointer);
    gldDiagLogV("GL: glNormalPointer(0x%X, %d, %p)", type, stride, pointer);
}

void _glsColorPointer(int size, unsigned int type, int stride, const void *pointer)
{
    _glsSetClientArray(&glsGetState()->clientColorArray, size, type, stride, pointer);
    gldDiagLogV("GL: glColorPointer(%d, 0x%X, %d, %p)", size, type, stride, pointer);
}

void _glsTexCoordPointer(int size, unsigned int type, int stride, const void *pointer)
{
    GLS_State *s = glsGetState();
    int unit = (int)s->clientActiveTexUnit - GL_TEXTURE0;
    if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
    _glsSetClientArray(&s->clientTexCoordArray[unit], size, type, stride, pointer);
    gldDiagLogV("GL: glTexCoordPointer(%d, 0x%X, %d, %p) unit=%d", size, type, stride, pointer, unit);
}

void _glsClientActiveTexture(unsigned int texture)
{
    GLS_State *s = glsGetState();
    s->clientActiveTexUnit = texture;
}

static void _glsSetClientState(unsigned int array, BOOL enable)
{
    GLS_State *s = glsGetState();
    int unit;

    switch (array) {
    case GL_VERTEX_ARRAY:
        s->clientVertexArray.enabled = enable;
        break;
    case GL_NORMAL_ARRAY:
        s->clientNormalArray.enabled = enable;
        break;
    case GL_COLOR_ARRAY:
        s->clientColorArray.enabled = enable;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        unit = (int)s->clientActiveTexUnit - GL_TEXTURE0;
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        s->clientTexCoordArray[unit].enabled = enable;
        break;
    default:
        gldDiagLogV("GL: %sClientState unhandled array 0x%X",
                   enable ? "glEnable" : "glDisable", array);
        break;
    }
}

void _glsEnableClientState(unsigned int array)  { _glsSetClientState(array, TRUE); }
void _glsDisableClientState(unsigned int array) { _glsSetClientState(array, FALSE); }

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

/* GL blend equation -> D3DBLENDOP. */
static D3DBLENDOP _glsMapBlendEquation(unsigned int mode)
{
    switch (mode) {
    case 0x8006: return D3DBLENDOP_ADD;          /* GL_FUNC_ADD */
    case 0x800A: return D3DBLENDOP_SUBTRACT;     /* GL_FUNC_SUBTRACT */
    case 0x800B: return D3DBLENDOP_REVSUBTRACT;  /* GL_FUNC_REVERSE_SUBTRACT */
    case 0x8007: return D3DBLENDOP_MIN;          /* GL_MIN */
    case 0x8008: return D3DBLENDOP_MAX;          /* GL_MAX */
    default:     return D3DBLENDOP_ADD;
    }
}

void _glsBlendEquationSeparate(unsigned int modeRGB, unsigned int modeAlpha)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    s->blendEquationRGB = modeRGB;
    s->blendEquationAlpha = modeAlpha;

    if (!pDev) return;

    __try {
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_BLENDOP, _glsMapBlendEquation(modeRGB));
        if (modeAlpha != modeRGB) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_BLENDOPALPHA,
                                            _glsMapBlendEquation(modeAlpha));
        } else {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
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
    _glsApplyStencilState();
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
    _glsApplyStencilState();
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
    _glsApplyStencilState();
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
    return glsFindFBO(s->boundDrawFBO);
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

/*
 * Resolve a framebuffer name to the D3D9 colour surface it draws to.
 *
 * Name 0 is the default framebuffer, i.e. the device's current render target.
 * A user framebuffer resolves through its first colour attachment's texture
 * level.  The caller releases the returned surface.
 */
static IDirect3DSurface9 *_glsFBOColorSurface(GLuint_t name, int *pWidth, int *pHeight)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DSurface9 *pSurf = NULL;
    D3DSURFACE_DESC desc;
    HRESULT hr;

    if (!pDev) return NULL;

    if (name == 0) {
        __try {
            hr = IDirect3DDevice9_GetRenderTarget(pDev, 0, &pSurf);
            if (SUCCEEDED(hr)) _glsSurfAcquired(pSurf, "FBOColorSurface/GetRenderTarget");
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return NULL;
        }
        if (FAILED(hr) || !pSurf) return NULL;
    } else {
        GLS_FBO *fbo = glsFindFBO(name);
        GLS_Texture *tex;
        if (!fbo) return NULL;
        if (!fbo->colorAttachment[0]) {
            gldDiagLog("GL: framebuffer %u has no colour texture attachment "
                       "(renderbuffer-only attachments have no D3D9 surface here)", name);
            return NULL;
        }
        tex = glsFindTexture(fbo->colorAttachment[0]);
        if (!tex || !tex->pTex) {
            gldDiagLog("GL: framebuffer %u colour attachment has no D3D9 texture", name);
            return NULL;
        }
        __try {
            hr = IDirect3DTexture9_GetSurfaceLevel(tex->pTex,
                                                   (UINT)fbo->colorAttachLevel[0], &pSurf);
                                                   if (SUCCEEDED(hr)) _glsSurfAcquired(pSurf, "FBOColorSurface/GetSurfaceLevel");
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return NULL;
        }
        if (FAILED(hr) || !pSurf) return NULL;
    }

    if (SUCCEEDED(IDirect3DSurface9_GetDesc(pSurf, &desc))) {
        if (pWidth)  *pWidth  = (int)desc.Width;
        if (pHeight) *pHeight = (int)desc.Height;
    }
    return pSurf;
}

/*
 * glBlitFramebuffer — rectangle copy between the read and draw framebuffers.
 *
 * StretchRect is D3D9's equivalent and handles the scale and the filter, but
 * it only accepts D3DPOOL_DEFAULT surfaces.  Texture attachments created by
 * glTexImage2D live in D3DPOOL_MANAGED, so a blit involving one is refused by
 * the runtime; that refusal is reported with its HRESULT rather than swallowed,
 * because the alternative — silently drawing nothing — is what makes this class
 * of bug invisible.
 */
void _glsBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1,
                          int dstX0, int dstY0, int dstX1, int dstY1,
                          unsigned int mask, unsigned int filter)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DSurface9 *pSrc = NULL, *pDst = NULL;
    int srcW = 0, srcH = 0, dstW = 0, dstH = 0;
    RECT srcRect, dstRect;
    HRESULT hr;

    if (!pDev) return;

    if (!(mask & GL_COLOR_BUFFER_BIT)) {
        gldDiagLog("GL: glBlitFramebuffer mask 0x%X has no colour bit; D3D9 StretchRect "
                   "cannot move depth or stencil between surfaces, nothing blitted", mask);
        return;
    }
    if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
        gldDiagLog("GL: glBlitFramebuffer depth/stencil bits in mask 0x%X ignored — "
                   "D3D9 has no surface-to-surface depth copy", mask);

    pSrc = _glsFBOColorSurface(s->boundReadFBO, &srcW, &srcH);
    pDst = _glsFBOColorSurface(s->boundDrawFBO, &dstW, &dstH);
    if (!pSrc || !pDst) {
        gldDiagLog("GL: glBlitFramebuffer read=%u draw=%u — one side has no usable "
                   "colour surface, nothing blitted", s->boundReadFBO, s->boundDrawFBO);
        if (pSrc) _glsSurfRel(pSrc);
        if (pDst) _glsSurfRel(pDst);
        return;
    }
    if (pSrc == pDst) {
        gldDiagLogV("GL: glBlitFramebuffer source and destination are the same surface; "
                   "D3D9 StretchRect forbids that, nothing blitted");
        _glsSurfRel(pSrc);
        _glsSurfRel(pDst);
        return;
    }

    /* GL rectangles are bottom-up and may be given in either order. */
    srcRect.left   = (srcX0 < srcX1) ? srcX0 : srcX1;
    srcRect.right  = (srcX0 < srcX1) ? srcX1 : srcX0;
    srcRect.top    = srcH - ((srcY0 > srcY1) ? srcY0 : srcY1);
    srcRect.bottom = srcH - ((srcY0 < srcY1) ? srcY0 : srcY1);
    dstRect.left   = (dstX0 < dstX1) ? dstX0 : dstX1;
    dstRect.right  = (dstX0 < dstX1) ? dstX1 : dstX0;
    dstRect.top    = dstH - ((dstY0 > dstY1) ? dstY0 : dstY1);
    dstRect.bottom = dstH - ((dstY0 < dstY1) ? dstY0 : dstY1);

    __try {
        hr = IDirect3DDevice9_StretchRect(pDev, pSrc, &srcRect, pDst, &dstRect,
                                          (filter == GL_LINEAR) ? D3DTEXF_LINEAR
                                                                : D3DTEXF_POINT);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (FAILED(hr))
        gldDiagLog("GL: glBlitFramebuffer StretchRect failed (hr=0x%08X) — the surfaces "
                   "are probably not both in D3DPOOL_DEFAULT; nothing blitted",
                   (unsigned)hr);
    else
        gldDiagLogV("GL: glBlitFramebuffer read=%u draw=%u (%d,%d)-(%d,%d) -> (%d,%d)-(%d,%d)",
                   s->boundReadFBO, s->boundDrawFBO,
                   srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1);

    _glsSurfRel(pSrc);
    _glsSurfRel(pDst);
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
    const unsigned int valid = 0x00FF; /* every GL_MAP_* bit through COHERENT */
    if (!buf) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    if (offset < 0 || length <= 0 || offset > buf->size || length > buf->size - offset) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return NULL;
    }
    if ((access & ~valid) || !(access & 0x0003) ||
        ((access & 0x0001) && (access & (0x0004 | 0x0008 | 0x0020))) ||
        ((access & 0x0010) && !(access & 0x0002))) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    if (buf->mapped || !buf->data) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    if ((access & (0x0040 | 0x0080)) &&
        (!buf->immutable || (access & ~buf->storageFlags & (0x0040 | 0x0080)))) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return NULL;
    }
    buf->mapped = TRUE;
    buf->mapOffset = offset;
    buf->mapLength = length;
    buf->mapAccess = access;
    return (char*)buf->data + offset;
}

void _glsFlushMappedBufferRange(unsigned int target, ptrdiff_t offset, ptrdiff_t length)
{
    GLS_Buffer *buf = _getBoundBuffer(target);
    if (!buf || !buf->mapped || !(buf->mapAccess & 0x0010)) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }
    if (offset < 0 || length < 0 || offset > buf->mapLength ||
        length > buf->mapLength - offset) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }
    /* Mapped storage is the authoritative CPU shadow.  D3D9 vertex/index
     * resources are populated from that shadow at draw time, so the flushed
     * bytes are already visible and require no device-side copy here. */
}

/*
 * Build one mip level by box-filtering the level above it, in place, through
 * locked surfaces.
 *
 * StretchRect would be the obvious tool, but it refuses D3DPOOL_MANAGED
 * surfaces, and every texture here is managed (that pool is what makes them
 * survive a device reset and what makes glGetTexImage possible without a
 * render-target round trip).  Filtering on the CPU works for any managed
 * format the converter already understands and needs no device capability.
 */
static BOOL _glsDownsampleLevel(GLS_Texture *tex, unsigned int target, int dstLevel)
{
    D3DLOCKED_RECT srcLR, dstLR;
    D3DSURFACE_DESC srcDesc, dstDesc;
    int bpp, x, y;
    HRESULT hr;

    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)dstLevel, &dstDesc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)dstLevel, &dstDesc);
    if (FAILED(hr)) return FALSE;
    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)(dstLevel - 1), &srcDesc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)(dstLevel - 1), &srcDesc);
    if (FAILED(hr)) return FALSE;

    bpp = _glsDestBytesPerPixel(dstDesc.Format);
    if (bpp == 0) {
        gldDiagLog("GL: glGenerateMipmap cannot filter D3DFMT %d (compressed or "
                   "unconvertible), levels below %d left as they were",
                   (int)dstDesc.Format, dstLevel);
        return FALSE;
    }

    if (!_glsLockTexLevel(tex, target, dstLevel - 1, NULL, &srcLR)) return FALSE;
    if (!_glsLockTexLevel(tex, target, dstLevel, NULL, &dstLR)) {
        _glsUnlockTexLevel(tex, target, dstLevel - 1);
        return FALSE;
    }

    for (y = 0; y < (int)dstDesc.Height; y++) {
        for (x = 0; x < (int)dstDesc.Width; x++) {
            int sx0 = x * 2, sy0 = y * 2;
            int sx1 = (sx0 + 1 < (int)srcDesc.Width)  ? sx0 + 1 : sx0;
            int sy1 = (sy0 + 1 < (int)srcDesc.Height) ? sy0 + 1 : sy0;
            unsigned char a[4], b[4], c[4], d[4], out[4];
            int k;

            _glsDecodeSurfacePixel((const unsigned char *)srcLR.pBits
                                   + (ptrdiff_t)sy0 * srcLR.Pitch + (ptrdiff_t)sx0 * bpp,
                                   srcDesc.Format, a);
            _glsDecodeSurfacePixel((const unsigned char *)srcLR.pBits
                                   + (ptrdiff_t)sy0 * srcLR.Pitch + (ptrdiff_t)sx1 * bpp,
                                   srcDesc.Format, b);
            _glsDecodeSurfacePixel((const unsigned char *)srcLR.pBits
                                   + (ptrdiff_t)sy1 * srcLR.Pitch + (ptrdiff_t)sx0 * bpp,
                                   srcDesc.Format, c);
            _glsDecodeSurfacePixel((const unsigned char *)srcLR.pBits
                                   + (ptrdiff_t)sy1 * srcLR.Pitch + (ptrdiff_t)sx1 * bpp,
                                   srcDesc.Format, d);
            for (k = 0; k < 4; k++)
                out[k] = (unsigned char)(((int)a[k] + b[k] + c[k] + d[k] + 2) / 4);

            _glsEncodePixel((unsigned char *)dstLR.pBits
                            + (ptrdiff_t)y * dstLR.Pitch + (ptrdiff_t)x * bpp,
                            dstDesc.Format, out);
        }
    }

    _glsUnlockTexLevel(tex, target, dstLevel);
    _glsUnlockTexLevel(tex, target, dstLevel - 1);
    return TRUE;
}

void _glsGenerateMipmap(unsigned int target)
{
    GLS_Texture *tex;
    int unit, level, levelCount;
    unsigned int faceTarget;
    int face;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) {
        gldDiagLogV("GL: glGenerateMipmap(0x%X) with no texture bound", target);
        return;
    }
    if (tex->pVolTex) {
        gldDiagLog("GL: glGenerateMipmap on a 3D texture is not supported, "
                   "levels left as they were");
        return;
    }
    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: glGenerateMipmap tex=%u has no storage", tex->id);
        return;
    }

    levelCount = tex->pTex
               ? (int)IDirect3DTexture9_GetLevelCount(tex->pTex)
               : (int)IDirect3DCubeTexture9_GetLevelCount(tex->pCubeTex);
    if (levelCount <= 1) {
        gldDiagLogV("GL: glGenerateMipmap tex=%u has a single level, nothing to build", tex->id);
        return;
    }

    if (tex->pCubeTex) {
        for (face = 0; face < 6; face++) {
            faceTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (unsigned int)face;
            for (level = 1; level < levelCount; level++)
                if (!_glsDownsampleLevel(tex, faceTarget, level))
                    break;
        }
    } else {
        for (level = 1; level < levelCount; level++)
            if (!_glsDownsampleLevel(tex, target, level))
                break;
    }

    gldDiagLogV("GL: glGenerateMipmap tex=%u target=0x%X (%d levels)",
               tex->id, target, levelCount);
}

void _glsBindBufferBase(unsigned int target, unsigned int index, unsigned int buffer)
{
    GLS_State *s = glsGetState();
    GLS_IndexedBufferBinding *binding = NULL;
    GLS_Buffer *b = glsFindBuffer(buffer);
    if (index >= GLS_MAX_BUFFER_BINDINGS) {
        s->lastError = GL_INVALID_VALUE;
        return;
    }
    if (target == GL_UNIFORM_BUFFER) {
        s->boundUniformBuffer = buffer;
        binding = &s->uniformBindings[index];
    } else if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        s->boundTransformFeedbackBuffer = buffer;
        binding = &s->transformFeedbackBindings[index];
    } else if (target == 0x90D2) { /* GL_SHADER_STORAGE_BUFFER */
        s->boundShaderStorageBuffer = buffer;
        binding = &s->shaderStorageBindings[index];
    } else if (target == 0x92C0) { /* GL_ATOMIC_COUNTER_BUFFER */
        s->boundAtomicCounterBuffer = buffer;
        binding = &s->atomicCounterBindings[index];
    }
    if (!binding) {
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    binding->buffer = buffer;
    binding->offset = 0;
    binding->size = b ? b->size : 0;
    /* Also bind to the generic target */
    _glsBindBuffer(target, buffer);
}

void _glsBindBufferRange(unsigned int target, unsigned int index, unsigned int buffer, ptrdiff_t offset, ptrdiff_t size)
{
    GLS_State *s = glsGetState();
    GLS_IndexedBufferBinding *binding = NULL;
    GLS_Buffer *b = glsFindBuffer(buffer);
    if (offset < 0 || size < 0 || (b && (offset > b->size || size > b->size - offset))) {
        s->lastError = GL_INVALID_VALUE;
        return;
    }
    _glsBindBufferBase(target, index, buffer);
    if (index >= GLS_MAX_BUFFER_BINDINGS) return;
    if (target == GL_UNIFORM_BUFFER) binding = &s->uniformBindings[index];
    else if (target == GL_TRANSFORM_FEEDBACK_BUFFER) binding = &s->transformFeedbackBindings[index];
    else if (target == 0x90D2) binding = &s->shaderStorageBindings[index];
    else if (target == 0x92C0) binding = &s->atomicCounterBindings[index];
    if (binding) {
        binding->offset = offset;
        binding->size = size;
    }
}

/* ===================================================================
 *  SECTION 22: Transform Feedback
 * =================================================================== */

void _glsBeginTransformFeedback(unsigned int primitiveMode)
{
    GLS_State *s = glsGetState();
    memset(s->transformFeedbackWriteOffset, 0,
           sizeof(s->transformFeedbackWriteOffset));
    s->transformFeedbackActive = TRUE;
    s->transformFeedbackMode = primitiveMode;
    gldAdvBeginTransformFeedback((GLenum)primitiveMode);
}

void _glsEndTransformFeedback(void)
{
    GLS_State *s = glsGetState();
    s->transformFeedbackActive = FALSE;
    gldAdvEndTransformFeedback();
}

void _glsTransformFeedbackVaryings(unsigned int program, int count, const char *const*varyings, unsigned int bufferMode)
{
    GLS_Program *prog = glsFindProgram(program);
    int i;
    if (prog && count >= 0 && count <= GLS_MAX_VERTEX_ATTRIBS) {
        prog->transformFeedbackMode = bufferMode;
        prog->transformFeedbackCount = count;
        for (i = 0; i < count; ++i) {
            strncpy(prog->transformFeedbackVaryings[i],
                    (varyings && varyings[i]) ? varyings[i] : "",
                    sizeof(prog->transformFeedbackVaryings[i]) - 1);
            prog->transformFeedbackVaryings[i]
                [sizeof(prog->transformFeedbackVaryings[i]) - 1] = '\0';
        }
    }
    /* Accept and store — no rendering yet */
    gldAdvTransformFeedbackVaryings(program, count, varyings, bufferMode);
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
    int i;

    if (_glsRunInstancedStageDraw(mode, first, count, 0, NULL, 0,
                                  instancecount, 0))
        return;

    /* Drawn as a loop rather than with D3D9 hardware instancing: the vertex
     * path here builds a fat CPU vertex per draw, so there is no per-instance
     * stream to bind a divisor to.  Every instance therefore renders with the
     * same attributes - correct only for shaders that key off gl_InstanceID,
     * which nothing in the fixed-function path does.  Faithful geometry beats
     * drawing nothing. */
    for (i = 0; i < instancecount; i++)
        _glsDrawArrays(mode, first, count);
}

void _glsDrawElementsInstanced(unsigned int mode, int count, unsigned int type, const void *indices, int instancecount)
{
    int i;

    if (_glsRunInstancedStageDraw(mode, 0, count, type, indices, 0,
                                  instancecount, 0))
        return;

    /* See _glsDrawArraysInstanced for why this is a loop, not real instancing. */
    for (i = 0; i < instancecount; i++)
        _glsDrawElements(mode, count, type, indices);
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

static void _glsTexBufferView(unsigned int target, unsigned int internalformat,
                              unsigned int buffer, ptrdiff_t offset,
                              ptrdiff_t rangeSize, BOOL ranged)
{
    GLS_Buffer *buf = glsFindBuffer(buffer);
    GLS_Texture *tex;
    unsigned int format = GL_RGBA, type = GL_UNSIGNED_BYTE;
    int bytesPerElement = 4;
    int width, height, unit;
    size_t uploadBytes;
    const void *upload;
    unsigned char *padded = NULL;
    if (target != GL_TEXTURE_BUFFER) {
        glsGetState()->lastError = GL_INVALID_ENUM;
        return;
    }
    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }
    if (buffer == 0) {
        _glsReleaseTextureResources(tex);
        tex->target = GL_TEXTURE_BUFFER;
        tex->internalFormat = internalformat;
        tex->bufferObject = 0;
        tex->bufferOffset = 0;
        tex->bufferSize = 0;
        return;
    }
    if (!buf || !buf->data || buf->size <= 0) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }
    if (ranged && (offset < 0 || rangeSize <= 0 ||
                   offset > buf->size || rangeSize > buf->size - offset)) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }
    switch (internalformat) {
    case GL_R8: case GL_R8UI: format = GL_RED; type = GL_UNSIGNED_BYTE; bytesPerElement = 1; break;
    case GL_R8I: format = GL_RED; type = GL_BYTE; bytesPerElement = 1; break;
    case GL_R16: case GL_R16UI: format = GL_RED; type = GL_UNSIGNED_SHORT; bytesPerElement = 2; break;
    case GL_R16I: format = GL_RED; type = GL_SHORT; bytesPerElement = 2; break;
    case GL_R16F: format = GL_RED; type = GL_HALF_FLOAT; bytesPerElement = 2; break;
    case GL_R32F: format = GL_RED; type = GL_FLOAT; bytesPerElement = 4; break;
    case GL_R32I: format = GL_RED; type = GL_INT; bytesPerElement = 4; break;
    case GL_R32UI: format = GL_RED; type = GL_UNSIGNED_INT; bytesPerElement = 4; break;
    case GL_RG8: case GL_RG8UI: format = GL_RG; type = GL_UNSIGNED_BYTE; bytesPerElement = 2; break;
    case GL_RG8I: format = GL_RG; type = GL_BYTE; bytesPerElement = 2; break;
    case GL_RG16: case GL_RG16UI: format = GL_RG; type = GL_UNSIGNED_SHORT; bytesPerElement = 4; break;
    case GL_RG16I: format = GL_RG; type = GL_SHORT; bytesPerElement = 4; break;
    case GL_RG16F: format = GL_RG; type = GL_HALF_FLOAT; bytesPerElement = 4; break;
    case GL_RG32F: format = GL_RG; type = GL_FLOAT; bytesPerElement = 8; break;
    case GL_RG32I: format = GL_RG; type = GL_INT; bytesPerElement = 8; break;
    case GL_RG32UI: format = GL_RG; type = GL_UNSIGNED_INT; bytesPerElement = 8; break;
    case GL_RGBA8: case GL_RGBA8UI: format = GL_RGBA; type = GL_UNSIGNED_BYTE; bytesPerElement = 4; break;
    case GL_RGBA8I: format = GL_RGBA; type = GL_BYTE; bytesPerElement = 4; break;
    case GL_RGBA16: case GL_RGBA16UI: format = GL_RGBA; type = GL_UNSIGNED_SHORT; bytesPerElement = 8; break;
    case GL_RGBA16I: format = GL_RGBA; type = GL_SHORT; bytesPerElement = 8; break;
    case GL_RGBA16F: format = GL_RGBA; type = GL_HALF_FLOAT; bytesPerElement = 8; break;
    case GL_RGBA32F: format = GL_RGBA; type = GL_FLOAT; bytesPerElement = 16; break;
    case GL_RGBA32I: format = GL_RGBA; type = GL_INT; bytesPerElement = 16; break;
    case GL_RGBA32UI: format = GL_RGBA; type = GL_UNSIGNED_INT; bytesPerElement = 16; break;
    default:
        glsGetState()->lastError = GL_INVALID_ENUM;
        return;
    }
    if (!ranged) {
        offset = 0;
        rangeSize = buf->size;
    }
    width = (int)(rangeSize / bytesPerElement);
    if (width < 1) width = 1;
    height = (width + 4095) / 4096;
    if (width > 4096) width = 4096;
    uploadBytes = (size_t)width * (size_t)height * (size_t)bytesPerElement;
    upload = (const unsigned char *)buf->data + offset;
    if (uploadBytes > (size_t)rangeSize) {
        padded = (unsigned char *)calloc(1, uploadBytes);
        if (!padded) { glsGetState()->lastError = GL_OUT_OF_MEMORY; return; }
        memcpy(padded, (const unsigned char *)buf->data + offset,
               (size_t)rangeSize);
        upload = padded;
    }
    _glsTexImage2D(GL_TEXTURE_BUFFER, 0, (int)internalformat, width, height, 0,
                   format, type, upload);
    free(padded);
    tex->bufferObject = buffer;
    tex->bufferOffset = (GLintptr_t)offset;
    tex->bufferSize = ranged ? (GLsizeiptr_t)rangeSize : 0;
}

void _glsTexBuffer(unsigned int target, unsigned int internalformat,
                   unsigned int buffer)
{
    _glsTexBufferView(target, internalformat, buffer, 0, 0, FALSE);
}

void _glsTexBufferRange(unsigned int target, unsigned int internalformat,
                        unsigned int buffer, ptrdiff_t offset, ptrdiff_t size)
{
    _glsTexBufferView(target, internalformat, buffer, offset, size, TRUE);
}

void _glsDrawArraysInstancedBaseInstance(unsigned int mode, int first, int count,
                                         int instancecount,
                                         unsigned int baseinstance)
{
    int i;
    if (_glsRunInstancedStageDraw(mode, first, count, 0, NULL, 0,
                                  instancecount, baseinstance))
        return;
    for (i = 0; i < instancecount; ++i)
        _glsDrawArrays(mode, first, count);
}

void _glsDrawElementsInstancedBaseVertexBaseInstance(
    unsigned int mode, int count, unsigned int type, const void *indices,
    int instancecount, int basevertex, unsigned int baseinstance)
{
    int i;
    if (_glsRunInstancedStageDraw(mode, 0, count, type, indices, basevertex,
                                  instancecount, baseinstance))
        return;
    for (i = 0; i < instancecount; ++i)
        _glsDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void _glsPrimitiveRestartIndex(unsigned int index)
{
    GLS_State *s = glsGetState();
    s->primitiveRestartIndex = index;
}

void _glsUniformBlockBinding(unsigned int program, unsigned int blockIndex, unsigned int blockBinding)
{
    gldAdvUniformBlockBinding(program, blockIndex, blockBinding);
}

/* ===================================================================
 *  SECTION 25: GL 3.2 — Sync
 * =================================================================== */

/*
 * Sync objects map onto D3DQUERYTYPE_EVENT, which signals once every command
 * issued before it has been consumed by the GPU - the same guarantee
 * glFenceSync makes.  The GLsync handle IS the query interface, so no side
 * table is needed and the lifetime is exactly the caller's.
 *
 * When no event query can be created the handle is NULL and the wait calls
 * report GL_WAIT_FAILED, rather than the old behaviour of always claiming
 * GL_ALREADY_SIGNALED - which told callers their work had completed when
 * nothing had been observed at all.
 */
void *_glsFenceSync(unsigned int condition, unsigned int flags)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DQuery9 *pq = NULL;
    HRESULT hr;

    (void)condition; (void)flags;

    if (!pDev) return NULL;

    hr = IDirect3DDevice9_CreateQuery(pDev, D3DQUERYTYPE_EVENT, &pq);
    if (FAILED(hr) || !pq) {
        gldDiagLog("GL: FenceSync CreateQuery(EVENT) failed hr=0x%08X", hr);
        return NULL;
    }

    IDirect3DQuery9_Issue(pq, D3DISSUE_END);
    return (void *)pq;
}

void _glsDeleteSync(void *sync)
{
    IDirect3DQuery9 *pq = (IDirect3DQuery9 *)sync;
    if (pq) IDirect3DQuery9_Release(pq);
}

unsigned int _glsClientWaitSync(void *sync, unsigned int flags, unsigned long long timeout)
{
    IDirect3DQuery9 *pq = (IDirect3DQuery9 *)sync;
    BOOL block = (timeout != 0);
    HRESULT hr;
    int spins = 0;

    (void)flags;

    if (!pq) return GL_WAIT_FAILED;

    for (;;) {
        hr = IDirect3DQuery9_GetData(pq, NULL, 0, D3DGETDATA_FLUSH);
        if (hr == S_OK)    return GL_ALREADY_SIGNALED;
        if (hr != S_FALSE) return GL_WAIT_FAILED;
        if (!block)        return GL_TIMEOUT_EXPIRED;
        if (++spins > 1000000) {
            gldDiagLog("GL: ClientWaitSync timed out waiting for GPU");
            return GL_TIMEOUT_EXPIRED;
        }
    }
}

void _glsWaitSync(void *sync, unsigned int flags, unsigned long long timeout)
{
    /* glWaitSync orders work on the GPU rather than blocking the caller.  D3D9
     * has a single immediate command stream, so a fence issued earlier is
     * already ordered ahead of everything issued after it - the ordering this
     * asks for is inherent, and there is nothing to submit. */
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

static void _applySamplerObjectToD3D(unsigned int unit,
                                     const GLS_Sampler *samp);

void _glsBindSampler(unsigned int unit, unsigned int sampler)
{
    GLS_State *s = glsGetState();
    if (unit >= GLS_MAX_TEX_UNITS) { s->lastError = GL_INVALID_VALUE; return; }
    if (sampler && !glsFindSampler(sampler)) {
        s->lastError = GL_INVALID_OPERATION;
        return;
    }
    s->boundSampler[unit] = sampler;
    if (sampler)
        _applySamplerObjectToD3D(unit, glsFindSampler(sampler));
    else {
        GLS_Texture *tex = glsFindTexture(s->boundTexture2D[unit]);
        if (!tex) tex = glsFindTexture(s->boundTextureCube[unit]);
        if (!tex) tex = glsFindTexture(s->boundTexture3D[unit]);
        _applyTextureObjectSamplingToD3D(unit, tex);
    }
}

static void _applyTextureObjectSamplingToD3D(unsigned int unit,
                                             const GLS_Texture *tex)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev || !tex || unit >= GLS_MAX_TEX_UNITS) return;
    __try {
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MINFILTER,
                                         _glsMapMinFilter(tex->minFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MIPFILTER,
                                         _glsMapMipFilter(tex->minFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MAGFILTER,
                                         _glsMapMagFilter(tex->magFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSU,
                                         _glsMapWrapMode(tex->wrapS));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSV,
                                         _glsMapWrapMode(tex->wrapT));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSW,
                                         _glsMapWrapMode(tex->wrapR));
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

static void _applySamplerObjectToD3D(unsigned int unit, const GLS_Sampler *samp)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD border;
    DWORD biasBits;
    float bias;
    if (!pDev || !samp || unit >= GLS_MAX_TEX_UNITS) return;
    border = D3DCOLOR_COLORVALUE(samp->borderColor[0], samp->borderColor[1],
                                 samp->borderColor[2], samp->borderColor[3]);
    bias = samp->lodBias;
    memcpy(&biasBits, &bias, sizeof(biasBits));
    __try {
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MINFILTER,
                                         _glsMapMinFilter(samp->minFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MIPFILTER,
                                         _glsMapMipFilter(samp->minFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MAGFILTER,
                                         _glsMapMagFilter(samp->magFilter));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSU,
                                         _glsMapWrapMode(samp->wrapS));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSV,
                                         _glsMapWrapMode(samp->wrapT));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_ADDRESSW,
                                         _glsMapWrapMode(samp->wrapR));
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_BORDERCOLOR, border);
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MIPMAPLODBIAS,
                                         biasBits);
        IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MAXANISOTROPY,
                                         (DWORD)(samp->maxAnisotropy > 1.0f
                                                 ? samp->maxAnisotropy : 1.0f));
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

static void _refreshBoundSampler(const GLS_Sampler *samp)
{
    GLS_State *s = glsGetState();
    int unit;
    if (!samp) return;
    for (unit = 0; unit < GLS_MAX_TEX_UNITS; ++unit)
        if (s->boundSampler[unit] == samp->id)
            _applySamplerObjectToD3D((unsigned int)unit, samp);
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
    _refreshBoundSampler(samp);
}

void _glsSamplerParameterf(unsigned int sampler, unsigned int pname, float param)
{
    GLS_Sampler *samp = glsFindSampler(sampler);
    _setSamplerParam(samp, pname, param);
    _refreshBoundSampler(samp);
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
    _refreshBoundSampler(samp);
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
    _refreshBoundSampler(samp);
}

/* ===================================================================
 *  SECTION 27: GL 4.x
 * =================================================================== */

/*
 * glTexStorage2D — allocate all mip levels of an immutable-format texture.
 *
 * This has to create the D3D9 resource, not just record the dimensions.  It
 * used to do only the bookkeeping, which looks harmless — "the upload will
 * allocate it" — and is not: glTexStorage2D is the *only* allocation call an
 * application using immutable storage makes.  Every level after it arrives
 * through glTexSubImage2D / glCompressedTexSubImage2D, which allocate nothing.
 * So the texture kept whatever resource it happened to have from an earlier,
 * differently sized definition (or none at all), while tex->width/height
 * claimed the new size.  Uploads sized for what the application declared then
 * ran off the end of what was really allocated: a 1024x1024 declaration
 * against a surviving 256x256 resource made every one of a nine-level mip
 * chain's uploads oversized by a uniform 4x in each dimension.
 *
 * Nothing here is specific to any title — this is what glTexStorage2D means.
 */
void _glsTexStorage2D(unsigned int target, int levels, unsigned int internalformat, int width, int height)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    BOOL cube, compressed;
    int unit;
    HRESULT hr;

    gldDiagLogV("GL: -> TexStorage2D target=0x%X levels=%d %dx%d int=0x%X",
               target, levels, width, height, internalformat);

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) return;
    if (!pDev || width <= 0 || height <= 0) return;

    cube = (target == GL_TEXTURE_CUBE_MAP) || (_glsCubeFaceFromTarget(target) >= 0);

    /* glTexStorage* states the level count outright, unlike glTexImage2D where
     * it has to be guessed from the base size.  A caller that passes a
     * nonsensical count still gets a full chain rather than nothing. */
    if (levels <= 0)
        levels = _glsMipLevelCount(width, height);

    /* internalformat may legitimately be a compressed sized format here, in
     * which case the storage has to be the block-compressed surface — there is
     * no decompressor to substitute an uncompressed one with. */
    d3dFmt = _glsMapCompressedFormatToD3D(internalformat);
    compressed = (d3dFmt != D3DFMT_UNKNOWN);

    if (compressed) {
        if (!gldIsTextureFormatSupported46(d3dFmt, cube)) {
            gldDiagLog("GL: TexStorage2D D3DFMT=%d (tex=%u) unsupported by device; no software "
                       "decompressor - texture left without storage", (int)d3dFmt, tex->id);
            return;
        }
    } else {
        /* Never hand CreateTexture a format the device has not already
         * confirmed it can create — see _glsResolveTextureFormat. */
        d3dFmt = _glsResolveTextureFormat(_glsMapGLFormatToD3D(internalformat), cube);
        if (d3dFmt == D3DFMT_UNKNOWN) {
            gldDiagLog("GL: TexStorage2D no creatable format for int=0x%X (tex=%u) %dx%d "
                       "- texture left without storage",
                       internalformat, tex->id, width, height);
            return;
        }
    }

    /* Redefinition at a new size/format must not leave the old resource behind. */
    _glsReleaseTextureResources(tex);

    tex->width          = width;
    tex->height         = height;
    tex->internalFormat = (GLenum_t)internalformat;
    tex->target         = target;

    __try {
        if (cube)
            hr = IDirect3DDevice9_CreateCubeTexture(pDev, width, levels, 0,
                                                    d3dFmt, D3DPOOL_MANAGED,
                                                    &tex->pCubeTex, NULL);
        else
            hr = IDirect3DDevice9_CreateTexture(pDev, width, height, levels, 0,
                                                d3dFmt, D3DPOOL_MANAGED,
                                                &tex->pTex, NULL);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (FAILED(hr)) {
        gldDiagLog("GL: TexStorage2D Create%sTexture FAILED hr=0x%08X %dx%d levels=%d fmt=%d (tex=%u)",
                   cube ? "Cube" : "", (unsigned)hr, width, height, levels, d3dFmt, tex->id);
        tex->pTex = NULL;
        tex->pCubeTex = NULL;
        return;
    }

    gldDiagLogV("GL: TexStorage2D tex=%u target=0x%X %dx%d levels=%d int=0x%X d3d=%d",
               tex->id, target, width, height, levels, internalformat, d3dFmt);
}

/*
 * glTexStorage3D — the volume-texture form of the above, and allocating for
 * the same reason: nothing after this call allocates anything.
 *
 * Only GL_TEXTURE_3D has a D3D9 resource type.  The array targets
 * glTexStorage3D also accepts (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_CUBE_MAP_ARRAY)
 * have no D3D9 equivalent at all, so those keep the bookkeeping and are
 * reported rather than being given a volume texture that would be silently
 * wrong.
 */
void _glsTexStorage3D(unsigned int target, int levels, unsigned int internalformat, int width, int height, int depth)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    int unit;
    HRESULT hr;

    gldDiagLogV("GL: -> TexStorage3D target=0x%X levels=%d %dx%dx%d int=0x%X",
               target, levels, width, height, depth, internalformat);

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) return;
    if (!pDev || width <= 0 || height <= 0 || depth <= 0) return;

    if (target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        tex->width          = width;
        tex->height         = height;
        tex->depth          = depth;
        tex->internalFormat = (GLenum_t)internalformat;
        gldDiagLog("GL: TexStorage3D target 0x%X has no D3D9 resource type (tex=%u) "
                   "- storage not allocated", target, tex->id);
        return;
    }

    if (levels <= 0)
        levels = _glsMipLevelCount(width, height);

    d3dFmt = _glsMapCompressedFormatToD3D(internalformat);
    if (d3dFmt == D3DFMT_UNKNOWN)
        d3dFmt = _glsMapGLFormatToD3D(internalformat);

    _glsReleaseTextureResources(tex);

    tex->width          = width;
    tex->height         = height;
    tex->depth          = depth;
    tex->internalFormat = (GLenum_t)internalformat;
    tex->target         = target;

    /* Volume textures have their own creatable-format set, so the 2D answer
     * from _glsResolveTextureFormat does not apply; mirror _glsTexImage3D and
     * let the one A8R8G8B8 retry stand in for it. */
    __try {
        hr = IDirect3DDevice9_CreateVolumeTexture(pDev, width, height, depth, levels, 0,
                                                  d3dFmt, D3DPOOL_MANAGED,
                                                  &tex->pVolTex, NULL);
        if (FAILED(hr) && d3dFmt != D3DFMT_A8R8G8B8 &&
            _glsMapCompressedFormatToD3D(internalformat) == D3DFMT_UNKNOWN) {
            d3dFmt = D3DFMT_A8R8G8B8;
            hr = IDirect3DDevice9_CreateVolumeTexture(pDev, width, height, depth, levels, 0,
                                                      d3dFmt, D3DPOOL_MANAGED,
                                                      &tex->pVolTex, NULL);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (FAILED(hr)) {
        gldDiagLog("GL: TexStorage3D CreateVolumeTexture FAILED hr=0x%08X %dx%dx%d levels=%d "
                   "fmt=%d (tex=%u) - texture left without storage",
                   (unsigned)hr, width, height, depth, levels, d3dFmt, tex->id);
        tex->pVolTex = NULL;
        return;
    }

    gldDiagLogV("GL: TexStorage3D tex=%u %dx%dx%d levels=%d int=0x%X d3d=%d",
               tex->id, width, height, depth, levels, internalformat, d3dFmt);
}

void _glsDispatchCompute(unsigned int x, unsigned int y, unsigned int z)
{
    GLS_Program *program = _getBoundProgram();
    char log[512];
    if (!program || !program->linked || !program->computeShader) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        return;
    }
    if (!gldComputeEmulatorDispatch(program, x, y, z, log, sizeof(log))) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
        gldDiagLog("GL: glDispatchCompute failed: %s", log);
    }
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
    /* D3D9 has no per-draw-buffer enables.  Index 0 is the only buffer
     * this device renders to, so it maps exactly; any higher index is a
     * request this device cannot honour. */
    if (index == 0) { _glsEnable(target); return; }
    gldDiagLog("GL: Enablei target=0x%X index=%u - D3D9 has no indexed enable; ignored",
               target, index);
}

void _glsDisablei(unsigned int target, unsigned int index)
{
    if (index == 0) { _glsDisable(target); return; }
    gldDiagLog("GL: Disablei target=0x%X index=%u - D3D9 has no indexed enable; ignored",
               target, index);
}

/* ===================================================================
 *  SECTION 29: Queries
 * =================================================================== */

/*
 * GL query targets that D3D9 can actually answer.
 *
 * Occlusion is the one with a direct analogue.  Anything else returns FALSE so
 * the caller logs and leaves the query without a D3D object, rather than
 * quietly reporting a count it never measured - the previous code set
 * result = 0 for every query, which tells a visibility test that nothing is
 * visible and makes a game cull the whole scene away.
 */
static BOOL _glsQueryTypeForTarget(unsigned int target, D3DQUERYTYPE *out)
{
    switch (target) {
    case GL_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        *out = D3DQUERYTYPE_OCCLUSION;
        return TRUE;
    default:
        return FALSE;
    }
}

void _glsBeginQuery(unsigned int target, unsigned int id)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Query *q = glsFindQuery(id);
    D3DQUERYTYPE qt;
    HRESULT hr;

    if (!q) return;

    q->target      = target;
    q->active      = TRUE;
    q->resultReady = FALSE;
    q->result      = 0;

    if (!pDev) return;

    if (!_glsQueryTypeForTarget(target, &qt)) {
        gldDiagLog("GL: BeginQuery target=0x%X has no D3D9 equivalent - result will be unavailable",
                   target);
        return;
    }

    if (!q->pQuery) {
        hr = IDirect3DDevice9_CreateQuery(pDev, qt, &q->pQuery);
        if (FAILED(hr) || !q->pQuery) {
            q->pQuery = NULL;
            gldDiagLog("GL: BeginQuery CreateQuery failed hr=0x%08X target=0x%X", hr, target);
            return;
        }
    }

    IDirect3DQuery9_Issue(q->pQuery, D3DISSUE_BEGIN);
}

void _glsEndQuery(unsigned int target)
{
    GLS_State *s = glsGetState();
    int i;

    for (i = 0; i < GLS_MAX_QUERIES; i++) {
        GLS_Query *q = &s->queries[i];
        if (q->allocated && q->active && q->target == target) {
            q->active = FALSE;
            if (q->pQuery)
                IDirect3DQuery9_Issue(q->pQuery, D3DISSUE_END);
            break;
        }
    }
}

/*
 * Pull the occlusion count out of D3D9.
 *
 * GetData returns S_FALSE while the GPU has not finished.  block=TRUE is the
 * glGetQueryObject*v(GL_QUERY_RESULT) contract, which is defined to wait; the
 * spin is bounded so a lost device cannot hang the caller forever.
 */
static BOOL _glsQueryFetch(GLS_Query *q, BOOL block)
{
    DWORD pixels = 0;
    HRESULT hr;
    int spins = 0;

    if (!q) return FALSE;
    if (q->resultReady) return TRUE;
    if (!q->pQuery || q->active) return FALSE;

    for (;;) {
        hr = IDirect3DQuery9_GetData(q->pQuery, &pixels, sizeof(pixels), D3DGETDATA_FLUSH);
        if (hr == S_OK) {
            q->result      = (GLuint_t)pixels;
            q->resultReady = TRUE;
            return TRUE;
        }
        if (hr != S_FALSE) {
            /* D3DERR_DEVICELOST or a real failure - no answer is coming. */
            gldDiagLog("GL: query %u GetData failed hr=0x%08X", q->id, hr);
            return FALSE;
        }
        if (!block) return FALSE;
        if (++spins > 1000000) {
            gldDiagLog("GL: query %u timed out waiting for GPU", q->id);
            return FALSE;
        }
    }
}

void _glsGetQueryiv(unsigned int target, unsigned int pname, int *params)
{
    GLS_State *s = glsGetState();
    int i;

    if (!params) return;

    switch (pname) {
    case GL_CURRENT_QUERY:
        *params = 0;
        for (i = 0; i < GLS_MAX_QUERIES; i++) {
            if (s->queries[i].allocated && s->queries[i].active &&
                s->queries[i].target == target) {
                *params = (int)s->queries[i].id;
                break;
            }
        }
        break;
    case GL_QUERY_COUNTER_BITS:
        /* D3D9 reports the occlusion count as a DWORD. */
        *params = 32;
        break;
    default:
        *params = 0;
        break;
    }
}

void _glsGetQueryObjectuiv(unsigned int id, unsigned int pname, unsigned int *params)
{
    GLS_Query *q = glsFindQuery(id);

    if (!params) return;

    if (!q) {
        *params = 0;
        return;
    }

    switch (pname) {
    case GL_QUERY_RESULT_AVAILABLE:
        *params = _glsQueryFetch(q, FALSE) ? 1u : 0u;
        break;
    case GL_QUERY_RESULT:
    case GL_QUERY_RESULT_NO_WAIT:
        if (_glsQueryFetch(q, pname == GL_QUERY_RESULT))
            *params = (unsigned int)q->result;
        else
            /* No measurement available.  Report "something passed" rather than
             * zero: a visibility test reading zero hides geometry that was
             * never actually tested, which is the worse failure. */
            *params = q->pQuery ? 0u : 1u;
        break;
    default:
        *params = 0;
        break;
    }
}

void _glsGetQueryObjectiv(unsigned int id, unsigned int pname, int *params)
{
    unsigned int v = 0;
    if (!params) return;
    _glsGetQueryObjectuiv(id, pname, &v);
    *params = (int)v;
}

void _glsGetQueryObjectui64v(unsigned int id, unsigned int pname, unsigned __int64 *params)
{
    unsigned int v = 0;
    if (!params) return;
    _glsGetQueryObjectuiv(id, pname, &v);
    *params = (unsigned __int64)v;
}

void _glsGetQueryObjecti64v(unsigned int id, unsigned int pname, __int64 *params)
{
    unsigned int v = 0;
    if (!params) return;
    _glsGetQueryObjectuiv(id, pname, &v);
    *params = (__int64)v;
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
    GLS_Texture *tex;
    int unit;
    if (samples <= 0) { glsGetState()->lastError = GL_INVALID_VALUE; return; }
    _glsTexStorage2D(target, 1, internalformat, width, height);
    tex = _glsBoundTextureForTarget(target, &unit);
    if (tex) {
        tex->samples = samples;
        tex->fixedSampleLocations = fixedsamplelocations;
    }
}

void _glsTexImage3DMultisample(unsigned int target, int samples, unsigned int internalformat, int width, int height, int depth, unsigned char fixedsamplelocations)
{
    GLS_Texture *tex;
    int unit;
    if (samples <= 0) { glsGetState()->lastError = GL_INVALID_VALUE; return; }
    _glsTexStorage3D(target, 1, internalformat, width, height, depth);
    tex = _glsBoundTextureForTarget(target, &unit);
    if (tex) {
        tex->samples = samples;
        tex->fixedSampleLocations = fixedsamplelocations;
    }
}


/* ===================================================================
 *  SECTION 31: GL 4.x Misc
 * =================================================================== */

void _glsMinSampleShading(float value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    glsGetState()->minSampleShading = value;
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
    GLS_State *s = glsGetState();
    if (pname != GL_PATCH_VERTICES) {
        s->lastError = GL_INVALID_ENUM;
        return;
    }
    if (value <= 0 || value > 32) {
        s->lastError = GL_INVALID_VALUE;
        return;
    }
    s->patchVertices = value;
}

void _glsMemoryBarrier(unsigned int barriers)
{
    IDirect3DDevice9 *dev = gldGetD3DDevice46();
    IDirect3DQuery9 *query = NULL;
    HRESULT hr;
    int spins = 0;
    (void)barriers;
    if (!dev) return;
    hr = IDirect3DDevice9_CreateQuery(dev, D3DQUERYTYPE_EVENT, &query);
    if (FAILED(hr) || !query) return;
    IDirect3DQuery9_Issue(query, D3DISSUE_END);
    do {
        hr = IDirect3DQuery9_GetData(query, NULL, 0, D3DGETDATA_FLUSH);
    } while (hr == S_FALSE && ++spins < 1000000);
    IDirect3DQuery9_Release(query);
    if (FAILED(hr)) {
        glsGetState()->lastError = GL_INVALID_OPERATION;
    }
}

void _glsBindImageTexture(unsigned int unit, unsigned int texture, int level, unsigned char layered, int layer, unsigned int access, unsigned int format)
{
    GLS_State *s = glsGetState();
    GLS_ImageBinding *b;
    if (unit >= GLS_MAX_IMAGE_UNITS || level < 0 || (texture && !glsFindTexture(texture))) {
        s->lastError = GL_INVALID_VALUE;
        return;
    }
    b = &s->imageBindings[unit];
    b->texture = texture;
    b->level = level;
    b->layered = layered;
    b->layer = layer;
    b->access = access;
    b->format = format;
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
    _glsMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
}


/* ===================================================================
 *  SECTION 32: Texture 3D / Compressed / Misc Legacy
 * =================================================================== */

/*
 * glTexImage3D — define one mip level of a volume texture.
 *
 * D3D9 volume textures are locked a box at a time rather than a rect at a
 * time, so each z slice is written through its own D3DBOX and the existing
 * row converter fills it.  Storage is allocated on the level 0 call as a full
 * mip chain, the same way _glsTexImage2D does it and for the same reason: GL
 * gives no advance notice of how many levels are coming.
 */
void _glsTexImage3D(unsigned int target, int level, int internalformat, int width, int height, int depth, int border, unsigned int format, unsigned int type, const void *pixels)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    D3DVOLUME_DESC desc;
    int unit, z;
    HRESULT hr;

    (void)border;

    gldDiagLogV("GL: -> TexImage3D target=0x%X level=%d %dx%dx%d int=0x%X fmt=0x%X type=0x%X px=%p",
               target, level, width, height, depth, internalformat, format, type, pixels);

    if (target != GL_TEXTURE_3D) {
        gldDiagLog("GL: TexImage3D target 0x%X is not GL_TEXTURE_3D, skipped", target);
        return;
    }

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;
    if (!pDev || width <= 0 || height <= 0 || depth <= 0) return;

    d3dFmt = _glsMapGLFormatToD3D((unsigned int)internalformat);

    if (level == 0) {
        int levels = _glsMipLevelCount(width, height);

        _glsReleaseTextureResources(tex);

        tex->width          = width;
        tex->height         = height;
        tex->depth          = depth;
        tex->internalFormat = (GLenum_t)internalformat;
        tex->target         = target;

        /* Volume textures have their own creatable-format set, so ask about
         * this exact resource type rather than reusing the 2D answer. */
        __try {
            hr = IDirect3DDevice9_CreateVolumeTexture(pDev, width, height, depth, levels, 0,
                                                      d3dFmt, D3DPOOL_MANAGED,
                                                      &tex->pVolTex, NULL);
            if (FAILED(hr) && d3dFmt != D3DFMT_A8R8G8B8) {
                d3dFmt = D3DFMT_A8R8G8B8;
                hr = IDirect3DDevice9_CreateVolumeTexture(pDev, width, height, depth, levels, 0,
                                                          d3dFmt, D3DPOOL_MANAGED,
                                                          &tex->pVolTex, NULL);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }

        if (FAILED(hr)) {
            gldDiagLog("GL: TexImage3D CreateVolumeTexture FAILED hr=0x%08X %dx%dx%d fmt=%d "
                       "- texture left without data", (unsigned)hr, width, height, depth, d3dFmt);
            tex->pVolTex = NULL;
            return;
        }
    }

    if (!tex->pVolTex) {
        gldDiagLog("GL: TexImage3D level %d with no storage allocated", level);
        return;
    }

    hr = IDirect3DVolumeTexture9_GetLevelDesc(tex->pVolTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: TexImage3D GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, (unsigned)hr);
        return;
    }

    if (pixels) {
        int srcBpp = _glsSourceBytesPerPixel(format, type);
        GLS_State *st = glsGetState();
        int rowPixels = (st->unpackRowLength > 0) ? st->unpackRowLength : width;
        int align = (st->unpackAlignment > 0) ? st->unpackAlignment : 4;
        int srcStride = ((rowPixels * srcBpp + align - 1) / align) * align;

        if (srcBpp == 0) {
            gldDiagLog("GL: TexImage3D unsupported fmt=0x%X type=0x%X, level skipped",
                       format, type);
            return;
        }

        for (z = 0; z < depth; z++) {
            D3DLOCKED_BOX lb;
            D3DBOX box;
            box.Left = 0; box.Top = 0;
            box.Right = (UINT)width; box.Bottom = (UINT)height;
            box.Front = (UINT)z; box.Back = (UINT)z + 1;

            __try {
                hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb, &box, 0);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                hr = E_FAIL;
            }
            if (FAILED(hr)) {
                gldDiagLog("GL: TexImage3D LockBox failed level=%d slice=%d hr=0x%08X",
                           level, z, (unsigned)hr);
                break;
            }
            _glsCopyPixelsToD3D(lb.pBits,
                                (const unsigned char *)pixels + (ptrdiff_t)z * srcStride * height,
                                width, height, format, type, lb.RowPitch, desc.Format);
            __try {
                IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
        }
    }

    gldDiagLogV("GL: TexImage3D tex=%u level=%d %dx%dx%d d3d=%d",
               tex->id, level, width, height, depth, desc.Format);
}

void _glsTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, unsigned int type, const void *pixels)
{
    GLS_Texture *tex;
    D3DVOLUME_DESC desc;
    int unit, z;
    HRESULT hr;
    int srcBpp, rowPixels, align, srcStride;
    GLS_State *st = glsGetState();

    if (target != GL_TEXTURE_3D) {
        gldDiagLog("GL: TexSubImage3D target 0x%X is not GL_TEXTURE_3D, skipped", target);
        return;
    }

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || !pixels || level < 0) return;
    if (width <= 0 || height <= 0 || depth <= 0) return;
    if (!tex->pVolTex) {
        gldDiagLog("GL: TexSubImage3D with no storage (tex=%u)", tex->id);
        return;
    }

    hr = IDirect3DVolumeTexture9_GetLevelDesc(tex->pVolTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: TexSubImage3D GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, (unsigned)hr);
        return;
    }

    srcBpp = _glsSourceBytesPerPixel(format, type);
    if (srcBpp == 0) {
        gldDiagLog("GL: TexSubImage3D unsupported fmt=0x%X type=0x%X, skipped", format, type);
        return;
    }
    rowPixels = (st->unpackRowLength > 0) ? st->unpackRowLength : width;
    align     = (st->unpackAlignment > 0) ? st->unpackAlignment : 4;
    srcStride = ((rowPixels * srcBpp + align - 1) / align) * align;

    for (z = 0; z < depth; z++) {
        D3DLOCKED_BOX lb;
        D3DBOX box;
        box.Left = (UINT)xoffset; box.Top = (UINT)yoffset;
        box.Right = (UINT)(xoffset + width); box.Bottom = (UINT)(yoffset + height);
        box.Front = (UINT)(zoffset + z); box.Back = (UINT)(zoffset + z + 1);

        __try {
            hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb, &box, 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (FAILED(hr)) {
            gldDiagLog("GL: TexSubImage3D LockBox failed level=%d slice=%d hr=0x%08X",
                       level, z, (unsigned)hr);
            return;
        }
        _glsCopyPixelsToD3D(lb.pBits,
                            (const unsigned char *)pixels + (ptrdiff_t)z * srcStride * height,
                            width, height, format, type, lb.RowPitch, desc.Format);
        __try {
            IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    gldDiagLogV("GL: TexSubImage3D tex=%u level=%d (%d,%d,%d) %dx%dx%d",
               tex->id, level, xoffset, yoffset, zoffset, width, height, depth);
}

/* Bytes one 4x4 block of a DXT format occupies. */
static int _glsCompressedBlockSize(D3DFORMAT fmt)
{
    return (fmt == D3DFMT_DXT1) ? 8 : 16;
}

/*
 * Common body for the compressed sub-image entry points.
 *
 * DXT data is addressed in 4x4 blocks, so an offset that is not block aligned
 * cannot be expressed as a locked rectangle at all — GL requires callers to
 * align it, and a misaligned one is reported rather than quietly rounded.
 */
static void _glsCompressedSubImage2D(unsigned int target, int level,
                                     int xoffset, int yoffset,
                                     int width, int height, unsigned int format,
                                     int imageSize, const void *data)
{
    GLS_Texture *tex;
    D3DLOCKED_RECT lr;
    D3DSURFACE_DESC desc;
    D3DFORMAT d3dFmt;
    RECT rect;
    UINT limitW, limitH;
    int unit, row, blockWidth, blockHeight, blockSize, srcRowBytes;
    HRESULT hr;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || !data || level < 0 || imageSize <= 0) return;
    if (width <= 0 || height <= 0) return;
    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: CompressedTexSubImage with no storage (tex=%u)", tex->id);
        return;
    }
    if ((xoffset & 3) || (yoffset & 3)) {
        gldDiagLog("GL: CompressedTexSubImage offset (%d,%d) is not 4x4 block aligned, skipped",
                   xoffset, yoffset);
        return;
    }

    d3dFmt = _glsMapCompressedFormatToD3D(format);
    if (d3dFmt == D3DFMT_UNKNOWN) {
        gldDiagLog("GL: CompressedTexSubImage unknown format 0x%X, skipped", format);
        return;
    }

    /* Ask the surface how big the level really is, and refuse a rectangle that
     * does not fit — the same check _glsTexSubImage2D makes, for the same
     * reason: LockRect does not validate the rectangle, so an oversized one
     * succeeds, hands back a pointer into a smaller allocation, and the block
     * copy below writes past the end of it.  For a MANAGED texture that is
     * process heap, and the damage only surfaces later inside ntdll walking a
     * block whose links were overwritten, with nothing pointing back here.
     * This path had no check at all, which is why it is the one an overrun
     * still gets through on.
     *
     * The bound is block-rounded rather than the raw level size.  The tail of
     * a compressed mip chain is 2x2 and 1x1 logically, but a DXT level smaller
     * than one block still occupies — and is legitimately specified with — a
     * full 4x4 block, so comparing against desc.Width/Height directly would
     * reject correct uploads.  Rounding up to the block grid admits exactly
     * that case and nothing wider: a level created 256 wide still rejects the
     * 512-wide rectangle of a texture whose storage was allocated too small. */
    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: CompressedTexSubImage GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, (unsigned)hr);
        return;
    }

    limitW = (desc.Width  + 3u) & ~3u;
    limitH = (desc.Height + 3u) & ~3u;

    if (xoffset < 0 || yoffset < 0 ||
        (UINT)(xoffset + width)  > limitW ||
        (UINT)(yoffset + height) > limitH) {
        gldDiagLog("GL: CompressedTexSubImage rejected out-of-range rect tex=%u level=%d "
                   "(%d,%d %dx%d) into %ux%u (block-rounded %ux%u) - would overrun the level",
                   tex->id, level, xoffset, yoffset, width, height,
                   desc.Width, desc.Height, limitW, limitH);
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }

    rect.left = xoffset; rect.top = yoffset;
    rect.right = xoffset + width; rect.bottom = yoffset + height;

    if (!_glsLockTexLevel(tex, target, level, &rect, &lr)) {
        gldDiagLog("GL: CompressedTexSubImage lock failed tex=%u level=%d", tex->id, level);
        return;
    }

    blockWidth  = (width  + 3) / 4;
    blockHeight = (height + 3) / 4;
    blockSize   = _glsCompressedBlockSize(d3dFmt);
    srcRowBytes = blockWidth * blockSize;

    for (row = 0; row < blockHeight; row++) {
        int copySize = srcRowBytes;
        if (copySize > lr.Pitch) copySize = lr.Pitch;
        if ((row + 1) * srcRowBytes > imageSize) break;
        memcpy((unsigned char *)lr.pBits + (ptrdiff_t)row * lr.Pitch,
               (const unsigned char *)data + (ptrdiff_t)row * srcRowBytes,
               (size_t)copySize);
    }

    _glsUnlockTexLevel(tex, target, level);
    gldDiagLogV("GL: CompressedTexSubImage2D tex=%u level=%d (%d,%d) %dx%d fmt=0x%X",
               tex->id, level, xoffset, yoffset, width, height, format);
}

/*
 * Compressed volume textures.  D3D9 does allow DXT volume textures, and the
 * block layout of a slice is identical to the 2D case, so each z slice is a
 * plain block copy through its own LockBox.
 */
void _glsCompressedTexImage3D(unsigned int target, int level, unsigned int internalformat, int width, int height, int depth, int border, int imageSize, const void *data)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    int unit, z, blockWidth, blockHeight, blockSize, sliceBytes;
    HRESULT hr;

    (void)border;

    if (target != GL_TEXTURE_3D) {
        gldDiagLog("GL: CompressedTexImage3D target 0x%X is not GL_TEXTURE_3D, skipped", target);
        return;
    }

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;
    if (!pDev || width <= 0 || height <= 0 || depth <= 0) return;

    d3dFmt = _glsMapCompressedFormatToD3D(internalformat);
    if (d3dFmt == D3DFMT_UNKNOWN) {
        gldDiagLog("GL: CompressedTexImage3D unknown format 0x%X, skipped", internalformat);
        return;
    }

    if (level == 0) {
        int levels = _glsMipLevelCount(width, height);
        _glsReleaseTextureResources(tex);
        tex->width = width; tex->height = height; tex->depth = depth;
        tex->internalFormat = internalformat;
        tex->target = target;

        __try {
            hr = IDirect3DDevice9_CreateVolumeTexture(pDev, width, height, depth, levels, 0,
                                                      d3dFmt, D3DPOOL_MANAGED,
                                                      &tex->pVolTex, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (FAILED(hr)) {
            /* No software decompressor exists here, so a device that cannot
             * create the format leaves the texture with no data rather than
             * receiving block bytes reinterpreted as pixels. */
            gldDiagLog("GL: CompressedTexImage3D CreateVolumeTexture FAILED hr=0x%08X fmt=%d "
                       "- texture left without data", (unsigned)hr, d3dFmt);
            tex->pVolTex = NULL;
            return;
        }
    }

    if (!tex->pVolTex || !data || imageSize <= 0) return;

    blockWidth  = (width  + 3) / 4;
    blockHeight = (height + 3) / 4;
    blockSize   = _glsCompressedBlockSize(d3dFmt);
    sliceBytes  = blockWidth * blockHeight * blockSize;

    for (z = 0; z < depth; z++) {
        D3DLOCKED_BOX lb;
        D3DBOX box;
        int row;
        box.Left = 0; box.Top = 0;
        box.Right = (UINT)width; box.Bottom = (UINT)height;
        box.Front = (UINT)z; box.Back = (UINT)z + 1;

        if ((z + 1) * sliceBytes > imageSize) break;

        __try {
            hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb, &box, 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (FAILED(hr)) {
            gldDiagLog("GL: CompressedTexImage3D LockBox failed level=%d slice=%d hr=0x%08X",
                       level, z, (unsigned)hr);
            break;
        }
        for (row = 0; row < blockHeight; row++) {
            int copySize = blockWidth * blockSize;
            if (copySize > lb.RowPitch) copySize = lb.RowPitch;
            memcpy((unsigned char *)lb.pBits + (ptrdiff_t)row * lb.RowPitch,
                   (const unsigned char *)data + (ptrdiff_t)z * sliceBytes
                       + (ptrdiff_t)row * blockWidth * blockSize,
                   (size_t)copySize);
        }
        __try {
            IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    gldDiagLogV("GL: CompressedTexImage3D tex=%u level=%d %dx%dx%d fmt=0x%X size=%d",
               tex->id, level, width, height, depth, internalformat, imageSize);
}

void _glsCompressedTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, int imageSize, const void *data)
{
    _glsCompressedSubImage2D(target, level, xoffset, yoffset, width, height,
                             format, imageSize, data);
}

/* A 1D compressed texture is the 2D case one block row high. */
void _glsCompressedTexImage1D(unsigned int target, int level, unsigned int internalformat, int width, int border, int imageSize, const void *data)
{
    _glsCompressedTexImage2D(target, level, internalformat, width, 1, border, imageSize, data);
}

void _glsCompressedTexSubImage1D(unsigned int target, int level, int xoffset, int width, unsigned int format, int imageSize, const void *data)
{
    _glsCompressedSubImage2D(target, level, xoffset, 0, width, 1, format, imageSize, data);
}

void _glsCompressedTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, int imageSize, const void *data)
{
    GLS_Texture *tex;
    D3DFORMAT d3dFmt;
    int unit, z, blockWidth, blockHeight, blockSize, sliceBytes;
    HRESULT hr;

    if (target != GL_TEXTURE_3D) {
        gldDiagLog("GL: CompressedTexSubImage3D target 0x%X is not GL_TEXTURE_3D, skipped", target);
        return;
    }
    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || !data || level < 0 || imageSize <= 0) return;
    if (width <= 0 || height <= 0 || depth <= 0) return;
    if (!tex->pVolTex) {
        gldDiagLog("GL: CompressedTexSubImage3D with no storage (tex=%u)", tex->id);
        return;
    }
    if ((xoffset & 3) || (yoffset & 3)) {
        gldDiagLog("GL: CompressedTexSubImage3D offset (%d,%d) is not 4x4 block aligned, skipped",
                   xoffset, yoffset);
        return;
    }

    d3dFmt = _glsMapCompressedFormatToD3D(format);
    if (d3dFmt == D3DFMT_UNKNOWN) {
        gldDiagLog("GL: CompressedTexSubImage3D unknown format 0x%X, skipped", format);
        return;
    }

    blockWidth  = (width  + 3) / 4;
    blockHeight = (height + 3) / 4;
    blockSize   = _glsCompressedBlockSize(d3dFmt);
    sliceBytes  = blockWidth * blockHeight * blockSize;

    for (z = 0; z < depth; z++) {
        D3DLOCKED_BOX lb;
        D3DBOX box;
        int row;
        box.Left = (UINT)xoffset; box.Top = (UINT)yoffset;
        box.Right = (UINT)(xoffset + width); box.Bottom = (UINT)(yoffset + height);
        box.Front = (UINT)(zoffset + z); box.Back = (UINT)(zoffset + z + 1);

        if ((z + 1) * sliceBytes > imageSize) break;

        __try {
            hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb, &box, 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (FAILED(hr)) {
            gldDiagLog("GL: CompressedTexSubImage3D LockBox failed level=%d slice=%d hr=0x%08X",
                       level, z, (unsigned)hr);
            return;
        }
        for (row = 0; row < blockHeight; row++) {
            int copySize = blockWidth * blockSize;
            if (copySize > lb.RowPitch) copySize = lb.RowPitch;
            memcpy((unsigned char *)lb.pBits + (ptrdiff_t)row * lb.RowPitch,
                   (const unsigned char *)data + (ptrdiff_t)z * sliceBytes
                       + (ptrdiff_t)row * blockWidth * blockSize,
                   (size_t)copySize);
        }
        __try {
            IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    gldDiagLogV("GL: CompressedTexSubImage3D tex=%u level=%d (%d,%d,%d) %dx%dx%d",
               tex->id, level, xoffset, yoffset, zoffset, width, height, depth);
}

/*
 * glGetTexImage — read one mip level back into client memory.
 *
 * D3DPOOL_MANAGED keeps a system-memory copy of every level, so the level can
 * simply be locked for reading; no render-target round trip is involved.  Row
 * order is left alone: the upload path writes GL row 0 to surface row 0, so
 * reading it back the same way returns what was handed in.
 */
void _glsGetTexImage(unsigned int target, int level, unsigned int format,
                     unsigned int type, void *pixels)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    void *dst;
    int unit, w, h;
    HRESULT hr;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;

    dst = _glsResolvePackTarget(s, pixels);
    if (!dst) return;

    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY || tex->pVolTex) {
        D3DVOLUME_DESC vdesc;
        D3DLOCKED_BOX lb;
        int z, comps = 4, typeBytes = 1, dstSliceBytes;
        if (!tex->pVolTex) return;
        hr = IDirect3DVolumeTexture9_GetLevelDesc(tex->pVolTex, (UINT)level, &vdesc);
        if (FAILED(hr)) return;
        switch (format) {
        case GL_RED: case GL_GREEN: case GL_BLUE: case GL_ALPHA:
        case GL_LUMINANCE: case GL_DEPTH_COMPONENT: comps = 1; break;
        case GL_RG: case GL_LUMINANCE_ALPHA: case GL_DEPTH_STENCIL: comps = 2; break;
        case GL_RGB: case GL_BGR: comps = 3; break;
        default: comps = 4; break;
        }
        switch (type) {
        case GL_BYTE: case GL_UNSIGNED_BYTE: typeBytes = 1; break;
        case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: typeBytes = 2; break;
        case GL_DOUBLE: typeBytes = 8; break;
        default: typeBytes = 4; break;
        }
        dstSliceBytes = (int)vdesc.Width * (int)vdesc.Height * comps * typeBytes;
        __try { hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb, NULL, D3DLOCK_READONLY); }
        __except(EXCEPTION_EXECUTE_HANDLER) { hr = E_FAIL; }
        if (FAILED(hr)) return;
        for (z = 0; z < (int)vdesc.Depth; ++z)
            _glsCopyPixelsFromD3D((unsigned char *)dst + (ptrdiff_t)z * dstSliceBytes,
                                  (const unsigned char *)lb.pBits + (ptrdiff_t)z * lb.SlicePitch,
                                  (int)vdesc.Width, (int)vdesc.Height, format, type,
                                  lb.RowPitch, vdesc.Format, 0);
        __try { IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level); }
        __except(EXCEPTION_EXECUTE_HANDLER) { }
        return;
    }
    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: glGetTexImage tex=%u has no storage, no data written", tex->id);
        return;
    }

    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: glGetTexImage GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, (unsigned)hr);
        return;
    }

    w = (int)desc.Width;
    h = (int)desc.Height;

    if (!_glsLockTexLevel(tex, target, level, NULL, &lr)) {
        gldDiagLog("GL: glGetTexImage lock failed tex=%u level=%d", tex->id, level);
        return;
    }
    _glsCopyPixelsFromD3D(dst, lr.pBits, w, h, format, type, lr.Pitch, desc.Format, 0);
    _glsUnlockTexLevel(tex, target, level);

    gldDiagLogV("GL: glGetTexImage tex=%u level=%d %dx%d fmt=0x%X type=0x%X",
               tex->id, level, w, h, format, type);
}

/*
 * glGetCompressedTexImage — hand back the stored DXT blocks verbatim.
 */
/* Object-based, tightly-packed resource transfer used by the private
 * programmable-stage worker.  It never changes public texture bindings. */
BOOL _glsTransferTextureLevel(GLS_Texture *tex, unsigned int target, int level,
                              unsigned int format, unsigned int type, void *pixels,
                              BOOL writeToTexture, int *outWidth, int *outHeight,
                              int *outDepth)
{
    GLS_State *s = glsGetState();
    int oldPackAlignment, oldPackRowLength, oldUnpackAlignment, oldUnpackRowLength;
    HRESULT hr;

    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;
    if (outDepth) *outDepth = 0;
    if (!tex || level < 0 || !s) return FALSE;

    oldPackAlignment = s->packAlignment;
    oldPackRowLength = s->packRowLength;
    oldUnpackAlignment = s->unpackAlignment;
    oldUnpackRowLength = s->unpackRowLength;
    s->packAlignment = s->unpackAlignment = 1;
    s->packRowLength = s->unpackRowLength = 0;

    if (tex->pVolTex) {
        D3DVOLUME_DESC desc;
        D3DLOCKED_BOX lb;
        int z, bpp, sliceBytes;

        hr = IDirect3DVolumeTexture9_GetLevelDesc(tex->pVolTex, (UINT)level, &desc);
        if (FAILED(hr)) goto fail;
        if (outWidth) *outWidth = (int)desc.Width;
        if (outHeight) *outHeight = (int)desc.Height;
        if (outDepth) *outDepth = (int)desc.Depth;
        if (!pixels) goto success;
        bpp = _glsSourceBytesPerPixel(format, type);
        if (bpp <= 0) goto fail;
        sliceBytes = (int)desc.Width * (int)desc.Height * bpp;
        __try {
            hr = IDirect3DVolumeTexture9_LockBox(tex->pVolTex, (UINT)level, &lb,
                                                 NULL, writeToTexture ? 0 : D3DLOCK_READONLY);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hr = E_FAIL;
        }
        if (FAILED(hr)) goto fail;
        for (z = 0; z < (int)desc.Depth; ++z) {
            unsigned char *cpu = (unsigned char *)pixels + (ptrdiff_t)z * sliceBytes;
            unsigned char *d3d = (unsigned char *)lb.pBits + (ptrdiff_t)z * lb.SlicePitch;
            if (writeToTexture)
                _glsCopyPixelsToD3D(d3d, cpu, (int)desc.Width, (int)desc.Height,
                                    format, type, lb.RowPitch, desc.Format);
            else
                _glsCopyPixelsFromD3D(cpu, d3d, (int)desc.Width, (int)desc.Height,
                                      format, type, lb.RowPitch, desc.Format, 0);
        }
        __try { IDirect3DVolumeTexture9_UnlockBox(tex->pVolTex, (UINT)level); }
        __except(EXCEPTION_EXECUTE_HANDLER) { }
    } else {
        D3DSURFACE_DESC desc;
        D3DLOCKED_RECT lr;

        if (!tex->pTex && !tex->pCubeTex) goto fail;
        if (tex->pTex)
            hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
        else
            hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
        if (FAILED(hr)) goto fail;
        if (outWidth) *outWidth = (int)desc.Width;
        if (outHeight) *outHeight = (int)desc.Height;
        if (outDepth) *outDepth = 1;
        if (!pixels) goto success;
        if (!_glsLockTexLevel(tex, target, level, NULL, &lr)) goto fail;
        if (writeToTexture)
            _glsCopyPixelsToD3D(lr.pBits, pixels, (int)desc.Width, (int)desc.Height,
                                format, type, lr.Pitch, desc.Format);
        else
            _glsCopyPixelsFromD3D(pixels, lr.pBits, (int)desc.Width, (int)desc.Height,
                                  format, type, lr.Pitch, desc.Format, 0);
        _glsUnlockTexLevel(tex, target, level);
    }

success:
    s->packAlignment = oldPackAlignment;
    s->packRowLength = oldPackRowLength;
    s->unpackAlignment = oldUnpackAlignment;
    s->unpackRowLength = oldUnpackRowLength;
    return TRUE;

fail:
    s->packAlignment = oldPackAlignment;
    s->packRowLength = oldPackRowLength;
    s->unpackAlignment = oldUnpackAlignment;
    s->unpackRowLength = oldUnpackRowLength;
    return FALSE;
}

/* Hand back the stored DXT blocks verbatim. */
void _glsGetCompressedTexImage(unsigned int target, int level, void *img)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    unsigned char *dst;
    int unit, blockWidth, blockHeight, blockSize, row;
    HRESULT hr;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || level < 0) return;

    dst = (unsigned char *)_glsResolvePackTarget(s, img);
    if (!dst) return;

    if (!tex->pTex && !tex->pCubeTex) {
        gldDiagLog("GL: glGetCompressedTexImage tex=%u has no storage, no data written", tex->id);
        return;
    }

    if (tex->pTex)
        hr = IDirect3DTexture9_GetLevelDesc(tex->pTex, (UINT)level, &desc);
    else
        hr = IDirect3DCubeTexture9_GetLevelDesc(tex->pCubeTex, (UINT)level, &desc);
    if (FAILED(hr)) {
        gldDiagLog("GL: glGetCompressedTexImage GetLevelDesc failed tex=%u level=%d hr=0x%08X",
                   tex->id, level, (unsigned)hr);
        return;
    }

    if (desc.Format != D3DFMT_DXT1 && desc.Format != D3DFMT_DXT2 &&
        desc.Format != D3DFMT_DXT3 && desc.Format != D3DFMT_DXT4 &&
        desc.Format != D3DFMT_DXT5) {
        gldDiagLogV("GL: glGetCompressedTexImage tex=%u level=%d is not a compressed "
                   "level (d3d=%d), no data written", tex->id, level, (int)desc.Format);
        return;
    }

    if (!_glsLockTexLevel(tex, target, level, NULL, &lr)) {
        gldDiagLog("GL: glGetCompressedTexImage lock failed tex=%u level=%d", tex->id, level);
        return;
    }

    blockWidth  = ((int)desc.Width  + 3) / 4;
    blockHeight = ((int)desc.Height + 3) / 4;
    blockSize   = _glsCompressedBlockSize(desc.Format);
    for (row = 0; row < blockHeight; row++)
        memcpy(dst + (ptrdiff_t)row * blockWidth * blockSize,
               (const unsigned char *)lr.pBits + (ptrdiff_t)row * lr.Pitch,
               (size_t)(blockWidth * blockSize));

    _glsUnlockTexLevel(tex, target, level);
    gldDiagLogV("GL: glGetCompressedTexImage tex=%u level=%d %dx%d",
               tex->id, level, (int)desc.Width, (int)desc.Height);
}

/*
 * glGetTexLevelParameter / glGetTexParameter — answered from the tracked
 * texture object, which already carries everything these query.
 */
void _glsGetTexLevelParameteriv(unsigned int target, int level, unsigned int pname, int *params)
{
    GLS_Texture *tex;
    int unit;
    int shift;

    if (!params) return;
    *params = 0;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) return;

    /* Level n is level 0 halved n times, floored at 1. */
    shift = (level > 0) ? level : 0;

    switch (pname) {
    case 0x1000: /* GL_TEXTURE_WIDTH */
        *params = (tex->width >> shift) > 0 ? (tex->width >> shift) : 1;
        break;
    case 0x1001: /* GL_TEXTURE_HEIGHT */
        *params = (tex->height >> shift) > 0 ? (tex->height >> shift) : 1;
        break;
    case 0x8071: /* GL_TEXTURE_DEPTH */
        *params = (tex->depth > 0) ? ((tex->depth >> shift) > 0 ? (tex->depth >> shift) : 1) : 1;
        break;
    case 0x1003: /* GL_TEXTURE_INTERNAL_FORMAT / GL_TEXTURE_COMPONENTS */
        *params = (int)tex->internalFormat;
        break;
    case 0x1005: /* GL_TEXTURE_BORDER */
        *params = 0;
        break;
    case 0x86A1: /* GL_TEXTURE_COMPRESSED */
        *params = (_glsMapCompressedFormatToD3D(tex->internalFormat) != D3DFMT_UNKNOWN)
                  ? GL_TRUE : GL_FALSE;
        break;
    case 0x86A0: { /* GL_TEXTURE_COMPRESSED_IMAGE_SIZE */
        D3DFORMAT f = _glsMapCompressedFormatToD3D(tex->internalFormat);
        if (f == D3DFMT_UNKNOWN) { *params = 0; break; }
        {
            int w = (tex->width  >> shift) > 0 ? (tex->width  >> shift) : 1;
            int h = (tex->height >> shift) > 0 ? (tex->height >> shift) : 1;
            *params = ((w + 3) / 4) * ((h + 3) / 4) * _glsCompressedBlockSize(f);
        }
        break;
    }
    default:
        *params = 0;
        break;
    }
}

void _glsGetTexParameteriv(unsigned int target, unsigned int pname, int *params)
{
    GLS_Texture *tex;
    int unit;

    if (!params) return;
    *params = 0;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex) return;

    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: *params = (int)tex->minFilter; break;
    case GL_TEXTURE_MAG_FILTER: *params = (int)tex->magFilter; break;
    case GL_TEXTURE_WRAP_S:     *params = (int)tex->wrapS;     break;
    case GL_TEXTURE_WRAP_T:     *params = (int)tex->wrapT;     break;
    case GL_TEXTURE_WRAP_R:     *params = (int)tex->wrapR;     break;
    default:                    *params = 0;                   break;
    }
}

/*
 * glAreTexturesResident.
 *
 * D3D9 has no residency query, but it does not need one: every texture this
 * wrapper creates lives in D3DPOOL_MANAGED, which the runtime guarantees is
 * always usable for rendering.  So "resident" is exactly "has a D3D9 resource",
 * which is a real answer rather than a stubbed-out TRUE.
 */
unsigned char _glsAreTexturesResident(int n, const unsigned int *textures,
                                      unsigned char *residences)
{
    GLS_State *s = glsGetState();
    unsigned char allResident = GL_TRUE;
    int i;

    if (n < 0) { s->lastError = GL_INVALID_VALUE; return GL_FALSE; }
    if (!textures || !residences) return GL_FALSE;

    for (i = 0; i < n; i++) {
        GLS_Texture *tex = glsFindTexture(textures[i]);
        if (!tex) {
            /* Zero and never-generated names are errors, not "not resident". */
            s->lastError = GL_INVALID_VALUE;
            residences[i] = GL_FALSE;
            allResident = GL_FALSE;
            continue;
        }
        residences[i] = (tex->pTex || tex->pCubeTex || tex->pVolTex) ? GL_TRUE : GL_FALSE;
        if (!residences[i]) allResident = GL_FALSE;
    }

    return allResident;
}

/*
 * glCopyTexSubImage3D — framebuffer rectangle into one z slice.
 *
 * A single slice is a 2D image, so this is the framebuffer grab from
 * glCopyTexSubImage2D handed to the volume upload path for that one layer.
 */
void _glsCopyTexSubImage3D(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int x, int y, int width, int height)
{
    GLS_State *s = glsGetState();
    GLS_Texture *tex;
    unsigned char *buf;
    int unit, savedUnpackAlign, savedUnpackRow;

    tex = _glsBoundTextureForTarget(target, &unit);
    if (!tex || !tex->pVolTex) {
        gldDiagLog("GL: CopyTexSubImage3D — no 3D texture storage bound to target 0x%X, "
                   "slice %d of level %d skipped", target, zoffset, level);
        return;
    }

    buf = _glsGrabFramebufferRect(x, y, width, height);
    if (!buf) {
        gldDiagLogV("GL: CopyTexSubImage3D — framebuffer not readable, slice %d of level %d "
                   "left unchanged", zoffset, level);
        return;
    }

    savedUnpackAlign = s->unpackAlignment;
    savedUnpackRow   = s->unpackRowLength;
    s->unpackAlignment = 1;
    s->unpackRowLength = 0;
    _glsTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, 1,
                      GL_BGRA, GL_UNSIGNED_BYTE, buf);
    s->unpackAlignment = savedUnpackAlign;
    s->unpackRowLength = savedUnpackRow;

    free(buf);
}

void _glsDrawRangeElements(unsigned int mode, unsigned int start, unsigned int end, int count, unsigned int type, const void *indices)
{
    /* start/end are only a promise about the index range, offered so a driver
     * can pre-transform that slice.  Ignoring the hint is legal and changes
     * nothing about what is drawn, so this is glDrawElements.
     *
     * This mattered: id Tech 4 issues every one of its draws through
     * glDrawRangeElementsEXT, so while this discarded its arguments the
     * renderer produced nothing at all no matter what else was correct. */
    (void)start; (void)end;
    _glsDrawElements(mode, count, type, indices);
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
    GLS_State *s = glsGetState();
    static BOOL warned = FALSE;
    s->fogCoord = coord;
    /* GL replaces the computed fog distance with this per-vertex value.
     * The D3D vertex built by this wrapper has no fog channel, so the
     * value is tracked but not carried into the vertex - fog falls back
     * to distance-based, which is the closest available behaviour. */
    if (!warned) {
        warned = TRUE;
        gldDiagLog("GL: FogCoord set (%.3f) - vertex has no fog channel; "
                   "using distance-based fog instead", coord);
    }
}

void _glsFogCoordfv(const float *coord)
{
    if (coord) _glsFogCoordf(coord[0]);
}

void _glsFogCoordd(double coord)
{
    _glsFogCoordf((float)coord);
}

void _glsFogCoorddv(const double *coord)
{
    if (coord) _glsFogCoordf((float)coord[0]);
}

void _glsFogCoordPointer(unsigned int type, int stride, const void *pointer)
{
    static BOOL warned = FALSE;
    (void)type; (void)stride; (void)pointer;
    if (!warned) {
        warned = TRUE;
        gldDiagLog("GL: FogCoordPointer - vertex has no fog channel; "
                   "array ignored, using distance-based fog");
    }
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


void _glsMultiDrawArrays(unsigned int mode, const int *first, const int *count, int drawcount)
{
    int i;

    if (!first || !count || drawcount <= 0) return;

    for (i = 0; i < drawcount; i++) {
        if (count[i] > 0)
            _glsDrawArrays(mode, first[i], count[i]);
    }
}

void _glsMultiDrawElements(unsigned int mode, const int *count, unsigned int type, const void *const*indices, int drawcount)
{
    int i;

    if (!count || !indices || drawcount <= 0) return;

    for (i = 0; i < drawcount; i++) {
        if (count[i] > 0)
            _glsDrawElements(mode, count[i], type, indices[i]);
    }
}

/* float -> DWORD bit pattern, for the render states that take a float.
 * Local copy so this does not depend on where _glsFloatAsDword is defined. */
static DWORD _glsF2DW(float f)
{
    union { float f; DWORD d; } u;
    u.f = f;
    return u.d;
}

void _glsPointParameterf(unsigned int pname, float param)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    if (!pDev) return;

    switch (pname) {
    case GL_POINT_SIZE_MIN:
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSIZE_MIN, _glsF2DW(param));
        break;
    case GL_POINT_SIZE_MAX:
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSIZE_MAX, _glsF2DW(param));
        break;
    case GL_POINT_FADE_THRESHOLD_SIZE:
        /* D3D9 fades a point sprite by alpha below POINTSIZE_MIN rather than
         * exposing a separate threshold; there is no state to carry this. */
        gldDiagLog("GL: PointParameter GL_POINT_FADE_THRESHOLD_SIZE (%.3f) has no D3D9 state - ignored",
                   param);
        break;
    case GL_POINT_SPRITE_COORD_ORIGIN:
        /* D3D9 point sprite texcoords always originate upper-left. */
        if ((unsigned int)param != GL_UPPER_LEFT)
            gldDiagLog("GL: PointParameter COORD_ORIGIN 0x%X unsupported - D3D9 is always UPPER_LEFT",
                       (unsigned int)param);
        break;
    default:
        gldDiagLogV("GL: PointParameterf unhandled pname=0x%X", pname);
        break;
    }
}

void _glsPointParameterfv(unsigned int pname, const float *params)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    if (!params) return;

    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        /* GL attenuates size by 1/sqrt(a + b*d + c*d^2); D3D9 uses the same
         * three coefficients in POINTSCALE_A/B/C.  Scaling is only applied
         * when POINTSCALEENABLE is on, and constant attenuation (1,0,0) is
         * the GL default meaning "no distance attenuation", so leave it off
         * in that case to avoid paying for a no-op transform. */
        BOOL attenuates = (params[1] != 0.0f || params[2] != 0.0f);
        if (!pDev) return;
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSCALE_A, _glsF2DW(params[0]));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSCALE_B, _glsF2DW(params[1]));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSCALE_C, _glsF2DW(params[2]));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_POINTSCALEENABLE, attenuates ? TRUE : FALSE);
        return;
    }

    _glsPointParameterf(pname, params[0]);
}

void _glsPointParameteri(unsigned int pname, int param)
{
    _glsPointParameterf(pname, (float)param);
}

void _glsPointParameteriv(unsigned int pname, const int *params)
{
    float f[4];
    if (!params) return;
    f[0] = (float)params[0]; f[1] = (float)params[1];
    f[2] = (float)params[2]; f[3] = (float)params[3];
    _glsPointParameterfv(pname, f);
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

/*
 * Push the fog state to D3D9.
 *
 * Table (per-pixel) fog is used rather than vertex fog because GL's fog is
 * defined per fragment.  With table fog D3D9 interprets FOGSTART/FOGEND in
 * eye-space w on any device advertising W-fog, which is the same space GL
 * uses, so the distances pass through unchanged.
 *
 * D3D9 takes these float render states as raw bit patterns, hence the
 * type-punning through a DWORD.
 */
static DWORD _glsFloatAsDword(float f)
{
    DWORD d;
    memcpy(&d, &f, sizeof(d));
    return d;
}

void _glsApplyFogState(void)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD d3dMode;

    if (!pDev || !s) return;

    switch (s->fogMode) {
    case 0x0800: d3dMode = D3DFOG_EXP;    break;  /* GL_EXP */
    case 0x0801: d3dMode = D3DFOG_EXP2;   break;  /* GL_EXP2 */
    case 0x2601: d3dMode = D3DFOG_LINEAR; break;  /* GL_LINEAR */
    default:     d3dMode = D3DFOG_LINEAR; break;
    }

    __try {
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGENABLE, s->enableFog ? TRUE : FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGCOLOR, _glsPackColor(s->fogColor));

        /* Per-pixel fog; vertex fog stays off so the two cannot compound. */
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGTABLEMODE, d3dMode);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_RANGEFOGENABLE, FALSE);

        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGSTART,   _glsFloatAsDword(s->fogStart));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGEND,     _glsFloatAsDword(s->fogEnd));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGDENSITY, _glsFloatAsDword(s->fogDensity));
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

static void _setFogParam(GLS_State *s, unsigned int pname, float value)
{
    switch (pname) {
    case GL_FOG_MODE:    s->fogMode = (GLenum_t)(int)value; break;
    case GL_FOG_DENSITY: s->fogDensity = value; break;
    case GL_FOG_START:   s->fogStart = value; break;
    case GL_FOG_END:     s->fogEnd = value; break;
    default: break;
    }
    _glsApplyFogState();
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
        _glsApplyFogState();
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
        _glsApplyFogState();
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
static unsigned int _glsListBaseValue = 0;

void _glsNewList(unsigned int list, unsigned int mode)
{
    gldNewList46(list, mode);
}

void _glsEndList(void)
{
    gldEndList46();
}

void _glsCallList(unsigned int list)
{
    GLS_DLUInt2 a = {{ list, 0 }};
    if (!_glsDLRecord(_glsDLCallList, &a, sizeof(a)))
        gldCallList46(list);
}

void _glsCallLists(int n, unsigned int type, const void *lists)
{
    int i;
    if (n < 0 || (!lists && n)) {
        glsGetState()->lastError = GL_INVALID_VALUE;
        return;
    }
    for (i = 0; i < n; ++i) {
        unsigned int v = 0;
        switch (type) {
        case GL_BYTE:           v = (unsigned int)((const signed char *)lists)[i]; break;
        case GL_UNSIGNED_BYTE:  v = (unsigned int)((const unsigned char *)lists)[i]; break;
        case GL_SHORT:          v = (unsigned int)((const short *)lists)[i]; break;
        case GL_UNSIGNED_SHORT: v = (unsigned int)((const unsigned short *)lists)[i]; break;
        case GL_INT:            v = (unsigned int)((const int *)lists)[i]; break;
        case GL_UNSIGNED_INT:   v = ((const unsigned int *)lists)[i]; break;
        case GL_FLOAT:          v = (unsigned int)((const float *)lists)[i]; break;
        case 0x1407: {
            const unsigned char *p = (const unsigned char *)lists + i * 2;
            v = ((unsigned int)p[0] << 8) | p[1]; break;
        }
        case 0x1408: {
            const unsigned char *p = (const unsigned char *)lists + i * 3;
            v = ((unsigned int)p[0] << 16) | ((unsigned int)p[1] << 8) | p[2]; break;
        }
        case 0x1409: {
            const unsigned char *p = (const unsigned char *)lists + i * 4;
            v = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
                ((unsigned int)p[2] << 8) | p[3]; break;
        }
        default:
            glsGetState()->lastError = GL_INVALID_ENUM;
            return;
        }
        _glsCallList(_glsListBaseValue + v);
    }
}

unsigned int _glsGenLists(int range)
{
    return gldGenLists46(range);
}

void _glsDeleteLists(unsigned int list, int range)
{
    gldDeleteLists46(list, range);
}

void _glsListBase(unsigned int base)
{
    _glsListBaseValue = base;
}

unsigned char _glsIsList(unsigned int list)
{
    return gldIsList46(list) ? GL_TRUE : GL_FALSE;
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
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    _glsShadeModelMode = mode;

    if (!pDev) return;

    __try {
        /* GL_FLAT (0x1D00) / GL_SMOOTH (0x1D01) */
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_SHADEMODE,
            (mode == 0x1D00) ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

void _glsHint(unsigned int target, unsigned int mode)
{
    (void)target; (void)mode; /* Accept and ignore */
}

void _glsLogicOp(unsigned int opcode)
{
    _glsLogicOpMode = opcode;
}

/*
 * glReadBuffer — choose the colour buffer glReadPixels and glCopyTex*Image
 * take their pixels from.
 *
 * The read path (_glsReadRenderTarget) always reads D3D9 render target 0.
 * GL_BACK, GL_FRONT and GL_FRONT_AND_BACK all land there, because a D3D9
 * swap chain presents rather than exposing a separate front buffer for
 * reading.  GL_COLOR_ATTACHMENTn selects an attachment of the bound read
 * framebuffer, which is honoured by binding it as render target 0.
 */
void _glsReadBuffer(unsigned int mode)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    _glsReadBufferMode = mode;

    if (mode >= GL_COLOR_ATTACHMENT0 && mode < GL_COLOR_ATTACHMENT0 + 4) {
        int idx = (int)(mode - GL_COLOR_ATTACHMENT0);
        GLS_FBO *fbo = glsFindFBO(s->boundReadFBO);
        if (!fbo || !fbo->colorAttachment[idx]) {
            gldDiagLog("GL: glReadBuffer(GL_COLOR_ATTACHMENT%d) — read framebuffer %u "
                       "has no such attachment, read target unchanged",
                       idx, s->boundReadFBO);
            return;
        }
        if (idx != 0)
            gldDiagLogV("GL: glReadBuffer(GL_COLOR_ATTACHMENT%d) — only attachment 0 is "
                       "bound as a D3D9 render target here, reads will come from it",
                       idx);
        return;
    }

    if (mode != GL_BACK && mode != GL_FRONT && mode != GL_FRONT_AND_BACK && mode != GL_NONE)
        gldDiagLogV("GL: glReadBuffer(0x%X) is not a colour buffer this device exposes; "
                   "reads continue to come from render target 0", mode);

    (void)pDev;
}

/*
 * glDrawBuffer — choose which colour buffer draws land in.
 *
 * For the default framebuffer D3D9 offers exactly one: the current render
 * target, which is already what every draw targets, so GL_BACK and
 * GL_FRONT_AND_BACK need no device change.  GL_NONE has no D3D9 equivalent
 * short of a full colour write mask, which is what it is translated to so the
 * request is actually honoured rather than accepted and dropped.
 */
void _glsDrawBuffer(unsigned int mode)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_State *s = glsGetState();

    _glsDrawBufferMode = mode;

    if (!pDev) return;

    __try {
        if (mode == GL_NONE) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE, 0);
        } else {
            DWORD writeMask = (s->colorMask[0] ? D3DCOLORWRITEENABLE_RED   : 0) |
                              (s->colorMask[1] ? D3DCOLORWRITEENABLE_GREEN : 0) |
                              (s->colorMask[2] ? D3DCOLORWRITEENABLE_BLUE  : 0) |
                              (s->colorMask[3] ? D3DCOLORWRITEENABLE_ALPHA : 0);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE, writeMask);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    if (mode != GL_NONE && mode != GL_BACK && mode != GL_FRONT &&
        mode != GL_FRONT_AND_BACK &&
        !(mode >= GL_COLOR_ATTACHMENT0 && mode < GL_COLOR_ATTACHMENT0 + 4))
        gldDiagLogV("GL: glDrawBuffer(0x%X) is not a colour buffer this device exposes; "
                   "draws continue to go to render target 0", mode);
}

/* ===================================================================
 *  SECTION: Attribute stack (accept params, no-op)
 * =================================================================== */

#define GLS_ATTR_STACK_DEPTH 16
#define GLS_ATTR_A_BEGIN offsetof(GLS_State, clearColor)
#define GLS_ATTR_A_END offsetof(GLS_State, renderMode)
#define GLS_ATTR_A_BYTES (GLS_ATTR_A_END - GLS_ATTR_A_BEGIN)
#define GLS_ATTR_B_BEGIN offsetof(GLS_State, pixelMap)
#define GLS_ATTR_B_END offsetof(GLS_State, inBeginEnd)
#define GLS_ATTR_B_BYTES (GLS_ATTR_B_END - GLS_ATTR_B_BEGIN)

typedef struct {
    unsigned int mask;
    unsigned char stateA[GLS_ATTR_A_BYTES];
    unsigned char stateB[GLS_ATTR_B_BYTES];
    float currentColor[4], currentNormal[3];
    float currentTexCoord[GLS_MAX_TEX_UNITS][4];
    float accumClear[4];
    unsigned int tex2D[GLS_MAX_TEX_UNITS], texCube[GLS_MAX_TEX_UNITS];
    unsigned int activeTexture;
    unsigned int listBase;
    double clip[GLS_MAX_CLIP_PLANES][4];
} GLS_AttribFrame;

typedef struct {
    unsigned int mask;
    GLS_ClientArray vertex, normal, color, texcoord[GLS_MAX_TEX_UNITS];
    unsigned int activeTexture, arrayBuffer, elementBuffer, vao;
    int unpackAlignment, packAlignment, unpackRowLength, packRowLength;
} GLS_ClientAttribFrame;

static GLS_AttribFrame _glsAttribFrames[GLS_ATTR_STACK_DEPTH];
static GLS_ClientAttribFrame _glsClientAttribFrames[GLS_ATTR_STACK_DEPTH];
static int _glsAttribDepth, _glsClientAttribDepth;

void _glsPushAttrib(unsigned int mask)
{
    GLS_State *s = glsGetState();
    GLS_AttribFrame *f;
    if (_glsAttribDepth == GLS_ATTR_STACK_DEPTH) {
        s->lastError = GL_STACK_OVERFLOW;
        return;
    }
    f = &_glsAttribFrames[_glsAttribDepth++];
    f->mask = mask;
    memcpy(f->stateA, (const unsigned char *)s + GLS_ATTR_A_BEGIN, GLS_ATTR_A_BYTES);
    memcpy(f->stateB, (const unsigned char *)s + GLS_ATTR_B_BEGIN, GLS_ATTR_B_BYTES);
    memcpy(f->currentColor, s->currentColor, sizeof(f->currentColor));
    memcpy(f->currentNormal, s->currentNormal, sizeof(f->currentNormal));
    memcpy(f->currentTexCoord, s->currentTexCoord, sizeof(f->currentTexCoord));
    memcpy(f->accumClear, s->accumClear, sizeof(f->accumClear));
    memcpy(f->tex2D, s->boundTexture2D, sizeof(f->tex2D));
    memcpy(f->texCube, s->boundTextureCube, sizeof(f->texCube));
    f->activeTexture = s->activeTexUnit;
    f->listBase = _glsListBaseValue;
    memcpy(f->clip, _glsClipPlanes, sizeof(f->clip));
}

void _glsPopAttrib(void)
{
    GLS_State *s = glsGetState();
    GLS_AttribFrame *f;
    int i;
    if (_glsAttribDepth == 0) {
        s->lastError = GL_STACK_UNDERFLOW;
        return;
    }
    f = &_glsAttribFrames[--_glsAttribDepth];
    if (f->mask) {
        memcpy((unsigned char *)s + GLS_ATTR_A_BEGIN, f->stateA, GLS_ATTR_A_BYTES);
        memcpy((unsigned char *)s + GLS_ATTR_B_BEGIN, f->stateB, GLS_ATTR_B_BYTES);
        memcpy(s->currentColor, f->currentColor, sizeof(f->currentColor));
        memcpy(s->currentNormal, f->currentNormal, sizeof(f->currentNormal));
        memcpy(s->currentTexCoord, f->currentTexCoord, sizeof(f->currentTexCoord));
        memcpy(s->accumClear, f->accumClear, sizeof(f->accumClear));
        memcpy(s->boundTexture2D, f->tex2D, sizeof(f->tex2D));
        memcpy(s->boundTextureCube, f->texCube, sizeof(f->texCube));
        s->activeTexUnit = f->activeTexture;
        _glsListBaseValue = f->listBase;
        memcpy(_glsClipPlanes, f->clip, sizeof(f->clip));
    }
    _glsBlendFuncSeparate(s->blendSrcRGB, s->blendDstRGB,
                          s->blendSrcAlpha, s->blendDstAlpha);
    _glsBlendEquationSeparate(s->blendEquationRGB, s->blendEquationAlpha);
    _glsDepthFunc(s->depthFunc);
    _glsDepthMask(s->depthMask);
    _glsCullFace(s->cullFaceMode);
    _glsFrontFace(s->frontFace);
    _glsColorMask(s->colorMask[0], s->colorMask[1], s->colorMask[2], s->colorMask[3]);
    _glsPolygonMode(GL_FRONT_AND_BACK, s->polygonModeFront);
    _glsApplyStencilState();
    _glsApplyFogState();
    _glsViewport(s->viewportX, s->viewportY, s->viewportW, s->viewportH);
    _glsDepthRange(s->depthRangeNear, s->depthRangeFar);
    _glsScissor(s->scissorX, s->scissorY, s->scissorW, s->scissorH);
    for (i = 0; i < GLS_MAX_TEX_UNITS; ++i) {
        s->activeTexUnit = GL_TEXTURE0 + i;
        _glsBindTexture(GL_TEXTURE_2D, s->boundTexture2D[i]);
        _glsBindTexture(GL_TEXTURE_CUBE_MAP, s->boundTextureCube[i]);
    }
    s->activeTexUnit = f->activeTexture;
}

void _glsPushClientAttrib(unsigned int mask)
{
    GLS_State *s = glsGetState();
    GLS_ClientAttribFrame *f;
    if (_glsClientAttribDepth == GLS_ATTR_STACK_DEPTH) {
        s->lastError = GL_STACK_OVERFLOW;
        return;
    }
    f = &_glsClientAttribFrames[_glsClientAttribDepth++];
    f->mask = mask;
    f->vertex = s->clientVertexArray; f->normal = s->clientNormalArray;
    f->color = s->clientColorArray;
    memcpy(f->texcoord, s->clientTexCoordArray, sizeof(f->texcoord));
    f->activeTexture = s->clientActiveTexUnit;
    f->arrayBuffer = s->boundArrayBuffer; f->elementBuffer = s->boundElementBuffer;
    f->vao = s->boundVAO;
    f->unpackAlignment = s->unpackAlignment; f->packAlignment = s->packAlignment;
    f->unpackRowLength = s->unpackRowLength; f->packRowLength = s->packRowLength;
}

void _glsPopClientAttrib(void)
{
    GLS_State *s = glsGetState();
    GLS_ClientAttribFrame *f;
    if (_glsClientAttribDepth == 0) {
        s->lastError = GL_STACK_UNDERFLOW;
        return;
    }
    f = &_glsClientAttribFrames[--_glsClientAttribDepth];
    if (f->mask & 1u) {
        s->unpackAlignment = f->unpackAlignment; s->packAlignment = f->packAlignment;
        s->unpackRowLength = f->unpackRowLength; s->packRowLength = f->packRowLength;
    }
    if (f->mask & 2u) {
        s->clientVertexArray = f->vertex; s->clientNormalArray = f->normal;
        s->clientColorArray = f->color;
        memcpy(s->clientTexCoordArray, f->texcoord, sizeof(f->texcoord));
        s->clientActiveTexUnit = f->activeTexture;
        s->boundArrayBuffer = f->arrayBuffer; s->boundElementBuffer = f->elementBuffer;
        s->boundVAO = f->vao;
    }
}

/* ===================================================================
 *  SECTION 33: Draw Calls
 * =================================================================== */

/*
 * Fill in the synthesized _glsl_texdim_* constants for the bound program.
 *
 * A shader that used texelFetch or textureSize was lowered onto tex2Dlod plus
 * a (width, height, 1/width, 1/height) constant per sampler, and nothing in GL
 * ever writes that constant — the size is implied by whichever texture happens
 * to be bound when the draw is issued, so it has to be pushed per draw.
 *
 * The dimensions come from the CPU-side GLS_Texture record, so this costs no
 * D3D9 round-trip.  Programs that use neither builtin have texDimCount == 0 and
 * pay one comparison, which is every program in evidence today.
 */
static void _glsPushTexDimConstants(IDirect3DDevice9 *pDev, GLS_State *s)
{
    GLS_Program *prog = _getBoundProgram();
    int i;

    if (!prog) return;
    if (prog->viewportRegister >= 0) {
        D3DVIEWPORT9 viewport;
        float adjust[4];
        if (_glsBuildClippedViewport(pDev, s, &viewport, adjust)) {
            __try {
                IDirect3DDevice9_SetVertexShaderConstantF(
                    pDev, prog->viewportRegister, adjust, 1);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
    if (prog->texDimCount <= 0) return;

    for (i = 0; i < prog->texDimCount; i++) {
        const GLS_TexDimBinding *b = &prog->texDim[i];
        GLS_Texture *tex;
        float dim[4];
        int unit;

        if (b->samplerPsRegister < 0 || b->samplerPsRegister >= GLS_MAX_TEX_UNITS)
            continue;

        /* glUniform1i on the sampler recorded which GL unit feeds this D3D9
         * stage; until it does, the stage reads unit 0. */
        unit = s->samplerStageUnit[b->samplerPsRegister];
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) continue;

        tex = glsFindTexture(s->boundTexture2D[unit]);
        if (!tex || tex->width <= 0 || tex->height <= 0) continue;

        dim[0] = (float)tex->width;
        dim[1] = (float)tex->height;
        dim[2] = 1.0f / (float)tex->width;
        dim[3] = 1.0f / (float)tex->height;

        __try {
            if (b->vsRegister >= 0)
                IDirect3DDevice9_SetVertexShaderConstantF(pDev, b->vsRegister, dim, 1);
            if (b->psRegister >= 0)
                IDirect3DDevice9_SetPixelShaderConstantF(pDev, b->psRegister, dim, 1);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }
}

/*
 * glDrawArrays — assemble `count` sequential vertices starting at `first`
 * and submit them as one indexed D3D9 draw.
 */
static int _glsStageListIndexCount(unsigned int mode, int count)
{
    switch (mode) {
    case GL_POINTS: return count;
    case GL_LINES: return (count / 2) * 2;
    case GL_LINE_STRIP: return count > 1 ? (count - 1) * 2 : 0;
    case GL_LINE_LOOP: return count > 1 ? count * 2 : 0;
    case GL_TRIANGLES: return (count / 3) * 3;
    case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN: case GL_POLYGON:
        return count > 2 ? (count - 2) * 3 : 0;
    case GL_QUADS: return (count / 4) * 6;
    case GL_QUAD_STRIP: return count >= 4 ? ((count / 2) - 1) * 6 : 0;
    default: return 0;
    }
}

static int _glsBuildStageListIndices(unsigned int mode, int count,
                                     unsigned int *indices,
                                     D3DPRIMITIVETYPE *type, int *arity)
{
    int i, n = 0;
    switch (mode) {
    case GL_POINTS:
        *type = D3DPT_POINTLIST; *arity = 1;
        for (i = 0; i < count; ++i) indices[n++] = (unsigned int)i;
        break;
    case GL_LINES:
        *type = D3DPT_LINELIST; *arity = 2;
        for (i = 0; i + 1 < count; i += 2) {
            indices[n++] = i; indices[n++] = i + 1;
        }
        break;
    case GL_LINE_STRIP:
        *type = D3DPT_LINELIST; *arity = 2;
        for (i = 0; i + 1 < count; ++i) {
            indices[n++] = i; indices[n++] = i + 1;
        }
        break;
    case GL_LINE_LOOP:
        *type = D3DPT_LINELIST; *arity = 2;
        for (i = 0; i < count; ++i) {
            indices[n++] = i; indices[n++] = (i + 1) % count;
        }
        break;
    case GL_TRIANGLES:
        *type = D3DPT_TRIANGLELIST; *arity = 3;
        for (i = 0; i + 2 < count; i += 3) {
            indices[n++] = i; indices[n++] = i + 1; indices[n++] = i + 2;
        }
        break;
    case GL_TRIANGLE_STRIP:
        *type = D3DPT_TRIANGLELIST; *arity = 3;
        for (i = 0; i + 2 < count; ++i) {
            if (i & 1) {
                indices[n++] = i + 1; indices[n++] = i; indices[n++] = i + 2;
            } else {
                indices[n++] = i; indices[n++] = i + 1; indices[n++] = i + 2;
            }
        }
        break;
    case GL_TRIANGLE_FAN: case GL_POLYGON:
        *type = D3DPT_TRIANGLELIST; *arity = 3;
        for (i = 1; i + 1 < count; ++i) {
            indices[n++] = 0; indices[n++] = i; indices[n++] = i + 1;
        }
        break;
    case GL_QUADS:
        *type = D3DPT_TRIANGLELIST; *arity = 3;
        for (i = 0; i + 3 < count; i += 4) {
            indices[n++] = i; indices[n++] = i + 1; indices[n++] = i + 2;
            indices[n++] = i; indices[n++] = i + 2; indices[n++] = i + 3;
        }
        break;
    case GL_QUAD_STRIP:
        *type = D3DPT_TRIANGLELIST; *arity = 3;
        for (i = 0; i + 3 < count; i += 2) {
            indices[n++] = i; indices[n++] = i + 1; indices[n++] = i + 3;
            indices[n++] = i; indices[n++] = i + 3; indices[n++] = i + 2;
        }
        break;
    default: break;
    }
    return n;
}

static BOOL _glsSubmitStageDraw(IDirect3DDevice9 *pDev, GLS_State *s,
                                GLS_Program *program, GLD_StageDraw *draw)
{
    D3DPRIMITIVETYPE primitiveType;
    GLS_PostStageVertex *vertices;
    unsigned int *indices;
    int indexCount, arity, i, j, primitiveCount, groupStart;
    BOOL submitted = TRUE;
    D3DVIEWPORT9 viewport;
    float viewportAdjust[4];

    if (!draw || !draw->vertices || !draw->vertexCount ||
        !_glsEnsurePostStagePipeline(pDev)) return FALSE;
    if (!_glsBuildClippedViewport(pDev, s, &viewport, viewportAdjust))
        return FALSE;
    indexCount = 0;
    for (groupStart = 0; groupStart < (int)draw->vertexCount; ) {
        int groupEnd = groupStart + 1;
        while (groupEnd < (int)draw->vertexCount &&
               draw->vertices[groupEnd].primitiveSerial ==
               draw->vertices[groupStart].primitiveSerial)
            ++groupEnd;
        indexCount += _glsStageListIndexCount(draw->primitiveMode,
                                              groupEnd - groupStart);
        groupStart = groupEnd;
    }
    if (indexCount <= 0) return FALSE;
    indices = (unsigned int *)malloc((size_t)indexCount * sizeof(*indices));
    vertices = (GLS_PostStageVertex *)malloc((size_t)indexCount * sizeof(*vertices));
    if (!indices || !vertices) { free(indices); free(vertices); return FALSE; }
    indexCount = 0;
    for (groupStart = 0; groupStart < (int)draw->vertexCount; ) {
        int groupEnd = groupStart + 1;
        int firstBuilt = indexCount;
        while (groupEnd < (int)draw->vertexCount &&
               draw->vertices[groupEnd].primitiveSerial ==
               draw->vertices[groupStart].primitiveSerial)
            ++groupEnd;
        indexCount += _glsBuildStageListIndices(draw->primitiveMode,
                                                groupEnd - groupStart,
                                                indices + indexCount,
                                                &primitiveType, &arity);
        for (i = firstBuilt; i < indexCount; ++i)
            indices[i] += (unsigned int)groupStart;
        groupStart = groupEnd;
    }
    if (indexCount <= 0) { free(indices); free(vertices); return FALSE; }

    for (i = 0; i < indexCount; i += arity) {
        int provoking = (s->provokingVertexMode == GL_FIRST_VERTEX_CONVENTION)
                      ? 0 : arity - 1;
        for (j = 0; j < arity; ++j) {
            GLD_StageVertex *source = &draw->vertices[indices[i + j]];
            int varying;
            memcpy(vertices[i + j].position, source->position,
                   sizeof(vertices[i + j].position));
            memcpy(vertices[i + j].varying, source->varying,
                   sizeof(vertices[i + j].varying));
            for (varying = 0; varying < program->stageVaryingCount; ++varying) {
                if (program->stageVaryings[varying].isFlat)
                    memcpy(vertices[i + j].varying[varying],
                           draw->vertices[indices[i + provoking]].varying[varying],
                           sizeof(vertices[i + j].varying[varying]));
            }
            if (s->clipOrigin == GL_UPPER_LEFT)
                vertices[i + j].position[1] = -vertices[i + j].position[1];
            if (s->clipDepthMode != GL_ZERO_TO_ONE)
                vertices[i + j].position[2] = 0.5f *
                    (vertices[i + j].position[2] + vertices[i + j].position[3]);
            {
                float clipX = vertices[i + j].position[0];
                float clipY = vertices[i + j].position[1];
                float clipW = vertices[i + j].position[3];
                vertices[i + j].position[0] =
                    viewportAdjust[0] * clipX + viewportAdjust[2] * clipW;
                vertices[i + j].position[1] =
                    viewportAdjust[1] * clipY + viewportAdjust[3] * clipW;
            }
        }
    }
    primitiveCount = indexCount / arity;
    __try {
        if (FAILED(IDirect3DDevice9_SetViewport(pDev, &viewport)) ||
            FAILED(IDirect3DDevice9_SetFVF(pDev, 0)) ||
            FAILED(IDirect3DDevice9_SetVertexDeclaration(pDev, g_postStageDecl)) ||
            FAILED(IDirect3DDevice9_SetVertexShader(pDev, g_postStageVS)) ||
            FAILED(IDirect3DDevice9_SetPixelShader(pDev, program->pPS)) ||
            FAILED(IDirect3DDevice9_DrawPrimitiveUP(pDev, primitiveType,
                                                    primitiveCount, vertices,
                                                    sizeof(*vertices))))
            submitted = FALSE;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        free(indices); free(vertices); return FALSE;
    }
    free(indices); free(vertices);
    return submitted;
}

static BOOL _glsSubmitSoftwareFragmentDraw(IDirect3DDevice9 *pDev,
                                           GLS_State *s,
                                           GLS_Program *program,
                                           unsigned int mode,
                                           int first, int count,
                                           unsigned int indexType,
                                           const void *indices,
                                           int baseVertex,
                                           int instanceCount,
                                           unsigned int baseInstance,
                                           GLD_StageDraw *proxyDraw)
{
    IDirect3DSurface9 *rt = NULL;
    D3DSURFACE_DESC desc;
    unsigned char *initialBGRA = NULL;
    unsigned char *resultBGRA = NULL;
    DWORD oldColorWrite = 0, oldZWrite = 0, oldZEnable = 0;
    DWORD oldStencil = 0, oldBlend = 0;
    BOOL haveRenderStates = FALSE;
    float savedRasterPos[4];
    float savedPixelZoomX, savedPixelZoomY;
    GLboolean_t savedRasterValid;
    GLuint_t savedUnpackBuffer;
    int savedUnpackAlignment, savedUnpackRowLength;
    size_t bytes;
    char fragmentLog[512] = "";
    HRESULT hr;
    BOOL ok = FALSE;

    if (!pDev || !s || !program || !program->softwareFragmentExecution ||
        !proxyDraw || !proxyDraw->vertices || !proxyDraw->vertexCount)
        return FALSE;

    __try {
        hr = IDirect3DDevice9_GetRenderTarget(pDev, 0, &rt);
        if (SUCCEEDED(hr)) _glsSurfAcquired(rt, "SoftwareFragment/GetRenderTarget");
        if (FAILED(hr) || !rt || FAILED(IDirect3DSurface9_GetDesc(rt, &desc))) {
            if (rt) _glsSurfRel(rt);
            return FALSE;
        }
        _glsSurfRel(rt);
        rt = NULL;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        if (rt) _glsSurfRel(rt);
        return FALSE;
    }
    if (!desc.Width || !desc.Height ||
        (size_t)desc.Width > ((size_t)-1) / 4u / (size_t)desc.Height)
        return FALSE;
    bytes = (size_t)desc.Width * (size_t)desc.Height * 4u;
    initialBGRA = _glsGrabFramebufferRect(0, 0, (int)desc.Width,
                                          (int)desc.Height);
    resultBGRA = (unsigned char *)malloc(bytes);
    if (!initialBGRA || !resultBGRA) goto done;

    if (!gldFragmentEmulatorDraw(program, mode, first, count, indexType,
                                 indices, baseVertex, instanceCount,
                                 baseInstance, (int)desc.Width,
                                 (int)desc.Height, initialBGRA, resultBGRA,
                                 fragmentLog, sizeof(fragmentLog))) {
        gldDiagLog("GL: software fragment draw failed: %s", fragmentLog);
        goto done;
    }

    /* Remix must still observe geometry and state through D3D9.  The proxy
     * draw carries post-GL-stage positions but cannot be allowed to replace
     * the software-computed colour or mutate the D3D depth/stencil buffers. */
    __try {
        haveRenderStates = SUCCEEDED(IDirect3DDevice9_GetRenderState(
                               pDev, D3DRS_COLORWRITEENABLE, &oldColorWrite)) &&
                           SUCCEEDED(IDirect3DDevice9_GetRenderState(
                               pDev, D3DRS_ZWRITEENABLE, &oldZWrite)) &&
                           SUCCEEDED(IDirect3DDevice9_GetRenderState(
                               pDev, D3DRS_ZENABLE, &oldZEnable)) &&
                           SUCCEEDED(IDirect3DDevice9_GetRenderState(
                               pDev, D3DRS_STENCILENABLE, &oldStencil)) &&
                           SUCCEEDED(IDirect3DDevice9_GetRenderState(
                               pDev, D3DRS_ALPHABLENDENABLE, &oldBlend));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE, 0);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE, FALSE);
        _glsSubmitStageDraw(pDev, s, program, proxyDraw);
        if (haveRenderStates) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE,
                                            oldColorWrite);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZWRITEENABLE,
                                            oldZWrite);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, oldZEnable);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILENABLE,
                                            oldStencil);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE,
                                            oldBlend);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        if (haveRenderStates) {
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORWRITEENABLE,
                                            oldColorWrite);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZWRITEENABLE,
                                            oldZWrite);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, oldZEnable);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_STENCILENABLE,
                                            oldStencil);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE,
                                            oldBlend);
        }
    }

    memcpy(savedRasterPos, s->rasterPos, sizeof(savedRasterPos));
    savedRasterValid = s->rasterPosValid;
    savedPixelZoomX = s->pixelZoomX;
    savedPixelZoomY = s->pixelZoomY;
    savedUnpackBuffer = s->boundPixelUnpackBuffer;
    savedUnpackAlignment = s->unpackAlignment;
    savedUnpackRowLength = s->unpackRowLength;
    s->rasterPos[0] = 0.0f;
    s->rasterPos[1] = 0.0f;
    s->rasterPos[2] = 0.0f;
    s->rasterPos[3] = 1.0f;
    s->rasterPosValid = GL_TRUE;
    s->pixelZoomX = 1.0f;
    s->pixelZoomY = 1.0f;
    s->boundPixelUnpackBuffer = 0;
    s->unpackAlignment = 1;
    s->unpackRowLength = 0;
    _glsDrawPixels((int)desc.Width, (int)desc.Height, GL_BGRA,
                   GL_UNSIGNED_BYTE, resultBGRA);
    memcpy(s->rasterPos, savedRasterPos, sizeof(savedRasterPos));
    s->rasterPosValid = savedRasterValid;
    s->pixelZoomX = savedPixelZoomX;
    s->pixelZoomY = savedPixelZoomY;
    s->boundPixelUnpackBuffer = savedUnpackBuffer;
    s->unpackAlignment = savedUnpackAlignment;
    s->unpackRowLength = savedUnpackRowLength;
    ok = TRUE;

done:
    free(initialBGRA);
    free(resultBGRA);
    return ok;
}

static BOOL _glsRunInstancedStageDraw(unsigned int mode, int first, int count,
                                      unsigned int indexType, const void *indices,
                                      int baseVertex, int instanceCount,
                                      unsigned int baseInstance)
{
    GLS_State *s = glsGetState();
    GLS_Program *program = _getBoundProgram();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLD_StageDraw stageDraw;
    char stageLog[512] = "";

    if (!program) return FALSE; /* fixed-function fallback remains a draw loop */
    if (instanceCount < 0) {
        s->lastError = GL_INVALID_VALUE;
        return TRUE;
    }
    if (!pDev || count <= 0 || instanceCount == 0) return TRUE;

    if (!program->softwareVertexExecution) {
        program->softwareVertexExecution = TRUE;
        if (!gldStageEmulatorLinkGraphics(program, program->infoLog,
                                          sizeof(program->infoLog))) {
            s->lastError = GL_INVALID_OPERATION;
            gldDiagLog("GL: instanced stage link failed: %s", program->infoLog);
            return TRUE;
        }
    }
    if (!gldStageEmulatorDraw(program, mode, first, count, indexType, indices,
                              baseVertex, instanceCount, baseInstance,
                              &stageDraw, stageLog, sizeof(stageLog))) {
        s->lastError = GL_INVALID_OPERATION;
        gldDiagLog("GL: instanced software stage draw failed: %s", stageLog);
        return TRUE;
    }
    gldAdvRecordTransformFeedbackDraw((GLenum)stageDraw.primitiveMode, 0,
                                       (GLsizei)stageDraw.vertexCount);
    _glsPushTexDimConstants(pDev, s);
    if (!s->enableRasterizerDiscard && _glsApplyTransforms(pDev, s)) {
        if (program->softwareFragmentExecution)
            _glsSubmitSoftwareFragmentDraw(pDev, s, program, mode, first,
                                           count, indexType, indices,
                                           baseVertex, instanceCount,
                                           baseInstance, &stageDraw);
        else
            _glsSubmitStageDraw(pDev, s, program, &stageDraw);
    }
    gldStageEmulatorFreeDraw(&stageDraw);
    return TRUE;
}

void _glsDrawArrays(unsigned int mode, int first, int count)
{
    GLS_State *s = glsGetState();
    GLS_Program *program = _getBoundProgram();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    GLS_VertexSources src;
    GLS_D3DVertex *verts;
    unsigned int *idx;
    int indexCount, primCount, i;

    if (!pDev || count <= 0) return;

    if (program && (program->softwareGraphicsStages ||
                    (s->transformFeedbackActive &&
                     program->transformFeedbackCount > 0))) {
        GLD_StageDraw stageDraw;
        char stageLog[512] = "";
        if (!gldStageEmulatorDraw(program, mode, first, count, 0, NULL, 0, 1, 0,
                                  &stageDraw, stageLog, sizeof(stageLog))) {
            s->lastError = GL_INVALID_OPERATION;
            gldDiagLog("GL: software stage DrawArrays failed: %s", stageLog);
            return;
        }
        if (program->softwareGraphicsStages) {
            gldAdvRecordTransformFeedbackDraw((GLenum)stageDraw.primitiveMode, 0,
                                               (GLsizei)stageDraw.vertexCount);
            _glsPushTexDimConstants(pDev, s);
            if (!s->enableRasterizerDiscard && _glsApplyTransforms(pDev, s)) {
                if (program->softwareFragmentExecution)
                    _glsSubmitSoftwareFragmentDraw(pDev, s, program, mode,
                                                   first, count, 0, NULL, 0,
                                                   1, 0, &stageDraw);
                else
                    _glsSubmitStageDraw(pDev, s, program, &stageDraw);
            }
            gldStageEmulatorFreeDraw(&stageDraw);
            return;
        }
        gldStageEmulatorFreeDraw(&stageDraw);
    }

    gldAdvRecordTransformFeedbackDraw((GLenum)mode, first, count);

    if (s->enableRasterizerDiscard) return;

    _glsPushTexDimConstants(pDev, s);

    indexCount = _glsExpandedIndexCount(mode, count);
    if (indexCount <= 0) {
        gldDiagLog("GL: glDrawArrays unsupported mode 0x%X", mode);
        return;
    }

    if (!_glsApplyTransforms(pDev, s))
        return;

    _glsResolveVertexSources(s, &src);
    if (!src.pos.present) {
        /* No position data bound — nothing meaningful to draw. */
        gldDiagLogV("GL: glDrawArrays(0x%X, %d, %d) with no position array", mode, first, count);
        return;
    }

    verts = (GLS_D3DVertex *)malloc((size_t)count * sizeof(GLS_D3DVertex));
    if (!verts) return;

    idx = (unsigned int *)malloc((size_t)indexCount * sizeof(unsigned int));
    if (!idx) { free(verts); return; }

    for (i = 0; i < count; i++)
        _glsBuildVertex(s, &src, first + i, &verts[i]);

    primCount = _glsExpandPrimitive(mode, count, &d3dPrimType, idx);
    if (primCount > 0)
        _glsSubmitIndexed(pDev, d3dPrimType, primCount, verts, count, idx, indexCount);

    free(idx);
    free(verts);
}

/*
 * glDrawElements — assemble the vertices referenced by the index array and
 * submit them as one indexed D3D9 draw.
 *
 * The GL indices are remapped onto a compacted vertex array rather than
 * passed through: the primitive expansion produces indices into the vertices
 * we assemble, so quads and line loops work here exactly as in DrawArrays.
 */
void _glsDrawElementsBaseVertex(unsigned int mode, int count, unsigned int type,
                                const void *indices, int basevertex)
{
    GLS_State *s = glsGetState();
    GLS_Program *program = _getBoundProgram();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    D3DPRIMITIVETYPE d3dPrimType;
    GLS_VertexSources src;
    GLS_Buffer *ibo;
    const void *indexData;
    GLS_D3DVertex *verts;
    unsigned int *glIndices, *expanded, *final;
    int indexCount, primCount, i;

    if (!pDev || count <= 0) return;

    if (program && (program->softwareGraphicsStages ||
                    (s->transformFeedbackActive &&
                     program->transformFeedbackCount > 0))) {
        GLD_StageDraw stageDraw;
        char stageLog[512] = "";
        if (!gldStageEmulatorDraw(program, mode, 0, count, type, indices,
                                  basevertex, 1, 0, &stageDraw, stageLog,
                                  sizeof(stageLog))) {
            s->lastError = GL_INVALID_OPERATION;
            gldDiagLog("GL: software stage DrawElements failed: %s", stageLog);
            return;
        }
        if (program->softwareGraphicsStages) {
            gldAdvRecordTransformFeedbackDraw((GLenum)stageDraw.primitiveMode, 0,
                                               (GLsizei)stageDraw.vertexCount);
            _glsPushTexDimConstants(pDev, s);
            if (!s->enableRasterizerDiscard && _glsApplyTransforms(pDev, s)) {
                if (program->softwareFragmentExecution)
                    _glsSubmitSoftwareFragmentDraw(pDev, s, program, mode, 0,
                                                   count, type, indices,
                                                   basevertex, 1, 0,
                                                   &stageDraw);
                else
                    _glsSubmitStageDraw(pDev, s, program, &stageDraw);
            }
            gldStageEmulatorFreeDraw(&stageDraw);
            return;
        }
        gldStageEmulatorFreeDraw(&stageDraw);
    }

    gldAdvRecordTransformFeedbackDraw((GLenum)mode, 0, count);

    if (s->enableRasterizerDiscard) return;

    _glsPushTexDimConstants(pDev, s);

    indexCount = _glsExpandedIndexCount(mode, count);
    if (indexCount <= 0) {
        gldDiagLog("GL: glDrawElements unsupported mode 0x%X", mode);
        return;
    }

    /* Resolve index data: an element buffer binding makes `indices` an
     * offset into that buffer, otherwise it is a client pointer. */
    ibo = glsFindBuffer(s->boundElementBuffer);
    indexData = (ibo && ibo->data)
              ? (const void *)((const char *)ibo->data + (ptrdiff_t)indices)
              : indices;
    if (!indexData) return;

    if (!_glsApplyTransforms(pDev, s))
        return;

    _glsResolveVertexSources(s, &src);
    if (!src.pos.present) {
        gldDiagLogV("GL: glDrawElements(0x%X, %d) with no position array", mode, count);
        return;
    }

    /* Widen the application's indices to 32-bit so one code path covers all
     * three GL index types. */
    glIndices = (unsigned int *)malloc((size_t)count * sizeof(unsigned int));
    if (!glIndices) return;

    for (i = 0; i < count; i++) {
        switch (type) {
        case GL_UNSIGNED_BYTE:  glIndices[i] = ((const unsigned char  *)indexData)[i]; break;
        case GL_UNSIGNED_SHORT: glIndices[i] = ((const unsigned short *)indexData)[i]; break;
        case GL_UNSIGNED_INT:   glIndices[i] = ((const unsigned int   *)indexData)[i]; break;
        default:
            gldDiagLogV("GL: glDrawElements bad index type 0x%X", type);
            free(glIndices);
            return;
        }
        if (basevertex < 0 && glIndices[i] < (unsigned int)(-basevertex)) {
            glsGetState()->lastError = GL_INVALID_OPERATION;
            free(glIndices);
            return;
        }
        glIndices[i] = (unsigned int)((int64_t)glIndices[i] + basevertex);
    }

    /* One assembled vertex per element reference.  This duplicates shared
     * vertices, but keeps the expansion indices valid for every mode and
     * avoids walking the index array to find its maximum. */
    verts = (GLS_D3DVertex *)malloc((size_t)count * sizeof(GLS_D3DVertex));
    if (!verts) { free(glIndices); return; }

    for (i = 0; i < count; i++)
        _glsBuildVertex(s, &src, (int)glIndices[i], &verts[i]);

    expanded = (unsigned int *)malloc((size_t)indexCount * sizeof(unsigned int));
    if (!expanded) { free(verts); free(glIndices); return; }

    primCount = _glsExpandPrimitive(mode, count, &d3dPrimType, expanded);
    if (primCount > 0) {
        final = expanded;  /* already indexes the compacted array */
        _glsSubmitIndexed(pDev, d3dPrimType, primCount, verts, count, final, indexCount);
    }

    free(expanded);
    free(verts);
    free(glIndices);
}

void _glsDrawElements(unsigned int mode, int count, unsigned int type,
                      const void *indices)
{
    _glsDrawElementsBaseVertex(mode, count, type, indices, 0);
}
