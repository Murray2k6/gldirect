/*********************************************************************************
*
*  gl_legacy_impl.c - OpenGL 1.x fixed-function paths that D3D9 can express
*
*  These are the parts of GL 1.1-1.5 that gl_impl.c did not cover: the texture
*  environment (fixed-function combiners), 1D textures, raster position and
*  bitmaps, evaluators, selection and feedback mode, the accumulation buffer,
*  pixel transfer, stipple and colour-index state.
*
*  Everything here is implemented to the GL specification for any application.
*  Where D3D9 genuinely cannot express something the call is logged once and
*  ignored - it never silently does nothing, and never reports success for
*  work it did not perform.
*
*********************************************************************************/

#include "gl_impl.h"
#include "gl_state.h"
#include "context_manager.h"
#include "display_list_emulator.h"
#include "gld_diag.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef struct { float v[4]; } GLL_DLFloat4;
typedef struct {
    int width, height;
    float xorig, yorig, xmove, ymove;
    int bitmapBytes;
} GLL_DLBitmap;

static BOOL _gllDLRecord(GLD_dlCommandFunc func, const void *data, int size)
{
    if (!gldDLIsRecording()) return FALSE;
    if (!gldDLRecordCommand(func, data, size)) {
        glsGetState()->lastError = 0x0505; /* GL_OUT_OF_MEMORY */
        return TRUE;
    }
    return gldDLGetRecordingMode() == GL_COMPILE;
}

static void _gllDLRasterPos(const void *p)
{
    const float *v = ((const GLL_DLFloat4 *)p)->v;
    _glsRasterPos4f(v[0], v[1], v[2], v[3]);
}

static void _gllDLWindowPos(const void *p)
{
    const float *v = ((const GLL_DLFloat4 *)p)->v;
    _glsWindowPos3f(v[0], v[1], v[2]);
}

static void _gllDLBitmap(const void *p)
{
    const GLL_DLBitmap *a = (const GLL_DLBitmap *)p;
    const unsigned char *bits = a->bitmapBytes > 0
        ? (const unsigned char *)(a + 1) : NULL;
    _glsBitmap(a->width, a->height, a->xorig, a->yorig,
               a->xmove, a->ymove, bits);
}

/* ---- GL enums used here, kept local so this file does not depend on which
 *      GL header happens to be reachable. ---- */
#define GLL_TEXTURE_ENV             0x2300
#define GLL_TEXTURE_ENV_MODE        0x2200
#define GLL_TEXTURE_ENV_COLOR       0x2201
#define GLL_TEXTURE_FILTER_CONTROL  0x8500
#define GLL_TEXTURE_LOD_BIAS        0x8501

#define GLL_MODULATE                0x2100
#define GLL_DECAL                   0x2101
#define GLL_BLEND                   0x0BE2
#define GLL_REPLACE                 0x1E01
#define GLL_ADD                     0x0104
#define GLL_COMBINE                 0x8570

#define GLL_COMBINE_RGB             0x8571
#define GLL_COMBINE_ALPHA           0x8572
#define GLL_SOURCE0_RGB             0x8580
#define GLL_SOURCE1_RGB             0x8581
#define GLL_SOURCE2_RGB             0x8582
#define GLL_SOURCE0_ALPHA           0x8588
#define GLL_SOURCE1_ALPHA           0x8589
#define GLL_SOURCE2_ALPHA           0x858A
#define GLL_OPERAND0_RGB            0x8590
#define GLL_OPERAND1_RGB            0x8591
#define GLL_OPERAND2_RGB            0x8592
#define GLL_OPERAND0_ALPHA          0x8598
#define GLL_OPERAND1_ALPHA          0x8599
#define GLL_OPERAND2_ALPHA          0x859A
#define GLL_RGB_SCALE               0x8573
#define GLL_ALPHA_SCALE             0x0D1C

#define GLL_ADD_SIGNED              0x8574
#define GLL_INTERPOLATE             0x8575
#define GLL_SUBTRACT                0x84E7
#define GLL_DOT3_RGB                0x86AE
#define GLL_DOT3_RGBA               0x86AF

#define GLL_TEXTURE                 0x1702
#define GLL_CONSTANT                0x8576
#define GLL_PRIMARY_COLOR           0x8577
#define GLL_PREVIOUS                0x8578
#define GLL_TEXTURE0                0x84C0

#define GLL_SRC_COLOR               0x0300
#define GLL_ONE_MINUS_SRC_COLOR     0x0301
#define GLL_SRC_ALPHA               0x0302
#define GLL_ONE_MINUS_SRC_ALPHA     0x0303

#define GLL_FALSE                   0
#define GLL_TRUE                    1

#define GLL_RENDER                  0x1C00
#define GLL_FEEDBACK                0x1C01
#define GLL_SELECT                  0x1C02

#define GLL_ACCUM                   0x0100
#define GLL_LOAD                    0x0101
#define GLL_RETURN                  0x0102
#define GLL_MULT                    0x0103

#define GLL_INVALID_ENUM            0x0500
#define GLL_INVALID_VALUE           0x0501
#define GLL_INVALID_OPERATION       0x0502

#define GLL_MAP1_COLOR_4            0x0A10
#define GLL_MAP1_INDEX              0x0A11
#define GLL_MAP1_NORMAL             0x0A12
#define GLL_MAP1_TEXTURE_COORD_1    0x0A13
#define GLL_MAP1_TEXTURE_COORD_2    0x0A14
#define GLL_MAP1_TEXTURE_COORD_3    0x0A15
#define GLL_MAP1_TEXTURE_COORD_4    0x0A16
#define GLL_MAP1_VERTEX_3           0x0A17
#define GLL_MAP1_VERTEX_4           0x0A18
#define GLL_MAP2_COLOR_4            0x0A20
#define GLL_MAP2_INDEX              0x0A21
#define GLL_MAP2_NORMAL             0x0A22
#define GLL_MAP2_TEXTURE_COORD_1    0x0A23
#define GLL_MAP2_TEXTURE_COORD_2    0x0A24
#define GLL_MAP2_TEXTURE_COORD_3    0x0A25
#define GLL_MAP2_TEXTURE_COORD_4    0x0A26
#define GLL_MAP2_VERTEX_3           0x0A27
#define GLL_MAP2_VERTEX_4           0x0A28

#define GLL_POINT                   0x1B00
#define GLL_LINE                    0x1B01
#define GLL_FILL                    0x1B02

#define GLL_LINE_LOOP               0x0002
#define GLL_LINE_STRIP              0x0003
#define GLL_TRIANGLE_STRIP          0x0005
#define GLL_POINTS                  0x0000

/* ===================================================================
 *  Texture environment  ->  D3D9 texture stage states
 * =================================================================== */

static int _glLegacyActiveUnit(GLS_State *s)
{
    int unit = (s->activeTexUnit >= GLL_TEXTURE0)
             ? (int)(s->activeTexUnit - GLL_TEXTURE0) : 0;
    if (unit < 0) unit = 0;
    if (unit >= GLS_MAX_TEX_UNITS) unit = GLS_MAX_TEX_UNITS - 1;
    return unit;
}

/* GL combine function -> D3D texture operation.
 * scale folds GL's RGB_SCALE/ALPHA_SCALE into D3D's MODULATE2X/4X, which is
 * the only place D3D9 exposes a post-combine multiply. */
static D3DTEXTUREOP _glLegacyCombineToOp(GLenum_t fn, float scale)
{
    switch (fn) {
    case GLL_REPLACE:     return D3DTOP_SELECTARG1;
    case GLL_MODULATE:    return (scale >= 4.0f) ? D3DTOP_MODULATE4X
                               : (scale >= 2.0f) ? D3DTOP_MODULATE2X
                                                 : D3DTOP_MODULATE;
    case GLL_ADD:         return D3DTOP_ADD;
    case GLL_ADD_SIGNED:  return D3DTOP_ADDSIGNED;
    case GLL_INTERPOLATE: return D3DTOP_LERP;
    case GLL_SUBTRACT:    return D3DTOP_SUBTRACT;
    case GLL_DOT3_RGB:
    case GLL_DOT3_RGBA:   return D3DTOP_DOTPRODUCT3;
    default:              return D3DTOP_MODULATE;
    }
}

static DWORD _glLegacySourceToArg(GLenum_t src)
{
    switch (src) {
    case GLL_TEXTURE:       return D3DTA_TEXTURE;
    case GLL_CONSTANT:      return D3DTA_TFACTOR;
    case GLL_PRIMARY_COLOR: return D3DTA_DIFFUSE;
    case GLL_PREVIOUS:      return D3DTA_CURRENT;
    default:
        /* GL_TEXTUREn cross-unit references: D3D9 can only read the stage's
         * own texture, so this collapses to that. */
        if (src >= GLL_TEXTURE0 && src < GLL_TEXTURE0 + GLS_MAX_TEX_UNITS)
            return D3DTA_TEXTURE;
        return D3DTA_CURRENT;
    }
}

static DWORD _glLegacyOperandModifier(GLenum_t op)
{
    switch (op) {
    case GLL_SRC_COLOR:           return 0;
    case GLL_ONE_MINUS_SRC_COLOR: return D3DTA_COMPLEMENT;
    case GLL_SRC_ALPHA:           return D3DTA_ALPHAREPLICATE;
    case GLL_ONE_MINUS_SRC_ALPHA: return D3DTA_COMPLEMENT | D3DTA_ALPHAREPLICATE;
    default:                      return 0;
    }
}

static DWORD _glLegacyPackColor(const float c[4])
{
    int r = (int)(c[0] * 255.0f + 0.5f);
    int g = (int)(c[1] * 255.0f + 0.5f);
    int b = (int)(c[2] * 255.0f + 0.5f);
    int a = (int)(c[3] * 255.0f + 0.5f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    if (a < 0) a = 0; if (a > 255) a = 255;
    return ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
}

/*
 * Push one unit's texture environment into the matching D3D9 stage.
 *
 * Exposed (not static) so the draw path can re-apply the environment when the
 * bound texture or active unit changes.
 */
void _glsApplyTexEnv(int unit)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    GLS_State *s = glsGetState();
    GLS_TexEnv *e;
    D3DTEXTUREOP colorOp, alphaOp;
    DWORD a1, a2, a0 = D3DTA_CURRENT;

    if (!pDev || unit < 0 || unit >= GLS_MAX_TEX_UNITS) return;
    e = &s->texEnv[unit];

    /* The environment colour is a single device-wide constant in D3D9, so the
     * last unit to specify one wins.  Only units that actually reference
     * GL_CONSTANT set it, which keeps the common case correct. */
    if (e->mode == GLL_BLEND ||
        e->srcRGB[0] == GLL_CONSTANT || e->srcRGB[1] == GLL_CONSTANT || e->srcRGB[2] == GLL_CONSTANT ||
        e->srcAlpha[0] == GLL_CONSTANT || e->srcAlpha[1] == GLL_CONSTANT || e->srcAlpha[2] == GLL_CONSTANT)
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_TEXTUREFACTOR, _glLegacyPackColor(e->envColor));

    switch (e->mode) {
    case GLL_REPLACE:
        colorOp = D3DTOP_SELECTARG1; a1 = D3DTA_TEXTURE; a2 = D3DTA_CURRENT;
        alphaOp = D3DTOP_SELECTARG1;
        break;
    case GLL_DECAL:
        /* Cf = Cp*(1-At) + Ct*At, alpha passes through unchanged. */
        colorOp = D3DTOP_BLENDTEXTUREALPHA; a1 = D3DTA_TEXTURE; a2 = D3DTA_CURRENT;
        alphaOp = D3DTOP_SELECTARG2;
        break;
    case GLL_BLEND:
        /* Cf = Cp*(1-Ct) + Cc*Ct, Av = Ap.
         * D3DTOP_LERP = Arg1*Arg2 + (1-Arg1)*Arg3, so the interpolant Arg1
         * must be the texture alpha and Arg2/Arg3 the two operands. */
        colorOp = D3DTOP_LERP; a1 = D3DTA_TFACTOR; a2 = D3DTA_CURRENT; a0 = D3DTA_TEXTURE;
        alphaOp = D3DTOP_SELECTARG2;
        break;
    case GLL_ADD:
        /* Cf = Cp + Ct, Av = Ap. */
        colorOp = D3DTOP_ADD; a1 = D3DTA_TEXTURE; a2 = D3DTA_CURRENT;
        alphaOp = D3DTOP_SELECTARG2;
        break;
    case GLL_COMBINE:
        colorOp = _glLegacyCombineToOp(e->combineRGB,   e->rgbScale);
        alphaOp = _glLegacyCombineToOp(e->combineAlpha, e->alphaScale);

        if (e->combineRGB == GLL_INTERPOLATE) {
            /* GL: A0*A2 + (1-A0)*A1 with A0 = SOURCE0.  D3D LERP has the
             * interpolant in Arg1, so SOURCE0 goes to arg0 and the two
             * operands swap into arg2/arg1. */
            a0 = _glLegacySourceToArg(e->srcRGB[0]) | _glLegacyOperandModifier(e->operandRGB[0]);
            a1 = _glLegacySourceToArg(e->srcRGB[2]) | _glLegacyOperandModifier(e->operandRGB[2]);
            a2 = _glLegacySourceToArg(e->srcRGB[1]) | _glLegacyOperandModifier(e->operandRGB[1]);
        } else {
            a1 = _glLegacySourceToArg(e->srcRGB[0]) | _glLegacyOperandModifier(e->operandRGB[0]);
            a2 = _glLegacySourceToArg(e->srcRGB[1]) | _glLegacyOperandModifier(e->operandRGB[1]);
            a0 = _glLegacySourceToArg(e->srcRGB[2]) | _glLegacyOperandModifier(e->operandRGB[2]);
        }

        IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLOROP,   colorOp);
        IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG1, a1);
        IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG2, a2);
        IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG0, a0);

        /* DOT3 in GL writes the scalar to RGB and, for DOT3_RGBA, to alpha as
         * well.  D3D9's DOTPRODUCT3 already replicates across all four, so
         * DOT3_RGB must keep alpha from the previous stage explicitly. */
        if (e->combineRGB == GLL_DOT3_RGB) {
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            return;
        }
        if (e->combineRGB == GLL_DOT3_RGBA) {
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAOP,   D3DTOP_DOTPRODUCT3);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG1, a1);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG2, a2);
            return;
        }

        if (e->combineAlpha == GLL_INTERPOLATE) {
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAOP, alphaOp);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG1,
                _glLegacySourceToArg(e->srcAlpha[2]) | _glLegacyOperandModifier(e->operandAlpha[2]));
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG2,
                _glLegacySourceToArg(e->srcAlpha[1]) | _glLegacyOperandModifier(e->operandAlpha[1]));
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG0,
                _glLegacySourceToArg(e->srcAlpha[0]) | _glLegacyOperandModifier(e->operandAlpha[0]));
        } else {
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAOP, alphaOp);
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG1,
                _glLegacySourceToArg(e->srcAlpha[0]) | _glLegacyOperandModifier(e->operandAlpha[0]));
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG2,
                _glLegacySourceToArg(e->srcAlpha[1]) | _glLegacyOperandModifier(e->operandAlpha[1]));
            IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG0,
                _glLegacySourceToArg(e->srcAlpha[2]) | _glLegacyOperandModifier(e->operandAlpha[2]));
        }
        return;

    case GLL_MODULATE:
    default:
        colorOp = D3DTOP_MODULATE; a1 = D3DTA_TEXTURE; a2 = D3DTA_CURRENT;
        alphaOp = D3DTOP_MODULATE;
        break;
    }

    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLOROP,   colorOp);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG1, a1);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG2, a2);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_COLORARG0, a0);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAOP,   alphaOp);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(pDev, unit, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
}

void _glsTexEnvfv(unsigned int target, unsigned int pname, const float *params)
{
    GLS_State *s = glsGetState();
    int unit = _glLegacyActiveUnit(s);
    GLS_TexEnv *e = &s->texEnv[unit];
    IDirect3DDevice9 *pDev;

    if (!params) return;

    if (target == GLL_TEXTURE_FILTER_CONTROL) {
        if (pname == GLL_TEXTURE_LOD_BIAS) {
            union { float f; DWORD d; } u;
            e->lodBias = params[0];
            u.f = params[0];
            pDev = gldGetD3DDevice46();
            if (pDev)
                IDirect3DDevice9_SetSamplerState(pDev, unit, D3DSAMP_MIPMAPLODBIAS, u.d);
            return;
        }
        s->lastError = GLL_INVALID_ENUM;
        return;
    }

    if (target != GLL_TEXTURE_ENV) {
        s->lastError = GLL_INVALID_ENUM;
        return;
    }

    switch (pname) {
    case GLL_TEXTURE_ENV_MODE:   e->mode         = (GLenum_t)params[0]; break;
    case GLL_COMBINE_RGB:        e->combineRGB   = (GLenum_t)params[0]; break;
    case GLL_COMBINE_ALPHA:      e->combineAlpha = (GLenum_t)params[0]; break;
    case GLL_SOURCE0_RGB:        e->srcRGB[0]    = (GLenum_t)params[0]; break;
    case GLL_SOURCE1_RGB:        e->srcRGB[1]    = (GLenum_t)params[0]; break;
    case GLL_SOURCE2_RGB:        e->srcRGB[2]    = (GLenum_t)params[0]; break;
    case GLL_SOURCE0_ALPHA:      e->srcAlpha[0]  = (GLenum_t)params[0]; break;
    case GLL_SOURCE1_ALPHA:      e->srcAlpha[1]  = (GLenum_t)params[0]; break;
    case GLL_SOURCE2_ALPHA:      e->srcAlpha[2]  = (GLenum_t)params[0]; break;
    case GLL_OPERAND0_RGB:       e->operandRGB[0]   = (GLenum_t)params[0]; break;
    case GLL_OPERAND1_RGB:       e->operandRGB[1]   = (GLenum_t)params[0]; break;
    case GLL_OPERAND2_RGB:       e->operandRGB[2]   = (GLenum_t)params[0]; break;
    case GLL_OPERAND0_ALPHA:     e->operandAlpha[0] = (GLenum_t)params[0]; break;
    case GLL_OPERAND1_ALPHA:     e->operandAlpha[1] = (GLenum_t)params[0]; break;
    case GLL_OPERAND2_ALPHA:     e->operandAlpha[2] = (GLenum_t)params[0]; break;
    case GLL_RGB_SCALE:          e->rgbScale     = params[0]; break;
    case GLL_ALPHA_SCALE:        e->alphaScale   = params[0]; break;
    case GLL_TEXTURE_ENV_COLOR:
        e->envColor[0] = params[0]; e->envColor[1] = params[1];
        e->envColor[2] = params[2]; e->envColor[3] = params[3];
        break;
    default:
        s->lastError = GLL_INVALID_ENUM;
        return;
    }

    _glsApplyTexEnv(unit);
}

void _glsTexEnvf(unsigned int target, unsigned int pname, float param)
{
    float v[4];
    v[0] = param; v[1] = v[2] = v[3] = 0.0f;
    _glsTexEnvfv(target, pname, v);
}

void _glsTexEnvi(unsigned int target, unsigned int pname, int param)
{
    float v[4];
    v[0] = (float)param; v[1] = v[2] = v[3] = 0.0f;
    _glsTexEnvfv(target, pname, v);
}

void _glsTexEnviv(unsigned int target, unsigned int pname, const int *params)
{
    float v[4];
    if (!params) return;
    if (pname == GLL_TEXTURE_ENV_COLOR) {
        /* Integer colours are normalised from the full int range per the spec. */
        v[0] = (float)params[0] / 2147483647.0f;
        v[1] = (float)params[1] / 2147483647.0f;
        v[2] = (float)params[2] / 2147483647.0f;
        v[3] = (float)params[3] / 2147483647.0f;
    } else {
        v[0] = (float)params[0]; v[1] = v[2] = v[3] = 0.0f;
    }
    _glsTexEnvfv(target, pname, v);
}

void _glsGetTexEnvfv(unsigned int target, unsigned int pname, float *params)
{
    GLS_State *s = glsGetState();
    GLS_TexEnv *e = &s->texEnv[_glLegacyActiveUnit(s)];

    if (!params) return;

    if (target == GLL_TEXTURE_FILTER_CONTROL) {
        if (pname == GLL_TEXTURE_LOD_BIAS) { params[0] = e->lodBias; return; }
        s->lastError = GLL_INVALID_ENUM;
        return;
    }

    switch (pname) {
    case GLL_TEXTURE_ENV_MODE:   params[0] = (float)e->mode; break;
    case GLL_COMBINE_RGB:        params[0] = (float)e->combineRGB; break;
    case GLL_COMBINE_ALPHA:      params[0] = (float)e->combineAlpha; break;
    case GLL_SOURCE0_RGB:        params[0] = (float)e->srcRGB[0]; break;
    case GLL_SOURCE1_RGB:        params[0] = (float)e->srcRGB[1]; break;
    case GLL_SOURCE2_RGB:        params[0] = (float)e->srcRGB[2]; break;
    case GLL_SOURCE0_ALPHA:      params[0] = (float)e->srcAlpha[0]; break;
    case GLL_SOURCE1_ALPHA:      params[0] = (float)e->srcAlpha[1]; break;
    case GLL_SOURCE2_ALPHA:      params[0] = (float)e->srcAlpha[2]; break;
    case GLL_OPERAND0_RGB:       params[0] = (float)e->operandRGB[0]; break;
    case GLL_OPERAND1_RGB:       params[0] = (float)e->operandRGB[1]; break;
    case GLL_OPERAND2_RGB:       params[0] = (float)e->operandRGB[2]; break;
    case GLL_OPERAND0_ALPHA:     params[0] = (float)e->operandAlpha[0]; break;
    case GLL_OPERAND1_ALPHA:     params[0] = (float)e->operandAlpha[1]; break;
    case GLL_OPERAND2_ALPHA:     params[0] = (float)e->operandAlpha[2]; break;
    case GLL_RGB_SCALE:          params[0] = e->rgbScale; break;
    case GLL_ALPHA_SCALE:        params[0] = e->alphaScale; break;
    case GLL_TEXTURE_ENV_COLOR:
        params[0] = e->envColor[0]; params[1] = e->envColor[1];
        params[2] = e->envColor[2]; params[3] = e->envColor[3];
        break;
    default:
        s->lastError = GLL_INVALID_ENUM;
        break;
    }
}

void _glsGetTexEnviv(unsigned int target, unsigned int pname, int *params)
{
    float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!params) return;
    _glsGetTexEnvfv(target, pname, v);
    if (pname == GLL_TEXTURE_ENV_COLOR) {
        params[0] = (int)(v[0] * 2147483647.0f);
        params[1] = (int)(v[1] * 2147483647.0f);
        params[2] = (int)(v[2] * 2147483647.0f);
        params[3] = (int)(v[3] * 2147483647.0f);
    } else {
        params[0] = (int)v[0];
    }
}

/* Reset every unit to the GL defaults.  Called once when the state machine is
 * initialised so the stage states match what GL promises before any
 * glTexEnv call arrives. */
void _glsInitTexEnvDefaults(void)
{
    GLS_State *s = glsGetState();
    int i;

    for (i = 0; i < GLS_MAX_TEX_UNITS; i++) {
        GLS_TexEnv *e = &s->texEnv[i];
        e->mode         = GLL_MODULATE;
        e->envColor[0]  = e->envColor[1] = e->envColor[2] = e->envColor[3] = 0.0f;
        e->combineRGB   = GLL_MODULATE;
        e->combineAlpha = GLL_MODULATE;
        e->srcRGB[0]    = GLL_TEXTURE;  e->srcRGB[1]   = GLL_PREVIOUS; e->srcRGB[2]   = GLL_CONSTANT;
        e->srcAlpha[0]  = GLL_TEXTURE;  e->srcAlpha[1] = GLL_PREVIOUS; e->srcAlpha[2] = GLL_CONSTANT;
        e->operandRGB[0]   = GLL_SRC_COLOR;
        e->operandRGB[1]   = GLL_SRC_COLOR;
        e->operandRGB[2]   = GLL_SRC_ALPHA;
        e->operandAlpha[0] = GLL_SRC_ALPHA;
        e->operandAlpha[1] = GLL_SRC_ALPHA;
        e->operandAlpha[2] = GLL_SRC_ALPHA;
        e->rgbScale     = 1.0f;
        e->alphaScale   = 1.0f;
        e->lodBias      = 0.0f;
    }
}

/* ===================================================================
 *  1D textures - carried as height-1 2D textures
 * =================================================================== */

#define GLL_TEXTURE_1D              0x0DE0
#define GLL_TEXTURE_2D              0x0DE1

void _glsTexImage1D(unsigned int target, int level, int internalformat, int width,
                    int border, unsigned int format, unsigned int type, const void *pixels)
{
    if (target != GLL_TEXTURE_1D) {
        glsGetState()->lastError = GLL_INVALID_ENUM;
        return;
    }
    /* A 1D texture is exactly an Nx1 2D texture; every sampler path, filter
     * and wrap mode then works unchanged. */
    _glsTexImage2D(GLL_TEXTURE_2D, level, internalformat, width, 1, border,
                   format, type, pixels);
}

void _glsTexSubImage1D(unsigned int target, int level, int xoffset, int width,
                       unsigned int format, unsigned int type, const void *pixels)
{
    if (target != GLL_TEXTURE_1D) {
        glsGetState()->lastError = GLL_INVALID_ENUM;
        return;
    }
    _glsTexSubImage2D(GLL_TEXTURE_2D, level, xoffset, 0, width, 1,
                      format, type, pixels);
}


/* Draw an RGBA image with its lower-left corner at window (x,y).
 * glDrawPixels reads the raster position, so it is set around the call and
 * restored afterwards - callers here place images explicitly. */
static void _glLegacyBlitRGBA(float x, float y, int w, int h, const unsigned char *rgba)
{
    GLS_State *s = glsGetState();
    float saved[4];
    GLboolean_t savedValid = s->rasterPosValid;

    saved[0] = s->rasterPos[0]; saved[1] = s->rasterPos[1];
    saved[2] = s->rasterPos[2]; saved[3] = s->rasterPos[3];

    s->rasterPos[0] = x;
    s->rasterPos[1] = y;
    s->rasterPosValid = GLL_TRUE;

    _glsDrawPixels(w, h, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, rgba);

    s->rasterPos[0] = saved[0]; s->rasterPos[1] = saved[1];
    s->rasterPos[2] = saved[2]; s->rasterPos[3] = saved[3];
    s->rasterPosValid = savedValid;
}

/* ===================================================================
 *  Raster position
 * =================================================================== */

/*
 * glRasterPos transforms an object-space point by the modelview and
 * projection matrices, divides by w, and maps to window coordinates - the
 * same path a vertex takes.  glWindowPos skips all of that and takes window
 * coordinates directly.
 */
void _glsRasterPos4f(float x, float y, float z, float w)
{
    GLL_DLFloat4 args = {{ x, y, z, w }};
    GLS_State *s = glsGetState();
    float eye[4], clip[4];
    const float *mv, *pr;
    int i;

    if (_gllDLRecord(_gllDLRasterPos, &args, sizeof(args))) return;

    mv = s->modelviewStack.stack[s->modelviewStack.top].m;
    pr = s->projectionStack.stack[s->projectionStack.top].m;

    /* Column-major matrices, column-vector convention: v' = M * v */
    for (i = 0; i < 4; i++)
        eye[i] = mv[i] * x + mv[4 + i] * y + mv[8 + i] * z + mv[12 + i] * w;
    for (i = 0; i < 4; i++)
        clip[i] = pr[i] * eye[0] + pr[4 + i] * eye[1] + pr[8 + i] * eye[2] + pr[12 + i] * eye[3];

    if (clip[3] == 0.0f) {
        s->rasterPosValid = GLL_FALSE;
        return;
    }

    clip[0] /= clip[3];
    clip[1] /= clip[3];
    clip[2] /= clip[3];

    s->rasterPos[0] = (float)s->viewportX + ((float)s->viewportW  * 0.5f) * (clip[0] + 1.0f);
    s->rasterPos[1] = (float)s->viewportY + ((float)s->viewportH * 0.5f) * (clip[1] + 1.0f);
    s->rasterPos[2] = (s->depthRangeFar - s->depthRangeNear) * 0.5f * clip[2]
                    + (s->depthRangeFar + s->depthRangeNear) * 0.5f;
    s->rasterPos[3] = 1.0f / clip[3];
    s->rasterPosValid = GLL_TRUE;

    /* The raster colour and texture coordinate are latched from current state
     * at the moment the position is set, per the spec. */
    s->rasterColor[0] = s->currentColor[0]; s->rasterColor[1] = s->currentColor[1];
    s->rasterColor[2] = s->currentColor[2]; s->rasterColor[3] = s->currentColor[3];
    s->rasterIndex    = s->currentIndex;
    {
        int unit = (int)(s->activeTexUnit - GLL_TEXTURE0);
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        s->rasterTexCoord[0] = s->currentTexCoord[unit][0];
        s->rasterTexCoord[1] = s->currentTexCoord[unit][1];
        s->rasterTexCoord[2] = s->currentTexCoord[unit][2];
        s->rasterTexCoord[3] = s->currentTexCoord[unit][3];
    }
}

void _glsWindowPos3f(float x, float y, float z)
{
    GLL_DLFloat4 args = {{ x, y, z, 1.0f }};
    GLS_State *s = glsGetState();
    if (_gllDLRecord(_gllDLWindowPos, &args, sizeof(args))) return;
    s->rasterPos[0] = x;
    s->rasterPos[1] = y;
    s->rasterPos[2] = z;
    s->rasterPos[3] = 1.0f;
    s->rasterPosValid = GLL_TRUE;
    s->rasterColor[0] = s->currentColor[0]; s->rasterColor[1] = s->currentColor[1];
    s->rasterColor[2] = s->currentColor[2]; s->rasterColor[3] = s->currentColor[3];
    s->rasterIndex    = s->currentIndex;
    {
        int unit = (int)(s->activeTexUnit - GLL_TEXTURE0);
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        s->rasterTexCoord[0] = s->currentTexCoord[unit][0];
        s->rasterTexCoord[1] = s->currentTexCoord[unit][1];
        s->rasterTexCoord[2] = s->currentTexCoord[unit][2];
        s->rasterTexCoord[3] = s->currentTexCoord[unit][3];
    }
}

void _glsPixelZoom(float xfactor, float yfactor)
{
    GLS_State *s = glsGetState();
    s->pixelZoomX = xfactor;
    s->pixelZoomY = yfactor;
}

/*
 * glBitmap draws a 1-bit stipple at the raster position and then advances it.
 *
 * The advance is the part programs actually depend on - it is how GL text
 * rendering walks a string - so it is always applied.  The glyph itself is
 * expanded to RGBA and pushed through the same path glDrawPixels uses.
 */
void _glsBitmap(int width, int height, float xorig, float yorig,
                float xmove, float ymove, const unsigned char *bitmap)
{
    GLL_DLBitmap header;
    unsigned char *record = NULL;
    size_t bitmapBytes = 0, recordBytes;
    GLS_State *s = glsGetState();
    unsigned char *rgba;
    int rowBytes, x, y;
    float px, py;

    if (gldDLIsRecording()) {
        if (width > 0 && height > 0 && bitmap) {
            size_t rb = ((size_t)width + 7u) / 8u;
            if (rb > (size_t)INT_MAX / (size_t)height) {
                s->lastError = 0x0505;
                return;
            }
            bitmapBytes = rb * (size_t)height;
        }
        recordBytes = sizeof(header) + bitmapBytes;
        if (recordBytes > (size_t)INT_MAX) {
            s->lastError = 0x0505;
            return;
        }
        record = (unsigned char *)malloc(recordBytes);
        if (!record) {
            s->lastError = 0x0505;
            return;
        }
        header.width = width; header.height = height;
        header.xorig = xorig; header.yorig = yorig;
        header.xmove = xmove; header.ymove = ymove;
        header.bitmapBytes = (int)bitmapBytes;
        memcpy(record, &header, sizeof(header));
        if (bitmapBytes) memcpy(record + sizeof(header), bitmap, bitmapBytes);
        if (_gllDLRecord(_gllDLBitmap, record, (int)recordBytes)) {
            free(record);
            return;
        }
        free(record);
    }

    if (s->rasterPosValid && bitmap && width > 0 && height > 0) {
        rowBytes = (width + 7) / 8;
        rgba = (unsigned char *)malloc((size_t)width * height * 4);
        if (rgba) {
            DWORD c = _glLegacyPackColor(s->rasterColor);
            unsigned char cr = (unsigned char)((c >> 16) & 0xFF);
            unsigned char cg = (unsigned char)((c >>  8) & 0xFF);
            unsigned char cb = (unsigned char)( c        & 0xFF);

            for (y = 0; y < height; y++) {
                const unsigned char *row = bitmap + (size_t)y * rowBytes;
                for (x = 0; x < width; x++) {
                    /* GL packs the leftmost pixel in the most significant bit. */
                    int set = (row[x >> 3] >> (7 - (x & 7))) & 1;
                    unsigned char *p = rgba + (((size_t)y * width) + x) * 4;
                    p[0] = cr; p[1] = cg; p[2] = cb;
                    p[3] = set ? 0xFF : 0x00;
                }
            }

            px = s->rasterPos[0] - xorig;
            py = s->rasterPos[1] - yorig;
            _glLegacyBlitRGBA(px, py, width, height, rgba);
            free(rgba);
        }
    }

    /* The raster position advances whether or not anything was drawn. */
    s->rasterPos[0] += xmove;
    s->rasterPos[1] += ymove;
}

/* ===================================================================
 *  Evaluators
 * =================================================================== */

static int _glLegacyMap1Index(GLenum_t target, int *components)
{
    switch (target) {
    case GLL_MAP1_VERTEX_3:        *components = 3; return 0;
    case GLL_MAP1_VERTEX_4:        *components = 4; return 1;
    case GLL_MAP1_INDEX:           *components = 1; return 2;
    case GLL_MAP1_COLOR_4:         *components = 4; return 3;
    case GLL_MAP1_NORMAL:          *components = 3; return 4;
    case GLL_MAP1_TEXTURE_COORD_1: *components = 1; return 5;
    case GLL_MAP1_TEXTURE_COORD_2: *components = 2; return 6;
    case GLL_MAP1_TEXTURE_COORD_3: *components = 3; return 7;
    case GLL_MAP1_TEXTURE_COORD_4: *components = 4; return 8;
    default:                       *components = 0; return -1;
    }
}

static int _glLegacyMap2Index(GLenum_t target, int *components)
{
    switch (target) {
    case GLL_MAP2_VERTEX_3:        *components = 3; return 0;
    case GLL_MAP2_VERTEX_4:        *components = 4; return 1;
    case GLL_MAP2_INDEX:           *components = 1; return 2;
    case GLL_MAP2_COLOR_4:         *components = 4; return 3;
    case GLL_MAP2_NORMAL:          *components = 3; return 4;
    case GLL_MAP2_TEXTURE_COORD_1: *components = 1; return 5;
    case GLL_MAP2_TEXTURE_COORD_2: *components = 2; return 6;
    case GLL_MAP2_TEXTURE_COORD_3: *components = 3; return 7;
    case GLL_MAP2_TEXTURE_COORD_4: *components = 4; return 8;
    default:                       *components = 0; return -1;
    }
}

/* Bernstein basis of degree n at t, written into b[0..n]. */
static void _glLegacyBernstein(int n, float t, float *b)
{
    float u = 1.0f - t;
    int i, j;

    b[0] = 1.0f;
    for (i = 1; i <= n; i++) {
        float saved = 0.0f;
        for (j = 0; j < i; j++) {
            float tmp = b[j];
            b[j] = saved + u * tmp;
            saved = t * tmp;
        }
        b[i] = saved;
    }
}

void _glsMap1f(unsigned int target, float u1, float u2, int stride, int order,
               const float *points)
{
    GLS_State *s = glsGetState();
    int comps, idx = _glLegacyMap1Index(target, &comps);
    GLS_Map1 *m;
    int i, c;

    if (idx < 0)                         { s->lastError = GLL_INVALID_ENUM;  return; }
    if (order < 1 || order > GLS_MAX_EVAL_ORDER) {
        s->lastError = GLL_INVALID_VALUE;
        gldDiagLog("GL: Map1 order %d outside 1..%d - ignored", order, GLS_MAX_EVAL_ORDER);
        return;
    }
    if (stride < comps || u1 == u2 || !points) { s->lastError = GLL_INVALID_VALUE; return; }

    m = &s->map1[idx];
    m->defined    = TRUE;
    m->u1         = u1;
    m->u2         = u2;
    m->order      = order;
    m->components = comps;
    for (i = 0; i < order; i++)
        for (c = 0; c < comps; c++)
            m->points[i * 4 + c] = points[i * stride + c];
}

void _glsMap2f(unsigned int target, float u1, float u2, int ustride, int uorder,
               float v1, float v2, int vstride, int vorder, const float *points)
{
    GLS_State *s = glsGetState();
    int comps, idx = _glLegacyMap2Index(target, &comps);
    GLS_Map2 *m;
    int i, j, c;

    if (idx < 0)                          { s->lastError = GLL_INVALID_ENUM;  return; }
    if (uorder < 1 || uorder > GLS_MAX_EVAL_ORDER ||
        vorder < 1 || vorder > GLS_MAX_EVAL_ORDER) {
        s->lastError = GLL_INVALID_VALUE;
        gldDiagLog("GL: Map2 order %dx%d outside 1..%d - ignored",
                   uorder, vorder, GLS_MAX_EVAL_ORDER);
        return;
    }
    if (u1 == u2 || v1 == v2 || !points)  { s->lastError = GLL_INVALID_VALUE; return; }

    m = &s->map2[idx];
    m->defined    = TRUE;
    m->u1 = u1; m->u2 = u2; m->v1 = v1; m->v2 = v2;
    m->uorder = uorder; m->vorder = vorder;
    m->components = comps;

    for (i = 0; i < uorder; i++)
        for (j = 0; j < vorder; j++)
            for (c = 0; c < comps; c++)
                m->points[((i * GLS_MAX_EVAL_ORDER) + j) * 4 + c] =
                    points[i * ustride + j * vstride + c];
}

/* Evaluate one 1D map at parametric u, writing `components` floats to out. */
static BOOL _glLegacyEvalMap1(GLS_Map1 *m, float u, float *out)
{
    float basis[GLS_MAX_EVAL_ORDER];
    float t;
    int i, c;

    if (!m->defined || !m->enabled) return FALSE;

    t = (u - m->u1) / (m->u2 - m->u1);
    _glLegacyBernstein(m->order - 1, t, basis);

    for (c = 0; c < m->components; c++) out[c] = 0.0f;
    for (i = 0; i < m->order; i++)
        for (c = 0; c < m->components; c++)
            out[c] += basis[i] * m->points[i * 4 + c];
    return TRUE;
}

static BOOL _glLegacyEvalMap2(GLS_Map2 *m, float u, float v, float *out)
{
    float ub[GLS_MAX_EVAL_ORDER], vb[GLS_MAX_EVAL_ORDER];
    float tu, tv;
    int i, j, c;

    if (!m->defined || !m->enabled) return FALSE;

    tu = (u - m->u1) / (m->u2 - m->u1);
    tv = (v - m->v1) / (m->v2 - m->v1);
    _glLegacyBernstein(m->uorder - 1, tu, ub);
    _glLegacyBernstein(m->vorder - 1, tv, vb);

    for (c = 0; c < m->components; c++) out[c] = 0.0f;
    for (i = 0; i < m->uorder; i++)
        for (j = 0; j < m->vorder; j++) {
            float w = ub[i] * vb[j];
            for (c = 0; c < m->components; c++)
                out[c] += w * m->points[((i * GLS_MAX_EVAL_ORDER) + j) * 4 + c];
        }
    return TRUE;
}

/*
 * Emit one evaluated vertex.
 *
 * Order matters: colour, normal and texture coordinate are current-state
 * changes and must be issued before the vertex that consumes them.
 */
static void _glLegacyEmitEval1(GLS_State *s, float u)
{
    float v[4];

    if (_glLegacyEvalMap1(&s->map1[3], u, v)) _glsColor4f(v[0], v[1], v[2], v[3]);
    if (_glLegacyEvalMap1(&s->map1[4], u, v)) _glsNormal3f(v[0], v[1], v[2]);
    if (_glLegacyEvalMap1(&s->map1[5], u, v)) _glsTexCoord2f(v[0], 0.0f);
    if (_glLegacyEvalMap1(&s->map1[6], u, v)) _glsTexCoord2f(v[0], v[1]);
    if (_glLegacyEvalMap1(&s->map1[7], u, v)) _glsTexCoord3f(v[0], v[1], v[2]);
    if (_glLegacyEvalMap1(&s->map1[8], u, v)) _glsTexCoord4f(v[0], v[1], v[2], v[3]);

    if (_glLegacyEvalMap1(&s->map1[1], u, v))      _glsVertex4f(v[0], v[1], v[2], v[3]);
    else if (_glLegacyEvalMap1(&s->map1[0], u, v)) _glsVertex3f(v[0], v[1], v[2]);
}

static void _glLegacyEmitEval2(GLS_State *s, float u, float vv)
{
    float v[4];

    if (_glLegacyEvalMap2(&s->map2[3], u, vv, v)) _glsColor4f(v[0], v[1], v[2], v[3]);
    if (_glLegacyEvalMap2(&s->map2[4], u, vv, v)) _glsNormal3f(v[0], v[1], v[2]);
    if (_glLegacyEvalMap2(&s->map2[5], u, vv, v)) _glsTexCoord2f(v[0], 0.0f);
    if (_glLegacyEvalMap2(&s->map2[6], u, vv, v)) _glsTexCoord2f(v[0], v[1]);
    if (_glLegacyEvalMap2(&s->map2[7], u, vv, v)) _glsTexCoord3f(v[0], v[1], v[2]);
    if (_glLegacyEvalMap2(&s->map2[8], u, vv, v)) _glsTexCoord4f(v[0], v[1], v[2], v[3]);

    if (_glLegacyEvalMap2(&s->map2[1], u, vv, v))      _glsVertex4f(v[0], v[1], v[2], v[3]);
    else if (_glLegacyEvalMap2(&s->map2[0], u, vv, v)) _glsVertex3f(v[0], v[1], v[2]);
}

void _glsEvalCoord1f(float u)
{
    _glLegacyEmitEval1(glsGetState(), u);
}

void _glsEvalCoord2f(float u, float v)
{
    _glLegacyEmitEval2(glsGetState(), u, v);
}

void _glsMapGrid1f(int un, float u1, float u2)
{
    GLS_State *s = glsGetState();
    if (un < 1) { s->lastError = GLL_INVALID_VALUE; return; }
    s->mapGrid1n = un; s->mapGrid1u1 = u1; s->mapGrid1u2 = u2;
}

void _glsMapGrid2f(int un, float u1, float u2, int vn, float v1, float v2)
{
    GLS_State *s = glsGetState();
    if (un < 1 || vn < 1) { s->lastError = GLL_INVALID_VALUE; return; }
    s->mapGrid2un = un; s->mapGrid2u1 = u1; s->mapGrid2u2 = u2;
    s->mapGrid2vn = vn; s->mapGrid2v1 = v1; s->mapGrid2v2 = v2;
}

void _glsEvalMesh1(unsigned int mode, int i1, int i2)
{
    GLS_State *s = glsGetState();
    float du;
    int i;

    if (mode != GLL_POINT && mode != GLL_LINE) {
        s->lastError = GLL_INVALID_ENUM;
        return;
    }
    if (s->mapGrid1n < 1) return;
    du = (s->mapGrid1u2 - s->mapGrid1u1) / (float)s->mapGrid1n;

    _glsBegin(mode == GLL_POINT ? GLL_POINTS : GLL_LINE_STRIP);
    for (i = i1; i <= i2; i++)
        _glLegacyEmitEval1(s, s->mapGrid1u1 + (float)i * du);
    _glsEnd();
}

void _glsEvalMesh2(unsigned int mode, int i1, int i2, int j1, int j2)
{
    GLS_State *s = glsGetState();
    float du, dv;
    int i, j;

    if (mode != GLL_POINT && mode != GLL_LINE && mode != GLL_FILL) {
        s->lastError = GLL_INVALID_ENUM;
        return;
    }
    if (s->mapGrid2un < 1 || s->mapGrid2vn < 1) return;
    du = (s->mapGrid2u2 - s->mapGrid2u1) / (float)s->mapGrid2un;
    dv = (s->mapGrid2v2 - s->mapGrid2v1) / (float)s->mapGrid2vn;

    if (mode == GLL_POINT) {
        _glsBegin(GLL_POINTS);
        for (i = i1; i <= i2; i++)
            for (j = j1; j <= j2; j++)
                _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)i * du,
                                      s->mapGrid2v1 + (float)j * dv);
        _glsEnd();
        return;
    }

    if (mode == GLL_LINE) {
        for (i = i1; i <= i2; i++) {
            _glsBegin(GLL_LINE_STRIP);
            for (j = j1; j <= j2; j++)
                _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)i * du,
                                      s->mapGrid2v1 + (float)j * dv);
            _glsEnd();
        }
        for (j = j1; j <= j2; j++) {
            _glsBegin(GLL_LINE_STRIP);
            for (i = i1; i <= i2; i++)
                _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)i * du,
                                      s->mapGrid2v1 + (float)j * dv);
            _glsEnd();
        }
        return;
    }

    /* GL_FILL: one triangle strip per row of the grid. */
    for (i = i1; i < i2; i++) {
        _glsBegin(GLL_TRIANGLE_STRIP);
        for (j = j1; j <= j2; j++) {
            _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)i * du,
                                  s->mapGrid2v1 + (float)j * dv);
            _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)(i + 1) * du,
                                  s->mapGrid2v1 + (float)j * dv);
        }
        _glsEnd();
    }
}

void _glsEvalPoint1(int i)
{
    GLS_State *s = glsGetState();
    float du;
    if (s->mapGrid1n < 1) return;
    du = (s->mapGrid1u2 - s->mapGrid1u1) / (float)s->mapGrid1n;
    _glsBegin(GLL_POINTS);
    _glLegacyEmitEval1(s, s->mapGrid1u1 + (float)i * du);
    _glsEnd();
}

void _glsEvalPoint2(int i, int j)
{
    GLS_State *s = glsGetState();
    float du, dv;
    if (s->mapGrid2un < 1 || s->mapGrid2vn < 1) return;
    du = (s->mapGrid2u2 - s->mapGrid2u1) / (float)s->mapGrid2un;
    dv = (s->mapGrid2v2 - s->mapGrid2v1) / (float)s->mapGrid2vn;
    _glsBegin(GLL_POINTS);
    _glLegacyEmitEval2(s, s->mapGrid2u1 + (float)i * du,
                          s->mapGrid2v1 + (float)j * dv);
    _glsEnd();
}

void _glsGetMapfv(unsigned int target, unsigned int query, float *v)
{
    GLS_State *s = glsGetState();
    int comps, i1 = _glLegacyMap1Index(target, &comps);
    int i2 = (i1 < 0) ? _glLegacyMap2Index(target, &comps) : -1;

    if (!v) return;

    if (i1 >= 0) {
        GLS_Map1 *m = &s->map1[i1];
        int i, c, n = 0;
        switch (query) {
        case 0x0A00: /* GL_COEFF  */
            for (i = 0; i < m->order; i++)
                for (c = 0; c < m->components; c++) v[n++] = m->points[i * 4 + c];
            break;
        case 0x0A01: /* GL_ORDER  */ v[0] = (float)m->order; break;
        case 0x0A02: /* GL_DOMAIN */ v[0] = m->u1; v[1] = m->u2; break;
        default: s->lastError = GLL_INVALID_ENUM; break;
        }
        return;
    }
    if (i2 >= 0) {
        GLS_Map2 *m = &s->map2[i2];
        int i, j, c, n = 0;
        switch (query) {
        case 0x0A00:
            for (i = 0; i < m->uorder; i++)
                for (j = 0; j < m->vorder; j++)
                    for (c = 0; c < m->components; c++)
                        v[n++] = m->points[((i * GLS_MAX_EVAL_ORDER) + j) * 4 + c];
            break;
        case 0x0A01: v[0] = (float)m->uorder; v[1] = (float)m->vorder; break;
        case 0x0A02: v[0] = m->u1; v[1] = m->u2; v[2] = m->v1; v[3] = m->v2; break;
        default: s->lastError = GLL_INVALID_ENUM; break;
        }
        return;
    }
    s->lastError = GLL_INVALID_ENUM;
}

void _glsGetMapdv(unsigned int target, unsigned int query, double *v)
{
    GLS_State *s = glsGetState();
    int comps, i1 = _glLegacyMap1Index((GLenum_t)target, &comps);
    int i2 = (i1 < 0) ? _glLegacyMap2Index((GLenum_t)target, &comps) : -1;
    int i, j, c, n = 0;

    if (!v) return;

    if (i1 >= 0) {
        GLS_Map1 *m = &s->map1[i1];
        switch (query) {
        case 0x0A00: /* GL_COEFF  */
            for (i = 0; i < m->order; i++)
                for (c = 0; c < m->components; c++) v[n++] = m->points[i * 4 + c];
            break;
        case 0x0A01: /* GL_ORDER  */ v[0] = m->order; break;
        case 0x0A02: /* GL_DOMAIN */ v[0] = m->u1; v[1] = m->u2; break;
        default: s->lastError = GLL_INVALID_ENUM; break;
        }
        return;
    }
    if (i2 >= 0) {
        GLS_Map2 *m = &s->map2[i2];
        switch (query) {
        case 0x0A00:
            for (i = 0; i < m->uorder; i++)
                for (j = 0; j < m->vorder; j++)
                    for (c = 0; c < m->components; c++)
                        v[n++] = m->points[((i * GLS_MAX_EVAL_ORDER) + j) * 4 + c];
            break;
        case 0x0A01: v[0] = m->uorder; v[1] = m->vorder; break;
        case 0x0A02: v[0] = m->u1; v[1] = m->u2; v[2] = m->v1; v[3] = m->v2; break;
        default: s->lastError = GLL_INVALID_ENUM; break;
        }
        return;
    }
    s->lastError = GLL_INVALID_ENUM;
}

/* Enable/disable hook for the GL_MAP1_x and GL_MAP2_x capabilities. */
BOOL _glsSetEvalEnable(unsigned int cap, BOOL enable)
{
    GLS_State *s = glsGetState();
    int comps, idx;

    idx = _glLegacyMap1Index(cap, &comps);
    if (idx >= 0) { s->map1[idx].enabled = enable; return TRUE; }

    idx = _glLegacyMap2Index(cap, &comps);
    if (idx >= 0) { s->map2[idx].enabled = enable; return TRUE; }

    if (cap == 0x0D80) { s->autoNormal = enable ? GLL_TRUE : GLL_FALSE; return TRUE; }
    return FALSE;
}

/* ===================================================================
 *  Selection and feedback
 * =================================================================== */

void _glsSelectBuffer(int size, unsigned int *buffer)
{
    GLS_State *s = glsGetState();
    if (s->renderMode == GLL_SELECT) { s->lastError = GLL_INVALID_OPERATION; return; }
    if (size < 0)                    { s->lastError = GLL_INVALID_VALUE;    return; }
    s->selectBuffer     = buffer;
    s->selectBufferSize = size;
    s->selectIndex      = 0;
}

void _glsFeedbackBuffer(int size, unsigned int type, float *buffer)
{
    GLS_State *s = glsGetState();
    if (s->renderMode == GLL_FEEDBACK) { s->lastError = GLL_INVALID_OPERATION; return; }
    if (size < 0)                      { s->lastError = GLL_INVALID_VALUE;    return; }
    /* GL_2D, GL_3D, GL_3D_COLOR, GL_3D_COLOR_TEXTURE, GL_4D_COLOR_TEXTURE */
    if (type != 0x0600 && type != 0x0601 && type != 0x0602 &&
        type != 0x0603 && type != 0x0604) {
        s->lastError = GLL_INVALID_ENUM;
        return;
    }
    s->feedbackBuffer     = buffer;
    s->feedbackBufferSize = size;
    s->feedbackType       = type;
    s->feedbackIndex      = 0;
    s->feedbackOverflow   = GLL_FALSE;
}

void _glsInitNames(void)
{
    GLS_State *s = glsGetState();
    s->nameStackDepth   = 0;
    s->selectHitPending = GLL_FALSE;
}

void _glsPushName(unsigned int name)
{
    GLS_State *s = glsGetState();
    if (s->nameStackDepth >= GLS_MAX_NAME_STACK) {
        s->lastError = 0x0503; /* GL_STACK_OVERFLOW */
        return;
    }
    s->nameStack[s->nameStackDepth++] = name;
}

void _glsPopName(void)
{
    GLS_State *s = glsGetState();
    if (s->nameStackDepth <= 0) {
        s->lastError = 0x0504; /* GL_STACK_UNDERFLOW */
        return;
    }
    s->nameStackDepth--;
}

void _glsLoadName(unsigned int name)
{
    GLS_State *s = glsGetState();
    if (s->nameStackDepth <= 0) {
        s->lastError = GLL_INVALID_OPERATION;
        return;
    }
    s->nameStack[s->nameStackDepth - 1] = name;
}

void _glsPassThrough(float token)
{
    GLS_State *s = glsGetState();
    if (s->renderMode != GLL_FEEDBACK || !s->feedbackBuffer) return;
    if (s->feedbackIndex + 2 > s->feedbackBufferSize) {
        s->feedbackOverflow = GLL_TRUE;
        return;
    }
    s->feedbackBuffer[s->feedbackIndex++] = 0x0700f; /* GL_PASS_THROUGH_TOKEN */
    s->feedbackBuffer[s->feedbackIndex++] = token;
}

int _glsRenderMode(unsigned int mode)
{
    GLS_State *s = glsGetState();
    int result = 0;

    /* The return value reports on the mode being left, not the one entered. */
    switch (s->renderMode) {
    case GLL_SELECT:
        result = s->selectHits;
        s->selectHits      = 0;
        s->selectIndex     = 0;
        s->selectHitPending = GLL_FALSE;
        break;
    case GLL_FEEDBACK:
        result = s->feedbackOverflow ? -1 : (int)s->feedbackIndex;
        s->feedbackIndex    = 0;
        s->feedbackOverflow = GLL_FALSE;
        break;
    default:
        result = 0;
        break;
    }

    if (mode != GLL_RENDER && mode != GLL_SELECT && mode != GLL_FEEDBACK) {
        s->lastError = GLL_INVALID_ENUM;
        return result;
    }

    if (mode == GLL_SELECT && !s->selectBuffer)   { s->lastError = GLL_INVALID_OPERATION; return result; }
    if (mode == GLL_FEEDBACK && !s->feedbackBuffer) { s->lastError = GLL_INVALID_OPERATION; return result; }

    s->renderMode = mode;
    if (mode == GLL_SELECT) {
        s->selectIndex      = 0;
        s->selectHits       = 0;
        s->selectHitPending = GLL_FALSE;
        s->nameStackDepth   = 0;
    }
    if (mode == GLL_FEEDBACK) {
        s->feedbackIndex    = 0;
        s->feedbackOverflow = GLL_FALSE;
    }

    if (mode != GLL_RENDER)
        gldDiagLogV("GL: RenderMode 0x%X entered - primitives are recorded, not drawn", mode);

    return result;
}

/*
 * Record a selection hit for the primitive just processed.
 *
 * Called from the draw path when renderMode is GL_SELECT.  A hit record is
 * {number of names, min z, max z, name stack contents}, with z normalised to
 * the full unsigned range as the spec requires.
 */
void _glsSelectRecordHit(float minZ, float maxZ)
{
    GLS_State *s = glsGetState();
    unsigned int zmin, zmax;
    int i;

    if (s->renderMode != GLL_SELECT || !s->selectBuffer) return;
    if (s->nameStackDepth <= 0) return;

    if (minZ < 0.0f) minZ = 0.0f;
    if (maxZ > 1.0f) maxZ = 1.0f;
    zmin = (unsigned int)(minZ * 4294967295.0);
    zmax = (unsigned int)(maxZ * 4294967295.0);

    if (s->selectIndex + 3 + s->nameStackDepth > s->selectBufferSize) {
        /* Overflow is reported by glRenderMode returning -1; GL says the
         * contents past this point are undefined, so simply stop writing. */
        s->selectHits = -1;
        return;
    }

    s->selectBuffer[s->selectIndex++] = (unsigned int)s->nameStackDepth;
    s->selectBuffer[s->selectIndex++] = zmin;
    s->selectBuffer[s->selectIndex++] = zmax;
    for (i = 0; i < s->nameStackDepth; i++)
        s->selectBuffer[s->selectIndex++] = s->nameStack[i];

    if (s->selectHits >= 0) s->selectHits++;
}

/* ===================================================================
 *  Stipple, edge flags, colour index
 * =================================================================== */

void _glsPolygonStipple(const unsigned char *mask)
{
    GLS_State *s = glsGetState();
    static BOOL warned = FALSE;

    if (mask) memcpy(s->polygonStipple, mask, sizeof(s->polygonStipple));

    if (!warned) {
        warned = TRUE;
        gldFlagFault("feature", "polygon-stipple");
        gldDiagLog("GL: PolygonStipple stored but not applied - D3D9 has no "
                   "polygon stipple; geometry draws unstippled");
    }
}

void _glsGetPolygonStipple(unsigned char *mask)
{
    GLS_State *s = glsGetState();
    if (mask) memcpy(mask, s->polygonStipple, sizeof(s->polygonStipple));
}

void _glsLineStipple(int factor, unsigned short pattern)
{
    GLS_State *s = glsGetState();
    static BOOL warned = FALSE;

    s->lineStippleFactor  = factor;
    s->lineStipplePattern = pattern;

    if (!warned) {
        warned = TRUE;
        gldFlagFault("feature", "line-stipple");
        gldDiagLog("GL: LineStipple stored but not applied - D3D9 has no line "
                   "stipple; lines draw solid");
    }
}

void _glsEdgeFlag(unsigned char flag)
{
    GLS_State *s = glsGetState();
    static BOOL warned = FALSE;

    s->edgeFlag = flag ? GLL_TRUE : GLL_FALSE;

    /* Edge flags only change what GL_LINE/GL_POINT polygon mode draws.  D3D9
     * has no per-vertex edge flag, so boundary selection is not reproduced. */
    if (!warned && !flag &&
        (s->polygonModeFront != GLL_FILL || s->polygonModeBack != GLL_FILL)) {
        warned = TRUE;
        gldFlagFault("feature", "edge-flag");
        gldDiagLogV("GL: EdgeFlag(FALSE) with non-FILL polygon mode - D3D9 has "
                   "no edge flags; all polygon edges will be drawn");
    }
}

void _glsIndexf(float c)
{
    glsGetState()->currentIndex = c;
}

void _glsClearIndex(float c)
{
    glsGetState()->clearIndexValue = c;
}

void _glsIndexMask(unsigned int mask)
{
    GLS_State *s = glsGetState();
    static BOOL warned = FALSE;

    s->indexWriteMask = mask;
    if (!warned) {
        warned = TRUE;
        gldFlagFault("feature", "colour-index-mask");
        gldDiagLog("GL: IndexMask - colour-index rendering has no D3D9 "
                   "equivalent; the device is always RGBA");
    }
}

/* ===================================================================
 *  Pixel transfer
 * =================================================================== */

static int _glLegacyPixelMapIndex(GLenum_t map)
{
    switch (map) {
    case 0x0C70: return 0;  /* GL_PIXEL_MAP_I_TO_I */
    case 0x0C71: return 1;  /* GL_PIXEL_MAP_S_TO_S */
    case 0x0C72: return 2;  /* GL_PIXEL_MAP_I_TO_R */
    case 0x0C73: return 3;  /* GL_PIXEL_MAP_I_TO_G */
    case 0x0C74: return 4;  /* GL_PIXEL_MAP_I_TO_B */
    case 0x0C75: return 5;  /* GL_PIXEL_MAP_I_TO_A */
    case 0x0C76: return 6;  /* GL_PIXEL_MAP_R_TO_R */
    case 0x0C77: return 7;  /* GL_PIXEL_MAP_G_TO_G */
    case 0x0C78: return 8;  /* GL_PIXEL_MAP_B_TO_B */
    case 0x0C79: return 9;  /* GL_PIXEL_MAP_A_TO_A */
    default:     return -1;
    }
}

void _glsPixelMapfv(unsigned int map, int mapsize, const float *values)
{
    GLS_State *s = glsGetState();
    int idx = _glLegacyPixelMapIndex(map);
    int i;

    if (idx < 0)                                  { s->lastError = GLL_INVALID_ENUM;  return; }
    if (mapsize < 1 || mapsize > GLS_MAX_PIXEL_MAP || !values) {
        s->lastError = GLL_INVALID_VALUE;
        return;
    }

    for (i = 0; i < mapsize; i++) s->pixelMap[idx][i] = values[i];
    s->pixelMapSize[idx] = mapsize;
}

void _glsGetPixelMapfv(unsigned int map, float *values)
{
    GLS_State *s = glsGetState();
    int idx = _glLegacyPixelMapIndex(map);
    int i;

    if (idx < 0) { s->lastError = GLL_INVALID_ENUM; return; }
    if (!values) return;
    for (i = 0; i < s->pixelMapSize[idx]; i++) values[i] = s->pixelMap[idx][i];
}

void _glsPixelTransferf(unsigned int pname, float param)
{
    GLS_State *s = glsGetState();

    switch (pname) {
    case 0x0D14: s->redScale    = param; break;  /* GL_RED_SCALE     */
    case 0x0D18: s->greenScale  = param; break;  /* GL_GREEN_SCALE   */
    case 0x0D1A: s->blueScale   = param; break;  /* GL_BLUE_SCALE    */
    case 0x0D1C: s->alphaScale  = param; break;  /* GL_ALPHA_SCALE   */
    case 0x0D1E: s->depthScale  = param; break;  /* GL_DEPTH_SCALE   */
    case 0x0D15: s->redBias     = param; break;  /* GL_RED_BIAS      */
    case 0x0D19: s->greenBias   = param; break;  /* GL_GREEN_BIAS    */
    case 0x0D1B: s->blueBias    = param; break;  /* GL_BLUE_BIAS     */
    case 0x0D1D: s->alphaBias   = param; break;  /* GL_ALPHA_BIAS    */
    case 0x0D1F: s->depthBias   = param; break;  /* GL_DEPTH_BIAS    */
    case 0x0D12: s->indexShift  = param; break;  /* GL_INDEX_SHIFT   */
    case 0x0D13: s->indexOffset = param; break;  /* GL_INDEX_OFFSET  */
    case 0x0D10: s->mapColorFlag   = (param != 0.0f); break; /* GL_MAP_COLOR   */
    case 0x0D11: s->mapStencilFlag = (param != 0.0f); break; /* GL_MAP_STENCIL */
    default:     s->lastError = GLL_INVALID_ENUM; break;
    }
}

/*
 * Apply the pixel transfer scale/bias and, when GL_MAP_COLOR is on, the
 * colour lookup maps to an RGBA image in place.  Used by the readback and
 * upload paths so glPixelTransfer actually affects pixels.
 */
void _glsApplyPixelTransferRGBA(unsigned char *rgba, int count)
{
    GLS_State *s = glsGetState();
    int i;

    if (!rgba || count <= 0) return;

    if (s->redScale == 1.0f && s->greenScale == 1.0f && s->blueScale == 1.0f &&
        s->alphaScale == 1.0f && s->redBias == 0.0f && s->greenBias == 0.0f &&
        s->blueBias == 0.0f && s->alphaBias == 0.0f && !s->mapColorFlag)
        return;   /* identity - nothing to do */

    for (i = 0; i < count; i++) {
        unsigned char *p = rgba + (size_t)i * 4;
        float c[4];
        int k;

        c[0] = (float)p[0] / 255.0f * s->redScale   + s->redBias;
        c[1] = (float)p[1] / 255.0f * s->greenScale + s->greenBias;
        c[2] = (float)p[2] / 255.0f * s->blueScale  + s->blueBias;
        c[3] = (float)p[3] / 255.0f * s->alphaScale + s->alphaBias;

        if (s->mapColorFlag) {
            static const int mapFor[4] = { 6, 7, 8, 9 };  /* R_TO_R, G_TO_G, B_TO_B, A_TO_A */
            for (k = 0; k < 4; k++) {
                int mi = mapFor[k], n = s->pixelMapSize[mi];
                if (n > 1) {
                    float t = c[k] < 0.0f ? 0.0f : (c[k] > 1.0f ? 1.0f : c[k]);
                    int   j = (int)(t * (float)(n - 1) + 0.5f);
                    c[k] = s->pixelMap[mi][j];
                }
            }
        }

        for (k = 0; k < 4; k++) {
            int v = (int)(c[k] * 255.0f + 0.5f);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            p[k] = (unsigned char)v;
        }
    }
}

/* ===================================================================
 *  Accumulation buffer
 * =================================================================== */

void _glsClearAccum(float r, float g, float b, float a)
{
    GLS_State *s = glsGetState();
    s->accumClear[0] = r; s->accumClear[1] = g;
    s->accumClear[2] = b; s->accumClear[3] = a;
}

/*
 * The accumulation buffer is kept in system memory as float RGBA.
 *
 * D3D9 has no accumulation buffer, so GL_ACCUM/GL_LOAD read the colour buffer
 * back, and GL_RETURN pushes the result out through the same path glDrawPixels
 * uses.  This is slow by construction - it is a full-frame readback per
 * operation - but it is correct, and applications that use accumulation are
 * doing multi-pass effects where correctness is the point.
 */
void _glsAccum(unsigned int op, float value)
{
    GLS_State *s = glsGetState();
    int w = s->viewportW, h = s->viewportH;
    int n = w * h, i;
    unsigned char *pixels;

    if (w <= 0 || h <= 0) return;

    if (!s->accumBuffer || s->accumWidth != w || s->accumHeight != h) {
        free(s->accumBuffer);
        s->accumBuffer = (float *)calloc((size_t)n * 4, sizeof(float));
        s->accumWidth  = w;
        s->accumHeight = h;
        if (!s->accumBuffer) {
            gldDiagLog("GL: Accum could not allocate %dx%d accumulation buffer", w, h);
            return;
        }
    }

    if (op == GLL_MULT) {
        for (i = 0; i < n * 4; i++) s->accumBuffer[i] *= value;
        return;
    }

    pixels = (unsigned char *)malloc((size_t)n * 4);
    if (!pixels) return;

    if (op == GLL_ACCUM || op == GLL_LOAD) {
        _glsReadPixels(0, 0, w, h, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixels);
        if (op == GLL_LOAD)
            for (i = 0; i < n * 4; i++)
                s->accumBuffer[i] = (float)pixels[i] / 255.0f * value;
        else
            for (i = 0; i < n * 4; i++)
                s->accumBuffer[i] += (float)pixels[i] / 255.0f * value;
        free(pixels);
        return;
    }

    if (op == GLL_RETURN) {
        for (i = 0; i < n * 4; i++) {
            int v = (int)(s->accumBuffer[i] * value * 255.0f + 0.5f);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            pixels[i] = (unsigned char)v;
        }
        _glLegacyBlitRGBA(0.0f, 0.0f, w, h, pixels);
        free(pixels);
        return;
    }

    free(pixels);
    s->lastError = GLL_INVALID_ENUM;
}

/* ===================================================================
 *  Double-precision and integer entry points
 *
 *  GL defines these as the same operation on a wider type.  They convert
 *  and delegate rather than duplicating the logic.
 * =================================================================== */

void _glsMap1d(unsigned int target, double u1, double u2, int stride, int order,
               const double *points)
{
    float tmp[GLS_MAX_EVAL_ORDER * 4];
    int comps, idx = _glLegacyMap1Index((GLenum_t)target, &comps);
    int i, c;

    if (idx < 0 || order < 1 || order > GLS_MAX_EVAL_ORDER || !points) {
        glsGetState()->lastError = (idx < 0) ? GLL_INVALID_ENUM : GLL_INVALID_VALUE;
        return;
    }
    for (i = 0; i < order; i++)
        for (c = 0; c < comps; c++)
            tmp[i * comps + c] = (float)points[i * stride + c];

    _glsMap1f(target, (float)u1, (float)u2, comps, order, tmp);
}

void _glsMap2d(unsigned int target, double u1, double u2, int ustride, int uorder,
               double v1, double v2, int vstride, int vorder, const double *points)
{
    static float tmp[GLS_MAX_EVAL_ORDER * GLS_MAX_EVAL_ORDER * 4];
    int comps, idx = _glLegacyMap2Index((GLenum_t)target, &comps);
    int i, j, c;

    if (idx < 0 || uorder < 1 || uorder > GLS_MAX_EVAL_ORDER ||
        vorder < 1 || vorder > GLS_MAX_EVAL_ORDER || !points) {
        glsGetState()->lastError = (idx < 0) ? GLL_INVALID_ENUM : GLL_INVALID_VALUE;
        return;
    }
    for (i = 0; i < uorder; i++)
        for (j = 0; j < vorder; j++)
            for (c = 0; c < comps; c++)
                tmp[(i * vorder + j) * comps + c] =
                    (float)points[i * ustride + j * vstride + c];

    _glsMap2f(target, (float)u1, (float)u2, vorder * comps, uorder,
                      (float)v1, (float)v2, comps,          vorder, tmp);
}

void _glsGetMapiv(unsigned int target, unsigned int query, int *v)
{
    float tmp[GLS_MAX_EVAL_ORDER * GLS_MAX_EVAL_ORDER * 4];
    int i, n;

    if (!v) return;
    memset(tmp, 0, sizeof(tmp));
    _glsGetMapfv(target, query, tmp);

    /* GL_ORDER and GL_DOMAIN return at most 4 values; GL_COEFF returns the
     * whole control mesh, whose size the caller already knows. */
    n = (query == 0x0A00) ? (GLS_MAX_EVAL_ORDER * GLS_MAX_EVAL_ORDER * 4) : 4;
    for (i = 0; i < n; i++) v[i] = (int)tmp[i];
}

/* GL scales unsigned pixel-map entries into [0,1] for the colour maps; the
 * index and stencil maps carry integers through unchanged. */
static BOOL _glLegacyPixelMapIsIndex(unsigned int map)
{
    return (map == 0x0C70 || map == 0x0C71);    /* I_TO_I, S_TO_S */
}

void _glsPixelMapuiv(unsigned int map, int mapsize, const unsigned int *values)
{
    float tmp[GLS_MAX_PIXEL_MAP];
    int i;

    if (!values || mapsize < 1 || mapsize > GLS_MAX_PIXEL_MAP) {
        glsGetState()->lastError = GLL_INVALID_VALUE;
        return;
    }
    for (i = 0; i < mapsize; i++)
        tmp[i] = _glLegacyPixelMapIsIndex(map) ? (float)values[i]
                                               : (float)values[i] / 4294967295.0f;
    _glsPixelMapfv(map, mapsize, tmp);
}

void _glsPixelMapusv(unsigned int map, int mapsize, const unsigned short *values)
{
    float tmp[GLS_MAX_PIXEL_MAP];
    int i;

    if (!values || mapsize < 1 || mapsize > GLS_MAX_PIXEL_MAP) {
        glsGetState()->lastError = GLL_INVALID_VALUE;
        return;
    }
    for (i = 0; i < mapsize; i++)
        tmp[i] = _glLegacyPixelMapIsIndex(map) ? (float)values[i]
                                               : (float)values[i] / 65535.0f;
    _glsPixelMapfv(map, mapsize, tmp);
}

void _glsGetPixelMapuiv(unsigned int map, unsigned int *values)
{
    float tmp[GLS_MAX_PIXEL_MAP];
    GLS_State *s = glsGetState();
    int idx = _glLegacyPixelMapIndex((GLenum_t)map), i;

    if (idx < 0) { glsGetState()->lastError = GLL_INVALID_ENUM; return; }
    if (!values) return;
    _glsGetPixelMapfv(map, tmp);
    for (i = 0; i < s->pixelMapSize[idx]; i++)
        values[i] = _glLegacyPixelMapIsIndex(map)
                  ? (unsigned int)tmp[i]
                  : (unsigned int)(tmp[i] * 4294967295.0);
}

void _glsGetPixelMapusv(unsigned int map, unsigned short *values)
{
    float tmp[GLS_MAX_PIXEL_MAP];
    GLS_State *s = glsGetState();
    int idx = _glLegacyPixelMapIndex((GLenum_t)map), i;

    if (idx < 0) { s->lastError = GLL_INVALID_ENUM; return; }
    if (!values) return;
    _glsGetPixelMapfv(map, tmp);
    for (i = 0; i < s->pixelMapSize[idx]; i++)
        values[i] = _glLegacyPixelMapIsIndex(map)
                  ? (unsigned short)tmp[i]
                  : (unsigned short)(tmp[i] * 65535.0f);
}

void _glsPixelTransferi(unsigned int pname, int param)
{
    _glsPixelTransferf(pname, (float)param);
}

/* glPrioritizeTextures is advisory; D3D9 exposes the same idea through
 * SetPriority on a managed resource, which is where the hint belongs. */
void _glsPrioritizeTextures(int n, const unsigned int *textures, const float *priorities)
{
    int i;

    if (!textures || !priorities || n <= 0) return;

    for (i = 0; i < n; i++) {
        GLS_Texture *t = glsFindTexture(textures[i]);
        DWORD pri;
        if (!t) continue;
        pri = (DWORD)(priorities[i] * 65535.0f);
        if (t->pTex)     IDirect3DTexture9_SetPriority(t->pTex, pri);
        if (t->pCubeTex) IDirect3DCubeTexture9_SetPriority(t->pCubeTex, pri);
    }
}

/* ===================================================================
 *  Synchronisation, queries and the remaining EXT entry points
 * =================================================================== */

/*
 * glFinish must not return until every issued command has completed.
 * A D3DQUERYTYPE_EVENT signals exactly that, so this waits on one.
 */
void _glsFinish(void)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DQuery9 *pq = NULL;
    int spins = 0;

    if (!pDev) return;

    /* Wrapped for the same reason the texture and format-probe paths are: this
     * calls into a d3d9.dll the wrapper does not own, on a device pointer read
     * from a process-global that an application is free to race.  id Tech 4
     * runs its backend on a second thread once SMP is on, and this was seen
     * faulting on the vtable load below - the device pointer was non-NULL but
     * no longer a valid object.
     *
     * A glFinish that cannot reach the device has nothing to wait for, so
     * returning without waiting is the correct degraded behaviour; a fault
     * here would take the process down instead. The pointer is logged once so
     * a stale-but-plausible value reads differently from obvious garbage. */
    __try {
        if (FAILED(IDirect3DDevice9_CreateQuery(pDev, D3DQUERYTYPE_EVENT, &pq)) || !pq)
            return;

        IDirect3DQuery9_Issue(pq, D3DISSUE_END);
        for (;;) {
            HRESULT hr = IDirect3DQuery9_GetData(pq, NULL, 0, D3DGETDATA_FLUSH);
            if (hr != S_FALSE) break;              /* S_OK, or the device is gone */
            if (++spins > 1000000) {
                gldDiagLog("GL: Finish timed out waiting for the GPU");
                break;
            }
        }
        IDirect3DQuery9_Release(pq);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        static BOOL warned = FALSE;
        if (!warned) {
            warned = TRUE;
            gldDiagLog("GL: Finish faulted reaching the device (pDev=%p, thread %lu) - "
                       "the pointer was published but is not a live object. Skipping "
                       "the wait rather than taking the process down.",
                       (void *)pDev, GetCurrentThreadId());
        }
    }
}

/*
 * glFlush only guarantees commands are submitted, not completed.  Issuing an
 * event query with D3DGETDATA_FLUSH pushes the command buffer without the
 * wait that glFinish performs.
 */
void _glsFlush(void)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    IDirect3DQuery9 *pq = NULL;

    if (!pDev) return;
    if (FAILED(IDirect3DDevice9_CreateQuery(pDev, D3DQUERYTYPE_EVENT, &pq)) || !pq)
        return;

    IDirect3DQuery9_Issue(pq, D3DISSUE_END);
    IDirect3DQuery9_GetData(pq, NULL, 0, D3DGETDATA_FLUSH);
    IDirect3DQuery9_Release(pq);
}

unsigned char _glsIsTexture(unsigned int texture)
{
    GLS_Texture *t;
    if (texture == 0) return GLL_FALSE;
    t = glsFindTexture(texture);
    return (t && t->allocated) ? GLL_TRUE : GLL_FALSE;
}

void _glsGetPointerv(unsigned int pname, void **params)
{
    GLS_State *s = glsGetState();
    int unit;

    if (!params) return;
    *params = NULL;

    switch (pname) {
    case 0x808E: *params = (void *)s->clientVertexArray.pointer; break; /* VERTEX   */
    case 0x808F: *params = (void *)s->clientNormalArray.pointer; break; /* NORMAL   */
    case 0x8090: *params = (void *)s->clientColorArray.pointer;  break; /* COLOR    */
    case 0x8092:                                                        /* TEXCOORD */
        unit = (s->clientActiveTexUnit >= GLL_TEXTURE0)
             ? (int)(s->clientActiveTexUnit - GLL_TEXTURE0) : 0;
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        *params = (void *)s->clientTexCoordArray[unit].pointer;
        break;
    case 0x8091:  /* GL_INDEX_ARRAY_POINTER     */
    case 0x8093:  /* GL_EDGE_FLAG_ARRAY_POINTER */
        /* Neither array is consumed by the draw path, so no pointer is held. */
        break;
    default:
        s->lastError = GLL_INVALID_ENUM;
        break;
    }
}

void _glsEdgeFlagPointer(int stride, const void *pointer)
{
    static BOOL warned = FALSE;
    (void)stride; (void)pointer;

    if (!warned) {
        warned = TRUE;
        gldFlagFault("feature", "edge-flag-array");
        gldDiagLog("GL: EdgeFlagPointer - D3D9 has no per-vertex edge flag; "
                   "the array is ignored and all polygon edges are drawn");
    }
}

/*
 * Paletted textures (EXT_paletted_texture).
 *
 * D3D9 does define D3DFMT_P8, but support is effectively absent on modern
 * drivers and translation layers, and the wrapper has no path that would bind
 * a palette.  Rather than pretend, these record nothing and say so once - an
 * application that checks for the extension string will not find it
 * advertised.
 */
static void _glLegacyNoPalette(const char *fn)
{
    static BOOL warned = FALSE;
    if (!warned) {
        warned = TRUE;
        gldDiagLog("GL: %s - paletted textures are not supported; the palette "
                   "is ignored", fn);
        gldFlagFault("feature", "paletted-texture");
    }
}

void _glsColorTableEXT(unsigned int target, unsigned int internalformat, int width,
                       unsigned int format, unsigned int type, const void *table)
{
    (void)target; (void)internalformat; (void)width;
    (void)format; (void)type; (void)table;
    _glLegacyNoPalette("ColorTableEXT");
}

void _glsColorSubTableEXT(unsigned int target, int start, int count,
                          unsigned int format, unsigned int type, const void *data)
{
    (void)target; (void)start; (void)count;
    (void)format; (void)type; (void)data;
    _glLegacyNoPalette("ColorSubTableEXT");
}

void _glsGetColorTableEXT(unsigned int target, unsigned int format,
                          unsigned int type, void *data)
{
    (void)target; (void)format; (void)type; (void)data;
    _glLegacyNoPalette("GetColorTableEXT");
}

void _glsGetColorTableParameterivEXT(unsigned int target, unsigned int pname, int *params)
{
    (void)target;
    if (!params) return;
    /* Report an empty table rather than leaving the caller's memory untouched. */
    *params = 0;
    (void)pname;
    _glLegacyNoPalette("GetColorTableParameterEXT");
}

void _glsGetColorTableParameterfvEXT(unsigned int target, unsigned int pname, float *params)
{
    (void)target;
    if (!params) return;
    *params = 0.0f;
    (void)pname;
    _glLegacyNoPalette("GetColorTableParameterEXT");
}

/*
 * glResizeBuffersMESA tells the driver the window changed size.  The D3D9
 * swap chain is recreated from the window rect, so the honest response is to
 * note it; the drawable is rebuilt by the context path on the next present.
 */
void _glsResizeBuffersMESA(void)
{
    gldDiagLogV("GL: ResizeBuffersMESA - swap chain is resized by the context "
               "path on the next present");
}

/* ===================================================================
 *  Secondary colour, multisample coverage, conditional render
 * =================================================================== */

/*
 * EXT_secondary_color adds a second interpolated colour that is added after
 * texturing.  That is exactly D3D9's specular vertex channel, so the value
 * rides in GLS_D3DVertex::specular and D3DRS_SPECULARENABLE turns the add on.
 */
void _glsSecondaryColor3f(float r, float g, float b)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    s->secondaryColor[0] = r;
    s->secondaryColor[1] = g;
    s->secondaryColor[2] = b;

    if (!s->secondaryColorUsed) {
        s->secondaryColorUsed = GLL_TRUE;
        if (pDev)
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARENABLE, TRUE);
    }
}

void _glsSecondaryColorPointer(int size, unsigned int type, int stride, const void *pointer)
{
    static BOOL warned = FALSE;
    (void)size; (void)type; (void)stride; (void)pointer;

    /* The vertex assembler reads a fixed set of client arrays; a secondary
     * colour array is not among them, so per-vertex specular from an array
     * would silently be the current value instead. */
    if (!warned) {
        warned = TRUE;
        gldFlagFault("feature", "secondary-color-array");
        gldDiagLogV("GL: SecondaryColorPointer - the secondary colour array is "
                   "not assembled; glSecondaryColor's current value is used");
    }
}

void _glsSampleCoverage(float value, unsigned char invert)
{
    GLS_State *s = glsGetState();
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();
    DWORD mask;
    int bits, i;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    s->sampleCoverageValue  = value;
    s->sampleCoverageInvert = invert ? GLL_TRUE : GLL_FALSE;

    if (!pDev) return;

    /* D3D9 expresses coverage as a sample bitmask rather than a fraction, so
     * the fraction is quantised to however many of the 32 mask bits it
     * covers.  With no multisampling the mask is ignored by the device. */
    bits = (int)(value * 32.0f + 0.5f);
    mask = 0;
    for (i = 0; i < bits; i++) mask |= (1u << i);
    if (invert) mask = ~mask;

    IDirect3DDevice9_SetRenderState(pDev, D3DRS_MULTISAMPLEMASK, mask);
}

void _glsSampleMaski(unsigned int index, unsigned int mask)
{
    IDirect3DDevice9 *pDev = gldGetD3DDevice46();

    /* D3D9 has a single 32-bit sample mask, so only word 0 is representable. */
    if (index != 0) {
        gldFlagFault("feature", "sample-mask-word");
        gldDiagLog("GL: SampleMaski word %u ignored - D3D9 has one 32-bit "
                   "sample mask", index);
        return;
    }
    if (pDev)
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_MULTISAMPLEMASK, (DWORD)mask);
}

/*
 * Conditional rendering skips draws when a previous occlusion query saw no
 * samples.  The query result is already available through the occlusion path,
 * so the decision is made once here and the draw path honours the flag.
 */
void _glsBeginConditionalRender(unsigned int id, unsigned int mode)
{
    GLS_State *s = glsGetState();
    unsigned int passed = 1;

    (void)mode;   /* WAIT vs NO_WAIT: the query is read synchronously either way */

    _glsGetQueryObjectuiv(id, 0x8866 /* GL_QUERY_RESULT */, &passed);
    s->conditionalRenderSkip = (passed == 0) ? GLL_TRUE : GLL_FALSE;

    if (s->conditionalRenderSkip)
        gldDiagLog("GL: BeginConditionalRender query %u saw no samples - "
                   "draws are skipped until EndConditionalRender", id);
}

void _glsEndConditionalRender(void)
{
    glsGetState()->conditionalRenderSkip = GLL_FALSE;
}
