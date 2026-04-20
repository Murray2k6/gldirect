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
* Description:  Texture management for the OpenGL 4.6 backend.
*               Handles texture creation, data upload, sampler state
*               translation, and texture lock/unlock (LockRect/UnlockRect).
*
*********************************************************************************/

#define STRICT
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include <glad/gl.h>
#include "texture_manager.h"
#include "format_mapper.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************
// File-static staging buffer for texture lock/unlock.
// Only one lock may be active at a time.
// ***********************************************************************

static void  *s_stagingBuffer     = NULL;
static int    s_stagingBufferSize = 0;

// ***********************************************************************
// Internal helpers
// ***********************************************************************

/*****************************************************************************
 * _gldComputeCompressedSize
 *
 * Compute the byte size of a compressed (DXTn) mip level.
 * DXT block size is 4x4 pixels.  The number of blocks is
 * ceil(width/4) * ceil(height/4), multiplied by the block byte size
 * (8 for DXT1, 16 for DXT3/DXT5).
 *****************************************************************************/
static int _gldComputeCompressedSize(UINT width, UINT height, int blockBytes)
{
    int blocksW = (width  + 3) / 4;
    int blocksH = (height + 3) / 4;
    return blocksW * blocksH * blockBytes;
}

/*****************************************************************************
 * _gldComputeRowPitch
 *
 * Compute the row pitch (bytes per row) for an uncompressed texture level.
 *****************************************************************************/
static int _gldComputeRowPitch(UINT width, int bytesPerPixel)
{
    return (int)width * bytesPerPixel;
}

// ***********************************************************************
// 10.1 — Texture creation and data upload
// ***********************************************************************

/*****************************************************************************
 * gldCreateTexture46
 *
 * Create an OpenGL texture object and optionally upload pixel data.
 *
 * Uses the Format_Mapper to translate the D3DFMT to GL format tuple.
 * Handles compressed (DXT) and uncompressed formats, multi-level mipmap
 * upload, auto mipmap generation, and swizzle mask configuration for
 * formats requiring channel remapping (A8, L8, A8L8).
 *
 * Returns the GLuint texture id, or 0 on failure.
 *****************************************************************************/
GLuint gldCreateTexture46(void *ctx, DWORD d3dFormat,
                          UINT width, UINT height, UINT levels,
                          BOOL autoGenMipmap, const void *data)
{
    GLD_formatMapping fmt;
    GLuint texId = 0;
    UINT level;
    UINT mipW, mipH;
    const unsigned char *pData;

    (void)ctx;  /* reserved for future use */

    /* Look up the GL format mapping for this D3DFMT. */
    if (!gldMapD3DFormat(d3dFormat, &fmt)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateTexture46: unsupported D3DFMT %lu", (unsigned long)d3dFormat);
        return 0;
    }

    /* Ensure at least 1 mip level. */
    if (levels == 0)
        levels = 1;

    /* Create the texture object. */
    glGenTextures(1, &texId);
    if (texId == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateTexture46: glGenTextures failed");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texId);
    GLD_CHECK_GL("glBindTexture", "CreateTexture");

    /* Upload each mip level. */
    pData = (const unsigned char *)data;

    for (level = 0; level < levels; level++) {
        mipW = width  >> level;
        mipH = height >> level;
        if (mipW < 1) mipW = 1;
        if (mipH < 1) mipH = 1;

        if (fmt.isCompressed) {
            int compressedSize = _gldComputeCompressedSize(mipW, mipH,
                                                           fmt.bytesPerPixel);
            glCompressedTexImage2D(
                GL_TEXTURE_2D,
                (GLint)level,
                fmt.glInternalFormat,
                (GLsizei)mipW,
                (GLsizei)mipH,
                0,                      /* border */
                (GLsizei)compressedSize,
                pData                   /* NULL is valid for empty texture */
            );
            GLD_CHECK_GL("glCompressedTexImage2D", "CreateTexture");

            if (pData)
                pData += compressedSize;
        } else {
            glTexImage2D(
                GL_TEXTURE_2D,
                (GLint)level,
                (GLint)fmt.glInternalFormat,
                (GLsizei)mipW,
                (GLsizei)mipH,
                0,                      /* border */
                fmt.glFormat,
                fmt.glType,
                pData                   /* NULL is valid for empty texture */
            );
            GLD_CHECK_GL("glTexImage2D", "CreateTexture");

            if (pData)
                pData += (int)mipW * (int)mipH * fmt.bytesPerPixel;
        }
    }

    /* Auto-generate mipmaps if requested. */
    if (autoGenMipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
        GLD_CHECK_GL("glGenerateMipmap", "CreateTexture");
    }

    /*
     * Configure texture swizzle masks for formats that need channel
     * remapping to match DX9 semantics.
     *
     * A8:   DX9 alpha-only  → GL_R8, swizzle (0,0,0,R)
     * L8:   DX9 luminance   → GL_R8, swizzle (R,R,R,1)
     * A8L8: DX9 lum+alpha   → GL_RG8, swizzle (R,R,R,G)
     */
    if (fmt.swizzleR != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, (GLint)fmt.swizzleR);
    }
    if (fmt.swizzleG != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, (GLint)fmt.swizzleG);
    }
    if (fmt.swizzleB != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, (GLint)fmt.swizzleB);
    }
    if (fmt.swizzleA != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, (GLint)fmt.swizzleA);
    }
    GLD_CHECK_GL("glTexParameteri(SWIZZLE)", "CreateTexture");

    /* Set sensible default sampler state. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    gldLogPrintf(GLDLOG_INFO,
        "gldCreateTexture46: created texture %u (%ux%u, %u levels, fmt=%lu)",
        texId, width, height, levels, (unsigned long)d3dFormat);

    return texId;
}

// ***********************************************************************
// 10.2 — Sampler state translation
// ***********************************************************************

/*****************************************************************************
 * _gldMapAddressMode
 *
 * Map a D3DTEXTUREADDRESS value to the equivalent GL wrap mode.
 *****************************************************************************/
static GLenum _gldMapAddressMode(DWORD d3dAddress)
{
    switch (d3dAddress) {
    case D3DTADDRESS_WRAP:      return GL_REPEAT;
    case D3DTADDRESS_MIRROR:    return GL_MIRRORED_REPEAT;
    case D3DTADDRESS_CLAMP:     return GL_CLAMP_TO_EDGE;
    case D3DTADDRESS_BORDER:    return GL_CLAMP_TO_BORDER;
    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldSetSamplerState46: unknown address mode %lu, defaulting to GL_REPEAT",
            (unsigned long)d3dAddress);
        return GL_REPEAT;
    }
}

/*****************************************************************************
 * _gldMapMinFilter
 *
 * Map the combined DX9 min filter + mip filter to a single GL min filter.
 *
 * Combined min/mip filter mapping:
 *   POINT  + NONE   -> GL_NEAREST
 *   LINEAR + NONE   -> GL_LINEAR
 *   POINT  + POINT  -> GL_NEAREST_MIPMAP_NEAREST
 *   POINT  + LINEAR -> GL_NEAREST_MIPMAP_LINEAR
 *   LINEAR + POINT  -> GL_LINEAR_MIPMAP_NEAREST
 *   LINEAR + LINEAR -> GL_LINEAR_MIPMAP_LINEAR
 *
 * ANISOTROPIC min filter is treated as LINEAR for the base filter,
 * with anisotropy configured separately.
 *****************************************************************************/
static GLenum _gldMapMinFilter(DWORD minFilter, DWORD mipFilter)
{
    /* Determine the base filter (NEAREST or LINEAR). */
    BOOL bLinear = (minFilter == D3DTEXF_LINEAR ||
                    minFilter == D3DTEXF_ANISOTROPIC);

    switch (mipFilter) {
    case D3DTEXF_NONE:
        return bLinear ? GL_LINEAR : GL_NEAREST;

    case D3DTEXF_POINT:
        return bLinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;

    case D3DTEXF_LINEAR:
        return bLinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;

    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldSetSamplerState46: unknown mip filter %lu, defaulting to no mip",
            (unsigned long)mipFilter);
        return bLinear ? GL_LINEAR : GL_NEAREST;
    }
}

/*****************************************************************************
 * _gldMapMagFilter
 *
 * Map a DX9 mag filter to the GL mag filter.
 * GL mag filter only supports GL_NEAREST or GL_LINEAR.
 *****************************************************************************/
static GLenum _gldMapMagFilter(DWORD magFilter)
{
    switch (magFilter) {
    case D3DTEXF_POINT:
        return GL_NEAREST;
    case D3DTEXF_LINEAR:
    case D3DTEXF_ANISOTROPIC:
        return GL_LINEAR;
    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldSetSamplerState46: unknown mag filter %lu, defaulting to GL_LINEAR",
            (unsigned long)magFilter);
        return GL_LINEAR;
    }
}

/*****************************************************************************
 * gldSetSamplerState46
 *
 * Translate DX9 sampler state to OpenGL texture parameters.
 *
 * Binds the texture, sets wrap modes, min/mag filters, and anisotropy.
 * Anisotropy is clamped to the GPU maximum.
 *****************************************************************************/
void gldSetSamplerState46(GLuint texId, DWORD addressU, DWORD addressV,
                          DWORD minFilter, DWORD magFilter,
                          DWORD mipFilter, DWORD maxAnisotropy)
{
    GLenum glWrapS, glWrapT;
    GLenum glMinFilter, glMagFilter;
    GLfloat gpuMaxAniso = 1.0f;
    GLfloat aniso;

    if (texId == 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetSamplerState46: called with texId 0, ignoring");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texId);
    GLD_CHECK_GL("glBindTexture", "SetSamplerState");

    /* Map and set address (wrap) modes. */
    glWrapS = _gldMapAddressMode(addressU);
    glWrapT = _gldMapAddressMode(addressV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)glWrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)glWrapT);
    GLD_CHECK_GL("glTexParameteri(WRAP)", "SetSamplerState");

    /* Map and set filter modes. */
    glMinFilter = _gldMapMinFilter(minFilter, mipFilter);
    glMagFilter = _gldMapMagFilter(magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)glMinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)glMagFilter);
    GLD_CHECK_GL("glTexParameteri(FILTER)", "SetSamplerState");

    /* Configure anisotropic filtering. */
    if (minFilter == D3DTEXF_ANISOTROPIC || magFilter == D3DTEXF_ANISOTROPIC) {
        /* Query the GPU maximum anisotropy. */
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gpuMaxAniso);
        GLD_CHECK_GL("glGetFloatv(MAX_ANISOTROPY)", "SetSamplerState");

        /* Clamp the requested value to the GPU max. */
        aniso = (GLfloat)maxAnisotropy;
        if (aniso < 1.0f)
            aniso = 1.0f;
        if (aniso > gpuMaxAniso)
            aniso = gpuMaxAniso;

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, aniso);
        GLD_CHECK_GL("glTexParameterf(ANISOTROPY)", "SetSamplerState");
    } else {
        /* Reset anisotropy to 1.0 when not using anisotropic filtering. */
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
    }
}

// ***********************************************************************
// 10.3 — Texture lock/unlock (LockRect / UnlockRect)
// ***********************************************************************

/*****************************************************************************
 * gldLockTexture46
 *
 * Read back texture data via glGetTexImage into a CPU staging buffer.
 * Returns a pointer to the data and the row pitch.
 *
 * Only one texture lock may be active at a time.  The staging buffer
 * is allocated via malloc and freed in gldUnlockTexture46.
 *****************************************************************************/
BOOL gldLockTexture46(GLuint texId, UINT level, const void *lockRect,
                      void **ppData, int *pPitch,
                      DWORD d3dFormat, UINT width, UINT height)
{
    GLD_formatMapping fmt;
    int bufferSize;
    int pitch;

    (void)lockRect;  /* sub-rectangle locking not yet implemented */

    if (!ppData || !pPitch) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockTexture46: NULL output pointer");
        return FALSE;
    }

    *ppData = NULL;
    *pPitch = 0;

    /* Reject if a lock is already active. */
    if (s_stagingBuffer != NULL) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockTexture46: a texture lock is already active");
        return FALSE;
    }

    /* Look up the GL format. */
    if (!gldMapD3DFormat(d3dFormat, &fmt)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockTexture46: unsupported D3DFMT %lu", (unsigned long)d3dFormat);
        return FALSE;
    }

    /* Compressed textures are not supported for lock/unlock. */
    if (fmt.isCompressed) {
        gldLogPrintf(GLDLOG_WARN,
            "gldLockTexture46: lock/unlock not supported for compressed formats");
        return FALSE;
    }

    /* Compute buffer size and pitch. */
    pitch = _gldComputeRowPitch(width, fmt.bytesPerPixel);
    bufferSize = pitch * (int)height;

    if (bufferSize <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockTexture46: invalid dimensions %ux%u", width, height);
        return FALSE;
    }

    /* Allocate the staging buffer. */
    s_stagingBuffer = malloc((size_t)bufferSize);
    if (!s_stagingBuffer) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockTexture46: failed to allocate %d byte staging buffer",
            bufferSize);
        return FALSE;
    }
    s_stagingBufferSize = bufferSize;

    /* Read back the texture data. */
    glBindTexture(GL_TEXTURE_2D, texId);
    GLD_CHECK_GL("glBindTexture", "LockRect");

    /* Ensure pixel pack alignment is set to 1 for tight packing. */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glGetTexImage(
        GL_TEXTURE_2D,
        (GLint)level,
        fmt.glFormat,
        fmt.glType,
        s_stagingBuffer
    );
    GLD_CHECK_GL("glGetTexImage", "LockRect");

    *ppData = s_stagingBuffer;
    *pPitch = pitch;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldLockTexture46: locked texture %u level %u (%ux%u, pitch=%d)",
        texId, level, width, height, pitch);

    return TRUE;
}

/*****************************************************************************
 * gldUnlockTexture46
 *
 * Upload the modified staging buffer back to the texture via
 * glTexSubImage2D, then free the staging buffer.
 *****************************************************************************/
BOOL gldUnlockTexture46(GLuint texId, UINT level,
                        DWORD d3dFormat, UINT width, UINT height)
{
    GLD_formatMapping fmt;

    /* Verify we have an active lock. */
    if (!s_stagingBuffer) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldUnlockTexture46: no active texture lock");
        return FALSE;
    }

    /* Look up the GL format. */
    if (!gldMapD3DFormat(d3dFormat, &fmt)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldUnlockTexture46: unsupported D3DFMT %lu", (unsigned long)d3dFormat);
        /* Free the staging buffer even on error. */
        free(s_stagingBuffer);
        s_stagingBuffer = NULL;
        s_stagingBufferSize = 0;
        return FALSE;
    }

    /* Bind the texture and upload the modified data. */
    glBindTexture(GL_TEXTURE_2D, texId);
    GLD_CHECK_GL("glBindTexture", "UnlockRect");

    /* Ensure pixel unpack alignment is set to 1 for tight packing. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage2D(
        GL_TEXTURE_2D,
        (GLint)level,
        0,                          /* xoffset */
        0,                          /* yoffset */
        (GLsizei)width,
        (GLsizei)height,
        fmt.glFormat,
        fmt.glType,
        s_stagingBuffer
    );
    GLD_CHECK_GL("glTexSubImage2D", "UnlockRect");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldUnlockTexture46: unlocked texture %u level %u (%ux%u)",
        texId, level, width, height);

    /* Free the staging buffer. */
    free(s_stagingBuffer);
    s_stagingBuffer = NULL;
    s_stagingBufferSize = 0;

    return TRUE;
}
