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
* Description:  GLD_driver function table implementation for the OpenGL 4.6
*               backend.  Provides the same interface as the DX9 backend but
*               routes all calls through the GL46 modules.
*
*********************************************************************************/

#ifndef __GL46_DRIVER_H
#define __GL46_DRIVER_H

#include "gld_context.h"
#include "gld_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL gldGetDXErrorString_GL46(HRESULT hr, char *buf, int nBufSize);
BOOL gldCreateDrawable_GL46(GLD_ctx *ctx, BOOL bPersistantInterface, BOOL bPersistantBuffers);
BOOL gldResizeDrawable_GL46(GLD_ctx *ctx, BOOL bDefaultDriver, BOOL bPersistantInterface, BOOL bPersistantBuffers);
BOOL gldDestroyDrawable_GL46(GLD_ctx *ctx);
BOOL gldCreatePrivateGlobals_GL46(void);
BOOL gldDestroyPrivateGlobals_GL46(void);
BOOL gldBuildPixelformatList_GL46(void);
BOOL gldInitialiseMesa_GL46(GLD_ctx *ctx);
BOOL gldSwapBuffers_GL46(GLD_ctx *ctx, HDC hDC, HWND hWnd);
PROC gldGetProcAddress_GL46(LPCSTR a);
BOOL gldGetDisplayMode_GL46(GLD_ctx *ctx, GLD_displayMode *glddm);

#ifdef __cplusplus
}
#endif

#endif
