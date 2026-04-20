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
* Description:  D3DFMT to OpenGL format mapping tables.
*               Maps Direct3D 9 texture/surface format enumerations to
*               OpenGL internal format, format, type tuples with swizzle
*               masks for channel remapping.
*
*********************************************************************************/

#ifndef __GL46_FORMAT_MAPPER_H
#define __GL46_FORMAT_MAPPER_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * D3DFMT constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally so we don't require the DX9 SDK headers in all
 * build configurations.
 */
#ifndef D3DFMT_A8R8G8B8
#define D3DFMT_A8R8G8B8     21
#endif
#ifndef D3DFMT_X8R8G8B8
#define D3DFMT_X8R8G8B8     22
#endif
#ifndef D3DFMT_R5G6B5
#define D3DFMT_R5G6B5       23
#endif
#ifndef D3DFMT_A1R5G5B5
#define D3DFMT_A1R5G5B5     25
#endif
#ifndef D3DFMT_A4R4G4B4
#define D3DFMT_A4R4G4B4     26
#endif
#ifndef D3DFMT_A8
#define D3DFMT_A8           28
#endif
#ifndef D3DFMT_L8
#define D3DFMT_L8           50
#endif
#ifndef D3DFMT_A8L8
#define D3DFMT_A8L8         51
#endif
#ifndef D3DFMT_D32
#define D3DFMT_D32          71
#endif
#ifndef D3DFMT_D24S8
#define D3DFMT_D24S8        75
#endif
#ifndef D3DFMT_D24X8
#define D3DFMT_D24X8        77
#endif
#ifndef D3DFMT_D16
#define D3DFMT_D16          80
#endif

/* MAKEFOURCC macro for compressed format codes */
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
     ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif

#ifndef D3DFMT_DXT1
#define D3DFMT_DXT1         MAKEFOURCC('D','X','T','1')
#endif
#ifndef D3DFMT_DXT3
#define D3DFMT_DXT3         MAKEFOURCC('D','X','T','3')
#endif
#ifndef D3DFMT_DXT5
#define D3DFMT_DXT5         MAKEFOURCC('D','X','T','5')
#endif

/*
 * GLD_formatMapping — describes how a D3DFMT value maps to OpenGL.
 *
 * Fields:
 *   d3dFormat        — the D3DFMT enumeration value.
 *   glInternalFormat — OpenGL sized internal format (e.g. GL_RGBA8).
 *   glFormat         — OpenGL pixel data format (e.g. GL_BGRA).
 *   glType           — OpenGL pixel data type (e.g. GL_UNSIGNED_INT_8_8_8_8_REV).
 *   swizzleR/G/B/A   — texture swizzle mask components. Use GL_RED, GL_GREEN,
 *                       GL_ZERO, GL_ONE, or 0 for "no swizzle needed".
 *   isCompressed     — TRUE if this is a compressed (DXTn/S3TC) format.
 *   bytesPerPixel    — bytes per pixel for uncompressed formats, or the
 *                       block size in bytes for compressed formats.
 */
typedef struct {
    DWORD   d3dFormat;
    GLenum  glInternalFormat;
    GLenum  glFormat;
    GLenum  glType;
    GLenum  swizzleR;
    GLenum  swizzleG;
    GLenum  swizzleB;
    GLenum  swizzleA;
    BOOL    isCompressed;
    int     bytesPerPixel;
} GLD_formatMapping;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Look up the OpenGL format mapping for a given D3DFMT value.
 *
 * Searches the internal static mapping table for a matching D3DFMT entry
 * and copies the result into *out.
 *
 * Parameters:
 *   d3dFormat — the D3DFMT enumeration value to look up.
 *   out       — pointer to a GLD_formatMapping struct to receive the result.
 *               Must not be NULL.
 *
 * Returns:
 *   TRUE  — mapping found and copied into *out.
 *   FALSE — unsupported format; an error is logged via gld_log.
 */
BOOL gldMapD3DFormat(DWORD d3dFormat, GLD_formatMapping *out);

#ifdef  __cplusplus
}
#endif

#endif
