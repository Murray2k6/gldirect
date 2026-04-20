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
*               translation, and texture lock/unlock (LockRect/UnlockRect)
*               for DX9-to-GL texture operations.
*
*********************************************************************************/

#ifndef __GL46_TEXTURE_MANAGER_H
#define __GL46_TEXTURE_MANAGER_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * D3DTEXTUREADDRESS constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally with #ifndef guards so we don't require the DX9 SDK
 * headers in all build configurations.
 */
#ifndef D3DTADDRESS_WRAP
#define D3DTADDRESS_WRAP        1
#endif
#ifndef D3DTADDRESS_MIRROR
#define D3DTADDRESS_MIRROR      2
#endif
#ifndef D3DTADDRESS_CLAMP
#define D3DTADDRESS_CLAMP       3
#endif
#ifndef D3DTADDRESS_BORDER
#define D3DTADDRESS_BORDER      4
#endif

/*
 * D3DTEXTUREFILTERTYPE constants from the DirectX 9 SDK (d3d9types.h).
 */
#ifndef D3DTEXF_NONE
#define D3DTEXF_NONE            0
#endif
#ifndef D3DTEXF_POINT
#define D3DTEXF_POINT           1
#endif
#ifndef D3DTEXF_LINEAR
#define D3DTEXF_LINEAR          2
#endif
#ifndef D3DTEXF_ANISOTROPIC
#define D3DTEXF_ANISOTROPIC     3
#endif

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Create an OpenGL texture object from DX9 texture parameters.
 *
 * Creates a GL_TEXTURE_2D via glGenTextures/glBindTexture, uses the
 * Format_Mapper (gldMapD3DFormat) to obtain the GL format tuple, and
 * uploads pixel data via glTexImage2D (or glCompressedTexImage2D for
 * DXT formats).
 *
 * When autoGenMipmap is TRUE, glGenerateMipmap(GL_TEXTURE_2D) is called
 * after the base level upload.
 *
 * For formats requiring channel remapping (A8, L8, A8L8), texture
 * swizzle masks are configured via glTexParameteri.
 *
 * Multi-level mipmap upload: when levels > 1 and data is non-NULL,
 * each mip level is uploaded with dimensions max(1, width >> level)
 * by max(1, height >> level).  The data pointer is advanced by the
 * size of each level.
 *
 * Parameters:
 *   ctx           - opaque context pointer (reserved; may be NULL).
 *   d3dFormat     - the D3DFMT enumeration value for the texture format.
 *   width         - base level width in pixels.
 *   height        - base level height in pixels.
 *   levels        - number of mipmap levels (1 = base only).
 *   autoGenMipmap - TRUE to call glGenerateMipmap after base upload.
 *   data          - pointer to the pixel data for all levels, or NULL
 *                   to create an empty texture.
 *
 * Returns:
 *   The GLuint texture id on success, or 0 on failure.
 */
GLuint gldCreateTexture46(void *ctx, DWORD d3dFormat,
                          UINT width, UINT height, UINT levels,
                          BOOL autoGenMipmap, const void *data);

/*
 * Translate DX9 sampler state to OpenGL texture parameters.
 *
 * Binds the specified texture and configures wrap modes, min/mag
 * filters, and anisotropy via glTexParameteri/glTexParameterf.
 *
 * Address mode mapping:
 *   D3DTADDRESS_WRAP   -> GL_REPEAT
 *   D3DTADDRESS_MIRROR -> GL_MIRRORED_REPEAT
 *   D3DTADDRESS_CLAMP  -> GL_CLAMP_TO_EDGE
 *   D3DTADDRESS_BORDER -> GL_CLAMP_TO_BORDER
 *
 * Filter mode mapping:
 *   D3DTEXF_POINT      -> GL_NEAREST
 *   D3DTEXF_LINEAR     -> GL_LINEAR
 *   D3DTEXF_ANISOTROPIC-> GL_LINEAR + anisotropy
 *
 * Combined min/mip filter:
 *   POINT  + NONE   -> GL_NEAREST
 *   LINEAR + NONE   -> GL_LINEAR
 *   POINT  + POINT  -> GL_NEAREST_MIPMAP_NEAREST
 *   POINT  + LINEAR -> GL_NEAREST_MIPMAP_LINEAR
 *   LINEAR + POINT  -> GL_LINEAR_MIPMAP_NEAREST
 *   LINEAR + LINEAR -> GL_LINEAR_MIPMAP_LINEAR
 *
 * Anisotropy is clamped to the GPU maximum reported by
 * glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY).
 *
 * Parameters:
 *   texId         - the OpenGL texture id to configure.
 *   addressU      - D3DTEXTUREADDRESS for the U (S) axis.
 *   addressV      - D3DTEXTUREADDRESS for the V (T) axis.
 *   minFilter     - D3DTEXTUREFILTERTYPE for minification.
 *   magFilter     - D3DTEXTUREFILTERTYPE for magnification.
 *   mipFilter     - D3DTEXTUREFILTERTYPE for mipmap selection.
 *   maxAnisotropy - maximum anisotropy value (D3DSAMP_MAXANISOTROPY).
 */
void gldSetSamplerState46(GLuint texId, DWORD addressU, DWORD addressV,
                          DWORD minFilter, DWORD magFilter,
                          DWORD mipFilter, DWORD maxAnisotropy);

/*
 * Lock a texture level for CPU read/write access (LockRect).
 *
 * Reads back the texture data via glGetTexImage into a CPU staging
 * buffer.  Returns a pointer to the data and the row pitch.
 *
 * The staging buffer is managed internally (malloc/free) and stored
 * in a file-static variable.  Only one texture lock may be active
 * at a time.
 *
 * Parameters:
 *   texId     - the OpenGL texture id to lock.
 *   level     - the mipmap level to lock.
 *   lockRect  - reserved for sub-rectangle locking (currently unused;
 *               pass NULL to lock the entire level).
 *   ppData    - receives a pointer to the locked pixel data.
 *   pPitch    - receives the row pitch in bytes.
 *   d3dFormat - the D3DFMT of the texture (for computing pitch).
 *   width     - width of the mip level in pixels.
 *   height    - height of the mip level in pixels.
 *
 * Returns:
 *   TRUE on success, FALSE on failure.
 */
BOOL gldLockTexture46(GLuint texId, UINT level, const void *lockRect,
                      void **ppData, int *pPitch,
                      DWORD d3dFormat, UINT width, UINT height);

/*
 * Unlock a previously locked texture level (UnlockRect).
 *
 * Uploads the modified staging buffer data back to the texture via
 * glTexSubImage2D.  Frees the staging buffer.
 *
 * Parameters:
 *   texId     - the OpenGL texture id to unlock.
 *   level     - the mipmap level to unlock.
 *   d3dFormat - the D3DFMT of the texture (for format lookup).
 *   width     - width of the mip level in pixels.
 *   height    - height of the mip level in pixels.
 *
 * Returns:
 *   TRUE on success, FALSE on failure.
 */
BOOL gldUnlockTexture46(GLuint texId, UINT level,
                        DWORD d3dFormat, UINT width, UINT height);

#ifdef  __cplusplus
}
#endif

#endif
