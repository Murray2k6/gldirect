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
* Description:  GL46 context creation and management via Direct3D 9.
*               This is an OpenGL-to-DX9 wrapper — no real OpenGL context
*               is created. All rendering is translated to D3D9 calls.
*
*********************************************************************************/

#define STRICT
#include <windows.h>
#include <stdlib.h>

#include <d3d9.h>

#include "context_manager.h"
#include "error_handler.h"
#include "gld_log.h"
#include "gld_context.h"
#include "gld_globals.h"

// ***********************************************************************
// D3D9 globals for the GL46 backend
// ***********************************************************************

typedef IDirect3D9* (WINAPI *FNDIRECT3DCREATE9)(UINT);

typedef struct {
    HINSTANCE           hD3D9DLL;           // Handle to d3d9.dll
    FNDIRECT3DCREATE9   fnDirect3DCreate9;  // Direct3DCreate9 function pointer
    BOOL                bDirect3D;          // Persistant Direct3D9 exists
    BOOL                bDirect3DDevice;    // Persistant Direct3DDevice9 exists
    IDirect3D9          *pD3D;              // Persistant Direct3D9
    IDirect3DDevice9    *pDev;              // Persistant Direct3DDevice9
} GLD_gl46_dx9_globals;

static GLD_gl46_dx9_globals gl46Globals;

// ***********************************************************************
// Initialize the D3D9 layer for the GL46 backend.
// Loads d3d9.dll and obtains Direct3DCreate9.
// Returns TRUE on success, FALSE on failure.
// ***********************************************************************

BOOL gldInitContext46(void)
{
    ZeroMemory(&gl46Globals, sizeof(gl46Globals));

    // Load d3d9.dll
    gl46Globals.hD3D9DLL = LoadLibrary("D3D9.DLL");
    if (gl46Globals.hD3D9DLL == NULL) {
        gldLogMessage(GLDLOG_ERROR, "GL46: Failed to load D3D9.DLL\n");
        return FALSE;
    }

    // Obtain Direct3DCreate9
    gl46Globals.fnDirect3DCreate9 = (FNDIRECT3DCREATE9)GetProcAddress(
        gl46Globals.hD3D9DLL, "Direct3DCreate9");
    if (gl46Globals.fnDirect3DCreate9 == NULL) {
        gldLogMessage(GLDLOG_ERROR, "GL46: Failed to get Direct3DCreate9\n");
        FreeLibrary(gl46Globals.hD3D9DLL);
        gl46Globals.hD3D9DLL = NULL;
        return FALSE;
    }

    // Create the IDirect3D9 interface immediately — this is cheap,
    // doesn't need a window, and allows GPU info queries before device creation
    if (!gl46Globals.pD3D) {
        gl46Globals.pD3D = gl46Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
        if (gl46Globals.pD3D) {
            gl46Globals.bDirect3D = TRUE;
        }
    }

    gldLogMessage(GLDLOG_SYSTEM, "GL46: D3D9 initialized successfully\n");
    return TRUE;
}

// ***********************************************************************
// Shut down the D3D9 layer for the GL46 backend.
// ***********************************************************************

void gldShutdownContext46(void)
{
    if (gl46Globals.pDev) {
        IDirect3DDevice9_Release(gl46Globals.pDev);
        gl46Globals.pDev = NULL;
    }
    if (gl46Globals.pD3D) {
        IDirect3D9_Release(gl46Globals.pD3D);
        gl46Globals.pD3D = NULL;
    }
    if (gl46Globals.hD3D9DLL) {
        FreeLibrary(gl46Globals.hD3D9DLL);
        gl46Globals.hD3D9DLL = NULL;
    }
    gl46Globals.bDirect3D = FALSE;
    gl46Globals.bDirect3DDevice = FALSE;
}

// ***********************************************************************
// Create a D3D9 device for the GL46 context.
// This replaces the old gldCreateContext46 which incorrectly tried to
// create a real OpenGL context.
// ***********************************************************************

HGLRC gldCreateContext46(HDC hDC, GLint *pMajor, GLint *pMinor)
{
    HWND hWnd;

    // Try to load D3D9 if not already loaded — not fatal if it fails
    if (!gl46Globals.fnDirect3DCreate9) {
        gldInitContext46();
    }

    // Create the D3D9 device now — games call glClear before SwapBuffers
    hWnd = WindowFromDC(hDC);
    if (!gl46Globals.pDev) {
        _gldEnsureDevice(hWnd);
    }

    // Report the emulated GL version
    if (pMajor) *pMajor = 4;
    if (pMinor) *pMinor = 6;

    gldLogPrintf(GLDLOG_SYSTEM,
        "GL46: Context created (emulating GL 4.6), D3D9 device=%p",
        (void*)gl46Globals.pDev);

    return (HGLRC)(INT_PTR)1;
}

// ***********************************************************************
// Ensure the D3D9 device exists. Called lazily from SwapBuffers/Present.
// Creates the device on the calling thread to avoid cross-thread deadlocks.
// ***********************************************************************

BOOL _gldEnsureDevice(HWND hWnd)
{
    IDirect3D9              *pD3D = NULL;
    IDirect3DDevice9        *pDev = NULL;
    D3DPRESENT_PARAMETERS   d3dpp;
    D3DDISPLAYMODE          d3ddm;
    D3DCAPS9                d3dCaps;
    DWORD                   dwBehaviourFlags;
    HRESULT                 hr;

    // Already have a device
    if (gl46Globals.pDev)
        return TRUE;

    // D3D9 not available
    if (!gl46Globals.fnDirect3DCreate9)
        return FALSE;

    if (!hWnd)
        hWnd = GetDesktopWindow();

    // Use the existing D3D9 interface (created in gldInitContext46)
    pD3D = gl46Globals.pD3D;
    if (!pD3D) {
        pD3D = gl46Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!pD3D) return FALSE;
        gl46Globals.pD3D = pD3D;
        gl46Globals.bDirect3D = TRUE;
    }

    // Get display mode
    hr = IDirect3D9_GetAdapterDisplayMode(pD3D, glb.dwAdapter, &d3ddm);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: GetAdapterDisplayMode failed (0x%08X)", hr);
        return FALSE;
    }

    // Get device caps
    hr = IDirect3D9_GetDeviceCaps(pD3D, glb.dwAdapter, D3DDEVTYPE_HAL, &d3dCaps);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: GetDeviceCaps failed (0x%08X)", hr);
        return FALSE;
    }

    // Set up presentation parameters
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed               = TRUE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat        = d3ddm.Format;
    d3dpp.BackBufferCount         = 1;
    d3dpp.EnableAutoDepthStencil  = TRUE;
    d3dpp.AutoDepthStencilFormat  = D3DFMT_D24S8;
    d3dpp.hDeviceWindow           = hWnd;
    d3dpp.PresentationInterval    = D3DPRESENT_INTERVAL_DEFAULT;

    if (!glb.bWaitForRetrace) {
        if (d3dCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
            d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }

    // Always multithreaded + vertex processing
    dwBehaviourFlags = (d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
        D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    dwBehaviourFlags |= D3DCREATE_MULTITHREADED;
    if (!glb.bFastFPU)
        dwBehaviourFlags |= D3DCREATE_FPU_PRESERVE;

    hr = IDirect3D9_CreateDevice(pD3D,
        glb.dwAdapter,
        D3DDEVTYPE_HAL,
        hWnd,
        dwBehaviourFlags,
        &d3dpp,
        &pDev);

    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: CreateDevice failed (0x%08X)", hr);
        return FALSE;
    }

    gl46Globals.pDev = pDev;
    gl46Globals.bDirect3DDevice = TRUE;

    // Begin the first scene so glClear and draw calls work immediately
    IDirect3DDevice9_BeginScene(pDev);

    // Set default render states for a clean starting point
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_DITHERENABLE, TRUE);

    gldLogPrintf(GLDLOG_SYSTEM, "GL46: D3D9 device created lazily for HWND=%p", (void*)hWnd);
    return TRUE;
}

// ***********************************************************************
// Make context current — for DX9 wrapper this is essentially a no-op
// since D3D9 doesn't have the concept of "make current". The device
// is always available once created.
// ***********************************************************************

BOOL gldMakeCurrent46(HDC hDC, HGLRC hRC)
{
    // Deactivation request
    if (hRC == NULL || hDC == NULL) {
        return TRUE;
    }

    // Nothing to do — D3D9 device is always "current"
    return TRUE;
}

// ***********************************************************************
// Delete the GL46 context and release D3D9 resources.
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

    gldLogMessage(GLDLOG_SYSTEM, "gldDeleteContext46: releasing resources\n");

    // --- Free cache structures ---
    if (glCtx->shaderCache) {
        free(glCtx->shaderCache);
        glCtx->shaderCache = NULL;
    }
    if (glCtx->bufferCache) {
        free(glCtx->bufferCache);
        glCtx->bufferCache = NULL;
    }
    if (glCtx->textureCache) {
        free(glCtx->textureCache);
        glCtx->textureCache = NULL;
    }
    if (glCtx->renderTargetCache) {
        free(glCtx->renderTargetCache);
        glCtx->renderTargetCache = NULL;
    }
    if (glCtx->fixedFuncCache) {
        free(glCtx->fixedFuncCache);
        glCtx->fixedFuncCache = NULL;
    }

    // --- Release D3D9 device (but keep the D3D9 interface for reuse) ---
    if (gl46Globals.pDev) {
        IDirect3DDevice9_Release(gl46Globals.pDev);
        gl46Globals.pDev = NULL;
        gl46Globals.bDirect3DDevice = FALSE;
    }

    // Clear the context state
    ZeroMemory(glCtx, sizeof(GLD_glContext));

    gldLogMessage(GLDLOG_INFO, "gldDeleteContext46: context destroyed\n");
    return TRUE;
}

// ***********************************************************************
// Get the D3D9 device pointer for use by other GL46 modules.
// ***********************************************************************

IDirect3DDevice9* gldGetD3DDevice46(void)
{
    return gl46Globals.pDev;
}

// ***********************************************************************
// Get the D3D9 interface pointer for use by other GL46 modules.
// ***********************************************************************

IDirect3D9* gldGetD3D46(void)
{
    return gl46Globals.pD3D;
}
