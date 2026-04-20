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
* Description:  Pixel format enumeration and selection for the OpenGL 4.6
*               backend.
*
*               Builds pixel format descriptors based on host GPU OpenGL
*               capabilities instead of DX9 capabilities.  Enumerates
*               16-bit and 32-bit color formats with depth 16/24 and
*               stencil 0/8, all double-buffered.
*
*********************************************************************************/

#ifndef __GL46_PIXEL_FORMAT_PROVIDER_H
#define __GL46_PIXEL_FORMAT_PROVIDER_H

#include <windows.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * Maximum number of pixel formats we enumerate.
 * 2 color depths * 2 depth sizes * 2 stencil sizes = 8 formats.
 */
#define GLD_PF46_MAX_FORMATS    8

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Build the pixel format descriptor list for the GL46 backend.
 *
 * Enumerates combinations of:
 *   - 16-bit color (R5G6B5) and 32-bit color (A8R8G8B8)
 *   - Depth buffer: 16-bit and 24-bit
 *   - Stencil buffer: 0-bit and 8-bit
 *   - All double-buffered
 *
 * Sets PFD_SUPPORT_OPENGL, PFD_DRAW_TO_WINDOW, and
 * PFD_GENERIC_ACCELERATED flags.  Clears PFD_GENERIC_FORMAT
 * to indicate hardware acceleration.
 *
 * Returns:
 *   The number of pixel formats built, or 0 on failure.
 */
int gldBuildPixelFormatList46(void);

/*
 * Choose the best matching pixel format for the given PFD.
 *
 * Compares the requested PFD against the enumerated formats and
 * returns the 1-based index of the best match.
 *
 * Parameters:
 *   hDC  — the device context (reserved for future use).
 *   ppfd — the requested pixel format descriptor.
 *
 * Returns:
 *   1-based pixel format index, or 0 if no match found.
 */
int gldChoosePixelFormat46(HDC hDC, const PIXELFORMATDESCRIPTOR *ppfd);

/*
 * Fill a PIXELFORMATDESCRIPTOR for the given format index.
 *
 * Parameters:
 *   hDC    — the device context (reserved for future use).
 *   format — 1-based pixel format index.
 *   size   — size of the PIXELFORMATDESCRIPTOR structure.
 *   ppfd   — pointer to the PFD to fill.
 *
 * Returns:
 *   The maximum pixel format index (total number of formats),
 *   or 0 on error.
 */
int gldDescribePixelFormat46(HDC hDC, int format, UINT size,
                             PIXELFORMATDESCRIPTOR *ppfd);

/*
 * Get the total number of enumerated pixel formats.
 *
 * Returns:
 *   The number of pixel formats available.
 */
int gldGetPixelFormatCount46(void);

#ifdef  __cplusplus
}
#endif

#endif
