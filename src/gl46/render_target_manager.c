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
*               OpenGL 4.6 backend.  Handles FBO creation, SetRenderTarget,
*               MRT, StretchRect (glBlitFramebuffer), readback (glReadPixels),
*               and LockRect/UnlockRect on render target surfaces.
*
*********************************************************************************/

#define STRICT
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include <glad/gl.h>
#include "render_target_manager.h"
#include "format_mapper.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************
// File-static staging buffer for render target lock/unlock.
// Only one render target lock may be active at a time.
// ***********************************************************************

static void  *s_rtStagingBuffer     = NULL;
static int    s_rtStagingBufferSize = 0;

// ***********************************************************************
// Internal helpers
// ***********************************************************************

/*****************************************************************************
 * _gldFBOStatusString
 *
 * Return a human-readable string for a glCheckFramebufferStatus result.
 *****************************************************************************/
static const char *_gldFBOStatusString(GLenum status)
{
    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_UNDEFINED:
        return "GL_FRAMEBUFFER_UNDEFINED";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
    default:
        return "UNKNOWN";
    }
}

/*****************************************************************************
 * _gldComputePixelSize
 *
 * Estimate the bytes-per-pixel for a GL format/type pair.
 * Used to compute staging buffer sizes and row pitches for readback.
 *****************************************************************************/
static int _gldComputePixelSize(GLenum format, GLenum type)
{
    int components = 0;

    /* Determine component count from format. */
    switch (format) {
    case GL_RED:
        components = 1;
        break;
    case GL_RG:
        components = 2;
        break;
    case GL_RGB:
    case GL_BGR:
        components = 3;
        break;
    case GL_RGBA:
    case GL_BGRA:
        components = 4;
        break;
    case GL_DEPTH_COMPONENT:
        components = 1;
        break;
    case GL_DEPTH_STENCIL:
        components = 1;  /* packed type */
        break;
    default:
        components = 4;  /* safe fallback */
        break;
    }

    /* Determine byte size from type. */
    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        return components * 1;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        return (components > 1) ? 2 : 2;  /* packed 16-bit */
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_24_8:
        return 4;
    default:
        return components;  /* best guess */
    }
}

// ***********************************************************************
// 11.1 — FBO creation for render targets and depth-stencil surfaces
// ***********************************************************************

/*****************************************************************************
 * gldCreateRenderTarget46
 *
 * Create an FBO with a color texture attachment.
 *
 * Uses the Format_Mapper to translate the D3DFMT to a GL format tuple,
 * creates the FBO and color texture, attaches the texture, and validates
 * FBO completeness.
 *
 * Returns a GLD_renderTarget with fboId and colorTexId, or {0,0} on failure.
 *****************************************************************************/
GLD_renderTarget gldCreateRenderTarget46(void *ctx, DWORD d3dFormat,
                                         UINT width, UINT height)
{
    GLD_renderTarget result = { 0, 0 };
    GLD_formatMapping fmt;
    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLenum fboStatus;

    (void)ctx;  /* reserved for future use */

    /* Look up the GL format mapping. */
    if (!gldMapD3DFormat(d3dFormat, &fmt)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateRenderTarget46: unsupported D3DFMT %lu",
            (unsigned long)d3dFormat);
        return result;
    }

    /* Create the FBO. */
    glGenFramebuffers(1, &fbo);
    if (fbo == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateRenderTarget46: glGenFramebuffers failed");
        return result;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer", "CreateRenderTarget");

    /* Create the color texture attachment. */
    glGenTextures(1, &colorTex);
    if (colorTex == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateRenderTarget46: glGenTextures failed");
        glDeleteFramebuffers(1, &fbo);
        return result;
    }

    glBindTexture(GL_TEXTURE_2D, colorTex);
    GLD_CHECK_GL("glBindTexture", "CreateRenderTarget");

    /* Allocate texture storage for the color attachment. */
    glTexImage2D(
        GL_TEXTURE_2D,
        0,                          /* level */
        (GLint)fmt.glInternalFormat,
        (GLsizei)width,
        (GLsizei)height,
        0,                          /* border */
        fmt.glFormat,
        fmt.glType,
        NULL                        /* no initial data */
    );
    GLD_CHECK_GL("glTexImage2D", "CreateRenderTarget");

    /* Set sensible defaults for render target textures. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Attach the color texture to the FBO. */
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        colorTex,
        0                           /* level */
    );
    GLD_CHECK_GL("glFramebufferTexture2D", "CreateRenderTarget");

    /* Validate FBO completeness. */
    fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateRenderTarget46: FBO incomplete — status: %s (0x%04X)",
            _gldFBOStatusString(fboStatus), (unsigned int)fboStatus);
        glDeleteTextures(1, &colorTex);
        glDeleteFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return result;
    }

    /* Restore default framebuffer binding. */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    result.fboId = fbo;
    result.colorTexId = colorTex;

    gldLogPrintf(GLDLOG_INFO,
        "gldCreateRenderTarget46: created FBO %u with color tex %u (%ux%u, fmt=%lu)",
        fbo, colorTex, width, height, (unsigned long)d3dFormat);

    return result;
}

/*****************************************************************************
 * gldCreateDepthStencil46
 *
 * Create a depth-stencil renderbuffer and attach it to an existing FBO.
 *
 * Uses the Format_Mapper to translate the D3DFMT to a GL internal format.
 * Attaches as GL_DEPTH_STENCIL_ATTACHMENT for D24S8, or GL_DEPTH_ATTACHMENT
 * for depth-only formats.
 *
 * Returns the renderbuffer id on success, or 0 on failure.
 *****************************************************************************/
GLuint gldCreateDepthStencil46(void *ctx, DWORD d3dFormat,
                               UINT width, UINT height, GLuint fbo)
{
    GLD_formatMapping fmt;
    GLuint rbo = 0;
    GLenum attachment;
    GLenum fboStatus;

    (void)ctx;  /* reserved for future use */

    /* Look up the GL format mapping. */
    if (!gldMapD3DFormat(d3dFormat, &fmt)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateDepthStencil46: unsupported D3DFMT %lu",
            (unsigned long)d3dFormat);
        return 0;
    }

    /* Determine the attachment point based on the format. */
    if (d3dFormat == D3DFMT_D24S8) {
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    } else {
        attachment = GL_DEPTH_ATTACHMENT;
    }

    /* Create the renderbuffer. */
    glGenRenderbuffers(1, &rbo);
    if (rbo == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateDepthStencil46: glGenRenderbuffers failed");
        return 0;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    GLD_CHECK_GL("glBindRenderbuffer", "CreateDepthStencilSurface");

    glRenderbufferStorage(
        GL_RENDERBUFFER,
        fmt.glInternalFormat,
        (GLsizei)width,
        (GLsizei)height
    );
    GLD_CHECK_GL("glRenderbufferStorage", "CreateDepthStencilSurface");

    /* Attach the renderbuffer to the FBO. */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer", "CreateDepthStencilSurface");

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        attachment,
        GL_RENDERBUFFER,
        rbo
    );
    GLD_CHECK_GL("glFramebufferRenderbuffer", "CreateDepthStencilSurface");

    /* Re-validate FBO completeness after attaching depth-stencil. */
    fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateDepthStencil46: FBO incomplete after depth-stencil attach — "
            "status: %s (0x%04X)",
            _gldFBOStatusString(fboStatus), (unsigned int)fboStatus);
        glDeleteRenderbuffers(1, &rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return 0;
    }

    /* Restore default framebuffer binding. */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    gldLogPrintf(GLDLOG_INFO,
        "gldCreateDepthStencil46: created RBO %u on FBO %u (%ux%u, fmt=%lu, %s)",
        rbo, fbo, width, height, (unsigned long)d3dFormat,
        (attachment == GL_DEPTH_STENCIL_ATTACHMENT) ? "depth+stencil" : "depth-only");

    return rbo;
}

// ***********************************************************************
// 11.2 — SetRenderTarget and multi-render-target (MRT) support
// ***********************************************************************

/*****************************************************************************
 * gldSetRenderTarget46
 *
 * Bind an FBO as the current render target.
 * Pass fbo=0 to bind the default backbuffer.
 *****************************************************************************/
void gldSetRenderTarget46(GLuint fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer", "SetRenderTarget");
}

/*****************************************************************************
 * gldSetMRT46
 *
 * Configure multiple render targets on an FBO.
 *
 * Attaches the given color textures to GL_COLOR_ATTACHMENT0..N and calls
 * glDrawBuffers to enable all attachments.
 *****************************************************************************/
void gldSetMRT46(GLuint fbo, int count, const GLuint *colorTexIds)
{
    GLenum drawBuffers[GLD_MAX_MRT_ATTACHMENTS];
    int i;

    if (count < 1 || count > GLD_MAX_MRT_ATTACHMENTS) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetMRT46: invalid attachment count %d (max %d)",
            count, GLD_MAX_MRT_ATTACHMENTS);
        return;
    }

    if (!colorTexIds) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetMRT46: NULL colorTexIds pointer");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer", "SetMRT");

    /* Attach each color texture and build the draw buffers list. */
    for (i = 0; i < count; i++) {
        GLenum attachment = GL_COLOR_ATTACHMENT0 + (GLenum)i;

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            attachment,
            GL_TEXTURE_2D,
            colorTexIds[i],
            0                       /* level */
        );
        GLD_CHECK_GL("glFramebufferTexture2D", "SetMRT");

        drawBuffers[i] = attachment;
    }

    /* Enable all attached color buffers for drawing. */
    glDrawBuffers(count, drawBuffers);
    GLD_CHECK_GL("glDrawBuffers", "SetMRT");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldSetMRT46: configured %d color attachments on FBO %u",
        count, fbo);
}

// ***********************************************************************
// 11.3 — StretchRect and render target readback
// ***********************************************************************

/*****************************************************************************
 * gldStretchRect46
 *
 * Blit pixels between two FBOs using glBlitFramebuffer.
 *
 * Binds srcFBO to GL_READ_FRAMEBUFFER and dstFBO to GL_DRAW_FRAMEBUFFER,
 * then issues the blit with the specified rectangles and filter.
 *****************************************************************************/
void gldStretchRect46(GLuint srcFBO,
                      int srcX0, int srcY0, int srcX1, int srcY1,
                      GLuint dstFBO,
                      int dstX0, int dstY0, int dstX1, int dstY1,
                      BOOL linearFilter)
{
    GLenum filter = linearFilter ? GL_LINEAR : GL_NEAREST;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    GLD_CHECK_GL("glBindFramebuffer(READ)", "StretchRect");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
    GLD_CHECK_GL("glBindFramebuffer(DRAW)", "StretchRect");

    glBlitFramebuffer(
        srcX0, srcY0, srcX1, srcY1,
        dstX0, dstY0, dstX1, dstY1,
        GL_COLOR_BUFFER_BIT,
        filter
    );
    GLD_CHECK_GL("glBlitFramebuffer", "StretchRect");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldStretchRect46: blit FBO %u [%d,%d,%d,%d] -> FBO %u [%d,%d,%d,%d] (%s)",
        srcFBO, srcX0, srcY0, srcX1, srcY1,
        dstFBO, dstX0, dstY0, dstX1, dstY1,
        linearFilter ? "LINEAR" : "NEAREST");
}

/*****************************************************************************
 * gldGetRenderTargetData46
 *
 * Read back render target pixel data via glReadPixels.
 *
 * Binds the source FBO for reading and transfers pixel data into the
 * caller-supplied CPU buffer.
 *****************************************************************************/
void gldGetRenderTargetData46(GLuint srcFBO, int width, int height,
                              GLenum format, GLenum type, void *destBuffer)
{
    if (!destBuffer) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldGetRenderTargetData46: NULL destBuffer");
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    GLD_CHECK_GL("glBindFramebuffer(READ)", "GetRenderTargetData");

    /* Ensure tight pixel packing. */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(
        0, 0,
        (GLsizei)width,
        (GLsizei)height,
        format,
        type,
        destBuffer
    );
    GLD_CHECK_GL("glReadPixels", "GetRenderTargetData");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldGetRenderTargetData46: read %dx%d pixels from FBO %u",
        width, height, srcFBO);
}

// ***********************************************************************
// 11.4 — LockRect on render target surfaces
// ***********************************************************************

/*****************************************************************************
 * gldLockRenderTarget46
 *
 * Read back framebuffer contents via glReadPixels into a staging buffer.
 * Returns a pointer to the data and the row pitch.
 *
 * Only one render target lock may be active at a time.
 *****************************************************************************/
BOOL gldLockRenderTarget46(GLuint fbo, int width, int height,
                           GLenum format, GLenum type,
                           void **ppData, int *pPitch)
{
    int pixelSize;
    int pitch;
    int bufferSize;

    if (!ppData || !pPitch) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockRenderTarget46: NULL output pointer");
        return FALSE;
    }

    *ppData = NULL;
    *pPitch = 0;

    /* Reject if a lock is already active. */
    if (s_rtStagingBuffer != NULL) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockRenderTarget46: a render target lock is already active");
        return FALSE;
    }

    if (width <= 0 || height <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockRenderTarget46: invalid dimensions %dx%d", width, height);
        return FALSE;
    }

    /* Compute buffer size. */
    pixelSize = _gldComputePixelSize(format, type);
    pitch = width * pixelSize;
    bufferSize = pitch * height;

    /* Allocate the staging buffer. */
    s_rtStagingBuffer = malloc((size_t)bufferSize);
    if (!s_rtStagingBuffer) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldLockRenderTarget46: failed to allocate %d byte staging buffer",
            bufferSize);
        return FALSE;
    }
    s_rtStagingBufferSize = bufferSize;

    /* Bind the FBO for reading and read back the pixels. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    GLD_CHECK_GL("glBindFramebuffer(READ)", "LockRenderTarget");

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(
        0, 0,
        (GLsizei)width,
        (GLsizei)height,
        format,
        type,
        s_rtStagingBuffer
    );
    GLD_CHECK_GL("glReadPixels", "LockRenderTarget");

    *ppData = s_rtStagingBuffer;
    *pPitch = pitch;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldLockRenderTarget46: locked FBO %u (%dx%d, pitch=%d)",
        fbo, width, height, pitch);

    return TRUE;
}

/*****************************************************************************
 * gldUnlockRenderTarget46
 *
 * Upload the modified staging buffer data back to the FBO's color
 * attachment texture via glTexSubImage2D.  Frees the staging buffer.
 *****************************************************************************/
BOOL gldUnlockRenderTarget46(GLuint fbo, GLuint colorTex,
                             int width, int height,
                             GLenum format, GLenum type)
{
    (void)fbo;  /* FBO id kept for API consistency; we write to the texture */

    /* Verify we have an active lock. */
    if (!s_rtStagingBuffer) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldUnlockRenderTarget46: no active render target lock");
        return FALSE;
    }

    if (colorTex == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldUnlockRenderTarget46: invalid colorTex 0");
        free(s_rtStagingBuffer);
        s_rtStagingBuffer = NULL;
        s_rtStagingBufferSize = 0;
        return FALSE;
    }

    /* Bind the color texture and upload the modified data. */
    glBindTexture(GL_TEXTURE_2D, colorTex);
    GLD_CHECK_GL("glBindTexture", "UnlockRenderTarget");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,                          /* level */
        0,                          /* xoffset */
        0,                          /* yoffset */
        (GLsizei)width,
        (GLsizei)height,
        format,
        type,
        s_rtStagingBuffer
    );
    GLD_CHECK_GL("glTexSubImage2D", "UnlockRenderTarget");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldUnlockRenderTarget46: unlocked FBO %u, uploaded to tex %u (%dx%d)",
        fbo, colorTex, width, height);

    /* Free the staging buffer. */
    free(s_rtStagingBuffer);
    s_rtStagingBuffer = NULL;
    s_rtStagingBufferSize = 0;

    return TRUE;
}
