/*********************************************************************************
*  gl_mesa_forward.c — Forward all GL function exports to Mesa's mesa_gl.dll
*
*  This file replaces gl_legacy_stubs.c. Instead of stub implementations,
*  every GL function loads its real implementation from mesa_gl.dll at runtime.
*  Mesa 26 provides complete GL 1.0-4.6 support via llvmpipe.
*
*  The .def file exports these function names. Each one forwards to Mesa.
*********************************************************************************/

#include <windows.h>
#include "mesa_proxy.h"

/* Helper: get a GL function pointer from Mesa, cache it */
#define MESA_FORWARD_VOID(name, params, args) \
    typedef void (APIENTRY *PFN_##name) params; \
    static PFN_##name pfn_##name = NULL; \
    void APIENTRY name params { \
        if (!pfn_##name) { \
            if (!g_mesaProxy.initialized) mesaProxyInit(); \
            pfn_##name = (PFN_##name)mesaProxyGetProcAddress(#name); \
            if (!pfn_##name) pfn_##name = (PFN_##name)GetProcAddress(g_mesaProxy.hMesaDLL, #name); \
        } \
        if (pfn_##name) pfn_##name args; \
    }

#define MESA_FORWARD_RET(rettype, name, params, args) \
    typedef rettype (APIENTRY *PFN_##name) params; \
    static PFN_##name pfn_##name = NULL; \
    rettype APIENTRY name params { \
        if (!pfn_##name) { \
            if (!g_mesaProxy.initialized) mesaProxyInit(); \
            pfn_##name = (PFN_##name)mesaProxyGetProcAddress(#name); \
            if (!pfn_##name) pfn_##name = (PFN_##name)GetProcAddress(g_mesaProxy.hMesaDLL, #name); \
        } \
        if (pfn_##name) return pfn_##name args; \
        return (rettype)0; \
    }

/* Suppress warnings */
#pragma warning(disable: 4100 4152)

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

#define GL_TRUE 1
#define GL_FALSE 0

/*---------------------------------------------------------------------------
 * GL 1.0/1.1 Core Functions — forwarded to Mesa
 *---------------------------------------------------------------------------*/

MESA_FORWARD_VOID(glAccum, (GLenum op, GLfloat value), (op, value))
MESA_FORWARD_VOID(glAlphaFunc, (GLenum func, GLfloat ref), (func, ref))
MESA_FORWARD_VOID(glBegin, (GLenum mode), (mode))
MESA_FORWARD_VOID(glEnd, (void), ())
MESA_FORWARD_VOID(glBitmap, (GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym, const GLubyte *bm), (w,h,xo,yo,xm,ym,bm))
MESA_FORWARD_VOID(glBlendFunc, (GLenum s, GLenum d), (s,d))
MESA_FORWARD_VOID(glCallList, (GLuint list), (list))
MESA_FORWARD_VOID(glCallLists, (GLsizei n, GLenum type, const GLvoid *lists), (n,type,lists))
MESA_FORWARD_VOID(glClear, (GLbitfield mask), (mask))
MESA_FORWARD_VOID(glClearAccum, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r,g,b,a))
MESA_FORWARD_VOID(glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r,g,b,a))
MESA_FORWARD_VOID(glClearDepth, (GLdouble d), (d))
MESA_FORWARD_VOID(glClearIndex, (GLfloat c), (c))
MESA_FORWARD_VOID(glClearStencil, (GLint s), (s))
MESA_FORWARD_VOID(glClipPlane, (GLenum p, const GLdouble *eq), (p,eq))

MESA_FORWARD_VOID(glColor3b, (GLbyte r, GLbyte g, GLbyte b), (r,g,b))
MESA_FORWARD_VOID(glColor3d, (GLdouble r, GLdouble g, GLdouble b), (r,g,b))
MESA_FORWARD_VOID(glColor3f, (GLfloat r, GLfloat g, GLfloat b), (r,g,b))
MESA_FORWARD_VOID(glColor3i, (GLint r, GLint g, GLint b), (r,g,b))
MESA_FORWARD_VOID(glColor3s, (GLshort r, GLshort g, GLshort b), (r,g,b))
MESA_FORWARD_VOID(glColor3ub, (GLubyte r, GLubyte g, GLubyte b), (r,g,b))
MESA_FORWARD_VOID(glColor3ui, (GLuint r, GLuint g, GLuint b), (r,g,b))
MESA_FORWARD_VOID(glColor3us, (GLushort r, GLushort g, GLushort b), (r,g,b))
MESA_FORWARD_VOID(glColor4b, (GLbyte r, GLbyte g, GLbyte b, GLbyte a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4d, (GLdouble r, GLdouble g, GLdouble b, GLdouble a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4i, (GLint r, GLint g, GLint b, GLint a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4s, (GLshort r, GLshort g, GLshort b, GLshort a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4ub, (GLubyte r, GLubyte g, GLubyte b, GLubyte a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4ui, (GLuint r, GLuint g, GLuint b, GLuint a), (r,g,b,a))
MESA_FORWARD_VOID(glColor4us, (GLushort r, GLushort g, GLushort b, GLushort a), (r,g,b,a))
MESA_FORWARD_VOID(glColor3bv, (const GLbyte *v), (v))
MESA_FORWARD_VOID(glColor3dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glColor3fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glColor3iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glColor3sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glColor3ubv, (const GLubyte *v), (v))
MESA_FORWARD_VOID(glColor3uiv, (const GLuint *v), (v))
MESA_FORWARD_VOID(glColor3usv, (const GLushort *v), (v))
MESA_FORWARD_VOID(glColor4bv, (const GLbyte *v), (v))
MESA_FORWARD_VOID(glColor4dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glColor4fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glColor4iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glColor4sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glColor4ubv, (const GLubyte *v), (v))
MESA_FORWARD_VOID(glColor4uiv, (const GLuint *v), (v))
MESA_FORWARD_VOID(glColor4usv, (const GLushort *v), (v))
MESA_FORWARD_VOID(glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a), (r,g,b,a))
MESA_FORWARD_VOID(glColorMaterial, (GLenum face, GLenum mode), (face,mode))
MESA_FORWARD_VOID(glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *p), (size,type,stride,p))
MESA_FORWARD_VOID(glCopyPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum type), (x,y,w,h,type))
MESA_FORWARD_VOID(glCopyTexImage1D, (GLenum t, GLint l, GLenum f, GLint x, GLint y, GLsizei w, GLint b), (t,l,f,x,y,w,b))
MESA_FORWARD_VOID(glCopyTexImage2D, (GLenum t, GLint l, GLenum f, GLint x, GLint y, GLsizei w, GLsizei h, GLint b), (t,l,f,x,y,w,h,b))
MESA_FORWARD_VOID(glCopyTexSubImage1D, (GLenum t, GLint l, GLint xo, GLint x, GLint y, GLsizei w), (t,l,xo,x,y,w))
MESA_FORWARD_VOID(glCopyTexSubImage2D, (GLenum t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h), (t,l,xo,yo,x,y,w,h))
MESA_FORWARD_VOID(glCullFace, (GLenum mode), (mode))
MESA_FORWARD_VOID(glDeleteLists, (GLuint list, GLsizei range), (list,range))
MESA_FORWARD_VOID(glDeleteTextures, (GLsizei n, const GLuint *t), (n,t))
MESA_FORWARD_VOID(glDepthFunc, (GLenum func), (func))
MESA_FORWARD_VOID(glDepthMask, (GLboolean flag), (flag))
MESA_FORWARD_VOID(glDepthRange, (GLdouble n, GLdouble f), (n,f))
MESA_FORWARD_VOID(glDisable, (GLenum cap), (cap))
MESA_FORWARD_VOID(glDisableClientState, (GLenum a), (a))
MESA_FORWARD_VOID(glDrawArrays, (GLenum mode, GLint first, GLsizei count), (mode,first,count))
MESA_FORWARD_VOID(glDrawBuffer, (GLenum mode), (mode))
MESA_FORWARD_VOID(glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *idx), (mode,count,type,idx))
MESA_FORWARD_VOID(glDrawPixels, (GLsizei w, GLsizei h, GLenum fmt, GLenum type, const GLvoid *pix), (w,h,fmt,type,pix))
MESA_FORWARD_VOID(glEdgeFlag, (GLboolean flag), (flag))
MESA_FORWARD_VOID(glEdgeFlagv, (const GLboolean *flag), (flag))
MESA_FORWARD_VOID(glEdgeFlagPointer, (GLsizei stride, const GLvoid *p), (stride,p))
MESA_FORWARD_VOID(glEnable, (GLenum cap), (cap))
MESA_FORWARD_VOID(glEnableClientState, (GLenum a), (a))
MESA_FORWARD_VOID(glEvalCoord1d, (GLdouble u), (u))
MESA_FORWARD_VOID(glEvalCoord1f, (GLfloat u), (u))
MESA_FORWARD_VOID(glEvalCoord1dv, (const GLdouble *u), (u))
MESA_FORWARD_VOID(glEvalCoord1fv, (const GLfloat *u), (u))
MESA_FORWARD_VOID(glEvalCoord2d, (GLdouble u, GLdouble v), (u,v))
MESA_FORWARD_VOID(glEvalCoord2f, (GLfloat u, GLfloat v), (u,v))
MESA_FORWARD_VOID(glEvalCoord2dv, (const GLdouble *u), (u))
MESA_FORWARD_VOID(glEvalCoord2fv, (const GLfloat *u), (u))
MESA_FORWARD_VOID(glEvalMesh1, (GLenum mode, GLint i1, GLint i2), (mode,i1,i2))
MESA_FORWARD_VOID(glEvalMesh2, (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2), (mode,i1,i2,j1,j2))
MESA_FORWARD_VOID(glEvalPoint1, (GLint i), (i))
MESA_FORWARD_VOID(glEvalPoint2, (GLint i, GLint j), (i,j))
MESA_FORWARD_VOID(glFeedbackBuffer, (GLsizei size, GLenum type, GLfloat *buf), (size,type,buf))
MESA_FORWARD_VOID(glFinish, (void), ())
MESA_FORWARD_VOID(glFlush, (void), ())
MESA_FORWARD_VOID(glFogf, (GLenum pname, GLfloat param), (pname,param))
MESA_FORWARD_VOID(glFogi, (GLenum pname, GLint param), (pname,param))
MESA_FORWARD_VOID(glFogfv, (GLenum pname, const GLfloat *p), (pname,p))
MESA_FORWARD_VOID(glFogiv, (GLenum pname, const GLint *p), (pname,p))
MESA_FORWARD_VOID(glFrontFace, (GLenum mode), (mode))
MESA_FORWARD_VOID(glFrustum, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l,r,b,t,n,f))
MESA_FORWARD_VOID(glGenTextures, (GLsizei n, GLuint *t), (n,t))
MESA_FORWARD_VOID(glGetBooleanv, (GLenum pname, GLboolean *p), (pname,p))
MESA_FORWARD_VOID(glGetDoublev, (GLenum pname, GLdouble *p), (pname,p))
MESA_FORWARD_RET(GLenum, glGetError, (void), ())
MESA_FORWARD_VOID(glGetFloatv, (GLenum pname, GLfloat *p), (pname,p))
MESA_FORWARD_VOID(glGetIntegerv, (GLenum pname, GLint *p), (pname,p))
MESA_FORWARD_VOID(glGetClipPlane, (GLenum p, GLdouble *eq), (p,eq))
MESA_FORWARD_VOID(glGetLightfv, (GLenum l, GLenum p, GLfloat *v), (l,p,v))
MESA_FORWARD_VOID(glGetLightiv, (GLenum l, GLenum p, GLint *v), (l,p,v))
MESA_FORWARD_VOID(glGetMapdv, (GLenum t, GLenum q, GLdouble *v), (t,q,v))
MESA_FORWARD_VOID(glGetMapfv, (GLenum t, GLenum q, GLfloat *v), (t,q,v))
MESA_FORWARD_VOID(glGetMapiv, (GLenum t, GLenum q, GLint *v), (t,q,v))
MESA_FORWARD_VOID(glGetMaterialfv, (GLenum f, GLenum p, GLfloat *v), (f,p,v))
MESA_FORWARD_VOID(glGetMaterialiv, (GLenum f, GLenum p, GLint *v), (f,p,v))
MESA_FORWARD_VOID(glGetPixelMapfv, (GLenum m, GLfloat *v), (m,v))
MESA_FORWARD_VOID(glGetPixelMapuiv, (GLenum m, GLuint *v), (m,v))
MESA_FORWARD_VOID(glGetPixelMapusv, (GLenum m, GLushort *v), (m,v))
MESA_FORWARD_VOID(glGetPointerv, (GLenum p, GLvoid **v), (p,v))
MESA_FORWARD_VOID(glGetPolygonStipple, (GLubyte *m), (m))
MESA_FORWARD_RET(const GLubyte*, glGetString, (GLenum name), (name))
MESA_FORWARD_VOID(glGetTexEnvfv, (GLenum t, GLenum p, GLfloat *v), (t,p,v))
MESA_FORWARD_VOID(glGetTexEnviv, (GLenum t, GLenum p, GLint *v), (t,p,v))
MESA_FORWARD_VOID(glGetTexGendv, (GLenum c, GLenum p, GLdouble *v), (c,p,v))
MESA_FORWARD_VOID(glGetTexGenfv, (GLenum c, GLenum p, GLfloat *v), (c,p,v))
MESA_FORWARD_VOID(glGetTexGeniv, (GLenum c, GLenum p, GLint *v), (c,p,v))
MESA_FORWARD_VOID(glGetTexImage, (GLenum t, GLint l, GLenum f, GLenum ty, GLvoid *p), (t,l,f,ty,p))
MESA_FORWARD_VOID(glGetTexLevelParameterfv, (GLenum t, GLint l, GLenum p, GLfloat *v), (t,l,p,v))
MESA_FORWARD_VOID(glGetTexLevelParameteriv, (GLenum t, GLint l, GLenum p, GLint *v), (t,l,p,v))
MESA_FORWARD_VOID(glGetTexParameterfv, (GLenum t, GLenum p, GLfloat *v), (t,p,v))
MESA_FORWARD_VOID(glGetTexParameteriv, (GLenum t, GLenum p, GLint *v), (t,p,v))
MESA_FORWARD_VOID(glHint, (GLenum t, GLenum m), (t,m))
MESA_FORWARD_VOID(glIndexd, (GLdouble c), (c))
MESA_FORWARD_VOID(glIndexf, (GLfloat c), (c))
MESA_FORWARD_VOID(glIndexi, (GLint c), (c))
MESA_FORWARD_VOID(glIndexs, (GLshort c), (c))
MESA_FORWARD_VOID(glIndexub, (GLubyte c), (c))
MESA_FORWARD_VOID(glIndexdv, (const GLdouble *c), (c))
MESA_FORWARD_VOID(glIndexfv, (const GLfloat *c), (c))
MESA_FORWARD_VOID(glIndexiv, (const GLint *c), (c))
MESA_FORWARD_VOID(glIndexsv, (const GLshort *c), (c))
MESA_FORWARD_VOID(glIndexubv, (const GLubyte *c), (c))
MESA_FORWARD_VOID(glIndexMask, (GLuint m), (m))
MESA_FORWARD_VOID(glIndexPointer, (GLenum t, GLsizei s, const GLvoid *p), (t,s,p))
MESA_FORWARD_VOID(glInitNames, (void), ())
MESA_FORWARD_VOID(glInterleavedArrays, (GLenum f, GLsizei s, const GLvoid *p), (f,s,p))
MESA_FORWARD_RET(GLboolean, glIsEnabled, (GLenum cap), (cap))
MESA_FORWARD_RET(GLboolean, glIsList, (GLuint list), (list))
MESA_FORWARD_RET(GLboolean, glIsTexture, (GLuint t), (t))
MESA_FORWARD_VOID(glLightf, (GLenum l, GLenum p, GLfloat v), (l,p,v))
MESA_FORWARD_VOID(glLighti, (GLenum l, GLenum p, GLint v), (l,p,v))
MESA_FORWARD_VOID(glLightfv, (GLenum l, GLenum p, const GLfloat *v), (l,p,v))
MESA_FORWARD_VOID(glLightiv, (GLenum l, GLenum p, const GLint *v), (l,p,v))
MESA_FORWARD_VOID(glLightModelf, (GLenum p, GLfloat v), (p,v))
MESA_FORWARD_VOID(glLightModeli, (GLenum p, GLint v), (p,v))
MESA_FORWARD_VOID(glLightModelfv, (GLenum p, const GLfloat *v), (p,v))
MESA_FORWARD_VOID(glLightModeliv, (GLenum p, const GLint *v), (p,v))
MESA_FORWARD_VOID(glLineStipple, (GLint f, GLushort p), (f,p))
MESA_FORWARD_VOID(glLineWidth, (GLfloat w), (w))
MESA_FORWARD_VOID(glListBase, (GLuint b), (b))
MESA_FORWARD_VOID(glLoadIdentity, (void), ())
MESA_FORWARD_VOID(glLoadMatrixd, (const GLdouble *m), (m))
MESA_FORWARD_VOID(glLoadMatrixf, (const GLfloat *m), (m))
MESA_FORWARD_VOID(glLoadName, (GLuint n), (n))
MESA_FORWARD_VOID(glLogicOp, (GLenum op), (op))

/*---------------------------------------------------------------------------
 * GL 1.0/1.1 Core Functions continued — glMap* through glEnd
 *---------------------------------------------------------------------------*/

MESA_FORWARD_VOID(glMap1d, (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points), (target,u1,u2,stride,order,points))
MESA_FORWARD_VOID(glMap1f, (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points), (target,u1,u2,stride,order,points))
MESA_FORWARD_VOID(glMap2d, (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points), (target,u1,u2,ustride,uorder,v1,v2,vstride,vorder,points))
MESA_FORWARD_VOID(glMap2f, (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points), (target,u1,u2,ustride,uorder,v1,v2,vstride,vorder,points))
MESA_FORWARD_VOID(glMapGrid1d, (GLint un, GLdouble u1, GLdouble u2), (un,u1,u2))
MESA_FORWARD_VOID(glMapGrid1f, (GLint un, GLfloat u1, GLfloat u2), (un,u1,u2))
MESA_FORWARD_VOID(glMapGrid2d, (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2), (un,u1,u2,vn,v1,v2))
MESA_FORWARD_VOID(glMapGrid2f, (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2), (un,u1,u2,vn,v1,v2))
MESA_FORWARD_VOID(glMaterialf, (GLenum face, GLenum pname, GLfloat param), (face,pname,param))
MESA_FORWARD_VOID(glMateriali, (GLenum face, GLenum pname, GLint param), (face,pname,param))
MESA_FORWARD_VOID(glMaterialfv, (GLenum face, GLenum pname, const GLfloat *params), (face,pname,params))
MESA_FORWARD_VOID(glMaterialiv, (GLenum face, GLenum pname, const GLint *params), (face,pname,params))
MESA_FORWARD_VOID(glMatrixMode, (GLenum mode), (mode))
MESA_FORWARD_VOID(glMultMatrixd, (const GLdouble *m), (m))
MESA_FORWARD_VOID(glMultMatrixf, (const GLfloat *m), (m))
MESA_FORWARD_VOID(glNewList, (GLuint list, GLenum mode), (list,mode))
MESA_FORWARD_VOID(glEndList, (void), ())

/* glNormal variants */
MESA_FORWARD_VOID(glNormal3b, (GLbyte nx, GLbyte ny, GLbyte nz), (nx,ny,nz))
MESA_FORWARD_VOID(glNormal3d, (GLdouble nx, GLdouble ny, GLdouble nz), (nx,ny,nz))
MESA_FORWARD_VOID(glNormal3f, (GLfloat nx, GLfloat ny, GLfloat nz), (nx,ny,nz))
MESA_FORWARD_VOID(glNormal3i, (GLint nx, GLint ny, GLint nz), (nx,ny,nz))
MESA_FORWARD_VOID(glNormal3s, (GLshort nx, GLshort ny, GLshort nz), (nx,ny,nz))
MESA_FORWARD_VOID(glNormal3bv, (const GLbyte *v), (v))
MESA_FORWARD_VOID(glNormal3dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glNormal3fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glNormal3iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glNormal3sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *p), (type,stride,p))

MESA_FORWARD_VOID(glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l,r,b,t,n,f))
MESA_FORWARD_VOID(glPassThrough, (GLfloat token), (token))

/* glPixel* */
MESA_FORWARD_VOID(glPixelMapfv, (GLenum map, GLsizei mapsize, const GLfloat *values), (map,mapsize,values))
MESA_FORWARD_VOID(glPixelMapuiv, (GLenum map, GLsizei mapsize, const GLuint *values), (map,mapsize,values))
MESA_FORWARD_VOID(glPixelMapusv, (GLenum map, GLsizei mapsize, const GLushort *values), (map,mapsize,values))
MESA_FORWARD_VOID(glPixelStoref, (GLenum pname, GLfloat param), (pname,param))
MESA_FORWARD_VOID(glPixelStorei, (GLenum pname, GLint param), (pname,param))
MESA_FORWARD_VOID(glPixelTransferf, (GLenum pname, GLfloat param), (pname,param))
MESA_FORWARD_VOID(glPixelTransferi, (GLenum pname, GLint param), (pname,param))
MESA_FORWARD_VOID(glPixelZoom, (GLfloat xfactor, GLfloat yfactor), (xfactor,yfactor))

MESA_FORWARD_VOID(glPointSize, (GLfloat size), (size))
MESA_FORWARD_VOID(glPolygonMode, (GLenum face, GLenum mode), (face,mode))
MESA_FORWARD_VOID(glPolygonOffset, (GLfloat factor, GLfloat units), (factor,units))
MESA_FORWARD_VOID(glPolygonStipple, (const GLubyte *mask), (mask))

/* Push/Pop */
MESA_FORWARD_VOID(glPopAttrib, (void), ())
MESA_FORWARD_VOID(glPopClientAttrib, (void), ())
MESA_FORWARD_VOID(glPopMatrix, (void), ())
MESA_FORWARD_VOID(glPopName, (void), ())
MESA_FORWARD_VOID(glPushAttrib, (GLbitfield mask), (mask))
MESA_FORWARD_VOID(glPushClientAttrib, (GLbitfield mask), (mask))
MESA_FORWARD_VOID(glPushMatrix, (void), ())
MESA_FORWARD_VOID(glPushName, (GLuint name), (name))

MESA_FORWARD_VOID(glPrioritizeTextures, (GLsizei n, const GLuint *textures, const GLfloat *priorities), (n,textures,priorities))


/* glRasterPos variants */
MESA_FORWARD_VOID(glRasterPos2d, (GLdouble x, GLdouble y), (x,y))
MESA_FORWARD_VOID(glRasterPos2f, (GLfloat x, GLfloat y), (x,y))
MESA_FORWARD_VOID(glRasterPos2i, (GLint x, GLint y), (x,y))
MESA_FORWARD_VOID(glRasterPos2s, (GLshort x, GLshort y), (x,y))
MESA_FORWARD_VOID(glRasterPos3d, (GLdouble x, GLdouble y, GLdouble z), (x,y,z))
MESA_FORWARD_VOID(glRasterPos3f, (GLfloat x, GLfloat y, GLfloat z), (x,y,z))
MESA_FORWARD_VOID(glRasterPos3i, (GLint x, GLint y, GLint z), (x,y,z))
MESA_FORWARD_VOID(glRasterPos3s, (GLshort x, GLshort y, GLshort z), (x,y,z))
MESA_FORWARD_VOID(glRasterPos4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), (x,y,z,w))
MESA_FORWARD_VOID(glRasterPos4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x,y,z,w))
MESA_FORWARD_VOID(glRasterPos4i, (GLint x, GLint y, GLint z, GLint w), (x,y,z,w))
MESA_FORWARD_VOID(glRasterPos4s, (GLshort x, GLshort y, GLshort z, GLshort w), (x,y,z,w))
MESA_FORWARD_VOID(glRasterPos2dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glRasterPos2fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glRasterPos2iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glRasterPos2sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glRasterPos3dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glRasterPos3fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glRasterPos3iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glRasterPos3sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glRasterPos4dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glRasterPos4fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glRasterPos4iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glRasterPos4sv, (const GLshort *v), (v))

MESA_FORWARD_VOID(glReadBuffer, (GLenum mode), (mode))
MESA_FORWARD_VOID(glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLvoid *pixels), (x,y,w,h,fmt,type,pixels))

/* glRect variants */
MESA_FORWARD_VOID(glRectd, (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2), (x1,y1,x2,y2))
MESA_FORWARD_VOID(glRectf, (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2), (x1,y1,x2,y2))
MESA_FORWARD_VOID(glRecti, (GLint x1, GLint y1, GLint x2, GLint y2), (x1,y1,x2,y2))
MESA_FORWARD_VOID(glRects, (GLshort x1, GLshort y1, GLshort x2, GLshort y2), (x1,y1,x2,y2))
MESA_FORWARD_VOID(glRectdv, (const GLdouble *v1, const GLdouble *v2), (v1,v2))
MESA_FORWARD_VOID(glRectfv, (const GLfloat *v1, const GLfloat *v2), (v1,v2))
MESA_FORWARD_VOID(glRectiv, (const GLint *v1, const GLint *v2), (v1,v2))
MESA_FORWARD_VOID(glRectsv, (const GLshort *v1, const GLshort *v2), (v1,v2))

MESA_FORWARD_RET(GLint, glRenderMode, (GLenum mode), (mode))
MESA_FORWARD_VOID(glRotated, (GLdouble angle, GLdouble x, GLdouble y, GLdouble z), (angle,x,y,z))
MESA_FORWARD_VOID(glRotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z), (angle,x,y,z))
MESA_FORWARD_VOID(glScaled, (GLdouble x, GLdouble y, GLdouble z), (x,y,z))
MESA_FORWARD_VOID(glScalef, (GLfloat x, GLfloat y, GLfloat z), (x,y,z))
MESA_FORWARD_VOID(glScissor, (GLint x, GLint y, GLsizei w, GLsizei h), (x,y,w,h))
MESA_FORWARD_VOID(glSelectBuffer, (GLsizei size, GLuint *buffer), (size,buffer))
MESA_FORWARD_VOID(glShadeModel, (GLenum mode), (mode))

/* Stencil */
MESA_FORWARD_VOID(glStencilFunc, (GLenum func, GLint ref, GLuint mask), (func,ref,mask))
MESA_FORWARD_VOID(glStencilMask, (GLuint mask), (mask))
MESA_FORWARD_VOID(glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass), (fail,zfail,zpass))


/* glTexCoord variants */
MESA_FORWARD_VOID(glTexCoord1d, (GLdouble s), (s))
MESA_FORWARD_VOID(glTexCoord1f, (GLfloat s), (s))
MESA_FORWARD_VOID(glTexCoord1i, (GLint s), (s))
MESA_FORWARD_VOID(glTexCoord1s, (GLshort s), (s))
MESA_FORWARD_VOID(glTexCoord2d, (GLdouble s, GLdouble t), (s,t))
MESA_FORWARD_VOID(glTexCoord2f, (GLfloat s, GLfloat t), (s,t))
MESA_FORWARD_VOID(glTexCoord2i, (GLint s, GLint t), (s,t))
MESA_FORWARD_VOID(glTexCoord2s, (GLshort s, GLshort t), (s,t))
MESA_FORWARD_VOID(glTexCoord3d, (GLdouble s, GLdouble t, GLdouble r), (s,t,r))
MESA_FORWARD_VOID(glTexCoord3f, (GLfloat s, GLfloat t, GLfloat r), (s,t,r))
MESA_FORWARD_VOID(glTexCoord3i, (GLint s, GLint t, GLint r), (s,t,r))
MESA_FORWARD_VOID(glTexCoord3s, (GLshort s, GLshort t, GLshort r), (s,t,r))
MESA_FORWARD_VOID(glTexCoord4d, (GLdouble s, GLdouble t, GLdouble r, GLdouble q), (s,t,r,q))
MESA_FORWARD_VOID(glTexCoord4f, (GLfloat s, GLfloat t, GLfloat r, GLfloat q), (s,t,r,q))
MESA_FORWARD_VOID(glTexCoord4i, (GLint s, GLint t, GLint r, GLint q), (s,t,r,q))
MESA_FORWARD_VOID(glTexCoord4s, (GLshort s, GLshort t, GLshort r, GLshort q), (s,t,r,q))
MESA_FORWARD_VOID(glTexCoord1dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glTexCoord1fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glTexCoord1iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glTexCoord1sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glTexCoord2dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glTexCoord2fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glTexCoord2iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glTexCoord2sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glTexCoord3dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glTexCoord3fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glTexCoord3iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glTexCoord3sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glTexCoord4dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glTexCoord4fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glTexCoord4iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glTexCoord4sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *p), (size,type,stride,p))

/* glTexEnv */
MESA_FORWARD_VOID(glTexEnvf, (GLenum target, GLenum pname, GLfloat param), (target,pname,param))
MESA_FORWARD_VOID(glTexEnvi, (GLenum target, GLenum pname, GLint param), (target,pname,param))
MESA_FORWARD_VOID(glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params), (target,pname,params))
MESA_FORWARD_VOID(glTexEnviv, (GLenum target, GLenum pname, const GLint *params), (target,pname,params))

/* glTexGen */
MESA_FORWARD_VOID(glTexGend, (GLenum coord, GLenum pname, GLdouble param), (coord,pname,param))
MESA_FORWARD_VOID(glTexGenf, (GLenum coord, GLenum pname, GLfloat param), (coord,pname,param))
MESA_FORWARD_VOID(glTexGeni, (GLenum coord, GLenum pname, GLint param), (coord,pname,param))
MESA_FORWARD_VOID(glTexGendv, (GLenum coord, GLenum pname, const GLdouble *params), (coord,pname,params))
MESA_FORWARD_VOID(glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params), (coord,pname,params))
MESA_FORWARD_VOID(glTexGeniv, (GLenum coord, GLenum pname, const GLint *params), (coord,pname,params))

/* glTexImage */
MESA_FORWARD_VOID(glTexImage1D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels), (target,level,internalformat,width,border,format,type,pixels))
MESA_FORWARD_VOID(glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels), (target,level,internalformat,width,height,border,format,type,pixels))

/* glTexParameter */
MESA_FORWARD_VOID(glTexParameterf, (GLenum target, GLenum pname, GLfloat param), (target,pname,param))
MESA_FORWARD_VOID(glTexParameteri, (GLenum target, GLenum pname, GLint param), (target,pname,param))
MESA_FORWARD_VOID(glTexParameterfv, (GLenum target, GLenum pname, const GLfloat *params), (target,pname,params))
MESA_FORWARD_VOID(glTexParameteriv, (GLenum target, GLenum pname, const GLint *params), (target,pname,params))

/* glTexSubImage */
MESA_FORWARD_VOID(glTexSubImage1D, (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels), (target,level,xoffset,width,format,type,pixels))
MESA_FORWARD_VOID(glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels), (target,level,xoffset,yoffset,width,height,format,type,pixels))

/* Translate */
MESA_FORWARD_VOID(glTranslated, (GLdouble x, GLdouble y, GLdouble z), (x,y,z))
MESA_FORWARD_VOID(glTranslatef, (GLfloat x, GLfloat y, GLfloat z), (x,y,z))


/* glVertex variants */
MESA_FORWARD_VOID(glVertex2d, (GLdouble x, GLdouble y), (x,y))
MESA_FORWARD_VOID(glVertex2f, (GLfloat x, GLfloat y), (x,y))
MESA_FORWARD_VOID(glVertex2i, (GLint x, GLint y), (x,y))
MESA_FORWARD_VOID(glVertex2s, (GLshort x, GLshort y), (x,y))
MESA_FORWARD_VOID(glVertex3d, (GLdouble x, GLdouble y, GLdouble z), (x,y,z))
MESA_FORWARD_VOID(glVertex3f, (GLfloat x, GLfloat y, GLfloat z), (x,y,z))
MESA_FORWARD_VOID(glVertex3i, (GLint x, GLint y, GLint z), (x,y,z))
MESA_FORWARD_VOID(glVertex3s, (GLshort x, GLshort y, GLshort z), (x,y,z))
MESA_FORWARD_VOID(glVertex4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), (x,y,z,w))
MESA_FORWARD_VOID(glVertex4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x,y,z,w))
MESA_FORWARD_VOID(glVertex4i, (GLint x, GLint y, GLint z, GLint w), (x,y,z,w))
MESA_FORWARD_VOID(glVertex4s, (GLshort x, GLshort y, GLshort z, GLshort w), (x,y,z,w))
MESA_FORWARD_VOID(glVertex2dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glVertex2fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glVertex2iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glVertex2sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glVertex3dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glVertex3fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glVertex3iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glVertex3sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glVertex4dv, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glVertex4fv, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glVertex4iv, (const GLint *v), (v))
MESA_FORWARD_VOID(glVertex4sv, (const GLshort *v), (v))
MESA_FORWARD_VOID(glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *p), (size,type,stride,p))

MESA_FORWARD_VOID(glViewport, (GLint x, GLint y, GLsizei w, GLsizei h), (x,y,w,h))

/* Missing from first batch */
MESA_FORWARD_RET(GLboolean, glAreTexturesResident, (GLsizei n, const GLuint *textures, GLboolean *residences), (n,textures,residences))
MESA_FORWARD_VOID(glArrayElement, (GLint i), (i))
MESA_FORWARD_VOID(glBindTexture, (GLenum target, GLuint texture), (target,texture))
MESA_FORWARD_RET(GLuint, glGenLists, (GLsizei range), (range))

/*---------------------------------------------------------------------------
 * EXT Extensions — forwarded to Mesa
 *---------------------------------------------------------------------------*/

MESA_FORWARD_VOID(glPolygonOffsetEXT, (GLfloat factor, GLfloat bias), (factor,bias))
MESA_FORWARD_VOID(glBlendEquationEXT, (GLenum mode), (mode))
MESA_FORWARD_VOID(glBlendColorEXT, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha), (red,green,blue,alpha))

/* EXT vertex array */
MESA_FORWARD_VOID(glVertexPointerEXT, (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *p), (size,type,stride,count,p))
MESA_FORWARD_VOID(glNormalPointerEXT, (GLenum type, GLsizei stride, GLsizei count, const GLvoid *p), (type,stride,count,p))
MESA_FORWARD_VOID(glColorPointerEXT, (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *p), (size,type,stride,count,p))
MESA_FORWARD_VOID(glIndexPointerEXT, (GLenum type, GLsizei stride, GLsizei count, const GLvoid *p), (type,stride,count,p))
MESA_FORWARD_VOID(glTexCoordPointerEXT, (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *p), (size,type,stride,count,p))
MESA_FORWARD_VOID(glEdgeFlagPointerEXT, (GLsizei stride, GLsizei count, const GLboolean *p), (stride,count,p))
MESA_FORWARD_VOID(glGetPointervEXT, (GLenum pname, GLvoid **params), (pname,params))
MESA_FORWARD_VOID(glArrayElementEXT, (GLint i), (i))
MESA_FORWARD_VOID(glDrawArraysEXT, (GLenum mode, GLint first, GLsizei count), (mode,first,count))

/* EXT texture object */
MESA_FORWARD_VOID(glBindTextureEXT, (GLenum target, GLuint texture), (target,texture))
MESA_FORWARD_VOID(glDeleteTexturesEXT, (GLsizei n, const GLuint *textures), (n,textures))
MESA_FORWARD_VOID(glGenTexturesEXT, (GLsizei n, GLuint *textures), (n,textures))
MESA_FORWARD_VOID(glPrioritizeTexturesEXT, (GLsizei n, const GLuint *textures, const GLfloat *priorities), (n,textures,priorities))

/* EXT texture3D */
MESA_FORWARD_VOID(glCopyTexSubImage3DEXT, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target,level,xoffset,yoffset,zoffset,x,y,width,height))
MESA_FORWARD_VOID(glTexImage3DEXT, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels), (target,level,internalformat,width,height,depth,border,format,type,pixels))
MESA_FORWARD_VOID(glTexSubImage3DEXT, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels), (target,level,xoffset,yoffset,zoffset,width,height,depth,format,type,pixels))

/* EXT color table */
MESA_FORWARD_VOID(glColorTableEXT, (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table), (target,internalformat,width,format,type,table))
MESA_FORWARD_VOID(glColorSubTableEXT, (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data), (target,start,count,format,type,data))
MESA_FORWARD_VOID(glGetColorTableEXT, (GLenum target, GLenum format, GLenum type, GLvoid *table), (target,format,type,table))
MESA_FORWARD_VOID(glGetColorTableParameterivEXT, (GLenum target, GLenum pname, GLint *params), (target,pname,params))
MESA_FORWARD_VOID(glGetColorTableParameterfvEXT, (GLenum target, GLenum pname, GLfloat *params), (target,pname,params))

/* EXT point parameters (not in .def but referenced in task description) */
MESA_FORWARD_VOID(glPointParameterfEXT, (GLenum pname, GLfloat param), (pname,param))
MESA_FORWARD_VOID(glPointParameterfvEXT, (GLenum pname, const GLfloat *params), (pname,params))

/* EXT compiled vertex array */
MESA_FORWARD_VOID(glLockArraysEXT, (GLint first, GLsizei count), (first,count))
MESA_FORWARD_VOID(glUnlockArraysEXT, (void), ())


/*---------------------------------------------------------------------------
 * MESA WindowPos extensions — forwarded to Mesa
 *---------------------------------------------------------------------------*/

MESA_FORWARD_VOID(glWindowPos2iMESA, (GLint x, GLint y), (x,y))
MESA_FORWARD_VOID(glWindowPos2sMESA, (GLshort x, GLshort y), (x,y))
MESA_FORWARD_VOID(glWindowPos2fMESA, (GLfloat x, GLfloat y), (x,y))
MESA_FORWARD_VOID(glWindowPos2dMESA, (GLdouble x, GLdouble y), (x,y))
MESA_FORWARD_VOID(glWindowPos2ivMESA, (const GLint *v), (v))
MESA_FORWARD_VOID(glWindowPos2svMESA, (const GLshort *v), (v))
MESA_FORWARD_VOID(glWindowPos2fvMESA, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glWindowPos2dvMESA, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glWindowPos3iMESA, (GLint x, GLint y, GLint z), (x,y,z))
MESA_FORWARD_VOID(glWindowPos3sMESA, (GLshort x, GLshort y, GLshort z), (x,y,z))
MESA_FORWARD_VOID(glWindowPos3fMESA, (GLfloat x, GLfloat y, GLfloat z), (x,y,z))
MESA_FORWARD_VOID(glWindowPos3dMESA, (GLdouble x, GLdouble y, GLdouble z), (x,y,z))
MESA_FORWARD_VOID(glWindowPos3ivMESA, (const GLint *v), (v))
MESA_FORWARD_VOID(glWindowPos3svMESA, (const GLshort *v), (v))
MESA_FORWARD_VOID(glWindowPos3fvMESA, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glWindowPos3dvMESA, (const GLdouble *v), (v))
MESA_FORWARD_VOID(glWindowPos4iMESA, (GLint x, GLint y, GLint z, GLint w), (x,y,z,w))
MESA_FORWARD_VOID(glWindowPos4sMESA, (GLshort x, GLshort y, GLshort z, GLshort w), (x,y,z,w))
MESA_FORWARD_VOID(glWindowPos4fMESA, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x,y,z,w))
MESA_FORWARD_VOID(glWindowPos4dMESA, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), (x,y,z,w))
MESA_FORWARD_VOID(glWindowPos4ivMESA, (const GLint *v), (v))
MESA_FORWARD_VOID(glWindowPos4svMESA, (const GLshort *v), (v))
MESA_FORWARD_VOID(glWindowPos4fvMESA, (const GLfloat *v), (v))
MESA_FORWARD_VOID(glWindowPos4dvMESA, (const GLdouble *v), (v))

MESA_FORWARD_VOID(glResizeBuffersMESA, (void), ())

/*---------------------------------------------------------------------------
 * ARB Multitexture — forwarded to Mesa
 *---------------------------------------------------------------------------*/

MESA_FORWARD_VOID(glActiveTextureARB, (GLenum texture), (texture))
MESA_FORWARD_VOID(glClientActiveTextureARB, (GLenum texture), (texture))
MESA_FORWARD_VOID(glMultiTexCoord1dARB, (GLenum target, GLdouble s), (target,s))
MESA_FORWARD_VOID(glMultiTexCoord1dvARB, (GLenum target, const GLdouble *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord1fARB, (GLenum target, GLfloat s), (target,s))
MESA_FORWARD_VOID(glMultiTexCoord1fvARB, (GLenum target, const GLfloat *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord1iARB, (GLenum target, GLint s), (target,s))
MESA_FORWARD_VOID(glMultiTexCoord1ivARB, (GLenum target, const GLint *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord1sARB, (GLenum target, GLshort s), (target,s))
MESA_FORWARD_VOID(glMultiTexCoord1svARB, (GLenum target, const GLshort *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord2dARB, (GLenum target, GLdouble s, GLdouble t), (target,s,t))
MESA_FORWARD_VOID(glMultiTexCoord2dvARB, (GLenum target, const GLdouble *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord2fARB, (GLenum target, GLfloat s, GLfloat t), (target,s,t))
MESA_FORWARD_VOID(glMultiTexCoord2fvARB, (GLenum target, const GLfloat *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord2iARB, (GLenum target, GLint s, GLint t), (target,s,t))
MESA_FORWARD_VOID(glMultiTexCoord2ivARB, (GLenum target, const GLint *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord2sARB, (GLenum target, GLshort s, GLshort t), (target,s,t))
MESA_FORWARD_VOID(glMultiTexCoord2svARB, (GLenum target, const GLshort *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord3dARB, (GLenum target, GLdouble s, GLdouble t, GLdouble r), (target,s,t,r))
MESA_FORWARD_VOID(glMultiTexCoord3dvARB, (GLenum target, const GLdouble *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord3fARB, (GLenum target, GLfloat s, GLfloat t, GLfloat r), (target,s,t,r))
MESA_FORWARD_VOID(glMultiTexCoord3fvARB, (GLenum target, const GLfloat *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord3iARB, (GLenum target, GLint s, GLint t, GLint r), (target,s,t,r))
MESA_FORWARD_VOID(glMultiTexCoord3ivARB, (GLenum target, const GLint *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord3sARB, (GLenum target, GLshort s, GLshort t, GLshort r), (target,s,t,r))
MESA_FORWARD_VOID(glMultiTexCoord3svARB, (GLenum target, const GLshort *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord4dARB, (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q), (target,s,t,r,q))
MESA_FORWARD_VOID(glMultiTexCoord4dvARB, (GLenum target, const GLdouble *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord4fARB, (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q), (target,s,t,r,q))
MESA_FORWARD_VOID(glMultiTexCoord4fvARB, (GLenum target, const GLfloat *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord4iARB, (GLenum target, GLint s, GLint t, GLint r, GLint q), (target,s,t,r,q))
MESA_FORWARD_VOID(glMultiTexCoord4ivARB, (GLenum target, const GLint *v), (target,v))
MESA_FORWARD_VOID(glMultiTexCoord4sARB, (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q), (target,s,t,r,q))
MESA_FORWARD_VOID(glMultiTexCoord4svARB, (GLenum target, const GLshort *v), (target,v))

