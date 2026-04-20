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
#include "gl46/gl_state.h"

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
	HWND hWnd;

	gldLogPrintf(GLDLOG_SYSTEM, "GL46: CreateDrawable for HDC=%X", ctx->hDC);

	/*
	 * Create the GL46 context (sets up D3D9 interface).
	 */
	hRC = gldCreateContext46(ctx->hDC,
		&ctx->gl46Ctx.glVersionMajor,
		&ctx->gl46Ctx.glVersionMinor);
	if (!hRC) {
		gldLogMessage(GLDLOG_ERROR, "GL46: gldCreateContext46 failed\n");
		return FALSE;
	}

	/* Store the dummy HGLRC sentinel */
	ctx->gl46Ctx.hRC = hRC;

	/*
	 * Create the D3D9 device eagerly so GL calls work immediately.
	 * The device needs the window handle from the context.
	 */
	hWnd = ctx->hWnd ? ctx->hWnd : WindowFromDC(ctx->hDC);
	if (!hWnd) hWnd = GetDesktopWindow();
	if (!gldGetD3DDevice46()) {
		if (!_gldEnsureDevice(hWnd)) {
			gldLogMessage(GLDLOG_ERROR, "GL46: Failed to create D3D9 device\n");
			return FALSE;
		}
	}

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
	IDirect3DDevice9 *pDev;

	// Lazy device creation — create on the render thread to avoid deadlocks
	if (!gldGetD3DDevice46()) {
		HWND hw = hWnd ? hWnd : (ctx ? ctx->hWnd : NULL);
		if (!hw) hw = WindowFromDC(hDC);
		if (!_gldEnsureDevice(hw))
			return TRUE;
	}

	pDev = gldGetD3DDevice46();
	if (pDev) {
		HRESULT hr;

		// End the current scene
		IDirect3DDevice9_EndScene(pDev);

		// Present the back buffer
		hr = IDirect3DDevice9_Present(pDev, NULL, NULL, NULL, NULL);

		if (hr == D3DERR_DEVICELOST) {
			// Device lost — try to reset next frame
			hr = IDirect3DDevice9_TestCooperativeLevel(pDev);
			if (hr == D3DERR_DEVICENOTRESET) {
				D3DPRESENT_PARAMETERS d3dpp;
				D3DDISPLAYMODE d3ddm;
				IDirect3DDevice9_GetDisplayMode(pDev, 0, &d3ddm);
				ZeroMemory(&d3dpp, sizeof(d3dpp));
				d3dpp.Windowed = TRUE;
				d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
				d3dpp.BackBufferFormat = d3ddm.Format;
				d3dpp.BackBufferCount = 1;
				d3dpp.EnableAutoDepthStencil = TRUE;
				d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
				d3dpp.hDeviceWindow = hWnd ? hWnd : (ctx ? ctx->hWnd : NULL);
				IDirect3DDevice9_Reset(pDev, &d3dpp);
			}
		}

		// Begin a new scene for the next frame
		IDirect3DDevice9_BeginScene(pDev);

		return TRUE;
	}
	return TRUE;
}

//---------------------------------------------------------------------------
// gldGetProcAddress_GL46
//
// Return function pointers for GL/WGL extensions. Since we're a DX9 wrapper
// emulating OpenGL, we provide stub implementations for WGL extensions that
// apps use to negotiate context versions and query capabilities.
//---------------------------------------------------------------------------

// WGL extension stubs

static const char *WINAPI _wglGetExtensionsStringARB(HDC hDC)
{
	return "WGL_ARB_create_context WGL_ARB_create_context_profile "
	       "WGL_ARB_extensions_string WGL_EXT_extensions_string "
	       "WGL_ARB_pixel_format WGL_ARB_multisample "
	       "WGL_ARB_pbuffer WGL_EXT_swap_control";
}

static const char *WINAPI _wglGetExtensionsStringEXT(void)
{
	return "WGL_ARB_create_context WGL_ARB_create_context_profile "
	       "WGL_ARB_extensions_string WGL_EXT_extensions_string "
	       "WGL_ARB_pixel_format WGL_ARB_multisample "
	       "WGL_ARB_pbuffer WGL_EXT_swap_control";
}

static HGLRC WINAPI _wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
{
	// The wrapper accepts any version request — we emulate everything via DX9.
	// Replicate what wglCreateContext does internally.
	extern int gldGetPixelFormat(void);
	extern HGLRC gldCreateContext(HDC a, const GLD_pixelFormat *lpPF);
	int ipf = gldGetPixelFormat();
	if (ipf < 1 || ipf > glb.nPixelFormatCount)
		return NULL;
	return gldCreateContext(hDC, &glb.lpPF[ipf-1]);
}

static BOOL WINAPI _wglChoosePixelFormatARB(HDC hDC, const int *piAttribIList,
	const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	// Return pixel format 1 as a default match
	if (piFormats && nMaxFormats > 0) {
		piFormats[0] = 1;
		if (nNumFormats) *nNumFormats = 1;
		return TRUE;
	}
	if (nNumFormats) *nNumFormats = 0;
	return TRUE;
}

static BOOL WINAPI _wglGetPixelFormatAttribivARB(HDC hDC, int iPixelFormat,
	int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	UINT i;
	for (i = 0; i < nAttributes; i++) {
		// Return reasonable defaults for common attributes
		piValues[i] = 0;
	}
	return TRUE;
}

// Pbuffer stubs — Dolphin queries these but doesn't strictly need them
typedef void* HPBUFFERARB;

static HPBUFFERARB WINAPI _wglCreatePbufferARB(HDC hDC, int iPixelFormat,
	int iWidth, int iHeight, const int *piAttribList)
{
	return NULL;
}

static HDC WINAPI _wglGetPbufferDCARB(HPBUFFERARB hPbuffer)
{
	return NULL;
}

static int WINAPI _wglReleasePbufferDCARB(HPBUFFERARB hPbuffer, HDC hDC)
{
	return 0;
}

static BOOL WINAPI _wglDestroyPbufferARB(HPBUFFERARB hPbuffer)
{
	return FALSE;
}

static BOOL WINAPI _wglQueryPbufferARB(HPBUFFERARB hPbuffer, int iAttribute, int *piValue)
{
	if (piValue) *piValue = 0;
	return TRUE;
}

static BOOL WINAPI _wglGetPixelFormatAttribfvARB(HDC hDC, int iPixelFormat,
	int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	UINT i;
	for (i = 0; i < nAttributes; i++)
		if (pfValues) pfValues[i] = 0.0f;
	return TRUE;
}

static BOOL WINAPI _wglBindTexImageARB(HPBUFFERARB hPbuffer, int iBuffer)
{
	return TRUE;
}

static BOOL WINAPI _wglReleaseTexImageARB(HPBUFFERARB hPbuffer, int iBuffer)
{
	return TRUE;
}

static BOOL WINAPI _wglSetPbufferAttribARB(HPBUFFERARB hPbuffer, const int *piAttribList)
{
	return TRUE;
}

static BOOL WINAPI _wglSwapIntervalEXT(int interval)
{
	// TODO: Could map to D3D presentation interval
	return TRUE;
}

static int WINAPI _wglGetSwapIntervalEXT(void)
{
	return 0;
}

// Lookup table for extension functions
typedef struct {
	const char *name;
	PROC        proc;
} GLD_procEntry;

static const GLD_procEntry gl46ProcTable[] = {
	{ "wglGetExtensionsStringARB",    (PROC)_wglGetExtensionsStringARB },
	{ "wglGetExtensionsStringEXT",    (PROC)_wglGetExtensionsStringEXT },
	{ "wglCreateContextAttribsARB",   (PROC)_wglCreateContextAttribsARB },
	{ "wglChoosePixelFormatARB",      (PROC)_wglChoosePixelFormatARB },
	{ "wglGetPixelFormatAttribivARB", (PROC)_wglGetPixelFormatAttribivARB },
	{ "wglSwapIntervalEXT",           (PROC)_wglSwapIntervalEXT },
	{ "wglGetSwapIntervalEXT",        (PROC)_wglGetSwapIntervalEXT },
	{ "wglCreatePbufferARB",          (PROC)_wglCreatePbufferARB },
	{ "wglGetPbufferDCARB",           (PROC)_wglGetPbufferDCARB },
	{ "wglReleasePbufferDCARB",       (PROC)_wglReleasePbufferDCARB },
	{ "wglDestroyPbufferARB",         (PROC)_wglDestroyPbufferARB },
	{ "wglQueryPbufferARB",           (PROC)_wglQueryPbufferARB },
	{ "wglGetPixelFormatAttribfvARB", (PROC)_wglGetPixelFormatAttribfvARB },
	{ "wglBindTexImageARB",           (PROC)_wglBindTexImageARB },
	{ "wglReleaseTexImageARB",        (PROC)_wglReleaseTexImageARB },
	{ "wglSetPbufferAttribARB",       (PROC)_wglSetPbufferAttribARB },
	{ NULL, NULL }
};

// (no generic no-op — every function must be in the table with correct signature)

// Typed __stdcall no-ops for every argument count (0-16 DWORD-sized args).
// On x86, __stdcall requires the callee to pop N*4 bytes.
// On x64, there's only one calling convention so these all work.
static int APIENTRY _stub_noop_0(void) { return 0; }
static int APIENTRY _stub_noop_1(int a) { (void)a; return 0; }
static int APIENTRY _stub_noop_2(int a, int b) { (void)a;(void)b; return 0; }
static int APIENTRY _stub_noop_3(int a, int b, int c) { (void)a;(void)b;(void)c; return 0; }
static int APIENTRY _stub_noop_4(int a, int b, int c, int d) { (void)a;(void)b;(void)c;(void)d; return 0; }
static int APIENTRY _stub_noop_5(int a, int b, int c, int d, int e) { (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
static int APIENTRY _stub_noop_6(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0; }
static int APIENTRY _stub_noop_7(int a, int b, int c, int d, int e, int f, int g) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return 0; }
static int APIENTRY _stub_noop_8(int a, int b, int c, int d, int e, int f, int g, int h) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; return 0; }
static int APIENTRY _stub_noop_9(int a, int b, int c, int d, int e, int f, int g, int h, int i) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i; return 0; }
static int APIENTRY _stub_noop_10(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j; return 0; }
static int APIENTRY _stub_noop_11(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k; return 0; }
static int APIENTRY _stub_noop_12(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k;(void)l; return 0; }

typedef struct { const char *prefix; int argCount; } GLS_FuncArgCount;

/* Map GL function name prefixes/patterns to argument counts for the generic no-op */
static int _glsGuessArgCount(const char *name)
{
	/* Functions with known arg counts based on name patterns */
	/* 0 args */
	if (strstr(name, "glEnd") == name && strlen(name) <= 12) return 0;
	if (strcmp(name, "glFlush") == 0 || strcmp(name, "glFinish") == 0) return 0;
	if (strcmp(name, "glGetError") == 0) return 0;
	if (strcmp(name, "glPopDebugGroup") == 0) return 0;
	if (strcmp(name, "glReleaseShaderCompiler") == 0) return 0;
	if (strcmp(name, "glGetGraphicsResetStatus") == 0) return 0;
	if (strcmp(name, "glMemoryBarrierByRegion") == 0) return 0;
	if (strcmp(name, "glTextureBarrier") == 0) return 0;

	/* 1 arg */
	if (strstr(name, "glIs") == name) return 1; /* glIsQuery, glIsSync, etc */
	if (strstr(name, "glDelete") == name && strstr(name, "Sync")) return 1;
	if (strstr(name, "glEnable") == name && !strstr(name, "Array") && !strstr(name, "Attrib")) return 1;
	if (strstr(name, "glDisable") == name && !strstr(name, "Array") && !strstr(name, "Attrib")) return 1;
	if (strcmp(name, "glClampColor") == 0) return 2;
	if (strcmp(name, "glClearDepthf") == 0) return 1;
	if (strcmp(name, "glDepthRangef") == 0) return 2;
	if (strcmp(name, "glGenerateTextureMipmap") == 0) return 1;
	if (strcmp(name, "glBindTextureUnit") == 0) return 2;

	/* 2 args */
	if (strstr(name, "glGen") == name || strstr(name, "glCreate") == name) return 2;
	if (strstr(name, "glDelete") == name) return 2;
	if (strstr(name, "glBind") == name) return 2;
	if (strstr(name, "glUniform1") == name) return 2;
	if (strstr(name, "glVertexAttrib1") == name) return 2;
	if (strstr(name, "glMultiTexCoord1") == name) return 2;
	if (strstr(name, "glSecondaryColor3") == name) return 3;
	if (strstr(name, "glWindowPos2") == name) return 2;
	if (strstr(name, "glWindowPos3") == name) return 3;

	/* 3 args */
	if (strstr(name, "glUniform2") == name) return 3;
	if (strstr(name, "glVertexAttrib2") == name) return 3;
	if (strstr(name, "glMultiTexCoord2") == name) return 3;
	if (strstr(name, "glGet") == name && strstr(name, "iv")) return 3;
	if (strstr(name, "glGet") == name && strstr(name, "fv")) return 3;
	if (strstr(name, "glSampler") == name) return 3;
	if (strstr(name, "glTexParameter") == name) return 3;
	if (strstr(name, "glTexStorage1D") == name) return 4;
	if (strstr(name, "glShaderBinary") == 0) return 5;
	if (strstr(name, "glProgramBinary") == name) return 4;
	if (strstr(name, "glProgramParameteri") == name) return 3;

	/* 4 args */
	if (strstr(name, "glUniform3") == name) return 4;
	if (strstr(name, "glVertexAttrib3") == name) return 4;
	if (strstr(name, "glMultiTexCoord3") == name) return 4;
	if (strstr(name, "glScissorIndexed") == name) return 5;
	if (strstr(name, "glViewportIndexedf") == name && !strstr(name, "fv")) return 5;
	if (strstr(name, "glUniformMatrix") == name) return 4;
	if (strstr(name, "glFramebuffer") == name) return 5;
	if (strstr(name, "glInvalidate") == name) return 3;

	/* 5 args */
	if (strstr(name, "glUniform4") == name) return 5;
	if (strstr(name, "glVertexAttrib4") == name) return 5;
	if (strstr(name, "glMultiTexCoord4") == name) return 5;
	if (strstr(name, "glTexStorage2D") == name) return 5;
	if (strstr(name, "glCopyImageSubData") == name) return 11;
	if (strstr(name, "glVertexAttribI") == name) return 5;
	if (strstr(name, "glVertexAttribFormat") == name) return 5;
	if (strstr(name, "glVertexArray") == name) return 5;

	/* 6+ args */
	if (strstr(name, "glTexStorage3D") == name) return 6;
	if (strstr(name, "glDrawElements") == name) return 5;
	if (strstr(name, "glDrawArrays") == name) return 4;
	if (strstr(name, "glDebugMessage") == name) return 6;
	if (strstr(name, "glPushDebugGroup") == name) return 4;
	if (strstr(name, "glObjectLabel") == name) return 4;
	if (strstr(name, "glReadnPixels") == name) return 8;
	if (strstr(name, "glTextureSubImage") == name) return 11;
	if (strstr(name, "glTextureStorage") == name) return 5;
	if (strstr(name, "glNamedBuffer") == name) return 4;
	if (strstr(name, "glNamedFramebuffer") == name) return 4;
	if (strstr(name, "glNamedRenderbuffer") == name) return 5;
	if (strstr(name, "glCompressedTexture") == name) return 9;
	if (strstr(name, "glCopyTexture") == name) return 9;
	if (strstr(name, "glBlitNamed") == name) return 12;
	if (strstr(name, "glClearNamed") == name) return 4;
	if (strstr(name, "glTransformFeedback") == name) return 3;
	if (strstr(name, "glGetQuery") == name) return 4;
	if (strstr(name, "glMap") == name) return 4;
	if (strstr(name, "glFlushMapped") == name) return 3;
	if (strstr(name, "glDepthRange") == name) return 3;

	/* Default: 4 args covers most GL functions */
	return 4;
}

static PROC _glsGetTypedNoop(int argCount)
{
	switch (argCount) {
	case 0:  return (PROC)_stub_noop_0;
	case 1:  return (PROC)_stub_noop_1;
	case 2:  return (PROC)_stub_noop_2;
	case 3:  return (PROC)_stub_noop_3;
	case 4:  return (PROC)_stub_noop_4;
	case 5:  return (PROC)_stub_noop_5;
	case 6:  return (PROC)_stub_noop_6;
	case 7:  return (PROC)_stub_noop_7;
	case 8:  return (PROC)_stub_noop_8;
	case 9:  return (PROC)_stub_noop_9;
	case 10: return (PROC)_stub_noop_10;
	case 11: return (PROC)_stub_noop_11;
	default: return (PROC)_stub_noop_12;
	}
}

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

	// Unknown function — return a typed no-op with the correct arg count
	// so __stdcall stack cleanup is correct on x86.
	gldDiagLog("wglGetProcAddress: \"%s\" -> UNMAPPED", a ? a : "(null)");
	{
		int argCount = _glsGuessArgCount(a);
		return _glsGetTypedNoop(argCount);
	}
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
