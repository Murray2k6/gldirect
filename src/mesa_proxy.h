/*********************************************************************************
*  mesa_proxy.h — Load and forward calls to Mesa's real opengl32.dll (mesa_gl.dll)
*
*  Our opengl32.dll is a wrapper that:
*  1. Loads mesa_gl.dll (Mesa llvmpipe GL 4.6) from the same directory
*  2. Forwards all GL and WGL calls to Mesa
*  3. Intercepts SwapBuffers to copy Mesa's framebuffer to D3D9
*
*  This gives us complete GL 1.0-4.6 support with D3D9 presentation.
*********************************************************************************/

#ifndef MESA_PROXY_H
#define MESA_PROXY_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the Mesa proxy — load mesa_gl.dll */
BOOL mesaProxyInit(void);

/* Shut down the Mesa proxy */
void mesaProxyShutdown(void);

/* Get a GL function from Mesa */
PROC mesaProxyGetProcAddress(LPCSTR name);

/* Get Mesa's WGL functions */
typedef HGLRC (WINAPI *PFN_wglCreateContext)(HDC);
typedef BOOL  (WINAPI *PFN_wglDeleteContext)(HGLRC);
typedef BOOL  (WINAPI *PFN_wglMakeCurrent)(HDC, HGLRC);
typedef PROC  (WINAPI *PFN_wglGetProcAddress)(LPCSTR);
typedef HGLRC (WINAPI *PFN_wglGetCurrentContext)(void);
typedef HDC   (WINAPI *PFN_wglGetCurrentDC)(void);
typedef BOOL  (WINAPI *PFN_wglShareLists)(HGLRC, HGLRC);
typedef int   (WINAPI *PFN_wglChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR*);
typedef int   (WINAPI *PFN_wglDescribePixelFormat)(HDC, int, UINT, PIXELFORMATDESCRIPTOR*);
typedef int   (WINAPI *PFN_wglGetPixelFormat)(HDC);
typedef BOOL  (WINAPI *PFN_wglSetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR*);
typedef BOOL  (WINAPI *PFN_wglSwapBuffers)(HDC);

typedef struct {
    HMODULE                  hMesaDLL;
    PFN_wglCreateContext     wglCreateContext;
    PFN_wglDeleteContext     wglDeleteContext;
    PFN_wglMakeCurrent       wglMakeCurrent;
    PFN_wglGetProcAddress    wglGetProcAddress;
    PFN_wglGetCurrentContext wglGetCurrentContext;
    PFN_wglGetCurrentDC      wglGetCurrentDC;
    PFN_wglShareLists        wglShareLists;
    PFN_wglChoosePixelFormat wglChoosePixelFormat;
    PFN_wglDescribePixelFormat wglDescribePixelFormat;
    PFN_wglGetPixelFormat    wglGetPixelFormat;
    PFN_wglSetPixelFormat    wglSetPixelFormat;
    PFN_wglSwapBuffers       wglSwapBuffers;
    BOOL                     initialized;
} MesaProxy;

/* Global Mesa proxy instance */
extern MesaProxy g_mesaProxy;

#ifdef __cplusplus
}
#endif

#endif /* MESA_PROXY_H */
