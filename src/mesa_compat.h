/*********************************************************************************
*
*  ===============================================================================
*  |                  GLDirect: Direct3D Device Driver for Mesa.                 |
*  |                                                                             |
*  |                Copyright (C) 1997-2007 SciTech Software, Inc.               |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x/2000/XP (Win32)
*
* Description:  Mesa compatibility shim for the DX9 backend.
*
*               This header provides all the Mesa types, constants, macros,
*               and function declarations that the DX9 backend files still
*               reference after the Mesa headers were removed. It bridges
*               the gap until the DX9 backend is fully replaced by GL46 modules.
*
*               DO NOT add new code that depends on these definitions.
*               They exist solely to keep the legacy DX9 backend compiling.
*
*********************************************************************************/

#ifndef __MESA_COMPAT_H
#define __MESA_COMPAT_H

#include <windows.h>
#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

//---------------------------------------------------------------------------
// GLD_GET_CONTEXT — extract GLD_context* from a GLcontext*
//---------------------------------------------------------------------------

#ifndef GLD_GET_CONTEXT
#define GLD_GET_CONTEXT(ctx)  ((GLD_context*)(ctx)->DriverCtx)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------
// GLAPIENTRY / GLAPI — calling convention for GL entry points
//---------------------------------------------------------------------------

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#ifndef GLAPI
#define GLAPI extern
#endif

//---------------------------------------------------------------------------
// Basic type aliases
//---------------------------------------------------------------------------

typedef unsigned char  GLchan;

/* __GLcontext typedef used by gld5_driver.c _gld_mesa_warning/_gld_mesa_fatal */
typedef struct __GLcontextRec __GLcontext;

//---------------------------------------------------------------------------
// Legacy GL constants removed from core profile but needed by DX9 backend
// All guarded with #ifndef to avoid conflicts with any other headers.
//---------------------------------------------------------------------------

/* Primitive types removed from core */
#ifndef GL_QUADS
#define GL_QUADS                0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP           0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON              0x0009
#endif

/* Shade model */
#ifndef GL_FLAT
#define GL_FLAT                 0x1D00
#endif
#ifndef GL_SMOOTH
#define GL_SMOOTH               0x1D01
#endif

/* Texture wrap (legacy) */
#ifndef GL_CLAMP
#define GL_CLAMP                0x2900
#endif

/* Texture environment modes */
#ifndef GL_ADD
#define GL_ADD                  0x0104
#endif
#ifndef GL_MODULATE
#define GL_MODULATE             0x2100
#endif
#ifndef GL_DECAL
#define GL_DECAL                0x2101
#endif

/* Lighting constants */
#ifndef GL_LIGHT0
#define GL_LIGHT0               0x4000
#endif
#ifndef GL_LIGHT1
#define GL_LIGHT1               0x4001
#endif
#ifndef GL_LIGHT2
#define GL_LIGHT2               0x4002
#endif
#ifndef GL_LIGHT3
#define GL_LIGHT3               0x4003
#endif
#ifndef GL_LIGHT4
#define GL_LIGHT4               0x4004
#endif
#ifndef GL_LIGHT5
#define GL_LIGHT5               0x4005
#endif
#ifndef GL_LIGHT6
#define GL_LIGHT6               0x4006
#endif
#ifndef GL_LIGHT7
#define GL_LIGHT7               0x4007
#endif
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
#ifndef GL_EMISSION
#define GL_EMISSION             0x1600
#endif
#ifndef GL_SHININESS
#define GL_SHININESS            0x1601
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#define GL_AMBIENT_AND_DIFFUSE  0x1602
#endif
#ifndef GL_COLOR_INDEXES
#define GL_COLOR_INDEXES        0x1603
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING             0x0B50
#endif
#ifndef GL_LIGHT_MODEL_LOCAL_VIEWER
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#endif
#ifndef GL_LIGHT_MODEL_AMBIENT
#define GL_LIGHT_MODEL_AMBIENT  0x0B53
#endif
#ifndef GL_COLOR_MATERIAL
#define GL_COLOR_MATERIAL       0x0B57
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE            0x0BA1
#endif

/* Fog constants */
#ifndef GL_FOG
#define GL_FOG                  0x0B60
#endif
#ifndef GL_FOG_INDEX
#define GL_FOG_INDEX            0x0B61
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
#ifndef GL_FOG_MODE
#define GL_FOG_MODE             0x0B65
#endif
#ifndef GL_FOG_COLOR
#define GL_FOG_COLOR            0x0B66
#endif
#ifndef GL_EXP
#define GL_EXP                  0x0800
#endif
#ifndef GL_EXP2
#define GL_EXP2                 0x0801
#endif

/* Alpha test */
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST           0x0BC0
#endif

/* Clip planes */
#ifndef GL_CLIP_PLANE0
#define GL_CLIP_PLANE0          0x3000
#endif
#ifndef GL_CLIP_PLANE1
#define GL_CLIP_PLANE1          0x3001
#endif
#ifndef GL_CLIP_PLANE2
#define GL_CLIP_PLANE2          0x3002
#endif
#ifndef GL_CLIP_PLANE3
#define GL_CLIP_PLANE3          0x3003
#endif
#ifndef GL_CLIP_PLANE4
#define GL_CLIP_PLANE4          0x3004
#endif
#ifndef GL_CLIP_PLANE5
#define GL_CLIP_PLANE5          0x3005
#endif

/* Matrix modes */
#ifndef GL_MODELVIEW
#define GL_MODELVIEW            0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION           0x1701
#endif

/* Display list compile modes */
#ifndef GL_COMPILE
#define GL_COMPILE              0x1300
#endif
#ifndef GL_COMPILE_AND_EXECUTE
#define GL_COMPILE_AND_EXECUTE  0x1301
#endif

/* Legacy format constants */
#ifndef GL_LUMINANCE
#define GL_LUMINANCE            0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA      0x190A
#endif
#ifndef GL_INTENSITY
#define GL_INTENSITY            0x8049
#endif
#ifndef GL_COLOR_INDEX
#define GL_COLOR_INDEX          0x1900
#endif
#ifndef GL_BITMAP
#define GL_BITMAP               0x1A00
#endif
#ifndef GL_ALPHA
#define GL_ALPHA                0x1906
#endif

/* Sized internal formats (legacy) */
#ifndef GL_ALPHA4
#define GL_ALPHA4               0x803B
#endif
#ifndef GL_ALPHA8
#define GL_ALPHA8               0x803C
#endif
#ifndef GL_ALPHA12
#define GL_ALPHA12              0x803D
#endif
#ifndef GL_ALPHA16
#define GL_ALPHA16              0x803E
#endif
#ifndef GL_LUMINANCE4
#define GL_LUMINANCE4           0x803F
#endif
#ifndef GL_LUMINANCE8
#define GL_LUMINANCE8           0x8040
#endif
#ifndef GL_LUMINANCE12
#define GL_LUMINANCE12          0x8041
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16          0x8042
#endif
#ifndef GL_LUMINANCE4_ALPHA4
#define GL_LUMINANCE4_ALPHA4    0x8043
#endif
#ifndef GL_LUMINANCE6_ALPHA2
#define GL_LUMINANCE6_ALPHA2    0x8044
#endif
#ifndef GL_LUMINANCE8_ALPHA8
#define GL_LUMINANCE8_ALPHA8    0x8045
#endif
#ifndef GL_LUMINANCE12_ALPHA4
#define GL_LUMINANCE12_ALPHA4   0x8046
#endif
#ifndef GL_LUMINANCE12_ALPHA12
#define GL_LUMINANCE12_ALPHA12  0x8047
#endif
#ifndef GL_LUMINANCE16_ALPHA16
#define GL_LUMINANCE16_ALPHA16  0x8048
#endif
#ifndef GL_INTENSITY4
#define GL_INTENSITY4           0x804A
#endif
#ifndef GL_INTENSITY8
#define GL_INTENSITY8           0x804B
#endif
#ifndef GL_INTENSITY12
#define GL_INTENSITY12          0x804C
#endif
#ifndef GL_INTENSITY16
#define GL_INTENSITY16          0x804D
#endif

/* Color index EXT */
#ifndef GL_COLOR_INDEX1_EXT
#define GL_COLOR_INDEX1_EXT     0x80E2
#endif
#ifndef GL_COLOR_INDEX2_EXT
#define GL_COLOR_INDEX2_EXT     0x80E3
#endif
#ifndef GL_COLOR_INDEX4_EXT
#define GL_COLOR_INDEX4_EXT     0x80E4
#endif
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT     0x80E5
#endif
#ifndef GL_COLOR_INDEX12_EXT
#define GL_COLOR_INDEX12_EXT    0x80E6
#endif
#ifndef GL_COLOR_INDEX16_EXT
#define GL_COLOR_INDEX16_EXT    0x80E7
#endif

/* Stencil wrap EXT */
#ifndef GL_INCR_WRAP_EXT
#define GL_INCR_WRAP_EXT        0x8507
#endif
#ifndef GL_DECR_WRAP_EXT
#define GL_DECR_WRAP_EXT        0x8508
#endif

/* ARB multitexture */
#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB         0x84C0
#endif

/* Texture coordinate generation */
#ifndef GL_OBJECT_LINEAR
#define GL_OBJECT_LINEAR        0x2401
#endif
#ifndef GL_EYE_LINEAR
#define GL_EYE_LINEAR           0x2400
#endif
#ifndef GL_SPHERE_MAP
#define GL_SPHERE_MAP           0x2402
#endif

/* Generate mipmap (legacy) */
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP      0x8191
#endif

/* Render mode */
#ifndef GL_RENDER
#define GL_RENDER               0x1C00
#endif
#ifndef GL_FEEDBACK
#define GL_FEEDBACK             0x1C01
#endif
#ifndef GL_SELECT
#define GL_SELECT               0x1C02
#endif

/* Current state queries */
#ifndef GL_CURRENT_COLOR
#define GL_CURRENT_COLOR        0x0B00
#endif
#ifndef GL_CURRENT_NORMAL
#define GL_CURRENT_NORMAL       0x0B02
#endif
#ifndef GL_CURRENT_TEXTURE_COORDS
#define GL_CURRENT_TEXTURE_COORDS 0x0B03
#endif

//---------------------------------------------------------------------------
// Mesa config limits
//---------------------------------------------------------------------------

#ifndef MAX_WIDTH
#define MAX_WIDTH               4096
#endif
#ifndef MAX_HEIGHT
#define MAX_HEIGHT              4096
#endif

#define MAX_CLIP_PLANES         6

//---------------------------------------------------------------------------
// Vertex attribute indices (Mesa VERT_ATTRIB_*)
//---------------------------------------------------------------------------

#define VERT_ATTRIB_POS      0
#define VERT_ATTRIB_WEIGHT   1
#define VERT_ATTRIB_NORMAL   2
#define VERT_ATTRIB_COLOR0   3
#define VERT_ATTRIB_COLOR1   4
#define VERT_ATTRIB_FOG      5
#define VERT_ATTRIB_SIX      6
#define VERT_ATTRIB_SEVEN    7
#define VERT_ATTRIB_TEX0     8
#define VERT_ATTRIB_TEX1     9
#define VERT_ATTRIB_TEX2     10
#define VERT_ATTRIB_TEX3     11
#define VERT_ATTRIB_TEX4     12
#define VERT_ATTRIB_TEX5     13
#define VERT_ATTRIB_TEX6     14
#define VERT_ATTRIB_TEX7     15
#define VERT_ATTRIB_MAX      16

//---------------------------------------------------------------------------
// Material attribute indices (Mesa MAT_ATTRIB_*)
//---------------------------------------------------------------------------

#define MAT_ATTRIB_FRONT_AMBIENT   0
#define MAT_ATTRIB_FRONT_DIFFUSE   1
#define MAT_ATTRIB_FRONT_SPECULAR  2
#define MAT_ATTRIB_FRONT_EMISSION  3
#define MAT_ATTRIB_FRONT_SHININESS 4
#define MAT_ATTRIB_FRONT_INDEXES   5
#define MAT_ATTRIB_BACK_AMBIENT    6
#define MAT_ATTRIB_BACK_DIFFUSE    7
#define MAT_ATTRIB_BACK_SPECULAR   8
#define MAT_ATTRIB_BACK_EMISSION   9
#define MAT_ATTRIB_BACK_SHININESS  10
#define MAT_ATTRIB_BACK_INDEXES    11
#define MAT_ATTRIB_MAX             12

//---------------------------------------------------------------------------
// Primitive sentinel values
//---------------------------------------------------------------------------

#define PRIM_OUTSIDE_BEGIN_END  0x1F
#define PRIM_UNKNOWN            0x1E

//---------------------------------------------------------------------------
// State flags (_NEW_*)
//---------------------------------------------------------------------------

#define _NEW_MODELVIEW       (1 << 0)
#define _NEW_PROJECTION      (1 << 1)
#define _NEW_TEXTURE_MATRIX  (1 << 2)
#define _NEW_COLOR_MATRIX    (1 << 3)
#define _NEW_ACCUM           (1 << 4)
#define _NEW_COLOR           (1 << 5)
#define _NEW_DEPTH           (1 << 6)
#define _NEW_EVAL            (1 << 7)
#define _NEW_FOG             (1 << 8)
#define _NEW_HINT            (1 << 9)
#define _NEW_LIGHT           (1 << 10)
#define _NEW_LINE            (1 << 11)
#define _NEW_PIXEL           (1 << 12)
#define _NEW_POINT           (1 << 13)
#define _NEW_POLYGON         (1 << 14)
#define _NEW_POLYGONSTIPPLE  (1 << 15)
#define _NEW_SCISSOR         (1 << 16)
#define _NEW_STENCIL         (1 << 17)
#define _NEW_TEXTURE         (1 << 18)
#define _NEW_TRANSFORM       (1 << 19)
#define _NEW_VIEWPORT        (1 << 20)
#define _NEW_PACKUNPACK      (1 << 21)
#define _NEW_ARRAY           (1 << 22)
#define _NEW_RENDERMODE      (1 << 23)
#define _NEW_BUFFERS         (1 << 24)
#define _NEW_MULTISAMPLE     (1 << 25)
#define _NEW_TRACK_MATRIX    (1 << 26)
#define _NEW_PROGRAM         (1 << 27)
#define _NEW_ALL             0xFFFFFFFF

//---------------------------------------------------------------------------
// Flush flags
//---------------------------------------------------------------------------

#define FLUSH_STORED_VERTICES  0x1
#define FLUSH_UPDATE_CURRENT   0x2

//---------------------------------------------------------------------------
// Clear buffer bit flags (DD_* used by Mesa driver interface)
//---------------------------------------------------------------------------

#define DD_FRONT_LEFT_BIT   0x01
#define DD_FRONT_RIGHT_BIT  0x02
#define DD_BACK_LEFT_BIT    0x04
#define DD_BACK_RIGHT_BIT   0x08
#define DD_DEPTH_BIT        0x10
#define DD_STENCIL_BIT      0x20
#define DD_ACCUM_BIT        0x40

//---------------------------------------------------------------------------
// Colour channel indices
//---------------------------------------------------------------------------

#define RCOMP  0
#define GCOMP  1
#define BCOMP  2
#define ACOMP  3

#define CHAN_MAX  255

#define UBYTE_TO_CHAN(ub)  ((GLchan)(ub))

//---------------------------------------------------------------------------
// Matrix type enum
//---------------------------------------------------------------------------

#define MATRIX_GENERAL    0
#define MATRIX_IDENTITY   1
#define MATRIX_3D_NO_ROT  2
#define MATRIX_PERSPECTIVE 3
#define MATRIX_2D         4
#define MATRIX_2D_NO_ROT  5
#define MATRIX_3D         6

//---------------------------------------------------------------------------
// Mesa texture format enum values
//---------------------------------------------------------------------------

enum {
    MESA_FORMAT_RGBA8888 = 0,
    MESA_FORMAT_ARGB8888,
    MESA_FORMAT_RGB888,
    MESA_FORMAT_RGB565,
    MESA_FORMAT_ARGB4444,
    MESA_FORMAT_ARGB1555,
    MESA_FORMAT_AL88,
    MESA_FORMAT_A8,
    MESA_FORMAT_L8,
    MESA_FORMAT_I8,
    MESA_FORMAT_CI8,
    MESA_FORMAT_RGB332,
    MESA_FORMAT_COUNT
};

//---------------------------------------------------------------------------
// Texgen flags
//---------------------------------------------------------------------------

#define TEXGEN_SPHERE_MAP       0x01
#define TEXGEN_OBJ_LINEAR       0x02
#define TEXGEN_EYE_LINEAR       0x04
#define TEXGEN_NEED_NORMALS     (TEXGEN_SPHERE_MAP)
#define TEXGEN_NEED_EYE_COORD   (TEXGEN_SPHERE_MAP | TEXGEN_EYE_LINEAR)

#define S_BIT  0x01
#define T_BIT  0x02
#define R_BIT  0x04
#define Q_BIT  0x08

//---------------------------------------------------------------------------
// Light flags
//---------------------------------------------------------------------------

#define LIGHT_SPOT         0x01
#define LIGHT_POSITIONAL   0x02


//---------------------------------------------------------------------------
// Utility macros
//---------------------------------------------------------------------------

#define ASSIGN_4V(v, a, b, c, d)  \
    do { (v)[0]=(a); (v)[1]=(b); (v)[2]=(c); (v)[3]=(d); } while(0)

#define COPY_4V(dst, src)  \
    do { (dst)[0]=(src)[0]; (dst)[1]=(src)[1]; (dst)[2]=(src)[2]; (dst)[3]=(src)[3]; } while(0)

#define COPY_4FV(dst, src)  COPY_4V(dst, src)

#define CROSS3(n, u, v)  \
    do { (n)[0]=(u)[1]*(v)[2]-(u)[2]*(v)[1]; \
         (n)[1]=(u)[2]*(v)[0]-(u)[0]*(v)[2]; \
         (n)[2]=(u)[0]*(v)[1]-(u)[1]*(v)[0]; } while(0)

#define NORMALIZE_3FV(v)  \
    do { GLfloat _len = (GLfloat)sqrt((v)[0]*(v)[0]+(v)[1]*(v)[1]+(v)[2]*(v)[2]); \
         if (_len > 1e-6f) { _len = 1.0f/_len; (v)[0]*=_len; (v)[1]*=_len; (v)[2]*=_len; } \
    } while(0)

#define FLUSH_VERTICES(ctx, flags)  ((void)0)

#define MALLOC(sz)   malloc(sz)
#define CALLOC(sz)   calloc(1, (sz))
#define FREE(p)      free(p)

#define UNCLAMPED_FLOAT_TO_UBYTE(ub, f)  \
    do { GLfloat _tmp = (f) * 255.0f; \
         if (_tmp < 0.0f) _tmp = 0.0f; \
         if (_tmp > 255.0f) _tmp = 255.0f; \
         (ub) = (GLubyte)(int)_tmp; } while(0)

#ifndef CLAMP
#define CLAMP(x, mn, mx)  ( (x)<(mn) ? (mn) : ((x)>(mx) ? (mx) : (x)) )
#endif

#ifndef MAX2
#define MAX2(a, b)  ( (a)>(b) ? (a) : (b) )
#endif

#ifndef MIN2
#define MIN2(a, b)  ( (a)<(b) ? (a) : (b) )
#endif

#define ENUM_TO_FLOAT(e)   ((GLfloat)(GLint)(e))
#define FLOAT_TO_ENUM(f)   ((GLenum)(GLint)(f))

//---------------------------------------------------------------------------
// ASSERT macro
//---------------------------------------------------------------------------

#ifndef ASSERT
#ifdef _DEBUG
#include <assert.h>
#define ASSERT(x)  assert(x)
#else
#define ASSERT(x)  ((void)0)
#endif
#endif

//---------------------------------------------------------------------------
// GET_CURRENT_CONTEXT — use Mesa's real glapi context mechanism
//---------------------------------------------------------------------------

/* _glapi_Context is the global context pointer from Mesa's glapi.c */
extern void *_glapi_Context;
extern void *_glapi_get_context(void);

#define GET_CURRENT_CONTEXT(c)  \
    struct __GLcontextRec *c = (struct __GLcontextRec *) _glapi_Context

//---------------------------------------------------------------------------
// Forward declarations for sub-structs
//---------------------------------------------------------------------------

struct gl_1d_map {
    GLfloat  u1, u2;
    GLfloat  du;
    GLfloat *Points;
    GLint    Order;
};

struct gl_2d_map {
    GLfloat  u1, u2;
    GLfloat  v1, v2;
    GLfloat  du, dv;
    GLfloat *Points;
    GLint    Uorder, Vorder;
};

//---------------------------------------------------------------------------
// gl_texture_image
//---------------------------------------------------------------------------

struct gl_texture_image {
    GLenum   Format;
    GLenum   IntFormat;     /* internal format (e.g. GL_RGBA8) */
    GLint    Width;
    GLint    Height;
    GLint    Border;
    GLvoid  *Data;
    const struct gl_texture_format *TexFormat;
    GLboolean IsCompressed;
    GLuint    CompressedSize;
};

//---------------------------------------------------------------------------
// gl_texture_format
//---------------------------------------------------------------------------

typedef void (*FetchTexelFunc1D)(const struct gl_texture_image *, GLint, GLint, GLint, GLvoid *);
typedef void (*FetchTexelFunc2D)(const struct gl_texture_image *, GLint, GLint, GLint, GLvoid *);
typedef void (*FetchTexelFunc3D)(const struct gl_texture_image *, GLint, GLint, GLint, GLvoid *);
typedef void (*StoreTexImageFunc)(void);

struct gl_texture_format {
    GLint    MesaFormat;
    GLenum   BaseFormat;
    GLint    RedBits;
    GLint    GreenBits;
    GLint    BlueBits;
    GLint    AlphaBits;
    GLint    LuminanceBits;
    GLint    IntensityBits;
    GLint    IndexBits;
    GLint    DepthBits;
    GLint    TexelBytes;
    FetchTexelFunc1D FetchTexel1D;
    FetchTexelFunc2D FetchTexel2D;
    FetchTexelFunc3D FetchTexel3D;
};

//---------------------------------------------------------------------------
// gl_texture_object
//---------------------------------------------------------------------------

struct gl_texture_object {
    GLuint   Name;
    GLenum   Target;
    void    *DriverData;
    /* Sampler state cached on the texture object (Mesa 5.x style) */
    GLenum   MinFilter;
    GLenum   MagFilter;
    GLenum   WrapS;
    GLenum   WrapT;
};

//---------------------------------------------------------------------------
// gl_pixelstore_attrib
//---------------------------------------------------------------------------

struct gl_pixelstore_attrib {
    GLint    Alignment;
    GLint    RowLength;
    GLint    SkipPixels;
    GLint    SkipRows;
    GLboolean SwapBytes;
    GLboolean LsbFirst;
    GLint    ImageHeight;
    GLint    SkipImages;
};

//---------------------------------------------------------------------------
// gl_stencil_attrib
//---------------------------------------------------------------------------

struct gl_stencil_attrib {
    GLboolean Enabled;
    GLenum    Function[2];
    GLint     Ref[2];
    GLuint    ValueMask[2];
    GLuint    WriteMask[2];
    GLenum    FailFunc[2];
    GLenum    ZFailFunc[2];
    GLenum    ZPassFunc[2];
    GLint     Clear;
};

//---------------------------------------------------------------------------
// GLmatrix / GLmatrixStack
//---------------------------------------------------------------------------

typedef struct {
    GLfloat *m;
    GLfloat *inv;
    GLuint   flags;
    GLuint   type;
} GLmatrix;

typedef struct {
    GLmatrix *Top;
} GLmatrixStack;

//---------------------------------------------------------------------------
// GLvertexformat — function pointer table for Begin/End etc.
//---------------------------------------------------------------------------

typedef struct gl_vertexformat {
    void (GLAPIENTRY *Begin)(GLenum);
    void (GLAPIENTRY *End)(void);
    void (GLAPIENTRY *Vertex2f)(GLfloat, GLfloat);
    void (GLAPIENTRY *Vertex2fv)(const GLfloat *);
    void (GLAPIENTRY *Vertex3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Vertex3fv)(const GLfloat *);
    void (GLAPIENTRY *Vertex4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Vertex4fv)(const GLfloat *);
    void (GLAPIENTRY *Color3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Color3fv)(const GLfloat *);
    void (GLAPIENTRY *Color4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Color4fv)(const GLfloat *);
    void (GLAPIENTRY *Normal3f)(GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *Normal3fv)(const GLfloat *);
    void (GLAPIENTRY *TexCoord2f)(GLfloat, GLfloat);
    void (GLAPIENTRY *TexCoord2fv)(const GLfloat *);
    void (GLAPIENTRY *TexCoord4f)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *TexCoord4fv)(const GLfloat *);
    void (GLAPIENTRY *MultiTexCoord2fARB)(GLenum, GLfloat, GLfloat);
    void (GLAPIENTRY *EvalCoord1f)(GLfloat);
    void (GLAPIENTRY *EvalCoord1fv)(const GLfloat *);
    void (GLAPIENTRY *EvalCoord2f)(GLfloat, GLfloat);
    void (GLAPIENTRY *EvalCoord2fv)(const GLfloat *);
    void (GLAPIENTRY *EvalPoint1)(GLint);
    void (GLAPIENTRY *EvalPoint2)(GLint, GLint);
    void (GLAPIENTRY *EvalMesh1)(GLenum, GLint, GLint);
    void (GLAPIENTRY *EvalMesh2)(GLenum, GLint, GLint, GLint, GLint);
    void (GLAPIENTRY *Materialfv)(GLenum, GLenum, const GLfloat *);
    void (GLAPIENTRY *Rectf)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    void (GLAPIENTRY *DrawElements)(GLenum, GLsizei, GLenum, const GLvoid *);
    void (GLAPIENTRY *DrawRangeElements)(GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);
    /* VertexAttrib NV */
    void (GLAPIENTRY *VertexAttrib1fNV)(GLuint, GLfloat);
    void (GLAPIENTRY *VertexAttrib1fvNV)(GLuint, const GLfloat *);
    void (GLAPIENTRY *VertexAttrib2fNV)(GLuint, GLfloat, GLfloat);
    void (GLAPIENTRY *VertexAttrib2fvNV)(GLuint, const GLfloat *);
    void (GLAPIENTRY *VertexAttrib3fNV)(GLuint, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *VertexAttrib3fvNV)(GLuint, const GLfloat *);
    void (GLAPIENTRY *VertexAttrib4fNV)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (GLAPIENTRY *VertexAttrib4fvNV)(GLuint, const GLfloat *);
    void (GLAPIENTRY *ArrayElement)(GLint);
    /* Display list call functions */
    void (GLAPIENTRY *CallList)(GLuint);
    void (GLAPIENTRY *CallLists)(GLsizei, GLenum, const GLvoid *);
} GLvertexformat;

//---------------------------------------------------------------------------
// GLvisual
//---------------------------------------------------------------------------

typedef struct gl_config {
    GLint    depthBits;
    GLint    stencilBits;
    GLint    accumRedBits;
    GLint    accumGreenBits;
    GLint    accumBlueBits;
    GLint    accumAlphaBits;
    GLint    redBits;
    GLint    greenBits;
    GLint    blueBits;
    GLint    alphaBits;
    GLboolean doubleBufferMode;
    GLboolean stereoMode;
} GLvisual;

//---------------------------------------------------------------------------
// GLframebuffer
//---------------------------------------------------------------------------

typedef struct gl_framebuffer {
    GLuint   Width;
    GLuint   Height;
} GLframebuffer;

//---------------------------------------------------------------------------
// Pixel state
//---------------------------------------------------------------------------

struct gl_pixel_attrib {
    GLfloat  ZoomX;
    GLfloat  ZoomY;
};

//---------------------------------------------------------------------------
// Current vertex state
//---------------------------------------------------------------------------

struct gl_current_attrib {
    GLfloat  Attrib[VERT_ATTRIB_MAX][4];
    GLfloat  RasterPos[4];
    GLfloat  RasterColor[4];
};

//---------------------------------------------------------------------------
// Evaluator state
//---------------------------------------------------------------------------

struct gl_eval_attrib {
    GLboolean Map1Vertex4;
    GLboolean Map1Vertex3;
    GLboolean Map1Color4;
    GLboolean Map1Normal;
    GLboolean Map1TextureCoord1;
    GLboolean Map1TextureCoord2;
    GLboolean Map1TextureCoord3;
    GLboolean Map1TextureCoord4;
    GLboolean Map2Vertex4;
    GLboolean Map2Vertex3;
    GLboolean Map2Color4;
    GLboolean Map2Normal;
    GLboolean Map2TextureCoord1;
    GLboolean Map2TextureCoord2;
    GLboolean Map2TextureCoord3;
    GLboolean Map2TextureCoord4;
    GLboolean AutoNormal;
    GLfloat  MapGrid1u1, MapGrid1u2;
    GLint    MapGrid1un;
    GLfloat  MapGrid2u1, MapGrid2u2;
    GLfloat  MapGrid2v1, MapGrid2v2;
    GLint    MapGrid2un, MapGrid2vn;
};

struct gl_evaluators {
    struct gl_1d_map Map1Vertex4;
    struct gl_1d_map Map1Vertex3;
    struct gl_1d_map Map1Color4;
    struct gl_1d_map Map1Normal;
    struct gl_1d_map Map1Texture1;
    struct gl_1d_map Map1Texture2;
    struct gl_1d_map Map1Texture3;
    struct gl_1d_map Map1Texture4;
    struct gl_2d_map Map2Vertex4;
    struct gl_2d_map Map2Vertex3;
    struct gl_2d_map Map2Color4;
    struct gl_2d_map Map2Normal;
    struct gl_2d_map Map2Texture1;
    struct gl_2d_map Map2Texture2;
    struct gl_2d_map Map2Texture3;
    struct gl_2d_map Map2Texture4;
};

//---------------------------------------------------------------------------
// Color state
//---------------------------------------------------------------------------

struct gl_colorbuffer_attrib {
    GLfloat  ClearColor[4];
    GLboolean AlphaEnabled;
    GLenum   AlphaFunc;
    GLfloat  AlphaRef;
    GLboolean BlendEnabled;
    GLenum   BlendSrcRGB;
    GLenum   BlendDstRGB;
    GLenum   BlendSrcA;
    GLenum   BlendDstA;
    GLubyte  ColorMask[4];
};

//---------------------------------------------------------------------------
// Depth state
//---------------------------------------------------------------------------

struct gl_depthbuffer_attrib {
    GLboolean Test;
    GLboolean Mask;
    GLenum   Func;
    GLfloat  Clear;
};

//---------------------------------------------------------------------------
// Polygon state
//---------------------------------------------------------------------------

struct gl_polygon_attrib {
    GLenum   FrontMode;
    GLenum   BackMode;
    GLboolean CullFlag;
    GLenum   CullFaceMode;
    GLenum   FrontFace;
    GLboolean OffsetFill;
    GLfloat  OffsetFactor;
    GLfloat  OffsetUnits;
};

//---------------------------------------------------------------------------
// Fog state
//---------------------------------------------------------------------------

struct gl_fog_attrib {
    GLboolean Enabled;
    GLenum   Mode;
    GLfloat  Color[4];
    GLfloat  Start;
    GLfloat  End;
    GLfloat  Density;
};

//---------------------------------------------------------------------------
// Point state
//---------------------------------------------------------------------------

struct gl_point_attrib {
    GLfloat  _Size;     /* current point size (may be clamped) */
};

//---------------------------------------------------------------------------
// Material state
//---------------------------------------------------------------------------

struct gl_material {
    GLfloat  Attrib[MAT_ATTRIB_MAX][4];
};

//---------------------------------------------------------------------------
// Light source (individual light)
//---------------------------------------------------------------------------

struct gl_light {
    GLboolean Enabled;
    GLfloat  Ambient[4];
    GLfloat  Diffuse[4];
    GLfloat  Specular[4];
    GLfloat  Position[4];
    GLfloat  Direction[3];
    GLfloat  SpotExponent;
    GLfloat  SpotCutoff;
    GLfloat  ConstantAttenuation;
    GLfloat  LinearAttenuation;
    GLfloat  QuadraticAttenuation;
    GLuint   _Flags;
    /* Derived values (precomputed by Mesa) */
    GLfloat  _Position[4];       /* eye-space position */
    GLfloat  EyeDirection[4];    /* eye-space spot direction */
    GLfloat  _CosCutoff;         /* cos(SpotCutoff) */
    /* Derived material*light products (front) */
    GLfloat  _MatAmbient[2][4];  /* [0]=front, [1]=back */
    GLfloat  _MatDiffuse[2][4];
    GLfloat  _MatSpecular[2][4];
};

//---------------------------------------------------------------------------
// Light model state
//---------------------------------------------------------------------------

struct gl_lightmodel {
    GLfloat  Ambient[4];
    GLboolean LocalViewer;
    GLboolean TwoSide;
};

//---------------------------------------------------------------------------
// Light state (expanded for DX9 shader backend)
//---------------------------------------------------------------------------

struct gl_light_attrib {
    GLenum   ShadeModel;
    GLboolean Enabled;
    struct gl_lightmodel Model;
    struct gl_material   Material[2]; /* [0]=front, [1]=back */
    struct gl_light      Light[8];    /* MAX_LIGHTS */
    GLboolean ColorMaterialEnabled;
    GLenum   ColorMaterialFace;
    GLenum   ColorMaterialMode;
};

//---------------------------------------------------------------------------
// Texture unit state
//---------------------------------------------------------------------------

struct gl_texture_unit {
    GLenum   EnvMode;
    GLfloat  EnvColor[4];
    GLuint   TexGenEnabled;
    GLuint   _GenBitS;
    GLuint   _GenBitT;
    GLuint   _GenBitR;
    GLuint   _GenBitQ;
    GLuint   _GenFlags;
    struct gl_texture_object *_Current;
    GLenum   MinFilter;
    GLenum   MagFilter;
    GLenum   WrapS;
    GLenum   WrapT;
    /* Texgen mode per coordinate */
    GLenum   GenModeS;
    GLenum   GenModeT;
    GLenum   GenModeR;
    GLenum   GenModeQ;
    /* Texgen planes */
    GLfloat  ObjectPlaneS[4];
    GLfloat  ObjectPlaneT[4];
    GLfloat  ObjectPlaneR[4];
    GLfloat  ObjectPlaneQ[4];
    GLfloat  EyePlaneS[4];
    GLfloat  EyePlaneT[4];
    GLfloat  EyePlaneR[4];
    GLfloat  EyePlaneQ[4];
};

//---------------------------------------------------------------------------
// Texture state
//---------------------------------------------------------------------------

struct gl_texture_attrib {
    GLuint   _EnabledUnits;
    GLuint   _GenFlags;
    GLuint   _TexGenEnabled;
    GLuint   _TexMatEnabled;
    struct gl_texture_unit Unit[8]; /* MAX_TEXTURE_UNITS */
};

//---------------------------------------------------------------------------
// Scissor state
//---------------------------------------------------------------------------

struct gl_scissor_attrib {
    GLboolean Enabled;
    GLint    X, Y;
    GLsizei  Width, Height;
};

//---------------------------------------------------------------------------
// Transform state
//---------------------------------------------------------------------------

struct gl_transform_attrib {
    GLuint   ClipPlanesEnabled;
    GLdouble _ClipUserPlane[MAX_CLIP_PLANES][4];
    GLfloat  _ClipUserPlaneF[MAX_CLIP_PLANES][4]; /* float alias for D3D SetClipPlane */
};

//---------------------------------------------------------------------------
// Viewport state
//---------------------------------------------------------------------------

struct gl_viewport_attrib {
    GLint    X, Y;
    GLsizei  Width, Height;
    GLfloat  Near, Far;
};

//---------------------------------------------------------------------------
// Display list state
//---------------------------------------------------------------------------

struct gl_list_state {
    GLuint   CurrentListNum;
};


//---------------------------------------------------------------------------
// Driver function table
//---------------------------------------------------------------------------

struct gl_driver_functions {
    GLenum   CurrentExecPrimitive;
    GLenum   CurrentSavePrimitive;
    GLuint   NeedFlush;
    GLuint   SaveNeedFlush;

    /* Core driver callbacks */
    void   (*UpdateState)(struct __GLcontextRec *ctx, GLuint new_state);
    void   (*ResizeBuffers)(GLframebuffer *fb);
    void   (*SaveFlushVertices)(struct __GLcontextRec *ctx);
    void   (*FlushVertices)(struct __GLcontextRec *ctx, GLuint flags);
    void   (*MakeCurrent)(struct __GLcontextRec *ctx, GLframebuffer *drawBuffer, GLframebuffer *readBuffer);
    void   (*Clear)(struct __GLcontextRec *ctx, GLbitfield mask, GLboolean all, GLint x, GLint y, GLint width, GLint height);

    /* Display list callbacks */
    void   (*NewList)(struct __GLcontextRec *ctx, GLuint list, GLenum mode);
    void   (*EndList)(struct __GLcontextRec *ctx);
    void   (*BeginCallList)(struct __GLcontextRec *ctx, GLuint list);
    void   (*EndCallList)(struct __GLcontextRec *ctx);
    GLboolean (*NotifySaveBegin)(struct __GLcontextRec *ctx, GLenum mode);

    /* String query */
    const GLubyte* (*GetString)(struct __GLcontextRec *ctx, GLenum name);

    /* Buffer operations */
    void   (*DrawBuffer)(struct __GLcontextRec *ctx, GLenum mode);
    void   (*GetBufferSize)(GLframebuffer *fb, GLuint *width, GLuint *height);
    void   (*Finish)(struct __GLcontextRec *ctx);
    void   (*Flush)(struct __GLcontextRec *ctx);
    void   (*Error)(struct __GLcontextRec *ctx);
    void   (*Accum)(struct __GLcontextRec *ctx, GLenum op, GLfloat value);

    /* Pixel operations */
    void   (*CopyPixels)(struct __GLcontextRec *ctx, GLint srcx, GLint srcy, GLsizei w, GLsizei h, GLint dstx, GLint dsty, GLenum type);
    void   (*DrawPixels)(struct __GLcontextRec *ctx, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, const GLvoid *pixels);
    void   (*ReadPixels)(struct __GLcontextRec *ctx, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const struct gl_pixelstore_attrib *pack, GLvoid *dest);
    void   (*Bitmap)(struct __GLcontextRec *ctx, GLint x, GLint y, GLsizei w, GLsizei h, const struct gl_pixelstore_attrib *unpack, const GLubyte *bitmap);

    /* Texture image functions */
    const struct gl_texture_format* (*ChooseTextureFormat)(struct __GLcontextRec *ctx, GLint internalFormat, GLenum srcFormat, GLenum srcType);
    void   (*TexImage1D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*TexImage2D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint height, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*TexImage3D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint height, GLint depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*TexSubImage1D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*TexSubImage2D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*TexSubImage3D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
    void   (*CopyTexImage1D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
    void   (*CopyTexImage2D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void   (*CopyTexSubImage1D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
    void   (*CopyTexSubImage2D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void   (*CopyTexSubImage3D)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    GLboolean (*TestProxyTexImage)(struct __GLcontextRec *ctx, GLenum target, GLint level, GLint internalFormat, GLenum format, GLenum type, GLint width, GLint height, GLint depth, GLint border);

    /* Texture object functions */
    void   (*BindTexture)(struct __GLcontextRec *ctx, GLenum target, struct gl_texture_object *tObj);
    void   (*CreateTexture)(struct __GLcontextRec *ctx, struct gl_texture_object *tObj);
    void   (*DeleteTexture)(struct __GLcontextRec *ctx, struct gl_texture_object *tObj);
    void   (*PrioritizeTexture)(struct __GLcontextRec *ctx, struct gl_texture_object *tObj, GLclampf priority);

    /* Imaging */
    void   (*CopyColorTable)(void);
    void   (*CopyColorSubTable)(void);
    void   (*CopyConvolutionFilter1D)(void);
    void   (*CopyConvolutionFilter2D)(void);

    /* State changing functions */
    void   (*AlphaFunc)(struct __GLcontextRec *ctx, GLenum func, GLfloat ref);
    void   (*BlendFunc)(struct __GLcontextRec *ctx, GLenum sfactor, GLenum dfactor);
    void   (*ClearColor)(struct __GLcontextRec *ctx, const GLfloat color[4]);
    void   (*ClearDepth)(struct __GLcontextRec *ctx, GLclampd d);
    void   (*ClearStencil)(struct __GLcontextRec *ctx, GLint s);
    void   (*ColorMask)(struct __GLcontextRec *ctx, GLboolean rmask, GLboolean gmask, GLboolean bmask, GLboolean amask);
    void   (*CullFace)(struct __GLcontextRec *ctx, GLenum mode);
    void   (*ClipPlane)(struct __GLcontextRec *ctx, GLenum plane, const GLfloat *eq);
    void   (*FrontFace)(struct __GLcontextRec *ctx, GLenum mode);
    void   (*DepthFunc)(struct __GLcontextRec *ctx, GLenum func);
    void   (*DepthMask)(struct __GLcontextRec *ctx, GLboolean flag);
    void   (*DepthRange)(struct __GLcontextRec *ctx, GLclampd nearVal, GLclampd farVal);
    void   (*Enable)(struct __GLcontextRec *ctx, GLenum cap, GLboolean state);
    void   (*Fogfv)(struct __GLcontextRec *ctx, GLenum pname, const GLfloat *params);
    void   (*Hint)(struct __GLcontextRec *ctx, GLenum target, GLenum mode);
    void   (*Lightfv)(struct __GLcontextRec *ctx, GLenum light, GLenum pname, const GLfloat *params);
    void   (*LightModelfv)(struct __GLcontextRec *ctx, GLenum pname, const GLfloat *params);
    void   (*LineStipple)(struct __GLcontextRec *ctx, GLint factor, GLushort pattern);
    void   (*LineWidth)(struct __GLcontextRec *ctx, GLfloat width);
    void   (*LogicOpcode)(struct __GLcontextRec *ctx, GLenum opcode);
    void   (*PointParameterfv)(struct __GLcontextRec *ctx, GLenum pname, const GLfloat *params);
    void   (*PointSize)(struct __GLcontextRec *ctx, GLfloat size);
    void   (*PolygonMode)(struct __GLcontextRec *ctx, GLenum face, GLenum mode);
    void   (*PolygonOffset)(struct __GLcontextRec *ctx, GLfloat factor, GLfloat units);
    void   (*PolygonStipple)(struct __GLcontextRec *ctx, const GLubyte *mask);
    void   (*RenderMode)(struct __GLcontextRec *ctx, GLenum mode);
    void   (*Scissor)(struct __GLcontextRec *ctx, GLint x, GLint y, GLsizei w, GLsizei h);
    void   (*ShadeModel)(struct __GLcontextRec *ctx, GLenum mode);
    void   (*StencilFunc)(struct __GLcontextRec *ctx, GLenum func, GLint ref, GLuint mask);
    void   (*StencilMask)(struct __GLcontextRec *ctx, GLuint mask);
    void   (*StencilOp)(struct __GLcontextRec *ctx, GLenum fail, GLenum zfail, GLenum zpass);
    void   (*TexGen)(struct __GLcontextRec *ctx, GLenum coord, GLenum pname, const GLfloat *params);
    void   (*TexEnv)(struct __GLcontextRec *ctx, GLenum target, GLenum pname, const GLfloat *param);
    void   (*TexParameter)(struct __GLcontextRec *ctx, GLenum target, struct gl_texture_object *texObj, GLenum pname, const GLfloat *params);
    void   (*TextureMatrix)(struct __GLcontextRec *ctx, GLuint unit);
    void   (*Viewport)(struct __GLcontextRec *ctx, GLint x, GLint y, GLsizei w, GLsizei h);

    /* Line stipple reset */
    void   (*ResetLineStipple)(struct __GLcontextRec *ctx);
};

//---------------------------------------------------------------------------
// GLcontext — the big Mesa context struct
//---------------------------------------------------------------------------

struct __GLcontextRec {
    struct gl_current_attrib     Current;
    struct gl_eval_attrib        Eval;
    struct gl_evaluators         EvalMap;
    struct gl_colorbuffer_attrib Color;
    struct gl_depthbuffer_attrib Depth;
    struct gl_polygon_attrib     Polygon;
    struct gl_fog_attrib         Fog;
    struct gl_light_attrib       Light;
    struct gl_scissor_attrib     Scissor;
    struct gl_transform_attrib   Transform;
    struct gl_viewport_attrib    Viewport;
    struct gl_stencil_attrib     Stencil;
    struct gl_pixel_attrib       Pixel;
    struct gl_list_state         ListState;
    struct gl_point_attrib       Point;
    struct gl_texture_attrib     Texture;

    GLvisual                     Visual;

    struct {
        GLuint  MaxTextureUnits;
        GLuint  MaxTextureCoordUnits;
        GLuint  MaxTextureLevels;
    } Const;

    GLmatrixStack                ModelviewMatrixStack;
    GLmatrixStack                ProjectionMatrixStack;
    GLmatrixStack                TextureMatrixStack[8];

    struct gl_driver_functions   Driver;

    GLvertexformat              *Exec;
    GLvertexformat              *Save;

    GLuint                       NewState;

    void                        *DriverCtx;
};

typedef struct __GLcontextRec GLcontext;

//---------------------------------------------------------------------------
// Mesa texture format globals (extern declarations)
//---------------------------------------------------------------------------

extern const struct gl_texture_format _mesa_texformat_argb8888;
extern const struct gl_texture_format _mesa_texformat_rgb888;
extern const struct gl_texture_format _mesa_texformat_rgb565;
extern const struct gl_texture_format _mesa_texformat_argb4444;
extern const struct gl_texture_format _mesa_texformat_argb1555;
extern const struct gl_texture_format _mesa_texformat_al88;
extern const struct gl_texture_format _mesa_texformat_a8;
extern const struct gl_texture_format _mesa_texformat_l8;
extern const struct gl_texture_format _mesa_texformat_i8;
extern const struct gl_texture_format _mesa_texformat_ci8;
extern const struct gl_texture_format _mesa_texformat_rgb332;

//---------------------------------------------------------------------------
// Mesa function stubs (declarations)
//---------------------------------------------------------------------------

void _mesa_error(GLcontext *ctx, GLenum error, const char *msg, ...);
void _mesa_printf(const char *msg, ...);
const char* _mesa_lookup_enum_by_nr(int nr);
void _mesa_enable_extension(GLcontext *ctx, const char *name);
void _mesa_disable_extension(GLcontext *ctx, const char *name);
void _mesa_enable_1_3_extensions(GLcontext *ctx);
void _mesa_enable_imaging_extensions(GLcontext *ctx);
void _mesa_update_state(GLcontext *ctx);
void _mesa_noop_vtxfmt_init(GLvertexformat *vfmt);
void* _mesa_alloc_instruction(GLcontext *ctx, int opcode, int size);
void mesa_print_display_list(GLuint list);
GLboolean _mesa_validate_DrawElements(GLcontext *ctx, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void _mesa_make_current(GLcontext *ctx, GLframebuffer *drawBuffer, GLframebuffer *readBuffer);
void _mesa_transfer_teximage(GLcontext *ctx, ...);

const struct gl_texture_format* _mesa_choose_tex_format(GLcontext *ctx, GLenum format, GLenum srcFormat, GLenum srcType);
GLint _mesa_image_row_stride(const struct gl_pixelstore_attrib *packing, GLsizei width, GLenum format, GLenum type);
GLvoid* _mesa_image_address(const struct gl_pixelstore_attrib *packing, const GLvoid *image, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint img, GLint row, GLint column);
GLubyte* _mesa_unpack_bitmap(GLsizei width, GLsizei height, const GLubyte *bitmap, const struct gl_pixelstore_attrib *packing);
void _mesa_problem(GLcontext *ctx, const char *msg, ...);

void _mesa_store_teximage3d(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint height, GLint depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
void _mesa_store_texsubimage3d(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
GLboolean _mesa_test_proxy_teximage(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLenum format, GLenum type, GLint width, GLint height, GLint depth, GLint border);

extern const struct gl_pixelstore_attrib _mesa_native_packing;

/* Math evaluator functions */
void _math_horner_bezier_curve(const GLfloat *cp, GLfloat *out, GLfloat t, GLuint dim, GLint order);
void _math_horner_bezier_surf(const GLfloat *cp, GLfloat *out, GLfloat u, GLfloat v, GLuint dim, GLint uorder, GLint vorder);
void _math_de_casteljau_surf(const GLfloat *cp, GLfloat *out, GLfloat *du, GLfloat *dv, GLfloat u, GLfloat v, GLuint dim, GLint uorder, GLint vorder);

/* Array element helper */
void _ae_invalidate_state(GLcontext *ctx, GLuint new_state);
GLboolean _ae_create_context(GLcontext *ctx);
void _ae_destroy_context(GLcontext *ctx);

/* Display list save flush */
void gldSaveFlushVertices(GLcontext *ctx);

/* Display list opcode allocation */
typedef void (*mesa_opcode_execute)(GLcontext *ctx, void *data);
typedef void (*mesa_opcode_destroy)(GLcontext *ctx, void *data);
typedef void (*mesa_opcode_print)(GLcontext *ctx, void *data);
int _mesa_alloc_opcode(GLcontext *ctx, int size, mesa_opcode_execute exec, mesa_opcode_destroy destroy, mesa_opcode_print print);

/* Vertex format install functions */
void _mesa_install_exec_vtxfmt(GLcontext *ctx, GLvertexformat *vfmt);
void _mesa_install_save_vtxfmt(GLcontext *ctx, GLvertexformat *vfmt);
void _mesa_save_vtxfmt_init(GLvertexformat *vfmt);

/* Display list call stubs */
void GLAPIENTRY _mesa_CallList(GLuint list);
void GLAPIENTRY _mesa_CallLists(GLsizei n, GLenum type, const GLvoid *lists);
void GLAPIENTRY _mesa_save_CallList(GLuint list);
void GLAPIENTRY _mesa_save_CallLists(GLsizei n, GLenum type, const GLvoid *lists);
void GLAPIENTRY _mesa_save_EvalMesh1(GLenum mode, GLint i1, GLint i2);
void GLAPIENTRY _mesa_save_EvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);

/* Context current setter — use Mesa's real glapi */
extern void _glapi_set_context(void *context);
#define _mesa_compat_set_current_context(ctx) _glapi_set_context((void*)(ctx))

#ifdef __cplusplus
}
#endif

#endif /* __MESA_COMPAT_H */
