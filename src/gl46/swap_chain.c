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
*********************************************************************************/

#include "swap_chain.h"
#include "context_manager.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

BOOL gldSwapBuffers46(HDC hDC)
{
    IDirect3DDevice9 *pDev;
    HWND hWnd;

    if (!hDC) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSwapBuffers46: NULL HDC");
        return FALSE;
    }

    pDev = gldGetD3DDevice46();
    if (!pDev) {
        static BOOL logged = FALSE;
        if (!logged) {
            logged = TRUE;
            gldLogPrintf(GLDLOG_WARN,
                "gldSwapBuffers46: no D3D9 device, falling back to GDI SwapBuffers");
        }
        return SwapBuffers(hDC);
    }

    hWnd = WindowFromDC(hDC);
    {
        HRESULT hr;

        /* EndScene may fail with D3DERR_INVALIDCALL if no scene is active;
         * that is normal and not an error. */
        (void)IDirect3DDevice9_EndScene(pDev);

        hr = IDirect3DDevice9_Present(pDev, NULL, NULL, hWnd ? hWnd : NULL, NULL);
        if (FAILED(hr)) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldSwapBuffers46: D3D9 Present failed (0x%08lX)",
                (unsigned long)hr);
            return FALSE;
        }
        return TRUE;
    }
}

// ***********************************************************************

void gldClear46(DWORD flags, DWORD color, float z, DWORD stencil,
                DWORD rectCount, const RECT *rects)
{
    GLbitfield  glClearBits = 0;
    GLboolean   savedDepthMask;
    GLboolean   savedColorMask[4];
    GLboolean   savedScissorEnabled;
    GLint       savedScissorBox[4];
    DWORD       i;

    /* Map D3DCLEAR flags to GL clear bits */
    if (flags & D3DCLEAR_TARGET)
        glClearBits |= GL_COLOR_BUFFER_BIT;
    if (flags & D3DCLEAR_ZBUFFER)
        glClearBits |= GL_DEPTH_BUFFER_BIT;
    if (flags & D3DCLEAR_STENCIL)
        glClearBits |= GL_STENCIL_BUFFER_BIT;

    if (glClearBits == 0)
        return;

    /* Save current depth mask and color mask state */
    glGetBooleanv(GL_DEPTH_WRITEMASK, &savedDepthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, savedColorMask);
    savedScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, savedScissorBox);

    /* Temporarily enable full writes so clear affects all channels */
    if (flags & D3DCLEAR_ZBUFFER)
        glDepthMask(GL_TRUE);
    if (flags & D3DCLEAR_TARGET)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* Set clear values */
    if (flags & D3DCLEAR_TARGET) {
        /* Extract ARGB components from D3DCOLOR (DWORD) */
        float a = (float)((color >> 24) & 0xFF) / 255.0f;
        float r = (float)((color >> 16) & 0xFF) / 255.0f;
        float g = (float)((color >>  8) & 0xFF) / 255.0f;
        float b = (float)((color      ) & 0xFF) / 255.0f;
        glClearColor(r, g, b, a);
        GLD_CHECK_GL("glClearColor", "Clear");
    }

    if (flags & D3DCLEAR_ZBUFFER) {
        glClearDepthf(z);
        GLD_CHECK_GL("glClearDepthf", "Clear");
    }

    if (flags & D3DCLEAR_STENCIL) {
        glClearStencil((GLint)stencil);
        GLD_CHECK_GL("glClearStencil", "Clear");
    }

    /* Perform the clear */
    if (rectCount == 0 || rects == NULL) {
        /* Full framebuffer clear */
        glClear(glClearBits);
        GLD_CHECK_GL("glClear", "Clear");
    } else {
        /* Rectangle-restricted clear via scissor test */
        glEnable(GL_SCISSOR_TEST);

        for (i = 0; i < rectCount; i++) {
            int x = rects[i].left;
            int y = rects[i].top;
            int w = rects[i].right - rects[i].left;
            int h = rects[i].bottom - rects[i].top;

            if (w <= 0 || h <= 0)
                continue;

            glScissor(x, y, w, h);
            GLD_CHECK_GL("glScissor", "Clear");

            glClear(glClearBits);
            GLD_CHECK_GL("glClear(rect)", "Clear");
        }
    }

    /* Restore previous state */
    glDepthMask(savedDepthMask);
    glColorMask(savedColorMask[0], savedColorMask[1],
                savedColorMask[2], savedColorMask[3]);

    if (savedScissorEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glScissor(savedScissorBox[0], savedScissorBox[1],
              savedScissorBox[2], savedScissorBox[3]);
}

// ***********************************************************************
