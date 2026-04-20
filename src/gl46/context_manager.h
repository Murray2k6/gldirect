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
* Description:  OpenGL 4.6 core profile context creation and management via WGL.
*
*********************************************************************************/

#ifndef __GL46_CONTEXT_MANAGER_H
#define __GL46_CONTEXT_MANAGER_H

#include <windows.h>
#include <glad/gl.h>

/*
 * GLD_ctx is defined in gld_context.h.  We need the full definition
 * for gldDeleteContext46's parameter type.
 */
#include "gld_context.h"

/*---------------------- Macros and type definitions ----------------------*/

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Create an OpenGL 4.6 core profile context on the given device context.
 *
 * Steps:
 *   1. Create a temporary legacy context and make it current.
 *   2. Load WGL extensions via gladLoadWGL (specifically wglCreateContextAttribsARB).
 *   3. Request a 4.6 core forward-compatible context.
 *   4. If 4.6 fails, fall back through 4.5, 4.4, ... 3.3 core.
 *   5. Delete the temporary context.
 *   6. Make the new context current and load GL 4.6 function pointers via gladLoadGL.
 *   7. Validate critical GL function pointers.
 *
 * On success, populates glCtx->glVersionMajor and glCtx->glVersionMinor with
 * the negotiated version and returns the HGLRC.
 * On failure, returns NULL.
 *
 * Parameters:
 *   hDC   — device context to create the GL context on.
 *   pMajor — [out] receives the negotiated major GL version (may be NULL).
 *   pMinor — [out] receives the negotiated minor GL version (may be NULL).
 */
HGLRC gldCreateContext46(HDC hDC, GLint *pMajor, GLint *pMinor);

/*
 * Make an OpenGL context current on the given device context.
 *
 * Calls wglMakeCurrent to bind the real OpenGL context, then updates
 * the per-thread current context pointer in the GLD_ctx slot array
 * via gldSetCurrentContext.
 *
 * Parameters:
 *   hDC  — device context to bind the GL context to.
 *   hRC  — the OpenGL rendering context handle (from gldCreateContext46).
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL gldMakeCurrent46(HDC hDC, HGLRC hRC);

/*
 * Delete an OpenGL context and release all associated GL resources.
 *
 * Iterates and deletes all OpenGL resources owned by the context
 * (shader program cache, buffer/VAO cache, texture cache, FBO cache),
 * then calls wglDeleteContext on the real handle, then clears the
 * context slot's GL state.
 *
 * Parameters:
 *   ctx — pointer to the GLD_ctx slot that owns the context.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL gldDeleteContext46(GLD_ctx *ctx);

#ifdef  __cplusplus
}
#endif

#endif
