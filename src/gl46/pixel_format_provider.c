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
#include "context_manager.h"
#include "gld_log.h"
#include "gld_globals.h"
#include <string.h>

/*---------------------- Static module state ----------------------*/

/*
 * Enumerated pixel formats.
 *
 * Each entry pairs the PIXELFORMATDESCRIPTOR the application sees with the
 * D3D9 surface formats that back it, so device creation can build a swap
 * chain that actually provides what the format promised.  The list is probed
 * against the d3d9.dll that is really loaded rather than assumed: a DXVK or
 * RTX Remix d3d9.dll accepts a narrower set of D3DFORMATs than a stock
 * runtime, and enumerating a format it will refuse produces a pixel format
 * that fails at device creation instead of at selection.
 */
static GLD_pf46Entry s_formats[GLD_PF46_MAX_FORMATS];
static int s_formatCount = 0;

/* TRUE once at least one multisampled format has been enumerated. */
static BOOL s_haveMultisample = FALSE;

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

    /*
     * Prefer matching depth buffer.
     *
     * A request for zero depth bits means "don't care", not "no depth
     * buffer": bootstrap windows are routinely filled in that way, and every
     * real driver still hands back a format that has depth.  Scoring an exact
     * match on zero would make a depth-less format win, and because the D3D9
     * device is process-global and built from whichever format is selected
     * when it is created, that loses depth for the whole session.
     */
    if (requested->cDepthBits == 0) {
        if (candidate->cDepthBits > 0)
            score += 10;
    } else {
        if (candidate->cDepthBits >= requested->cDepthBits)
            score += 10;
        if (candidate->cDepthBits == requested->cDepthBits)
            score += 5;
    }

    /* Prefer matching stencil buffer, on the same reasoning. */
    if (requested->cStencilBits == 0) {
        if (candidate->cStencilBits > 0)
            score += 5;
    } else {
        if (candidate->cStencilBits >= requested->cStencilBits)
            score += 5;
        if (candidate->cStencilBits == requested->cStencilBits)
            score += 3;
    }

    /* Prefer matching alpha */
    if (candidate->cAlphaBits >= requested->cAlphaBits)
        score += 3;

    return score;
}

/* ===================================================================
 * Public API
 * =================================================================== */

/* Colour formats worth offering as a windowed back buffer, best first. */
static const struct {
    D3DFORMAT fmt;
    BYTE colorBits, redBits, greenBits, blueBits, alphaBits;
    BYTE redShift, greenShift, blueShift, alphaShift;
} s_colorCandidates[] = {
    { D3DFMT_A8R8G8B8, 32, 8, 8, 8, 8,  16, 8, 0, 24 },
    { D3DFMT_X8R8G8B8, 32, 8, 8, 8, 0,  16, 8, 0,  0 },
    { D3DFMT_A1R5G5B5, 16, 5, 5, 5, 1,  10, 5, 0, 15 },
    { D3DFMT_X1R5G5B5, 16, 5, 5, 5, 0,  10, 5, 0,  0 },
    { D3DFMT_R5G6B5,   16, 5, 6, 5, 0,  11, 5, 0,  0 },
};

/*
 * Depth-stencil formats, best first.  D3DFMT_UNKNOWN is the depth-less
 * entry: an application is entitled to ask for a format with no depth
 * buffer, and offering one costs a swap chain nothing.
 */
static const struct {
    D3DFORMAT fmt;
    BYTE depthBits, stencilBits;
} s_depthCandidates[] = {
    { D3DFMT_D24S8,   24, 8 },
    { D3DFMT_D24X8,   24, 0 },
    { D3DFMT_D32,     32, 0 },
    { D3DFMT_D16,     16, 0 },
    { D3DFMT_D15S1,   15, 1 },
    { D3DFMT_UNKNOWN,  0, 0 },
};

/* Sample counts to probe. NONE first so an application that never asks for
 * multisampling is never handed a multisampled format by preference. */
static const D3DMULTISAMPLE_TYPE s_msCandidates[] = {
    D3DMULTISAMPLE_NONE,
    D3DMULTISAMPLE_2_SAMPLES,
    D3DMULTISAMPLE_4_SAMPLES,
    D3DMULTISAMPLE_8_SAMPLES,
    D3DMULTISAMPLE_16_SAMPLES,
};

/*
 * Fallback list used when D3D9 is not available to probe.  Keeps the wrapper
 * enumerating something so pixel format selection and context creation still
 * work in a headless or pre-initialisation call.
 */
static int sBuildAssumedList(void)
{
    static const struct { BYTE depthBits, stencilBits; D3DFORMAT fmt; } ds[] = {
        { 24, 8, D3DFMT_D24S8 },
        { 24, 0, D3DFMT_D24X8 },
        { 16, 0, D3DFMT_D16   },
        {  0, 0, D3DFMT_UNKNOWN },
    };
    int i;

    s_formatCount = 0;
    s_haveMultisample = FALSE;

    for (i = 0; i < (int)(sizeof(ds) / sizeof(ds[0])); i++) {
        GLD_pf46Entry *e = &s_formats[s_formatCount];

        memset(e, 0, sizeof(*e));
        sInitPFD(&e->pfd, 32, 8, 8, 8, 8, 16, 8, 0, 24,
                 ds[i].depthBits, ds[i].stencilBits);
        e->colorFormat = D3DFMT_X8R8G8B8;
        e->depthFormat = ds[i].fmt;
        e->msType      = D3DMULTISAMPLE_NONE;
        e->msQuality   = 0;
        e->samples     = 0;
        s_formatCount++;
    }

    gldLogPrintf(GLDLOG_WARN,
        "gldBuildPixelFormatList46: D3D9 unavailable, assuming %d formats",
        s_formatCount);

    return s_formatCount;
}

int gldBuildPixelFormatList46(void)
{
    IDirect3D9     *pD3D = gldGetD3D46();
    D3DDISPLAYMODE  mode;
    int             ci, di, mi;
    int             probedColor = 0;

    s_formatCount = 0;
    s_haveMultisample = FALSE;

    if (!pD3D)
        return sBuildAssumedList();

    ZeroMemory(&mode, sizeof(mode));
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(pD3D, glb.dwAdapter, &mode)))
        return sBuildAssumedList();

    /*
     * Windowed presentation, so every colour format has to be one the runtime
     * will present against the current desktop mode.  On a 32-bit desktop that
     * normally rules the 16-bit formats out, which is the honest answer: an
     * application asking for 16-bit colour then matches a 32-bit format under
     * the "at least this many bits" rule instead of being handed a format the
     * device would refuse to create.
     */
    for (ci = 0; ci < (int)(sizeof(s_colorCandidates) / sizeof(s_colorCandidates[0])); ci++) {
        D3DFORMAT colorFmt = s_colorCandidates[ci].fmt;

        if (FAILED(IDirect3D9_CheckDeviceType(pD3D, glb.dwAdapter, D3DDEVTYPE_HAL,
                                              mode.Format, colorFmt, TRUE)))
            continue;

        probedColor++;

        for (di = 0; di < (int)(sizeof(s_depthCandidates) / sizeof(s_depthCandidates[0])); di++) {
            D3DFORMAT depthFmt = s_depthCandidates[di].fmt;

            if (depthFmt != D3DFMT_UNKNOWN) {
                if (FAILED(IDirect3D9_CheckDeviceFormat(pD3D, glb.dwAdapter,
                        D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_DEPTHSTENCIL,
                        D3DRTYPE_SURFACE, depthFmt)))
                    continue;

                /* A depth format the runtime accepts on its own may still be
                 * unusable with this colour format. */
                if (FAILED(IDirect3D9_CheckDepthStencilMatch(pD3D, glb.dwAdapter,
                        D3DDEVTYPE_HAL, mode.Format, colorFmt, depthFmt)))
                    continue;
            }

            for (mi = 0; mi < (int)(sizeof(s_msCandidates) / sizeof(s_msCandidates[0])); mi++) {
                D3DMULTISAMPLE_TYPE msType = s_msCandidates[mi];
                DWORD  msQualityLevels = 0;
                DWORD  msQuality = 0;
                GLD_pf46Entry *e;

                if (s_formatCount >= GLD_PF46_MAX_FORMATS)
                    break;

                if (msType != D3DMULTISAMPLE_NONE) {
                    if (FAILED(IDirect3D9_CheckDeviceMultiSampleType(pD3D,
                            glb.dwAdapter, D3DDEVTYPE_HAL, colorFmt, TRUE,
                            msType, &msQualityLevels)))
                        continue;

                    /* The depth buffer has to carry the same sample count or
                     * the device cannot be created with the pair. */
                    if (depthFmt != D3DFMT_UNKNOWN &&
                        FAILED(IDirect3D9_CheckDeviceMultiSampleType(pD3D,
                            glb.dwAdapter, D3DDEVTYPE_HAL, depthFmt, TRUE,
                            msType, NULL)))
                        continue;

                    if (msQualityLevels > 0)
                        msQuality = msQualityLevels - 1;
                }

                e = &s_formats[s_formatCount];
                memset(e, 0, sizeof(*e));

                sInitPFD(&e->pfd,
                         s_colorCandidates[ci].colorBits,
                         s_colorCandidates[ci].redBits,
                         s_colorCandidates[ci].greenBits,
                         s_colorCandidates[ci].blueBits,
                         s_colorCandidates[ci].alphaBits,
                         s_colorCandidates[ci].redShift,
                         s_colorCandidates[ci].greenShift,
                         s_colorCandidates[ci].blueShift,
                         s_colorCandidates[ci].alphaShift,
                         s_depthCandidates[di].depthBits,
                         s_depthCandidates[di].stencilBits);

                e->colorFormat = colorFmt;
                e->depthFormat = depthFmt;
                e->msType      = msType;
                e->msQuality   = msQuality;
                e->samples     = (int)msType;   /* D3DMULTISAMPLE_n_SAMPLES == n */

                if (e->samples > 0)
                    s_haveMultisample = TRUE;

                s_formatCount++;
            }
        }
    }

    /* A runtime that refused every colour format still has to produce a
     * usable list; the device creation path logs the real failure. */
    if (s_formatCount == 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldBuildPixelFormatList46: D3D9 accepted none of the %d probed "
            "colour formats for windowed presentation", probedColor);
        return sBuildAssumedList();
    }

    gldLogPrintf(GLDLOG_INFO,
        "gldBuildPixelFormatList46: enumerated %d pixel formats "
        "(multisampled formats: %s)",
        s_formatCount, s_haveMultisample ? "yes" : "none");

    return s_formatCount;
}

// ***********************************************************************

BOOL gldGetPixelFormatD3D46(int format, GLD_pf46Entry *out)
{
    if (!out)
        return FALSE;

    if (format < 1 || format > s_formatCount)
        return FALSE;

    *out = s_formats[format - 1];
    return TRUE;
}

// ***********************************************************************

BOOL gldHaveMultisampleFormats46(void)
{
    return s_haveMultisample;
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
        int score = sMatchScore(ppfd, &s_formats[i].pfd);
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
    memcpy(ppfd, &s_formats[format - 1].pfd, sizeof(PIXELFORMATDESCRIPTOR));

    return s_formatCount;
}

// ***********************************************************************

int gldGetPixelFormatCount46(void)
{
    return s_formatCount;
}

// ***********************************************************************
// WGL_ARB_pixel_format / WGL_EXT_pixel_format
// ***********************************************************************

/*
 * Attribute tokens.  The ARB and EXT forms of this extension share both
 * the token values and the entry point signatures, so one implementation
 * serves both.  Defined locally so the provider stays independent of any
 * particular <wglext.h>.
 */
#ifndef WGL_NUMBER_PIXEL_FORMATS_ARB
#define WGL_NUMBER_PIXEL_FORMATS_ARB        0x2000
#define WGL_DRAW_TO_WINDOW_ARB              0x2001
#define WGL_DRAW_TO_BITMAP_ARB              0x2002
#define WGL_ACCELERATION_ARB                0x2003
#define WGL_NEED_PALETTE_ARB                0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB         0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB          0x2006
#define WGL_SWAP_METHOD_ARB                 0x2007
#define WGL_NUMBER_OVERLAYS_ARB             0x2008
#define WGL_NUMBER_UNDERLAYS_ARB            0x2009
#define WGL_TRANSPARENT_ARB                 0x200A
#define WGL_SHARE_DEPTH_ARB                 0x200C
#define WGL_SHARE_STENCIL_ARB               0x200D
#define WGL_SHARE_ACCUM_ARB                 0x200E
#define WGL_SUPPORT_GDI_ARB                 0x200F
#define WGL_SUPPORT_OPENGL_ARB              0x2010
#define WGL_DOUBLE_BUFFER_ARB               0x2011
#define WGL_STEREO_ARB                      0x2012
#define WGL_PIXEL_TYPE_ARB                  0x2013
#define WGL_COLOR_BITS_ARB                  0x2014
#define WGL_RED_BITS_ARB                    0x2015
#define WGL_RED_SHIFT_ARB                   0x2016
#define WGL_GREEN_BITS_ARB                  0x2017
#define WGL_GREEN_SHIFT_ARB                 0x2018
#define WGL_BLUE_BITS_ARB                   0x2019
#define WGL_BLUE_SHIFT_ARB                  0x201A
#define WGL_ALPHA_BITS_ARB                  0x201B
#define WGL_ALPHA_SHIFT_ARB                 0x201C
#define WGL_ACCUM_BITS_ARB                  0x201D
#define WGL_ACCUM_RED_BITS_ARB              0x201E
#define WGL_ACCUM_GREEN_BITS_ARB            0x201F
#define WGL_ACCUM_BLUE_BITS_ARB             0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB            0x2021
#define WGL_DEPTH_BITS_ARB                  0x2022
#define WGL_STENCIL_BITS_ARB                0x2023
#define WGL_AUX_BUFFERS_ARB                 0x2024
#define WGL_NO_ACCELERATION_ARB             0x2025
#define WGL_GENERIC_ACCELERATION_ARB        0x2026
#define WGL_FULL_ACCELERATION_ARB           0x2027
#define WGL_SWAP_EXCHANGE_ARB               0x2028
#define WGL_SWAP_COPY_ARB                   0x2029
#define WGL_SWAP_UNDEFINED_ARB              0x202A
#define WGL_TYPE_RGBA_ARB                   0x202B
#define WGL_TYPE_COLORINDEX_ARB             0x202C
#define WGL_DRAW_TO_PBUFFER_ARB             0x202D
#define WGL_TRANSPARENT_RED_VALUE_ARB       0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB     0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB      0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB     0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB     0x203B
#define WGL_SAMPLE_BUFFERS_ARB              0x2041
#define WGL_SAMPLES_ARB                     0x2042
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB    0x20A9
#endif

/* WGL boolean attribute values, spelled GL_TRUE / GL_FALSE by the spec. */
#define GLD_PF46_TRUE           1
#define GLD_PF46_FALSE          0

/* Maximum attribute pairs accepted from a caller's list. */
#define GLD_PF46_MAX_ATTRIBS    64

/*
 * How a requested attribute participates in format selection.
 */
typedef enum {
    GLD_PF46_MATCH_EXACT,       /* boolean/enum: value must be identical  */
    GLD_PF46_MATCH_MINIMUM,     /* size: candidate must be >= requested   */
    GLD_PF46_MATCH_IGNORE       /* informational only, never constrains   */
} GLD_pf46MatchKind;

/*
 * Read a single WGL attribute of an enumerated format.
 *
 * Returns FALSE for attributes this backend does not know about; the
 * caller decides whether that is fatal.
 */
static BOOL sGetAttribValue(const GLD_pf46Entry *entry, int attrib,
                            int *value)
{
    const PIXELFORMATDESCRIPTOR *pfd = &entry->pfd;

    switch (attrib) {

    case WGL_NUMBER_PIXEL_FORMATS_ARB:
        *value = s_formatCount;
        return TRUE;

    /* Drawable targets: this backend presents through a D3D9 swap chain
     * bound to a window, so window drawing is the only supported target. */
    case WGL_DRAW_TO_WINDOW_ARB:
        *value = (pfd->dwFlags & PFD_DRAW_TO_WINDOW) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_DRAW_TO_BITMAP_ARB:
        *value = (pfd->dwFlags & PFD_DRAW_TO_BITMAP) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_DRAW_TO_PBUFFER_ARB:
        *value = GLD_PF46_FALSE;
        return TRUE;

    case WGL_ACCELERATION_ARB:
        *value = (pfd->dwFlags & PFD_GENERIC_FORMAT) ?
                 WGL_NO_ACCELERATION_ARB : WGL_FULL_ACCELERATION_ARB;
        return TRUE;

    case WGL_NEED_PALETTE_ARB:
        *value = (pfd->dwFlags & PFD_NEED_PALETTE) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_NEED_SYSTEM_PALETTE_ARB:
        *value = (pfd->dwFlags & PFD_NEED_SYSTEM_PALETTE) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;

    case WGL_SWAP_LAYER_BUFFERS_ARB:
        *value = GLD_PF46_FALSE;
        return TRUE;
    case WGL_SWAP_METHOD_ARB:
        /* The swap chain is created with D3DSWAPEFFECT_DISCARD. */
        *value = WGL_SWAP_UNDEFINED_ARB;
        return TRUE;

    case WGL_NUMBER_OVERLAYS_ARB:
    case WGL_NUMBER_UNDERLAYS_ARB:
        *value = 0;
        return TRUE;

    case WGL_TRANSPARENT_ARB:
        *value = GLD_PF46_FALSE;
        return TRUE;
    case WGL_TRANSPARENT_RED_VALUE_ARB:
    case WGL_TRANSPARENT_GREEN_VALUE_ARB:
    case WGL_TRANSPARENT_BLUE_VALUE_ARB:
    case WGL_TRANSPARENT_ALPHA_VALUE_ARB:
    case WGL_TRANSPARENT_INDEX_VALUE_ARB:
        *value = 0;
        return TRUE;

    /* Only the main plane exists, and it trivially shares its own
     * buffers, which is what these queries report for plane 0. */
    case WGL_SHARE_DEPTH_ARB:
    case WGL_SHARE_STENCIL_ARB:
    case WGL_SHARE_ACCUM_ARB:
        *value = GLD_PF46_TRUE;
        return TRUE;

    case WGL_SUPPORT_GDI_ARB:
        *value = (pfd->dwFlags & PFD_SUPPORT_GDI) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_SUPPORT_OPENGL_ARB:
        *value = (pfd->dwFlags & PFD_SUPPORT_OPENGL) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_DOUBLE_BUFFER_ARB:
        *value = (pfd->dwFlags & PFD_DOUBLEBUFFER) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;
    case WGL_STEREO_ARB:
        *value = (pfd->dwFlags & PFD_STEREO) ? GLD_PF46_TRUE : GLD_PF46_FALSE;
        return TRUE;

    case WGL_PIXEL_TYPE_ARB:
        *value = (pfd->iPixelType == PFD_TYPE_COLORINDEX) ?
                 WGL_TYPE_COLORINDEX_ARB : WGL_TYPE_RGBA_ARB;
        return TRUE;

    case WGL_COLOR_BITS_ARB:    *value = pfd->cColorBits;   return TRUE;
    case WGL_RED_BITS_ARB:      *value = pfd->cRedBits;     return TRUE;
    case WGL_RED_SHIFT_ARB:     *value = pfd->cRedShift;    return TRUE;
    case WGL_GREEN_BITS_ARB:    *value = pfd->cGreenBits;   return TRUE;
    case WGL_GREEN_SHIFT_ARB:   *value = pfd->cGreenShift;  return TRUE;
    case WGL_BLUE_BITS_ARB:     *value = pfd->cBlueBits;    return TRUE;
    case WGL_BLUE_SHIFT_ARB:    *value = pfd->cBlueShift;   return TRUE;
    case WGL_ALPHA_BITS_ARB:    *value = pfd->cAlphaBits;   return TRUE;
    case WGL_ALPHA_SHIFT_ARB:   *value = pfd->cAlphaShift;  return TRUE;

    case WGL_ACCUM_BITS_ARB:        *value = pfd->cAccumBits;       return TRUE;
    case WGL_ACCUM_RED_BITS_ARB:    *value = pfd->cAccumRedBits;    return TRUE;
    case WGL_ACCUM_GREEN_BITS_ARB:  *value = pfd->cAccumGreenBits;  return TRUE;
    case WGL_ACCUM_BLUE_BITS_ARB:   *value = pfd->cAccumBlueBits;   return TRUE;
    case WGL_ACCUM_ALPHA_BITS_ARB:  *value = pfd->cAccumAlphaBits;  return TRUE;

    case WGL_DEPTH_BITS_ARB:    *value = pfd->cDepthBits;   return TRUE;
    case WGL_STENCIL_BITS_ARB:  *value = pfd->cStencilBits; return TRUE;
    case WGL_AUX_BUFFERS_ARB:   *value = pfd->cAuxBuffers;  return TRUE;

    case WGL_SAMPLE_BUFFERS_ARB:
        *value = (entry->samples > 0) ? 1 : 0;
        return TRUE;
    case WGL_SAMPLES_ARB:
        *value = entry->samples;
        return TRUE;

    /* No sRGB-capable formats are enumerated. */
    case WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB:
        *value = 0;
        return TRUE;
    }

    return FALSE;
}

/*
 * Classify a requested attribute for the matching pass.
 */
static GLD_pf46MatchKind sMatchKind(int attrib)
{
    switch (attrib) {

    /* Boolean/enum attributes must match exactly. */
    case WGL_DRAW_TO_WINDOW_ARB:
    case WGL_DRAW_TO_BITMAP_ARB:
    case WGL_DRAW_TO_PBUFFER_ARB:
    case WGL_ACCELERATION_ARB:
    case WGL_NEED_PALETTE_ARB:
    case WGL_NEED_SYSTEM_PALETTE_ARB:
    case WGL_SWAP_LAYER_BUFFERS_ARB:
    case WGL_SWAP_METHOD_ARB:
    case WGL_TRANSPARENT_ARB:
    case WGL_SHARE_DEPTH_ARB:
    case WGL_SHARE_STENCIL_ARB:
    case WGL_SHARE_ACCUM_ARB:
    case WGL_SUPPORT_GDI_ARB:
    case WGL_SUPPORT_OPENGL_ARB:
    case WGL_DOUBLE_BUFFER_ARB:
    case WGL_STEREO_ARB:
    case WGL_PIXEL_TYPE_ARB:
    case WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB:
        return GLD_PF46_MATCH_EXACT;

    /* Sizes: the format must provide at least the requested amount. */
    case WGL_COLOR_BITS_ARB:
    case WGL_RED_BITS_ARB:
    case WGL_GREEN_BITS_ARB:
    case WGL_BLUE_BITS_ARB:
    case WGL_ALPHA_BITS_ARB:
    case WGL_ACCUM_BITS_ARB:
    case WGL_ACCUM_RED_BITS_ARB:
    case WGL_ACCUM_GREEN_BITS_ARB:
    case WGL_ACCUM_BLUE_BITS_ARB:
    case WGL_ACCUM_ALPHA_BITS_ARB:
    case WGL_DEPTH_BITS_ARB:
    case WGL_STENCIL_BITS_ARB:
    case WGL_AUX_BUFFERS_ARB:
    case WGL_SAMPLE_BUFFERS_ARB:
    case WGL_SAMPLES_ARB:
        return GLD_PF46_MATCH_MINIMUM;
    }

    /* Shifts, counts and anything unrecognised describe a format but
     * never select one. */
    return GLD_PF46_MATCH_IGNORE;
}

/*
 * TRUE for constraints this backend can never satisfy, whatever format
 * is picked.  These are the first thing dropped when a strict match
 * finds nothing.
 */
static BOOL sIsUnsatisfiable(int attrib, int value)
{
    switch (attrib) {
    /* Multisampling is no longer categorically unavailable — the enumerated
     * list carries whatever sample counts the runtime accepted, so a sample
     * request is left to ordinary "at least this many" matching. */
    case WGL_ACCUM_BITS_ARB:
    case WGL_ACCUM_RED_BITS_ARB:
    case WGL_ACCUM_GREEN_BITS_ARB:
    case WGL_ACCUM_BLUE_BITS_ARB:
    case WGL_ACCUM_ALPHA_BITS_ARB:
    case WGL_AUX_BUFFERS_ARB:
        return (value > 0);
    case WGL_STEREO_ARB:
    case WGL_DRAW_TO_BITMAP_ARB:
    case WGL_DRAW_TO_PBUFFER_ARB:
    case WGL_SUPPORT_GDI_ARB:
    case WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB:
        return (value != 0);
    case WGL_SWAP_METHOD_ARB:
        return (value != WGL_SWAP_UNDEFINED_ARB);
    /* Every format presents through a double-buffered D3D9 swap chain
     * and carries RGBA colour; there is no single-buffered or
     * colour-index format to fall back to. */
    case WGL_DOUBLE_BUFFER_ARB:
        return (value == 0);
    case WGL_PIXEL_TYPE_ARB:
        return (value == WGL_TYPE_COLORINDEX_ARB);
    }
    return FALSE;
}

/*
 * Intrinsic quality of a format, used to order matches that satisfy the
 * request equally well.  Deeper colour, depth and stencil first, so an
 * application that only asks for "a window, double buffered" and takes
 * piFormats[0] gets the 32-bit RGBA / D24S8 format.
 */
static int sFormatQuality(const GLD_pf46Entry *entry)
{
    const PIXELFORMATDESCRIPTOR *pfd = &entry->pfd;

    /* Sample count is deliberately absent: a multisampled format is only
     * better when the application asked for one, and letting it win ties
     * would hand multisampling to callers that never requested it. */
    return pfd->cColorBits + pfd->cAlphaBits +
           pfd->cDepthBits + pfd->cStencilBits;
}

/*
 * Score one candidate against the parsed attribute list.
 *
 * relaxUnsatisfiable drops constraints this backend cannot meet;
 * relaxSizes additionally turns the size minimums into preferences.
 *
 * Returns -1 when the candidate is rejected.
 */
static int sScoreARB(const GLD_pf46Entry *entry,
                     const int *attribs, const int *values, int count,
                     BOOL relaxUnsatisfiable, BOOL relaxSizes)
{
    int score = 0;
    int i;

    for (i = 0; i < count; i++) {
        GLD_pf46MatchKind kind = sMatchKind(attribs[i]);
        int got = 0;

        if (kind == GLD_PF46_MATCH_IGNORE)
            continue;

        if (relaxUnsatisfiable && sIsUnsatisfiable(attribs[i], values[i]))
            continue;

        if (!sGetAttribValue(entry, attribs[i], &got)) {
            /* Unknown attribute: cannot constrain the choice. */
            continue;
        }

        if (kind == GLD_PF46_MATCH_EXACT) {
            if (got != values[i])
                return -1;
            score += 100;
            continue;
        }

        /* Minimum: prefer the smallest value that meets the request. */
        if (got < values[i]) {
            if (!relaxSizes)
                return -1;
            score += 50 - ((values[i] - got) > 49 ? 49 : (values[i] - got));
            continue;
        }
        score += 100 - ((got - values[i]) > 99 ? 99 : (got - values[i]));
    }

    /* Keep the request score dominant over the tie-break. */
    return score * 1000 + sFormatQuality(entry);
}

/*
 * Run one selection pass, filling sorted[] with 0-based format indices
 * ordered best-first.  Returns the number of matches.
 */
static int sCollectMatches(const int *attribs, const int *values, int count,
                           BOOL relaxUnsatisfiable, BOOL relaxSizes,
                           int *sorted)
{
    int scores[GLD_PF46_MAX_FORMATS];
    int matched = 0;
    int i, j;

    for (i = 0; i < s_formatCount; i++) {
        int score = sScoreARB(&s_formats[i], attribs, values, count,
                              relaxUnsatisfiable, relaxSizes);
        if (score < 0)
            continue;

        /* Insertion sort: highest score first, lower index breaks ties. */
        for (j = matched; j > 0 && scores[j - 1] < score; j--) {
            scores[j] = scores[j - 1];
            sorted[j]  = sorted[j - 1];
        }
        scores[j] = score;
        sorted[j] = i;
        matched++;
    }

    return matched;
}

// ***********************************************************************

BOOL gldChoosePixelFormatARB46(HDC hDC, const int *piAttribIList,
                               const FLOAT *pfAttribFList, UINT nMaxFormats,
                               int *piFormats, UINT *nNumFormats)
{
    int  attribs[GLD_PF46_MAX_ATTRIBS];
    int  values[GLD_PF46_MAX_ATTRIBS];
    int  sorted[GLD_PF46_MAX_FORMATS];
    int  count = 0;
    int  matched;
    UINT written;
    UINT i;

    (void)hDC;  /* Every enumerated format is available on every DC */

    if (!nNumFormats)
        return FALSE;

    *nNumFormats = 0;

    if (!piFormats && nMaxFormats > 0)
        return FALSE;

    if (s_formatCount == 0) {
        gldLogPrintf(GLDLOG_WARN,
            "wglChoosePixelFormatARB: format list empty, building it now");
        gldBuildPixelFormatList46();
        if (s_formatCount == 0) {
            gldLogPrintf(GLDLOG_ERROR,
                "wglChoosePixelFormatARB: no pixel formats available");
            return FALSE;
        }
    }

    /* Parse the integer attribute list (attribute/value pairs, 0 ends it). */
    if (piAttribIList) {
        const int *p = piAttribIList;
        while (p[0] != 0) {
            if (count >= GLD_PF46_MAX_ATTRIBS) {
                gldLogPrintf(GLDLOG_WARN,
                    "wglChoosePixelFormatARB: more than %d attributes, "
                    "ignoring the remainder", GLD_PF46_MAX_ATTRIBS);
                break;
            }
            attribs[count] = p[0];
            values[count]  = p[1];
            count++;
            p += 2;
        }
    }

    /*
     * The float list only carries attributes this backend has no
     * enumerated formats for (pbuffer/colourspace values), so it never
     * constrains the choice — but its presence must not fail the call.
     */
    if (pfAttribFList && pfAttribFList[0] != 0.0f) {
        gldLogPrintf(GLDLOG_DEBUG,
            "wglChoosePixelFormatARB: ignoring float attribute list");
    }

    /* Pass 1: honour every constraint. */
    matched = sCollectMatches(attribs, values, count, FALSE, FALSE, sorted);

    /* Pass 2: drop constraints no enumerated format can satisfy
     * (multisampling, stereo, accum/aux buffers, bitmap/pbuffer targets). */
    if (matched == 0) {
        matched = sCollectMatches(attribs, values, count, TRUE, FALSE, sorted);
        if (matched > 0)
            gldLogPrintf(GLDLOG_WARN,
                "wglChoosePixelFormatARB: no exact match, relaxed the "
                "constraints this backend cannot provide");
    }

    /* Pass 3: treat the size minimums as preferences.  Returning nothing
     * makes applications that expect a driver-supplied format abort, so
     * hand back the closest format instead. */
    if (matched == 0) {
        matched = sCollectMatches(attribs, values, count, TRUE, TRUE, sorted);
        if (matched > 0)
            gldLogPrintf(GLDLOG_WARN,
                "wglChoosePixelFormatARB: no format meets the requested "
                "buffer sizes, returning the closest match");
    }

    if (matched == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "wglChoosePixelFormatARB: no pixel format matched");
        return TRUE;    /* Valid call, zero matches */
    }

    written = ((UINT)matched < nMaxFormats) ? (UINT)matched : nMaxFormats;
    for (i = 0; i < written; i++)
        piFormats[i] = sorted[i] + 1;   /* 1-based index */

    *nNumFormats = written;

    gldLogPrintf(GLDLOG_DEBUG,
        "wglChoosePixelFormatARB: %d attribute(s), %d match(es), "
        "returning %u (best format %d)",
        count, matched, written, written ? piFormats[0] : 0);

    return TRUE;
}

// ***********************************************************************

BOOL gldGetPixelFormatAttribivARB46(HDC hDC, int iPixelFormat, int iLayerPlane,
                                    UINT nAttributes, const int *piAttributes,
                                    int *piValues)
{
    const GLD_pf46Entry *entry;
    UINT i;

    (void)hDC;

    if (nAttributes == 0)
        return TRUE;

    if (!piAttributes || !piValues)
        return FALSE;

    /* Only the main plane is implemented. */
    if (iLayerPlane != 0) {
        gldLogPrintf(GLDLOG_WARN,
            "wglGetPixelFormatAttribivARB: layer plane %d not supported",
            iLayerPlane);
        return FALSE;
    }

    if (s_formatCount == 0)
        gldBuildPixelFormatList46();

    /*
     * WGL_NUMBER_PIXEL_FORMATS_ARB may be queried on its own with any
     * format index, so resolve the descriptor leniently and only reject
     * an out-of-range index when a real format attribute is asked for.
     */
    entry = (iPixelFormat >= 1 && iPixelFormat <= s_formatCount) ?
            &s_formats[iPixelFormat - 1] : NULL;

    for (i = 0; i < nAttributes; i++) {
        int value = 0;

        if (piAttributes[i] == WGL_NUMBER_PIXEL_FORMATS_ARB) {
            piValues[i] = s_formatCount;
            continue;
        }

        if (!entry) {
            gldLogPrintf(GLDLOG_WARN,
                "wglGetPixelFormatAttribivARB: format %d out of range [1, %d]",
                iPixelFormat, s_formatCount);
            return FALSE;
        }

        if (!sGetAttribValue(entry, piAttributes[i], &value)) {
            /*
             * An unknown attribute describes a capability this backend
             * does not have.  Reporting zero answers the question
             * truthfully; failing the whole query would lose the
             * attributes the caller asked for alongside it.
             */
            gldLogPrintf(GLDLOG_DEBUG,
                "wglGetPixelFormatAttribivARB: attribute 0x%04X unknown, "
                "reporting 0", piAttributes[i]);
            value = 0;
        }

        piValues[i] = value;
    }

    return TRUE;
}

// ***********************************************************************

BOOL gldGetPixelFormatAttribfvARB46(HDC hDC, int iPixelFormat, int iLayerPlane,
                                    UINT nAttributes, const int *piAttributes,
                                    FLOAT *pfValues)
{
    int  ivalues[GLD_PF46_MAX_ATTRIBS];
    UINT chunk;
    UINT done = 0;

    if (nAttributes == 0)
        return TRUE;

    if (!piAttributes || !pfValues)
        return FALSE;

    /* Reuse the integer path in fixed-size chunks — every attribute this
     * backend exposes is integer-valued. */
    while (done < nAttributes) {
        UINT i;

        chunk = nAttributes - done;
        if (chunk > GLD_PF46_MAX_ATTRIBS)
            chunk = GLD_PF46_MAX_ATTRIBS;

        if (!gldGetPixelFormatAttribivARB46(hDC, iPixelFormat, iLayerPlane,
                                            chunk, piAttributes + done,
                                            ivalues))
            return FALSE;

        for (i = 0; i < chunk; i++)
            pfValues[done + i] = (FLOAT)ivalues[i];

        done += chunk;
    }

    return TRUE;
}

// ***********************************************************************
