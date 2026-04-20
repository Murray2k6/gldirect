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
*               Provides top-level dispatch functions that route DX9 API
*               calls to the appropriate GL46 translation modules.  These
*               are thin wrappers that coordinate the various GL46 modules
*               (State_Translator, Buffer_Manager, Texture_Manager,
*               Shader_Translator, Render_Target_Manager, etc.) into a
*               coherent DX9-to-GL translation pipeline.
*
*               During initial development, many functions are stubs that
*               log the call and will be fleshed out during integration.
*
*********************************************************************************/

#ifndef __GL46_DX9_DISPATCH_H
#define __GL46_DX9_DISPATCH_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * Forward declaration — GLD_ctx is defined in gld_context.h.
 */
struct GLD_ctx_s;

/*
 * D3DRENDERSTATETYPE subset — commonly dispatched render states.
 * Full enum is in d3d9types.h; we define only what we reference here.
 */
#ifndef D3DRS_ZENABLE
#define D3DRS_ZENABLE               7
#endif
#ifndef D3DRS_FILLMODE
#define D3DRS_FILLMODE              8
#endif
#ifndef D3DRS_ZWRITEENABLE
#define D3DRS_ZWRITEENABLE          14
#endif
#ifndef D3DRS_ALPHATESTENABLE
#define D3DRS_ALPHATESTENABLE       15
#endif
#ifndef D3DRS_SRCBLEND
#define D3DRS_SRCBLEND              19
#endif
#ifndef D3DRS_DESTBLEND
#define D3DRS_DESTBLEND             20
#endif
#ifndef D3DRS_CULLMODE
#define D3DRS_CULLMODE              22
#endif
#ifndef D3DRS_ZFUNC
#define D3DRS_ZFUNC                 23
#endif
#ifndef D3DRS_ALPHAREF
#define D3DRS_ALPHAREF              24
#endif
#ifndef D3DRS_ALPHAFUNC
#define D3DRS_ALPHAFUNC             25
#endif
#ifndef D3DRS_ALPHABLENDENABLE
#define D3DRS_ALPHABLENDENABLE      27
#endif
#ifndef D3DRS_FOGENABLE
#define D3DRS_FOGENABLE             28
#endif
#ifndef D3DRS_FOGCOLOR
#define D3DRS_FOGCOLOR              34
#endif
#ifndef D3DRS_FOGTABLEMODE
#define D3DRS_FOGTABLEMODE          35
#endif
#ifndef D3DRS_FOGSTART
#define D3DRS_FOGSTART              36
#endif
#ifndef D3DRS_FOGEND
#define D3DRS_FOGEND                37
#endif
#ifndef D3DRS_FOGDENSITY
#define D3DRS_FOGDENSITY            38
#endif
#ifndef D3DRS_STENCILENABLE
#define D3DRS_STENCILENABLE         52
#endif
#ifndef D3DRS_STENCILFAIL
#define D3DRS_STENCILFAIL           53
#endif
#ifndef D3DRS_STENCILZFAIL
#define D3DRS_STENCILZFAIL          54
#endif
#ifndef D3DRS_STENCILPASS
#define D3DRS_STENCILPASS           55
#endif
#ifndef D3DRS_STENCILFUNC
#define D3DRS_STENCILFUNC           56
#endif
#ifndef D3DRS_STENCILREF
#define D3DRS_STENCILREF            57
#endif
#ifndef D3DRS_STENCILMASK
#define D3DRS_STENCILMASK           58
#endif
#ifndef D3DRS_STENCILWRITEMASK
#define D3DRS_STENCILWRITEMASK      59
#endif
#ifndef D3DRS_BLENDOP
#define D3DRS_BLENDOP               171
#endif
#ifndef D3DRS_SCISSORTESTENABLE
#define D3DRS_SCISSORTESTENABLE     174
#endif
#ifndef D3DRS_COLORWRITEENABLE
#define D3DRS_COLORWRITEENABLE      168
#endif
#ifndef D3DRS_POINTSIZE
#define D3DRS_POINTSIZE             154
#endif
#ifndef D3DRS_TWOSIDEDSTENCILMODE
#define D3DRS_TWOSIDEDSTENCILMODE   185
#endif
#ifndef D3DRS_SEPARATEALPHABLENDENABLE
#define D3DRS_SEPARATEALPHABLENDENABLE 206
#endif

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Dispatch a DX9 DrawPrimitive call through the GL46 pipeline.
 *
 * Flushes dirty shader constants, binds the active shader program
 * (translated or fixed-function), and issues the draw call via
 * Buffer_Manager.
 *
 * Parameters:
 *   primType    — DX9 primitive type (D3DPT_* constant).
 *   startVertex — index of the first vertex.
 *   primCount   — number of primitives to draw.
 */
void gldDX9_DrawPrimitive(DWORD primType, UINT startVertex, UINT primCount);

/*
 * Dispatch a DX9 DrawIndexedPrimitive call through the GL46 pipeline.
 *
 * Parameters:
 *   primType   — DX9 primitive type (D3DPT_* constant).
 *   baseVertex — value added to each index.
 *   startIndex — offset into the index buffer.
 *   primCount  — number of primitives to draw.
 *   indexType  — GL_UNSIGNED_SHORT or GL_UNSIGNED_INT.
 */
void gldDX9_DrawIndexedPrimitive(DWORD primType, INT baseVertex,
                                 UINT startIndex, UINT primCount,
                                 GLenum indexType);

/*
 * Dispatch a DX9 SetRenderState call to the appropriate GL46 module.
 *
 * Routes the render state type to State_Translator or
 * Fixed_Function_Emulator as appropriate.
 *
 * Parameters:
 *   state — the D3DRENDERSTATETYPE value.
 *   value — the render state value.
 */
void gldDX9_SetRenderState(DWORD state, DWORD value);

/*
 * Dispatch a DX9 SetTexture call.
 *
 * Binds the specified texture to the given stage via
 * Texture_Manager.
 *
 * Parameters:
 *   stage     — texture stage index (0-7).
 *   textureId — GL texture object ID (0 to unbind).
 */
void gldDX9_SetTexture(DWORD stage, GLuint textureId);

/*
 * Dispatch a DX9 SetTextureStageState call.
 *
 * Updates texture stage state in the Fixed_Function_Emulator.
 *
 * Parameters:
 *   stage — texture stage index (0-7).
 *   type  — the D3DTEXTURESTAGESTATETYPE value.
 *   value — the state value.
 */
void gldDX9_SetTextureStageState(DWORD stage, DWORD type, DWORD value);

/*
 * Dispatch a DX9 SetTransform call.
 *
 * Routes to Fixed_Function_Emulator for matrix updates.
 *
 * Parameters:
 *   transformType — D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION, etc.
 *   matrix        — pointer to 16 floats (row-major DX9 convention).
 */
void gldDX9_SetTransform(DWORD transformType, const float *matrix);

/*
 * Dispatch a DX9 SetViewport call.
 *
 * Routes to Coordinate_Adapter for viewport translation.
 *
 * Parameters:
 *   x, y          — viewport origin (DX9 convention).
 *   width, height — viewport dimensions.
 *   minZ, maxZ    — depth range.
 */
void gldDX9_SetViewport(int x, int y, int width, int height,
                        float minZ, float maxZ);

/*
 * Dispatch a DX9 Clear call.
 *
 * Routes to swap_chain module for clear operations.
 *
 * Parameters:
 *   flags     — D3DCLEAR_* flags.
 *   color     — ARGB clear color.
 *   z         — depth clear value.
 *   stencil   — stencil clear value.
 *   rectCount — number of clear rectangles (0 = full).
 *   rects     — array of RECT structures.
 */
void gldDX9_Clear(DWORD flags, DWORD color, float z, DWORD stencil,
                  DWORD rectCount, const RECT *rects);

/*
 * Dispatch a DX9 Present call.
 *
 * Swaps buffers via the swap chain module.
 *
 * Parameters:
 *   hDC — the device context to present on.
 */
void gldDX9_Present(HDC hDC);

/*
 * Dispatch a DX9 SetRenderTarget call.
 *
 * Routes to Render_Target_Manager.
 *
 * Parameters:
 *   index — render target index (0-3).
 *   fbo   — GL framebuffer object ID (0 for default back buffer).
 */
void gldDX9_SetRenderTarget(DWORD index, GLuint fbo);

/*
 * Dispatch a DX9 SetScissorRect call.
 *
 * Routes to State_Translator for scissor state.
 *
 * Parameters:
 *   rect — the scissor rectangle.
 */
void gldDX9_SetScissorRect(const RECT *rect);

/*
 * Dispatch a DX9 CreateVertexShader call.
 *
 * Parses and compiles the shader bytecode via Shader_Translator.
 *
 * Parameters:
 *   bytecode     — pointer to the DX9 shader bytecode.
 *   bytecodeSize — size of the bytecode in bytes.
 *
 * Returns:
 *   GL shader program ID, or 0 on failure.
 */
GLuint gldDX9_CreateVertexShader(const DWORD *bytecode, int bytecodeSize);

/*
 * Dispatch a DX9 CreatePixelShader call.
 *
 * Parameters:
 *   bytecode     — pointer to the DX9 shader bytecode.
 *   bytecodeSize — size of the bytecode in bytes.
 *
 * Returns:
 *   GL shader program ID, or 0 on failure.
 */
GLuint gldDX9_CreatePixelShader(const DWORD *bytecode, int bytecodeSize);

/*
 * Dispatch a DX9 SetVertexShaderConstantF call.
 *
 * Routes to shader_uniforms module.
 *
 * Parameters:
 *   startReg — first constant register to set.
 *   data     — pointer to float4 constant data.
 *   count    — number of float4 registers to set.
 */
void gldDX9_SetVertexShaderConstantF(UINT startReg, const float *data,
                                     UINT count);

/*
 * Dispatch a DX9 SetPixelShaderConstantF call.
 *
 * Parameters:
 *   startReg — first constant register to set.
 *   data     — pointer to float4 constant data.
 *   count    — number of float4 registers to set.
 */
void gldDX9_SetPixelShaderConstantF(UINT startReg, const float *data,
                                    UINT count);

#ifdef  __cplusplus
}
#endif

#endif
