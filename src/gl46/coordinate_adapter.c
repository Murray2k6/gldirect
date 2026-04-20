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

#define STRICT
#include <windows.h>

#include <glad/gl.h>
#include "coordinate_adapter.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

/*****************************************************************************
 * gldAdjustProjectionMatrix
 *
 * Fallback depth remap when ARB_clip_control is unavailable.
 * Post-multiplies the column-major 4x4 matrix by:
 *
 *   | 1  0  0  0 |
 *   | 0  1  0  0 |
 *   | 0  0  2  0 |
 *   | 0  0 -1  1 |
 *
 * For a column-major matrix M, post-multiplying M' = M * R means:
 *   new_col2 = 2 * old_col2 - old_col3
 *   (all other columns unchanged)
 *
 * Column-major layout:
 *   col0 = [0,1,2,3], col1 = [4,5,6,7],
 *   col2 = [8,9,10,11], col3 = [12,13,14,15]
 *
 * The remap matrix R has:
 *   R col0 = (1,0,0,0), R col1 = (0,1,0,0),
 *   R col2 = (0,0,2,-1), R col3 = (0,0,0,1)
 *
 * So (M*R) col_j = M * R_col_j:
 *   result col0 = M * (1,0,0,0) = M col0  (unchanged)
 *   result col1 = M * (0,1,0,0) = M col1  (unchanged)
 *   result col2 = M * (0,0,2,-1) = 2*M_col2 + (-1)*M_col3
 *   result col3 = M * (0,0,0,1) = M col3  (unchanged)
 *****************************************************************************/
void gldAdjustProjectionMatrix(float *mat4x4)
{
    int i;

    if (!mat4x4)
        return;

    /*
     * result_col2[i] = 2 * mat_col2[i] - mat_col3[i]
     *
     * col2 indices: 8,9,10,11
     * col3 indices: 12,13,14,15
     */
    for (i = 0; i < 4; i++) {
        mat4x4[8 + i] = 2.0f * mat4x4[8 + i] - mat4x4[12 + i];
    }
}

/*****************************************************************************
 * gldFlipViewportY
 *
 * Fallback Y-flip when ARB_clip_control is unavailable.
 * Converts DX9 top-left origin Y to OpenGL bottom-left origin Y.
 *
 * Transform: *y = windowHeight - *y - *height
 *****************************************************************************/
void gldFlipViewportY(int windowHeight, int *y, int *height)
{
    if (!y || !height)
        return;

    *y = windowHeight - *y - *height;
}

// ***********************************************************************

void gldInitCoordinateAdapter(GLD_glContext *ctx)
{
    BOOL hasClipControl = FALSE;

    if (!ctx) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldInitCoordinateAdapter: NULL context pointer");
        return;
    }

    // Default to unavailable until proven otherwise.
    ctx->bClipControlAvailable = FALSE;

    // ------------------------------------------------------------------
    // Determine ARB_clip_control availability based on GL version.
    //
    // GL 4.5+: ARB_clip_control is core and guaranteed available.
    // GL 3.3–4.4: query the GLAD extension flag for GL_ARB_clip_control.
    // ------------------------------------------------------------------
    if (ctx->glVersionMajor > 4 ||
        (ctx->glVersionMajor == 4 && ctx->glVersionMinor >= 5)) {
        // Core in GL 4.5+, guaranteed available.
        hasClipControl = TRUE;
        gldLogPrintf(GLDLOG_INFO,
            "ARB_clip_control is core in GL %d.%d",
            ctx->glVersionMajor, ctx->glVersionMinor);
    } else {
        // GL 3.3–4.4 fallback: check the extension via GLAD.
        if (GLAD_GL_ARB_clip_control) {
            hasClipControl = TRUE;
            gldLogPrintf(GLDLOG_INFO,
                "GL_ARB_clip_control extension available on GL %d.%d",
                ctx->glVersionMajor, ctx->glVersionMinor);
        } else {
            gldLogPrintf(GLDLOG_WARN,
                "GL_ARB_clip_control not available on GL %d.%d — "
                "will use manual projection/viewport adjustments",
                ctx->glVersionMajor, ctx->glVersionMinor);
        }
    }

    // ------------------------------------------------------------------
    // If clip control is available, activate it to match DX9 conventions:
    //   - GL_UPPER_LEFT: window origin at top-left (matches DX9)
    //   - GL_ZERO_TO_ONE: clip space depth [0,1] (matches DX9)
    // ------------------------------------------------------------------
    if (hasClipControl) {
        if (glClipControl) {
            glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
            gldCheckGLError("glClipControl", "CoordinateAdapter_Init");

            ctx->bClipControlAvailable = TRUE;
            gldLogPrintf(GLDLOG_SYSTEM,
                "glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE) set successfully — "
                "DX9 coordinate conventions active");
        } else {
            gldLogPrintf(GLDLOG_WARN,
                "ARB_clip_control reported available but glClipControl is NULL — "
                "falling back to manual adjustments");
        }
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldSetViewport46
 *
 * Translate DX9 SetViewport parameters to glViewport and glDepthRangef,
 * applying Y-flip and depth remap based on the active clip control mode.
 *
 * DX9 uses a left-handed coordinate system with:
 *   - Top-left window origin
 *   - Depth range [0, 1]
 *   - Clockwise front-face winding
 *
 * When clip control is active (GL_UPPER_LEFT, GL_ZERO_TO_ONE):
 *   - Viewport coordinates pass through directly.
 *   - Depth range passes through directly: glDepthRangef(MinZ, MaxZ).
 *
 * When clip control is unavailable (OpenGL defaults):
 *   - Y is flipped: glViewport(X, windowHeight - Y - Height, Width, Height).
 *   - Depth is remapped from [0,1] to [-1,1]: glDepthRangef(MinZ*2-1, MaxZ*2-1).
 *
 * In both cases, glFrontFace(GL_CW) is set to account for DX9's left-handed
 * coordinate system where clockwise winding indicates front faces.
 *****************************************************************************/
void gldSetViewport46(GLD_glContext *ctx, int X, int Y, int Width, int Height,
                      float MinZ, float MaxZ)
{
    GLint   vpX;
    GLint   vpY;
    GLsizei vpWidth;
    GLsizei vpHeight;
    GLfloat depthNear;
    GLfloat depthFar;

    if (!ctx) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetViewport46: NULL context pointer");
        return;
    }

    // ------------------------------------------------------------------
    // Cache the viewport state in the context for other modules to query.
    // ------------------------------------------------------------------
    ctx->viewportX      = (GLint)X;
    ctx->viewportY      = (GLint)Y;
    ctx->viewportWidth  = (GLsizei)Width;
    ctx->viewportHeight = (GLsizei)Height;
    ctx->depthRangeNear = MinZ;
    ctx->depthRangeFar  = MaxZ;

    // ------------------------------------------------------------------
    // Compute viewport and depth range based on clip control availability.
    // ------------------------------------------------------------------
    if (ctx->bClipControlAvailable) {
        /*
         * Clip control is active (GL_UPPER_LEFT, GL_ZERO_TO_ONE).
         * DX9 conventions match directly — no transformation needed.
         */
        vpX      = (GLint)X;
        vpY      = (GLint)Y;
        vpWidth  = (GLsizei)Width;
        vpHeight = (GLsizei)Height;
        depthNear = MinZ;
        depthFar  = MaxZ;
    } else {
        /*
         * Clip control unavailable — OpenGL defaults apply:
         *   - Bottom-left window origin: flip Y.
         *   - Depth range [-1, 1]: remap from DX9 [0, 1].
         *
         * Get the window height from the HWND for the Y-flip.
         */
        int windowHeight = 0;
        RECT clientRect;

        if (ctx->hWnd && GetClientRect(ctx->hWnd, &clientRect)) {
            windowHeight = clientRect.bottom - clientRect.top;
        } else {
            gldLogPrintf(GLDLOG_WARN,
                "gldSetViewport46: could not determine window height for "
                "Y-flip — using viewport Y + Height as fallback");
            windowHeight = Y + Height;
        }

        vpX      = (GLint)X;
        vpY      = (GLint)(windowHeight - Y - Height);
        vpWidth  = (GLsizei)Width;
        vpHeight = (GLsizei)Height;

        /* Remap depth from DX9 [0,1] to OpenGL [-1,1]: z' = 2*z - 1 */
        depthNear = MinZ * 2.0f - 1.0f;
        depthFar  = MaxZ * 2.0f - 1.0f;
    }

    // ------------------------------------------------------------------
    // Issue the GL calls.
    // ------------------------------------------------------------------
    glViewport(vpX, vpY, vpWidth, vpHeight);
    gldCheckGLError("glViewport", "SetViewport");

    glDepthRangef(depthNear, depthFar);
    gldCheckGLError("glDepthRangef", "SetViewport");

    /*
     * DX9 uses a left-handed coordinate system where clockwise winding
     * indicates front faces. OpenGL uses a right-handed system where
     * counter-clockwise is the default front face. Setting GL_CW as the
     * front face makes OpenGL match DX9's winding convention.
     */
    glFrontFace(GL_CW);
    gldCheckGLError("glFrontFace", "SetViewport");
}
