/*********************************************************************************
*
*  ===============================================================================
*  |                  GLDirect: Direct3D Device Driver for Mesa.                 |
*  |                                                                             |
*  |                Copyright (C) 1997-2007 SciTech Software, Inc.               |
*  |                                                                             |
*  |Permission is hereby granted, free of charge, to any person obtaining a copy |
*  |of this software and associated documentation files (the "Software"), to deal|
*  |in the Software without restriction, including without limitation the rights |
*  |to use, copy, modify, merge, publish, distribute, sublicense, and/or sell    |
*  |copies of the Software, and to permit persons to whom the Software is        |
*  |furnished to do so, subject to the following conditions:                     |
*  |                                                                             |
*  |The above copyright notice and this permission notice shall be included in   |
*  |all copies or substantial portions of the Software.                          |
*  |                                                                             |
*  |THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   |
*  |IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     |
*  |FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  |
*  |AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       |
*  |LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,|
*  |OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN    |
*  |THE SOFTWARE.                                                                |
*  ===============================================================================
*
*  ===============================================================================
*  |        Original Author: Keith Harrison <sio2@users.sourceforge.net>         |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x/2000/XP/XBox (Win32)
*
* Description:  Texture / Bitmap functions
*
*********************************************************************************/

#include "gld_context.h"
#include "gld_log.h"
#include "gldirect5.h"
#include "mesa_compat.h"

/* D3DX removed — all texture ops use pure D3D9 API */

// TODO: Mesa includes removed - replaced by mesa_compat.h shim
// #include "texformat.h"
// #include "colormac.h"
// #include "texstore.h"
// #include "image.h"
// #include "mem.h"

//---------------------------------------------------------------------------

#define GLD_FLIP_HEIGHT(y,h) (gldCtx->dwHeight - (y) - (h))

// ** FOR DEBUGGING **
// Spits all textures out to disk for inspection as they are specified.
#ifdef DEBUG
#if 0

#define GLD_SAVE_TEXTURE_TO_FILE
#define GLD_SAVE_TEXTURE_EXT		"dds"
/* D3DXIFF_DDS removed — D3DX no longer available. Texture save is disabled. */
#define GLD_SAVE_TEXTURE_FORMAT		0

#endif
#endif

//---------------------------------------------------------------------------
// Utils
//---------------------------------------------------------------------------

#ifdef GLD_SAVE_TEXTURE_TO_FILE
static void _gldSaveTextureToFile(
	GLuint Name,
	IDirect3DTexture9* pTex)
{
	// D3DXSaveSurfaceToFile removed — this was debug-only functionality.
	// Texture saving requires D3DX which we no longer link against.
	// If texture debugging is needed, use a GPU debugger (PIX, RenderDoc, etc.)
	(void)Name;
	(void)pTex;
	gldLogMessage(GLDLOG_WARN, "GLD_SAVE_TEXTURE_TO_FILE: D3DX removed, texture save disabled\n");
}
#endif

//---------------------------------------------------------------------------
// 1D texture fetch
//---------------------------------------------------------------------------

#define CHAN_SRC( t, i, j, k, sz )					\
	((GLchan *)(t)->Data + (i) * (sz))
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + (i) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + (i))
#define FLOAT_SRC( t, i, j, k )						\
	((GLfloat *)(t)->Data + (i))

//---------------------------------------------------------------------------

static void gld_fetch_1d_texel_X8R8G8B8(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 4 );
   GLchan *rgba = (GLchan *) texel;
   rgba[RCOMP] = src[2];
   rgba[GCOMP] = src[1];
   rgba[BCOMP] = src[0];
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_1d_texel_X1R5G5B5(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >> 10) & 0xf8) * 255 / 0xf8 );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  5) & 0xf8) * 255 / 0xf8 );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf8) * 255 / 0xf8 );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_1d_texel_X4R4G4B4(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >>  8) & 0xf) * 255 / 0xf );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  4) & 0xf) * 255 / 0xf );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf) * 255 / 0xf );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

#undef CHAN_SRC
#undef UBYTE_SRC
#undef USHORT_SRC
#undef FLOAT_SRC

//---------------------------------------------------------------------------
// 2D texture fetch
//---------------------------------------------------------------------------

#define CHAN_SRC( t, i, j, k, sz )					\
	((GLchan *)(t)->Data + ((t)->Width * (j) + (i)) * (sz))
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + ((t)->Width * (j) + (i)) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + ((t)->Width * (j) + (i)))
#define FLOAT_SRC( t, i, j, k )						\
	((GLfloat *)(t)->Data + ((t)->Width * (j) + (i)))

//---------------------------------------------------------------------------

static void gld_fetch_2d_texel_X8R8G8B8(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 4 );
   GLchan *rgba = (GLchan *) texel;
   rgba[RCOMP] = src[2];
   rgba[GCOMP] = src[1];
   rgba[BCOMP] = src[0];
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_2d_texel_X1R5G5B5(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >> 10) & 0xf8) * 255 / 0xf8 );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  5) & 0xf8) * 255 / 0xf8 );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf8) * 255 / 0xf8 );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_2d_texel_X4R4G4B4(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >>  8) & 0xf) * 255 / 0xf );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  4) & 0xf) * 255 / 0xf );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf) * 255 / 0xf );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

#undef CHAN_SRC
#undef UBYTE_SRC
#undef USHORT_SRC
#undef FLOAT_SRC

//---------------------------------------------------------------------------
// 3D texture fetch
//---------------------------------------------------------------------------

#define CHAN_SRC( t, i, j, k, sz )					\
	(GLchan *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				(t)->Width + (i)) * (sz)
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				 (t)->Width + (i)) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->Width + (i)))
#define FLOAT_SRC( t, i, j, k )						\
	((GLfloat *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->Width + (i)))

//---------------------------------------------------------------------------

static void gld_fetch_3d_texel_X8R8G8B8(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 4 );
   GLchan *rgba = (GLchan *) texel;
   rgba[RCOMP] = src[2];
   rgba[GCOMP] = src[1];
   rgba[BCOMP] = src[0];
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_3d_texel_X1R5G5B5(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >> 10) & 0xf8) * 255 / 0xf8 );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  5) & 0xf8) * 255 / 0xf8 );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf8) * 255 / 0xf8 );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

static void gld_fetch_3d_texel_X4R4G4B4(
	const struct gl_texture_image *texImage,
	GLint i, GLint j, GLint k, GLvoid *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   GLchan *rgba = (GLchan *) texel; GLushort s = *src;
   rgba[RCOMP] = UBYTE_TO_CHAN( ((s >>  8) & 0xf) * 255 / 0xf );
   rgba[GCOMP] = UBYTE_TO_CHAN( ((s >>  4) & 0xf) * 255 / 0xf );
   rgba[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf) * 255 / 0xf );
   rgba[ACOMP] = CHAN_MAX;
}

//---------------------------------------------------------------------------

#undef CHAN_SRC
#undef UBYTE_SRC
#undef USHORT_SRC
#undef FLOAT_SRC

//---------------------------------------------------------------------------
// Direct3D texture formats that have no Mesa equivalent
//---------------------------------------------------------------------------

const struct gl_texture_format _gld_texformat_X8R8G8B8 = {
   MESA_FORMAT_ARGB8888,		/* MesaFormat */
   GL_RGBA,				/* BaseFormat */
//   GL_UNSIGNED_BYTE,	/* Type */
   8,					/* RedBits */
   8,					/* GreenBits */
   8,					/* BlueBits */
   0,					/* AlphaBits */
   0,					/* LuminanceBits */
   0,					/* IntensityBits */
   0,					/* IndexBits */
   0,					/* DepthBits */
   4,					/* TexelBytes */
   gld_fetch_1d_texel_X8R8G8B8,		/* FetchTexel1D */
   gld_fetch_2d_texel_X8R8G8B8,		/* FetchTexel2D */
   gld_fetch_3d_texel_X8R8G8B8,		/* FetchTexel3D */
};

const struct gl_texture_format _gld_texformat_X1R5G5B5 = {
   MESA_FORMAT_ARGB1555,		/* MesaFormat */
   GL_RGBA,				/* BaseFormat */
//   GL_UNSIGNED_SHORT_1_5_5_5_REV,	/* Type */
   5,					/* RedBits */
   5,					/* GreenBits */
   5,					/* BlueBits */
   0,					/* AlphaBits */
   0,					/* LuminanceBits */
   0,					/* IntensityBits */
   0,					/* IndexBits */
   0,					/* DepthBits */
   2,					/* TexelBytes */
   gld_fetch_1d_texel_X1R5G5B5,		/* FetchTexel1D */
   gld_fetch_2d_texel_X1R5G5B5,		/* FetchTexel2D */
   gld_fetch_3d_texel_X1R5G5B5,		/* FetchTexel3D */
};

const struct gl_texture_format _gld_texformat_X4R4G4B4 = {
   MESA_FORMAT_ARGB4444,		/* MesaFormat */
   GL_RGBA,				/* BaseFormat */
//   GL_UNSIGNED_SHORT_4_4_4_4_REV,	/* Type */
   4,					/* RedBits */
   4,					/* GreenBits */
   4,					/* BlueBits */
   0,					/* AlphaBits */
   0,					/* LuminanceBits */
   0,					/* IntensityBits */
   0,					/* IndexBits */
   0,					/* DepthBits */
   2,					/* TexelBytes */
   gld_fetch_1d_texel_X4R4G4B4,		/* FetchTexel1D */
   gld_fetch_2d_texel_X4R4G4B4,		/* FetchTexel2D */
   gld_fetch_3d_texel_X4R4G4B4,		/* FetchTexel3D */
};

//---------------------------------------------------------------------------
// Texture unit constants
//---------------------------------------------------------------------------

// List of possible combinations of texture environments.
// Example: GLD_TEXENV_MODULATE_RGBA means 
//          GL_MODULATE, GL_RGBA base internal format.
#define GLD_TEXENV_DECAL_RGB		0
#define GLD_TEXENV_DECAL_RGBA		1
#define GLD_TEXENV_DECAL_ALPHA		2
#define GLD_TEXENV_REPLACE_RGB		3
#define GLD_TEXENV_REPLACE_RGBA		4
#define GLD_TEXENV_REPLACE_ALPHA	5
#define GLD_TEXENV_MODULATE_RGB		6
#define GLD_TEXENV_MODULATE_RGBA	7
#define GLD_TEXENV_MODULATE_ALPHA	8
#define GLD_TEXENV_BLEND_RGB		9
#define GLD_TEXENV_BLEND_RGBA		10
#define GLD_TEXENV_BLEND_ALPHA		11
#define GLD_TEXENV_ADD_RGB			12
#define GLD_TEXENV_ADD_RGBA			13
#define GLD_TEXENV_ADD_ALPHA		14

// Per-stage (i.e. per-unit) texture environment
typedef struct {
	DWORD			ColorArg1;	// Colour argument 1
	D3DTEXTUREOP	ColorOp;	// Colour operation
	DWORD			ColorArg2;	// Colour argument 2
	DWORD			AlphaArg1;	// Alpha argument 1
	D3DTEXTUREOP	AlphaOp;	// Alpha operation
	DWORD			AlphaArg2;	// Alpha argument 2
} GLD_texenv;

// TODO: Do we really need to set ARG1 and ARG2 every time?
//       They seem to always be TEXTURE and CURRENT respectively.

// C = Colour out
// A = Alpha out
// Ct = Colour from Texture
// Cf = Colour from fragment (diffuse)
// At = Alpha from Texture
// Af = Alpha from fragment (diffuse)
// Cc = GL_TEXTURE_ENV_COLOUR (GL_BLEND)
const GLD_texenv gldTexEnv[] = {
	// DECAL_RGB: C=Ct, A=Af
	{D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// DECAL_RGBA: C=Cf(1-At)+CtAt, A=Af
	{D3DTA_TEXTURE, D3DTOP_BLENDTEXTUREALPHA, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// DECAL_ALPHA: <undefined> use DECAL_RGB
	{D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},

	// REPLACE_RGB: C=Ct, A=Af
	{D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// REPLACE_RGBA: C=Ct, A=At
	{D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT},
	// REPLACE_ALPHA: C=Cf, A=At
	{D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT},

	// MODULATE_RGB: C=CfCt, A=Af
	{D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// MODULATE_RGBA: C=CfCt, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},
	// MODULATE_ALPHA: C=Cf, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},

	// BLEND_RGB: C=Cf(1-Ct)+CcCt, A=Af
	{D3DTA_TEXTURE, D3DTOP_LERP, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// BLEND_RGBA: C=Cf(1-Ct)+CcCt, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_LERP, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},
	// BLEND_ALPHA: C=Cf, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},

	// ADD_RGB: C=Cf+Ct, A=Af
	{D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT},
	// ADD_RGBA: C=Cf+Ct, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},
	// ADD_ALPHA: C=Cf, A=AfAt
	{D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_CURRENT,
	D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT},
};

//---------------------------------------------------------------------------

D3DTEXTUREADDRESS _gldConvertWrap(
	GLenum wrap)
{
	return (wrap == GL_CLAMP) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
}

//---------------------------------------------------------------------------

D3DTEXTUREFILTERTYPE _gldConvertMagFilter(
	GLenum magfilter)
{
	return (magfilter == GL_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
}

//---------------------------------------------------------------------------

void _gldConvertMinFilter(
	GLenum minfilter,
	D3DTEXTUREFILTERTYPE *min_filter,
	D3DTEXTUREFILTERTYPE *mip_filter)
{
	switch (minfilter) {
	case GL_NEAREST:
		*min_filter = D3DTEXF_POINT;
		*mip_filter = D3DTEXF_NONE;
		break;
	case GL_LINEAR:
		*min_filter = D3DTEXF_LINEAR;
		*mip_filter = D3DTEXF_NONE;
		break;
	case GL_NEAREST_MIPMAP_NEAREST:
		*min_filter = D3DTEXF_POINT;
		*mip_filter = D3DTEXF_POINT;
		break;
	case GL_LINEAR_MIPMAP_NEAREST:
		*min_filter = D3DTEXF_LINEAR;
		*mip_filter = D3DTEXF_POINT;
		break;
	case GL_NEAREST_MIPMAP_LINEAR:
		*min_filter = D3DTEXF_POINT;
		*mip_filter = D3DTEXF_LINEAR;
		break;
	case GL_LINEAR_MIPMAP_LINEAR:
		*min_filter = D3DTEXF_LINEAR;
		*mip_filter = D3DTEXF_LINEAR;
		break;
	}
}

//---------------------------------------------------------------------------

D3DFORMAT _gldGLFormatToD3DFormat(
	GLenum internalFormat)
{
	switch (internalFormat) {
	case GL_INTENSITY:
	case GL_INTENSITY4:
	case GL_INTENSITY8:
	case GL_INTENSITY12:
	case GL_INTENSITY16:
		// LUNIMANCE != INTENSITY, but D3D doesn't have I8 textures
		return D3DFMT_L8;
	case 1:
	case GL_LUMINANCE:
	case GL_LUMINANCE4:
	case GL_LUMINANCE8:
	case GL_LUMINANCE12:
	case GL_LUMINANCE16:
		return D3DFMT_L8;
	case GL_ALPHA:
	case GL_ALPHA4:
	case GL_ALPHA8:
	case GL_ALPHA12:
	case GL_ALPHA16:
		return D3DFMT_A8;
	case GL_COLOR_INDEX:
	case GL_COLOR_INDEX1_EXT:
	case GL_COLOR_INDEX2_EXT:
	case GL_COLOR_INDEX4_EXT:
	case GL_COLOR_INDEX8_EXT:
	case GL_COLOR_INDEX12_EXT:
	case GL_COLOR_INDEX16_EXT:
		return D3DFMT_X8R8G8B8;
	case 2:
	case GL_LUMINANCE_ALPHA:
	case GL_LUMINANCE4_ALPHA4:
	case GL_LUMINANCE6_ALPHA2:
	case GL_LUMINANCE8_ALPHA8:
	case GL_LUMINANCE12_ALPHA4:
	case GL_LUMINANCE12_ALPHA12:
	case GL_LUMINANCE16_ALPHA16:
		return D3DFMT_A8L8;
	case GL_R3_G3_B2:
		// TODO: Mesa does not support RGB332 internally
		return D3DFMT_X4R4G4B4; //D3DFMT_R3G3B2;
	case GL_RGB4:
		return D3DFMT_X4R4G4B4;
	case GL_RGB5:
		return D3DFMT_X1R5G5B5;
	case 3:
	case GL_RGB:
	case GL_RGB8:
	case GL_RGB10:
	case GL_RGB12:
	case GL_RGB16:
		return D3DFMT_R8G8B8;
	case GL_RGBA4:
		return D3DFMT_A4R4G4B4;
	case 4:
	case GL_RGBA:
	case GL_RGBA2:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_RGBA12:
	case GL_RGBA16:
		return D3DFMT_A8R8G8B8;
	case GL_RGB5_A1:
		return D3DFMT_A1R5G5B5;
	}

	// Return an acceptable default
	return D3DFMT_A8R8G8B8;
}

//---------------------------------------------------------------------------

GLenum _gldDecodeBaseFormat(
	IDirect3DTexture9 *pTex)
{
	// Examine Direct3D texture and return base OpenGL internal texture format
	// NOTE: We can't use any base format info from Mesa because D3D might have
	// used a different texture format when we used CreateTexture().

	// Base internal format is one of (Red Book p355):
	//	GL_ALPHA, 
	//	GL_LUMINANCE, 
	//	GL_LUMINANCE_ALPHA, 
	//	GL_INTENSITY, 
	//	GL_RGB, 
	//	GL_RGBA

	// NOTE: INTENSITY not used (not supported by Direct3D)
	//       LUMINANCE has same texture functions as RGB
	//       LUMINANCE_ALPHA has same texture functions as RGBA

	// TODO: cache format instead of using GetLevelDesc()
	D3DSURFACE_DESC desc;
	_GLD_DX9_TEX(GetLevelDesc(pTex, 0, &desc));

	switch (desc.Format) {
    case D3DFMT_R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_R3G3B2:
    case D3DFMT_X4R4G4B4:
    case D3DFMT_P8:
    case D3DFMT_L8:
		return GL_RGB;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_A8R3G3B2:
    case D3DFMT_A8P8:
    case D3DFMT_A8L8:
    case D3DFMT_A4L4:
		return GL_RGBA;
    case D3DFMT_A8:
		return GL_ALPHA;
	// Compressed texture formats. Need to check these...
    case D3DFMT_DXT1:
		return GL_RGBA;
    case D3DFMT_DXT2:
		return GL_RGB;
    case D3DFMT_DXT3:
		return GL_RGBA;
    case D3DFMT_DXT4:
		return GL_RGB;
    case D3DFMT_DXT5:
		return GL_RGBA;
	}

	// Fell through. Return arbitary default.
	return GL_RGBA;
}

//---------------------------------------------------------------------------

const struct gl_texture_format* _gldMesaFormatForD3DFormat(
	D3DFORMAT d3dfmt)
{
	switch (d3dfmt) {
	case D3DFMT_A8R8G8B8:
		return &_mesa_texformat_argb8888;
	case D3DFMT_R8G8B8:
		return &_mesa_texformat_rgb888;
	case D3DFMT_R5G6B5:
		return &_mesa_texformat_rgb565;
	case D3DFMT_A4R4G4B4:
		return &_mesa_texformat_argb4444;
	case D3DFMT_A1R5G5B5:
		return &_mesa_texformat_argb1555;
	case D3DFMT_A8L8:
		return &_mesa_texformat_al88;
	case D3DFMT_R3G3B2:
		return &_mesa_texformat_rgb332;
	case D3DFMT_A8:
		return &_mesa_texformat_a8;
	case D3DFMT_L8:
		return &_mesa_texformat_l8;
	case D3DFMT_X8R8G8B8:
		return &_gld_texformat_X8R8G8B8;
	case D3DFMT_X1R5G5B5:
		return &_gld_texformat_X1R5G5B5;
	case D3DFMT_X4R4G4B4:
		return &_gld_texformat_X4R4G4B4;
	}

	// If we reach here then the format is not directly supported.
	// Fall back to X8R8G8B8 which is universally supported by D3D9.
	return &_gld_texformat_X8R8G8B8;
}

//---------------------------------------------------------------------------
// Copy* functions
//---------------------------------------------------------------------------

void gldCopyTexImage1D_DX9(
	GLcontext *ctx,
	GLenum target, GLint level,
	GLenum internalFormat,
	GLint x, GLint y,
	GLsizei width, GLint border )
{
	// TODO
}

//---------------------------------------------------------------------------

void gldCopyTexImage2D_DX9(
	GLcontext *ctx,
	GLenum target,
	GLint level,
	GLenum internalFormat,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height,
	GLint border)
{
	// TODO
}

//---------------------------------------------------------------------------

void gldCopyTexSubImage1D_DX9(
	GLcontext *ctx,
	GLenum target, GLint level,
	GLint xoffset, GLint x, GLint y, GLsizei width )
{
	// TODO
}

//---------------------------------------------------------------------------

void gldCopyTexSubImage2D_DX9(
	GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height)
{
	// TODO
}

//---------------------------------------------------------------------------

void gldCopyTexSubImage3D_DX9(
	GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLint zoffset,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height )
{
	// TODO ?
}

//---------------------------------------------------------------------------
// Bitmap/Pixel functions
//---------------------------------------------------------------------------

#define GLD_FLIP_Y(y) (gldCtx->dwHeight - (y))

#define _GLD_FVF_IMAGE	(D3DFVF_XYZRHW | D3DFVF_TEX1)

typedef struct {
	FLOAT	x, y;		// 2D raster coords
	FLOAT	z;			// depth value
	FLOAT	rhw;		// reciprocal homogenous W (always 1.0f)
	FLOAT	tu, tv;		// texture coords
} _GLD_IMAGE_VERTEX;

//---------------------------------------------------------------------------

HRESULT _gldDrawPixels(
	GLcontext *ctx,
	BOOL bChromakey,	// Alpha test for glBitmap() images
	GLint x,			// GL x position
	GLint y,			// GL y position (needs flipping)
	GLsizei width,		// Width of input image
	GLsizei height,		// Height of input image
	IDirect3DSurface9 *pImage)
{
	//
	// Draw input image as texture implementing PixelZoom and clipping.
	// Any fragment operations currently enabled will be used.
	//

	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTexture;
	D3DSURFACE_DESC		d3dsd;
	IDirect3DSurface9	*pSurface;
	_GLD_IMAGE_VERTEX	v[4];
	HRESULT				hr;

	float				ZoomWidth, ZoomHeight;
	float				ScaleWidth, ScaleHeight;
	float				fPixelCenterY; // An adjustment for sampling textures from their pixel center

	// Create a texture to hold image — pure D3D9, no D3DX
	hr = IDirect3DDevice9_CreateTexture(
		gld->pDev,
		width, height,
		1, // miplevels
		0, // usage
		D3DFMT_A8R8G8B8, // format
		D3DPOOL_MANAGED, // pool
		&pTexture, NULL);
	if (FAILED(hr))
		return hr;

	hr = IDirect3DTexture9_GetSurfaceLevel(pTexture, 0, &pSurface);
	if (FAILED(hr)) {
		IDirect3DTexture9_Release(pTexture);
		return hr;
	}

	// Copy image into texture via StretchRect or UpdateSurface
	// Both surfaces are same format/size so UpdateSurface works
	hr = IDirect3DDevice9_StretchRect(gld->pDev, pImage, NULL, pSurface, NULL, D3DTEXF_POINT);
	if (FAILED(hr)) {
		// Fallback: lock both surfaces and memcpy
		D3DLOCKED_RECT lrSrc, lrDst;
		if (SUCCEEDED(IDirect3DSurface9_LockRect(pImage, &lrSrc, NULL, D3DLOCK_READONLY)) &&
		    SUCCEEDED(IDirect3DSurface9_LockRect(pSurface, &lrDst, NULL, 0))) {
			UINT row;
			UINT copyPitch = (lrSrc.Pitch < lrDst.Pitch) ? lrSrc.Pitch : lrDst.Pitch;
			for (row = 0; row < (UINT)height; row++) {
				memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
				       (BYTE*)lrSrc.pBits + row * lrSrc.Pitch, copyPitch);
			}
			IDirect3DSurface9_UnlockRect(pSurface);
			IDirect3DSurface9_UnlockRect(pImage);
			hr = D3D_OK;
		}
	}
	IDirect3DSurface9_Release(pSurface);
	if (FAILED(hr)) {
		IDirect3DTexture9_Release(pTexture);
		return hr;
	}

	//
	// Set up the quad like this (ascii-art ahead!)
	//
	// 3--2
	// |  |
	// 0--1
	//
	//

	// Set depth
	v[0].z = v[1].z = v[2].z = v[3].z = ctx->Current.RasterPos[2];
	// Set Reciprocal Homogenous W
	v[0].rhw = v[1].rhw = v[2].rhw = v[3].rhw = 1.0f;

	// Set texcoords
	// Examine texture size - if different to input width and height
	// then we'll need to munge the texcoords to fit.
	IDirect3DTexture9_GetLevelDesc(pTexture, 0, &d3dsd);
	ScaleWidth = (float)width / (float)d3dsd.Width;
	ScaleHeight = (float)height / (float)d3dsd.Height;
	fPixelCenterY = 1.0f / (float)d3dsd.Height;
	v[0].tu = 0.0f;			v[0].tv = 0.0f;
	v[1].tu = ScaleWidth;	v[1].tv = 0.0f;
	v[2].tu = ScaleWidth;	v[2].tv = ScaleHeight - fPixelCenterY;
	v[3].tu = 0.0f;			v[3].tv = ScaleHeight - fPixelCenterY;

	// Set raster positions
	ZoomWidth = (float)width * ctx->Pixel.ZoomX;
	ZoomHeight = (float)height * ctx->Pixel.ZoomY;

	v[0].x = x;				v[0].y = GLD_FLIP_Y(y);
	v[1].x = x+ZoomWidth;	v[1].y = GLD_FLIP_Y(y);
	v[2].x = x+ZoomWidth;	v[2].y = GLD_FLIP_Y(y+ZoomHeight);
	v[3].x = x;				v[3].y = GLD_FLIP_Y(y+ZoomHeight);

	// Draw image with full HW acceleration
	// NOTE: Be nice to use a State Block for all this state...
	IDirect3DDevice9_SetTexture(gld->pDev, 0, (IDirect3DBaseTexture9*)pTexture);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_CLIPPING, TRUE);

	IDirect3DDevice9_SetSamplerState(gld->pDev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(gld->pDev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(gld->pDev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(gld->pDev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(gld->pDev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	IDirect3DDevice9_SetTextureStageState(gld->pDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	IDirect3DDevice9_SetTextureStageState(gld->pDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	IDirect3DDevice9_SetTextureStageState(gld->pDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(gld->pDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(gld->pDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	IDirect3DDevice9_SetTextureStageState(gld->pDev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	// End current Effect
	gldEndEffect(gld, gld->iCurEffect);

	IDirect3DDevice9_SetVertexShader(gld->pDev, NULL);
	IDirect3DDevice9_SetPixelShader(gld->pDev, NULL);
	IDirect3DDevice9_SetFVF(gld->pDev, _GLD_FVF_IMAGE);

	//
	// Emulate Chromakey with an Alpha Test.
	// [Alpha Test is more widely supported anyway]
	//
	if (bChromakey) {
		// Switch on alpha testing
		IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_ALPHATESTENABLE, TRUE);
		// Fragment passes if alpha is greater than reference value
		IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_ALPHAFUNC, D3DCMP_GREATER);
		// Set alpha reference value between Bitmap alpha values of
		// zero (transparent) and one (opaque).
		IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_ALPHAREF, 0x7f);
	}

	IDirect3DDevice9_DrawPrimitiveUP(gld->pDev, D3DPT_TRIANGLEFAN, 2, &v, sizeof(_GLD_IMAGE_VERTEX));

	// Release texture
	IDirect3DDevice9_SetTexture(gld->pDev, 0, NULL);
	IDirect3DTexture9_Release(pTexture);

	// Reset state to before we messed it up
	FLUSH_VERTICES(ctx, _NEW_ALL);

	// Start the current Effect
	gldBeginEffect(gld, gld->iCurEffect);

	// Restore stream to before we messed it up.
	_GLD_DX9_DEV(SetStreamSource(gld->pDev, 0, gld->pVB, 0, GLD_4D_VERTEX_SIZE));
	_GLD_DX9_DEV(SetVertexDeclaration(gld->pDev, gld->pVertDecl));

	return S_OK;
}

//---------------------------------------------------------------------------

void gld_DrawPixels_DX9(
	GLcontext *ctx,
	GLint x, GLint y, GLsizei width, GLsizei height,
	GLenum format, GLenum type,
	const struct gl_pixelstore_attrib *unpack,
	const GLvoid *pixels )
{
	GLD_context			*gldCtx;
	GLD_driver_dx9		*gld;

	IDirect3DSurface9	*pImage;
	HRESULT				hr;
	D3DLOCKED_RECT		d3dLockedRect;

	// Mesa does not currently handle this format.
	if (format == GL_BGR)
		return;

	gldCtx	= GLD_GET_CONTEXT(ctx);
	gld		= GLD_GET_DX9_DRIVER(gldCtx);

	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
		gld->pDev, 
		width,
		height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SCRATCH,
		&pImage,
		NULL);
	if (FAILED(hr)) {
		return;
	}

	//
	// Use Mesa to fill in image
	//

	// Lock all of surface 
	hr = IDirect3DSurface9_LockRect(pImage, &d3dLockedRect, NULL, 0);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pImage);
		return;
	}

	// unpack image, apply transfer ops and store directly in texture
	_mesa_transfer_teximage(
		ctx,
		2,
		GL_RGBA,
		&_mesa_texformat_argb8888,
		d3dLockedRect.pBits,
		width, height, 1, 0, 0, 0,
		d3dLockedRect.Pitch,
		0, /* dstImageStride */
		format, type, pixels, unpack);

	IDirect3DSurface9_UnlockRect(pImage);

	_gldDrawPixels(ctx, FALSE, x, y, width, height, pImage);

	IDirect3DSurface9_Release(pImage);
}

//---------------------------------------------------------------------------

void gld_ReadPixels_DX9(
	GLcontext *ctx,
	GLint x, GLint y, GLsizei width, GLsizei height,
	GLenum format, GLenum type,
	const struct gl_pixelstore_attrib *pack,
	GLvoid *dest)
{
	GLD_context						*gldCtx;
	GLD_driver_dx9					*gld;

	IDirect3DSurface9				*pBackbuffer = NULL;
	IDirect3DSurface9				*pNativeImage = NULL;
	IDirect3DSurface9				*pCanonicalImage = NULL;

	D3DSURFACE_DESC					d3dsd;
	RECT							rcSrc; // Source rect
	POINT							ptDst; // Dest point
	HRESULT							hr;
	D3DLOCKED_RECT					d3dLockedRect;
	struct gl_pixelstore_attrib		srcPacking;
	int								i;
	GLint							DstRowStride;
	const struct gl_texture_format	*MesaFormat;

	switch (format) {
	case GL_STENCIL_INDEX:
	case GL_DEPTH_COMPONENT:
		return;
	}
	
	MesaFormat = _mesa_choose_tex_format(ctx, format, format, type);
	DstRowStride = _mesa_image_row_stride(pack, width, format, type);

	gldCtx	= GLD_GET_CONTEXT(ctx);
	gld		= GLD_GET_DX9_DRIVER(gldCtx);

	// Get backbuffer
	hr = IDirect3DDevice9_GetBackBuffer(
		gld->pDev,
		0, // First swapchain
		0, // First backbuffer
		D3DBACKBUFFER_TYPE_MONO,
		&pBackbuffer);
	if (FAILED(hr))
		return;

	// Get backbuffer description
	hr = IDirect3DSurface9_GetDesc(pBackbuffer, &d3dsd);
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	// Create a surface compatible with backbuffer
	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
		gld->pDev, 
		width,
		height,
		d3dsd.Format,
		D3DPOOL_SCRATCH,
		&pNativeImage,
		NULL);
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	// Compute source rect and dest point
	SetRect(&rcSrc, 0, 0, width, height);
	OffsetRect(&rcSrc, x, GLD_FLIP_HEIGHT(y, height));
	ptDst.x = ptDst.y = 0;

	// Get source pixels.
	//
	// Copy from backbuffer to our scratch surface using GetRenderTargetData
	// or lock/memcpy. D3DXLoadSurfaceFromSurface has been replaced.
	//
	{
		// Use GetRenderTargetData to copy backbuffer to a system-mem surface,
		// then lock both and memcpy the sub-rect we need.
		IDirect3DSurface9 *pRTCopy = NULL;
		D3DSURFACE_DESC bbDesc;
		IDirect3DSurface9_GetDesc(pBackbuffer, &bbDesc);

		hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
			gld->pDev, bbDesc.Width, bbDesc.Height,
			bbDesc.Format, D3DPOOL_SYSTEMMEM, &pRTCopy, NULL);
		if (SUCCEEDED(hr)) {
			hr = IDirect3DDevice9_GetRenderTargetData(gld->pDev, pBackbuffer, pRTCopy);
			if (SUCCEEDED(hr)) {
				// Now copy the sub-rect from pRTCopy to pNativeImage
				D3DLOCKED_RECT lrSrc, lrDst;
				if (SUCCEEDED(IDirect3DSurface9_LockRect(pRTCopy, &lrSrc, &rcSrc, D3DLOCK_READONLY)) &&
				    SUCCEEDED(IDirect3DSurface9_LockRect(pNativeImage, &lrDst, NULL, 0))) {
					UINT row;
					// Bytes per pixel for the native format
					UINT bpp = (d3dsd.Format == D3DFMT_A8R8G8B8 || d3dsd.Format == D3DFMT_X8R8G8B8) ? 4 :
					           (d3dsd.Format == D3DFMT_R5G6B5 || d3dsd.Format == D3DFMT_X1R5G5B5 || d3dsd.Format == D3DFMT_A1R5G5B5) ? 2 : 4;
					UINT copyBytes = width * bpp;
					for (row = 0; row < (UINT)height; row++) {
						memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
						       (BYTE*)lrSrc.pBits + row * lrSrc.Pitch,
						       copyBytes);
					}
					IDirect3DSurface9_UnlockRect(pNativeImage);
					IDirect3DSurface9_UnlockRect(pRTCopy);
				} else {
					hr = E_FAIL;
				}
			}
			IDirect3DSurface9_Release(pRTCopy);
		}
	}
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	// Create an RGBA8888 surface
	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
		gld->pDev, 
		width,
		height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SCRATCH,
		&pCanonicalImage,
		NULL);
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	// Convert native format to RGBA8888 via lock/memcpy
	// If native format is already A8R8G8B8, direct copy; otherwise pixel conversion.
	{
		D3DLOCKED_RECT lrSrc, lrDst;
		hr = IDirect3DSurface9_LockRect(pNativeImage, &lrSrc, NULL, D3DLOCK_READONLY);
		if (SUCCEEDED(hr)) {
			hr = IDirect3DSurface9_LockRect(pCanonicalImage, &lrDst, NULL, 0);
			if (SUCCEEDED(hr)) {
				if (d3dsd.Format == D3DFMT_A8R8G8B8 || d3dsd.Format == D3DFMT_X8R8G8B8) {
					// Same pixel size — direct row copy
					UINT row;
					UINT copyBytes = width * 4;
					for (row = 0; row < (UINT)height; row++) {
						memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
						       (BYTE*)lrSrc.pBits + row * lrSrc.Pitch,
						       copyBytes);
					}
					// For X8R8G8B8, set alpha to 0xFF
					if (d3dsd.Format == D3DFMT_X8R8G8B8) {
						for (row = 0; row < (UINT)height; row++) {
							DWORD *pPixel = (DWORD*)((BYTE*)lrDst.pBits + row * lrDst.Pitch);
							UINT col;
							for (col = 0; col < (UINT)width; col++) {
								pPixel[col] |= 0xFF000000;
							}
						}
					}
				} else if (d3dsd.Format == D3DFMT_R5G6B5) {
					// 16-bit RGB565 to 32-bit ARGB8888
					UINT row;
					for (row = 0; row < (UINT)height; row++) {
						WORD *pSrcPixel = (WORD*)((BYTE*)lrSrc.pBits + row * lrSrc.Pitch);
						DWORD *pDstPixel = (DWORD*)((BYTE*)lrDst.pBits + row * lrDst.Pitch);
						UINT col;
						for (col = 0; col < (UINT)width; col++) {
							WORD s = pSrcPixel[col];
							BYTE r = (BYTE)(((s >> 11) & 0x1F) * 255 / 31);
							BYTE g = (BYTE)(((s >> 5) & 0x3F) * 255 / 63);
							BYTE b = (BYTE)((s & 0x1F) * 255 / 31);
							pDstPixel[col] = 0xFF000000 | (r << 16) | (g << 8) | b;
						}
					}
				} else if (d3dsd.Format == D3DFMT_X1R5G5B5 || d3dsd.Format == D3DFMT_A1R5G5B5) {
					// 16-bit X/A1R5G5B5 to 32-bit ARGB8888
					UINT row;
					for (row = 0; row < (UINT)height; row++) {
						WORD *pSrcPixel = (WORD*)((BYTE*)lrSrc.pBits + row * lrSrc.Pitch);
						DWORD *pDstPixel = (DWORD*)((BYTE*)lrDst.pBits + row * lrDst.Pitch);
						UINT col;
						for (col = 0; col < (UINT)width; col++) {
							WORD s = pSrcPixel[col];
							BYTE a = (d3dsd.Format == D3DFMT_A1R5G5B5) ? ((s & 0x8000) ? 0xFF : 0x00) : 0xFF;
							BYTE r = (BYTE)(((s >> 10) & 0x1F) * 255 / 31);
							BYTE g = (BYTE)(((s >> 5) & 0x1F) * 255 / 31);
							BYTE b = (BYTE)((s & 0x1F) * 255 / 31);
							pDstPixel[col] = ((DWORD)a << 24) | (r << 16) | (g << 8) | b;
						}
					}
				} else {
					// Unsupported format — zero fill
					UINT row;
					for (row = 0; row < (UINT)height; row++) {
						memset((BYTE*)lrDst.pBits + row * lrDst.Pitch, 0, width * 4);
					}
				}
				IDirect3DSurface9_UnlockRect(pCanonicalImage);
			}
			IDirect3DSurface9_UnlockRect(pNativeImage);
		}
	}
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	srcPacking.Alignment	= 1;
	srcPacking.ImageHeight	= height;
	srcPacking.LsbFirst		= GL_FALSE;
	srcPacking.RowLength	= 0;
	srcPacking.SkipImages	= 0;
	srcPacking.SkipPixels	= 0;
	srcPacking.SkipRows		= 0;
	srcPacking.SwapBytes	= GL_FALSE;

	// Lock all of image
	hr = IDirect3DSurface9_LockRect(pCanonicalImage, &d3dLockedRect, NULL, 0);
	if (FAILED(hr)) {
		goto gld_ReadPixels_DX9_return;
	}

	// We need to flip the data. Yuck.
	// Perhaps Mesa has a span packer we can use in future...
	for (i=0; i<height; i++) {
		BYTE *pDestRow = (BYTE*)_mesa_image_address(pack, dest, width, height, format, type, 0, i, 0);
		BYTE *pSrcRow = (BYTE*)d3dLockedRect.pBits + (d3dLockedRect.Pitch * (height-i-1));
		_mesa_transfer_teximage(
			ctx,
			2,
			GL_RGBA,				// base format
			MesaFormat,				// dst format
			pDestRow,				// dest addr
			width, 1, 1, 0, 0, 0,	// src x,y,z & dst offsets x,y,z
			DstRowStride,			// dst row stride
			0,						// dstImageStride
			GL_BGRA,				// src format
			GL_UNSIGNED_BYTE,		// src type
			pSrcRow,				// src addr
			&srcPacking);			// packing params of source image
	}

	IDirect3DSurface9_UnlockRect(pCanonicalImage);

gld_ReadPixels_DX9_return:
	SAFE_RELEASE(pCanonicalImage);
	SAFE_RELEASE(pNativeImage);
	SAFE_RELEASE(pBackbuffer);
}

//---------------------------------------------------------------------------

void gld_CopyPixels_DX9(
	GLcontext *ctx,
	GLint srcx,
	GLint srcy,
	GLsizei width,
	GLsizei height,
	GLint dstx,
	GLint dsty,
	GLenum type)
{
	//
	// NOTE: Not allowed to copy vidmem to vidmem!
	//       Therefore we use an intermediate image surface.
	//

	GLD_context			*gldCtx;
	GLD_driver_dx9		*gld;

	IDirect3DSurface9	*pBackbuffer;
	D3DSURFACE_DESC		d3dsd;
	IDirect3DSurface9	*pImage;
	RECT				rcSrc; // Source rect
	POINT				ptDst; // Dest point
	HRESULT				hr;

	// Only backbuffer
	if (type != GL_COLOR)
		return;

	gldCtx	= GLD_GET_CONTEXT(ctx);
	gld		= GLD_GET_DX9_DRIVER(gldCtx);

	// Get backbuffer
	hr = IDirect3DDevice9_GetBackBuffer(
		gld->pDev,
		0, // First swapchain
		0, // First backbuffer
		D3DBACKBUFFER_TYPE_MONO,
		&pBackbuffer);
	if (FAILED(hr))
		return;

	// Get backbuffer description
	hr = IDirect3DSurface9_GetDesc(pBackbuffer, &d3dsd);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pBackbuffer);
		return;
	}

	// Create a surface compatible with backbuffer
	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
		gld->pDev, 
		width,
		height,
		d3dsd.Format,
		D3DPOOL_SCRATCH,
		&pImage,
		NULL);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pBackbuffer);
		return;
	}

	// Compute source rect and dest point
	SetRect(&rcSrc, 0, 0, width, height);
	OffsetRect(&rcSrc, srcx, GLD_FLIP_HEIGHT(srcy, height));
	ptDst.x = ptDst.y = 0;

	// Copy backbuffer sub-rect to scratch surface via GetRenderTargetData + lock/memcpy
	{
		IDirect3DSurface9 *pRTCopy = NULL;
		D3DSURFACE_DESC bbDesc;
		IDirect3DSurface9_GetDesc(pBackbuffer, &bbDesc);

		hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
			gld->pDev, bbDesc.Width, bbDesc.Height,
			bbDesc.Format, D3DPOOL_SYSTEMMEM, &pRTCopy, NULL);
		if (SUCCEEDED(hr)) {
			hr = IDirect3DDevice9_GetRenderTargetData(gld->pDev, pBackbuffer, pRTCopy);
			if (SUCCEEDED(hr)) {
				D3DLOCKED_RECT lrSrc, lrDst;
				if (SUCCEEDED(IDirect3DSurface9_LockRect(pRTCopy, &lrSrc, &rcSrc, D3DLOCK_READONLY)) &&
				    SUCCEEDED(IDirect3DSurface9_LockRect(pImage, &lrDst, NULL, 0))) {
					UINT row;
					UINT bpp = (d3dsd.Format == D3DFMT_A8R8G8B8 || d3dsd.Format == D3DFMT_X8R8G8B8) ? 4 :
					           (d3dsd.Format == D3DFMT_R5G6B5 || d3dsd.Format == D3DFMT_X1R5G5B5 || d3dsd.Format == D3DFMT_A1R5G5B5) ? 2 : 4;
					UINT copyBytes = width * bpp;
					for (row = 0; row < (UINT)height; row++) {
						memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
						       (BYTE*)lrSrc.pBits + row * lrSrc.Pitch,
						       copyBytes);
					}
					IDirect3DSurface9_UnlockRect(pImage);
					IDirect3DSurface9_UnlockRect(pRTCopy);
				} else {
					hr = E_FAIL;
				}
			}
			IDirect3DSurface9_Release(pRTCopy);
		}
	}
	IDirect3DSurface9_Release(pBackbuffer);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pImage);
		return;
	}

	_gldDrawPixels(ctx, FALSE, dstx, dsty, width, height, pImage);

	IDirect3DSurface9_Release(pImage);
}

//---------------------------------------------------------------------------

void gld_Bitmap_DX9(
	GLcontext *ctx,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height,
	const struct gl_pixelstore_attrib *unpack,
	const GLubyte *bitmap)
{
	GLD_context			*gldCtx;
	GLD_driver_dx9		*gld;

	IDirect3DSurface9	*pImage;
	HRESULT				hr;
	D3DLOCKED_RECT		d3dLockedRect;
	BYTE				*pTempBitmap;
	D3DCOLOR			clBitmapOne, clBitmapZero;
	D3DCOLOR			*pBits;
	const GLubyte		*src;
	int					i, j, k;

	gldCtx	= GLD_GET_CONTEXT(ctx);
	gld		= GLD_GET_DX9_DRIVER(gldCtx);

	// A NULL bitmap is valid, but merely advances the raster position
	if ((bitmap == NULL) || (width == 0) || (height == 0))
		return;

	clBitmapZero	= D3DCOLOR_RGBA(0,0,0,0); // NOTE: Alpha is Zero
	clBitmapOne		= D3DCOLOR_COLORVALUE(
		ctx->Current.RasterColor[0],
		ctx->Current.RasterColor[1],
		ctx->Current.RasterColor[2],
		1.0f); // NOTE: Alpha is One

	hr = IDirect3DDevice9_CreateOffscreenPlainSurface(
		gld->pDev, 
		width,
		height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SCRATCH,
		&pImage,
		NULL);
	if (FAILED(hr)) {
		return;
	}

	// Lock all of surface 
	hr = IDirect3DSurface9_LockRect(pImage, &d3dLockedRect, NULL, 0);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pImage);
		return;
	}

	pTempBitmap = (BYTE*)_mesa_unpack_bitmap(width, height, bitmap, unpack);
	if (pTempBitmap == NULL) {
		IDirect3DSurface9_Release(pImage);
		return;
	}

	pBits = (D3DCOLOR*)d3dLockedRect.pBits;

	for (i=0; i<height; i++) {
		GLubyte byte;
		pBits = (D3DCOLOR*)((BYTE*)d3dLockedRect.pBits + (i*d3dLockedRect.Pitch));
		src = (const GLubyte *) _mesa_image_address(
			&_mesa_native_packing, pTempBitmap, width, height, GL_COLOR_INDEX, GL_BITMAP,
			0, i, 0);
		for (j=0; j<(width>>3); j++) {
			byte = *src++;
			for (k=0; k<8; k++) {
				*pBits++ = (byte & 128) ? clBitmapOne : clBitmapZero;
				byte <<= 1;
			}
		}
		// Fill remaining bits from bitmap
		if (width & 7) {
			byte = *src;
			for (k=0; k<(width & 7); k++) {
				*pBits++ = (byte & 128) ? clBitmapOne : clBitmapZero;
				byte <<= 1;
			}
		}
	}

	FREE(pTempBitmap);

/*
	// unpack image, apply transfer ops and store directly in texture
	_mesa_transfer_teximage(
		ctx,
		2,
		GL_BITMAP,
		&_mesa_texformat_argb8888,
		d3dLockedRect.pBits,
		width, height, 1, 0, 0, 0,
		d3dLockedRect.Pitch,
		0, // dstImageStride
		GL_BITMAP, GL_COLOR_INDEX, bitmap, unpack);
*/
	IDirect3DSurface9_UnlockRect(pImage);

	_gldDrawPixels(ctx, TRUE, x, y, width, height, pImage);

	IDirect3DSurface9_Release(pImage);
}

//---------------------------------------------------------------------------
// Texture functions
//---------------------------------------------------------------------------

void _gldAllocateTexture(
	GLcontext *ctx,
	struct gl_texture_object *tObj,
	struct gl_texture_image *texImage)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTex;
	D3DFORMAT			d3dFormat;

	if (!tObj || !texImage)
		return;

	pTex = (IDirect3DTexture9*)tObj->DriverData;
	if (pTex) {
		// Decide whether we can keep existing D3D texture
		// by examining top-level surface.
		D3DSURFACE_DESC d3dsd;
		_GLD_DX9_TEX(GetLevelDesc(pTex, 0, &d3dsd));
		// Release existing texture if not compatible
		if ((d3dsd.Width == texImage->Width) || 
			(d3dsd.Height == texImage->Height))
		{
			return; // Keep the existing texture
		}
		tObj->DriverData = NULL;
		_GLD_DX9_TEX(Release(pTex));
	}

	d3dFormat = _gldGLFormatToD3DFormat(texImage->IntFormat);
	// Pure D3D9: IDirect3DDevice9_CreateTexture replaces D3DXCreateTexture.
	// When mipmaps are requested, use 0 (full chain) instead of D3DX_DEFAULT.
	{
		UINT mipLevels = (glb.bUseMipmaps) ? 0 : 1;
		HRESULT hr = IDirect3DDevice9_CreateTexture(
			gld->pDev,
			texImage->Width,
			texImage->Height,
			mipLevels,
			0,				// Usage
			d3dFormat,
			D3DPOOL_MANAGED,
			&pTex,
			NULL);
		if (FAILED(hr)) {
			// If the requested format isn't supported, fall back to A8R8G8B8
			if (d3dFormat != D3DFMT_A8R8G8B8) {
				hr = IDirect3DDevice9_CreateTexture(
					gld->pDev,
					texImage->Width,
					texImage->Height,
					mipLevels,
					0,
					D3DFMT_A8R8G8B8,
					D3DPOOL_MANAGED,
					&pTex,
					NULL);
			}
			if (FAILED(hr))
				pTex = NULL;
		}
	}
	tObj->DriverData = pTex;
}

//---------------------------------------------------------------------------

const struct gl_texture_format* gld_ChooseTextureFormat_DX9(
	GLcontext *ctx,
	GLint internalFormat,
	GLenum srcFormat,
	GLenum srcType)
{
	// [Based on mesa_choose_tex_format()]
	//
	// We will choose only texture formats that are supported
	// by Direct3D. If the hardware doesn't support a particular
	// texture format, then the D3DX texture calls that we use
	// will automatically use a HW supported format.
	//
	// The most critical aim is to reduce copying; if we can use
	// texture-image data directly then it will be a big performance assist.
	//

	switch (internalFormat) {
	case GL_INTENSITY:
	case GL_INTENSITY4:
	case GL_INTENSITY8:
	case GL_INTENSITY12:
	case GL_INTENSITY16:
		return &_mesa_texformat_l8; // D3DFMT_L8
	case 1:
	case GL_LUMINANCE:
	case GL_LUMINANCE4:
	case GL_LUMINANCE8:
	case GL_LUMINANCE12:
	case GL_LUMINANCE16:
		return &_mesa_texformat_l8; // D3DFMT_L8
	case GL_ALPHA:
	case GL_ALPHA4:
	case GL_ALPHA8:
	case GL_ALPHA12:
	case GL_ALPHA16:
		return &_mesa_texformat_a8; // D3DFMT_A8
	case GL_COLOR_INDEX:
	case GL_COLOR_INDEX1_EXT:
	case GL_COLOR_INDEX2_EXT:
	case GL_COLOR_INDEX4_EXT:
	case GL_COLOR_INDEX8_EXT:
	case GL_COLOR_INDEX12_EXT:
	case GL_COLOR_INDEX16_EXT:
		return &_mesa_texformat_rgb565; // D3DFMT_R5G6B5
		// Mesa will convert this for us later...
		//      return &_mesa_texformat_ci8; // D3DFMT_R5G6B5
	case 2:
	case GL_LUMINANCE_ALPHA:
	case GL_LUMINANCE4_ALPHA4:
	case GL_LUMINANCE6_ALPHA2:
	case GL_LUMINANCE8_ALPHA8:
	case GL_LUMINANCE12_ALPHA4:
	case GL_LUMINANCE12_ALPHA12:
	case GL_LUMINANCE16_ALPHA16:
		return &_mesa_texformat_al88; // D3DFMT_A8L8
	case GL_R3_G3_B2:
		return &_mesa_texformat_rgb332; // D3DFMT_R3G3B2
	case GL_RGB4:
	case GL_RGBA4:
	case GL_RGBA2:
		return &_mesa_texformat_argb4444; // D3DFMT_A4R4G4B4
	case 3:
	case GL_RGB:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB10:
	case GL_RGB12:
	case GL_RGB16:
		return &_mesa_texformat_rgb565;
	case 4:
	case GL_RGBA:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_RGBA12:
	case GL_RGBA16:
		return &_mesa_texformat_argb8888;
	case GL_RGB5_A1:
		return &_mesa_texformat_argb1555;
	default:
		_mesa_problem(NULL, "unexpected format in fxDDChooseTextureFormat");
		return NULL;
   }
}

//---------------------------------------------------------------------------

/*
// Safer(?), slower version.
void gld_TexImage2D_DX9(
	GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint internalFormat,
	GLint width,
	GLint height,
	GLint border,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *tObj,
	struct gl_texture_image *texImage)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTex;
	IDirect3DSurface9	*pSurface;
	RECT				rcSrcRect;
	HRESULT				hr;
	GLint				texelBytes = 4;
	GLvoid				*tempImage;

	if (!tObj || !texImage)
		return;

	if (level == 0) {
		_gldAllocateTexture(ctx, tObj, texImage);
	}

	pTex = (IDirect3DTexture9*)tObj->DriverData;
	if (!pTex)
		return; // Texture has not been created
	if (level >= IDirect3DTexture9_GetLevelCount(pTex))
		return; // Level does not exist
	hr = IDirect3DTexture9_GetSurfaceLevel(pTex, level, &pSurface);
	if (FAILED(hr))
		return; // Surface level doesn't exist (or just a plain error)

	tempImage = MALLOC(width * height * texelBytes);
	if (!tempImage) {
		_mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
		IDirect3DSurface9_Release(pSurface);
		return;
	}
	// unpack image, apply transfer ops and store in tempImage
	_mesa_transfer_teximage(ctx, 2, texImage->Format,
		&_mesa_texformat_argb8888, // dest format
		tempImage,
		width, height, 1, 0, 0, 0,
		width * texelBytes,
		0, // dstImageStride
		format, type, pixels, packing);

	SetRect(&rcSrcRect, 0, 0, width, height);
	// Pure D3D9: lock surface, memcpy pixel data, unlock
	{
		D3DLOCKED_RECT lrDst;
		hr = IDirect3DSurface9_LockRect(pSurface, &lrDst, NULL, 0);
		if (SUCCEEDED(hr)) {
			UINT row;
			UINT srcPitch = width * texelBytes;
			UINT copyBytes = (srcPitch < (UINT)lrDst.Pitch) ? srcPitch : (UINT)lrDst.Pitch;
			for (row = 0; row < (UINT)height; row++) {
				memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
				       (BYTE*)tempImage + row * srcPitch,
				       copyBytes);
			}
			IDirect3DSurface9_UnlockRect(pSurface);
		}
	}

	FREE(tempImage);
	IDirect3DSurface9_Release(pSurface);
}
*/

//---------------------------------------------------------------------------

static void gldTexImage2DScaled(
	GLcontext *ctx,
	GLint width,
	GLint height,
	GLenum format,
	IDirect3DSurface9 *pSurface,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_image *texImage)
{
	const GLint		texelBytes = 4;
	GLvoid			*tempImage;

	tempImage = MALLOC(width * height * texelBytes);
	if (!tempImage) {
		_mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
		IDirect3DSurface9_Release(pSurface);
		return;
	}
	// unpack image, apply transfer ops and store in tempImage
	_mesa_transfer_teximage(ctx, 2, texImage->Format,
		&_mesa_texformat_argb8888, // dest format
		tempImage,
		width, height, 1, 0, 0, 0,
		width * texelBytes,
		0, // dstImageStride
		format, type, pixels, packing);

	// Pure D3D9: lock surface, memcpy pixel data, unlock
	// When the dest surface is smaller than the source, we copy only
	// what fits (the D3D9 managed texture may have been created at a
	// different size than requested).
	{
		D3DSURFACE_DESC d3dsd;
		D3DLOCKED_RECT lrDst;
		IDirect3DSurface9_GetDesc(pSurface, &d3dsd);
		if (SUCCEEDED(IDirect3DSurface9_LockRect(pSurface, &lrDst, NULL, 0))) {
			UINT srcPitch = width * texelBytes;
			UINT copyRows = ((UINT)height < d3dsd.Height) ? (UINT)height : d3dsd.Height;
			UINT copyBytes = ((UINT)(width * texelBytes) < (UINT)lrDst.Pitch) ? (UINT)(width * texelBytes) : (UINT)lrDst.Pitch;
			UINT row;
			for (row = 0; row < copyRows; row++) {
				memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
				       (BYTE*)tempImage + row * srcPitch,
				       copyBytes);
			}
			IDirect3DSurface9_UnlockRect(pSurface);
		}
	}

	FREE(tempImage);
	IDirect3DSurface9_Release(pSurface);
}

//---------------------------------------------------------------------------

// Faster, more efficient version.
// Copies subimage straight to dest texture
void gld_TexImage2D_DX9(
	GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint internalFormat,
	GLint width,
	GLint height,
	GLint border,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *tObj,
	struct gl_texture_image *texImage)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTex;
	IDirect3DSurface9	*pSurface;
	HRESULT				hr;
	D3DLOCKED_RECT		d3dLockedRect;
	D3DSURFACE_DESC		d3dsd;

	if (!tObj || !texImage)
		return;

	// GLQUAKE FIX
	// Test for input alpha data with non-alpha internalformat
	if (((internalFormat==3) || (internalFormat==GL_RGB)) && (format==GL_RGBA)) {
		// Input format has alpha, but a non-alpha format has been requested.
		texImage->IntFormat = GL_RGBA;
		internalFormat = GL_RGBA;
	}

	if (level == 0) {
		_gldAllocateTexture(ctx, tObj, texImage);
	}

	pTex = (IDirect3DTexture9*)tObj->DriverData;
	if (!pTex)
		return; // Texture has not been created
	if (level >= IDirect3DTexture9_GetLevelCount(pTex))
		return; // Level does not exist
	hr = IDirect3DTexture9_GetSurfaceLevel(pTex, level, &pSurface);
	if (FAILED(hr))
		return; // Surface level doesn't exist (or just a plain error)

	IDirect3DSurface9_GetDesc(pSurface, &d3dsd);
	if ((width > d3dsd.Width) | (height > d3dsd.Height)) {
		gldTexImage2DScaled(ctx, width, height, format, pSurface, type, pixels, packing, texImage);
		return;
	}

	// Lock all of surface 
	hr = IDirect3DSurface9_LockRect(pSurface, &d3dLockedRect, NULL, 0);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pSurface);
#ifdef GLD_SAVE_TEXTURE_TO_FILE
		_gldSaveTextureToFile(tObj->Name, (IDirect3DTexture9*)tObj->DriverData);
#endif
		return;
	}

	// unpack image, apply transfer ops and store directly in texture
	_mesa_transfer_teximage(
		ctx,
		2,
		texImage->Format,
		_gldMesaFormatForD3DFormat(d3dsd.Format),
		d3dLockedRect.pBits,
		width, height, 1, 0, 0, 0,
		d3dLockedRect.Pitch,
		0, // dstImageStride
		format, type, pixels, packing);

	IDirect3DSurface9_UnlockRect(pSurface);
	IDirect3DSurface9_Release(pSurface);

#ifdef GLD_SAVE_TEXTURE_TO_FILE
	_gldSaveTextureToFile(tObj->Name, (IDirect3DTexture9*)tObj->DriverData);
#endif
}

//---------------------------------------------------------------------------

void gld_TexImage1D_DX9(GLcontext *ctx, GLenum target, GLint level,
                       GLint internalFormat,
                       GLint width, GLint border,
                       GLenum format, GLenum type, const GLvoid *pixels,
                       const struct gl_pixelstore_attrib *packing,
                       struct gl_texture_object *texObj,
                       struct gl_texture_image *texImage )
{
	// A 1D texture is a 2D texture with a height of zero
	gld_TexImage2D_DX9(ctx, target, level, internalFormat, width, 1, border, format, type, pixels, packing, texObj, texImage);
}

//---------------------------------------------------------------------------

#if 0
void gld_TexSubImage2D_DX9( GLcontext *ctx, GLenum target, GLint level,
                          GLint xoffset, GLint yoffset,
                          GLsizei width, GLsizei height,
                          GLenum format, GLenum type,
                          const GLvoid *pixels,
                          const struct gl_pixelstore_attrib *packing,
                          struct gl_texture_object *tObj,
                          struct gl_texture_image *texImage )
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTex;
	IDirect3DSurface9	*pSurface;
	D3DFORMAT			d3dFormat;
	HRESULT				hr;
	GLint				texelBytes = 4;
	GLvoid				*tempImage;
	RECT				rcSrcRect;
	RECT				rcDstRect;

	if (!tObj || !texImage)
		return;

	pTex = (IDirect3DTexture9*)tObj->DriverData;
	if (!pTex)
		return; // Texture has not been created
	if (level >= IDirect3DTexture9_GetLevelCount(pTex))
		return; // Level does not exist
	hr = IDirect3DTexture9_GetSurfaceLevel(pTex, level, &pSurface);
	if (FAILED(hr))
		return; // Surface level doesn't exist (or just a plain error)

	d3dFormat = _gldGLFormatToD3DFormat(texImage->Format);
	tempImage = MALLOC(width * height * texelBytes);
	if (!tempImage) {
		_mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
		IDirect3DSurface9_Release(pSurface);
		return;
	}

	// unpack image, apply transfer ops and store in tempImage
	_mesa_transfer_teximage(ctx, 2, texImage->Format,
		&_mesa_texformat_argb8888, // dest format
		tempImage,
		width, height, 1, 0, 0, 0,
		width * texelBytes,
		0, // dstImageStride
		format, type, pixels, packing);

	// Source rectangle is whole of input image
	SetRect(&rcSrcRect, 0, 0, width, height);

	// Dest rectangle must be offset to dest image
	SetRect(&rcDstRect, 0, 0, width, height);
	OffsetRect(&rcDstRect, xoffset, yoffset);

	// Pure D3D9: lock dest sub-rect, memcpy pixel data, unlock
	{
		D3DLOCKED_RECT lrDst;
		hr = IDirect3DSurface9_LockRect(pSurface, &lrDst, &rcDstRect, 0);
		if (SUCCEEDED(hr)) {
			UINT row;
			UINT srcPitch = width * texelBytes;
			UINT copyBytes = (srcPitch < (UINT)lrDst.Pitch) ? srcPitch : (UINT)lrDst.Pitch;
			for (row = 0; row < (UINT)height; row++) {
				memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
				       (BYTE*)tempImage + row * srcPitch,
				       copyBytes);
			}
			IDirect3DSurface9_UnlockRect(pSurface);
		}
	}

	FREE(tempImage);
	IDirect3DSurface9_Release(pSurface);
}
#else

//---------------------------------------------------------------------------

static void gldTexSubImage2DScaled(
	GLcontext *ctx,
	GLint xoffset,
	GLint yoffset,
	GLint width,
	GLint height,
	GLenum format,
	IDirect3DSurface9 *pSurface,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_image *texImage,
	D3DSURFACE_DESC *d3dsd)
{
	RECT			rcDstRect;
	const GLint		texelBytes = 4;
	GLvoid			*tempImage;

	tempImage = MALLOC(width * height * texelBytes);
	if (!tempImage) {
		_mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage2D");
		IDirect3DSurface9_Release(pSurface);
		return;
	}
	// unpack image, apply transfer ops and store in tempImage
	_mesa_transfer_teximage(ctx, 2, texImage->Format,
		&_mesa_texformat_argb8888, // dest format
		tempImage,
		width, height, 1, 0, 0, 0,
		width * texelBytes,
		0, // dstImageStride
		format, type, pixels, packing);

	// Dest rectangle must be offset to dest image
	SetRect(&rcDstRect, 0, 0, width, height);
	OffsetRect(&rcDstRect, xoffset, yoffset);

	// Pure D3D9: lock dest sub-rect, memcpy pixel data, unlock
	{
		D3DLOCKED_RECT lrDst;
		HRESULT hr = IDirect3DSurface9_LockRect(pSurface, &lrDst, &rcDstRect, 0);
		if (SUCCEEDED(hr)) {
			UINT srcPitch = width * texelBytes;
			UINT copyRows = ((UINT)height < d3dsd->Height) ? (UINT)height : d3dsd->Height;
			UINT copyBytes = ((UINT)(width * texelBytes) < (UINT)lrDst.Pitch) ? (UINT)(width * texelBytes) : (UINT)lrDst.Pitch;
			UINT row;
			for (row = 0; row < copyRows; row++) {
				memcpy((BYTE*)lrDst.pBits + row * lrDst.Pitch,
				       (BYTE*)tempImage + row * srcPitch,
				       copyBytes);
			}
			IDirect3DSurface9_UnlockRect(pSurface);
		}
	}

	FREE(tempImage);
	IDirect3DSurface9_Release(pSurface);
}

//---------------------------------------------------------------------------

// Faster, more efficient version.
// Copies subimage straight to dest texture
void gld_TexSubImage2D_DX9( GLcontext *ctx, GLenum target, GLint level,
                          GLint xoffset, GLint yoffset,
                          GLsizei width, GLsizei height,
                          GLenum format, GLenum type,
                          const GLvoid *pixels,
                          const struct gl_pixelstore_attrib *packing,
                          struct gl_texture_object *tObj,
                          struct gl_texture_image *texImage )
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	IDirect3DTexture9	*pTex;
	IDirect3DSurface9	*pSurface;
	HRESULT				hr;
	RECT				rcDstRect;
	D3DLOCKED_RECT		d3dLockedRect;
	D3DSURFACE_DESC		d3dsd;

	if (!tObj || !texImage)
		return;

	pTex = (IDirect3DTexture9*)tObj->DriverData;
	if (!pTex)
		return; // Texture has not been created
	if (level >= IDirect3DTexture9_GetLevelCount(pTex))
		return; // Level does not exist
	hr = IDirect3DTexture9_GetSurfaceLevel(pTex, level, &pSurface);
	if (FAILED(hr))
		return; // Surface level doesn't exist (or just a plain error)

	IDirect3DSurface9_GetDesc(pSurface, &d3dsd);
	if ((width > d3dsd.Width) || (height > d3dsd.Height)) {
		gldTexSubImage2DScaled(ctx, xoffset, yoffset, width, height, format, pSurface, type, pixels, packing, texImage, &d3dsd);
		return;
	}

	// Dest rectangle must be offset to dest image
	SetRect(&rcDstRect, 0, 0, width, height);
	OffsetRect(&rcDstRect, xoffset, yoffset);

	// Lock sub-rect of surface 
	hr = IDirect3DSurface9_LockRect(pSurface, &d3dLockedRect, &rcDstRect, 0);
	if (FAILED(hr)) {
		IDirect3DSurface9_Release(pSurface);
		return;
	}

	// unpack image, apply transfer ops and store directly in texture
	_mesa_transfer_teximage(ctx, 2, texImage->Format,
		_gldMesaFormatForD3DFormat(d3dsd.Format),
		d3dLockedRect.pBits,
		width, height, 1,
		0, 0, 0, // NOTE: d3dLockedRect.pBits is already offset!!!
		d3dLockedRect.Pitch,
		0, // dstImageStride
		format, type, pixels, packing);


	IDirect3DSurface9_UnlockRect(pSurface);
	IDirect3DSurface9_Release(pSurface);
}
#endif
//---------------------------------------------------------------------------

void gld_TexSubImage1D_DX9( GLcontext *ctx, GLenum target, GLint level,
                          GLint xoffset, GLsizei width,
                          GLenum format, GLenum type,
                          const GLvoid *pixels,
                          const struct gl_pixelstore_attrib *packing,
                          struct gl_texture_object *texObj,
                          struct gl_texture_image *texImage )
{
	gld_TexSubImage2D_DX9(ctx, target, level, xoffset, 0, width, 1, format, type, pixels, packing, texObj, texImage);
}

//---------------------------------------------------------------------------

void gld_DeleteTexture_DX9(
	GLcontext *ctx,
	struct gl_texture_object *tObj)
{
	GLD_context *gld = (GLD_context*)(ctx->DriverCtx);

	if (tObj) {
		IDirect3DTexture9 *pTex = (IDirect3DTexture9*)tObj->DriverData;
		if (pTex) {
/*			// Make sure texture is not bound to a stage before releasing it
			for (int i=0; i<MAX_TEXTURE_UNITS; i++) {
				if (gld->CurrentTexture[i] == pTex) {
					gld->pDev->SetTexture(i, NULL);
					gld->CurrentTexture[i] = NULL;
				}
			}*/
			_GLD_DX9_TEX(Release(pTex));
			tObj->DriverData = NULL;
		}
	}
}

//---------------------------------------------------------------------------
#if 0
__inline void _gldSetColorOps(
	const GLD_driver_dx9 *gld,
	GLuint unit,
	DWORD ColorArg1,
	D3DTEXTUREOP ColorOp,
	DWORD ColorArg2)
{
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_COLORARG1, ColorArg1));
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_COLOROP, ColorOp));
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_COLORARG2, ColorArg2));
}

//---------------------------------------------------------------------------

__inline void _gldSetAlphaOps(
	const GLD_driver_dx9 *gld,
	GLuint unit,
	DWORD AlphaArg1,
	D3DTEXTUREOP AlphaOp,
	DWORD AlphaArg2)
{
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_ALPHAARG1, AlphaArg1));
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_ALPHAOP, AlphaOp));
	_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_ALPHAARG2, AlphaArg2));
}

//---------------------------------------------------------------------------

void gldUpdateTextureUnit(
	GLcontext *ctx,
	GLuint unit,
	BOOL bPassThrough)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	D3DTEXTUREFILTERTYPE	minfilter;
	D3DTEXTUREFILTERTYPE	mipfilter;
	GLenum					BaseFormat;
	DWORD					dwColorArg0;
	int						iTexEnv = 0;
	GLD_texenv				*pTexenv;

	// NOTE: If bPassThrough is FALSE then texture stage can be
	// disabled otherwise it must pass-through it's current fragment.

	const struct gl_texture_unit *pUnit = &ctx->Texture.Unit[unit];
	const struct gl_texture_object *tObj = pUnit->_Current;

	IDirect3DTexture9 *pTex = NULL;
	if (tObj) {
		pTex = (IDirect3DTexture9*)tObj->DriverData;
	}

	// Enable texturing if unit is enabled and a valid D3D texture exists
	// Mesa 5: TEXTUREn_x altered to TEXTURE_nD_BIT
	//if (pTex && (pUnit->Enabled & (TEXTURE0_1D | TEXTURE0_2D))) {
	if (pTex && (pUnit->_ReallyEnabled & (TEXTURE_1D_BIT | TEXTURE_2D_BIT))) {
		// Enable texturing
		_GLD_DX9_DEV(SetTexture(gld->pDev, unit, pTex));
	} else {
		// Disable texturing, then return
		_GLD_DX9_DEV(SetTexture(gld->pDev, unit, NULL));
		if (bPassThrough) {
			_gldSetColorOps(gld, unit, D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_DIFFUSE);
			_gldSetAlphaOps(gld, unit, D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_DIFFUSE);
		} else {
			_gldSetColorOps(gld, unit, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_DIFFUSE);
			_gldSetAlphaOps(gld, unit, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_DIFFUSE);
		}
		return;
	}

	// Texture parameters
	_gldConvertMinFilter(tObj->MinFilter, &minfilter, &mipfilter);
	_GLD_DX9_DEV(SetSamplerState(gld->pDev, unit, D3DSAMP_MINFILTER, minfilter));
	_GLD_DX9_DEV(SetSamplerState(gld->pDev, unit, D3DSAMP_MIPFILTER, mipfilter));
	_GLD_DX9_DEV(SetSamplerState(gld->pDev, unit, D3DSAMP_MAGFILTER, _gldConvertMagFilter(tObj->MagFilter)));
	_GLD_DX9_DEV(SetSamplerState(gld->pDev, unit, D3DSAMP_ADDRESSU, _gldConvertWrap(tObj->WrapS)));
	_GLD_DX9_DEV(SetSamplerState(gld->pDev, unit, D3DSAMP_ADDRESSV, _gldConvertWrap(tObj->WrapT)));

	// Texture priority
	_GLD_DX9_TEX(SetPriority(pTex, (DWORD)(tObj->Priority*65535.0f)));

	// Texture environment
	// TODO: Examine input texture for alpha and use specific alpha/non-alpha ops.
	//       See Page 355 of the Red Book.
	BaseFormat = _gldDecodeBaseFormat(pTex);

	switch (BaseFormat) {
	case GL_RGB:
		iTexEnv = 0;
		break;
	case GL_RGBA:
		iTexEnv = 1;
		break;
	case GL_ALPHA:
		iTexEnv = 2;
		break;
	}

	switch (pUnit->EnvMode) {
	case GL_DECAL:
		iTexEnv += 0;
		break;
	case GL_REPLACE:
		iTexEnv += 3;
		break;
	case GL_MODULATE:
		iTexEnv += 6;
		break;
	case GL_BLEND:
		// Set blend colour
		dwColorArg0 = gldClampedColour(pUnit->EnvColor);
		_GLD_DX9_DEV(SetTextureStageState(gld->pDev, unit, D3DTSS_COLORARG0, dwColorArg0));
		iTexEnv += 9;
		break;
	case GL_ADD:
		iTexEnv += 12;
		break;
	}
	pTexenv = (GLD_texenv*)&gldTexEnv[iTexEnv];
	_gldSetColorOps(gld, unit, pTexenv->ColorArg1, pTexenv->ColorOp, pTexenv->ColorArg2);
	_gldSetAlphaOps(gld, unit, pTexenv->AlphaArg1, pTexenv->AlphaOp, pTexenv->AlphaArg2);
}

//---------------------------------------------------------------------------

void gld_NEW_TEXTURE_DX9(
	GLcontext *ctx)
{
	// TODO: Support for three (ATI Radeon) or more (nVidia GeForce3) texture units

	BOOL bUnit0Enabled;
	BOOL bUnit1Enabled;

	if (!ctx)
		return; // Sanity check

	if (ctx->Const.MaxTextureUnits == 1) {
		gldUpdateTextureUnit(ctx, 0, TRUE);
		return;
	}

	//
	// NOTE: THE FOLLOWING RELATES TO TWO TEXTURE UNITS, AND TWO ONLY!!
	//
	bUnit0Enabled = (ctx->Texture.Unit[0]._ReallyEnabled & (TEXTURE_1D_BIT | TEXTURE_2D_BIT)) ? TRUE : FALSE;
	bUnit1Enabled = (ctx->Texture.Unit[1]._ReallyEnabled & (TEXTURE_1D_BIT | TEXTURE_2D_BIT)) ? TRUE : FALSE;

	// If Unit0 is disabled and Unit1 is enabled then we must pass-though
	gldUpdateTextureUnit(ctx, 0, (!bUnit0Enabled && bUnit1Enabled) ? TRUE : FALSE);
	// We can always disable the last texture unit
	gldUpdateTextureUnit(ctx, 1, FALSE);

#ifdef _DEBUG
#if 0
	{
		// Find out whether device supports current renderstates
		GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
		GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
//		GLD_context			*gld	= GLD_GET_CONTEXT(ctx);

		DWORD dwPasses;
		_GLD_DX9_DEV(ValidateDevice(gld->pDev, &dwPasses));
//		if (FAILED(hr)) {
//			gldLogError(GLDLOG_ERROR, "ValidateDevice failed", hr);
//		}
		if (dwPasses != 1) {
			gldLogMessage(GLDLOG_ERROR, "ValidateDevice: Can't do in one pass\n");
		}
	}
#endif
#endif // _DEBUG
};
#else
//---------------------------------------------------------------------------

void gld_NEW_TEXTURE_DX9(
	GLcontext *ctx)
{
	// ** NOT REQUIRED WHEN USING SHADERS **
}
#endif
//---------------------------------------------------------------------------
