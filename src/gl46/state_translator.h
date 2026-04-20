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
* Language:     ANSI C
* Environment:  Windows 9x/NT/2000/XP (Win32)
*
* Description:  DX9 render state to OpenGL 4.6 state translation.
*               Maps Direct3D 9 blend, depth, stencil, rasterizer, and
*               other render states to equivalent OpenGL API calls.
*
*********************************************************************************/

#ifndef __GL46_STATE_TRANSLATOR_H
#define __GL46_STATE_TRANSLATOR_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * D3DBLEND constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally so we don't require the DX9 SDK headers in all
 * build configurations.
 */
#ifndef D3DBLEND_ZERO
#define D3DBLEND_ZERO               1
#endif
#ifndef D3DBLEND_ONE
#define D3DBLEND_ONE                2
#endif
#ifndef D3DBLEND_SRCCOLOR
#define D3DBLEND_SRCCOLOR           3
#endif
#ifndef D3DBLEND_INVSRCCOLOR
#define D3DBLEND_INVSRCCOLOR        4
#endif
#ifndef D3DBLEND_SRCALPHA
#define D3DBLEND_SRCALPHA           5
#endif
#ifndef D3DBLEND_INVSRCALPHA
#define D3DBLEND_INVSRCALPHA        6
#endif
#ifndef D3DBLEND_DESTALPHA
#define D3DBLEND_DESTALPHA          7
#endif
#ifndef D3DBLEND_INVDESTALPHA
#define D3DBLEND_INVDESTALPHA       8
#endif
#ifndef D3DBLEND_DESTCOLOR
#define D3DBLEND_DESTCOLOR          9
#endif
#ifndef D3DBLEND_INVDESTCOLOR
#define D3DBLEND_INVDESTCOLOR       10
#endif
#ifndef D3DBLEND_SRCALPHASAT
#define D3DBLEND_SRCALPHASAT        11
#endif
#ifndef D3DBLEND_BLENDFACTOR
#define D3DBLEND_BLENDFACTOR        14
#endif
#ifndef D3DBLEND_INVBLENDFACTOR
#define D3DBLEND_INVBLENDFACTOR     15
#endif

/*
 * D3DBLENDOP constants from the DirectX 9 SDK (d3d9types.h).
 */
#ifndef D3DBLENDOP_ADD
#define D3DBLENDOP_ADD              1
#endif
#ifndef D3DBLENDOP_SUBTRACT
#define D3DBLENDOP_SUBTRACT         2
#endif
#ifndef D3DBLENDOP_REVSUBTRACT
#define D3DBLENDOP_REVSUBTRACT      3
#endif
#ifndef D3DBLENDOP_MIN
#define D3DBLENDOP_MIN              4
#endif
#ifndef D3DBLENDOP_MAX
#define D3DBLENDOP_MAX              5
#endif

/*
 * D3DCMPFUNC constants from the DirectX 9 SDK (d3d9types.h).
 * Used for depth test and stencil test comparison functions.
 */
#ifndef D3DCMP_NEVER
#define D3DCMP_NEVER                1
#endif
#ifndef D3DCMP_LESS
#define D3DCMP_LESS                 2
#endif
#ifndef D3DCMP_EQUAL
#define D3DCMP_EQUAL                3
#endif
#ifndef D3DCMP_LESSEQUAL
#define D3DCMP_LESSEQUAL            4
#endif
#ifndef D3DCMP_GREATER
#define D3DCMP_GREATER              5
#endif
#ifndef D3DCMP_NOTEQUAL
#define D3DCMP_NOTEQUAL             6
#endif
#ifndef D3DCMP_GREATEREQUAL
#define D3DCMP_GREATEREQUAL         7
#endif
#ifndef D3DCMP_ALWAYS
#define D3DCMP_ALWAYS               8
#endif

/*
 * D3DCULL constants from the DirectX 9 SDK (d3d9types.h).
 * D3DCULL_NONE disables culling, D3DCULL_CW culls clockwise-wound
 * faces, D3DCULL_CCW culls counter-clockwise-wound faces.
 */
#ifndef D3DCULL_NONE
#define D3DCULL_NONE                1
#endif
#ifndef D3DCULL_CW
#define D3DCULL_CW                  2
#endif
#ifndef D3DCULL_CCW
#define D3DCULL_CCW                 3
#endif

/*
 * D3DFILL constants from the DirectX 9 SDK (d3d9types.h).
 */
#ifndef D3DFILL_POINT
#define D3DFILL_POINT               1
#endif
#ifndef D3DFILL_WIREFRAME
#define D3DFILL_WIREFRAME           2
#endif
#ifndef D3DFILL_SOLID
#define D3DFILL_SOLID               3
#endif

/*
 * D3DCOLORWRITEENABLE bitmask bits from the DirectX 9 SDK (d3d9types.h).
 */
#ifndef D3DCOLORWRITEENABLE_RED
#define D3DCOLORWRITEENABLE_RED     (1L)
#endif
#ifndef D3DCOLORWRITEENABLE_GREEN
#define D3DCOLORWRITEENABLE_GREEN   (2L)
#endif
#ifndef D3DCOLORWRITEENABLE_BLUE
#define D3DCOLORWRITEENABLE_BLUE    (4L)
#endif
#ifndef D3DCOLORWRITEENABLE_ALPHA
#define D3DCOLORWRITEENABLE_ALPHA   (8L)
#endif

/*
 * D3DSTENCILOP constants from the DirectX 9 SDK (d3d9types.h).
 */
#ifndef D3DSTENCILOP_KEEP
#define D3DSTENCILOP_KEEP           1
#endif
#ifndef D3DSTENCILOP_ZERO
#define D3DSTENCILOP_ZERO           2
#endif
#ifndef D3DSTENCILOP_REPLACE
#define D3DSTENCILOP_REPLACE        3
#endif
#ifndef D3DSTENCILOP_INCRSAT
#define D3DSTENCILOP_INCRSAT        4
#endif
#ifndef D3DSTENCILOP_DECRSAT
#define D3DSTENCILOP_DECRSAT        5
#endif
#ifndef D3DSTENCILOP_INVERT
#define D3DSTENCILOP_INVERT         6
#endif
#ifndef D3DSTENCILOP_INCR
#define D3DSTENCILOP_INCR           7
#endif
#ifndef D3DSTENCILOP_DECR
#define D3DSTENCILOP_DECR           8
#endif

/*
 * D3DFOGMODE constants from the DirectX 9 SDK (d3d9types.h).
 * Used for table fog mode (D3DRS_FOGTABLEMODE).
 */
#ifndef D3DFOG_NONE
#define D3DFOG_NONE                 0
#endif
#ifndef D3DFOG_EXP
#define D3DFOG_EXP                  1
#endif
#ifndef D3DFOG_EXP2
#define D3DFOG_EXP2                 2
#endif
#ifndef D3DFOG_LINEAR
#define D3DFOG_LINEAR               3
#endif

/*
 * GLD_fogState — captures DX9 fog render state for the Fixed_Function_Emulator.
 *
 * These parameters are not translated to GL calls directly (GL_FOG is
 * removed from the core profile).  Instead, the Fixed_Function_Emulator
 * reads this struct and generates GLSL code to compute fog blending in
 * the fragment shader.
 */
typedef struct {
    BOOL    enabled;        /* D3DRS_FOGENABLE                          */
    DWORD   fogColor;       /* D3DRS_FOGCOLOR — D3DCOLOR (ARGB packed)  */
    float   fogStart;       /* D3DRS_FOGSTART                           */
    float   fogEnd;         /* D3DRS_FOGEND                             */
    float   fogDensity;     /* D3DRS_FOGDENSITY                         */
    DWORD   fogMode;        /* D3DRS_FOGTABLEMODE — D3DFOGMODE enum:
                               0=NONE, 1=EXP, 2=EXP2, 3=LINEAR        */
} GLD_fogState;

/*
 * GLD_alphaTestState — captures DX9 alpha test render state for the
 * Fixed_Function_Emulator.
 *
 * GL_ALPHA_TEST is removed from the OpenGL core profile, so the
 * Fixed_Function_Emulator injects a `discard` statement in the
 * fragment shader based on these parameters.
 */
typedef struct {
    BOOL    enabled;        /* D3DRS_ALPHATESTENABLE                    */
    DWORD   alphaFunc;      /* D3DRS_ALPHAFUNC — D3DCMPFUNC enum (1–8) */
    float   alphaRef;       /* D3DRS_ALPHAREF — normalised [0.0, 1.0]  */
} GLD_alphaTestState;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Map a D3DBLEND enumeration value to the equivalent OpenGL blend factor.
 *
 * Parameters:
 *   d3dBlend — a D3DBLEND_* constant (1–15).
 *
 * Returns:
 *   The corresponding GL blend factor enum (e.g. GL_ZERO, GL_ONE,
 *   GL_SRC_ALPHA).  Returns GL_ONE for unrecognised values and logs
 *   a warning.
 */
GLenum gldMapD3DBlendFactor(DWORD d3dBlend);

/*
 * Map a D3DBLENDOP enumeration value to the equivalent OpenGL blend
 * equation.
 *
 * Parameters:
 *   d3dBlendOp — a D3DBLENDOP_* constant (1–5).
 *
 * Returns:
 *   The corresponding GL blend equation enum (e.g. GL_FUNC_ADD,
 *   GL_FUNC_SUBTRACT).  Returns GL_FUNC_ADD for unrecognised values
 *   and logs a warning.
 */
GLenum gldMapD3DBlendOp(DWORD d3dBlendOp);

/*
 * Translate DX9 blend render state to OpenGL blend state.
 *
 * Configures GL_BLEND enable/disable, blend function, blend equation,
 * and optionally separate alpha blend via glBlendFuncSeparate /
 * glBlendEquationSeparate.
 *
 * Parameters:
 *   blendEnable          — D3DRS_ALPHABLENDENABLE (TRUE/FALSE).
 *   srcBlend             — D3DRS_SRCBLEND (D3DBLEND_* constant).
 *   destBlend            — D3DRS_DESTBLEND (D3DBLEND_* constant).
 *   blendOp              — D3DRS_BLENDOP (D3DBLENDOP_* constant).
 *   separateAlphaEnable  — D3DRS_SEPARATEALPHABLENDENABLE (TRUE/FALSE).
 *   srcBlendAlpha        — D3DRS_SRCBLENDALPHA (D3DBLEND_* constant).
 *   destBlendAlpha       — D3DRS_DESTBLENDALPHA (D3DBLEND_* constant).
 *   blendOpAlpha         — D3DRS_BLENDOPALPHA (D3DBLENDOP_* constant).
 */
void gldSetBlendState(BOOL blendEnable,
                      DWORD srcBlend, DWORD destBlend, DWORD blendOp,
                      BOOL separateAlphaEnable,
                      DWORD srcBlendAlpha, DWORD destBlendAlpha,
                      DWORD blendOpAlpha);

/*
 * Map a D3DCMPFUNC enumeration value to the equivalent OpenGL comparison
 * function.
 *
 * Parameters:
 *   d3dCmpFunc — a D3DCMP_* constant (1–8).
 *
 * Returns:
 *   The corresponding GL comparison function enum (e.g. GL_LESS,
 *   GL_LEQUAL).  Returns GL_ALWAYS for unrecognised values and logs
 *   a warning.
 */
GLenum gldMapD3DCmpFunc(DWORD d3dCmpFunc);

/*
 * Map a D3DSTENCILOP enumeration value to the equivalent OpenGL stencil
 * operation.
 *
 * Parameters:
 *   d3dStencilOp — a D3DSTENCILOP_* constant (1–8).
 *
 * Returns:
 *   The corresponding GL stencil operation enum (e.g. GL_KEEP,
 *   GL_INCR_WRAP).  Returns GL_KEEP for unrecognised values and logs
 *   a warning.
 */
GLenum gldMapD3DStencilOp(DWORD d3dStencilOp);

/*
 * Translate DX9 depth render state to OpenGL depth state.
 *
 * Configures GL_DEPTH_TEST enable/disable, depth write mask, and
 * depth comparison function.
 *
 * Parameters:
 *   depthEnable      — D3DRS_ZENABLE (TRUE/FALSE).
 *   depthWriteEnable — D3DRS_ZWRITEENABLE (TRUE/FALSE).
 *   depthFunc        — D3DRS_ZFUNC (D3DCMPFUNC constant).
 */
void gldSetDepthState(BOOL depthEnable, BOOL depthWriteEnable, DWORD depthFunc);

/*
 * Translate DX9 stencil render state to OpenGL stencil state.
 *
 * Configures GL_STENCIL_TEST enable/disable, stencil function,
 * stencil operations, and stencil write mask.  When two-sided stencil
 * is enabled, uses glStencilFuncSeparate and glStencilOpSeparate to
 * configure separate front and back stencil state.
 *
 * Parameters:
 *   stencilEnable    — D3DRS_STENCILENABLE (TRUE/FALSE).
 *   stencilFunc      — D3DRS_STENCILFUNC (D3DCMPFUNC constant).
 *   stencilRef       — D3DRS_STENCILREF (reference value).
 *   stencilMask      — D3DRS_STENCILMASK (read mask).
 *   stencilWriteMask — D3DRS_STENCILWRITEMASK (write mask).
 *   stencilFail      — D3DRS_STENCILFAIL (D3DSTENCILOP constant).
 *   stencilZFail     — D3DRS_STENCILZFAIL (D3DSTENCILOP constant).
 *   stencilPass      — D3DRS_STENCILPASS (D3DSTENCILOP constant).
 *   twoSidedEnable   — D3DRS_TWOSIDEDSTENCILMODE (TRUE/FALSE).
 *   ccwStencilFunc   — D3DRS_CCW_STENCILFUNC (D3DCMPFUNC constant).
 *   ccwStencilFail   — D3DRS_CCW_STENCILFAIL (D3DSTENCILOP constant).
 *   ccwStencilZFail  — D3DRS_CCW_STENCILZFAIL (D3DSTENCILOP constant).
 *   ccwStencilPass   — D3DRS_CCW_STENCILPASS (D3DSTENCILOP constant).
 */
void gldSetStencilState(BOOL stencilEnable,
                        DWORD stencilFunc, DWORD stencilRef, DWORD stencilMask,
                        DWORD stencilWriteMask,
                        DWORD stencilFail, DWORD stencilZFail, DWORD stencilPass,
                        BOOL twoSidedEnable,
                        DWORD ccwStencilFunc, DWORD ccwStencilFail,
                        DWORD ccwStencilZFail, DWORD ccwStencilPass);

/*
 * Translate DX9 cull mode to OpenGL face culling state.
 *
 * DX9 D3DCULL_CW/CCW specifies which faces to *cull* (remove), and
 * the winding convention is inverted relative to OpenGL due to the
 * coordinate system handedness difference (DX9 left-handed vs OpenGL
 * right-handed).
 *
 * Mapping:
 *   D3DCULL_NONE → glDisable(GL_CULL_FACE)
 *   D3DCULL_CW   → glEnable(GL_CULL_FACE), glFrontFace(GL_CCW), glCullFace(GL_BACK)
 *   D3DCULL_CCW  → glEnable(GL_CULL_FACE), glFrontFace(GL_CW), glCullFace(GL_BACK)
 *
 * Parameters:
 *   cullMode — D3DCULL_NONE (1), D3DCULL_CW (2), or D3DCULL_CCW (3).
 */
void gldSetCullMode(DWORD cullMode);

/*
 * Translate DX9 fill mode to OpenGL polygon mode.
 *
 * Mapping:
 *   D3DFILL_POINT     → glPolygonMode(GL_FRONT_AND_BACK, GL_POINT)
 *   D3DFILL_WIREFRAME → glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
 *   D3DFILL_SOLID     → glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
 *
 * Parameters:
 *   fillMode — D3DFILL_POINT (1), D3DFILL_WIREFRAME (2), or D3DFILL_SOLID (3).
 */
void gldSetFillMode(DWORD fillMode);

/*
 * Translate DX9 scissor test state to OpenGL scissor state.
 *
 * When scissorEnable is FALSE, GL_SCISSOR_TEST is disabled and no
 * further scissor configuration is performed.
 *
 * When scissorEnable is TRUE, GL_SCISSOR_TEST is enabled and glScissor
 * is called with the rectangle coordinates.  If clip control is
 * unavailable (clipControlAvailable is FALSE), the Y coordinate is
 * flipped from DX9 top-left origin to OpenGL bottom-left origin using
 * gldFlipViewportY.
 *
 * Parameters:
 *   scissorEnable        — D3DRS_SCISSORTESTENABLE (TRUE/FALSE).
 *   x                    — scissor rect left edge in pixels.
 *   y                    — scissor rect top edge in pixels (DX9 convention).
 *   width                — scissor rect width in pixels.
 *   height               — scissor rect height in pixels.
 *   clipControlAvailable — TRUE if glClipControl(GL_UPPER_LEFT) is active.
 *   windowHeight         — total window height in pixels (for Y-flip fallback).
 */
void gldSetScissorState(BOOL scissorEnable, int x, int y, int width, int height,
                        BOOL clipControlAvailable, int windowHeight);

/*
 * Translate DX9 color write enable mask to OpenGL color mask.
 *
 * Decodes the D3DCOLORWRITEENABLE bitmask (RED=1, GREEN=2, BLUE=4,
 * ALPHA=8) into individual boolean flags for glColorMask.
 *
 * Parameters:
 *   colorWriteEnable — D3DRS_COLORWRITEENABLE bitmask (0–15).
 */
void gldSetColorWriteMask(DWORD colorWriteEnable);

/*
 * Capture DX9 fog render state into a GLD_fogState struct.
 *
 * This function does not issue any GL calls.  The captured state is
 * consumed by the Fixed_Function_Emulator when it generates or selects
 * a shader variant that includes fog computation.
 *
 * Parameters:
 *   outState   — pointer to the GLD_fogState struct to populate.
 *   fogEnable  — D3DRS_FOGENABLE (TRUE/FALSE).
 *   fogColor   — D3DRS_FOGCOLOR (D3DCOLOR, ARGB packed DWORD).
 *   fogStart   — D3DRS_FOGSTART (float, linear fog near distance).
 *   fogEnd     — D3DRS_FOGEND (float, linear fog far distance).
 *   fogDensity — D3DRS_FOGDENSITY (float, exponential fog density).
 *   fogMode    — D3DRS_FOGTABLEMODE (D3DFOGMODE: 0=NONE, 1=EXP,
 *                2=EXP2, 3=LINEAR).
 */
void gldSetFogState(GLD_fogState* outState, BOOL fogEnable, DWORD fogColor,
                    float fogStart, float fogEnd, float fogDensity, DWORD fogMode);

/*
 * Capture DX9 alpha test render state into a GLD_alphaTestState struct.
 *
 * This function does not issue any GL calls.  The captured state is
 * consumed by the Fixed_Function_Emulator, which injects a `discard`
 * statement in the fragment shader when alpha test is enabled (since
 * GL_ALPHA_TEST is removed from the OpenGL core profile).
 *
 * Parameters:
 *   outState        — pointer to the GLD_alphaTestState struct to populate.
 *   alphaTestEnable — D3DRS_ALPHATESTENABLE (TRUE/FALSE).
 *   alphaFunc       — D3DRS_ALPHAFUNC (D3DCMPFUNC constant, 1–8).
 *   alphaRef        — D3DRS_ALPHAREF normalised to [0.0, 1.0].
 */
void gldSetAlphaTestState(GLD_alphaTestState* outState, BOOL alphaTestEnable,
                          DWORD alphaFunc, float alphaRef);

/*
 * Set the OpenGL point size and store the value for shader upload.
 *
 * Calls glPointSize to set the GL state.  The point size value will
 * also need to be uploaded as a uniform to the active vertex shader
 * (via gl_PointSize) by the Fixed_Function_Emulator, but for now this
 * function only sets the GL-side state.
 *
 * Parameters:
 *   pointSize — D3DRS_POINTSIZE value (float, in pixels).
 */
void gldSetPointSize(float pointSize);

#ifdef  __cplusplus
}
#endif

#endif
