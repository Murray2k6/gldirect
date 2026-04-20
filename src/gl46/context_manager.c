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
* Description:  OpenGL 4.6 core profile context creation and management via WGL.
*
*********************************************************************************/

#define STRICT
#include <windows.h>
#include <stdlib.h>

#include <glad/gl.h>
#include <glad/wgl.h>

#include "context_manager.h"
#include "error_handler.h"
#include "gld_log.h"
#include "gld_context.h"

/*
 * GLDirect IS opengl32.dll, so wglCreateContext etc. are local symbols,
 * not imports. Declare them as extern to avoid LNK4217 warnings about
 * importing symbols that are also defined locally.
 */
extern HGLRC WINAPI wglCreateContext(HDC);
extern BOOL  WINAPI wglDeleteContext(HGLRC);
extern BOOL  WINAPI wglMakeCurrent(HDC, HGLRC);
extern PROC  WINAPI wglGetProcAddress(LPCSTR);
// ***********************************************************************
// Version fallback table: try 4.6 down to 3.3 core profile.
// ***********************************************************************

typedef struct {
    int major;
    int minor;
} GLD_glVersion;

static const GLD_glVersion s_versionFallbacks[] = {
    { 4, 6 },
    { 4, 5 },
    { 4, 4 },
    { 4, 3 },
    { 4, 2 },
    { 4, 1 },
    { 4, 0 },
    { 3, 3 },
};

static const int s_numVersionFallbacks =
    sizeof(s_versionFallbacks) / sizeof(s_versionFallbacks[0]);

// ***********************************************************************
// Validate that critical GL function pointers were loaded by GLAD.
// Returns TRUE if all critical functions are present, FALSE otherwise.
// ***********************************************************************

static BOOL _gldValidateCriticalFunctions(void)
{
    // Table of critical GL 3.3+ core functions that must be present.
    static const struct {
        const char  *name;
        void       **pfn;
    } criticalFuncs[] = {
        { "glCreateShader",      (void**)&glad_glCreateShader      },
        { "glCreateProgram",     (void**)&glad_glCreateProgram     },
        { "glGenVertexArrays",   (void**)&glad_glGenVertexArrays   },
        { "glGenBuffers",        (void**)&glad_glGenBuffers        },
        { "glGenFramebuffers",   (void**)&glad_glGenFramebuffers   },
        { "glGenTextures",       (void**)&glad_glGenTextures       },
        { "glCompileShader",     (void**)&glad_glCompileShader     },
        { "glLinkProgram",       (void**)&glad_glLinkProgram       },
        { "glUseProgram",        (void**)&glad_glUseProgram        },
        { "glBindVertexArray",   (void**)&glad_glBindVertexArray   },
    };

    BOOL allPresent = TRUE;
    int i;
    int count = sizeof(criticalFuncs) / sizeof(criticalFuncs[0]);

    for (i = 0; i < count; i++) {
        if (*(criticalFuncs[i].pfn) == NULL) {
            gldLogPrintf(GLDLOG_ERROR,
                "Critical GL function not loaded: %s",
                criticalFuncs[i].name);
            allPresent = FALSE;
        }
    }

    return allPresent;
}

// ***********************************************************************
// Try to create a core profile context for a specific GL version.
// Returns the HGLRC on success, or NULL on failure.
// ***********************************************************************

static HGLRC _gldTryCreateCoreContext(HDC hDC, int major, int minor)
{
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, major,
        WGL_CONTEXT_MINOR_VERSION_ARB, minor,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        0  // terminator
    };

    return wglCreateContextAttribsARB(hDC, NULL, attribs);
}

// ***********************************************************************

HGLRC gldCreateContext46(HDC hDC, GLint *pMajor, GLint *pMinor)
{
    HGLRC hTempRC   = NULL;
    HGLRC hCoreRC   = NULL;
    int   gladVer   = 0;
    int   i;

    // ------------------------------------------------------------------
    // Step 1: Create a temporary legacy context so we can load WGL
    //         extension function pointers.
    // ------------------------------------------------------------------
    hTempRC = wglCreateContext(hDC);
    if (!hTempRC) {
        gldLogFatal("gldCreateContext46",
            "wglCreateContext failed for temporary legacy context");
        return NULL;
    }

    if (!wglMakeCurrent(hDC, hTempRC)) {
        gldLogFatal("gldCreateContext46",
            "wglMakeCurrent failed for temporary legacy context");
        wglDeleteContext(hTempRC);
        return NULL;
    }

    // ------------------------------------------------------------------
    // Step 2: Load WGL extension function pointers via GLAD.
    //         This gives us wglCreateContextAttribsARB.
    // ------------------------------------------------------------------
    if (!gladLoadWGL(hDC, (GLADloadfunc)wglGetProcAddress)) {
        gldLogFatal("gldCreateContext46",
            "gladLoadWGL failed — WGL extensions not available");
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hTempRC);
        return NULL;
    }

    // Verify that the critical WGL extension is available.
    if (!GLAD_WGL_ARB_create_context || !wglCreateContextAttribsARB) {
        gldLogFatal("gldCreateContext46",
            "WGL_ARB_create_context not supported — cannot create core profile context");
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hTempRC);
        return NULL;
    }

    // ------------------------------------------------------------------
    // Step 3 & 4: Try to create a core profile context, starting at 4.6
    //             and falling back through 4.5 … 3.3.
    // ------------------------------------------------------------------
    for (i = 0; i < s_numVersionFallbacks; i++) {
        int major = s_versionFallbacks[i].major;
        int minor = s_versionFallbacks[i].minor;

        gldLogPrintf(GLDLOG_INFO,
            "Attempting to create OpenGL %d.%d core profile context...",
            major, minor);

        hCoreRC = _gldTryCreateCoreContext(hDC, major, minor);
        if (hCoreRC) {
            gldLogPrintf(GLDLOG_SYSTEM,
                "Created OpenGL %d.%d core profile context successfully",
                major, minor);

            // Store the negotiated version for the caller.
            if (pMajor) *pMajor = major;
            if (pMinor) *pMinor = minor;
            break;
        }

        gldLogPrintf(GLDLOG_WARN,
            "OpenGL %d.%d core profile context creation failed, trying next version...",
            major, minor);
    }

    // ------------------------------------------------------------------
    // Step 5: Delete the temporary legacy context (no longer needed).
    // ------------------------------------------------------------------
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hTempRC);
    hTempRC = NULL;

    if (!hCoreRC) {
        gldLogFatal("gldCreateContext46",
            "All core profile context versions (4.6 down to 3.3) failed");
        return NULL;
    }

    // ------------------------------------------------------------------
    // Step 6: Make the new core context current and load all GL function
    //         pointers via GLAD.
    // ------------------------------------------------------------------
    if (!wglMakeCurrent(hDC, hCoreRC)) {
        gldLogFatal("gldCreateContext46",
            "wglMakeCurrent failed for new core profile context");
        wglDeleteContext(hCoreRC);
        return NULL;
    }

    gladVer = gladLoadGL((GLADloadfunc)wglGetProcAddress);
    if (!gladVer) {
        gldLogFatal("gldCreateContext46",
            "gladLoadGL failed — could not load GL function pointers");
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hCoreRC);
        return NULL;
    }

    gldLogPrintf(GLDLOG_SYSTEM,
        "GLAD loaded GL %d.%d function pointers",
        GLAD_VERSION_MAJOR(gladVer),
        GLAD_VERSION_MINOR(gladVer));

    // ------------------------------------------------------------------
    // Step 7: Validate that critical GL functions were loaded.
    // ------------------------------------------------------------------
    if (!_gldValidateCriticalFunctions()) {
        gldLogFatal("gldCreateContext46",
            "One or more critical GL functions are NULL after GLAD init");
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hCoreRC);
        return NULL;
    }

    gldLogPrintf(GLDLOG_INFO,
        "OpenGL context ready — all critical function pointers validated");

    return hCoreRC;
}

// ***********************************************************************
// Make an OpenGL context current on the given device context.
// ***********************************************************************

BOOL gldMakeCurrent46(HDC hDC, HGLRC hRC)
{
    // Allow deactivation: NULL context unbinds the current context.
    if (hRC == NULL || hDC == NULL) {
        if (!wglMakeCurrent(NULL, NULL)) {
            gldLogPrintf(GLDLOG_WARN,
                "gldMakeCurrent46: wglMakeCurrent(NULL, NULL) failed");
            return FALSE;
        }
        return TRUE;
    }

    // Bind the real OpenGL context to the device context.
    if (!wglMakeCurrent(hDC, hRC)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldMakeCurrent46: wglMakeCurrent failed for HDC=%p, HGLRC=%p",
            (void*)hDC, (void*)hRC);
        return FALSE;
    }

    return TRUE;
}

// ***********************************************************************
// Delete an OpenGL context and release all associated GL resources.
// ***********************************************************************

BOOL gldDeleteContext46(GLD_ctx *ctx)
{
    GLD_glContext *glCtx;

    if (!ctx) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldDeleteContext46: NULL context pointer");
        return FALSE;
    }

    glCtx = &ctx->gl46Ctx;

    // Nothing to do if there is no real GL context handle.
    if (!glCtx->hRC) {
        gldLogPrintf(GLDLOG_WARN,
            "gldDeleteContext46: context has no GL handle, nothing to delete");
        return TRUE;
    }

    gldLogPrintf(GLDLOG_SYSTEM,
        "gldDeleteContext46: destroying GL context HGLRC=%p",
        (void*)glCtx->hRC);

    // ------------------------------------------------------------------
    // Make the context current so we can issue GL delete calls on its
    // resources.  If we cannot make it current, skip resource cleanup
    // and go straight to wglDeleteContext (which will release GPU-side
    // objects when the driver destroys the context).
    // ------------------------------------------------------------------
    if (glCtx->hDC && wglMakeCurrent(glCtx->hDC, glCtx->hRC)) {

        // --- Shader program cache ---
        if (glCtx->shaderCache) {
            // Actual per-object cleanup will be implemented by the
            // Shader_Translator module.  For now, free the cache struct.
            free(glCtx->shaderCache);
            glCtx->shaderCache = NULL;
        }

        // --- Buffer / VAO cache ---
        if (glCtx->bufferCache) {
            // Actual per-object cleanup will be implemented by the
            // Buffer_Manager module.
            free(glCtx->bufferCache);
            glCtx->bufferCache = NULL;
        }

        // --- Texture cache ---
        if (glCtx->textureCache) {
            // Actual per-object cleanup will be implemented by the
            // Texture_Manager module.
            free(glCtx->textureCache);
            glCtx->textureCache = NULL;
        }

        // --- Render target / FBO cache ---
        if (glCtx->renderTargetCache) {
            // Actual per-object cleanup will be implemented by the
            // Render_Target_Manager module.
            free(glCtx->renderTargetCache);
            glCtx->renderTargetCache = NULL;
        }

        // --- Fixed-function emulator cache ---
        if (glCtx->fixedFuncCache) {
            // Actual per-object cleanup will be implemented by the
            // Fixed_Function_Emulator module.
            free(glCtx->fixedFuncCache);
            glCtx->fixedFuncCache = NULL;
        }

        // Unbind the context before deleting it.
        wglMakeCurrent(NULL, NULL);

    } else {
        gldLogPrintf(GLDLOG_WARN,
            "gldDeleteContext46: could not make context current for resource cleanup — "
            "GPU resources will be released by the driver on context destruction");
    }

    // ------------------------------------------------------------------
    // Destroy the real WGL context handle.
    // ------------------------------------------------------------------
    if (!wglDeleteContext(glCtx->hRC)) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldDeleteContext46: wglDeleteContext failed for HGLRC=%p",
            (void*)glCtx->hRC);
        return FALSE;
    }

    // ------------------------------------------------------------------
    // Clear the GL context state in the slot so it can be reused.
    // ------------------------------------------------------------------
    ZeroMemory(glCtx, sizeof(GLD_glContext));

    gldLogPrintf(GLDLOG_INFO,
        "gldDeleteContext46: GL context destroyed and slot cleared");

    return TRUE;
}
