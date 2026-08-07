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
#include "gld_diag.h"
#include "gl_impl.h"
#include "glsl_to_hlsl.h"
#include "pixel_format_provider.h"

/* The wrapper's currently selected pixel format, owned by gld_wgl.c, which
 * publishes it through this one accessor and no header. */
extern int gldGetPixelFormat(void);

// ***********************************************************************
// D3D9 globals for the GL46 backend
// ***********************************************************************

typedef IDirect3D9* (WINAPI *FNDIRECT3DCREATE9)(UINT);

// Texture-format capability cache.  State 0 means "not yet probed" rather
// than "unsupported" so the ZeroMemory(&gl46Globals, ...) in
// gldInitContext46 leaves the table empty instead of poisoned to "no".
#define GLD_FMTCACHE_UNKNOWN        0
#define GLD_FMTCACHE_SUPPORTED      1
#define GLD_FMTCACHE_UNSUPPORTED    2

// Headroom over the 11 distinct formats the GL format mappers can return
// (A8R8G8B8, X8R8G8B8, L8, A8, A8L8, R5G6B5, A4R4G4B4, A1R5G5B5,
//  DXT1, DXT3, DXT5).
#define GLD_FMTCACHE_ENTRIES        16

// Arbitrary non-zero, non-repeating pattern.  Non-zero matters: the whole
// struct is ZeroMemory'd at init, so a zero guard could not be told apart
// from "never initialised".
#define GLD_FMTCACHE_GUARD          0x5AFEC0DEu

typedef struct {
    D3DFORMAT           fmt;                // Format this entry describes
    unsigned char       texState;           // D3DRTYPE_TEXTURE probe result
    unsigned char       cubeState;          // D3DRTYPE_CUBETEXTURE probe result
} GLD_gl46_fmt_entry;

typedef struct {
    HINSTANCE           hD3D9DLL;           // Handle to d3d9.dll
    FNDIRECT3DCREATE9   fnDirect3DCreate9;  // Direct3DCreate9 function pointer
    BOOL                bDirect3D;          // Persistant Direct3D9 exists
    BOOL                bDirect3DDevice;    // Persistant Direct3DDevice9 exists
    IDirect3D9          *pD3D;              // Persistant Direct3D9
    IDirect3DDevice9    *pDev;              // Persistant Direct3DDevice9
    UINT                availableTextureMem; // Last non-zero D3D9 memory estimate
    BOOL                bRemixDetected;     // NVIDIA DXVK Remix d3d9.dll detected
    // Guard bands around the format cache.  fmtCacheCount doubles as the loop
    // bound for a scan over fmtCache, so a stray write that lands on it turns
    // that scan into an unbounded read.  These two words make the next such
    // corruption say which side it came from instead of only that it happened:
    // guardLo intact + guardHi broken means a forward overrun of fmtCache
    // itself, both broken means a wider write, neither means the count was hit
    // on its own by a wild pointer from somewhere else entirely.
    UINT                guardLo;            // == GLD_FMTCACHE_GUARD
    GLD_gl46_fmt_entry  fmtCache[GLD_FMTCACHE_ENTRIES]; // Per-format creatability
    UINT                fmtCacheCount;      // Entries in use in fmtCache
    UINT                guardHi;            // == GLD_FMTCACHE_GUARD
    // Device caps as reported at device-creation time.  Cached here for the
    // same reason as fmtCache: the answer never changes for the life of the
    // device, and the shader transpiler needs the two shader-version fields on
    // every compile to know which D3DCompile profile the device can load.
    D3DCAPS9            dxCaps;
    BOOL                bDxCapsValid;
} GLD_gl46_dx9_globals;

static GLD_gl46_dx9_globals gl46Globals;

// Serialises device creation.
//
// Both creation paths were check-then-act: they tested gl46Globals.pDev, then
// created a device and published it many calls later.  An application that
// issues GL from more than one thread - id Tech 4 does exactly that once it
// enables SMP - can have two threads pass that test together, create two
// devices, and leave one of them orphaned with the global flipping between
// them.  Against an RTX Remix d3d9.dll a second device is worse than a leak,
// because the bridge tracks every COM handle it hands out.
//
// SRWLOCK rather than CRITICAL_SECTION deliberately: it is statically
// initialised and has no destroy call, so there is no teardown ordering to get
// wrong and no deleted-lock window during module eviction.
static SRWLOCK g_deviceLock = SRWLOCK_INIT;

// WGL_EXT_swap_control state.
//
// s_swapInterval is what the application last asked for, -1 until the first
// query seeds it from the INI's bWaitForRetrace.  s_deviceSwapInterval is the
// interval the live device was actually built with, -1 while no device exists,
// and the two differing is what tells SwapBuffers a Reset is owed.  A plain int
// read/write is atomic on both targets, and the worst a racing writer can cost
// is one frame presented at the previous interval.
static int s_swapInterval       = -1;
static int s_deviceSwapInterval = -1;

// ***********************************************************************
// Initialize the D3D9 layer for the GL46 backend.
// Loads d3d9.dll and obtains Direct3DCreate9.
// Returns TRUE on success, FALSE on failure.
// ***********************************************************************

BOOL gldInitContext46(void)
{
    char dllPath[MAX_PATH];
    char modulePath[MAX_PATH];
    char *lastSlash;

    ZeroMemory(&gl46Globals, sizeof(gl46Globals));
    gl46Globals.guardLo = GLD_FMTCACHE_GUARD;
    gl46Globals.guardHi = GLD_FMTCACHE_GUARD;

    // Load d3d9.dll from the game directory (supports local d3d9.dll wrappers)
    GetModuleFileName(NULL, modulePath, MAX_PATH);
    lastSlash = strrchr(modulePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strcpy(dllPath, modulePath);
        strcat(dllPath, "d3d9.dll");
        gl46Globals.hD3D9DLL = LoadLibraryA(dllPath);
        if (gl46Globals.hD3D9DLL) {
            gldLogPrintf(GLDLOG_SYSTEM, "GL46: Loaded d3d9.dll from %s", dllPath);

            // Detect NVIDIA DXVK Remix — it exports "Direct3DCreate9Ex" and
            // has a "remixInitialize" or ".trex" folder nearby, or exports
            // remix-specific functions. Check for the Remix bridge marker.
            {
                PROC pRemixInit = GetProcAddress(gl46Globals.hD3D9DLL, "remixapi_InitializeLibrary");
                if (!pRemixInit) {
                    // Also check for .trex folder which is the Remix bridge
                    char trexPath[MAX_PATH];
                    strcpy(trexPath, modulePath);
                    strcat(trexPath, ".trex");
                    DWORD attr = GetFileAttributesA(trexPath);
                    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                        pRemixInit = (PROC)1; // marker
                    }
                }
                if (pRemixInit) {
                    gl46Globals.bRemixDetected = TRUE;
                    gldLogMessage(GLDLOG_SYSTEM, "GL46: NVIDIA DXVK Remix detected — will use D3D9 fixed-function pipeline\n");
                } else {
                    gl46Globals.bRemixDetected = FALSE;
                }
            }
        }
    }

    if (gl46Globals.hD3D9DLL == NULL) {
        /* No application-local interceptor: use the operating-system D3D9
         * runtime. Restrict the first fallback to System32 so a different
         * working-directory DLL cannot be injected accidentally. */
        gl46Globals.hD3D9DLL = LoadLibraryExA("d3d9.dll", NULL,
                                              LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (gl46Globals.hD3D9DLL == NULL)
            gl46Globals.hD3D9DLL = LoadLibraryA("d3d9.dll");
        if (gl46Globals.hD3D9DLL == NULL) {
            gldLogMessage(GLDLOG_ERROR, "GL46: D3D9 runtime is unavailable\n");
            return FALSE;
        }
        gldLogMessage(GLDLOG_SYSTEM, "GL46: using the system D3D9 runtime\n");
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
    /* Announced unconditionally, because the absence of this line is itself
     * the answer to a question that is otherwise unanswerable from outside:
     * whether teardown ran at all.  A d3d9.dll that reports live objects at
     * module eviction means either they were leaked or shutdown never
     * happened, and those need completely different fixes.  A crashing process
     * does not reach orderly teardown, so "no line" means stop looking for a
     * leak and go fix the crash. */
    gldDiagLog("GL46: shutdown starting (device=%p, d3d9=%p)",
               (void *)gl46Globals.pDev, (void *)gl46Globals.pD3D);

    if (gl46Globals.pDev) {
        ULONG refs;
        // Stop handing this device out *before* destroying it.
        //
        // gldGetD3DDevice46 returns this pointer, and an application is free
        // to issue GL from more than one thread - id Tech 4 does exactly that
        // once it enables SMP, running the backend on its own thread.  While
        // the release below was in progress the global still held the old
        // pointer, so a call arriving on another thread passed the NULL check
        // and then dereferenced a device that was being torn down.  That was
        // observed as a fault on the vtable load in glFinish's CreateQuery,
        // immediately after "Enabling SMP".
        //
        // Clearing first narrows that to the case where the other thread had
        // already loaded the pointer; it does not make the wrapper thread-safe,
        // but it removes the window that a single global read opens.
        IDirect3DDevice9 *pDying = gl46Globals.pDev;
        gl46Globals.pDev = NULL;
        gl46Globals.bDirect3DDevice = FALSE;

        // The GL object table is process-global and has outlived every
        // context created against this device.  This is the one moment it
        // can be emptied, and it has to happen before the device goes away
        // or every texture, shader and query in it is simply abandoned.
        _glsReleaseAllDeviceResources(pDying);

        /* Release reports the count that remains.  Anything but zero means
         * something else still holds this device - a surface, a swapchain, an
         * interface obtained through a Get* call that AddRef'd and was never
         * released - and names that as a real leak rather than a guess. */
        refs = IDirect3DDevice9_Release(pDying);
        if (refs != 0)
            gldDiagLog("GL46: device still has %lu reference(s) after release - "
                       "something is holding it and it will survive to module eviction",
                       (unsigned long)refs);
        else
            gldDiagLog("GL46: device released cleanly (0 references remain)");
    }
    if (gl46Globals.pD3D) {
        /* Same unpublish-before-destroy ordering as the device above. */
        IDirect3D9 *pD3DDying = gl46Globals.pD3D;
        ULONG refs;
        gl46Globals.pD3D = NULL;
        gl46Globals.bDirect3D = FALSE;
        refs = IDirect3D9_Release(pD3DDying);
        if (refs != 0)
            gldDiagLog("GL46: IDirect3D9 still has %lu reference(s) after release",
                       (unsigned long)refs);
    }
    if (gl46Globals.hD3D9DLL) {
        FreeLibrary(gl46Globals.hD3D9DLL);
        gl46Globals.hD3D9DLL = NULL;
    }
    gl46Globals.fnDirect3DCreate9 = NULL;
    gl46Globals.bDirect3D = FALSE;
    gl46Globals.bDirect3DDevice = FALSE;
}

// ***********************************************************************
// Build the presentation parameters this backend presents with.
//
// Device creation and every later Reset go through here so they cannot
// disagree about the back buffer's size, format or depth-stencil policy.
// The previous copy in gldSwapBuffers_GL46 never set a size at all, which
// left the runtime to pick one from a window that may not have been sized
// yet — the leading explanation for a swapchain far smaller than the
// resolution the game actually renders at.
// ***********************************************************************

void gldBuildPresentParams46(HWND hWnd, D3DPRESENT_PARAMETERS *out)
{
    D3DDISPLAYMODE  d3ddm;
    D3DCAPS9        d3dCaps;
    RECT            rc;

    if (!out)
        return;

    ZeroMemory(out, sizeof(*out));

    // Nothing to ask about the adapter.  The caller gets a zeroed block it
    // can still inspect and log rather than uninitialised stack.
    if (!gl46Globals.pD3D)
        return;

    ZeroMemory(&d3ddm, sizeof(d3ddm));
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(gl46Globals.pD3D, glb.dwAdapter, &d3ddm)))
        return;

    out->Windowed               = TRUE;
    out->SwapEffect             = D3DSWAPEFFECT_DISCARD;
    out->BackBufferFormat       = d3ddm.Format;
    out->BackBufferCount        = 1;
    out->EnableAutoDepthStencil = FALSE;
    out->hDeviceWindow          = hWnd;
    out->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;

    /*
     * Honour the pixel format the application selected.
     *
     * Without this the device was always created with EnableAutoDepthStencil
     * FALSE and no depth-stencil surface anywhere, so a format that promised
     * 24-bit depth and 8-bit stencil delivered neither: glEnable(GL_DEPTH_TEST)
     * turned on D3DRS_ZENABLE against a device with nothing to test against,
     * and stencil work had no buffer at all.  The format list is probed
     * against the loaded d3d9.dll, so whatever it names here is a combination
     * that runtime already said it would accept.
     */
    {
        GLD_pf46Entry entry;

        if (gldGetPixelFormatD3D46(gldGetPixelFormat(), &entry)) {
            out->BackBufferFormat  = entry.colorFormat;
            out->MultiSampleType   = entry.msType;
            out->MultiSampleQuality = entry.msQuality;

            if (entry.depthFormat != D3DFMT_UNKNOWN) {
                out->EnableAutoDepthStencil = TRUE;
                out->AutoDepthStencilFormat = entry.depthFormat;
            }
        }
    }

    if (hWnd && GetClientRect(hWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
        out->BackBufferWidth  = rc.right;
        out->BackBufferHeight = rc.bottom;
    } else {
        out->BackBufferWidth  = d3ddm.Width;
        out->BackBufferHeight = d3ddm.Height;
    }

    /*
     * Presentation interval, from whatever the application last asked
     * wglSwapIntervalEXT for.  This backend always presents windowed, and
     * D3D9 accepts only DEFAULT, ONE and IMMEDIATE there, so an interval of
     * 2 or more becomes ONE rather than being refused.
     */
    {
        int   interval = gldGetSwapInterval46();
        DWORD wanted   = (interval <= 0) ? D3DPRESENT_INTERVAL_IMMEDIATE
                                         : D3DPRESENT_INTERVAL_ONE;

        ZeroMemory(&d3dCaps, sizeof(d3dCaps));
        if (SUCCEEDED(IDirect3D9_GetDeviceCaps(gl46Globals.pD3D, glb.dwAdapter,
                                               D3DDEVTYPE_HAL, &d3dCaps)) &&
            (d3dCaps.PresentationIntervals & wanted))
            out->PresentationInterval = wanted;
        else if (wanted == D3DPRESENT_INTERVAL_ONE)
            out->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    }
}

// ***********************************************************************
// WGL_EXT_swap_control
// ***********************************************************************

BOOL gldSetSwapInterval46(int interval)
{
    /*
     * A negative interval means adaptive vsync, which belongs to
     * WGL_EXT_swap_control_tear.  That extension is not advertised, so the
     * spec's answer is to refuse the value rather than silently clamp it.
     */
    if (interval < 0) {
        gldLogPrintf(GLDLOG_WARN,
            "wglSwapIntervalEXT: interval %d needs WGL_EXT_swap_control_tear",
            interval);
        return FALSE;
    }

    if (interval != s_swapInterval) {
        gldLogPrintf(GLDLOG_SYSTEM,
            "wglSwapIntervalEXT: swap interval %d -> %d (applied at the next present)",
            s_swapInterval, interval);
        s_swapInterval = interval;
    }

    return TRUE;
}

// ***********************************************************************

int gldGetSwapInterval46(void)
{
    /*
     * Until the application says otherwise the INI's bWaitForRetrace decides,
     * which keeps the setting meaningful for titles that never call
     * wglSwapIntervalEXT at all.
     */
    if (s_swapInterval < 0)
        s_swapInterval = glb.bWaitForRetrace ? 1 : 0;

    return s_swapInterval;
}

// ***********************************************************************

BOOL gldSwapIntervalNeedsReset46(void)
{
    /* Before the first device is built there is nothing to reset. */
    if (s_deviceSwapInterval < 0)
        return FALSE;

    return (gldGetSwapInterval46() != s_deviceSwapInterval);
}

// ***********************************************************************

void gldNoteSwapIntervalApplied46(void)
{
    /*
     * Called only where a device was actually created or Reset with the
     * parameters gldBuildPresentParams46() produced.  Recording it there
     * instead would claim the interval had changed even when the Reset that
     * was meant to carry it failed, and the retry would never happen.
     */
    s_deviceSwapInterval = gldGetSwapInterval46();
}

// ***********************************************************************
// Apply the render and transform state this backend starts a frame from.
// Shared by device creation and by both Reset paths, since a Reset clears
// device state exactly as a fresh device would.
// ***********************************************************************

void gldApplyDefaultDeviceState46(IDirect3DDevice9 *pDev)
{
    D3DMATRIX identity;

    if (!pDev)
        return;

    ZeroMemory(&identity, sizeof(identity));
    identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;

    __try {
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
        /* OpenGL starts with GL_DEPTH_TEST disabled.  D3D9 starts with depth
         * enabled when a depth surface exists, so leaving its default intact
         * rejects otherwise valid first-frame draws against uncleared depth. */
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_CLIPPING, TRUE);

        IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &identity);
        IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &identity);
        IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &identity);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        /* Device lost or invalid — the next frame's Reset deals with it. */
    }
}

// ***********************************************************************
// Create a D3D9 device for the GL46 context.
// Full device creation — no lazy, no dummy handles.
// ***********************************************************************

HGLRC gldCreateContext46(HDC hDC, GLint *pMajor, GLint *pMinor)
{
    IDirect3DDevice9        *pDev = NULL;
    D3DPRESENT_PARAMETERS   d3dpp;
    D3DDISPLAYMODE          d3ddm;
    D3DCAPS9                d3dCaps;
    D3DADAPTER_IDENTIFIER9  d3dIdent;
    DWORD                   dwBehaviourFlags;
    HRESULT                 hr;
    HWND                    hWnd;
    BOOL                    bDesktopFallback;
    RECT                    rcClient;
    BOOL                    bGotClientRect;

    // Load D3D9 if not already loaded
    if (!gl46Globals.fnDirect3DCreate9) {
        if (!gldInitContext46()) {
            gldLogMessage(GLDLOG_ERROR, "GL46: D3D9 not available\n");
            return NULL;
        }
    }

    // Ensure we have the IDirect3D9 interface
    if (!gl46Globals.pD3D) {
        gl46Globals.pD3D = gl46Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!gl46Globals.pD3D) {
            gldLogMessage(GLDLOG_ERROR, "GL46: Direct3DCreate9 failed\n");
            return NULL;
        }
        gl46Globals.bDirect3D = TRUE;
    }

    // Get the window handle
    hWnd = WindowFromDC(hDC);
    bDesktopFallback = FALSE;
    if (!hWnd) {
        hWnd = GetDesktopWindow();
        bDesktopFallback = TRUE;
    }
    ZeroMemory(&rcClient, sizeof(rcClient));
    bGotClientRect = (hWnd && GetClientRect(hWnd, &rcClient)) ? TRUE : FALSE;

    // The device already exists — reuse it rather than create a second one.
    //
    // gl46Globals.pDev is process-global ("Persistant Direct3DDevice9" above)
    // and every texture, shader and query in the GL object table is bound to
    // it.  Destroying and recreating it on each wglCreateContext left all of
    // those pointing at a dead device; presenting the same device against this
    // context's window instead is what Reset is for.
    if (gl46Globals.pDev) {
        HRESULT hrReset = E_FAIL;

        gldBuildPresentParams46(hWnd, &d3dpp);

        gldDiagLog("GL46: context reuse Reset hWnd=%p desktopFallback=%d "
                   "clientRect=%s %ldx%ld -> backbuffer %ux%u",
                   (void*)hWnd, (int)bDesktopFallback,
                   bGotClientRect ? "ok" : "unavailable",
                   bGotClientRect ? (long)rcClient.right  : 0L,
                   bGotClientRect ? (long)rcClient.bottom : 0L,
                   d3dpp.BackBufferWidth, d3dpp.BackBufferHeight);

        // Reset destroys anything the runtime cannot carry across it and
        // refuses to run while such an object is still alive.  Managed
        // textures and shader objects survive by contract and are kept.
        _glsReleaseDeviceLosableResources(gl46Globals.pDev);

        // There may or may not be an open scene, depending on whether the
        // previous context ever reached SwapBuffers.
        __try {
            IDirect3DDevice9_EndScene(gl46Globals.pDev);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }

        __try {
            hrReset = IDirect3DDevice9_Reset(gl46Globals.pDev, &d3dpp);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hrReset = E_FAIL;
        }

        if (FAILED(hrReset)) {
            // Failing the whole context creation would take down a title that
            // is otherwise fine; the device is still there and the next
            // SwapBuffers gets another chance at a Reset.
            gldLogPrintf(GLDLOG_ERROR,
                "GL46: Reset on context reuse failed (0x%08X)", hrReset);
            gldDiagLog("GL46: Reset on context reuse failed (0x%08X)",
                       (unsigned)hrReset);
        } else {
            gldNoteSwapIntervalApplied46();
        }

        __try {
            IDirect3DDevice9_BeginScene(gl46Globals.pDev);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }

        gldApplyDefaultDeviceState46(gl46Globals.pDev);

        if (pMajor) *pMajor = 4;
        if (pMinor) *pMinor = 6;
        return (HGLRC)(INT_PTR)1;
    }

    // Get display mode.  gldBuildPresentParams46 asks for this again; the call
    // is kept here as the adapter validity gate it has always been, so a dead
    // adapter is reported before anything is created.
    hr = IDirect3D9_GetAdapterDisplayMode(gl46Globals.pD3D, glb.dwAdapter, &d3ddm);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: GetAdapterDisplayMode failed (0x%08X)", hr);
        return NULL;
    }

    // Get device caps
    hr = IDirect3D9_GetDeviceCaps(gl46Globals.pD3D, glb.dwAdapter, D3DDEVTYPE_HAL, &d3dCaps);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: GetDeviceCaps failed (0x%08X)", hr);
        return NULL;
    }

    // The shader transpiler targets whichever D3DCompile profile this device
    // can actually load; CreateVertexShader/CreatePixelShader reject anything
    // above the reported version, so the caps are a hard ceiling rather than a
    // hint.  Pushed rather than pulled: glsl_to_hlsl.c is deliberately
    // buildable without this module.
    gl46Globals.dxCaps = d3dCaps;
    gl46Globals.bDxCapsValid = TRUE;
    glslSetDeviceCaps(&d3dCaps);
    gldDiagLog("GL46: device shader models VS 0x%08X PS 0x%08X",
               (unsigned)d3dCaps.VertexShaderVersion,
               (unsigned)d3dCaps.PixelShaderVersion);

    // Set up presentation parameters — explicit size for DXVK compatibility
    gldBuildPresentParams46(hWnd, &d3dpp);

    gldDiagLog("GL46: create device hWnd=%p desktopFallback=%d "
               "clientRect=%s %ldx%ld -> backbuffer %ux%u",
               (void*)hWnd, (int)bDesktopFallback,
               bGotClientRect ? "ok" : "unavailable",
               bGotClientRect ? (long)rcClient.right  : 0L,
               bGotClientRect ? (long)rcClient.bottom : 0L,
               d3dpp.BackBufferWidth, d3dpp.BackBufferHeight);

    // Vertex processing + multithreaded
    dwBehaviourFlags = (d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
        D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    dwBehaviourFlags |= D3DCREATE_MULTITHREADED;
    if (!glb.bFastFPU)
        dwBehaviourFlags |= D3DCREATE_FPU_PRESERVE;

    // Create the device
    hr = IDirect3D9_CreateDevice(gl46Globals.pD3D,
        glb.dwAdapter,
        D3DDEVTYPE_HAL,
        hWnd,
        dwBehaviourFlags,
        &d3dpp,
        &pDev);

    if (FAILED(hr) || !pDev) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: CreateDevice failed (0x%08X)", hr);
        return NULL;
    }

    // The device now carries the presentation interval those parameters asked
    // for, so a later wglSwapIntervalEXT can tell whether it changed anything.
    gldNoteSwapIntervalApplied46();

    {
        UINT available = IDirect3DDevice9_GetAvailableTextureMem(pDev);
        if (available)
            gl46Globals.availableTextureMem = available;
    }

    // Begin the first scene
    IDirect3DDevice9_BeginScene(pDev);

    // Set default render states and identity transforms
    gldApplyDefaultDeviceState46(pDev);

    // Store the device last, for the same reason as _gldEnsureDevice: this
    // pointer is handed to any thread that calls gldGetD3DDevice46, so it must
    // not be visible before the scene and default state above are in place.
    gl46Globals.pDev = pDev;
    gl46Globals.bDirect3DDevice = TRUE;

    // Log adapter info
    if (SUCCEEDED(IDirect3D9_GetAdapterIdentifier(gl46Globals.pD3D, glb.dwAdapter, 0, &d3dIdent))) {
        gldLogPrintf(GLDLOG_SYSTEM, "GL46: GPU: %s", d3dIdent.Description);
        gldLogPrintf(GLDLOG_SYSTEM, "GL46: Driver: %s %d.%d.%02d.%d",
            d3dIdent.Driver,
            HIWORD(d3dIdent.DriverVersion.HighPart),
            LOWORD(d3dIdent.DriverVersion.HighPart),
            HIWORD(d3dIdent.DriverVersion.LowPart),
            LOWORD(d3dIdent.DriverVersion.LowPart));
    }

    gldLogPrintf(GLDLOG_SYSTEM, "GL46: D3D9 device created: %p, HWND=%p, %ux%u",
        (void*)pDev, (void*)hWnd, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight);

    // Report emulated GL version
    if (pMajor) *pMajor = 4;
    if (pMinor) *pMinor = 6;

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
    D3DDEVTYPE              deviceType = D3DDEVTYPE_HAL;
    DWORD                   dwBehaviourFlags;
    HRESULT                 hr;

    // Already have a device
    if (gl46Globals.pDev)
        return TRUE;

    // D3D9 not available
    if (!gl46Globals.fnDirect3DCreate9)
        return FALSE;

    // Take the lock and re-test.  The check above is an uncontended fast path;
    // this is the one that decides, so two threads arriving together create one
    // device between them instead of one each.
    AcquireSRWLockExclusive(&g_deviceLock);
    if (gl46Globals.pDev) {
        ReleaseSRWLockExclusive(&g_deviceLock);
        return TRUE;
    }

    if (!hWnd)
        hWnd = GetDesktopWindow();

    // Don't create device for zero-size windows (DXVK will fail)
    {
        RECT rc;
        if (GetClientRect(hWnd, &rc) && rc.right == 0 && rc.bottom == 0) {
            ReleaseSRWLockExclusive(&g_deviceLock);
            return FALSE;
        }
    }

    // Use the existing D3D9 interface (created in gldInitContext46)
    pD3D = gl46Globals.pD3D;
    if (!pD3D) {
        pD3D = gl46Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!pD3D) {
            ReleaseSRWLockExclusive(&g_deviceLock);
            return FALSE;
        }
        gl46Globals.pD3D = pD3D;
        gl46Globals.bDirect3D = TRUE;
    }

    // Get display mode
    hr = IDirect3D9_GetAdapterDisplayMode(pD3D, glb.dwAdapter, &d3ddm);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: GetAdapterDisplayMode failed (0x%08X)", hr);
        gldDiagLog("GL46: GetAdapterDisplayMode failed (0x%08lX), adapter=%lu",
                   (unsigned long)hr, (unsigned long)glb.dwAdapter);
        ReleaseSRWLockExclusive(&g_deviceLock);
        return FALSE;
    }

    // Get device caps
    hr = IDirect3D9_GetDeviceCaps(pD3D, glb.dwAdapter, D3DDEVTYPE_HAL, &d3dCaps);
    if (FAILED(hr)) {
        /* Remote/headless sessions can expose D3D9 without a HAL device. The
         * reference rasterizer is slow but preserves presentation semantics. */
        deviceType = D3DDEVTYPE_REF;
        hr = IDirect3D9_GetDeviceCaps(pD3D, glb.dwAdapter, deviceType, &d3dCaps);
        if (FAILED(hr)) {
            gldLogPrintf(GLDLOG_ERROR, "GL46: GetDeviceCaps failed (0x%08X)", hr);
            gldDiagLog("GL46: HAL and REF GetDeviceCaps failed (0x%08lX), adapter=%lu",
                       (unsigned long)hr, (unsigned long)glb.dwAdapter);
            ReleaseSRWLockExclusive(&g_deviceLock);
            return FALSE;
        }
        gldDiagLog("GL46: HAL unavailable; using the D3D9 reference device");
    }

    // Second device-creation path; same shader-model ceiling push as
    // gldCreateContext46, for the same reason.
    gl46Globals.dxCaps = d3dCaps;
    gl46Globals.bDxCapsValid = TRUE;
    glslSetDeviceCaps(&d3dCaps);
    gldDiagLog("GL46: device shader models VS 0x%08X PS 0x%08X",
               (unsigned)d3dCaps.VertexShaderVersion,
               (unsigned)d3dCaps.PixelShaderVersion);

    // Set up presentation parameters — explicit size for DXVK compatibility
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed               = TRUE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat        = d3ddm.Format;
    d3dpp.BackBufferCount         = 1;
    d3dpp.EnableAutoDepthStencil  = FALSE;
    d3dpp.hDeviceWindow           = hWnd;
    d3dpp.PresentationInterval    = D3DPRESENT_INTERVAL_DEFAULT;

    /* Same pixel-format plumbing as gldBuildPresentParams46(); see the note
     * there for why a device without a depth-stencil surface silently broke
     * every format that advertised depth or stencil bits. */
    {
        GLD_pf46Entry entry;

        if (gldGetPixelFormatD3D46(gldGetPixelFormat(), &entry)) {
            d3dpp.BackBufferFormat    = entry.colorFormat;
            d3dpp.MultiSampleType     = entry.msType;
            d3dpp.MultiSampleQuality  = entry.msQuality;

            if (entry.depthFormat != D3DFMT_UNKNOWN) {
                d3dpp.EnableAutoDepthStencil = TRUE;
                d3dpp.AutoDepthStencilFormat = entry.depthFormat;
            }
        }
    }
    {
        RECT rc;
        if (hWnd && GetClientRect(hWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
            d3dpp.BackBufferWidth  = rc.right;
            d3dpp.BackBufferHeight = rc.bottom;
        } else {
            d3dpp.BackBufferWidth  = d3ddm.Width;
            d3dpp.BackBufferHeight = d3ddm.Height;
        }
    }

    /* Same presentation-interval policy as gldBuildPresentParams46(), which
     * this path cannot call because it runs before gl46Globals.pD3D is
     * published and would be handed a zeroed parameter block. */
    {
        int   interval = gldGetSwapInterval46();
        DWORD wanted   = (interval <= 0) ? D3DPRESENT_INTERVAL_IMMEDIATE
                                         : D3DPRESENT_INTERVAL_ONE;

        if (d3dCaps.PresentationIntervals & wanted)
            d3dpp.PresentationInterval = wanted;
    }

    // Always multithreaded + vertex processing
    dwBehaviourFlags = (d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
        D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    dwBehaviourFlags |= D3DCREATE_MULTITHREADED;
    if (!glb.bFastFPU)
        dwBehaviourFlags |= D3DCREATE_FPU_PRESERVE;

    hr = IDirect3D9_CreateDevice(pD3D,
        glb.dwAdapter,
        deviceType,
        hWnd,
        dwBehaviourFlags,
        &d3dpp,
        &pDev);

    // !pDev checked as well as the HRESULT: gldCreateContext46 has always
    // tested both, and a d3d9.dll that reports success without writing the
    // out-parameter would otherwise publish NULL as if it were a device.
    if (FAILED(hr) || !pDev) {
        gldLogPrintf(GLDLOG_ERROR, "GL46: CreateDevice failed (0x%08X)", hr);
        gldDiagLog("GL46: CreateDevice failed (0x%08lX), HWND=%p, %ux%u fmt=%d",
                   (unsigned long)hr, (void *)hWnd,
                   d3dpp.BackBufferWidth, d3dpp.BackBufferHeight,
                   (int)d3dpp.BackBufferFormat);
        ReleaseSRWLockExclusive(&g_deviceLock);
        return FALSE;
    }

    gldNoteSwapIntervalApplied46();

    {
        UINT available = IDirect3DDevice9_GetAvailableTextureMem(pDev);
        if (available)
            gl46Globals.availableTextureMem = available;
    }

    // Begin the first scene so glClear and draw calls work immediately
    IDirect3DDevice9_BeginScene(pDev);

    // Set default render states for a clean starting point
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Publish last.  Everything above is setup this device needs before anyone
    // draws with it, and gldGetD3DDevice46 hands this pointer to any thread
    // that asks - so it must not become visible until it is ready to use.
    gl46Globals.pDev = pDev;
    gl46Globals.bDirect3DDevice = TRUE;

    ReleaseSRWLockExclusive(&g_deviceLock);

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

    // --- The D3D9 device deliberately stays alive ---
    //
    // It is process-global, not per-context: releasing it here destroyed it
    // out from under the GL object table (and out from under any other logical
    // context still using it), leaving every texture, shader and query in
    // g_glState pointing at a dead device, and forced a fresh CreateDevice for
    // the next context.  gldCreateContext46 now Resets it instead, and
    // gldShutdownContext46 is the single place it is released.

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
// Return D3D9's current texture-memory estimate in KiB.  The last non-zero
// value is retained because Wolfenstein queries this after deleting its
// temporary capability-discovery context.
// ***********************************************************************

UINT gldGetAvailableVideoMemoryKB46(void)
{
    UINT available;

    if (gl46Globals.pDev) {
        available = IDirect3DDevice9_GetAvailableTextureMem(gl46Globals.pDev);
        if (available)
            gl46Globals.availableTextureMem = available;
    }

    if (!gl46Globals.availableTextureMem) {
        /* D3D9 exposes this as a 32-bit byte count.  Use its maximum useful
         * estimate when a wrapper does not implement the optional query. */
        gl46Globals.availableTextureMem = 0xFFF00000u;
    }

    return gl46Globals.availableTextureMem / 1024u;
}

// ***********************************************************************
// Get the D3D9 interface pointer for use by other GL46 modules.
// ***********************************************************************

IDirect3D9* gldGetD3D46(void)
{
    return gl46Globals.pD3D;
}

// ***********************************************************************
// Check if NVIDIA DXVK Remix d3d9.dll was detected.
// ***********************************************************************

BOOL gldIsRemixDetected(void)
{
    return gl46Globals.bRemixDetected;
}

// ***********************************************************************
// Ask the device whether a texture of this D3DFORMAT can actually be
// created, and remember the answer.
//
// The answer comes from asking whatever d3d9.dll the game directory
// supplied what it can create — never from identifying *which*
// implementation that is.  Correct behaviour against stock Microsoft D3D9
// is the baseline; compatibility with translation layers falls out of
// asking rather than assuming.
//
// Each distinct format is probed once and the answer reused for the rest
// of the process, matching the cache-once shape of bRemixDetected and
// availableTextureMem above.  The cache survives gldDeleteContext46 for
// the same reason pD3D does: it describes the adapter, not the device.
//
// Callers use this so CreateTexture is never handed a format it cannot
// make: one failed CreateTexture can leave some d3d9.dll implementations
// in a state where every later CreateTexture fails too, which no reactive
// fallback can recover from.
//
// The probe calls into that d3d9.dll, so it is wrapped the same way
// gl_impl.c wraps its texture calls; a structured exception is answered as
// "unsupported", failing toward the safe answer.
//
// Note the cache is keyed by format and cube-or-not only, not by Usage.
// That is correct while every caller passes Usage = 0, which is what is
// queried here.
// ***********************************************************************

BOOL gldIsTextureFormatSupported46(D3DFORMAT fmt, BOOL cubeMap)
{
    GLD_gl46_fmt_entry  *pEntry = NULL;
    unsigned char       *pState = NULL;
    D3DDISPLAYMODE      d3ddm;
    HRESULT             hr = E_FAIL;
    UINT                i;

    // Nothing to ask.  Defensive: callers reach here only with a live
    // device, which implies a live IDirect3D9.
    if (!gl46Globals.pD3D)
        return FALSE;

    // fmtCacheCount is the loop bound below, and it sits in memory immediately
    // after fmtCache, so anything that writes one entry past the end of the
    // array lands on the count.  A corrupted count turns the scan into an
    // unbounded read that walks off the end of .data and faults - observed as
    // a crash at the compare instruction of this very loop, with the array
    // base and a huge index, which says nothing about who did the writing.
    //
    // Refusing to trust it costs one comparison and converts that crash into a
    // report naming the bad value, which is evidence; the cache is rebuilt from
    // empty because its contents are no more trustworthy than its length.
    if (gl46Globals.guardLo != GLD_FMTCACHE_GUARD ||
        gl46Globals.guardHi != GLD_FMTCACHE_GUARD) {
        static BOOL warned = FALSE;
        if (!warned) {
            warned = TRUE;
            gldDiagLog("GL46: format cache guard band broken - lo=0x%08X hi=0x%08X "
                       "(both should be 0x%08X), count=%u. lo intact means a forward "
                       "overrun of fmtCache; neither broken means a wild write hit the "
                       "count on its own.",
                       gl46Globals.guardLo, gl46Globals.guardHi,
                       GLD_FMTCACHE_GUARD, gl46Globals.fmtCacheCount);
        }
        gl46Globals.guardLo = GLD_FMTCACHE_GUARD;
        gl46Globals.guardHi = GLD_FMTCACHE_GUARD;
    }

    if (gl46Globals.fmtCacheCount > GLD_FMTCACHE_ENTRIES) {
        gldLogPrintf(GLDLOG_ERROR,
            "GL46: format cache count is %u, above the %d the table holds - "
            "something wrote past the end of fmtCache. Discarding the cache.",
            gl46Globals.fmtCacheCount, GLD_FMTCACHE_ENTRIES);
        gldDiagLog("GL46: format cache count corrupted (%u > %d) - cache discarded. "
                   "A write one entry past fmtCache lands on this count.",
                   gl46Globals.fmtCacheCount, GLD_FMTCACHE_ENTRIES);
        ZeroMemory(gl46Globals.fmtCache, sizeof(gl46Globals.fmtCache));
        gl46Globals.fmtCacheCount = 0;
    }

    for (i = 0; i < gl46Globals.fmtCacheCount; i++) {
        if (gl46Globals.fmtCache[i].fmt == fmt) {
            pEntry = &gl46Globals.fmtCache[i];
            break;
        }
    }

    if (!pEntry) {
        if (gl46Globals.fmtCacheCount >= GLD_FMTCACHE_ENTRIES) {
            // Answer this one call without caching rather than run off the
            // end of the table.
            gldLogPrintf(GLDLOG_WARN,
                "GL46: format capability cache full - D3DFMT=%d answered uncached",
                (int)fmt);
        } else {
            pEntry = &gl46Globals.fmtCache[gl46Globals.fmtCacheCount++];
            pEntry->fmt       = fmt;
            pEntry->texState  = GLD_FMTCACHE_UNKNOWN;
            pEntry->cubeState = GLD_FMTCACHE_UNKNOWN;
        }
    }

    if (pEntry) {
        pState = cubeMap ? &pEntry->cubeState : &pEntry->texState;
        if (*pState != GLD_FMTCACHE_UNKNOWN)
            return (*pState == GLD_FMTCACHE_SUPPORTED);
    }

    __try {
        hr = IDirect3D9_GetAdapterDisplayMode(gl46Globals.pD3D, glb.dwAdapter, &d3ddm);
        if (SUCCEEDED(hr)) {
            hr = IDirect3D9_CheckDeviceFormat(gl46Globals.pD3D,
                glb.dwAdapter,
                D3DDEVTYPE_HAL,
                d3ddm.Format,
                0,
                cubeMap ? D3DRTYPE_CUBETEXTURE : D3DRTYPE_TEXTURE,
                fmt);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hr = E_FAIL;
    }

    if (pState)
        *pState = SUCCEEDED(hr) ? GLD_FMTCACHE_SUPPORTED : GLD_FMTCACHE_UNSUPPORTED;

    gldLogPrintf(GLDLOG_INFO, "GL46: CheckDeviceFormat D3DFMT=%d %s = %s (hr=0x%08X)",
        (int)fmt,
        cubeMap ? "cube" : "2d",
        SUCCEEDED(hr) ? "supported" : "unsupported",
        hr);

    return SUCCEEDED(hr);
}
