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

#define STRICT
#include <windows.h>

#include <glad/gl.h>
#include "state_translator.h"
#include "coordinate_adapter.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

/*****************************************************************************
 * D3DBLEND → GL blend factor mapping table.
 *
 * Indexed by D3DBLEND_* value.  Entries 0, 12, and 13 are unused
 * (D3DBLEND values skip from 11 to 14), so they default to GL_ONE.
 * The table is sized to 16 entries to cover the full range 0–15.
 *****************************************************************************/
static const GLenum s_blendFactorTable[16] = {
    /* [0]  (unused)                */ GL_ONE,
    /* [1]  D3DBLEND_ZERO           */ GL_ZERO,
    /* [2]  D3DBLEND_ONE            */ GL_ONE,
    /* [3]  D3DBLEND_SRCCOLOR       */ GL_SRC_COLOR,
    /* [4]  D3DBLEND_INVSRCCOLOR    */ GL_ONE_MINUS_SRC_COLOR,
    /* [5]  D3DBLEND_SRCALPHA       */ GL_SRC_ALPHA,
    /* [6]  D3DBLEND_INVSRCALPHA    */ GL_ONE_MINUS_SRC_ALPHA,
    /* [7]  D3DBLEND_DESTALPHA      */ GL_DST_ALPHA,
    /* [8]  D3DBLEND_INVDESTALPHA   */ GL_ONE_MINUS_DST_ALPHA,
    /* [9]  D3DBLEND_DESTCOLOR      */ GL_DST_COLOR,
    /* [10] D3DBLEND_INVDESTCOLOR   */ GL_ONE_MINUS_DST_COLOR,
    /* [11] D3DBLEND_SRCALPHASAT    */ GL_SRC_ALPHA_SATURATE,
    /* [12] (unused)                */ GL_ONE,
    /* [13] (unused)                */ GL_ONE,
    /* [14] D3DBLEND_BLENDFACTOR    */ GL_CONSTANT_COLOR,
    /* [15] D3DBLEND_INVBLENDFACTOR */ GL_ONE_MINUS_CONSTANT_COLOR,
};

// ***********************************************************************

/*****************************************************************************
 * D3DBLENDOP → GL blend equation mapping table.
 *
 * Indexed by D3DBLENDOP_* value (1–5).  Entry 0 is unused and defaults
 * to GL_FUNC_ADD.
 *****************************************************************************/
static const GLenum s_blendOpTable[6] = {
    /* [0] (unused)                 */ GL_FUNC_ADD,
    /* [1] D3DBLENDOP_ADD           */ GL_FUNC_ADD,
    /* [2] D3DBLENDOP_SUBTRACT      */ GL_FUNC_SUBTRACT,
    /* [3] D3DBLENDOP_REVSUBTRACT   */ GL_FUNC_REVERSE_SUBTRACT,
    /* [4] D3DBLENDOP_MIN           */ GL_MIN,
    /* [5] D3DBLENDOP_MAX           */ GL_MAX,
};

// ***********************************************************************

GLenum gldMapD3DBlendFactor(DWORD d3dBlend)
{
    if (d3dBlend < 16) {
        GLenum factor = s_blendFactorTable[d3dBlend];
        /* Warn on unused/gap entries (0, 12, 13) */
        if (d3dBlend == 0 || d3dBlend == 12 || d3dBlend == 13) {
            gldLogPrintf(GLDLOG_WARN,
                "gldMapD3DBlendFactor: unrecognised D3DBLEND value %lu, "
                "defaulting to GL_ONE",
                (unsigned long)d3dBlend);
        }
        return factor;
    }

    gldLogPrintf(GLDLOG_WARN,
        "gldMapD3DBlendFactor: D3DBLEND value %lu out of range, "
        "defaulting to GL_ONE",
        (unsigned long)d3dBlend);
    return GL_ONE;
}

// ***********************************************************************

GLenum gldMapD3DBlendOp(DWORD d3dBlendOp)
{
    if (d3dBlendOp >= 1 && d3dBlendOp <= 5) {
        return s_blendOpTable[d3dBlendOp];
    }

    gldLogPrintf(GLDLOG_WARN,
        "gldMapD3DBlendOp: unrecognised D3DBLENDOP value %lu, "
        "defaulting to GL_FUNC_ADD",
        (unsigned long)d3dBlendOp);
    return GL_FUNC_ADD;
}

// ***********************************************************************

/*****************************************************************************
 * gldSetBlendState
 *
 * Translate DX9 blend render state to OpenGL blend state.
 *
 * When blendEnable is FALSE, GL_BLEND is disabled and no further blend
 * configuration is performed.
 *
 * When separateAlphaEnable is TRUE, glBlendFuncSeparate and
 * glBlendEquationSeparate are used so that the RGB and alpha channels
 * can have independent blend factors and equations.  Otherwise the
 * simpler glBlendFunc / glBlendEquation path is used.
 *****************************************************************************/
void gldSetBlendState(BOOL blendEnable,
                      DWORD srcBlend, DWORD destBlend, DWORD blendOp,
                      BOOL separateAlphaEnable,
                      DWORD srcBlendAlpha, DWORD destBlendAlpha,
                      DWORD blendOpAlpha)
{
    if (!blendEnable) {
        glDisable(GL_BLEND);
        GLD_CHECK_GL("glDisable(GL_BLEND)", "SetRenderState");
        return;
    }

    glEnable(GL_BLEND);
    GLD_CHECK_GL("glEnable(GL_BLEND)", "SetRenderState");

    if (separateAlphaEnable) {
        /* Separate RGB and alpha blend factors */
        GLenum glSrcRGB   = gldMapD3DBlendFactor(srcBlend);
        GLenum glDstRGB   = gldMapD3DBlendFactor(destBlend);
        GLenum glSrcAlpha = gldMapD3DBlendFactor(srcBlendAlpha);
        GLenum glDstAlpha = gldMapD3DBlendFactor(destBlendAlpha);
        GLenum glOpRGB    = gldMapD3DBlendOp(blendOp);
        GLenum glOpAlpha  = gldMapD3DBlendOp(blendOpAlpha);

        glBlendFuncSeparate(glSrcRGB, glDstRGB, glSrcAlpha, glDstAlpha);
        GLD_CHECK_GL("glBlendFuncSeparate", "SetRenderState");

        glBlendEquationSeparate(glOpRGB, glOpAlpha);
        GLD_CHECK_GL("glBlendEquationSeparate", "SetRenderState");
    } else {
        /* Unified blend factors for all channels */
        GLenum glSrc = gldMapD3DBlendFactor(srcBlend);
        GLenum glDst = gldMapD3DBlendFactor(destBlend);
        GLenum glOp  = gldMapD3DBlendOp(blendOp);

        glBlendFunc(glSrc, glDst);
        GLD_CHECK_GL("glBlendFunc", "SetRenderState");

        glBlendEquation(glOp);
        GLD_CHECK_GL("glBlendEquation", "SetRenderState");
    }
}

// ***********************************************************************

/*****************************************************************************
 * D3DCMPFUNC → GL comparison function mapping table.
 *
 * Indexed by D3DCMP_* value (1–8).  Entry 0 is unused and defaults
 * to GL_ALWAYS.
 *****************************************************************************/
static const GLenum s_cmpFuncTable[9] = {
    /* [0] (unused)                 */ GL_ALWAYS,
    /* [1] D3DCMP_NEVER             */ GL_NEVER,
    /* [2] D3DCMP_LESS              */ GL_LESS,
    /* [3] D3DCMP_EQUAL             */ GL_EQUAL,
    /* [4] D3DCMP_LESSEQUAL         */ GL_LEQUAL,
    /* [5] D3DCMP_GREATER           */ GL_GREATER,
    /* [6] D3DCMP_NOTEQUAL          */ GL_NOTEQUAL,
    /* [7] D3DCMP_GREATEREQUAL      */ GL_GEQUAL,
    /* [8] D3DCMP_ALWAYS            */ GL_ALWAYS,
};

// ***********************************************************************

/*****************************************************************************
 * D3DSTENCILOP → GL stencil operation mapping table.
 *
 * Indexed by D3DSTENCILOP_* value (1–8).  Entry 0 is unused and
 * defaults to GL_KEEP.
 *****************************************************************************/
static const GLenum s_stencilOpTable[9] = {
    /* [0] (unused)                    */ GL_KEEP,
    /* [1] D3DSTENCILOP_KEEP           */ GL_KEEP,
    /* [2] D3DSTENCILOP_ZERO           */ GL_ZERO,
    /* [3] D3DSTENCILOP_REPLACE        */ GL_REPLACE,
    /* [4] D3DSTENCILOP_INCRSAT        */ GL_INCR,
    /* [5] D3DSTENCILOP_DECRSAT        */ GL_DECR,
    /* [6] D3DSTENCILOP_INVERT         */ GL_INVERT,
    /* [7] D3DSTENCILOP_INCR           */ GL_INCR_WRAP,
    /* [8] D3DSTENCILOP_DECR           */ GL_DECR_WRAP,
};

// ***********************************************************************

GLenum gldMapD3DCmpFunc(DWORD d3dCmpFunc)
{
    if (d3dCmpFunc >= 1 && d3dCmpFunc <= 8) {
        return s_cmpFuncTable[d3dCmpFunc];
    }

    gldLogPrintf(GLDLOG_WARN,
        "gldMapD3DCmpFunc: unrecognised D3DCMPFUNC value %lu, "
        "defaulting to GL_ALWAYS",
        (unsigned long)d3dCmpFunc);
    return GL_ALWAYS;
}

// ***********************************************************************

GLenum gldMapD3DStencilOp(DWORD d3dStencilOp)
{
    if (d3dStencilOp >= 1 && d3dStencilOp <= 8) {
        return s_stencilOpTable[d3dStencilOp];
    }

    gldLogPrintf(GLDLOG_WARN,
        "gldMapD3DStencilOp: unrecognised D3DSTENCILOP value %lu, "
        "defaulting to GL_KEEP",
        (unsigned long)d3dStencilOp);
    return GL_KEEP;
}

// ***********************************************************************

/*****************************************************************************
 * gldSetDepthState
 *
 * Translate DX9 depth render state to OpenGL depth state.
 *
 * When depthEnable is FALSE, GL_DEPTH_TEST is disabled and no further
 * depth configuration is performed.
 *
 * When depthEnable is TRUE, GL_DEPTH_TEST is enabled, the depth write
 * mask is set via glDepthMask, and the depth comparison function is
 * set via glDepthFunc.
 *****************************************************************************/
void gldSetDepthState(BOOL depthEnable, BOOL depthWriteEnable, DWORD depthFunc)
{
    if (!depthEnable) {
        glDisable(GL_DEPTH_TEST);
        GLD_CHECK_GL("glDisable(GL_DEPTH_TEST)", "SetRenderState");
        return;
    }

    glEnable(GL_DEPTH_TEST);
    GLD_CHECK_GL("glEnable(GL_DEPTH_TEST)", "SetRenderState");

    glDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
    GLD_CHECK_GL("glDepthMask", "SetRenderState");

    glDepthFunc(gldMapD3DCmpFunc(depthFunc));
    GLD_CHECK_GL("glDepthFunc", "SetRenderState");
}

// ***********************************************************************

/*****************************************************************************
 * gldSetStencilState
 *
 * Translate DX9 stencil render state to OpenGL stencil state.
 *
 * When stencilEnable is FALSE, GL_STENCIL_TEST is disabled and no
 * further stencil configuration is performed.
 *
 * When twoSidedEnable is TRUE, glStencilFuncSeparate and
 * glStencilOpSeparate are used to configure separate front-face and
 * back-face stencil operations.  The front face uses the primary
 * stencil parameters and the back face uses the CCW parameters.
 *
 * When twoSidedEnable is FALSE, the simpler glStencilFunc /
 * glStencilOp / glStencilMask path is used for both faces.
 *****************************************************************************/
void gldSetStencilState(BOOL stencilEnable,
                        DWORD stencilFunc, DWORD stencilRef, DWORD stencilMask,
                        DWORD stencilWriteMask,
                        DWORD stencilFail, DWORD stencilZFail, DWORD stencilPass,
                        BOOL twoSidedEnable,
                        DWORD ccwStencilFunc, DWORD ccwStencilFail,
                        DWORD ccwStencilZFail, DWORD ccwStencilPass)
{
    if (!stencilEnable) {
        glDisable(GL_STENCIL_TEST);
        GLD_CHECK_GL("glDisable(GL_STENCIL_TEST)", "SetRenderState");
        return;
    }

    glEnable(GL_STENCIL_TEST);
    GLD_CHECK_GL("glEnable(GL_STENCIL_TEST)", "SetRenderState");

    if (twoSidedEnable) {
        /* Front face (CW in DX9 convention) stencil state */
        glStencilFuncSeparate(GL_FRONT,
                              gldMapD3DCmpFunc(stencilFunc),
                              (GLint)stencilRef,
                              (GLuint)stencilMask);
        GLD_CHECK_GL("glStencilFuncSeparate(GL_FRONT)", "SetRenderState");

        glStencilOpSeparate(GL_FRONT,
                            gldMapD3DStencilOp(stencilFail),
                            gldMapD3DStencilOp(stencilZFail),
                            gldMapD3DStencilOp(stencilPass));
        GLD_CHECK_GL("glStencilOpSeparate(GL_FRONT)", "SetRenderState");

        /* Back face (CCW in DX9 convention) stencil state */
        glStencilFuncSeparate(GL_BACK,
                              gldMapD3DCmpFunc(ccwStencilFunc),
                              (GLint)stencilRef,
                              (GLuint)stencilMask);
        GLD_CHECK_GL("glStencilFuncSeparate(GL_BACK)", "SetRenderState");

        glStencilOpSeparate(GL_BACK,
                            gldMapD3DStencilOp(ccwStencilFail),
                            gldMapD3DStencilOp(ccwStencilZFail),
                            gldMapD3DStencilOp(ccwStencilPass));
        GLD_CHECK_GL("glStencilOpSeparate(GL_BACK)", "SetRenderState");

        /* Write mask applies to both faces */
        glStencilMask((GLuint)stencilWriteMask);
        GLD_CHECK_GL("glStencilMask", "SetRenderState");
    } else {
        /* Single-sided stencil: applies to both faces */
        glStencilFunc(gldMapD3DCmpFunc(stencilFunc),
                      (GLint)stencilRef,
                      (GLuint)stencilMask);
        GLD_CHECK_GL("glStencilFunc", "SetRenderState");

        glStencilOp(gldMapD3DStencilOp(stencilFail),
                    gldMapD3DStencilOp(stencilZFail),
                    gldMapD3DStencilOp(stencilPass));
        GLD_CHECK_GL("glStencilOp", "SetRenderState");

        glStencilMask((GLuint)stencilWriteMask);
        GLD_CHECK_GL("glStencilMask", "SetRenderState");
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetCullMode
 *
 * Translate DX9 cull mode to OpenGL face culling state.
 *
 * DX9 D3DCULL_CW/CCW specifies which faces to *cull* (remove).
 * The winding convention is inverted relative to OpenGL due to the
 * coordinate system handedness difference:
 *
 *   D3DCULL_NONE (1) → disable face culling entirely.
 *   D3DCULL_CW   (2) → cull CW-wound faces in DX9.  Due to the
 *                       handedness inversion, CW in DX9 corresponds
 *                       to CCW in OpenGL.  We set glFrontFace(GL_CCW)
 *                       so that CCW faces are "front", then cull the
 *                       back faces (which are the CW-wound ones in
 *                       OpenGL, i.e. the DX9-CW faces).
 *   D3DCULL_CCW  (3) → cull CCW-wound faces in DX9.  The inversion
 *                       means CCW in DX9 maps to CW in OpenGL.  We
 *                       set glFrontFace(GL_CW) so that CW faces are
 *                       "front", then cull the back faces (which are
 *                       the CCW-wound ones in OpenGL, i.e. the
 *                       DX9-CCW faces).
 *****************************************************************************/
void gldSetCullMode(DWORD cullMode)
{
    switch (cullMode) {
    case D3DCULL_NONE:
        glDisable(GL_CULL_FACE);
        GLD_CHECK_GL("glDisable(GL_CULL_FACE)", "SetRenderState");
        break;

    case D3DCULL_CW:
        glEnable(GL_CULL_FACE);
        GLD_CHECK_GL("glEnable(GL_CULL_FACE)", "SetRenderState");
        glFrontFace(GL_CCW);
        GLD_CHECK_GL("glFrontFace(GL_CCW)", "SetRenderState");
        glCullFace(GL_BACK);
        GLD_CHECK_GL("glCullFace(GL_BACK)", "SetRenderState");
        break;

    case D3DCULL_CCW:
        glEnable(GL_CULL_FACE);
        GLD_CHECK_GL("glEnable(GL_CULL_FACE)", "SetRenderState");
        glFrontFace(GL_CW);
        GLD_CHECK_GL("glFrontFace(GL_CW)", "SetRenderState");
        glCullFace(GL_BACK);
        GLD_CHECK_GL("glCullFace(GL_BACK)", "SetRenderState");
        break;

    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldSetCullMode: unrecognised D3DCULL value %lu, "
            "defaulting to D3DCULL_NONE (no culling)",
            (unsigned long)cullMode);
        glDisable(GL_CULL_FACE);
        GLD_CHECK_GL("glDisable(GL_CULL_FACE)", "SetRenderState");
        break;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetFillMode
 *
 * Translate DX9 fill mode to OpenGL polygon rasterization mode.
 *
 *   D3DFILL_POINT     (1) → GL_POINT
 *   D3DFILL_WIREFRAME (2) → GL_LINE
 *   D3DFILL_SOLID     (3) → GL_FILL
 *****************************************************************************/
void gldSetFillMode(DWORD fillMode)
{
    switch (fillMode) {
    case D3DFILL_POINT:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        GLD_CHECK_GL("glPolygonMode(GL_POINT)", "SetRenderState");
        break;

    case D3DFILL_WIREFRAME:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GLD_CHECK_GL("glPolygonMode(GL_LINE)", "SetRenderState");
        break;

    case D3DFILL_SOLID:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        GLD_CHECK_GL("glPolygonMode(GL_FILL)", "SetRenderState");
        break;

    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldSetFillMode: unrecognised D3DFILLMODE value %lu, "
            "defaulting to D3DFILL_SOLID",
            (unsigned long)fillMode);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        GLD_CHECK_GL("glPolygonMode(GL_FILL)", "SetRenderState");
        break;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetScissorState
 *
 * Translate DX9 scissor test state to OpenGL scissor state.
 *
 * When scissorEnable is FALSE, GL_SCISSOR_TEST is disabled.
 *
 * When scissorEnable is TRUE, GL_SCISSOR_TEST is enabled and glScissor
 * is called with the rectangle coordinates.  If clip control is
 * unavailable, the Y coordinate is flipped from DX9 top-left origin
 * to OpenGL bottom-left origin using gldFlipViewportY.
 *****************************************************************************/
void gldSetScissorState(BOOL scissorEnable, int x, int y, int width, int height,
                        BOOL clipControlAvailable, int windowHeight)
{
    if (!scissorEnable) {
        glDisable(GL_SCISSOR_TEST);
        GLD_CHECK_GL("glDisable(GL_SCISSOR_TEST)", "SetRenderState");
        return;
    }

    glEnable(GL_SCISSOR_TEST);
    GLD_CHECK_GL("glEnable(GL_SCISSOR_TEST)", "SetRenderState");

    if (!clipControlAvailable) {
        /*
         * Clip control is unavailable — OpenGL uses bottom-left origin.
         * Flip Y from DX9 top-left to OpenGL bottom-left.
         */
        gldFlipViewportY(windowHeight, &y, &height);
    }

    glScissor(x, y, width, height);
    GLD_CHECK_GL("glScissor", "SetRenderState");
}

// ***********************************************************************

/*****************************************************************************
 * gldSetColorWriteMask
 *
 * Translate DX9 color write enable bitmask to OpenGL color mask.
 *
 * D3DCOLORWRITEENABLE bits:
 *   RED   = 1 (bit 0)
 *   GREEN = 2 (bit 1)
 *   BLUE  = 4 (bit 2)
 *   ALPHA = 8 (bit 3)
 *
 * Each bit is decoded to a GL_TRUE/GL_FALSE flag for glColorMask.
 *****************************************************************************/
void gldSetColorWriteMask(DWORD colorWriteEnable)
{
    GLboolean r = (colorWriteEnable & D3DCOLORWRITEENABLE_RED)   ? GL_TRUE : GL_FALSE;
    GLboolean g = (colorWriteEnable & D3DCOLORWRITEENABLE_GREEN) ? GL_TRUE : GL_FALSE;
    GLboolean b = (colorWriteEnable & D3DCOLORWRITEENABLE_BLUE)  ? GL_TRUE : GL_FALSE;
    GLboolean a = (colorWriteEnable & D3DCOLORWRITEENABLE_ALPHA) ? GL_TRUE : GL_FALSE;

    glColorMask(r, g, b, a);
    GLD_CHECK_GL("glColorMask", "SetRenderState");
}

// ***********************************************************************

/*****************************************************************************
 * gldSetFogState
 *
 * Capture DX9 fog render state into a GLD_fogState struct for the
 * Fixed_Function_Emulator to consume as shader uniforms.
 *
 * No GL calls are issued here — GL_FOG is removed from the core
 * profile.  The Fixed_Function_Emulator reads this struct when
 * generating or selecting a shader variant that includes fog
 * computation in the fragment shader.
 *
 * Fog modes (D3DFOGMODE):
 *   0 = D3DFOG_NONE    — fog disabled regardless of fogEnable
 *   1 = D3DFOG_EXP     — exponential fog:  f = exp(-density * d)
 *   2 = D3DFOG_EXP2    — exponential²:     f = exp(-(density * d)²)
 *   3 = D3DFOG_LINEAR  — linear fog:       f = (end - d) / (end - start)
 *****************************************************************************/
void gldSetFogState(GLD_fogState* outState, BOOL fogEnable, DWORD fogColor,
                    float fogStart, float fogEnd, float fogDensity, DWORD fogMode)
{
    if (!outState) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetFogState: NULL outState pointer, fog state not captured");
        return;
    }

    outState->enabled    = fogEnable;
    outState->fogColor   = fogColor;
    outState->fogStart   = fogStart;
    outState->fogEnd     = fogEnd;
    outState->fogDensity = fogDensity;
    outState->fogMode    = fogMode;

    if (fogMode > D3DFOG_LINEAR) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetFogState: unrecognised D3DFOGMODE value %lu, "
            "treating as D3DFOG_NONE",
            (unsigned long)fogMode);
        outState->fogMode = D3DFOG_NONE;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetAlphaTestState
 *
 * Capture DX9 alpha test render state into a GLD_alphaTestState struct
 * for the Fixed_Function_Emulator to consume.
 *
 * GL_ALPHA_TEST is removed from the OpenGL core profile, so the
 * Fixed_Function_Emulator injects a `discard` statement in the
 * fragment shader when alpha test is enabled.  The comparison function
 * and reference value are passed as shader uniforms.
 *
 * alphaRef is expected to be normalised to [0.0, 1.0].  Values outside
 * this range are clamped.
 *****************************************************************************/
void gldSetAlphaTestState(GLD_alphaTestState* outState, BOOL alphaTestEnable,
                          DWORD alphaFunc, float alphaRef)
{
    if (!outState) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetAlphaTestState: NULL outState pointer, alpha test state not captured");
        return;
    }

    outState->enabled   = alphaTestEnable;
    outState->alphaFunc = alphaFunc;

    /* Clamp alphaRef to [0.0, 1.0] */
    if (alphaRef < 0.0f)
        alphaRef = 0.0f;
    else if (alphaRef > 1.0f)
        alphaRef = 1.0f;
    outState->alphaRef = alphaRef;

    /* Validate the comparison function */
    if (alphaFunc < 1 || alphaFunc > 8) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetAlphaTestState: unrecognised D3DCMPFUNC value %lu, "
            "defaulting to D3DCMP_ALWAYS (8)",
            (unsigned long)alphaFunc);
        outState->alphaFunc = D3DCMP_ALWAYS;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetPointSize
 *
 * Set the OpenGL point size via glPointSize and store the value for
 * later upload to the active vertex shader as a gl_PointSize uniform
 * by the Fixed_Function_Emulator.
 *
 * In the OpenGL core profile, gl_PointSize must be written by the
 * vertex shader for point primitives.  This function sets the GL-side
 * state; the Fixed_Function_Emulator is responsible for uploading the
 * value as a uniform when generating point-rendering shaders.
 *****************************************************************************/
void gldSetPointSize(float pointSize)
{
    /* Clamp to a sane minimum to avoid GL errors */
    if (pointSize < 1.0f)
        pointSize = 1.0f;

    glPointSize(pointSize);
    GLD_CHECK_GL("glPointSize", "SetRenderState");
}
