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
* Description:  Swap chain management and clear operations for the
*               OpenGL 4.6 backend.
*
*               Provides buffer swap via the Win32 SwapBuffers API and
*               DX9-to-GL clear flag translation with support for
*               rectangle-restricted clears via scissor test.
*
*********************************************************************************/

#ifndef __GL46_SWAP_CHAIN_H
#define __GL46_SWAP_CHAIN_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * D3DCLEAR flag constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally so we don't require the DX9 SDK headers.
 */
#ifndef D3DCLEAR_TARGET
#define D3DCLEAR_TARGET     1
#endif
#ifndef D3DCLEAR_ZBUFFER
#define D3DCLEAR_ZBUFFER    2
#endif
#ifndef D3DCLEAR_STENCIL
#define D3DCLEAR_STENCIL    4
#endif

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Swap the front and back buffers for the given device context.
 *
 * Calls the Win32 SwapBuffers API on the HDC associated with the
 * current OpenGL context.
 *
 * Parameters:
 *   hDC — the device context to swap buffers on.
 *
 * Returns:
 *   TRUE on success, FALSE on failure.
 */
BOOL gldSwapBuffers46(HDC hDC);

/*
 * Translate a DX9 Clear call to OpenGL clear operations.
 *
 * Maps D3DCLEAR flags to GL clear bits, sets clear values, and
 * handles rectangle-restricted clears via the scissor test.
 *
 * Temporarily enables full depth write and color write before
 * clearing, then restores the previous state.
 *
 * Parameters:
 *   flags     — combination of D3DCLEAR_TARGET, D3DCLEAR_ZBUFFER,
 *               D3DCLEAR_STENCIL.
 *   color     — ARGB packed clear color (D3DCOLOR format).
 *   z         — depth clear value [0.0, 1.0].
 *   stencil   — stencil clear value.
 *   rectCount — number of rectangles to clear (0 = full framebuffer).
 *   rects     — array of RECT structures defining clear regions.
 *               May be NULL when rectCount is 0.
 */
void gldClear46(DWORD flags, DWORD color, float z, DWORD stencil,
                DWORD rectCount, const RECT *rects);

#ifdef  __cplusplus
}
#endif

#endif
