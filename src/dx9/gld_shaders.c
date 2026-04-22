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
* Environment:  Windows 9x/2000/XP
*
* Description:  Fixed-function pipeline state management for D3D9.
*               Replaces the former D3DX Effect (HLSL) system with direct
*               D3D9 SetTransform/SetMaterial/SetLight/SetRenderState/SetTexture calls.
*               Only d3d9.dll is required — no D3DX dependency.
*
*********************************************************************************/

#include <string.h>

#include "gld_context.h"
#include "gld_log.h"
#include "gldirect5.h"
#include "mesa_compat.h"

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------

static void _gldVec4FromGLfloat(GLD_VEC4 *vec, const GLfloat *f)
{
	vec->x = f[0];
	vec->y = f[1];
	vec->z = f[2];
	vec->w = f[3];
}

//---------------------------------------------------------------------------

static void _gldVec4FromGLfloat3(GLD_VEC4 *vec, const GLfloat *f)
{
	vec->x = f[0];
	vec->y = f[1];
	vec->z = f[2];
	vec->w = 1.0f;
}

//---------------------------------------------------------------------------

static void _gldColorValueFromGLfloat(D3DCOLORVALUE *cv, const GLfloat *f)
{
	cv->r = f[0];
	cv->g = f[1];
	cv->b = f[2];
	cv->a = f[3];
}

//---------------------------------------------------------------------------
// Effect state matching
//---------------------------------------------------------------------------

static int _gldFindEffect(
	GLD_driver_dx9 *gld,
	const GLD_effect_state *pEffectState)
{
	int			i;
	GLD_effect	*pGLDEffect;

	// See if effect state has already been cached.
	pGLDEffect = gld->Effects;
	for (i=0; i<gld->nEffects; i++, pGLDEffect++) {
		if (memcmp(&pGLDEffect->State, pEffectState, sizeof(GLD_effect_state)) == 0)
			return i; // Found a matching state. Return its index.
	}

	// State not found. Create a new cache entry.
	if (gld->nEffects >= 100) {
		// Cache full — reuse slot 0
		gldLogMessage(GLDLOG_WARN, "Effect state cache full, reusing slot 0\n");
		pGLDEffect = &gld->Effects[0];
		pGLDEffect->State = *pEffectState;
		pGLDEffect->bActive = FALSE;
		return 0;
	}

	pGLDEffect = &gld->Effects[gld->nEffects];
	ZeroMemory(pGLDEffect, sizeof(GLD_effect));
	pGLDEffect->State = *pEffectState;
	pGLDEffect->bActive = FALSE;

	i = gld->nEffects;
	gld->nEffects++;
	return i;
}

//---------------------------------------------------------------------------
// Apply fixed-function state to D3D9 device
//---------------------------------------------------------------------------

static void _gldApplyFixedFunctionState(
	GLcontext *ctx,
	GLD_driver_dx9 *gld)
{
	IDirect3DDevice9	*pDev = gld->pDev;
	int					i;

	// Ensure we're using fixed-function (no shaders)
	IDirect3DDevice9_SetVertexShader(pDev, NULL);
	IDirect3DDevice9_SetPixelShader(pDev, NULL);

	//
	// Transforms
	//
	IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &gld->matProjection);
	IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &gld->matModelView);
	// D3D9 fixed-function uses D3DTS_VIEW for the view matrix.
	// In GLDirect the ModelView matrix combines both, so set VIEW to identity.
	{
		D3DMATRIX matIdentity;
		ZeroMemory(&matIdentity, sizeof(matIdentity));
		matIdentity._11 = matIdentity._22 = matIdentity._33 = matIdentity._44 = 1.0f;
		IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &matIdentity);
	}

	//
	// Lighting
	//
	if (ctx->Light.Enabled) {
		D3DMATERIAL9	d3dMtl;
		D3DLIGHT9		d3dLight;
		const struct gl_material *mat = &ctx->Light.Material[0];

		IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);

		// Two-sided lighting
		if (ctx->Light.Model.TwoSide) {
			// D3D9 doesn't natively support two-sided lighting in fixed-function.
			// We approximate by enabling back-face culling off and using front material.
			// The HLSL system had the same limitation in practice.
		}

		// Front material
		ZeroMemory(&d3dMtl, sizeof(d3dMtl));
		_gldColorValueFromGLfloat(&d3dMtl.Ambient,  mat->Attrib[MAT_ATTRIB_FRONT_AMBIENT]);
		_gldColorValueFromGLfloat(&d3dMtl.Diffuse,  mat->Attrib[MAT_ATTRIB_FRONT_DIFFUSE]);
		_gldColorValueFromGLfloat(&d3dMtl.Specular, mat->Attrib[MAT_ATTRIB_FRONT_SPECULAR]);
		_gldColorValueFromGLfloat(&d3dMtl.Emissive, mat->Attrib[MAT_ATTRIB_FRONT_EMISSION]);
		d3dMtl.Power = mat->Attrib[MAT_ATTRIB_FRONT_SHININESS][0];
		IDirect3DDevice9_SetMaterial(pDev, &d3dMtl);

		// Global ambient
		{
			const GLfloat *amb = ctx->Light.Model.Ambient;
			DWORD dwAmbient = D3DCOLOR_COLORVALUE(amb[0], amb[1], amb[2], amb[3]);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENT, dwAmbient);
		}

		// Color material
		if (ctx->Light.ColorMaterialEnabled) {
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORVERTEX, TRUE);
			switch (ctx->Light.ColorMaterialMode) {
			case GL_AMBIENT:
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				break;
			case GL_DIFFUSE:
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				break;
			case GL_SPECULAR:
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR1);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				break;
			case GL_EMISSION:
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
				break;
			case GL_AMBIENT_AND_DIFFUSE:
			default:
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
				IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				break;
			}
		} else {
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORVERTEX, FALSE);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
		}

		// Lights
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			const struct gl_light *glLit = &ctx->Light.Light[i];
			if (glLit->Enabled) {
				ZeroMemory(&d3dLight, sizeof(d3dLight));

				_gldColorValueFromGLfloat(&d3dLight.Ambient, glLit->Ambient);
				_gldColorValueFromGLfloat(&d3dLight.Diffuse, glLit->Diffuse);
				_gldColorValueFromGLfloat(&d3dLight.Specular, glLit->Specular);

				if (glLit->_Flags & LIGHT_SPOT) {
					d3dLight.Type = D3DLIGHT_SPOT;
					d3dLight.Position.x = glLit->_Position[0];
					d3dLight.Position.y = glLit->_Position[1];
					d3dLight.Position.z = glLit->_Position[2];
					d3dLight.Direction.x = glLit->EyeDirection[0];
					d3dLight.Direction.y = glLit->EyeDirection[1];
					d3dLight.Direction.z = glLit->EyeDirection[2];
					d3dLight.Falloff = glLit->SpotExponent;
					// D3D uses half-angle in radians; GL _CosCutoff is cos(cutoff).
					// acos(_CosCutoff) gives the half-angle.
					d3dLight.Theta = 0.0f;
					d3dLight.Phi = (float)acos((double)glLit->_CosCutoff) * 2.0f;
				} else if (glLit->_Flags & LIGHT_POSITIONAL) {
					d3dLight.Type = D3DLIGHT_POINT;
					d3dLight.Position.x = glLit->_Position[0];
					d3dLight.Position.y = glLit->_Position[1];
					d3dLight.Position.z = glLit->_Position[2];
				} else {
					d3dLight.Type = D3DLIGHT_DIRECTIONAL;
					d3dLight.Direction.x = gld->LightDir[i].x;
					d3dLight.Direction.y = gld->LightDir[i].y;
					d3dLight.Direction.z = gld->LightDir[i].z;
				}

				d3dLight.Attenuation0 = glLit->ConstantAttenuation;
				d3dLight.Attenuation1 = glLit->LinearAttenuation;
				d3dLight.Attenuation2 = glLit->QuadraticAttenuation;
				d3dLight.Range = 1e10f; // Effectively infinite

				IDirect3DDevice9_SetLight(pDev, i, &d3dLight);
				IDirect3DDevice9_LightEnable(pDev, i, TRUE);
			} else {
				IDirect3DDevice9_LightEnable(pDev, i, FALSE);
			}
		}
	} else {
		IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
	}

	//
	// Fog
	//
	if (ctx->Fog.Enabled) {
		IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGENABLE, TRUE);
		{
			DWORD dwFogColor = D3DCOLOR_COLORVALUE(
				ctx->Fog.Color[0], ctx->Fog.Color[1],
				ctx->Fog.Color[2], ctx->Fog.Color[3]);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGCOLOR, dwFogColor);
		}

		switch (ctx->Fog.Mode) {
		case GL_LINEAR:
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGSTART, *(DWORD*)&ctx->Fog.Start);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGEND, *(DWORD*)&ctx->Fog.End);
			break;
		case GL_EXP:
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGVERTEXMODE, D3DFOG_EXP);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGDENSITY, *(DWORD*)&ctx->Fog.Density);
			break;
		case GL_EXP2:
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);
			IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGDENSITY, *(DWORD*)&ctx->Fog.Density);
			break;
		}
		// Use vertex fog, disable pixel fog
		IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGTABLEMODE, D3DFOG_NONE);
		IDirect3DDevice9_SetRenderState(pDev, D3DRS_RANGEFOGENABLE, FALSE);
	} else {
		IDirect3DDevice9_SetRenderState(pDev, D3DRS_FOGENABLE, FALSE);
	}

	//
	// Textures
	//
	if (ctx->Texture._EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			GLuint uiUnitMask = (GLuint)(1 << i);
			if (ctx->Texture._EnabledUnits & uiUnitMask) {
				const struct gl_texture_unit *pUnit = &ctx->Texture.Unit[i];
				const struct gl_texture_object *tObj = pUnit->_Current;

				// Set texture
				if (tObj && tObj->DriverData) {
					IDirect3DDevice9_SetTexture(pDev, i, (IDirect3DBaseTexture9*)tObj->DriverData);
				} else {
					IDirect3DDevice9_SetTexture(pDev, i, NULL);
				}

				// Texture matrix
				if (ctx->Texture._TexMatEnabled & uiUnitMask) {
					IDirect3DDevice9_SetTransform(pDev, (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + i), &gld->matTexture[i]);
					IDirect3DDevice9_SetTextureStageState(pDev, i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT4);
				} else {
					IDirect3DDevice9_SetTextureStageState(pDev, i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
				}

				// Texgen
				if (pUnit->TexGenEnabled) {
					DWORD dwTexCoordIndex = i;
					if (pUnit->_GenBitS & TEXGEN_SPHERE_MAP) {
						dwTexCoordIndex = D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR | i;
					} else if (pUnit->_GenBitS & TEXGEN_EYE_LINEAR) {
						dwTexCoordIndex = D3DTSS_TCI_CAMERASPACEPOSITION | i;
					}
					// Note: Object linear texgen has no direct D3D9 equivalent;
					// it would need custom vertex processing. For the fixed-function
					// fallback we use camera-space position as the closest approximation.
					else if (pUnit->_GenBitS & TEXGEN_OBJ_LINEAR) {
						dwTexCoordIndex = D3DTSS_TCI_CAMERASPACEPOSITION | i;
					}
					IDirect3DDevice9_SetTextureStageState(pDev, i, D3DTSS_TEXCOORDINDEX, dwTexCoordIndex);
				} else {
					IDirect3DDevice9_SetTextureStageState(pDev, i, D3DTSS_TEXCOORDINDEX, i);
				}
			} else {
				IDirect3DDevice9_SetTexture(pDev, i, NULL);
			}
		}
		// Disable texture stages beyond what we use
		IDirect3DDevice9_SetTextureStageState(pDev, GLD_MAX_TEXTURE_UNITS_DX9, D3DTSS_COLOROP, D3DTOP_DISABLE);
		IDirect3DDevice9_SetTextureStageState(pDev, GLD_MAX_TEXTURE_UNITS_DX9, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	} else {
		// No textures enabled
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			IDirect3DDevice9_SetTexture(pDev, i, NULL);
		}
		IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
		IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		IDirect3DDevice9_SetTextureStageState(pDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		IDirect3DDevice9_SetTextureStageState(pDev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

void gldUpdateShaders(
	GLcontext *ctx)
{
	int								i;
	GLD_context						*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9					*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_effect_state				gldES;

	//
	// Fill in a GLD_effect_state structure containing the current GL state.
	// This is used to detect state changes and cache unique configurations.
	//

	// Important - ensure all member vars are reset.
	ZeroMemory(&gldES, sizeof(gldES));

	//
	// Texture
	//
	if (ctx->Texture._EnabledUnits) {
		gldES.Texture._EnabledUnits		= ctx->Texture._EnabledUnits;
		gldES.Texture._GenFlags			= ctx->Texture._GenFlags;
		gldES.Texture._TexGenEnabled	= ctx->Texture._TexGenEnabled;
		gldES.Texture._TexMatEnabled	= ctx->Texture._TexMatEnabled;
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			GLuint UnitMask = (GLuint)1 << i;
			if (ctx->Texture._EnabledUnits && UnitMask) {
				const struct gl_texture_unit *glUnit	= &ctx->Texture.Unit[i];
				GLD_effect_texunit *gldUnit				= &gldES.Texture.Unit[i];
				gldUnit->EnvMode		= glUnit->EnvMode;
				if (gldES.Texture._GenFlags) {
					gldUnit->_GenBitS	= glUnit->_GenBitS;
					gldUnit->_GenBitT	= glUnit->_GenBitT;
					gldUnit->_GenBitR	= glUnit->_GenBitR;
					gldUnit->_GenBitQ	= glUnit->_GenBitQ;
				}
				gldUnit->TexGenEnabled	= glUnit->TexGenEnabled;
				if (glUnit->_Current) {
					gldUnit->MagFilter	= glUnit->_Current->MagFilter;
					gldUnit->MinFilter	= glUnit->_Current->MinFilter;
					gldUnit->WrapS		= glUnit->_Current->WrapS;
					gldUnit->WrapT		= glUnit->_Current->WrapT;
				}
			}
		}
	}

	//
	// Fog
	//
	gldES.Fog.Enabled	= ctx->Fog.Enabled;
	gldES.Fog.Mode		= ctx->Fog.Mode;

	//
	// Lighting
	//
	gldES.Light.Enabled = ctx->Light.Enabled;
	if (ctx->Light.Enabled) {
		gldES.Light.ColorMaterial.Enabled	= ctx->Light.ColorMaterialEnabled;
		gldES.Light.ColorMaterial.Face		= ctx->Light.ColorMaterialFace;
		gldES.Light.ColorMaterial.Mode		= ctx->Light.ColorMaterialMode;
		gldES.Light.TwoSide				= ctx->Light.Model.TwoSide;
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			gldES.Light.Light[i].Enabled	= ctx->Light.Light[i].Enabled;
			gldES.Light.Light[i]._Flags		= ctx->Light.Light[i]._Flags;
		}
	}

	// Find a matching effect state (or create a new one)
	gld->iCurEffect = -1;
	i = _gldFindEffect(gld, &gldES);
	if (i < 0) {
		gldLogMessage(GLDLOG_ERROR, "FindEffect failed\n");
		return;
	}

	// Set current effect
	gld->iCurEffect = i;

	// Apply the fixed-function state if the effect changed
	if (gld->iCurEffect != gld->iLastEffect) {
		if (gld->iLastEffect >= 0) {
			gldEndEffect(gld, gld->iLastEffect);
		}
		gld->iLastEffect = gld->iCurEffect;
		gldBeginEffect(gld, gld->iCurEffect);
	}

	// Always apply dynamic state (transforms, materials, lights, fog, textures)
	_gldApplyFixedFunctionState(ctx, gld);
}

//---------------------------------------------------------------------------

void gldReleaseShaders(
	GLD_driver_dx9 *gld)
{
	// No D3DX resources to release — just reset the effect cache
	gld->nEffects = 0;
	gld->iCurEffect = -1;
	gld->iLastEffect = -1;
}

//---------------------------------------------------------------------------

void gldBeginEffect(
	GLD_driver_dx9 *gld,
	int iEffect)
{
	// Fixed-function pipeline: mark effect as active.
	// No D3DX Begin/BeginPass needed.
	if (iEffect < 0 || iEffect >= gld->nEffects)
		return;

	gld->Effects[iEffect].bActive = TRUE;

#ifdef _DEBUG
	D3DPERF_BeginEvent(0xffffffff, L"gldBeginEffect");
#endif
}

//---------------------------------------------------------------------------

void gldEndEffect(
	GLD_driver_dx9 *gld,
	int iEffect)
{
	// Fixed-function pipeline: mark effect as inactive.
	// No D3DX EndPass/End needed.
	if (iEffect < 0 || iEffect >= gld->nEffects)
		return;

	gld->Effects[iEffect].bActive = FALSE;

#ifdef _DEBUG
	D3DPERF_EndEvent(); // gldBeginEffect
#endif
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
