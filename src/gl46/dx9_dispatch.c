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
* Description:  DX9 API dispatch layer for the OpenGL 4.6 backend.
*
*               Routes DX9 API calls to the appropriate GL46 translation
*               modules.  Many functions are currently stubs that log the
*               call and will be fleshed out during integration.
*
*********************************************************************************/

#include "dx9_dispatch.h"
#include "error_handler.h"
#include "state_translator.h"
#include "buffer_manager.h"
#include "swap_chain.h"
#include "gld_log.h"
#include <string.h>

// ***********************************************************************

void gldDX9_DrawPrimitive(DWORD primType, UINT startVertex, UINT primCount)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_DrawPrimitive: primType=%lu startVertex=%u primCount=%u",
        primType, startVertex, primCount);

    /* TODO: Flush dirty shader constants via shader_uniforms */
    /* TODO: Bind active shader program (translated or fixed-function) */

    /* Issue the draw call via Buffer_Manager */
    gldDrawPrimitive46(NULL, primType, startVertex, primCount);

    GLD_CHECK_GL("DrawPrimitive", "DrawPrimitive");
}

// ***********************************************************************

void gldDX9_DrawIndexedPrimitive(DWORD primType, INT baseVertex,
                                 UINT startIndex, UINT primCount,
                                 GLenum indexType)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_DrawIndexedPrimitive: primType=%lu base=%d start=%u count=%u",
        primType, baseVertex, startIndex, primCount);

    /* TODO: Flush dirty shader constants via shader_uniforms */
    /* TODO: Bind active shader program (translated or fixed-function) */

    /* Issue the indexed draw call via Buffer_Manager */
    gldDrawIndexedPrimitive46(NULL, primType, baseVertex,
                              startIndex, primCount, indexType);

    GLD_CHECK_GL("DrawIndexedPrimitive", "DrawIndexedPrimitive");
}

// ***********************************************************************

void gldDX9_SetRenderState(DWORD state, DWORD value)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetRenderState: state=%lu value=%lu", state, value);

    /*
     * Route render state changes to the appropriate State_Translator
     * function.  For states that affect the fixed-function pipeline
     * (fog, alpha test), the values are captured for the
     * Fixed_Function_Emulator to consume.
     *
     * TODO: Wire up full state routing during integration.
     * For now, handle the most common states.
     */
    switch (state) {
    case D3DRS_CULLMODE:
        gldSetCullMode(value);
        break;

    case D3DRS_FILLMODE:
        gldSetFillMode(value);
        break;

    case D3DRS_COLORWRITEENABLE:
        gldSetColorWriteMask(value);
        break;

    case D3DRS_POINTSIZE: {
        /* Reinterpret DWORD as float (DX9 convention) */
        float pointSize;
        memcpy(&pointSize, &value, sizeof(float));
        gldSetPointSize(pointSize);
        break;
    }

    /* Blend state — requires multiple render state values.
     * Individual state changes are cached and applied together
     * before the next draw call.  For now, log and skip. */
    case D3DRS_ALPHABLENDENABLE:
    case D3DRS_SRCBLEND:
    case D3DRS_DESTBLEND:
    case D3DRS_BLENDOP:
    case D3DRS_SEPARATEALPHABLENDENABLE:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: blend state %lu=%lu (cached for batch apply)",
            state, value);
        break;

    /* Depth state — similar batching needed */
    case D3DRS_ZENABLE:
    case D3DRS_ZWRITEENABLE:
    case D3DRS_ZFUNC:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: depth state %lu=%lu (cached for batch apply)",
            state, value);
        break;

    /* Stencil state */
    case D3DRS_STENCILENABLE:
    case D3DRS_STENCILFUNC:
    case D3DRS_STENCILREF:
    case D3DRS_STENCILMASK:
    case D3DRS_STENCILWRITEMASK:
    case D3DRS_STENCILFAIL:
    case D3DRS_STENCILZFAIL:
    case D3DRS_STENCILPASS:
    case D3DRS_TWOSIDEDSTENCILMODE:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: stencil state %lu=%lu (cached for batch apply)",
            state, value);
        break;

    /* Fog state — forwarded to Fixed_Function_Emulator */
    case D3DRS_FOGENABLE:
    case D3DRS_FOGCOLOR:
    case D3DRS_FOGTABLEMODE:
    case D3DRS_FOGSTART:
    case D3DRS_FOGEND:
    case D3DRS_FOGDENSITY:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: fog state %lu=%lu (forwarded to FF emulator)",
            state, value);
        break;

    /* Alpha test — forwarded to Fixed_Function_Emulator */
    case D3DRS_ALPHATESTENABLE:
    case D3DRS_ALPHAFUNC:
    case D3DRS_ALPHAREF:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: alpha test state %lu=%lu (forwarded to FF emulator)",
            state, value);
        break;

    case D3DRS_SCISSORTESTENABLE:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: scissor enable=%lu", value);
        break;

    default:
        gldLogPrintf(GLDLOG_DEBUG,
            "gldDX9_SetRenderState: unhandled state %lu=%lu", state, value);
        break;
    }
}

// ***********************************************************************

void gldDX9_SetTexture(DWORD stage, GLuint textureId)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetTexture: stage=%lu textureId=%u", stage, textureId);

    /*
     * TODO: Bind the texture to the appropriate texture unit via
     * Texture_Manager.  For now, issue the GL calls directly.
     */
    glActiveTexture(GL_TEXTURE0 + stage);
    if (textureId != 0)
        glBindTexture(GL_TEXTURE_2D, textureId);
    else
        glBindTexture(GL_TEXTURE_2D, 0);

    GLD_CHECK_GL("glBindTexture", "SetTexture");
}

// ***********************************************************************

void gldDX9_SetTextureStageState(DWORD stage, DWORD type, DWORD value)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetTextureStageState: stage=%lu type=%lu value=%lu",
        stage, type, value);

    /*
     * TODO: Forward to Fixed_Function_Emulator to update texture
     * stage state for shader variant selection.
     */
}

// ***********************************************************************

void gldDX9_SetTransform(DWORD transformType, const float *matrix)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetTransform: type=%lu", transformType);

    /*
     * TODO: Forward to Fixed_Function_Emulator for matrix update.
     * The matrix needs to be transposed from DX9 row-major to
     * GL column-major format.
     */
    (void)matrix;
}

// ***********************************************************************

void gldDX9_SetViewport(int x, int y, int width, int height,
                        float minZ, float maxZ)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetViewport: (%d,%d) %dx%d depth=[%.3f,%.3f]",
        x, y, width, height, minZ, maxZ);

    /*
     * TODO: Route to Coordinate_Adapter for viewport translation
     * with Y-flip and depth range adjustment based on clip control.
     * For now, issue GL calls directly.
     */
    glViewport(x, y, width, height);
    GLD_CHECK_GL("glViewport", "SetViewport");

    glDepthRangef(minZ, maxZ);
    GLD_CHECK_GL("glDepthRangef", "SetViewport");
}

// ***********************************************************************

void gldDX9_Clear(DWORD flags, DWORD color, float z, DWORD stencil,
                  DWORD rectCount, const RECT *rects)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_Clear: flags=0x%lX color=0x%08lX z=%.3f stencil=%lu rects=%lu",
        flags, color, z, stencil, rectCount);

    gldClear46(flags, color, z, stencil, rectCount, rects);
}

// ***********************************************************************

void gldDX9_Present(HDC hDC)
{
    gldLogPrintf(GLDLOG_DEBUG, "gldDX9_Present");

    gldSwapBuffers46(hDC);
}

// ***********************************************************************

void gldDX9_SetRenderTarget(DWORD index, GLuint fbo)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetRenderTarget: index=%lu fbo=%u", index, fbo);

    /*
     * TODO: Route to Render_Target_Manager.
     * For now, bind the FBO directly.
     */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer", "SetRenderTarget");
}

// ***********************************************************************

void gldDX9_SetScissorRect(const RECT *rect)
{
    if (!rect) {
        gldLogPrintf(GLDLOG_WARN,
            "gldDX9_SetScissorRect: NULL rect pointer");
        return;
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetScissorRect: (%ld,%ld)-(%ld,%ld)",
        rect->left, rect->top, rect->right, rect->bottom);

    /*
     * TODO: Route through State_Translator with Y-flip support.
     * For now, set scissor directly.
     */
    glScissor(rect->left, rect->top,
              rect->right - rect->left,
              rect->bottom - rect->top);
    GLD_CHECK_GL("glScissor", "SetScissorRect");
}

// ***********************************************************************

GLuint gldDX9_CreateVertexShader(const DWORD *bytecode, int bytecodeSize)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_CreateVertexShader: %d bytes", bytecodeSize);

    /*
     * TODO: Route to Shader_Translator to parse bytecode, generate
     * GLSL, compile, and return the program ID.
     */
    (void)bytecode;
    (void)bytecodeSize;

    gldLogUnsupported("CreateVertexShader (stub — not yet wired)");
    return 0;
}

// ***********************************************************************

GLuint gldDX9_CreatePixelShader(const DWORD *bytecode, int bytecodeSize)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_CreatePixelShader: %d bytes", bytecodeSize);

    /*
     * TODO: Route to Shader_Translator to parse bytecode, generate
     * GLSL, compile, and return the program ID.
     */
    (void)bytecode;
    (void)bytecodeSize;

    gldLogUnsupported("CreatePixelShader (stub — not yet wired)");
    return 0;
}

// ***********************************************************************

void gldDX9_SetVertexShaderConstantF(UINT startReg, const float *data,
                                     UINT count)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetVertexShaderConstantF: reg=%u count=%u", startReg, count);

    /*
     * TODO: Route to shader_uniforms module to update the VS
     * constant shadow buffer and mark dirty range.
     */
    (void)data;
}

// ***********************************************************************

void gldDX9_SetPixelShaderConstantF(UINT startReg, const float *data,
                                    UINT count)
{
    gldLogPrintf(GLDLOG_DEBUG,
        "gldDX9_SetPixelShaderConstantF: reg=%u count=%u", startReg, count);

    /*
     * TODO: Route to shader_uniforms module to update the PS
     * constant shadow buffer and mark dirty range.
     */
    (void)data;
}

// ***********************************************************************
