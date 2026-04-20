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
* Description:  Coordinate system adaptation between DX9 and OpenGL.
*               Handles clip control, viewport Y-flip, and depth range
*               differences using ARB_clip_control where available.
*
*********************************************************************************/

#ifndef __GL46_COORDINATE_ADAPTER_H
#define __GL46_COORDINATE_ADAPTER_H

#include <windows.h>
#include <glad/gl.h>

/*
 * GLD_glContext is defined in gld_context.h.  We need the full definition
 * for the context parameter types.
 */
#include "gld_context.h"

/*---------------------- Macros and type definitions ----------------------*/

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Initialize the coordinate adapter for a newly created GL context.
 *
 * Checks whether ARB_clip_control is available:
 *   - GL 4.5+: ARB_clip_control is core, guaranteed available.
 *   - GL 3.3–4.4: queries the extension string for GL_ARB_clip_control.
 *
 * If available, calls glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE) to
 * match DX9 conventions (top-left origin, depth [0,1]).
 *
 * Stores the result in ctx->bClipControlAvailable for other modules
 * to query.
 *
 * Parameters:
 *   ctx — pointer to the GLD_glContext to initialize clip control for.
 *         Must have glVersionMajor/glVersionMinor already populated.
 */
void gldInitCoordinateAdapter(GLD_glContext *ctx);

/*
 * Adjust a projection matrix to remap depth from DX9 [0,1] to
 * OpenGL default [-1,1] clip space.
 *
 * This is the fallback path used when ARB_clip_control is NOT available.
 * Post-multiplies the input matrix by a depth remap matrix:
 *
 *   | 1  0  0  0 |
 *   | 0  1  0  0 |
 *   | 0  0  2  0 |
 *   | 0  0 -1  1 |
 *
 * The result transforms z' = 2*z - 1, converting [0,1] depth to [-1,1].
 *
 * The matrix is stored in column-major order (OpenGL convention).
 * The z-column indices are [2], [6], [10], [14].
 *
 * Parameters:
 *   mat4x4 — pointer to a column-major 4x4 float matrix (16 floats).
 *            Modified in place.
 */
void gldAdjustProjectionMatrix(float *mat4x4);

/*
 * Flip the Y coordinate of a viewport or scissor rectangle from
 * DX9 top-left origin to OpenGL bottom-left origin.
 *
 * This is the fallback path used when ARB_clip_control is NOT available.
 * Transforms: *y = windowHeight - *y - *height
 *
 * Parameters:
 *   windowHeight — total height of the render window in pixels.
 *   y            — pointer to the Y coordinate (modified in place).
 *   height       — pointer to the rectangle height (read only, not modified).
 */
void gldFlipViewportY(int windowHeight, int *y, int *height);

/*
 * Translate DX9 SetViewport parameters to OpenGL glViewport and
 * glDepthRangef calls, applying Y-flip and depth remap based on
 * the active clip control mode.
 *
 * When clip control is active (GL_UPPER_LEFT, GL_ZERO_TO_ONE):
 *   - Passes viewport coordinates directly to glViewport.
 *   - Sets glDepthRangef(MinZ, MaxZ).
 *
 * When clip control is unavailable:
 *   - Flips Y via glViewport(X, windowHeight - Y - Height, Width, Height).
 *   - Remaps depth via glDepthRangef(MinZ*2-1, MaxZ*2-1).
 *
 * Also sets glFrontFace(GL_CW) to account for DX9's left-handed
 * coordinate system (DX9 uses CW winding for front faces by default,
 * which maps to GL_CW in OpenGL's right-handed system).
 *
 * Parameters:
 *   ctx    — pointer to the GLD_glContext. Must have bClipControlAvailable
 *            and hWnd already populated.
 *   X      — viewport left edge in pixels (DX9 convention).
 *   Y      — viewport top edge in pixels (DX9 convention).
 *   Width  — viewport width in pixels.
 *   Height — viewport height in pixels.
 *   MinZ   — near depth value [0,1] (DX9 convention).
 *   MaxZ   — far depth value [0,1] (DX9 convention).
 */
void gldSetViewport46(GLD_glContext *ctx, int X, int Y, int Width, int Height,
                      float MinZ, float MaxZ);

#ifdef  __cplusplus
}
#endif

#endif
