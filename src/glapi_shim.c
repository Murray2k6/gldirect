/*********************************************************************************
*  glapi_shim.c — Minimal GL API dispatch shim
*
*  Provides the global context/dispatch variables that Mesa 5.1 core and the
*  DX9 backend need, without the full glapi.c dispatch table (which references
*  hundreds of unresolved extension functions).
*
*  The real GL dispatch goes through gl_mesa_forward.c → mesa_gl.dll (Mesa 26).
*********************************************************************************/

#include <windows.h>
#include <stdlib.h>

/* These are the globals that Mesa 5.1 core and the DX9 backend reference */
void *_glapi_Context = NULL;
void *_glapi_Dispatch = NULL;
void *_glapi_RealDispatch = NULL;

void _glapi_set_context(void *context)
{
    _glapi_Context = context;
}

void *_glapi_get_context(void)
{
    return _glapi_Context;
}

void _glapi_set_dispatch(void *table)
{
    _glapi_Dispatch = table;
}

void *_glapi_get_dispatch(void)
{
    return _glapi_Dispatch;
}

void _glapi_check_multithread(void)
{
    /* No-op — single-threaded for now */
}

void _glapi_noop_enable_warnings(unsigned char enable)
{
    (void)enable;
}

void _glapi_set_warning_func(void *func)
{
    (void)func;
}

/* _glapi_get_proc_offset — returns -1 for unknown */
int _glapi_get_proc_offset(const char *funcName)
{
    (void)funcName;
    return -1;
}

/* _glapi_get_proc_address — returns NULL, Mesa 26 handles this */
void *_glapi_get_proc_address(const char *funcName)
{
    (void)funcName;
    return NULL;
}

/* _glapi_get_dispatch_table_size */
unsigned int _glapi_get_dispatch_table_size(void)
{
    return 1024; /* Large enough for Mesa 5.1's dispatch table */
}

/* Thread-local storage stubs */
typedef unsigned int _glthread_TSD;

void _glthread_SetTSD(_glthread_TSD *tsd, void *ptr)
{
    (void)tsd;
    (void)ptr;
}

void *_glthread_GetTSD(_glthread_TSD *tsd)
{
    (void)tsd;
    return NULL;
}

void _glthread_InitTSD(_glthread_TSD *tsd)
{
    (void)tsd;
}


/* _glapi_add_entrypoint — register extension function entry point */
int _glapi_add_entrypoint(const char *funcName, unsigned int offset)
{
    (void)funcName;
    (void)offset;
    return 1; /* Success */
}

/* GL functions called directly by swrast imaging module.
 * Forward to Mesa proxy. */
#include "mesa_proxy.h"

void APIENTRY glColorTable(unsigned int target, unsigned int internalformat,
    int width, unsigned int format, unsigned int type, const void *table)
{
    typedef void (APIENTRY *PFN)(unsigned int, unsigned int, int, unsigned int, unsigned int, const void*);
    static PFN fn = NULL;
    if (!fn) fn = (PFN)mesaProxyGetProcAddress("glColorTable");
    if (fn) fn(target, internalformat, width, format, type, table);
}

void APIENTRY glColorSubTable(unsigned int target, int start, int count,
    unsigned int format, unsigned int type, const void *data)
{
    typedef void (APIENTRY *PFN)(unsigned int, int, int, unsigned int, unsigned int, const void*);
    static PFN fn = NULL;
    if (!fn) fn = (PFN)mesaProxyGetProcAddress("glColorSubTable");
    if (fn) fn(target, start, count, format, type, data);
}

void APIENTRY glConvolutionFilter1D(unsigned int target, unsigned int internalformat,
    int width, unsigned int format, unsigned int type, const void *image)
{
    typedef void (APIENTRY *PFN)(unsigned int, unsigned int, int, unsigned int, unsigned int, const void*);
    static PFN fn = NULL;
    if (!fn) fn = (PFN)mesaProxyGetProcAddress("glConvolutionFilter1D");
    if (fn) fn(target, internalformat, width, format, type, image);
}

void APIENTRY glConvolutionFilter2D(unsigned int target, unsigned int internalformat,
    int width, int height, unsigned int format, unsigned int type, const void *image)
{
    typedef void (APIENTRY *PFN)(unsigned int, unsigned int, int, int, unsigned int, unsigned int, const void*);
    static PFN fn = NULL;
    if (!fn) fn = (PFN)mesaProxyGetProcAddress("glConvolutionFilter2D");
    if (fn) fn(target, internalformat, width, height, format, type, image);
}
