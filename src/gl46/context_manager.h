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

#ifndef __GL46_CONTEXT_MANAGER_H
#define __GL46_CONTEXT_MANAGER_H

#include <windows.h>
#include <d3d9.h>

/*
 * GLD_ctx is defined in gld_context.h.  We need the full definition
 * for gldDeleteContext46's parameter type.
 */
#include "gld_context.h"

/*---------------------- Macros and type definitions ----------------------*/

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Initialize the D3D9 layer for the GL46 backend.
 * Loads d3d9.dll and obtains Direct3DCreate9.
 * Must be called before gldCreateContext46.
 * Returns TRUE on success, FALSE on failure.
 */
BOOL gldInitContext46(void);

/*
 * Shut down the D3D9 layer and release all persistent resources.
 * Call during DLL cleanup / process detach.
 */
void gldShutdownContext46(void);

/*
 * Build the presentation parameters this backend presents with.
 *
 * One implementation for all three callers — first device creation, context
 * reuse (which resets the device) and the device-lost reset in
 * gldSwapBuffers_GL46 — so a reset can never present with a different shape
 * than the device was created with.  The back buffer takes hWnd's client size,
 * falling back to the adapter's current display mode when there is no window or
 * it has no area yet.
 *
 * *out is zeroed first, so it is safe to inspect even when D3D9 has not been
 * initialised and nothing could be filled in.
 */
void gldBuildPresentParams46(HWND hWnd, D3DPRESENT_PARAMETERS *out);

/*
 * Apply the render and transform state this backend assumes at the start of a
 * frame.  A reset clears device state the same way a fresh device would need it
 * set, so both paths call this.  NULL pDev is a no-op.
 */
void gldApplyDefaultDeviceState46(IDirect3DDevice9 *pDev);

/*
 * WGL_EXT_swap_control.
 *
 * The interval the application asks for through wglSwapIntervalEXT.  It seeds
 * from the INI's bWaitForRetrace and an explicit call then overrides it, since
 * the application is stating what it wants frame by frame.
 *
 * D3D9 can only change a presentation interval through a device Reset, so a new
 * value is recorded here and gldSwapBuffers_GL46 applies it after the next
 * Present.  gldSwapIntervalNeedsReset46() reports whether the live device was
 * built with a different interval than the current request.
 *
 * gldSetSwapInterval46 returns FALSE for values this backend will not honour
 * (negative intervals need WGL_EXT_swap_control_tear, which is not advertised).
 */
BOOL gldSetSwapInterval46(int interval);
int  gldGetSwapInterval46(void);
BOOL gldPresentParamsNeedReset46(void);

/*
 * Record that a device was successfully created or Reset with the parameters
 * gldBuildPresentParams46() last produced, so the requested interval and the
 * one the live device carries agree again.  Call it only after the call that
 * carried them actually succeeded.
 */
void gldNotePresentParamsApplied46(void);

/*
 * Create a D3D9 device for the GL46 context.
 *
 * Parameters:
 *   hDC    — device context (used to find the target window).
 *   pMajor — [out] receives the emulated major GL version (may be NULL).
 *   pMinor — [out] receives the emulated minor GL version (may be NULL).
 *
 * Returns a non-NULL dummy HGLRC on success, NULL on failure.
 * The actual D3D9 device is stored internally and accessed via gldGetD3DDevice46().
 */
HGLRC gldCreateContext46(HDC hDC, GLint *pMajor, GLint *pMinor);

/*
 * Make context current — no-op for DX9 wrapper since D3D9 doesn't
 * have the concept of "make current".
 *
 * Returns TRUE always.
 */
BOOL gldMakeCurrent46(HDC hDC, HGLRC hRC);

/*
 * Delete the GL46 context and release associated resources.
 *
 * Parameters:
 *   ctx — pointer to the GLD_ctx slot that owns the context.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL gldDeleteContext46(GLD_ctx *ctx);

/*
 * Get the D3D9 device pointer for use by other GL46 modules.
 * Returns NULL if no device has been created yet.
 */
IDirect3DDevice9* gldGetD3DDevice46(void);

/* Return the latest non-zero D3D9 texture-memory estimate in KiB. */
UINT gldGetAvailableVideoMemoryKB46(void);

/*
 * Ensure the D3D9 device exists. Creates it lazily on the calling thread.
 * Returns TRUE on success, FALSE on failure.
 */
BOOL _gldEnsureDevice(HWND hWnd);

/*
 * Get the D3D9 interface pointer for use by other GL46 modules.
 * Returns NULL if D3D9 has not been initialized.
 */
IDirect3D9* gldGetD3D46(void);

/*
 * Check if NVIDIA DXVK Remix d3d9.dll was detected.
 * When TRUE, the wrapper should use D3D9 fixed-function pipeline
 * for draw calls so Remix can intercept geometry for path tracing.
 */
BOOL gldIsRemixDetected(void);

/*
 * Ask the device whether a texture of this D3DFORMAT can actually be
 * created — as a cube map when cubeMap is TRUE, otherwise as a 2D texture.
 *
 * Answered by IDirect3D9::CheckDeviceFormat against whatever d3d9.dll the
 * game directory supplied (Usage = 0, D3DPOOL-independent), and cached per
 * format for the life of the process.  Callers must use this before
 * CreateTexture/CreateCubeTexture instead of creating and reacting to a
 * failure.
 *
 * Returns FALSE when D3D9 has not been initialised, i.e. when there is
 * nothing to ask.
 */
BOOL gldIsTextureFormatSupported46(D3DFORMAT fmt, BOOL cubeMap);

#ifdef  __cplusplus
}
#endif

#endif /* __GL46_CONTEXT_MANAGER_H */
