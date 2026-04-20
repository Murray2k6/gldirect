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
* Description:  Immediate-mode (glBegin/glEnd) emulation and legacy matrix
*               stack for OpenGL 4.6 core profile.
*
*               OpenGL core profile removes all immediate-mode entry points
*               (glBegin, glEnd, glVertex*, glColor*, etc.) and the built-in
*               matrix stack (glMatrixMode, glPushMatrix, glPopMatrix, etc.).
*               This module provides replacement functions that accumulate
*               vertex data into a streaming VBO and maintain a software
*               matrix stack, allowing legacy GL 1.x applications to work
*               through GLDirect's core-profile context.
*
*               Vertex format is fixed at 12 floats (48 bytes) per vertex:
*                 position (3f) + color (4f) + normal (3f) + texcoord (2f)
*
*********************************************************************************/

#ifndef __GL46_IMMEDIATE_MODE_EMULATOR_H
#define __GL46_IMMEDIATE_MODE_EMULATOR_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/*
 * Maximum number of vertices in the immediate-mode accumulation buffer.
 * 64K vertices * 48 bytes = 3 MB — fits comfortably in GPU memory.
 */
#define GLD_IM_MAX_VERTICES     65536

/*
 * Number of floats per vertex in the interleaved buffer.
 * Layout: position(3) + color(4) + normal(3) + texcoord(2) = 12
 */
#define GLD_IM_FLOATS_PER_VERTEX    12

/*
 * Byte size of a single vertex.
 */
#define GLD_IM_VERTEX_SIZE      (GLD_IM_FLOATS_PER_VERTEX * sizeof(float))

/*
 * Legacy primitive types removed from core profile.
 * Defined locally since they are not in the GL 4.6 core headers.
 */
#ifndef GL_QUADS
#define GL_QUADS                0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP           0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON              0x0009
#endif

/*
 * Legacy matrix mode constants (removed from core profile headers).
 */
#ifndef GL_MODELVIEW
#define GL_MODELVIEW            0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION           0x1701
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE              0x1702
#endif

/*
 * Matrix stack depths matching classic OpenGL minimums.
 */
#define GLD_IM_MODELVIEW_STACK_DEPTH    32
#define GLD_IM_PROJECTION_STACK_DEPTH   2
#define GLD_IM_TEXTURE_STACK_DEPTH      2

/*
 * Legacy light/material parameter constants (removed from core profile).
 */
#ifndef GL_LIGHT0
#define GL_LIGHT0               0x4000
#endif
#ifndef GL_LIGHT1
#define GL_LIGHT1               0x4001
#endif
#ifndef GL_LIGHT7
#define GL_LIGHT7               0x4007
#endif
#ifndef GL_AMBIENT
#define GL_AMBIENT              0x1200
#endif
#ifndef GL_DIFFUSE
#define GL_DIFFUSE              0x1201
#endif
#ifndef GL_SPECULAR
#define GL_SPECULAR             0x1202
#endif
#ifndef GL_POSITION
#define GL_POSITION             0x1203
#endif
#ifndef GL_EMISSION
#define GL_EMISSION             0x1600
#endif
#ifndef GL_SHININESS
#define GL_SHININESS            0x1601
#endif
#ifndef GL_FRONT
#define GL_FRONT                0x0404
#endif
#ifndef GL_BACK
#define GL_BACK                 0x0405
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK       0x0408
#endif

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Immediate-mode vertex accumulation (Task 17.1)
 * =================================================================== */

/*
 * Begin accumulating vertices for a primitive.
 *
 * Records the primitive mode and resets the vertex count.
 * GL_QUADS, GL_QUAD_STRIP, and GL_POLYGON are accepted and
 * converted to triangles at gldImEnd() time.
 *
 * Parameters:
 *   mode — GL primitive mode (GL_TRIANGLES, GL_QUADS, etc.)
 */
void gldImBegin(GLenum mode);

/*
 * Finalize the current primitive batch.
 *
 * Uploads accumulated vertex data into a streaming VAO/VBO,
 * configures vertex attributes, and issues glDrawArrays.
 * For GL_QUADS and GL_POLYGON, vertices are converted to
 * GL_TRIANGLES before upload.
 */
void gldImEnd(void);

/*
 * Append a 2D vertex position and finalize the current vertex.
 * Z is set to 0.0.
 */
void gldImVertex2f(float x, float y);

/*
 * Append a 3D vertex position and finalize the current vertex.
 */
void gldImVertex3f(float x, float y, float z);

/*
 * Set the current color (RGB, alpha defaults to 1.0).
 */
void gldImColor3f(float r, float g, float b);

/*
 * Set the current color (RGBA).
 */
void gldImColor4f(float r, float g, float b, float a);

/*
 * Set the current color from unsigned bytes (RGB, alpha = 255).
 */
void gldImColor3ub(unsigned char r, unsigned char g, unsigned char b);

/*
 * Set the current color from unsigned bytes (RGBA).
 */
void gldImColor4ub(unsigned char r, unsigned char g, unsigned char b,
                   unsigned char a);

/*
 * Set the current normal vector.
 */
void gldImNormal3f(float nx, float ny, float nz);

/*
 * Set the current texture coordinate (2D).
 */
void gldImTexCoord2f(float s, float t);

/* ===================================================================
 * Legacy matrix stack (Task 17.2)
 * =================================================================== */

/*
 * Set the current matrix mode.
 *
 * Parameters:
 *   mode — GL_MODELVIEW, GL_PROJECTION, or GL_TEXTURE
 */
void gldImMatrixMode(GLenum mode);

/*
 * Replace the current matrix with the identity matrix.
 */
void gldImLoadIdentity(void);

/*
 * Replace the current matrix with the given 4x4 matrix.
 *
 * Parameters:
 *   m — pointer to 16 floats in column-major order.
 */
void gldImLoadMatrixf(const float *m);

/*
 * Post-multiply the current matrix by the given 4x4 matrix.
 *
 * Parameters:
 *   m — pointer to 16 floats in column-major order.
 */
void gldImMultMatrixf(const float *m);

/*
 * Push the current matrix onto the stack.
 * Logs an error if the stack is full.
 */
void gldImPushMatrix(void);

/*
 * Pop the top matrix from the stack and make it current.
 * Logs an error if the stack is empty.
 */
void gldImPopMatrix(void);

/*
 * Apply a rotation to the current matrix.
 *
 * Parameters:
 *   angle — rotation angle in degrees.
 *   x, y, z — axis of rotation.
 */
void gldImRotatef(float angle, float x, float y, float z);

/*
 * Apply a scaling transformation to the current matrix.
 */
void gldImScalef(float x, float y, float z);

/*
 * Apply a translation to the current matrix.
 */
void gldImTranslatef(float x, float y, float z);

/* ===================================================================
 * Legacy lighting/material stubs (Task 17.2)
 * =================================================================== */

/*
 * Set a light parameter.
 * Forwards to Fixed_Function_Emulator state.
 *
 * Parameters:
 *   light — GL_LIGHT0 through GL_LIGHT7
 *   pname — parameter name (GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR,
 *           GL_POSITION, etc.)
 *   params — pointer to parameter value(s).
 */
void gldImLightfv(GLenum light, GLenum pname, const float *params);

/*
 * Set a material parameter.
 * Forwards to Fixed_Function_Emulator state.
 *
 * Parameters:
 *   face  — GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK
 *   pname — parameter name (GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR,
 *           GL_EMISSION, GL_SHININESS, etc.)
 *   params — pointer to parameter value(s).
 */
void gldImMaterialfv(GLenum face, GLenum pname, const float *params);

#ifdef  __cplusplus
}
#endif

#endif
