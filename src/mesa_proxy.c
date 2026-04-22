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

    if (g_mesaProxy.initialized)
        return TRUE;

    gldDiagLog("mesaProxyInit: starting");

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

    gldDiagLog("mesaProxyInit: loading %s", dllPath);

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

    /* No temp context needed. Mesa 26 llvmpipe exports all GL functions
     * directly from the DLL — GetProcAddress(hMesaDLL, "glFuncName") works
     * without a current context. mesaProxyGetProcAddress uses this as its
     * primary lookup path. The previous run confirmed all 700+ functions
     * resolve to "-> Mesa" without any context.
     *
     * Temp context creation crashes because Mesa's wglSetPixelFormat
     * internally calls Windows SetPixelFormat which routes back to our
     * DLL export, causing infinite recursion. */

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
extern void APIENTRY glNamedRenderbufferStorageMultisampleCoverageEXT(unsigned int, int, int, unsigned int, int, int);
extern void APIENTRY glNamedProgramLocalParameterI4iEXT(unsigned int, unsigned int, unsigned int, int, int, int, int);
extern void APIENTRY glNamedProgramLocalParameterI4ivEXT(unsigned int, unsigned int, unsigned int, const int*);
extern void APIENTRY glNamedProgramLocalParametersI4ivEXT(unsigned int, unsigned int, unsigned int, int, const int*);
extern void APIENTRY glNamedProgramLocalParameterI4uiEXT(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
extern void APIENTRY glNamedProgramLocalParameterI4uivEXT(unsigned int, unsigned int, unsigned int, const unsigned int*);
extern void APIENTRY glNamedProgramLocalParametersI4uivEXT(unsigned int, unsigned int, unsigned int, int, const unsigned int*);
extern void APIENTRY glGetNamedProgramLocalParameterIivEXT(unsigned int, unsigned int, unsigned int, int*);
extern void APIENTRY glGetNamedProgramLocalParameterIuivEXT(unsigned int, unsigned int, unsigned int, unsigned int*);
extern void APIENTRY glTextureRenderbufferEXT(unsigned int, unsigned int, unsigned int);
extern void APIENTRY glMultiTexRenderbufferEXT(unsigned int, unsigned int, unsigned int);
extern void APIENTRY glGetMultiQueryObjectuivAMD(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int*);
extern void APIENTRY glVertexWeighthNV(unsigned short);
extern void APIENTRY glVertexWeighthvNV(const unsigned short*);

typedef struct { const char *name; PROC proc; } _MesaExtEntry;

static const _MesaExtEntry _mesaExtTable[] = {
    { "glNamedFramebufferTextureEXT",                     (PROC)glNamedFramebufferTextureEXT },
    { "glNamedFramebufferTextureLayerEXT",                (PROC)glNamedFramebufferTextureLayerEXT },
    { "glNamedFramebufferTextureFaceEXT",                 (PROC)glNamedFramebufferTextureFaceEXT },
    { "glNamedRenderbufferStorageMultisampleCoverageEXT", (PROC)glNamedRenderbufferStorageMultisampleCoverageEXT },
    { "glNamedProgramLocalParameterI4iEXT",               (PROC)glNamedProgramLocalParameterI4iEXT },
    { "glNamedProgramLocalParameterI4ivEXT",              (PROC)glNamedProgramLocalParameterI4ivEXT },
    { "glNamedProgramLocalParametersI4ivEXT",             (PROC)glNamedProgramLocalParametersI4ivEXT },
    { "glNamedProgramLocalParameterI4uiEXT",              (PROC)glNamedProgramLocalParameterI4uiEXT },
    { "glNamedProgramLocalParameterI4uivEXT",             (PROC)glNamedProgramLocalParameterI4uivEXT },
    { "glNamedProgramLocalParametersI4uivEXT",            (PROC)glNamedProgramLocalParametersI4uivEXT },
    { "glGetNamedProgramLocalParameterIivEXT",            (PROC)glGetNamedProgramLocalParameterIivEXT },
    { "glGetNamedProgramLocalParameterIuivEXT",           (PROC)glGetNamedProgramLocalParameterIuivEXT },
    { "glTextureRenderbufferEXT",                         (PROC)glTextureRenderbufferEXT },
    { "glMultiTexRenderbufferEXT",                        (PROC)glMultiTexRenderbufferEXT },
    { "glGetMultiQueryObjectuivAMD",                      (PROC)glGetMultiQueryObjectuivAMD },
    { "glVertexWeighthNV",                                (PROC)glVertexWeighthNV },
    { "glVertexWeighthvNV",                               (PROC)glVertexWeighthvNV },
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
        if (p) return p;
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

    /* Finally try our own DLL — gl_mesa_forward.c exports all GL functions
     * which forward to Mesa. This catches core GL 1.3+ functions like
     * glMultiTexCoord2f that Mesa doesn't export via wglGetProcAddress. */
    {
        static HMODULE hSelf = NULL;
        if (!hSelf) hSelf = GetModuleHandleA("opengl32.dll");
        if (hSelf) {
            p = GetProcAddress(hSelf, name);
            if (p) return p;
        }
    }

    return NULL;
}
