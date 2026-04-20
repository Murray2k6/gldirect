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
*********************************************************************************/

#include "pixel_format_provider.h"
#include "gld_log.h"
#include <string.h>

/*---------------------- Static module state ----------------------*/

/*
 * Enumerated pixel format descriptors.
 * Built by gldBuildPixelFormatList46().
 */
static PIXELFORMATDESCRIPTOR s_formats[GLD_PF46_MAX_FORMATS];
static int s_formatCount = 0;

/*---------------------- Internal helper functions ----------------------*/

/*
 * Initialize a single PFD entry with the given parameters.
 */
static void sInitPFD(PIXELFORMATDESCRIPTOR *pfd,
                     BYTE colorBits, BYTE redBits, BYTE greenBits,
                     BYTE blueBits, BYTE alphaBits,
                     BYTE redShift, BYTE greenShift, BYTE blueShift,
                     BYTE alphaShift,
                     BYTE depthBits, BYTE stencilBits)
{
    memset(pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));

    pfd->nSize          = sizeof(PIXELFORMATDESCRIPTOR);
    pfd->nVersion       = 1;
    pfd->dwFlags        = PFD_SUPPORT_OPENGL |
                          PFD_DRAW_TO_WINDOW |
                          PFD_DOUBLEBUFFER |
                          PFD_GENERIC_ACCELERATED;
    pfd->iPixelType     = PFD_TYPE_RGBA;
    pfd->cColorBits     = colorBits;
    pfd->cRedBits       = redBits;
    pfd->cRedShift      = redShift;
    pfd->cGreenBits     = greenBits;
    pfd->cGreenShift    = greenShift;
    pfd->cBlueBits      = blueBits;
    pfd->cBlueShift     = blueShift;
    pfd->cAlphaBits     = alphaBits;
    pfd->cAlphaShift    = alphaShift;
    pfd->cAccumBits     = 0;
    pfd->cDepthBits     = depthBits;
    pfd->cStencilBits   = stencilBits;
    pfd->cAuxBuffers    = 0;
    pfd->iLayerType     = PFD_MAIN_PLANE;
}

/*
 * Compute a match score between a requested PFD and a candidate.
 * Higher score = better match.  Returns -1 if the candidate is
 * incompatible (missing required flags).
 */
static int sMatchScore(const PIXELFORMATDESCRIPTOR *requested,
                       const PIXELFORMATDESCRIPTOR *candidate)
{
    int score = 0;

    /* Check required flags */
    if (requested->dwFlags & PFD_SUPPORT_OPENGL) {
        if (!(candidate->dwFlags & PFD_SUPPORT_OPENGL))
            return -1;
    }
    if (requested->dwFlags & PFD_DRAW_TO_WINDOW) {
        if (!(candidate->dwFlags & PFD_DRAW_TO_WINDOW))
            return -1;
    }
    if (requested->dwFlags & PFD_DOUBLEBUFFER) {
        if (!(candidate->dwFlags & PFD_DOUBLEBUFFER))
            return -1;
    }

    /* Prefer matching color depth */
    if (candidate->cColorBits >= requested->cColorBits)
        score += 10;
    if (candidate->cColorBits == requested->cColorBits)
        score += 5;

    /* Prefer matching depth buffer */
    if (candidate->cDepthBits >= requested->cDepthBits)
        score += 10;
    if (candidate->cDepthBits == requested->cDepthBits)
        score += 5;

    /* Prefer matching stencil buffer */
    if (candidate->cStencilBits >= requested->cStencilBits)
        score += 5;
    if (candidate->cStencilBits == requested->cStencilBits)
        score += 3;

    /* Prefer matching alpha */
    if (candidate->cAlphaBits >= requested->cAlphaBits)
        score += 3;

    return score;
}

/* ===================================================================
 * Public API
 * =================================================================== */

int gldBuildPixelFormatList46(void)
{
    /*
     * Enumerate 8 pixel format combinations:
     *   Color: 16-bit (R5G6B5), 32-bit (A8R8G8B8)
     *   Depth: 16-bit, 24-bit
     *   Stencil: 0-bit, 8-bit
     *
     * All formats are double-buffered with hardware acceleration.
     */

    /* Color depth parameters */
    static const struct {
        BYTE colorBits, redBits, greenBits, blueBits, alphaBits;
        BYTE redShift, greenShift, blueShift, alphaShift;
    } colorConfigs[] = {
        /* 16-bit R5G6B5 */
        { 16, 5, 6, 5, 0,  11, 5, 0, 0 },
        /* 32-bit A8R8G8B8 */
        { 32, 8, 8, 8, 8,  16, 8, 0, 24 },
    };

    /* Depth/stencil parameters */
    static const struct {
        BYTE depthBits, stencilBits;
    } dsConfigs[] = {
        { 16, 0 },
        { 16, 8 },
        { 24, 0 },
        { 24, 8 },
    };

    int ci, di;

    s_formatCount = 0;

    for (ci = 0; ci < 2; ci++) {
        for (di = 0; di < 4; di++) {
            if (s_formatCount >= GLD_PF46_MAX_FORMATS)
                break;

            sInitPFD(&s_formats[s_formatCount],
                     colorConfigs[ci].colorBits,
                     colorConfigs[ci].redBits,
                     colorConfigs[ci].greenBits,
                     colorConfigs[ci].blueBits,
                     colorConfigs[ci].alphaBits,
                     colorConfigs[ci].redShift,
                     colorConfigs[ci].greenShift,
                     colorConfigs[ci].blueShift,
                     colorConfigs[ci].alphaShift,
                     dsConfigs[di].depthBits,
                     dsConfigs[di].stencilBits);

            s_formatCount++;
        }
    }

    gldLogPrintf(GLDLOG_INFO,
        "gldBuildPixelFormatList46: enumerated %d pixel formats",
        s_formatCount);

    return s_formatCount;
}

// ***********************************************************************

int gldChoosePixelFormat46(HDC hDC, const PIXELFORMATDESCRIPTOR *ppfd)
{
    int bestIndex = 0;
    int bestScore = -1;
    int i;

    (void)hDC;  /* Reserved for future use */

    if (!ppfd) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldChoosePixelFormat46: NULL PFD pointer");
        return 0;
    }

    if (s_formatCount == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldChoosePixelFormat46: no pixel formats available "
            "(call gldBuildPixelFormatList46 first)");
        return 0;
    }

    for (i = 0; i < s_formatCount; i++) {
        int score = sMatchScore(ppfd, &s_formats[i]);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i + 1;  /* 1-based index */
        }
    }

    if (bestScore < 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldChoosePixelFormat46: no compatible format found");
        return 0;
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldChoosePixelFormat46: selected format %d (score %d)",
        bestIndex, bestScore);

    return bestIndex;
}

// ***********************************************************************

int gldDescribePixelFormat46(HDC hDC, int format, UINT size,
                             PIXELFORMATDESCRIPTOR *ppfd)
{
    (void)hDC;  /* Reserved for future use */

    if (s_formatCount == 0)
        return 0;

    /* If ppfd is NULL, just return the format count */
    if (!ppfd)
        return s_formatCount;

    if (size < sizeof(PIXELFORMATDESCRIPTOR))
        return s_formatCount;

    if (format < 1 || format > s_formatCount) {
        gldLogPrintf(GLDLOG_WARN,
            "gldDescribePixelFormat46: format %d out of range [1, %d]",
            format, s_formatCount);
        return s_formatCount;
    }

    /* Copy the PFD for the requested format (1-based index) */
    memcpy(ppfd, &s_formats[format - 1], sizeof(PIXELFORMATDESCRIPTOR));

    return s_formatCount;
}

// ***********************************************************************

int gldGetPixelFormatCount46(void)
{
    return s_formatCount;
}

// ***********************************************************************
