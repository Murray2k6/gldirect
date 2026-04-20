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

void APIENTRY glAccum(GLenum op, GLfloat value) {}
void APIENTRY glAlphaFunc(GLenum func, GLfloat ref) {}
void APIENTRY glBegin(GLenum mode) {}
void APIENTRY glBitmap(GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym, const GLubyte *bm) {}
void APIENTRY glCallList(GLuint list) {}
void APIENTRY glCallLists(GLsizei n, GLenum type, const GLvoid *lists) {}
void APIENTRY glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
void APIENTRY glClearIndex(GLfloat c) {}
void APIENTRY glClipPlane(GLenum plane, const GLdouble *equation) {}

void APIENTRY glColor3b(GLbyte r, GLbyte g, GLbyte b) {}
void APIENTRY glColor3d(GLdouble r, GLdouble g, GLdouble b) {}
void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {}
void APIENTRY glColor3i(GLint r, GLint g, GLint b) {}
void APIENTRY glColor3s(GLshort r, GLshort g, GLshort b) {}
void APIENTRY glColor3ub(GLubyte r, GLubyte g, GLubyte b) {}
void APIENTRY glColor3ui(GLuint r, GLuint g, GLuint b) {}
void APIENTRY glColor3us(GLushort r, GLushort g, GLushort b) {}
void APIENTRY glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a) {}
void APIENTRY glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) {}
void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
void APIENTRY glColor4i(GLint r, GLint g, GLint b, GLint a) {}
void APIENTRY glColor4s(GLshort r, GLshort g, GLshort b, GLshort a) {}
void APIENTRY glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {}
void APIENTRY glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a) {}
void APIENTRY glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) {}
void APIENTRY glColor3bv(const GLbyte *v) {}
void APIENTRY glColor3dv(const GLdouble *v) {}
void APIENTRY glColor3fv(const GLfloat *v) {}
void APIENTRY glColor3iv(const GLint *v) {}
void APIENTRY glColor3sv(const GLshort *v) {}
void APIENTRY glColor3ubv(const GLubyte *v) {}
void APIENTRY glColor3uiv(const GLuint *v) {}
void APIENTRY glColor3usv(const GLushort *v) {}
void APIENTRY glColor4bv(const GLbyte *v) {}
void APIENTRY glColor4dv(const GLdouble *v) {}
void APIENTRY glColor4fv(const GLfloat *v) {}
void APIENTRY glColor4iv(const GLint *v) {}
void APIENTRY glColor4sv(const GLshort *v) {}
void APIENTRY glColor4ubv(const GLubyte *v) {}
void APIENTRY glColor4uiv(const GLuint *v) {}
void APIENTRY glColor4usv(const GLushort *v) {}

void APIENTRY glColorMaterial(GLenum face, GLenum mode) {}
void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum type) {}
void APIENTRY glDeleteLists(GLuint list, GLsizei range) {}
void APIENTRY glDrawPixels(GLsizei w, GLsizei h, GLenum fmt, GLenum type, const GLvoid *pix) {}
void APIENTRY glEdgeFlag(GLboolean flag) {}
void APIENTRY glEdgeFlagv(const GLboolean *flag) {}
void APIENTRY glEnd(void) {}
void APIENTRY glEndList(void) {}

void APIENTRY glEvalCoord1d(GLdouble u) {}
void APIENTRY glEvalCoord1f(GLfloat u) {}
void APIENTRY glEvalCoord1dv(const GLdouble *u) {}
void APIENTRY glEvalCoord1fv(const GLfloat *u) {}
void APIENTRY glEvalCoord2d(GLdouble u, GLdouble v) {}
void APIENTRY glEvalCoord2f(GLfloat u, GLfloat v) {}
void APIENTRY glEvalCoord2dv(const GLdouble *u) {}
void APIENTRY glEvalCoord2fv(const GLfloat *u) {}
void APIENTRY glEvalPoint1(GLint i) {}
void APIENTRY glEvalPoint2(GLint i, GLint j) {}
void APIENTRY glEvalMesh1(GLenum mode, GLint i1, GLint i2) {}
void APIENTRY glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {}

void APIENTRY glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer) {}
void APIENTRY glFogf(GLenum pname, GLfloat param) {}
void APIENTRY glFogi(GLenum pname, GLint param) {}
void APIENTRY glFogfv(GLenum pname, const GLfloat *params) {}
void APIENTRY glFogiv(GLenum pname, const GLint *params) {}
void APIENTRY glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {}
GLuint APIENTRY glGenLists(GLsizei range) { return 0; }

void APIENTRY glGetClipPlane(GLenum plane, GLdouble *equation) {}
void APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat *params) {}
void APIENTRY glGetLightiv(GLenum light, GLenum pname, GLint *params) {}
void APIENTRY glGetMapdv(GLenum target, GLenum query, GLdouble *v) {}
void APIENTRY glGetMapfv(GLenum target, GLenum query, GLfloat *v) {}
void APIENTRY glGetMapiv(GLenum target, GLenum query, GLint *v) {}
void APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params) {}
void APIENTRY glGetMaterialiv(GLenum face, GLenum pname, GLint *params) {}
void APIENTRY glGetPixelMapfv(GLenum map, GLfloat *values) {}
void APIENTRY glGetPixelMapuiv(GLenum map, GLuint *values) {}
void APIENTRY glGetPixelMapusv(GLenum map, GLushort *values) {}
void APIENTRY glGetPolygonStipple(GLubyte *mask) {}
void APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params) {}
void APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint *params) {}
void APIENTRY glGetTexGeniv(GLenum coord, GLenum pname, GLint *params) {}
void APIENTRY glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params) {}
void APIENTRY glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params) {}

void APIENTRY glIndexd(GLdouble c) {}
void APIENTRY glIndexf(GLfloat c) {}
void APIENTRY glIndexi(GLint c) {}
void APIENTRY glIndexs(GLshort c) {}
void APIENTRY glIndexub(GLubyte c) {}
void APIENTRY glIndexdv(const GLdouble *c) {}
void APIENTRY glIndexfv(const GLfloat *c) {}
void APIENTRY glIndexiv(const GLint *c) {}
void APIENTRY glIndexsv(const GLshort *c) {}
void APIENTRY glIndexubv(const GLubyte *c) {}
void APIENTRY glIndexMask(GLuint mask) {}
void APIENTRY glInitNames(void) {}
GLboolean APIENTRY glIsList(GLuint list) { return GL_FALSE; }

void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param) {}
void APIENTRY glLighti(GLenum light, GLenum pname, GLint param) {}
void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params) {}
void APIENTRY glLightiv(GLenum light, GLenum pname, const GLint *params) {}
void APIENTRY glLightModelf(GLenum pname, GLfloat param) {}
void APIENTRY glLightModeli(GLenum pname, GLint param) {}
void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params) {}
void APIENTRY glLightModeliv(GLenum pname, const GLint *params) {}
void APIENTRY glLineStipple(GLint factor, GLushort pattern) {}
void APIENTRY glListBase(GLuint base) {}
void APIENTRY glLoadIdentity(void) {}
void APIENTRY glLoadMatrixd(const GLdouble *m) {}
void APIENTRY glLoadMatrixf(const GLfloat *m) {}
void APIENTRY glLoadName(GLuint name) {}

void APIENTRY glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points) {}
void APIENTRY glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points) {}
void APIENTRY glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points) {}
void APIENTRY glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) {}
void APIENTRY glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {}
void APIENTRY glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) {}
void APIENTRY glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {}
void APIENTRY glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) {}
void APIENTRY glMaterialf(GLenum face, GLenum pname, GLfloat param) {}
void APIENTRY glMateriali(GLenum face, GLenum pname, GLint param) {}
void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {}
void APIENTRY glMaterialiv(GLenum face, GLenum pname, const GLint *params) {}
void APIENTRY glMatrixMode(GLenum mode) {}
void APIENTRY glMultMatrixd(const GLdouble *m) {}
void APIENTRY glMultMatrixf(const GLfloat *m) {}
void APIENTRY glNewList(GLuint list, GLenum mode) {}

void APIENTRY glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) {}
void APIENTRY glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) {}
void APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {}
void APIENTRY glNormal3i(GLint nx, GLint ny, GLint nz) {}
void APIENTRY glNormal3s(GLshort nx, GLshort ny, GLshort nz) {}
void APIENTRY glNormal3bv(const GLbyte *v) {}
void APIENTRY glNormal3dv(const GLdouble *v) {}
void APIENTRY glNormal3fv(const GLfloat *v) {}
void APIENTRY glNormal3iv(const GLint *v) {}
void APIENTRY glNormal3sv(const GLshort *v) {}

void APIENTRY glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {}
void APIENTRY glPassThrough(GLfloat token) {}
void APIENTRY glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values) {}
void APIENTRY glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values) {}
void APIENTRY glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values) {}
void APIENTRY glPixelTransferf(GLenum pname, GLfloat param) {}
void APIENTRY glPixelTransferi(GLenum pname, GLint param) {}
void APIENTRY glPixelZoom(GLfloat xfactor, GLfloat yfactor) {}
void APIENTRY glPolygonStipple(const GLubyte *mask) {}
void APIENTRY glPopAttrib(void) {}
void APIENTRY glPopClientAttrib(void) {}
void APIENTRY glPopMatrix(void) {}
void APIENTRY glPopName(void) {}
void APIENTRY glPushAttrib(GLbitfield mask) {}
void APIENTRY glPushClientAttrib(GLbitfield mask) {}
void APIENTRY glPushMatrix(void) {}
void APIENTRY glPushName(GLuint name) {}

void APIENTRY glRasterPos2d(GLdouble x, GLdouble y) {}
void APIENTRY glRasterPos2f(GLfloat x, GLfloat y) {}
void APIENTRY glRasterPos2i(GLint x, GLint y) {}
void APIENTRY glRasterPos2s(GLshort x, GLshort y) {}
void APIENTRY glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) {}
void APIENTRY glRasterPos3i(GLint x, GLint y, GLint z) {}
void APIENTRY glRasterPos3s(GLshort x, GLshort y, GLshort z) {}
void APIENTRY glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {}
void APIENTRY glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {}
void APIENTRY glRasterPos4i(GLint x, GLint y, GLint z, GLint w) {}
void APIENTRY glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {}
void APIENTRY glRasterPos2dv(const GLdouble *v) {}
void APIENTRY glRasterPos2fv(const GLfloat *v) {}
void APIENTRY glRasterPos2iv(const GLint *v) {}
void APIENTRY glRasterPos2sv(const GLshort *v) {}
void APIENTRY glRasterPos3dv(const GLdouble *v) {}
void APIENTRY glRasterPos3fv(const GLfloat *v) {}
void APIENTRY glRasterPos3iv(const GLint *v) {}
void APIENTRY glRasterPos3sv(const GLshort *v) {}
void APIENTRY glRasterPos4dv(const GLdouble *v) {}
void APIENTRY glRasterPos4fv(const GLfloat *v) {}
void APIENTRY glRasterPos4iv(const GLint *v) {}
void APIENTRY glRasterPos4sv(const GLshort *v) {}

void APIENTRY glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {}
void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {}
void APIENTRY glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {}
void APIENTRY glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {}
void APIENTRY glRectdv(const GLdouble *v1, const GLdouble *v2) {}
void APIENTRY glRectfv(const GLfloat *v1, const GLfloat *v2) {}
void APIENTRY glRectiv(const GLint *v1, const GLint *v2) {}
void APIENTRY glRectsv(const GLshort *v1, const GLshort *v2) {}
GLint APIENTRY glRenderMode(GLenum mode) { return 0; }
void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {}
void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) {}
void APIENTRY glSelectBuffer(GLsizei size, GLuint *buffer) {}
void APIENTRY glShadeModel(GLenum mode) {}

void APIENTRY glTexCoord1d(GLdouble s) {}
void APIENTRY glTexCoord1f(GLfloat s) {}
void APIENTRY glTexCoord1i(GLint s) {}
void APIENTRY glTexCoord1s(GLshort s) {}
void APIENTRY glTexCoord2d(GLdouble s, GLdouble t) {}
void APIENTRY glTexCoord2f(GLfloat s, GLfloat t) {}
void APIENTRY glTexCoord2i(GLint s, GLint t) {}
void APIENTRY glTexCoord2s(GLshort s, GLshort t) {}
void APIENTRY glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) {}
void APIENTRY glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {}
void APIENTRY glTexCoord3i(GLint s, GLint t, GLint r) {}
void APIENTRY glTexCoord3s(GLshort s, GLshort t, GLshort r) {}
void APIENTRY glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) {}
void APIENTRY glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {}
void APIENTRY glTexCoord4i(GLint s, GLint t, GLint r, GLint q) {}
void APIENTRY glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {}
void APIENTRY glTexCoord1dv(const GLdouble *v) {}
void APIENTRY glTexCoord1fv(const GLfloat *v) {}
void APIENTRY glTexCoord1iv(const GLint *v) {}
void APIENTRY glTexCoord1sv(const GLshort *v) {}
void APIENTRY glTexCoord2dv(const GLdouble *v) {}
void APIENTRY glTexCoord2fv(const GLfloat *v) {}
void APIENTRY glTexCoord2iv(const GLint *v) {}
void APIENTRY glTexCoord2sv(const GLshort *v) {}
void APIENTRY glTexCoord3dv(const GLdouble *v) {}
void APIENTRY glTexCoord3fv(const GLfloat *v) {}
void APIENTRY glTexCoord3iv(const GLint *v) {}
void APIENTRY glTexCoord3sv(const GLshort *v) {}
void APIENTRY glTexCoord4dv(const GLdouble *v) {}
void APIENTRY glTexCoord4fv(const GLfloat *v) {}
void APIENTRY glTexCoord4iv(const GLint *v) {}
void APIENTRY glTexCoord4sv(const GLshort *v) {}

void APIENTRY glTexGend(GLenum coord, GLenum pname, GLdouble param) {}
void APIENTRY glTexGenf(GLenum coord, GLenum pname, GLfloat param) {}
void APIENTRY glTexGeni(GLenum coord, GLenum pname, GLint param) {}
void APIENTRY glTexGendv(GLenum coord, GLenum pname, const GLdouble *params) {}
void APIENTRY glTexGeniv(GLenum coord, GLenum pname, const GLint *params) {}
void APIENTRY glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) {}
void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {}
void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) {}
void APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {}
void APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *params) {}

void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) {}

void APIENTRY glVertex2d(GLdouble x, GLdouble y) {}
void APIENTRY glVertex2f(GLfloat x, GLfloat y) {}
void APIENTRY glVertex2i(GLint x, GLint y) {}
void APIENTRY glVertex2s(GLshort x, GLshort y) {}
void APIENTRY glVertex3d(GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {}
void APIENTRY glVertex3i(GLint x, GLint y, GLint z) {}
void APIENTRY glVertex3s(GLshort x, GLshort y, GLshort z) {}
void APIENTRY glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {}
void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {}
void APIENTRY glVertex4i(GLint x, GLint y, GLint z, GLint w) {}
void APIENTRY glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {}
void APIENTRY glVertex2dv(const GLdouble *v) {}
void APIENTRY glVertex2fv(const GLfloat *v) {}
void APIENTRY glVertex2iv(const GLint *v) {}
void APIENTRY glVertex2sv(const GLshort *v) {}
void APIENTRY glVertex3dv(const GLdouble *v) {}
void APIENTRY glVertex3fv(const GLfloat *v) {}
void APIENTRY glVertex3iv(const GLint *v) {}
void APIENTRY glVertex3sv(const GLshort *v) {}
void APIENTRY glVertex4dv(const GLdouble *v) {}
void APIENTRY glVertex4fv(const GLfloat *v) {}
void APIENTRY glVertex4iv(const GLint *v) {}
void APIENTRY glVertex4sv(const GLshort *v) {}

void APIENTRY glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer) {}

/*---------------------------------------------------------------------------
 * Extension functions exported by opengl32.def
 *---------------------------------------------------------------------------*/

void APIENTRY glPolygonOffsetEXT(GLfloat factor, GLfloat bias) {}
void APIENTRY glBlendEquationEXT(GLenum mode) {}
void APIENTRY glBlendColorEXT(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
void APIENTRY glVertexPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) {}
void APIENTRY glNormalPointerEXT(GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) {}
void APIENTRY glColorPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) {}
void APIENTRY glIndexPointerEXT(GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) {}
void APIENTRY glTexCoordPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *ptr) {}
void APIENTRY glEdgeFlagPointerEXT(GLsizei stride, GLsizei count, const GLboolean *ptr) {}
void APIENTRY glGetPointervEXT(GLenum pname, GLvoid **params) {}
void APIENTRY glArrayElementEXT(GLint i) {}
void APIENTRY glDrawArraysEXT(GLenum mode, GLint first, GLsizei count) {}
GLboolean APIENTRY glAreTexturesResidentEXT(GLsizei n, const GLuint *textures, GLboolean *residences) { return GL_TRUE; }
void APIENTRY glBindTextureEXT(GLenum target, GLuint texture) {}
void APIENTRY glDeleteTexturesEXT(GLsizei n, const GLuint *textures) {}
void APIENTRY glGenTexturesEXT(GLsizei n, GLuint *textures) {}
GLboolean APIENTRY glIsTextureEXT(GLuint texture) { return GL_FALSE; }
void APIENTRY glPrioritizeTexturesEXT(GLsizei n, const GLuint *textures, const GLfloat *priorities) {}
void APIENTRY glCopyTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {}
void APIENTRY glTexImage3DEXT(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {}
void APIENTRY glTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels) {}

/* Color table EXT */
void APIENTRY glColorTableEXT(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table) {}
void APIENTRY glColorSubTableEXT(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data) {}
void APIENTRY glGetColorTableEXT(GLenum target, GLenum format, GLenum type, GLvoid *data) {}
void APIENTRY glGetColorTableParameterivEXT(GLenum target, GLenum pname, GLint *params) {}
void APIENTRY glGetColorTableParameterfvEXT(GLenum target, GLenum pname, GLfloat *params) {}

/* Point parameter EXT */
void APIENTRY glPointParameterfEXT(GLenum pname, GLfloat param) {}
void APIENTRY glPointParameterfvEXT(GLenum pname, const GLfloat *params) {}

/* Lock arrays EXT */
void APIENTRY glLockArraysEXT(GLint first, GLsizei count) {}
void APIENTRY glUnlockArraysEXT(void) {}

/*---------------------------------------------------------------------------
 * ARB Multitexture exports
 *---------------------------------------------------------------------------*/

void APIENTRY glActiveTextureARB(GLenum texture) {}
void APIENTRY glClientActiveTextureARB(GLenum texture) {}

void APIENTRY glMultiTexCoord1dARB(GLenum target, GLdouble s) {}
void APIENTRY glMultiTexCoord1dvARB(GLenum target, const GLdouble *v) {}
void APIENTRY glMultiTexCoord1fARB(GLenum target, GLfloat s) {}
void APIENTRY glMultiTexCoord1fvARB(GLenum target, const GLfloat *v) {}
void APIENTRY glMultiTexCoord1iARB(GLenum target, GLint s) {}
void APIENTRY glMultiTexCoord1ivARB(GLenum target, const GLint *v) {}
void APIENTRY glMultiTexCoord1sARB(GLenum target, GLshort s) {}
void APIENTRY glMultiTexCoord1svARB(GLenum target, const GLshort *v) {}
void APIENTRY glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t) {}
void APIENTRY glMultiTexCoord2dvARB(GLenum target, const GLdouble *v) {}
void APIENTRY glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) {}
void APIENTRY glMultiTexCoord2fvARB(GLenum target, const GLfloat *v) {}
void APIENTRY glMultiTexCoord2iARB(GLenum target, GLint s, GLint t) {}
void APIENTRY glMultiTexCoord2ivARB(GLenum target, const GLint *v) {}
void APIENTRY glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t) {}
void APIENTRY glMultiTexCoord2svARB(GLenum target, const GLshort *v) {}
void APIENTRY glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r) {}
void APIENTRY glMultiTexCoord3dvARB(GLenum target, const GLdouble *v) {}
void APIENTRY glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r) {}
void APIENTRY glMultiTexCoord3fvARB(GLenum target, const GLfloat *v) {}
void APIENTRY glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r) {}
void APIENTRY glMultiTexCoord3ivARB(GLenum target, const GLint *v) {}
void APIENTRY glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r) {}
void APIENTRY glMultiTexCoord3svARB(GLenum target, const GLshort *v) {}
void APIENTRY glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {}
void APIENTRY glMultiTexCoord4dvARB(GLenum target, const GLdouble *v) {}
void APIENTRY glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {}
void APIENTRY glMultiTexCoord4fvARB(GLenum target, const GLfloat *v) {}
void APIENTRY glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q) {}
void APIENTRY glMultiTexCoord4ivARB(GLenum target, const GLint *v) {}
void APIENTRY glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) {}
void APIENTRY glMultiTexCoord4svARB(GLenum target, const GLshort *v) {}

/*---------------------------------------------------------------------------
 * Mesa WindowPos exports
 *---------------------------------------------------------------------------*/

void APIENTRY glWindowPos2dMESA(GLdouble x, GLdouble y) {}
void APIENTRY glWindowPos2dvMESA(const GLdouble *v) {}
void APIENTRY glWindowPos2fMESA(GLfloat x, GLfloat y) {}
void APIENTRY glWindowPos2fvMESA(const GLfloat *v) {}
void APIENTRY glWindowPos2iMESA(GLint x, GLint y) {}
void APIENTRY glWindowPos2ivMESA(const GLint *v) {}
void APIENTRY glWindowPos2sMESA(GLshort x, GLshort y) {}
void APIENTRY glWindowPos2svMESA(const GLshort *v) {}
void APIENTRY glWindowPos3dMESA(GLdouble x, GLdouble y, GLdouble z) {}
void APIENTRY glWindowPos3dvMESA(const GLdouble *v) {}
void APIENTRY glWindowPos3fMESA(GLfloat x, GLfloat y, GLfloat z) {}
void APIENTRY glWindowPos3fvMESA(const GLfloat *v) {}
void APIENTRY glWindowPos3iMESA(GLint x, GLint y, GLint z) {}
void APIENTRY glWindowPos3ivMESA(const GLint *v) {}
void APIENTRY glWindowPos3sMESA(GLshort x, GLshort y, GLshort z) {}
void APIENTRY glWindowPos3svMESA(const GLshort *v) {}
void APIENTRY glWindowPos4dMESA(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {}
void APIENTRY glWindowPos4dvMESA(const GLdouble *v) {}
void APIENTRY glWindowPos4fMESA(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {}
void APIENTRY glWindowPos4fvMESA(const GLfloat *v) {}
void APIENTRY glWindowPos4iMESA(GLint x, GLint y, GLint z, GLint w) {}
void APIENTRY glWindowPos4ivMESA(const GLint *v) {}
void APIENTRY glWindowPos4sMESA(GLshort x, GLshort y, GLshort z, GLshort w) {}
void APIENTRY glWindowPos4svMESA(const GLshort *v) {}

void APIENTRY glResizeBuffersMESA(void) {}

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
void APIENTRY glArrayElement(GLint i) {}
void APIENTRY glBindTexture(GLenum target, GLuint texture) {}
void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {}
void APIENTRY glClear(GLbitfield mask) {}
void APIENTRY glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
void APIENTRY glClearDepth(GLdouble depth) {}
void APIENTRY glClearStencil(GLint s) {}
void APIENTRY glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {}
void APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {}
void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {}
void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {}
void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {}
void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {}
void APIENTRY glCullFace(GLenum mode) {}
void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures) {}
void APIENTRY glDepthFunc(GLenum func) {}
void APIENTRY glDepthMask(GLboolean flag) {}
void APIENTRY glDepthRange(GLdouble nearVal, GLdouble farVal) {}
void APIENTRY glDisable(GLenum cap) {}
void APIENTRY glDisableClientState(GLenum array) {}
void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {}
void APIENTRY glDrawBuffer(GLenum mode) {}
void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {}
void APIENTRY glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer) {}
void APIENTRY glEnable(GLenum cap) {}
void APIENTRY glEnableClientState(GLenum array) {}
void APIENTRY glFinish(void) {}
void APIENTRY glFlush(void) {}
void APIENTRY glFrontFace(GLenum mode) {}
void APIENTRY glGenTextures(GLsizei n, GLuint *textures) {}
void APIENTRY glGetBooleanv(GLenum pname, GLboolean *params) {}
void APIENTRY glGetDoublev(GLenum pname, GLdouble *params) {}
GLenum APIENTRY glGetError(void) { return 0; /* GL_NO_ERROR */ }
void APIENTRY glGetFloatv(GLenum pname, GLfloat *params) {}
void APIENTRY glGetIntegerv(GLenum pname, GLint *params) {}
void APIENTRY glGetPointerv(GLenum pname, GLvoid **params) {}
const GLubyte * APIENTRY glGetString(GLenum name) { return (const GLubyte *)""; }
void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) {}
void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {}
void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {}
void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {}
void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {}
void APIENTRY glHint(GLenum target, GLenum mode) {}
void APIENTRY glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {}
GLboolean APIENTRY glIsEnabled(GLenum cap) { return GL_FALSE; }
GLboolean APIENTRY glIsTexture(GLuint texture) { return GL_FALSE; }
void APIENTRY glLineWidth(GLfloat width) {}
void APIENTRY glLogicOp(GLenum opcode) {}
void APIENTRY glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {}
void APIENTRY glPixelStoref(GLenum pname, GLfloat param) {}
void APIENTRY glPixelStorei(GLenum pname, GLint param) {}
void APIENTRY glPointSize(GLfloat size) {}
void APIENTRY glPolygonMode(GLenum face, GLenum mode) {}
void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) {}
void APIENTRY glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLfloat *priorities) {}
void APIENTRY glReadBuffer(GLenum mode) {}
void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {}
void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {}
void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) {}
void APIENTRY glStencilMask(GLuint mask) {}
void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {}
void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {}
void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {}
void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {}
void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {}
void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {}
void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {}
void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {}
void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels) {}
void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {}
void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {}
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {}
