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

#define STRICT
#include <windows.h>

#include <glad/gl.h>
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

// Convert a GL error code to a human-readable string.
static const char* _gldGLErrorString(GLenum err)
{
    switch (err) {
    case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default:                                return "GL_UNKNOWN_ERROR";
    }
}

// ***********************************************************************

void gldCheckGLError(const char* funcName, const char* dx9Call)
{
    GLenum err;

    // Consume all queued GL errors
    while ((err = glGetError()) != GL_NO_ERROR) {
        gldLogPrintf(GLDLOG_ERROR,
            "GL error 0x%04X (%s) in %s [DX9: %s]",
            (unsigned int)err,
            _gldGLErrorString(err),
            funcName ? funcName : "unknown",
            dx9Call ? dx9Call : "unknown");
    }
}

// ***********************************************************************

void gldLogUnsupported(const char* featureName)
{
    gldLogPrintf(GLDLOG_WARN,
        "Unsupported DX9 feature: %s (no GL equivalent, using fallback)",
        featureName ? featureName : "unknown");
}

// ***********************************************************************

BOOL gldLogFatal(const char* context, const char* message)
{
    gldLogPrintf(GLDLOG_CRITICAL,
        "Fatal error in %s: %s",
        context ? context : "unknown",
        message ? message : "unspecified error");

    return FALSE;
}

// ***********************************************************************
