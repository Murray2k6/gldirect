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
* Description:  OpenGL 4.6 error handling and diagnostic logging.
*
*********************************************************************************/

#ifndef __GL46_ERROR_HANDLER_H
#define __GL46_ERROR_HANDLER_H

#include <windows.h>

/*---------------------- Macros and type definitions ----------------------*/

// Convenience macro to check for GL errors after a call.
// Usage: GLD_CHECK_GL("glBindTexture", "SetTexture")
#define GLD_CHECK_GL(funcName, dx9Call) \
    gldCheckGLError((funcName), (dx9Call))

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

// Check for queued OpenGL errors and log each one.
// Calls glGetError() in a loop until GL_NO_ERROR, logging each error
// with the GL function name and the originating DX9 call.
void gldCheckGLError(const char* funcName, const char* dx9Call);

// Log a warning when a DX9 feature has no OpenGL equivalent.
// Continues execution with a reasonable fallback.
void gldLogUnsupported(const char* featureName);

// Log a fatal error during context creation or GLAD initialization.
// Returns FALSE to indicate failure without crashing.
BOOL gldLogFatal(const char* context, const char* message);

#ifdef  __cplusplus
}
#endif

#endif
