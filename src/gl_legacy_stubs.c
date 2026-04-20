/*********************************************************************************
*
*  Legacy GL function stubs for opengl32.def exports.
*
*  These are legacy OpenGL functions that were previously provided by Mesa
*  but are no longer available after the Mesa removal. They are exported
*  by opengl32.def so that applications linking against this DLL can resolve
*  them. All are no-op stubs.
*
*  This file will be removed when the DX9 backend is fully replaced by GL46.
*
*********************************************************************************/

#include <windows.h>
#include <stdio.h>

/* Include the GL state implementation functions */
#include "gl46/gl_impl.h"

/* We intentionally do NOT include glad/gl.h here to avoid macro conflicts.
 * GLAD defines glXxx as macros expanding to glad_glXxx function pointers,
 * but the .def file needs actual function symbols. We define our own
 * minimal GL types. */

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

#define GL_TRUE  1
#define GL_FALSE 0

/* Suppress "unreferenced formal parameter" warnings */
#pragma warning(disable: 4100)

/*---------------------------------------------------------------------------
 * Legacy GL 1.0/1.1 functions not in GLAD core profile
 * These are exported by opengl32.def
 *---------------------------------------------------------------------------*/

void APIENTRY glAccum(GLenum op, GLfloat value) { (void)op; (void)value; }
void APIENTRY glAlphaFunc(GLenum func, GLfloat ref) { _glsAlphaFunc(func, ref); }
void APIENTRY glBegin(GLenum mode) { _glsBegin(mode); }
void APIENTRY glBitmap(GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym, const GLubyte *bm) { (void)w; (void)h; (void)xo; (void)yo; (void)xm; (void)ym; (void)bm; }
void APIENTRY glCallList(GLuint list) { _glsCallList(list); }
void APIENTRY glCallLists(GLsizei n, GLenum type, const GLvoid *lists) { _glsCallLists(n, type, lists); }
void APIENTRY glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
void APIENTRY glClearIndex(GLfloat c) { (void)c; }
void APIENTRY glClipPlane(GLenum plane, const GLdouble *equation) { _glsClipPlane(plane, equation); }

void APIENTRY glColor3b(GLbyte r, GLbyte g, GLbyte b) { _glsColor4f(r/127.0f, g/127.0f, b/127.0f, 1.0f); }
void APIENTRY glColor3d(GLdouble r, GLdouble g, GLdouble b) { _glsColor4f((float)r, (float)g, (float)b, 1.0f); }
void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) { _glsColor3f(r, g, b); }
void APIENTRY glColor3i(GLint r, GLint g, GLint b) { _glsColor4f(r/2147483647.0f, g/2147483647.0f, b/2147483647.0f, 1.0f); }
void APIENTRY glColor3s(GLshort r, GLshort g, GLshort b) { _glsColor4f(r/32767.0f, g/32767.0f, b/32767.0f, 1.0f); }
void APIENTRY glColor3ub(GLubyte r, GLubyte g, GLubyte b) { _glsColor3ub(r, g, b); }
void APIENTRY glColor3ui(GLuint r, GLuint g, GLuint b) { _glsColor4f(r/4294967295.0f, g/4294967295.0f, b/4294967295.0f, 1.0f); }
void APIENTRY glColor3us(GLushort r, GLushort g, GLushort b) { _glsColor4f(r/65535.0f, g/65535.0f, b/65535.0f, 1.0f); }
void APIENTRY glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a) { _glsColor4f(r/127.0f, g/127.0f, b/127.0f, a/127.0f); }
void APIENTRY glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { _glsColor4f((float)r, (float)g, (float)b, (float)a); }
void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { _glsColor4f(r, g, b, a); }
void APIENTRY glColor4i(GLint r, GLint g, GLint b, GLint a) { _glsColor4f(r/2147483647.0f, g/2147483647.0f, b/2147483647.0f, a/2147483647.0f); }
void APIENTRY glColor4s(GLshort r, GLshort g, GLshort b, GLshort a) { _glsColor4f(r/32767.0f, g/32767.0f, b/32767.0f, a/32767.0f); }
void APIENTRY glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { _glsColor4ub(r, g, b, a); }
void APIENTRY glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a) { _glsColor4f(r/4294967295.0f, g/4294967295.0f, b/4294967295.0f, a/4294967295.0f); }
void APIENTRY glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { _glsColor4f(r/65535.0f, g/65535.0f, b/65535.0f, a/65535.0f); }
void APIENTRY glColor3bv(const GLbyte *v) { if(v) _glsColor4f(v[0]/127.0f, v[1]/127.0f, v[2]/127.0f, 1.0f); }
void APIENTRY glColor3dv(const GLdouble *v) { if(v) _glsColor4f((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glColor3fv(const GLfloat *v) { if(v) _glsColor3f(v[0], v[1], v[2]); }
void APIENTRY glColor3iv(const GLint *v) { if(v) _glsColor4f(v[0]/2147483647.0f, v[1]/2147483647.0f, v[2]/2147483647.0f, 1.0f); }
void APIENTRY glColor3sv(const GLshort *v) { if(v) _glsColor4f(v[0]/32767.0f, v[1]/32767.0f, v[2]/32767.0f, 1.0f); }
void APIENTRY glColor3ubv(const GLubyte *v) { if(v) _glsColor3ub(v[0], v[1], v[2]); }
void APIENTRY glColor3uiv(const GLuint *v) { if(v) _glsColor4f(v[0]/4294967295.0f, v[1]/4294967295.0f, v[2]/4294967295.0f, 1.0f); }
void APIENTRY glColor3usv(const GLushort *v) { if(v) _glsColor4f(v[0]/65535.0f, v[1]/65535.0f, v[2]/65535.0f, 1.0f); }
void APIENTRY glColor4bv(const GLbyte *v) { if(v) _glsColor4f(v[0]/127.0f, v[1]/127.0f, v[2]/127.0f, v[3]/127.0f); }
void APIENTRY glColor4dv(const GLdouble *v) { if(v) _glsColor4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glColor4fv(const GLfloat *v) { if(v) _glsColor4f(v[0], v[1], v[2], v[3]); }
void APIENTRY glColor4iv(const GLint *v) { if(v) _glsColor4f(v[0]/2147483647.0f, v[1]/2147483647.0f, v[2]/2147483647.0f, v[3]/2147483647.0f); }
void APIENTRY glColor4sv(const GLshort *v) { if(v) _glsColor4f(v[0]/32767.0f, v[1]/32767.0f, v[2]/32767.0f, v[3]/32767.0f); }
void APIENTRY glColor4ubv(const GLubyte *v) { if(v) _glsColor4ub(v[0], v[1], v[2], v[3]); }
void APIENTRY glColor4uiv(const GLuint *v) { if(v) _glsColor4f(v[0]/4294967295.0f, v[1]/4294967295.0f, v[2]/4294967295.0f, v[3]/4294967295.0f); }
void APIENTRY glColor4usv(const GLushort *v) { if(v) _glsColor4f(v[0]/65535.0f, v[1]/65535.0f, v[2]/65535.0f, v[3]/65535.0f); }

void APIENTRY glColorMaterial(GLenum face, GLenum mode) { _glsColorMaterial(face, mode); }
void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum type) { (void)x; (void)y; (void)w; (void)h; (void)type; }
void APIENTRY glDeleteLists(GLuint list, GLsizei range) { _glsDeleteLists(list, range); }
void APIENTRY glDrawPixels(GLsizei w, GLsizei h, GLenum fmt, GLenum type, const GLvoid *pix) { (void)w; (void)h; (void)fmt; (void)type; (void)pix; }
void APIENTRY glEdgeFlag(GLboolean flag) { (void)flag; }
void APIENTRY glEdgeFlagv(const GLboolean *flag) { (void)flag; }
void APIENTRY glEnd(void) { _glsEnd(); }
void APIENTRY glEndList(void) { _glsEndList(); }

void APIENTRY glEvalCoord1d(GLdouble u) { (void)u; }
void APIENTRY glEvalCoord1f(GLfloat u) { (void)u; }
void APIENTRY glEvalCoord1dv(const GLdouble *u) { (void)u; }
void APIENTRY glEvalCoord1fv(const GLfloat *u) { (void)u; }
void APIENTRY glEvalCoord2d(GLdouble u, GLdouble v) { (void)u; (void)v; }
void APIENTRY glEvalCoord2f(GLfloat u, GLfloat v) { (void)u; (void)v; }
void APIENTRY glEvalCoord2dv(const GLdouble *u) { (void)u; }
void APIENTRY glEvalCoord2fv(const GLfloat *u) { (void)u; }
void APIENTRY glEvalPoint1(GLint i) { (void)i; }
void APIENTRY glEvalPoint2(GLint i, GLint j) { (void)i; (void)j; }
void APIENTRY glEvalMesh1(GLenum mode, GLint i1, GLint i2) { (void)mode; (void)i1; (void)i2; }
void APIENTRY glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) { (void)mode; (void)i1; (void)i2; (void)j1; (void)j2; }

void APIENTRY glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer) { (void)size; (void)type; (void)buffer; }
void APIENTRY glFogf(GLenum pname, GLfloat param) { _glsFogf(pname, param); }
void APIENTRY glFogi(GLenum pname, GLint param) { _glsFogi(pname, param); }
void APIENTRY glFogfv(GLenum pname, const GLfloat *params) { _glsFogfv(pname, params); }
void APIENTRY glFogiv(GLenum pname, const GLint *params) { _glsFogiv(pname, params); }
void APIENTRY glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { _glsFrustum(l, r, b, t, n, f); }
GLuint APIENTRY glGenLists(GLsizei range) { return _glsGenLists(range); }

void APIENTRY glGetClipPlane(GLenum plane, GLdouble *equation) { _glsGetClipPlane(plane, equation); }
void APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat *params) { _glsGetLightfv(light, pname, params); }
void APIENTRY glGetLightiv(GLenum light, GLenum pname, GLint *params) { _glsGetLightiv(light, pname, params); }
void APIENTRY glGetMapdv(GLenum target, GLenum query, GLdouble *v) { (void)target; (void)query; if(v) *v = 0.0; }
void APIENTRY glGetMapfv(GLenum target, GLenum query, GLfloat *v) { (void)target; (void)query; if(v) *v = 0.0f; }
void APIENTRY glGetMapiv(GLenum target, GLenum query, GLint *v) { (void)target; (void)query; if(v) *v = 0; }
void APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params) { _glsGetMaterialfv(face, pname, params); }
void APIENTRY glGetMaterialiv(GLenum face, GLenum pname, GLint *params) { _glsGetMaterialiv(face, pname, params); }
void APIENTRY glGetPixelMapfv(GLenum map, GLfloat *values) { (void)map; if(values) *values = 0.0f; }
void APIENTRY glGetPixelMapuiv(GLenum map, GLuint *values) { (void)map; if(values) *values = 0; }
void APIENTRY glGetPixelMapusv(GLenum map, GLushort *values) { (void)map; if(values) *values = 0; }
void APIENTRY glGetPolygonStipple(GLubyte *mask) { (void)mask; }
void APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params) { (void)target; (void)pname; if(params) *params = 0.0f; }
void APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint *params) { (void)target; (void)pname; if(params) *params = 0; }
void APIENTRY glGetTexGeniv(GLenum coord, GLenum pname, GLint *params) { (void)coord; (void)pname; if(params) *params = 0; }
void APIENTRY glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params) { (void)coord; (void)pname; if(params) *params = 0.0; }
void APIENTRY glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params) { (void)coord; (void)pname; if(params) *params = 0.0f; }

void APIENTRY glIndexd(GLdouble c) { (void)c; }
void APIENTRY glIndexf(GLfloat c) { (void)c; }
void APIENTRY glIndexi(GLint c) { (void)c; }
void APIENTRY glIndexs(GLshort c) { (void)c; }
void APIENTRY glIndexub(GLubyte c) { (void)c; }
void APIENTRY glIndexdv(const GLdouble *c) { (void)c; }
void APIENTRY glIndexfv(const GLfloat *c) { (void)c; }
void APIENTRY glIndexiv(const GLint *c) { (void)c; }
void APIENTRY glIndexsv(const GLshort *c) { (void)c; }
void APIENTRY glIndexubv(const GLubyte *c) { (void)c; }
void APIENTRY glIndexMask(GLuint mask) { (void)mask; }
void APIENTRY glInitNames(void) { }
GLboolean APIENTRY glIsList(GLuint list) { return _glsIsList(list); }

void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param) { _glsLightf(light, pname, param); }
void APIENTRY glLighti(GLenum light, GLenum pname, GLint param) { _glsLightf(light, pname, (float)param); }
void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params) { _glsLightfv(light, pname, params); }
void APIENTRY glLightiv(GLenum light, GLenum pname, const GLint *params) { if(params) { float fv[4]; fv[0]=(float)params[0]; fv[1]=(float)params[1]; fv[2]=(float)params[2]; fv[3]=(float)params[3]; _glsLightfv(light, pname, fv); } }
void APIENTRY glLightModelf(GLenum pname, GLfloat param) { _glsLightModelf(pname, param); }
void APIENTRY glLightModeli(GLenum pname, GLint param) { _glsLightModelf(pname, (float)param); }
void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params) { _glsLightModelfv(pname, params); }
void APIENTRY glLightModeliv(GLenum pname, const GLint *params) { if(params) { float fv[4]; fv[0]=(float)params[0]; fv[1]=(float)params[1]; fv[2]=(float)params[2]; fv[3]=(float)params[3]; _glsLightModelfv(pname, fv); } }
void APIENTRY glLineStipple(GLint factor, GLushort pattern) { (void)factor; (void)pattern; }
void APIENTRY glListBase(GLuint base) { _glsListBase(base); }
void APIENTRY glLoadIdentity(void) { _glsLoadIdentity(); }
void APIENTRY glLoadMatrixd(const GLdouble *m) { _glsLoadMatrixd(m); }
void APIENTRY glLoadMatrixf(const GLfloat *m) { _glsLoadMatrixf(m); }
void APIENTRY glLoadName(GLuint name) { (void)name; }

void APIENTRY glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points) { (void)target; (void)u1; (void)u2; (void)stride; (void)order; (void)points; }
void APIENTRY glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points) { (void)target; (void)u1; (void)u2; (void)stride; (void)order; (void)points; }
void APIENTRY glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points) { (void)target; (void)u1; (void)u2; (void)ustride; (void)uorder; (void)v1; (void)v2; (void)vstride; (void)vorder; (void)points; }
void APIENTRY glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) { (void)target; (void)u1; (void)u2; (void)ustride; (void)uorder; (void)v1; (void)v2; (void)vstride; (void)vorder; (void)points; }
void APIENTRY glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) { (void)un; (void)u1; (void)u2; }
void APIENTRY glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) { (void)un; (void)u1; (void)u2; }
void APIENTRY glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) { (void)un; (void)u1; (void)u2; (void)vn; (void)v1; (void)v2; }
void APIENTRY glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) { (void)un; (void)u1; (void)u2; (void)vn; (void)v1; (void)v2; }
void APIENTRY glMaterialf(GLenum face, GLenum pname, GLfloat param) { _glsMaterialf(face, pname, param); }
void APIENTRY glMateriali(GLenum face, GLenum pname, GLint param) { _glsMaterialf(face, pname, (float)param); }
void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) { _glsMaterialfv(face, pname, params); }
void APIENTRY glMaterialiv(GLenum face, GLenum pname, const GLint *params) { if(params) { float fv[4]; fv[0]=(float)params[0]; fv[1]=(float)params[1]; fv[2]=(float)params[2]; fv[3]=(float)params[3]; _glsMaterialfv(face, pname, fv); } }
void APIENTRY glMatrixMode(GLenum mode) { _glsMatrixMode(mode); }
void APIENTRY glMultMatrixd(const GLdouble *m) { _glsMultMatrixd(m); }
void APIENTRY glMultMatrixf(const GLfloat *m) { _glsMultMatrixf(m); }
void APIENTRY glNewList(GLuint list, GLenum mode) { _glsNewList(list, mode); }

void APIENTRY glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) { _glsNormal3f(nx/127.0f, ny/127.0f, nz/127.0f); }
void APIENTRY glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) { _glsNormal3f((float)nx, (float)ny, (float)nz); }
void APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) { _glsNormal3f(nx, ny, nz); }
void APIENTRY glNormal3i(GLint nx, GLint ny, GLint nz) { _glsNormal3f(nx/2147483647.0f, ny/2147483647.0f, nz/2147483647.0f); }
void APIENTRY glNormal3s(GLshort nx, GLshort ny, GLshort nz) { _glsNormal3f(nx/32767.0f, ny/32767.0f, nz/32767.0f); }
void APIENTRY glNormal3bv(const GLbyte *v) { if(v) _glsNormal3f(v[0]/127.0f, v[1]/127.0f, v[2]/127.0f); }
void APIENTRY glNormal3dv(const GLdouble *v) { if(v) _glsNormal3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glNormal3fv(const GLfloat *v) { if(v) _glsNormal3f(v[0], v[1], v[2]); }
void APIENTRY glNormal3iv(const GLint *v) { if(v) _glsNormal3f(v[0]/2147483647.0f, v[1]/2147483647.0f, v[2]/2147483647.0f); }
void APIENTRY glNormal3sv(const GLshort *v) { if(v) _glsNormal3f(v[0]/32767.0f, v[1]/32767.0f, v[2]/32767.0f); }

void APIENTRY glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { _glsOrtho(l, r, b, t, n, f); }
void APIENTRY glPassThrough(GLfloat token) { (void)token; }
void APIENTRY glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values) { (void)map; (void)mapsize; (void)values; }
void APIENTRY glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values) { (void)map; (void)mapsize; (void)values; }
void APIENTRY glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values) { (void)map; (void)mapsize; (void)values; }
void APIENTRY glPixelTransferf(GLenum pname, GLfloat param) { (void)pname; (void)param; }
void APIENTRY glPixelTransferi(GLenum pname, GLint param) { (void)pname; (void)param; }
void APIENTRY glPixelZoom(GLfloat xfactor, GLfloat yfactor) { (void)xfactor; (void)yfactor; }
void APIENTRY glPolygonStipple(const GLubyte *mask) { (void)mask; }
void APIENTRY glPopAttrib(void) { _glsPopAttrib(); }
void APIENTRY glPopClientAttrib(void) { _glsPopClientAttrib(); }
void APIENTRY glPopMatrix(void) { _glsPopMatrix(); }
void APIENTRY glPopName(void) { }
void APIENTRY glPushAttrib(GLbitfield mask) { _glsPushAttrib(mask); }
void APIENTRY glPushClientAttrib(GLbitfield mask) { _glsPushClientAttrib(mask); }
void APIENTRY glPushMatrix(void) { _glsPushMatrix(); }
void APIENTRY glPushName(GLuint name) { (void)name; }

void APIENTRY glRasterPos2d(GLdouble x, GLdouble y) { (void)x; (void)y; }
void APIENTRY glRasterPos2f(GLfloat x, GLfloat y) { (void)x; (void)y; }
void APIENTRY glRasterPos2i(GLint x, GLint y) { (void)x; (void)y; }
void APIENTRY glRasterPos2s(GLshort x, GLshort y) { (void)x; (void)y; }
void APIENTRY glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) { (void)x; (void)y; (void)z; }
void APIENTRY glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void APIENTRY glRasterPos3i(GLint x, GLint y, GLint z) { (void)x; (void)y; (void)z; }
void APIENTRY glRasterPos3s(GLshort x, GLshort y, GLshort z) { (void)x; (void)y; (void)z; }
void APIENTRY glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glRasterPos4i(GLint x, GLint y, GLint z, GLint w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glRasterPos2dv(const GLdouble *v) { (void)v; }
void APIENTRY glRasterPos2fv(const GLfloat *v) { (void)v; }
void APIENTRY glRasterPos2iv(const GLint *v) { (void)v; }
void APIENTRY glRasterPos2sv(const GLshort *v) { (void)v; }
void APIENTRY glRasterPos3dv(const GLdouble *v) { (void)v; }
void APIENTRY glRasterPos3fv(const GLfloat *v) { (void)v; }
void APIENTRY glRasterPos3iv(const GLint *v) { (void)v; }
void APIENTRY glRasterPos3sv(const GLshort *v) { (void)v; }
void APIENTRY glRasterPos4dv(const GLdouble *v) { (void)v; }
void APIENTRY glRasterPos4fv(const GLfloat *v) { (void)v; }
void APIENTRY glRasterPos4iv(const GLint *v) { (void)v; }
void APIENTRY glRasterPos4sv(const GLshort *v) { (void)v; }

void APIENTRY glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) { _glsBegin(0x0007); _glsVertex2f((float)x1,(float)y1); _glsVertex2f((float)x2,(float)y1); _glsVertex2f((float)x2,(float)y2); _glsVertex2f((float)x1,(float)y2); _glsEnd(); }
void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) { _glsBegin(0x0007); _glsVertex2f(x1,y1); _glsVertex2f(x2,y1); _glsVertex2f(x2,y2); _glsVertex2f(x1,y2); _glsEnd(); }
void APIENTRY glRecti(GLint x1, GLint y1, GLint x2, GLint y2) { _glsBegin(0x0007); _glsVertex2f((float)x1,(float)y1); _glsVertex2f((float)x2,(float)y1); _glsVertex2f((float)x2,(float)y2); _glsVertex2f((float)x1,(float)y2); _glsEnd(); }
void APIENTRY glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) { _glsBegin(0x0007); _glsVertex2f((float)x1,(float)y1); _glsVertex2f((float)x2,(float)y1); _glsVertex2f((float)x2,(float)y2); _glsVertex2f((float)x1,(float)y2); _glsEnd(); }
void APIENTRY glRectdv(const GLdouble *v1, const GLdouble *v2) { if(v1&&v2) { _glsBegin(0x0007); _glsVertex2f((float)v1[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v2[1]); _glsVertex2f((float)v1[0],(float)v2[1]); _glsEnd(); } }
void APIENTRY glRectfv(const GLfloat *v1, const GLfloat *v2) { if(v1&&v2) { _glsBegin(0x0007); _glsVertex2f(v1[0],v1[1]); _glsVertex2f(v2[0],v1[1]); _glsVertex2f(v2[0],v2[1]); _glsVertex2f(v1[0],v2[1]); _glsEnd(); } }
void APIENTRY glRectiv(const GLint *v1, const GLint *v2) { if(v1&&v2) { _glsBegin(0x0007); _glsVertex2f((float)v1[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v2[1]); _glsVertex2f((float)v1[0],(float)v2[1]); _glsEnd(); } }
void APIENTRY glRectsv(const GLshort *v1, const GLshort *v2) { if(v1&&v2) { _glsBegin(0x0007); _glsVertex2f((float)v1[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v1[1]); _glsVertex2f((float)v2[0],(float)v2[1]); _glsVertex2f((float)v1[0],(float)v2[1]); _glsEnd(); } }
GLint APIENTRY glRenderMode(GLenum mode) { return 0; }
void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) { _glsRotated(angle, x, y, z); }
void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { _glsRotatef(angle, x, y, z); }
void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z) { _glsScaled(x, y, z); }
void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) { _glsScalef(x, y, z); }
void APIENTRY glSelectBuffer(GLsizei size, GLuint *buffer) { (void)size; (void)buffer; }
void APIENTRY glShadeModel(GLenum mode) { _glsShadeModel(mode); }

void APIENTRY glTexCoord1d(GLdouble s) { _glsTexCoord2f((float)s, 0.0f); }
void APIENTRY glTexCoord1f(GLfloat s) { _glsTexCoord2f(s, 0.0f); }
void APIENTRY glTexCoord1i(GLint s) { _glsTexCoord2f((float)s, 0.0f); }
void APIENTRY glTexCoord1s(GLshort s) { _glsTexCoord2f((float)s, 0.0f); }
void APIENTRY glTexCoord2d(GLdouble s, GLdouble t) { _glsTexCoord2f((float)s, (float)t); }
void APIENTRY glTexCoord2f(GLfloat s, GLfloat t) { _glsTexCoord2f(s, t); }
void APIENTRY glTexCoord2i(GLint s, GLint t) { _glsTexCoord2f((float)s, (float)t); }
void APIENTRY glTexCoord2s(GLshort s, GLshort t) { _glsTexCoord2f((float)s, (float)t); }
void APIENTRY glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) { _glsTexCoord3f((float)s, (float)t, (float)r); }
void APIENTRY glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) { _glsTexCoord3f(s, t, r); }
void APIENTRY glTexCoord3i(GLint s, GLint t, GLint r) { _glsTexCoord3f((float)s, (float)t, (float)r); }
void APIENTRY glTexCoord3s(GLshort s, GLshort t, GLshort r) { _glsTexCoord3f((float)s, (float)t, (float)r); }
void APIENTRY glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) { _glsTexCoord4f((float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) { _glsTexCoord4f(s, t, r, q); }
void APIENTRY glTexCoord4i(GLint s, GLint t, GLint r, GLint q) { _glsTexCoord4f((float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) { _glsTexCoord4f((float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord1dv(const GLdouble *v) { if(v) _glsTexCoord2f((float)v[0], 0.0f); }
void APIENTRY glTexCoord1fv(const GLfloat *v) { if(v) _glsTexCoord2f(v[0], 0.0f); }
void APIENTRY glTexCoord1iv(const GLint *v) { if(v) _glsTexCoord2f((float)v[0], 0.0f); }
void APIENTRY glTexCoord1sv(const GLshort *v) { if(v) _glsTexCoord2f((float)v[0], 0.0f); }
void APIENTRY glTexCoord2dv(const GLdouble *v) { if(v) _glsTexCoord2f((float)v[0], (float)v[1]); }
void APIENTRY glTexCoord2fv(const GLfloat *v) { if(v) _glsTexCoord2f(v[0], v[1]); }
void APIENTRY glTexCoord2iv(const GLint *v) { if(v) _glsTexCoord2f((float)v[0], (float)v[1]); }
void APIENTRY glTexCoord2sv(const GLshort *v) { if(v) _glsTexCoord2f((float)v[0], (float)v[1]); }
void APIENTRY glTexCoord3dv(const GLdouble *v) { if(v) _glsTexCoord3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glTexCoord3fv(const GLfloat *v) { if(v) _glsTexCoord3f(v[0], v[1], v[2]); }
void APIENTRY glTexCoord3iv(const GLint *v) { if(v) _glsTexCoord3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glTexCoord3sv(const GLshort *v) { if(v) _glsTexCoord3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glTexCoord4dv(const GLdouble *v) { if(v) _glsTexCoord4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glTexCoord4fv(const GLfloat *v) { if(v) _glsTexCoord4f(v[0], v[1], v[2], v[3]); }
void APIENTRY glTexCoord4iv(const GLint *v) { if(v) _glsTexCoord4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glTexCoord4sv(const GLshort *v) { if(v) _glsTexCoord4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }

void APIENTRY glTexGend(GLenum coord, GLenum pname, GLdouble param) { (void)coord; (void)pname; (void)param; }
void APIENTRY glTexGenf(GLenum coord, GLenum pname, GLfloat param) { (void)coord; (void)pname; (void)param; }
void APIENTRY glTexGeni(GLenum coord, GLenum pname, GLint param) { (void)coord; (void)pname; (void)param; }
void APIENTRY glTexGendv(GLenum coord, GLenum pname, const GLdouble *params) { (void)coord; (void)pname; (void)params; }
void APIENTRY glTexGeniv(GLenum coord, GLenum pname, const GLint *params) { (void)coord; (void)pname; (void)params; }
void APIENTRY glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) { (void)coord; (void)pname; (void)params; }
void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) { (void)target; (void)pname; (void)param; }
void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) { (void)target; (void)pname; (void)param; }
void APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) { (void)target; (void)pname; (void)params; }
void APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *params) { (void)target; (void)pname; (void)params; }

void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z) { _glsTranslated(x, y, z); }
void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) { _glsTranslatef(x, y, z); }

void APIENTRY glVertex2d(GLdouble x, GLdouble y) { _glsVertex2f((float)x, (float)y); }
void APIENTRY glVertex2f(GLfloat x, GLfloat y) { _glsVertex2f(x, y); }
void APIENTRY glVertex2i(GLint x, GLint y) { _glsVertex2f((float)x, (float)y); }
void APIENTRY glVertex2s(GLshort x, GLshort y) { _glsVertex2f((float)x, (float)y); }
void APIENTRY glVertex3d(GLdouble x, GLdouble y, GLdouble z) { _glsVertex3f((float)x, (float)y, (float)z); }
void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) { _glsVertex3f(x, y, z); }
void APIENTRY glVertex3i(GLint x, GLint y, GLint z) { _glsVertex3f((float)x, (float)y, (float)z); }
void APIENTRY glVertex3s(GLshort x, GLshort y, GLshort z) { _glsVertex3f((float)x, (float)y, (float)z); }
void APIENTRY glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { _glsVertex4f((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { _glsVertex4f(x, y, z, w); }
void APIENTRY glVertex4i(GLint x, GLint y, GLint z, GLint w) { _glsVertex4f((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) { _glsVertex4f((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex2dv(const GLdouble *v) { if(v) _glsVertex2f((float)v[0], (float)v[1]); }
void APIENTRY glVertex2fv(const GLfloat *v) { if(v) _glsVertex2f(v[0], v[1]); }
void APIENTRY glVertex2iv(const GLint *v) { if(v) _glsVertex2f((float)v[0], (float)v[1]); }
void APIENTRY glVertex2sv(const GLshort *v) { if(v) _glsVertex2f((float)v[0], (float)v[1]); }
void APIENTRY glVertex3dv(const GLdouble *v) { if(v) _glsVertex3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glVertex3fv(const GLfloat *v) { if(v) _glsVertex3f(v[0], v[1], v[2]); }
void APIENTRY glVertex3iv(const GLint *v) { if(v) _glsVertex3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glVertex3sv(const GLshort *v) { if(v) _glsVertex3f((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glVertex4dv(const GLdouble *v) { if(v) _glsVertex4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glVertex4fv(const GLfloat *v) { if(v) _glsVertex4f(v[0], v[1], v[2], v[3]); }
void APIENTRY glVertex4iv(const GLint *v) { if(v) _glsVertex4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glVertex4sv(const GLshort *v) { if(v) _glsVertex4f((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }

void APIENTRY glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer) { (void)format; (void)stride; (void)pointer; }

/*---------------------------------------------------------------------------
 * Extension functions exported by opengl32.def
 *---------------------------------------------------------------------------*/

void APIENTRY glPolygonOffsetEXT(GLfloat factor, GLfloat bias) { _glsPolygonOffset(factor, bias); }
void APIENTRY glBlendEquationEXT(GLenum mode) { _glsBlendEquation(mode); }
void APIENTRY glBlendColorEXT(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { _glsBlendColor(r, g, b, a); }
void APIENTRY glVertexPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) { (void)size; (void)type; (void)stride; (void)count; (void)ptr; }
void APIENTRY glNormalPointerEXT(GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) { (void)type; (void)stride; (void)count; (void)ptr; }
void APIENTRY glColorPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) { (void)size; (void)type; (void)stride; (void)count; (void)ptr; }
void APIENTRY glIndexPointerEXT(GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) { (void)type; (void)stride; (void)count; (void)ptr; }
void APIENTRY glTexCoordPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) { (void)size; (void)type; (void)stride; (void)count; (void)ptr; }
void APIENTRY glEdgeFlagPointerEXT(GLsizei stride, GLsizei count, const GLboolean *ptr) { (void)stride; (void)count; (void)ptr; }
void APIENTRY glGetPointervEXT(GLenum pname, GLvoid **params) { (void)pname; if(params) *params = NULL; }
void APIENTRY glArrayElementEXT(GLint i) { (void)i; }
void APIENTRY glDrawArraysEXT(GLenum mode, GLint first, GLsizei count) { (void)mode; (void)first; (void)count; }
GLboolean APIENTRY glAreTexturesResidentEXT(GLsizei n, const GLuint *textures, GLboolean *residences) { return GL_TRUE; }
void APIENTRY glBindTextureEXT(GLenum target, GLuint texture) { _glsBindTexture(target, texture); }
void APIENTRY glDeleteTexturesEXT(GLsizei n, const GLuint *textures) { _glsDeleteTextures(n, textures); }
void APIENTRY glGenTexturesEXT(GLsizei n, GLuint *textures) {
	extern void APIENTRY glGenTextures(GLsizei n, GLuint *textures);
	glGenTextures(n, textures);
}
GLboolean APIENTRY glIsTextureEXT(GLuint texture) { return GL_FALSE; }
void APIENTRY glPrioritizeTexturesEXT(GLsizei n, const GLuint *textures, const GLfloat *priorities) { (void)n; (void)textures; (void)priorities; }
void APIENTRY glCopyTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) { (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)x; (void)y; (void)width; (void)height; }
void APIENTRY glTexImage3DEXT(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels) { (void)target; (void)level; (void)internalformat; (void)width; (void)height; (void)depth; (void)border; (void)format; (void)type; (void)pixels; }
void APIENTRY glTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels) { (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)type; (void)pixels; }

/* Color table EXT */
void APIENTRY glColorTableEXT(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table) { (void)target; (void)internalformat; (void)width; (void)format; (void)type; (void)table; }
void APIENTRY glColorSubTableEXT(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data) { (void)target; (void)start; (void)count; (void)format; (void)type; (void)data; }
void APIENTRY glGetColorTableEXT(GLenum target, GLenum format, GLenum type, GLvoid *data) { (void)target; (void)format; (void)type; (void)data; }
void APIENTRY glGetColorTableParameterivEXT(GLenum target, GLenum pname, GLint *params) { (void)target; (void)pname; if(params) *params = 0; }
void APIENTRY glGetColorTableParameterfvEXT(GLenum target, GLenum pname, GLfloat *params) { (void)target; (void)pname; if(params) *params = 0.0f; }

/* Point parameter EXT */
void APIENTRY glPointParameterfEXT(GLenum pname, GLfloat param) { _glsPointParameterf(pname, param); }
void APIENTRY glPointParameterfvEXT(GLenum pname, const GLfloat *params) { _glsPointParameterfv(pname, params); }

/* Lock arrays EXT */
void APIENTRY glLockArraysEXT(GLint first, GLsizei count) { (void)first; (void)count; }
void APIENTRY glUnlockArraysEXT(void) { }

/*---------------------------------------------------------------------------
 * ARB Multitexture exports
 *---------------------------------------------------------------------------*/

void APIENTRY glActiveTextureARB(GLenum texture) { _glsActiveTexture(texture); }
void APIENTRY glClientActiveTextureARB(GLenum texture) { _glsClientActiveTexture(texture); }

void APIENTRY glMultiTexCoord1dARB(GLenum target, GLdouble s) { _glsMultiTexCoord2fARB(target, (float)s, 0.0f); }
void APIENTRY glMultiTexCoord1dvARB(GLenum target, const GLdouble *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], 0.0f); }
void APIENTRY glMultiTexCoord1fARB(GLenum target, GLfloat s) { _glsMultiTexCoord2fARB(target, s, 0.0f); }
void APIENTRY glMultiTexCoord1fvARB(GLenum target, const GLfloat *v) { if(v) _glsMultiTexCoord2fARB(target, v[0], 0.0f); }
void APIENTRY glMultiTexCoord1iARB(GLenum target, GLint s) { _glsMultiTexCoord2fARB(target, (float)s, 0.0f); }
void APIENTRY glMultiTexCoord1ivARB(GLenum target, const GLint *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], 0.0f); }
void APIENTRY glMultiTexCoord1sARB(GLenum target, GLshort s) { _glsMultiTexCoord2fARB(target, (float)s, 0.0f); }
void APIENTRY glMultiTexCoord1svARB(GLenum target, const GLshort *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], 0.0f); }
void APIENTRY glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t) { _glsMultiTexCoord2fARB(target, (float)s, (float)t); }
void APIENTRY glMultiTexCoord2dvARB(GLenum target, const GLdouble *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], (float)v[1]); }
void APIENTRY glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) { _glsMultiTexCoord2fARB(target, s, t); }
void APIENTRY glMultiTexCoord2fvARB(GLenum target, const GLfloat *v) { if(v) _glsMultiTexCoord2fvARB(target, v); }
void APIENTRY glMultiTexCoord2iARB(GLenum target, GLint s, GLint t) { _glsMultiTexCoord2fARB(target, (float)s, (float)t); }
void APIENTRY glMultiTexCoord2ivARB(GLenum target, const GLint *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], (float)v[1]); }
void APIENTRY glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t) { _glsMultiTexCoord2fARB(target, (float)s, (float)t); }
void APIENTRY glMultiTexCoord2svARB(GLenum target, const GLshort *v) { if(v) _glsMultiTexCoord2fARB(target, (float)v[0], (float)v[1]); }
void APIENTRY glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r) { (void)target; (void)s; (void)t; (void)r; }
void APIENTRY glMultiTexCoord3dvARB(GLenum target, const GLdouble *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r) { (void)target; (void)s; (void)t; (void)r; }
void APIENTRY glMultiTexCoord3fvARB(GLenum target, const GLfloat *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r) { (void)target; (void)s; (void)t; (void)r; }
void APIENTRY glMultiTexCoord3ivARB(GLenum target, const GLint *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r) { (void)target; (void)s; (void)t; (void)r; }
void APIENTRY glMultiTexCoord3svARB(GLenum target, const GLshort *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) { (void)target; (void)s; (void)t; (void)r; (void)q; }
void APIENTRY glMultiTexCoord4dvARB(GLenum target, const GLdouble *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { (void)target; (void)s; (void)t; (void)r; (void)q; }
void APIENTRY glMultiTexCoord4fvARB(GLenum target, const GLfloat *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q) { (void)target; (void)s; (void)t; (void)r; (void)q; }
void APIENTRY glMultiTexCoord4ivARB(GLenum target, const GLint *v) { (void)target; (void)v; }
void APIENTRY glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) { (void)target; (void)s; (void)t; (void)r; (void)q; }
void APIENTRY glMultiTexCoord4svARB(GLenum target, const GLshort *v) { (void)target; (void)v; }

/*---------------------------------------------------------------------------
 * Mesa WindowPos exports
 *---------------------------------------------------------------------------*/

void APIENTRY glWindowPos2dMESA(GLdouble x, GLdouble y) { (void)x; (void)y; }
void APIENTRY glWindowPos2dvMESA(const GLdouble *v) { (void)v; }
void APIENTRY glWindowPos2fMESA(GLfloat x, GLfloat y) { (void)x; (void)y; }
void APIENTRY glWindowPos2fvMESA(const GLfloat *v) { (void)v; }
void APIENTRY glWindowPos2iMESA(GLint x, GLint y) { (void)x; (void)y; }
void APIENTRY glWindowPos2ivMESA(const GLint *v) { (void)v; }
void APIENTRY glWindowPos2sMESA(GLshort x, GLshort y) { (void)x; (void)y; }
void APIENTRY glWindowPos2svMESA(const GLshort *v) { (void)v; }
void APIENTRY glWindowPos3dMESA(GLdouble x, GLdouble y, GLdouble z) { (void)x; (void)y; (void)z; }
void APIENTRY glWindowPos3dvMESA(const GLdouble *v) { (void)v; }
void APIENTRY glWindowPos3fMESA(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void APIENTRY glWindowPos3fvMESA(const GLfloat *v) { (void)v; }
void APIENTRY glWindowPos3iMESA(GLint x, GLint y, GLint z) { (void)x; (void)y; (void)z; }
void APIENTRY glWindowPos3ivMESA(const GLint *v) { (void)v; }
void APIENTRY glWindowPos3sMESA(GLshort x, GLshort y, GLshort z) { (void)x; (void)y; (void)z; }
void APIENTRY glWindowPos3svMESA(const GLshort *v) { (void)v; }
void APIENTRY glWindowPos4dMESA(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glWindowPos4dvMESA(const GLdouble *v) { (void)v; }
void APIENTRY glWindowPos4fMESA(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glWindowPos4fvMESA(const GLfloat *v) { (void)v; }
void APIENTRY glWindowPos4iMESA(GLint x, GLint y, GLint z, GLint w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glWindowPos4ivMESA(const GLint *v) { (void)v; }
void APIENTRY glWindowPos4sMESA(GLshort x, GLshort y, GLshort z, GLshort w) { (void)x; (void)y; (void)z; (void)w; }
void APIENTRY glWindowPos4svMESA(const GLshort *v) { (void)v; }

void APIENTRY glResizeBuffersMESA(void) { }

/*---------------------------------------------------------------------------
 * Core GL functions that are in the .def file.
 * GLAD provides these as function pointers (glad_glXxx), but the .def
 * file needs actual exported function symbols. These stubs forward to
 * the GLAD function pointers at runtime if they are loaded, otherwise no-op.
 * For now they are simple stubs — the real GL context will handle these
 * via the GL46 modules.
 *---------------------------------------------------------------------------*/

/* Note: Many of these are also in GLAD core profile. The .def file
 * exports them by name. Since we don't include glad/gl.h here,
 * there's no macro conflict. */

GLboolean APIENTRY glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences) { return GL_TRUE; }
void APIENTRY glArrayElement(GLint i) { (void)i; }
void APIENTRY glBindTexture(GLenum target, GLuint texture) { _glsBindTexture(target, texture); }
void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) { _glsBlendFunc(sfactor, dfactor); }
void APIENTRY glClear(GLbitfield mask) { _glsClear(mask); }
void APIENTRY glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { _glsClearColor(r, g, b, a); }
void APIENTRY glClearDepth(GLdouble depth) { _glsClearDepth(depth); }
void APIENTRY glClearStencil(GLint s) { _glsClearStencil(s); }
void APIENTRY glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { _glsColorMask(r, g, b, a); }
void APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) { (void)size; (void)type; (void)stride; (void)pointer; }
void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) { (void)target; (void)level; (void)internalformat; (void)x; (void)y; (void)width; (void)border; }
void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) { (void)target; (void)level; (void)internalformat; (void)x; (void)y; (void)width; (void)height; (void)border; }
void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) { (void)target; (void)level; (void)xoffset; (void)x; (void)y; (void)width; }
void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) { (void)target; (void)level; (void)xoffset; (void)yoffset; (void)x; (void)y; (void)width; (void)height; }
void APIENTRY glCullFace(GLenum mode) { _glsCullFace(mode); }
void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures) { _glsDeleteTextures(n, textures); }
void APIENTRY glDepthFunc(GLenum func) { _glsDepthFunc(func); }
void APIENTRY glDepthMask(GLboolean flag) { _glsDepthMask(flag); }
void APIENTRY glDepthRange(GLdouble nearVal, GLdouble farVal) { _glsDepthRange(nearVal, farVal); }
void APIENTRY glDisable(GLenum cap) { _glsDisable(cap); }
void APIENTRY glDisableClientState(GLenum array) { (void)array; }
void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) { _glsDrawArrays(mode, first, count); }
void APIENTRY glDrawBuffer(GLenum mode) { _glsDrawBuffer(mode); }
void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) { _glsDrawElements(mode, count, type, indices); }
void APIENTRY glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer) { (void)stride; (void)pointer; }
void APIENTRY glEnable(GLenum cap) { _glsEnable(cap); }
void APIENTRY glEnableClientState(GLenum array) { (void)array; }
void APIENTRY glFinish(void) { }
void APIENTRY glFlush(void) { }
void APIENTRY glFrontFace(GLenum mode) { _glsFrontFace(mode); }
void APIENTRY glGenTextures(GLsizei n, GLuint *textures) {
	_glsGenTextures(n, textures);
}
void APIENTRY glGetBooleanv(GLenum pname, GLboolean *params) { _glsGetBooleanv(pname, params); }
void APIENTRY glGetDoublev(GLenum pname, GLdouble *params) { if(params) { float fv[16]; _glsGetFloatv(pname, fv); int i; for(i=0;i<16;i++) params[i]=(GLdouble)fv[i]; } }
GLenum APIENTRY glGetError(void) { return _glsGetError(); }
void APIENTRY glGetFloatv(GLenum pname, GLfloat *params) {
	_glsGetFloatv(pname, params);
}
void APIENTRY glGetIntegerv(GLenum pname, GLint *params) {
	_glsGetIntegerv(pname, params);
}
void APIENTRY glGetPointerv(GLenum pname, GLvoid **params) { (void)pname; if(params) *params = NULL; }
const GLubyte * APIENTRY glGetString(GLenum name) {
	extern const GLubyte* _gldGetStringGeneric(void *ctx, GLenum name);
	const GLubyte *result = _gldGetStringGeneric(NULL, name);
	{
		FILE *f = fopen("gldirect_diag.log", "a");
		if (f) { fprintf(f, "GL: glGetString(0x%X) -> \"%s\"\n", name, result ? (const char*)result : "NULL"); fflush(f); fclose(f); }
	}
	return result;
}
void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) { (void)target; (void)level; (void)format; (void)type; (void)pixels; }
void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) { (void)target; (void)level; (void)pname; if(params) *params = 0.0f; }
void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) { (void)target; (void)level; (void)pname; if(params) *params = 0; }
void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) { (void)target; (void)pname; if(params) *params = 0.0f; }
void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) { (void)target; (void)pname; if(params) *params = 0; }
void APIENTRY glHint(GLenum target, GLenum mode) { _glsHint(target, mode); }
void APIENTRY glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer) { (void)type; (void)stride; (void)pointer; }
GLboolean APIENTRY glIsEnabled(GLenum cap) { return _glsIsEnabled(cap); }
GLboolean APIENTRY glIsTexture(GLuint texture) { return GL_FALSE; }
void APIENTRY glLineWidth(GLfloat width) { _glsLineWidth(width); }
void APIENTRY glLogicOp(GLenum opcode) { _glsLogicOp(opcode); }
void APIENTRY glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) { (void)type; (void)stride; (void)pointer; }
void APIENTRY glPixelStoref(GLenum pname, GLfloat param) { _glsPixelStorei(pname, (int)param); }
void APIENTRY glPixelStorei(GLenum pname, GLint param) { _glsPixelStorei(pname, param); }
void APIENTRY glPointSize(GLfloat size) { _glsPointSize(size); }
void APIENTRY glPolygonMode(GLenum face, GLenum mode) { _glsPolygonMode(face, mode); }
void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) { _glsPolygonOffset(factor, units); }
void APIENTRY glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLfloat *priorities) { (void)n; (void)textures; (void)priorities; }
void APIENTRY glReadBuffer(GLenum mode) { _glsReadBuffer(mode); }
void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) { (void)x; (void)y; (void)width; (void)height; (void)format; (void)type; (void)pixels; }
void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { _glsScissor(x, y, width, height); }
void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) { _glsStencilFunc(func, ref, mask); }
void APIENTRY glStencilMask(GLuint mask) { _glsStencilMaskSeparate(0x0408, mask); }
void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { _glsStencilOp(fail, zfail, zpass); }
void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) { (void)size; (void)type; (void)stride; (void)pointer; }
void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels) { (void)target; (void)level; (void)internalformat; (void)width; (void)border; (void)format; (void)type; (void)pixels; }
void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) { _glsTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) { _glsTexParameterf(target, pname, param); }
void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) { _glsTexParameteri(target, pname, param); }
void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) { if(params) _glsTexParameterf(target, pname, params[0]); }
void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params) { if(params) _glsTexParameteri(target, pname, params[0]); }
void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels) { (void)target; (void)level; (void)xoffset; (void)width; (void)format; (void)type; (void)pixels; }
void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) { _glsTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels); }
void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) { (void)size; (void)type; (void)stride; (void)pointer; }
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { _glsViewport(x, y, width, height); }
