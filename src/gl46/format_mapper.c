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

#define STRICT
#include <windows.h>
#include <string.h>

#include <glad/gl.h>
#include "format_mapper.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

/*
 * Static lookup table mapping D3DFMT values to OpenGL format tuples.
 *
 * Swizzle values:
 *   0        — no swizzle override needed (identity mapping).
 *   GL_ZERO  — channel reads as 0.
 *   GL_ONE   — channel reads as 1.
 *   GL_RED   — channel reads from the red component.
 *   GL_GREEN — channel reads from the green component.
 *
 * For most color formats the swizzle fields are 0 (identity), meaning
 * the hardware's default channel mapping is correct.  Swizzle overrides
 * are only needed for formats where DX9 channel semantics differ from
 * the OpenGL format's native layout (A8, L8, A8L8).
 *
 * Compressed formats use bytesPerPixel to store the block size in bytes
 * (8 for DXT1, 16 for DXT3/DXT5).  glFormat and glType are set to 0
 * since compressed uploads use glCompressedTexImage2D.
 *
 * Depth-stencil formats set glFormat/glType to the appropriate depth
 * transfer format for use with glTexImage2D when creating depth textures.
 */
static const GLD_formatMapping s_formatTable[] = {

    // -----------------------------------------------------------------
    // Color formats (ARGB / XRGB / RGB / RGBA)
    // -----------------------------------------------------------------

    /* D3DFMT_A8R8G8B8 → GL_RGBA8 / GL_BGRA / GL_UNSIGNED_INT_8_8_8_8_REV */
    {
        D3DFMT_A8R8G8B8,
        GL_RGBA8,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        0, 0, 0, 0,        /* no swizzle */
        FALSE,
        4
    },

    /* D3DFMT_X8R8G8B8 → GL_RGB8 / GL_BGRA / GL_UNSIGNED_INT_8_8_8_8_REV */
    {
        D3DFMT_X8R8G8B8,
        GL_RGB8,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        0, 0, 0, 0,        /* no swizzle */
        FALSE,
        4
    },

    /* D3DFMT_R5G6B5 → GL_RGB565 / GL_RGB / GL_UNSIGNED_SHORT_5_6_5 */
    {
        D3DFMT_R5G6B5,
        GL_RGB565,
        GL_RGB,
        GL_UNSIGNED_SHORT_5_6_5,
        0, 0, 0, 0,        /* no swizzle */
        FALSE,
        2
    },

    /* D3DFMT_A1R5G5B5 → GL_RGB5_A1 / GL_BGRA / GL_UNSIGNED_SHORT_1_5_5_5_REV */
    {
        D3DFMT_A1R5G5B5,
        GL_RGB5_A1,
        GL_BGRA,
        GL_UNSIGNED_SHORT_1_5_5_5_REV,
        0, 0, 0, 0,        /* no swizzle */
        FALSE,
        2
    },

    /* D3DFMT_A4R4G4B4 → GL_RGBA4 / GL_BGRA / GL_UNSIGNED_SHORT_4_4_4_4_REV */
    {
        D3DFMT_A4R4G4B4,
        GL_RGBA4,
        GL_BGRA,
        GL_UNSIGNED_SHORT_4_4_4_4_REV,
        0, 0, 0, 0,        /* no swizzle */
        FALSE,
        2
    },

    // -----------------------------------------------------------------
    // Single/dual-channel formats with swizzle masks
    // -----------------------------------------------------------------

    /*
     * D3DFMT_A8 → GL_R8 / GL_RED / GL_UNSIGNED_BYTE
     * DX9 A8: alpha-only.  Map to GL_R8 and swizzle (0,0,0,R)
     * so that RGB reads as zero and alpha reads from the red channel.
     */
    {
        D3DFMT_A8,
        GL_R8,
        GL_RED,
        GL_UNSIGNED_BYTE,
        GL_ZERO, GL_ZERO, GL_ZERO, GL_RED,
        FALSE,
        1
    },

    /*
     * D3DFMT_L8 → GL_R8 / GL_RED / GL_UNSIGNED_BYTE
     * DX9 L8: luminance-only.  Map to GL_R8 and swizzle (R,R,R,1)
     * so that RGB all read from the red channel and alpha is 1.
     */
    {
        D3DFMT_L8,
        GL_R8,
        GL_RED,
        GL_UNSIGNED_BYTE,
        GL_RED, GL_RED, GL_RED, GL_ONE,
        FALSE,
        1
    },

    /*
     * D3DFMT_A8L8 → GL_RG8 / GL_RG / GL_UNSIGNED_BYTE
     * DX9 A8L8: luminance + alpha.  Map to GL_RG8 and swizzle (R,R,R,G)
     * so that RGB all read from the red (luminance) channel and alpha
     * reads from the green channel.
     */
    {
        D3DFMT_A8L8,
        GL_RG8,
        GL_RG,
        GL_UNSIGNED_BYTE,
        GL_RED, GL_RED, GL_RED, GL_GREEN,
        FALSE,
        2
    },

    // -----------------------------------------------------------------
    // Compressed formats (S3TC / DXTn)
    // -----------------------------------------------------------------

    /* D3DFMT_DXT1 → GL_COMPRESSED_RGBA_S3TC_DXT1_EXT */
    {
        D3DFMT_DXT1,
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
        0,                  /* not used for compressed uploads */
        0,                  /* not used for compressed uploads */
        0, 0, 0, 0,        /* no swizzle */
        TRUE,
        8                   /* 8 bytes per 4x4 block */
    },

    /* D3DFMT_DXT3 → GL_COMPRESSED_RGBA_S3TC_DXT3_EXT */
    {
        D3DFMT_DXT3,
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
        0,
        0,
        0, 0, 0, 0,
        TRUE,
        16                  /* 16 bytes per 4x4 block */
    },

    /* D3DFMT_DXT5 → GL_COMPRESSED_RGBA_S3TC_DXT5_EXT */
    {
        D3DFMT_DXT5,
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
        0,
        0,
        0, 0, 0, 0,
        TRUE,
        16                  /* 16 bytes per 4x4 block */
    },

    // -----------------------------------------------------------------
    // Depth-stencil formats
    // -----------------------------------------------------------------

    /* D3DFMT_D16 → GL_DEPTH_COMPONENT16 */
    {
        D3DFMT_D16,
        GL_DEPTH_COMPONENT16,
        GL_DEPTH_COMPONENT,
        GL_UNSIGNED_SHORT,
        0, 0, 0, 0,
        FALSE,
        2
    },

    /* D3DFMT_D24S8 → GL_DEPTH24_STENCIL8 */
    {
        D3DFMT_D24S8,
        GL_DEPTH24_STENCIL8,
        GL_DEPTH_STENCIL,
        GL_UNSIGNED_INT_24_8,
        0, 0, 0, 0,
        FALSE,
        4
    },

    /* D3DFMT_D24X8 → GL_DEPTH_COMPONENT24 */
    {
        D3DFMT_D24X8,
        GL_DEPTH_COMPONENT24,
        GL_DEPTH_COMPONENT,
        GL_UNSIGNED_INT,
        0, 0, 0, 0,
        FALSE,
        4
    },

    /* D3DFMT_D32 → GL_DEPTH_COMPONENT32F */
    {
        D3DFMT_D32,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        0, 0, 0, 0,
        FALSE,
        4
    },
};

static const int s_formatTableSize =
    (int)(sizeof(s_formatTable) / sizeof(s_formatTable[0]));

// ***********************************************************************

/*****************************************************************************
 * gldMapD3DFormat
 *
 * Look up the OpenGL format mapping for a given D3DFMT value.
 *
 * Performs a linear search through the static mapping table.  The table
 * is small (< 20 entries) so a linear scan is perfectly adequate.
 *
 * Returns TRUE on success (mapping copied into *out), or FALSE if the
 * format is unsupported (an error is logged).
 *****************************************************************************/
BOOL gldMapD3DFormat(DWORD d3dFormat, GLD_formatMapping *out)
{
    int i;

    if (!out) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldMapD3DFormat: NULL output pointer");
        return FALSE;
    }

    for (i = 0; i < s_formatTableSize; i++) {
        if (s_formatTable[i].d3dFormat == d3dFormat) {
            memcpy(out, &s_formatTable[i], sizeof(GLD_formatMapping));
            return TRUE;
        }
    }

    /* Unsupported format — log and return failure. */
    gldLogPrintf(GLDLOG_ERROR,
        "gldMapD3DFormat: unsupported D3DFMT value %lu (0x%08lX)",
        (unsigned long)d3dFormat, (unsigned long)d3dFormat);
    return FALSE;
}
