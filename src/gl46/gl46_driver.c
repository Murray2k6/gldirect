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
* Description:  GLD_driver function table implementation for the OpenGL 4.6
*               backend.  Routes all driver calls through the GL46 modules
*               (context_manager, pixel_format_provider, coordinate_adapter,
*               fixed_function_emulator, swap_chain).
*
*********************************************************************************/

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "gl46_driver.h"
#include "gld_driver.h"
#include "gld_log.h"
#include "gld_pf.h"
#include "gld_diag.h"

#include <d3d9.h>

/* GL46 module headers */
#include "gl46/pixel_format_provider.h"
#include "gl46/context_manager.h"
#include "gl46/coordinate_adapter.h"
#include "gl46/fixed_function_emulator.h"
#include "gl46/gl_modern_stubs.h"
#include "gl46/gl_generated_stubs.h"
#include "gl46/gl_state.h"
#include "gl46/gl_impl.h"

//---------------------------------------------------------------------------
// gldGetDXErrorString_GL46
//
// The GL46 backend has no DirectX errors.  Return a static identifier.
//---------------------------------------------------------------------------

BOOL gldGetDXErrorString_GL46(
	HRESULT hr,
	char *buf,
	int nBufSize)
{
	if (buf && nBufSize > 0) {
		strncpy(buf, "GL46 backend", nBufSize - 1);
		buf[nBufSize - 1] = '\0';
	}
	return TRUE;
}

//---------------------------------------------------------------------------
// gldCreatePrivateGlobals_GL46
//
// Called once during driver init.  Initialize D3D9 (optional) and build
// the pixel format list.
//---------------------------------------------------------------------------

BOOL gldCreatePrivateGlobals_GL46(void)
{
	gldLogMessage(GLDLOG_SYSTEM, "GL46: CreatePrivateGlobals\n");

	/* Initialize the GL state machine */
	glsInitState();

	/* Try to initialize D3D9 — not fatal if it fails */
	if (!gldInitContext46()) {
		gldLogMessage(GLDLOG_WARN, "GL46: D3D9 not available — rendering disabled\n");
	}

	/* Pre-build the pixel format table. */
	gldBuildPixelFormatList46();

	return TRUE;
}

//---------------------------------------------------------------------------
// gldDestroyPrivateGlobals_GL46
//
// Shut down D3D9 and release persistent resources.
//---------------------------------------------------------------------------

BOOL gldDestroyPrivateGlobals_GL46(void)
{
	gldLogMessage(GLDLOG_SYSTEM, "GL46: DestroyPrivateGlobals\n");
	gldShutdownContext46();
	return TRUE;
}

//---------------------------------------------------------------------------
// gldBuildPixelformatList_GL46
//
// Populate glb.lpPF and glb.nPixelFormatCount from the GL46 pixel format
// provider.  Each entry is a GLD_pixelFormat containing a PFD and driver
// data (the 1-based pixel format index from the provider).
//---------------------------------------------------------------------------

BOOL gldBuildPixelformatList_GL46(void)
{
	int nFormats, i;
	PIXELFORMATDESCRIPTOR pfd;
	GLD_pixelFormat *lpPF;

	gldLogMessage(GLDLOG_SYSTEM, "GL46: BuildPixelformatList\n");

	/* Ensure the provider has built its internal list */
	nFormats = gldBuildPixelFormatList46();
	if (nFormats <= 0) {
		gldLogMessage(GLDLOG_ERROR, "GL46: gldBuildPixelFormatList46 returned 0 formats\n");
		return FALSE;
	}

	/* Free any previous list */
	if (glb.lpPF) {
		free(glb.lpPF);
		glb.lpPF = NULL;
	}

	/* Allocate GLD_pixelFormat array */
	lpPF = (GLD_pixelFormat *)calloc(nFormats, sizeof(GLD_pixelFormat));
	if (!lpPF) {
		gldLogMessage(GLDLOG_ERROR, "GL46: Failed to allocate pixel format list\n");
		return FALSE;
	}

	/* Fill each entry from the provider */
	for (i = 0; i < nFormats; i++) {
		memset(&pfd, 0, sizeof(pfd));
		gldDescribePixelFormat46(NULL, i + 1, sizeof(pfd), &pfd);
		lpPF[i].pfd = pfd;
		lpPF[i].dwDriverData = (DWORD)(i + 1); /* 1-based provider index */
	}

	glb.lpPF = lpPF;
	glb.nPixelFormatCount = nFormats;

	gldLogPrintf(GLDLOG_SYSTEM, "GL46: %d pixel formats enumerated", nFormats);
	return TRUE;
}

//---------------------------------------------------------------------------
// gldCreateDrawable_GL46
//
// Called when a rendering context is created.  Creates a D3D9 device
// for the GL46 context (OpenGL-to-DX9 translation).
//---------------------------------------------------------------------------

BOOL gldCreateDrawable_GL46(
	GLD_ctx *ctx,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	HGLRC hRC;

	gldLogPrintf(GLDLOG_SYSTEM, "GL46: CreateDrawable for HDC=%X", ctx->hDC);

	/*
	 * Create the GL46 context with full D3D9 device creation.
	 */
	hRC = gldCreateContext46(ctx->hDC,
		&ctx->gl46Ctx.glVersionMajor,
		&ctx->gl46Ctx.glVersionMinor);
	if (!hRC) {
		gldLogMessage(GLDLOG_ERROR, "GL46: gldCreateContext46 failed\n");
		return FALSE;
	}

	/* Store the HGLRC */
	ctx->gl46Ctx.hRC = hRC;

	/* Verify device exists */
	if (!gldGetD3DDevice46()) {
		gldLogMessage(GLDLOG_ERROR, "GL46: No D3D9 device after context creation\n");
		return FALSE;
	}

	gldLogPrintf(GLDLOG_SYSTEM, "GL46: D3D9 context ready (GL %d.%d), device=%p",
		ctx->gl46Ctx.glVersionMajor, ctx->gl46Ctx.glVersionMinor,
		(void*)gldGetD3DDevice46());

	gldLogPrintf(GLDLOG_SYSTEM, "GL46: D3D9 context ready (emulating GL %d.%d), device=%s",
		ctx->gl46Ctx.glVersionMajor, ctx->gl46Ctx.glVersionMinor,
		gldGetD3DDevice46() ? "YES" : "NO");

	/* Initialise the coordinate adapter */
	gldInitCoordinateAdapter(&ctx->gl46Ctx);

	return TRUE;
}

//---------------------------------------------------------------------------
// gldResizeDrawable_GL46
//
// Called when the window is resized.  Update the viewport dimensions
// stored in the GL46 context.
//---------------------------------------------------------------------------

BOOL gldResizeDrawable_GL46(
	GLD_ctx *ctx,
	BOOL bDefaultDriver,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	RECT rc;

	if (ctx->hWnd && GetClientRect(ctx->hWnd, &rc)) {
		ctx->dwWidth  = rc.right  - rc.left;
		ctx->dwHeight = rc.bottom - rc.top;
	}

	/* Keep the GL46 context viewport in sync */
	ctx->gl46Ctx.viewportX      = 0;
	ctx->gl46Ctx.viewportY      = 0;
	ctx->gl46Ctx.viewportWidth  = (GLsizei)ctx->dwWidth;
	ctx->gl46Ctx.viewportHeight = (GLsizei)ctx->dwHeight;

	gldLogPrintf(GLDLOG_INFO, "GL46: ResizeDrawable %ux%u", ctx->dwWidth, ctx->dwHeight);
	return TRUE;
}

//---------------------------------------------------------------------------
// gldDestroyDrawable_GL46
//
// Destroy the real OpenGL context and clean up.
//---------------------------------------------------------------------------

BOOL gldDestroyDrawable_GL46(
	GLD_ctx *ctx)
{
	gldLogMessage(GLDLOG_SYSTEM, "GL46: DestroyDrawable\n");
	gldDeleteContext46(ctx);
	return TRUE;
}

//---------------------------------------------------------------------------
// gldInitialiseMesa_GL46
//
// Allocate and initialise the fixed-function shader cache for this context.
// The legacy Mesa context is left as-is (allocated in gldCreateContextBuffers).
//---------------------------------------------------------------------------

BOOL gldInitialiseMesa_GL46(
	GLD_ctx *ctx)
{
	GLD_glContext *gl46 = &ctx->gl46Ctx;

	gldLogMessage(GLDLOG_SYSTEM, "GL46: InitialiseMesa\n");

	/* Allocate the fixed-function emulator cache if not already present */
	if (!gl46->fixedFuncCache) {
		gl46->fixedFuncCache = (GLD_fixedFuncCache *)calloc(1, sizeof(GLD_fixedFuncCache));
		if (!gl46->fixedFuncCache) {
			gldLogMessage(GLDLOG_ERROR, "GL46: Failed to allocate fixedFuncCache\n");
			return FALSE;
		}
		gldInitFixedFuncCache(gl46->fixedFuncCache);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
// gldSwapBuffers_GL46
//
// Present the back buffer via D3D9.
//---------------------------------------------------------------------------

BOOL gldSwapBuffers_GL46(
	GLD_ctx *ctx,
	HDC hDC,
	HWND hWnd)
{
	IDirect3DDevice9 *pDev = gldGetD3DDevice46();

	if (!pDev) {
		gldLogMessage(GLDLOG_ERROR, "GL46: SwapBuffers called with no D3D9 device\n");
		return FALSE;
	}

	{
		HRESULT hr;
		BOOL bWasReset = FALSE;

		// End the current scene
		IDirect3DDevice9_EndScene(pDev);

		// Present the back buffer
		hr = IDirect3DDevice9_Present(pDev, NULL, NULL, NULL, NULL);

		if (hr == D3DERR_DEVICELOST) {
			// Device lost — try to reset next frame
			hr = IDirect3DDevice9_TestCooperativeLevel(pDev);
			if (hr == D3DERR_DEVICENOTRESET) {
				D3DPRESENT_PARAMETERS d3dpp;
				HWND hResetWnd = hWnd ? hWnd : (ctx ? ctx->hWnd : NULL);

				// Same parameters the device was created with.  The block
				// that used to live here never set BackBufferWidth/Height at
				// all and asked for an auto depth-stencil the device was not
				// created with, so a recovered device could come back a
				// different shape than the one the game is drawing into.
				gldBuildPresentParams46(hResetWnd, &d3dpp);

				gldDiagLog("GL46: device-lost Reset hWnd=%p -> backbuffer %ux%u",
					(void*)hResetWnd, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight);

				// Reset refuses to run while anything it destroys is alive.
				_glsReleaseDeviceLosableResources(pDev);

				hr = IDirect3DDevice9_Reset(pDev, &d3dpp);
				if (FAILED(hr))
					gldLogPrintf(GLDLOG_ERROR,
						"GL46: device-lost Reset failed (0x%08X)", hr);
				else
					bWasReset = TRUE;
			}
		}

		// Begin a new scene for the next frame
		IDirect3DDevice9_BeginScene(pDev);

		// A Reset clears render and transform state, so re-apply the defaults
		// the backend assumes — but only when one actually happened.  Doing it
		// unconditionally would overwrite, every single frame, whatever state
		// the application's GL calls had put on the device.
		if (bWasReset)
			gldApplyDefaultDeviceState46(pDev);

		return TRUE;
	}
	return TRUE;
}

//---------------------------------------------------------------------------
// gldGetProcAddress_GL46
//
// Return function pointers for GL/WGL extensions. Since we're a DX9 wrapper
// emulating OpenGL, we provide local implementations for WGL extensions that
// apps use to negotiate context versions and query capabilities.
//---------------------------------------------------------------------------

// WGL extensions implemented by the direct D3D9 backend.

#define GLD_WGL_EXTENSIONS \
	"WGL_ARB_create_context WGL_ARB_create_context_profile " \
	"WGL_ARB_extensions_string WGL_EXT_extensions_string " \
	"WGL_ARB_pixel_format WGL_EXT_pixel_format"

static const char *WINAPI _wglGetExtensionsStringARB(HDC hDC)
{
	return GLD_WGL_EXTENSIONS;
}

static const char *WINAPI _wglGetExtensionsStringEXT(void)
{
	return GLD_WGL_EXTENSIONS;
}

/*
 * WGL_ARB_pixel_format / WGL_EXT_pixel_format. Both extensions share the
 * token values and entry point signatures, so the ARB and EXT names are
 * aliases of one implementation in the pixel format provider. The indices
 * these return are the wrapper's own pixel format indices, the same ones
 * wglChoosePixelFormat / wglDescribePixelFormat use.
 */

static BOOL WINAPI _wglChoosePixelFormatARB(HDC hDC, const int *piAttribIList,
                                            const FLOAT *pfAttribFList,
                                            UINT nMaxFormats, int *piFormats,
                                            UINT *nNumFormats)
{
	extern BOOL gldValidate(void);

	if (!gldValidate()) {
		gldLogMessage(GLDLOG_ERROR,
			"GL46: wglChoosePixelFormatARB: gldValidate failed\n");
		return FALSE;
	}

	return gldChoosePixelFormatARB46(hDC, piAttribIList, pfAttribFList,
	                                 nMaxFormats, piFormats, nNumFormats);
}

static BOOL WINAPI _wglGetPixelFormatAttribivARB(HDC hDC, int iPixelFormat,
                                                 int iLayerPlane,
                                                 UINT nAttributes,
                                                 const int *piAttributes,
                                                 int *piValues)
{
	extern BOOL gldValidate(void);

	if (!gldValidate())
		return FALSE;

	return gldGetPixelFormatAttribivARB46(hDC, iPixelFormat, iLayerPlane,
	                                      nAttributes, piAttributes, piValues);
}

static BOOL WINAPI _wglGetPixelFormatAttribfvARB(HDC hDC, int iPixelFormat,
                                                 int iLayerPlane,
                                                 UINT nAttributes,
                                                 const int *piAttributes,
                                                 FLOAT *pfValues)
{
	extern BOOL gldValidate(void);

	if (!gldValidate())
		return FALSE;

	return gldGetPixelFormatAttribfvARB46(hDC, iPixelFormat, iLayerPlane,
	                                      nAttributes, piAttributes, pfValues);
}

static HGLRC WINAPI _wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
{
	// The wrapper accepts any version request — we emulate everything via DX9.
	// Always use the eager real-window path (gldCreateContext) so the D3D9
	// device is bound to the actual HWND the app provided.
	extern int   gldGetPixelFormat(void);
	extern void  gldSetPixelFormat(int iPixelFormat);
	extern BOOL  gldValidate(void);
	extern HGLRC gldCreateContext(HDC a, const GLD_pixelFormat *lpPF);
	extern BOOL  IsValidPFD(int iPFD);
	extern int   gldResolvePFDForDC(HDC hDC);

	int   ipf = 0;
	HGLRC hglrc = NULL;

	gldLogMessage(GLDLOG_SYSTEM, "GL46: wglCreateContextAttribsARB called\n");

	if (hDC == NULL) {
		gldLogMessage(GLDLOG_ERROR,
			"GL46: wglCreateContextAttribsARB: NULL HDC\n");
		SetLastError(ERROR_INVALID_HANDLE);
		return NULL;
	}

	// License gate (matches wglCreateContext)
	if (!gldValidate()) {
		gldLogMessage(GLDLOG_ERROR,
			"GL46: wglCreateContextAttribsARB: gldValidate failed\n");
		SetLastError(ERROR_ACCESS_DENIED);
		return NULL;
	}

	// Make sure the pixel format table exists.
	if (glb.nPixelFormatCount == 0 || glb.lpPF == NULL) {
		gldLogMessage(GLDLOG_WARN, "GL46: PF table empty, building it now\n");
		gldBuildPixelFormatList46();
	}
	if (glb.nPixelFormatCount == 0 || glb.lpPF == NULL) {
		gldLogMessage(GLDLOG_ERROR,
			"GL46: wglCreateContextAttribsARB FAILED (no pixel formats available)\n");
		SetLastError(ERROR_INVALID_PIXEL_FORMAT);
		return NULL;
	}

	// 1. Resolve the real OS-level PF on the DC and translate it to our
	//    wrapper's PF index by matching the actual PIXELFORMATDESCRIPTOR.
	ipf = gldResolvePFDForDC(hDC);

	// 2. Fall back to the wrapper's thread-local current PF.
	if (!IsValidPFD(ipf))
		ipf = gldGetPixelFormat();

	// 3. Last-resort fallback to PF 1. This wrapper emulates *every*
	//    GL version (1.x through 4.6) via DX9 regardless of which PF
	//    is selected, so any valid PF satisfies any GL version request.
	//    Refusing the context here would break games that call
	//    wglCreateContextAttribsARB on a DC whose PF we couldn't
	//    successfully translate.
	if (!IsValidPFD(ipf)) {
		gldLogPrintf(GLDLOG_WARN,
			"GL46: wglCreateContextAttribsARB: could not resolve PF for DC, using wrapper PF 1");
		ipf = 1;
	}

	// Sync the wrapper's internal current-PF.
	gldSetPixelFormat(ipf);

	/*
	 * Wolfenstein: The New Order creates its attribute context while the
	 * bootstrap window can still have a zero-sized client area.  Creating a
	 * DXVK-Remix D3D9 swap chain for that window makes DXVK fall back to the
	 * desktop dimensions and can stall inside CreateDevice.  Keep the WGL
	 * context usable for capability/function queries, but defer D3D9 device
	 * creation until wglMakeCurrent or SwapBuffers sees a real drawable.
	 */
	{
		HWND hWnd = WindowFromDC(hDC);
		RECT rc;
		BOOL drawable = hWnd && IsWindow(hWnd) && GetClientRect(hWnd, &rc) &&
			(rc.right > rc.left) && (rc.bottom > rc.top);

		if (drawable) {
			hglrc = gldCreateContext(hDC, &glb.lpPF[ipf - 1]);
		} else {
			gldLogMessage(GLDLOG_SYSTEM,
				"GL46: deferring D3D9 device creation for zero-sized/bootstrap context\n");
			hglrc = gldCreateContextLazy(hDC, &glb.lpPF[ipf - 1]);
		}
	}

	if (hglrc == NULL) {
		gldLogMessage(GLDLOG_ERROR,
			"GL46: wglCreateContextAttribsARB: gldCreateContext returned NULL\n");
		SetLastError(ERROR_INVALID_OPERATION);
	} else {
		gldLogPrintf(GLDLOG_SYSTEM,
			"GL46: wglCreateContextAttribsARB: real-window context created (HGLRC=%d, PF=%d)",
			(int)(INT_PTR)hglrc, ipf);
	}
	(void)hShareContext;
	(void)attribList;
	return hglrc;
}

// Lookup table for extension functions
typedef struct {
	const char *name;
	PROC        proc;
} GLD_procEntry;

static const GLD_procEntry gl46ProcTable[] = {
	{ "wglGetExtensionsStringARB",     (PROC)_wglGetExtensionsStringARB },
	{ "wglGetExtensionsStringEXT",     (PROC)_wglGetExtensionsStringEXT },
	{ "wglCreateContextAttribsARB",    (PROC)_wglCreateContextAttribsARB },
	{ "wglChoosePixelFormatARB",       (PROC)_wglChoosePixelFormatARB },
	{ "wglChoosePixelFormatEXT",       (PROC)_wglChoosePixelFormatARB },
	{ "wglGetPixelFormatAttribivARB",  (PROC)_wglGetPixelFormatAttribivARB },
	{ "wglGetPixelFormatAttribivEXT",  (PROC)_wglGetPixelFormatAttribivARB },
	{ "wglGetPixelFormatAttribfvARB",  (PROC)_wglGetPixelFormatAttribfvARB },
	{ "wglGetPixelFormatAttribfvEXT",  (PROC)_wglGetPixelFormatAttribfvARB },
	{ NULL, NULL }
};

/* There is intentionally no generic fallback. Every supported GL name has an
 * exact-signature entry, and every unknown name must resolve to NULL. */

PROC gldGetProcAddress_GL46(
	LPCSTR a)
{
	int i;

	if (!a)
		return NULL;

	// Check WGL extension table
	for (i = 0; gl46ProcTable[i].name != NULL; i++) {
		if (strcmp(a, gl46ProcTable[i].name) == 0)
			return gl46ProcTable[i].proc;
	}

	// Check modern GL function table (GL 2.0 - 4.6)
	for (i = 0; g_modernGL[i].name != NULL; i++) {
		if (strcmp(a, g_modernGL[i].name) == 0)
			return g_modernGL[i].proc;
	}

	// Check the generated table: every remaining GL 1.0-4.6 name, core and
	// extension alias, with the exact signature the Khronos registry gives it.
	// Scanned after g_modernGL so a hand-written implementation always wins
	// over the generated one for the same name.
	for (i = 0; g_generatedGL[i].name != NULL; i++) {
		if (strcmp(a, g_generatedGL[i].name) == 0)
			return g_generatedGL[i].proc;
	}

	// For GL functions that are exported from the DLL, return their address
	// via GetProcAddress on our own module
	{
		static HMODULE hSelf = NULL;
		if (!hSelf)
			hSelf = GetModuleHandle("opengl32.dll");
		if (hSelf) {
			PROC p = GetProcAddress(hSelf, a);
			if (p)
				return p;
		}
	}

	/* WGL requires NULL for an unsupported name. A callable no-op makes games
	 * believe a capability exists and commonly fails much later in rendering. */
	gldDiagLog("wglGetProcAddress: \"%s\" -> UNMAPPED", a ? a : "(null)");
	return NULL;
}

//---------------------------------------------------------------------------
// gldGetDisplayMode_GL46
//
// Query the current display mode and fill in the GLD_displayMode struct.
//---------------------------------------------------------------------------

BOOL gldGetDisplayMode_GL46(
	GLD_ctx *ctx,
	GLD_displayMode *glddm)
{
	DEVMODE dm;

	if (!glddm)
		return FALSE;

	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
		glddm->Width   = dm.dmPelsWidth;
		glddm->Height  = dm.dmPelsHeight;
		glddm->BPP     = dm.dmBitsPerPel;
		glddm->Refresh = dm.dmDisplayFrequency;
	} else {
		/* Fallback: use GetDeviceCaps on the screen DC */
		HDC hScreenDC = GetDC(NULL);
		if (hScreenDC) {
			glddm->Width   = (DWORD)GetDeviceCaps(hScreenDC, HORZRES);
			glddm->Height  = (DWORD)GetDeviceCaps(hScreenDC, VERTRES);
			glddm->BPP     = (DWORD)GetDeviceCaps(hScreenDC, BITSPIXEL);
			glddm->Refresh = (DWORD)GetDeviceCaps(hScreenDC, VREFRESH);
			ReleaseDC(NULL, hScreenDC);
		} else {
			glddm->Width   = 1024;
			glddm->Height  = 768;
			glddm->BPP     = 32;
			glddm->Refresh = 60;
		}
	}

	return TRUE;
}
