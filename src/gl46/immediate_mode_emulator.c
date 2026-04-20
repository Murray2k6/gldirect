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
*********************************************************************************/

#include "immediate_mode_emulator.h"
#include "error_handler.h"
#include "fixed_function_emulator.h"
#include "gld_log.h"
#include <math.h>
#include <string.h>

/*---------------------- Macros and type definitions ----------------------*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Attribute layout locations matching the fixed-function emulator:
 *   0 = position, 1 = normal, 2 = color, 4 = texcoord0 */
#define GLD_IM_ATTRIB_POSITION  0
#define GLD_IM_ATTRIB_COLOR     2
#define GLD_IM_ATTRIB_NORMAL    1
#define GLD_IM_ATTRIB_TEXCOORD  4

/*---------------------- Static module state ----------------------*/

/*
 * Vertex accumulation buffer — fixed-size array of floats.
 * Each vertex is GLD_IM_FLOATS_PER_VERTEX (12) floats:
 *   [0..2]  position  (x, y, z)
 *   [3..6]  color     (r, g, b, a)
 *   [7..9]  normal    (nx, ny, nz)
 *   [10..11] texcoord (s, t)
 */
static float s_vertexBuffer[GLD_IM_MAX_VERTICES * GLD_IM_FLOATS_PER_VERTEX];

/* Number of complete vertices accumulated so far */
static int s_vertexCount = 0;

/* TRUE if we are between gldImBegin() and gldImEnd() */
static BOOL s_inBeginEnd = FALSE;

/* Primitive mode passed to gldImBegin() */
static GLenum s_primitiveMode = GL_TRIANGLES;

/* Current vertex attribute state — copied into each vertex on gldImVertex* */
static float s_curColor[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
static float s_curNormal[3]  = { 0.0f, 0.0f, 1.0f };
static float s_curTexCoord[2] = { 0.0f, 0.0f };

/* Streaming VAO/VBO for immediate-mode rendering */
static GLuint s_imVAO = 0;
static GLuint s_imVBO = 0;
static GLsizei s_imVBOCapacity = 0;

/* ===================================================================
 * Matrix stack state
 * =================================================================== */

/* Current matrix mode */
static GLenum s_matrixMode = GL_MODELVIEW;

/* Matrix stacks */
static float s_modelviewStack[GLD_IM_MODELVIEW_STACK_DEPTH][16];
static int   s_modelviewStackTop = 0;

static float s_projectionStack[GLD_IM_PROJECTION_STACK_DEPTH][16];
static int   s_projectionStackTop = 0;

static float s_textureStack[GLD_IM_TEXTURE_STACK_DEPTH][16];
static int   s_textureStackTop = 0;

/* Current matrices (top of each stack) */
static float s_modelviewMatrix[16];
static float s_projectionMatrix[16];
static float s_textureMatrix[16];

/*---------------------- Internal helper functions ----------------------*/

/*
 * Set a 4x4 matrix to the identity matrix (column-major).
 */
static void sIdentityMatrix(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

/*
 * Multiply two 4x4 column-major matrices: result = a * b.
 * result may alias neither a nor b.
 */
static void sMultMatrix(float *result, const float *a, const float *b)
{
    int i, j, k;
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++) {
                sum += a[k * 4 + i] * b[j * 4 + k];
            }
            result[j * 4 + i] = sum;
        }
    }
}

/*
 * Get a pointer to the current matrix based on s_matrixMode.
 */
static float* sGetCurrentMatrix(void)
{
    switch (s_matrixMode) {
    case GL_PROJECTION:
        return s_projectionMatrix;
    case GL_TEXTURE:
        return s_textureMatrix;
    case GL_MODELVIEW:
    default:
        return s_modelviewMatrix;
    }
}

/*
 * Emit a single vertex into the converted triangle buffer.
 * Used by the quad/polygon conversion routines.
 */
static void sCopyVertex(float *dst, const float *src)
{
    memcpy(dst, src, GLD_IM_FLOATS_PER_VERTEX * sizeof(float));
}

/*
 * Ensure the streaming VAO/VBO are created.
 */
static void sEnsureVAO(void)
{
    if (s_imVAO == 0) {
        glGenVertexArrays(1, &s_imVAO);
        GLD_CHECK_GL("glGenVertexArrays", "ImEnd");
    }
    if (s_imVBO == 0) {
        glGenBuffers(1, &s_imVBO);
        GLD_CHECK_GL("glGenBuffers", "ImEnd");
    }
}

/*
 * Upload vertex data and configure the VAO attributes.
 */
static void sUploadAndDraw(const float *data, int vertexCount, GLenum mode)
{
    GLsizeiptr dataSize;

    if (vertexCount <= 0)
        return;

    sEnsureVAO();

    glBindVertexArray(s_imVAO);
    GLD_CHECK_GL("glBindVertexArray", "ImEnd");

    glBindBuffer(GL_ARRAY_BUFFER, s_imVBO);
    GLD_CHECK_GL("glBindBuffer", "ImEnd");

    dataSize = (GLsizeiptr)(vertexCount * GLD_IM_FLOATS_PER_VERTEX * sizeof(float));

    /* Buffer orphaning for streaming */
    if (s_imVBOCapacity < dataSize) {
        glBufferData(GL_ARRAY_BUFFER, dataSize, NULL, GL_STREAM_DRAW);
        GLD_CHECK_GL("glBufferData(alloc)", "ImEnd");
        s_imVBOCapacity = (GLsizei)dataSize;
    } else {
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)s_imVBOCapacity,
                     NULL, GL_STREAM_DRAW);
        GLD_CHECK_GL("glBufferData(orphan)", "ImEnd");
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, data);
    GLD_CHECK_GL("glBufferSubData", "ImEnd");

    /* Configure vertex attributes:
     *   location 0: position  — 3 floats at offset 0
     *   location 2: color     — 4 floats at offset 12
     *   location 1: normal    — 3 floats at offset 28
     *   location 4: texcoord  — 2 floats at offset 40
     */
    glVertexAttribPointer(GLD_IM_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE,
                          GLD_IM_VERTEX_SIZE, (const void*)0);
    glEnableVertexAttribArray(GLD_IM_ATTRIB_POSITION);

    glVertexAttribPointer(GLD_IM_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE,
                          GLD_IM_VERTEX_SIZE, (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(GLD_IM_ATTRIB_COLOR);

    glVertexAttribPointer(GLD_IM_ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE,
                          GLD_IM_VERTEX_SIZE, (const void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(GLD_IM_ATTRIB_NORMAL);

    glVertexAttribPointer(GLD_IM_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                          GLD_IM_VERTEX_SIZE, (const void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(GLD_IM_ATTRIB_TEXCOORD);

    GLD_CHECK_GL("glVertexAttribPointer", "ImEnd");

    /* Issue the draw call */
    glDrawArrays(mode, 0, vertexCount);
    GLD_CHECK_GL("glDrawArrays", "ImEnd");

    /* Unbind */
    glBindVertexArray(0);
}

/* ===================================================================
 * Immediate-mode vertex accumulation (Task 17.1)
 * =================================================================== */

void gldImBegin(GLenum mode)
{
    if (s_inBeginEnd) {
        gldLogPrintf(GLDLOG_WARN,
            "gldImBegin: called while already in Begin/End block");
        return;
    }

    s_primitiveMode = mode;
    s_vertexCount = 0;
    s_inBeginEnd = TRUE;
}

void gldImEnd(void)
{
    if (!s_inBeginEnd) {
        gldLogPrintf(GLDLOG_WARN,
            "gldImEnd: called without matching gldImBegin");
        return;
    }

    s_inBeginEnd = FALSE;

    if (s_vertexCount == 0)
        return;

    switch (s_primitiveMode) {
    case GL_QUADS: {
        /*
         * Convert quads to triangles: for each quad (v0,v1,v2,v3),
         * emit two triangles: (v0,v1,v2) and (v0,v2,v3).
         */
        int numQuads = s_vertexCount / 4;
        int triVertCount = numQuads * 6;
        /* Use a temporary buffer for the converted triangles.
         * We can reuse the tail of s_vertexBuffer if there's room,
         * but it's safer to use a separate static buffer. */
        static float s_triBuffer[GLD_IM_MAX_VERTICES * GLD_IM_FLOATS_PER_VERTEX];
        int q, outIdx = 0;

        if (triVertCount > GLD_IM_MAX_VERTICES) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImEnd: GL_QUADS conversion exceeds max vertices (%d)",
                triVertCount);
            return;
        }

        for (q = 0; q < numQuads; q++) {
            const float *v0 = &s_vertexBuffer[(q * 4 + 0) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v1 = &s_vertexBuffer[(q * 4 + 1) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v2 = &s_vertexBuffer[(q * 4 + 2) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v3 = &s_vertexBuffer[(q * 4 + 3) * GLD_IM_FLOATS_PER_VERTEX];

            /* Triangle 1: v0, v1, v2 */
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v0); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v1); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v2); outIdx++;

            /* Triangle 2: v0, v2, v3 */
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v0); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v2); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v3); outIdx++;
        }

        sUploadAndDraw(s_triBuffer, triVertCount, GL_TRIANGLES);
        break;
    }

    case GL_QUAD_STRIP: {
        /*
         * Convert quad strip to triangles.
         * Quad strip: vertices come in pairs (v0,v1), (v2,v3), ...
         * Each quad is (v[2i], v[2i+1], v[2i+3], v[2i+2]).
         * Emit two triangles per quad:
         *   (v[2i], v[2i+1], v[2i+3]) and (v[2i], v[2i+3], v[2i+2])
         */
        int numQuads = (s_vertexCount / 2) - 1;
        int triVertCount;
        static float s_triBuffer[GLD_IM_MAX_VERTICES * GLD_IM_FLOATS_PER_VERTEX];
        int q, outIdx = 0;

        if (numQuads <= 0)
            return;

        triVertCount = numQuads * 6;
        if (triVertCount > GLD_IM_MAX_VERTICES) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImEnd: GL_QUAD_STRIP conversion exceeds max vertices (%d)",
                triVertCount);
            return;
        }

        for (q = 0; q < numQuads; q++) {
            const float *v0 = &s_vertexBuffer[(q * 2 + 0) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v1 = &s_vertexBuffer[(q * 2 + 1) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v2 = &s_vertexBuffer[(q * 2 + 2) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v3 = &s_vertexBuffer[(q * 2 + 3) * GLD_IM_FLOATS_PER_VERTEX];

            /* Triangle 1: v0, v1, v3 */
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v0); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v1); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v3); outIdx++;

            /* Triangle 2: v0, v3, v2 */
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v0); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v3); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v2); outIdx++;
        }

        sUploadAndDraw(s_triBuffer, triVertCount, GL_TRIANGLES);
        break;
    }

    case GL_POLYGON: {
        /*
         * Convert polygon to triangle fan: (v0,v1,v2), (v0,v2,v3), ...
         * GL_TRIANGLE_FAN is still in core profile, so we can use it
         * directly. But for safety, convert to explicit triangles.
         */
        int numTris = s_vertexCount - 2;
        int triVertCount;
        static float s_triBuffer[GLD_IM_MAX_VERTICES * GLD_IM_FLOATS_PER_VERTEX];
        int t, outIdx = 0;

        if (numTris <= 0)
            return;

        triVertCount = numTris * 3;
        if (triVertCount > GLD_IM_MAX_VERTICES) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImEnd: GL_POLYGON conversion exceeds max vertices (%d)",
                triVertCount);
            return;
        }

        for (t = 0; t < numTris; t++) {
            const float *v0 = &s_vertexBuffer[0];
            const float *v1 = &s_vertexBuffer[(t + 1) * GLD_IM_FLOATS_PER_VERTEX];
            const float *v2 = &s_vertexBuffer[(t + 2) * GLD_IM_FLOATS_PER_VERTEX];

            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v0); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v1); outIdx++;
            sCopyVertex(&s_triBuffer[outIdx * GLD_IM_FLOATS_PER_VERTEX], v2); outIdx++;
        }

        sUploadAndDraw(s_triBuffer, triVertCount, GL_TRIANGLES);
        break;
    }

    default:
        /* Standard primitive modes (GL_TRIANGLES, GL_TRIANGLE_STRIP,
         * GL_TRIANGLE_FAN, GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP,
         * GL_POINTS) — pass through directly. */
        sUploadAndDraw(s_vertexBuffer, s_vertexCount, s_primitiveMode);
        break;
    }

    s_vertexCount = 0;
}

/*
 * Internal: finalize the current vertex by writing position and
 * copying the current color/normal/texcoord state into the buffer.
 */
static void sEmitVertex(float x, float y, float z)
{
    float *v;

    if (!s_inBeginEnd) {
        gldLogPrintf(GLDLOG_WARN,
            "gldImVertex: called outside Begin/End block");
        return;
    }

    if (s_vertexCount >= GLD_IM_MAX_VERTICES) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldImVertex: vertex buffer overflow (%d vertices)",
            GLD_IM_MAX_VERTICES);
        return;
    }

    v = &s_vertexBuffer[s_vertexCount * GLD_IM_FLOATS_PER_VERTEX];

    /* Position (3 floats) */
    v[0] = x;
    v[1] = y;
    v[2] = z;

    /* Color (4 floats) */
    v[3] = s_curColor[0];
    v[4] = s_curColor[1];
    v[5] = s_curColor[2];
    v[6] = s_curColor[3];

    /* Normal (3 floats) */
    v[7] = s_curNormal[0];
    v[8] = s_curNormal[1];
    v[9] = s_curNormal[2];

    /* Texcoord (2 floats) */
    v[10] = s_curTexCoord[0];
    v[11] = s_curTexCoord[1];

    s_vertexCount++;
}

void gldImVertex2f(float x, float y)
{
    sEmitVertex(x, y, 0.0f);
}

void gldImVertex3f(float x, float y, float z)
{
    sEmitVertex(x, y, z);
}

void gldImColor3f(float r, float g, float b)
{
    s_curColor[0] = r;
    s_curColor[1] = g;
    s_curColor[2] = b;
    s_curColor[3] = 1.0f;
}

void gldImColor4f(float r, float g, float b, float a)
{
    s_curColor[0] = r;
    s_curColor[1] = g;
    s_curColor[2] = b;
    s_curColor[3] = a;
}

void gldImColor3ub(unsigned char r, unsigned char g, unsigned char b)
{
    s_curColor[0] = r / 255.0f;
    s_curColor[1] = g / 255.0f;
    s_curColor[2] = b / 255.0f;
    s_curColor[3] = 1.0f;
}

void gldImColor4ub(unsigned char r, unsigned char g, unsigned char b,
                   unsigned char a)
{
    s_curColor[0] = r / 255.0f;
    s_curColor[1] = g / 255.0f;
    s_curColor[2] = b / 255.0f;
    s_curColor[3] = a / 255.0f;
}

void gldImNormal3f(float nx, float ny, float nz)
{
    s_curNormal[0] = nx;
    s_curNormal[1] = ny;
    s_curNormal[2] = nz;
}

void gldImTexCoord2f(float s, float t)
{
    s_curTexCoord[0] = s;
    s_curTexCoord[1] = t;
}


/* ===================================================================
 * Legacy matrix stack (Task 17.2)
 * =================================================================== */

/*
 * Module initialization — called implicitly on first use.
 * Sets all matrices to identity.
 */
static BOOL s_matricesInitialized = FALSE;

static void sInitMatrices(void)
{
    if (s_matricesInitialized)
        return;

    sIdentityMatrix(s_modelviewMatrix);
    sIdentityMatrix(s_projectionMatrix);
    sIdentityMatrix(s_textureMatrix);

    s_modelviewStackTop = 0;
    s_projectionStackTop = 0;
    s_textureStackTop = 0;

    s_matricesInitialized = TRUE;
}

void gldImMatrixMode(GLenum mode)
{
    sInitMatrices();

    switch (mode) {
    case GL_MODELVIEW:
    case GL_PROJECTION:
    case GL_TEXTURE:
        s_matrixMode = mode;
        break;
    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldImMatrixMode: unsupported mode 0x%04X", (unsigned)mode);
        break;
    }
}

void gldImLoadIdentity(void)
{
    sInitMatrices();
    sIdentityMatrix(sGetCurrentMatrix());
}

void gldImLoadMatrixf(const float *m)
{
    sInitMatrices();

    if (!m) {
        gldLogPrintf(GLDLOG_WARN, "gldImLoadMatrixf: NULL matrix pointer");
        return;
    }

    memcpy(sGetCurrentMatrix(), m, 16 * sizeof(float));
}

void gldImMultMatrixf(const float *m)
{
    float temp[16];
    float *cur;

    sInitMatrices();

    if (!m) {
        gldLogPrintf(GLDLOG_WARN, "gldImMultMatrixf: NULL matrix pointer");
        return;
    }

    cur = sGetCurrentMatrix();
    sMultMatrix(temp, cur, m);
    memcpy(cur, temp, 16 * sizeof(float));
}

void gldImPushMatrix(void)
{
    sInitMatrices();

    switch (s_matrixMode) {
    case GL_MODELVIEW:
        if (s_modelviewStackTop >= GLD_IM_MODELVIEW_STACK_DEPTH) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPushMatrix: modelview stack overflow (depth %d)",
                GLD_IM_MODELVIEW_STACK_DEPTH);
            return;
        }
        memcpy(s_modelviewStack[s_modelviewStackTop], s_modelviewMatrix,
               16 * sizeof(float));
        s_modelviewStackTop++;
        break;

    case GL_PROJECTION:
        if (s_projectionStackTop >= GLD_IM_PROJECTION_STACK_DEPTH) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPushMatrix: projection stack overflow (depth %d)",
                GLD_IM_PROJECTION_STACK_DEPTH);
            return;
        }
        memcpy(s_projectionStack[s_projectionStackTop], s_projectionMatrix,
               16 * sizeof(float));
        s_projectionStackTop++;
        break;

    case GL_TEXTURE:
        if (s_textureStackTop >= GLD_IM_TEXTURE_STACK_DEPTH) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPushMatrix: texture stack overflow (depth %d)",
                GLD_IM_TEXTURE_STACK_DEPTH);
            return;
        }
        memcpy(s_textureStack[s_textureStackTop], s_textureMatrix,
               16 * sizeof(float));
        s_textureStackTop++;
        break;

    default:
        break;
    }
}

void gldImPopMatrix(void)
{
    sInitMatrices();

    switch (s_matrixMode) {
    case GL_MODELVIEW:
        if (s_modelviewStackTop <= 0) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPopMatrix: modelview stack underflow");
            return;
        }
        s_modelviewStackTop--;
        memcpy(s_modelviewMatrix, s_modelviewStack[s_modelviewStackTop],
               16 * sizeof(float));
        break;

    case GL_PROJECTION:
        if (s_projectionStackTop <= 0) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPopMatrix: projection stack underflow");
            return;
        }
        s_projectionStackTop--;
        memcpy(s_projectionMatrix, s_projectionStack[s_projectionStackTop],
               16 * sizeof(float));
        break;

    case GL_TEXTURE:
        if (s_textureStackTop <= 0) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldImPopMatrix: texture stack underflow");
            return;
        }
        s_textureStackTop--;
        memcpy(s_textureMatrix, s_textureStack[s_textureStackTop],
               16 * sizeof(float));
        break;

    default:
        break;
    }
}

void gldImRotatef(float angle, float x, float y, float z)
{
    float rad, c, s, len, ic;
    float rot[16];

    sInitMatrices();

    /* Normalize the axis */
    len = (float)sqrt(x * x + y * y + z * z);
    if (len < 1.0e-6f) {
        gldLogPrintf(GLDLOG_WARN,
            "gldImRotatef: zero-length rotation axis");
        return;
    }
    x /= len;
    y /= len;
    z /= len;

    rad = angle * (float)(M_PI / 180.0);
    c = (float)cos(rad);
    s = (float)sin(rad);
    ic = 1.0f - c;

    /* Build rotation matrix (column-major) */
    rot[0]  = x * x * ic + c;
    rot[1]  = y * x * ic + z * s;
    rot[2]  = z * x * ic - y * s;
    rot[3]  = 0.0f;

    rot[4]  = x * y * ic - z * s;
    rot[5]  = y * y * ic + c;
    rot[6]  = z * y * ic + x * s;
    rot[7]  = 0.0f;

    rot[8]  = x * z * ic + y * s;
    rot[9]  = y * z * ic - x * s;
    rot[10] = z * z * ic + c;
    rot[11] = 0.0f;

    rot[12] = 0.0f;
    rot[13] = 0.0f;
    rot[14] = 0.0f;
    rot[15] = 1.0f;

    gldImMultMatrixf(rot);
}

void gldImScalef(float x, float y, float z)
{
    float scale[16];

    sInitMatrices();

    memset(scale, 0, sizeof(scale));
    scale[0]  = x;
    scale[5]  = y;
    scale[10] = z;
    scale[15] = 1.0f;

    gldImMultMatrixf(scale);
}

void gldImTranslatef(float x, float y, float z)
{
    float trans[16];

    sInitMatrices();

    sIdentityMatrix(trans);
    trans[12] = x;
    trans[13] = y;
    trans[14] = z;

    gldImMultMatrixf(trans);
}

/* ===================================================================
 * Legacy lighting/material stubs (Task 17.2)
 * =================================================================== */

void gldImLightfv(GLenum light, GLenum pname, const float *params)
{
    /*
     * Stub: forward light parameters to the Fixed_Function_Emulator.
     * The fixed-function emulator maintains its own light state arrays.
     * For now, log the call for debugging purposes.
     */
    (void)light;
    (void)pname;
    (void)params;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldImLightfv: light=0x%04X pname=0x%04X (stub — forwarded to FF emulator)",
        (unsigned)light, (unsigned)pname);
}

void gldImMaterialfv(GLenum face, GLenum pname, const float *params)
{
    /*
     * Stub: forward material parameters to the Fixed_Function_Emulator.
     * The fixed-function emulator maintains material state as uniforms.
     * For now, log the call for debugging purposes.
     */
    (void)face;
    (void)pname;
    (void)params;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldImMaterialfv: face=0x%04X pname=0x%04X (stub — forwarded to FF emulator)",
        (unsigned)face, (unsigned)pname);
}
