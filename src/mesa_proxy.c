/*********************************************************************************
*  mesa_proxy.c — Load Mesa's opengl32.dll and forward GL calls to it
*********************************************************************************/

#include "mesa_proxy.h"
#include "gld_log.h"

MesaProxy g_mesaProxy = {0};

BOOL mesaProxyInit(void)
{
    char dllPath[MAX_PATH];
    char modulePath[MAX_PATH];
    char *lastSlash;

    if (g_mesaProxy.initialized)
        return TRUE;

    /* Build path to mesa_gl.dll in the same directory as our DLL */
    GetModuleFileName(NULL, modulePath, MAX_PATH);
    lastSlash = strrchr(modulePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strcpy(dllPath, modulePath);
        strcat(dllPath, "mesa_gl.dll");
    } else {
        strcpy(dllPath, "mesa_gl.dll");
    }

    /* Load Mesa's DLL */
    g_mesaProxy.hMesaDLL = LoadLibraryA(dllPath);
    if (!g_mesaProxy.hMesaDLL) {
        /* Try current directory */
        g_mesaProxy.hMesaDLL = LoadLibraryA("mesa_gl.dll");
    }

    if (!g_mesaProxy.hMesaDLL) {
        gldLogPrintf(GLDLOG_WARN, "MesaProxy: Could not load mesa_gl.dll from %s", dllPath);
        return FALSE;
    }

    /* Load WGL functions */
    g_mesaProxy.wglCreateContext     = (PFN_wglCreateContext)GetProcAddress(g_mesaProxy.hMesaDLL, "wglCreateContext");
    g_mesaProxy.wglDeleteContext     = (PFN_wglDeleteContext)GetProcAddress(g_mesaProxy.hMesaDLL, "wglDeleteContext");
    g_mesaProxy.wglMakeCurrent       = (PFN_wglMakeCurrent)GetProcAddress(g_mesaProxy.hMesaDLL, "wglMakeCurrent");
    g_mesaProxy.wglGetProcAddress    = (PFN_wglGetProcAddress)GetProcAddress(g_mesaProxy.hMesaDLL, "wglGetProcAddress");
    g_mesaProxy.wglGetCurrentContext = (PFN_wglGetCurrentContext)GetProcAddress(g_mesaProxy.hMesaDLL, "wglGetCurrentContext");
    g_mesaProxy.wglGetCurrentDC      = (PFN_wglGetCurrentDC)GetProcAddress(g_mesaProxy.hMesaDLL, "wglGetCurrentDC");
    g_mesaProxy.wglShareLists        = (PFN_wglShareLists)GetProcAddress(g_mesaProxy.hMesaDLL, "wglShareLists");
    g_mesaProxy.wglChoosePixelFormat = (PFN_wglChoosePixelFormat)GetProcAddress(g_mesaProxy.hMesaDLL, "wglChoosePixelFormat");
    g_mesaProxy.wglDescribePixelFormat = (PFN_wglDescribePixelFormat)GetProcAddress(g_mesaProxy.hMesaDLL, "wglDescribePixelFormat");
    g_mesaProxy.wglGetPixelFormat    = (PFN_wglGetPixelFormat)GetProcAddress(g_mesaProxy.hMesaDLL, "wglGetPixelFormat");
    g_mesaProxy.wglSetPixelFormat    = (PFN_wglSetPixelFormat)GetProcAddress(g_mesaProxy.hMesaDLL, "wglSetPixelFormat");
    g_mesaProxy.wglSwapBuffers       = (PFN_wglSwapBuffers)GetProcAddress(g_mesaProxy.hMesaDLL, "wglSwapBuffers");

    if (!g_mesaProxy.wglCreateContext || !g_mesaProxy.wglMakeCurrent ||
        !g_mesaProxy.wglGetProcAddress) {
        gldLogMessage(GLDLOG_ERROR, "MesaProxy: Failed to load critical WGL functions from mesa_gl.dll\n");
        FreeLibrary(g_mesaProxy.hMesaDLL);
        g_mesaProxy.hMesaDLL = NULL;
        return FALSE;
    }

    g_mesaProxy.initialized = TRUE;
    gldLogPrintf(GLDLOG_SYSTEM, "MesaProxy: Loaded mesa_gl.dll successfully (GL 4.6 llvmpipe)");
    return TRUE;
}

void mesaProxyShutdown(void)
{
    if (g_mesaProxy.hMesaDLL) {
        FreeLibrary(g_mesaProxy.hMesaDLL);
        g_mesaProxy.hMesaDLL = NULL;
    }
    g_mesaProxy.initialized = FALSE;
}

PROC mesaProxyGetProcAddress(LPCSTR name)
{
    PROC p;

    if (!g_mesaProxy.initialized)
        return NULL;

    /* First try wglGetProcAddress (for GL extensions) */
    if (g_mesaProxy.wglGetProcAddress) {
        p = g_mesaProxy.wglGetProcAddress(name);
        if (p) return p;
    }

    /* Then try GetProcAddress on the DLL (for core GL functions) */
    p = GetProcAddress(g_mesaProxy.hMesaDLL, name);
    return p;
}
