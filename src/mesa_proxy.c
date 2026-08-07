/*********************************************************************************
*  mesa_proxy.c — Load Mesa's opengl32.dll and forward GL calls to it
*********************************************************************************/

#include "mesa_proxy.h"
#include "gld_log.h"
#include "gld_diag.h"

MesaProxy g_mesaProxy = {0};

BOOL mesaProxyInit(void)
{
    char dllPath[MAX_PATH];
    char modulePath[MAX_PATH];
    char *lastSlash;
    HMODULE selfModule = NULL;
    DWORD moduleLength;

    if (g_mesaProxy.initialized)
        return TRUE;

    gldDiagLog("mesaProxyInit: starting");

    /* Locate Mesa beside this wrapper DLL, not beside the executable. */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)(const void *)&mesaProxyInit, &selfModule)) {
        selfModule = NULL;
    }
    moduleLength = GetModuleFileNameA(selfModule, modulePath, MAX_PATH);
    if (moduleLength == 0 || moduleLength >= MAX_PATH) {
        gldDiagLog("mesaProxyInit: cannot resolve wrapper directory, error=%lu",
                   (unsigned long)GetLastError());
        return FALSE;
    }
    lastSlash = strrchr(modulePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strcpy(dllPath, modulePath);
        strcat(dllPath, "mesa_gl.dll");
    } else {
        strcpy(dllPath, "mesa_gl.dll");
    }

    gldDiagLog("mesaProxyInit: loading %s", dllPath);

    /* This Mesa build exposes the complete 4.6 core/GLSL 4.60 path when the
     * version override is requested (its legacy compatibility context remains
     * capped at 4.5). Respect explicit application/user values. */
    {
        char value[32];
        SetLastError(ERROR_SUCCESS);
        if (GetEnvironmentVariableA("MESA_GL_VERSION_OVERRIDE", value,
                                    sizeof(value)) == 0 &&
            GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            SetEnvironmentVariableA("MESA_GL_VERSION_OVERRIDE", "4.6");
        }
        SetLastError(ERROR_SUCCESS);
        if (GetEnvironmentVariableA("MESA_GLSL_VERSION_OVERRIDE", value,
                                    sizeof(value)) == 0 &&
            GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            SetEnvironmentVariableA("MESA_GLSL_VERSION_OVERRIDE", "460");
        }
    }

    /* Load Mesa's DLL */
    g_mesaProxy.hMesaDLL = LoadLibraryA(dllPath);
    if (!g_mesaProxy.hMesaDLL) {
        gldDiagLog("mesaProxyInit: LoadLibrary failed for %s, error=%d", dllPath, (int)GetLastError());
        /* Try current directory */
        g_mesaProxy.hMesaDLL = LoadLibraryA("mesa_gl.dll");
    }

    if (!g_mesaProxy.hMesaDLL) {
        gldDiagLog("mesaProxyInit: mesa_gl.dll not found anywhere");
        gldLogPrintf(GLDLOG_WARN, "MesaProxy: Could not load mesa_gl.dll from %s", dllPath);
        return FALSE;
    }

    gldDiagLog("mesaProxyInit: mesa_gl.dll loaded at %p", (void*)g_mesaProxy.hMesaDLL);

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
    gldDiagLog("mesaProxyInit: WGL functions loaded, initialized=TRUE");

    /* No temp context is created here, because Mesa's wglSetPixelFormat calls
     * Windows SetPixelFormat internally, which routes back to our own export
     * of that name and recurses until the stack is gone.
     *
     * That has a consequence worth stating plainly, because an earlier comment
     * here claimed the opposite. Mesa's opengl32.dll exports only the legacy
     * GL 1.1 entry points - the same set Microsoft's opengl32.dll exports.
     * Measured against this build (Mesa 26.0.5 llvmpipe), a GetProcAddress on
     * the module resolved 0 of 16 sampled functions spanning GL 1.2 through
     * 4.6; all 16 resolved through wglGetProcAddress, and only once a Mesa
     * context was current. So mesaProxyGetProcAddress must try
     * wglGetProcAddress first, as it does, and callers must accept that
     * anything above GL 1.1 returns NULL until a Mesa context has been made
     * current. Resolving a function pointer early and caching it is therefore
     * unsafe for anything above 1.1.
     *
     * Note also that this Mesa reports "4.5 (Compatibility Profile)", not 4.6. */

    gldDiagLog("mesaProxyInit: complete");
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

/*---------------------------------------------------------------------------
 * EXT_direct_state_access and vendor extension functions implemented in
 * gl_ext_dsa.c. Mesa 26 doesn't provide these, so we supply our own
 * implementations that forward to the equivalent non-DSA Mesa functions.
 *---------------------------------------------------------------------------*/

/* Declarations from gl_ext_dsa.c */
extern void APIENTRY glNamedFramebufferTextureEXT(unsigned int, unsigned int, unsigned int, int);
extern void APIENTRY glNamedFramebufferTextureLayerEXT(unsigned int, unsigned int, unsigned int, int, int);
extern void APIENTRY glNamedFramebufferTextureFaceEXT(unsigned int, unsigned int, unsigned int, int, unsigned int);
extern void APIENTRY glGetMultiQueryObjectuivAMD(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int*);

typedef struct { const char *name; PROC proc; } _MesaExtEntry;

static const _MesaExtEntry _mesaExtTable[] = {
    { "glNamedFramebufferTextureEXT",                     (PROC)glNamedFramebufferTextureEXT },
    { "glNamedFramebufferTextureLayerEXT",                (PROC)glNamedFramebufferTextureLayerEXT },
    { "glNamedFramebufferTextureFaceEXT",                 (PROC)glNamedFramebufferTextureFaceEXT },
    { "glGetMultiQueryObjectuivAMD",                      (PROC)glGetMultiQueryObjectuivAMD },
    { NULL, NULL }
};

PROC mesaProxyGetProcAddress(LPCSTR name)
{
    PROC p;
    int i;

    if (!name)
        return NULL;

    if (!g_mesaProxy.initialized)
        return NULL;

    /* First try Mesa's wglGetProcAddress (for GL extensions, needs current context) */
    if (g_mesaProxy.wglGetProcAddress) {
        p = g_mesaProxy.wglGetProcAddress(name);
        /* The Win32 WGL contract permits the four small integer sentinels and
         * -1 for core names.  They are failure values, not callable entry
         * points; returning -1 here previously produced an access violation
         * as soon as a forwarded core call was made while Mesa was current. */
        if (p && p != (PROC)1 && p != (PROC)2 && p != (PROC)3 && p != (PROC)-1)
            return p;
    }

    /* Then try GetProcAddress on Mesa DLL (for core GL functions) */
    p = GetProcAddress(g_mesaProxy.hMesaDLL, name);
    if (p) return p;

    /* Check our EXT_direct_state_access / vendor extension table.
     * These are functions Mesa doesn't implement but games need. */
    for (i = 0; _mesaExtTable[i].name != NULL; i++) {
        if (strcmp(name, _mesaExtTable[i].name) == 0)
            return _mesaExtTable[i].proc;
    }

    /* Do not resolve back to this wrapper's forwarding thunk: that turns a
     * genuinely missing Mesa symbol into unbounded recursion. */
    return NULL;
}
