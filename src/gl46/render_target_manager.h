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
* Description:  Render target and depth-stencil management for the
*               OpenGL 4.6 backend.  Translates DX9 render target
*               surfaces and depth-stencil surfaces to OpenGL FBOs,
*               handles SetRenderTarget, MRT, StretchRect, readback,
*               and LockRect/UnlockRect on render target surfaces.
*
*********************************************************************************/

#ifndef __GL46_RENDER_TARGET_MANAGER_H
#define __GL46_RENDER_TARGET_MANAGER_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/* Maximum number of simultaneous color attachments for MRT. */
#define GLD_MAX_MRT_ATTACHMENTS 4

/*
 * GLD_renderTarget — describes an FBO with its color texture attachment.
 *
 * Returned by gldCreateRenderTarget46 to give the caller both the
 * FBO id and the color texture id for further operations (MRT attach,
 * readback, etc.).
 */
typedef struct {
    GLuint  fboId;
    GLuint  colorTexId;
} GLD_renderTarget;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Create an FBO with a color texture attachment for use as a render target.
 *
 * Creates an FBO via glGenFramebuffers, creates a color texture with the
 * mapped GL format and dimensions, attaches it as GL_COLOR_ATTACHMENT0,
 * and validates FBO completeness via glCheckFramebufferStatus.
 *
 * Parameters:
 *   ctx       - opaque context pointer (reserved; may be NULL).
 *   d3dFormat - the D3DFMT enumeration value for the color surface format.
 *   width     - render target width in pixels.
 *   height    - render target height in pixels.
 *
 * Returns:
 *   A GLD_renderTarget with fboId and colorTexId on success.
 *   Both fields are 0 on failure.
 */
GLD_renderTarget gldCreateRenderTarget46(void *ctx, DWORD d3dFormat,
                                         UINT width, UINT height);

/*
 * Create a depth-stencil renderbuffer and attach it to an existing FBO.
 *
 * Creates a renderbuffer via glGenRenderbuffers/glRenderbufferStorage
 * with the mapped depth-stencil format, and attaches it to the FBO
 * using GL_DEPTH_STENCIL_ATTACHMENT for D24S8 or GL_DEPTH_ATTACHMENT
 * for depth-only formats.
 *
 * Parameters:
 *   ctx       - opaque context pointer (reserved; may be NULL).
 *   d3dFormat - the D3DFMT enumeration value (D3DFMT_D24S8, D3DFMT_D16, etc.).
 *   width     - renderbuffer width in pixels.
 *   height    - renderbuffer height in pixels.
 *   fbo       - the FBO to attach the renderbuffer to.
 *
 * Returns:
 *   The GLuint renderbuffer id on success, or 0 on failure.
 */
GLuint gldCreateDepthStencil46(void *ctx, DWORD d3dFormat,
                               UINT width, UINT height, GLuint fbo);

/*
 * Bind an FBO as the current render target (SetRenderTarget).
 *
 * Binds the specified FBO via glBindFramebuffer(GL_FRAMEBUFFER, fbo).
 * Pass fbo=0 to bind the default backbuffer.
 *
 * Parameters:
 *   fbo - the FBO to bind, or 0 for the default framebuffer.
 */
void gldSetRenderTarget46(GLuint fbo);

/*
 * Configure multiple render targets (MRT) on an FBO.
 *
 * Attaches the given color textures to GL_COLOR_ATTACHMENT0..N on the
 * specified FBO and calls glDrawBuffers to enable all attachments.
 *
 * Parameters:
 *   fbo         - the FBO to configure.
 *   count       - number of color attachments (1..GLD_MAX_MRT_ATTACHMENTS).
 *   colorTexIds - array of GLuint texture ids to attach.
 */
void gldSetMRT46(GLuint fbo, int count, const GLuint *colorTexIds);

/*
 * Blit pixels between two FBOs (StretchRect).
 *
 * Binds srcFBO to GL_READ_FRAMEBUFFER and dstFBO to GL_DRAW_FRAMEBUFFER,
 * then calls glBlitFramebuffer with the specified source and destination
 * rectangles.
 *
 * Parameters:
 *   srcFBO       - source FBO (0 for default framebuffer).
 *   srcX0..srcY1 - source rectangle.
 *   dstFBO       - destination FBO (0 for default framebuffer).
 *   dstX0..dstY1 - destination rectangle.
 *   linearFilter - TRUE for GL_LINEAR, FALSE for GL_NEAREST.
 */
void gldStretchRect46(GLuint srcFBO,
                      int srcX0, int srcY0, int srcX1, int srcY1,
                      GLuint dstFBO,
                      int dstX0, int dstY0, int dstX1, int dstY1,
                      BOOL linearFilter);

/*
 * Read back render target pixel data (GetRenderTargetData).
 *
 * Binds the source FBO for reading and uses glReadPixels to transfer
 * pixel data into the caller-supplied CPU buffer.
 *
 * Parameters:
 *   srcFBO     - the FBO to read from (0 for default framebuffer).
 *   width      - width of the region to read.
 *   height     - height of the region to read.
 *   format     - GL pixel format (e.g. GL_BGRA).
 *   type       - GL pixel type (e.g. GL_UNSIGNED_INT_8_8_8_8_REV).
 *   destBuffer - pointer to CPU memory to receive the pixel data.
 */
void gldGetRenderTargetData46(GLuint srcFBO, int width, int height,
                              GLenum format, GLenum type, void *destBuffer);

/*
 * Lock a render target surface for CPU read/write access (LockRect).
 *
 * Reads back the framebuffer contents via glReadPixels into a staging
 * buffer allocated internally.  Returns a pointer to the data and the
 * row pitch.
 *
 * Parameters:
 *   fbo    - the FBO to read from.
 *   width  - width of the surface in pixels.
 *   height - height of the surface in pixels.
 *   format - GL pixel format.
 *   type   - GL pixel type.
 *   ppData - receives a pointer to the locked pixel data.
 *   pPitch - receives the row pitch in bytes.
 *
 * Returns:
 *   TRUE on success, FALSE on failure.
 */
BOOL gldLockRenderTarget46(GLuint fbo, int width, int height,
                           GLenum format, GLenum type,
                           void **ppData, int *pPitch);

/*
 * Unlock a previously locked render target surface (UnlockRect).
 *
 * Uploads the modified staging buffer data back to the FBO's color
 * attachment texture via glTexSubImage2D.  Frees the staging buffer.
 *
 * Parameters:
 *   fbo      - the FBO whose color attachment to update.
 *   colorTex - the color texture attached to the FBO.
 *   width    - width of the surface in pixels.
 *   height   - height of the surface in pixels.
 *   format   - GL pixel format.
 *   type     - GL pixel type.
 *
 * Returns:
 *   TRUE on success, FALSE on failure.
 */
BOOL gldUnlockRenderTarget46(GLuint fbo, GLuint colorTex,
                             int width, int height,
                             GLenum format, GLenum type);

#ifdef  __cplusplus
}
#endif

#endif
