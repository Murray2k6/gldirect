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
* Description:  VAO/VBO management and DX9 FVF decoding.
*               Translates DX9 Flexible Vertex Format bitmasks into
*               OpenGL vertex attribute descriptors and manages Vertex
*               Array Objects and Vertex Buffer Objects for geometry
*               submission.
*
*********************************************************************************/

#define STRICT
#include <windows.h>
#include <string.h>

#include <glad/gl.h>
#include "buffer_manager.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************

/*****************************************************************************
 * gldDeclTypeToGL
 *
 * Map a D3DDECLTYPE value to its OpenGL type, component count, byte size,
 * and normalisation flag.
 *
 * Returns TRUE on success, FALSE for unsupported types.
 *****************************************************************************/
static BOOL gldDeclTypeToGL(BYTE declType, GLenum *glType,
                            int *componentCount, int *byteSize,
                            GLboolean *normalized)
{
    switch (declType) {
    case D3DDECLTYPE_FLOAT1:
        *glType         = GL_FLOAT;
        *componentCount = 1;
        *byteSize       = 1 * sizeof(float);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_FLOAT2:
        *glType         = GL_FLOAT;
        *componentCount = 2;
        *byteSize       = 2 * sizeof(float);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_FLOAT3:
        *glType         = GL_FLOAT;
        *componentCount = 3;
        *byteSize       = 3 * sizeof(float);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_FLOAT4:
        *glType         = GL_FLOAT;
        *componentCount = 4;
        *byteSize       = 4 * sizeof(float);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_D3DCOLOR:
        *glType         = GL_UNSIGNED_BYTE;
        *componentCount = 4;
        *byteSize       = 4 * sizeof(GLubyte);
        *normalized     = GL_TRUE;
        return TRUE;
    case D3DDECLTYPE_UBYTE4:
        *glType         = GL_UNSIGNED_BYTE;
        *componentCount = 4;
        *byteSize       = 4 * sizeof(GLubyte);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_SHORT2:
        *glType         = GL_SHORT;
        *componentCount = 2;
        *byteSize       = 2 * sizeof(short);
        *normalized     = GL_FALSE;
        return TRUE;
    case D3DDECLTYPE_SHORT4:
        *glType         = GL_SHORT;
        *componentCount = 4;
        *byteSize       = 4 * sizeof(short);
        *normalized     = GL_FALSE;
        return TRUE;
    default:
        return FALSE;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldUsageToAttribLocation
 *
 * Map a D3DDECLUSAGE + UsageIndex pair to a GLSL vertex attribute
 * location, matching the FVF convention used by gldDecodeFVF.
 *
 * Returns the attribute location, or -1 for unsupported usages.
 *****************************************************************************/
static int gldUsageToAttribLocation(BYTE usage, BYTE usageIndex)
{
    switch (usage) {
    case D3DDECLUSAGE_POSITION:
    case D3DDECLUSAGE_POSITIONT:
        return 0;
    case D3DDECLUSAGE_NORMAL:
        return 1;
    case D3DDECLUSAGE_COLOR:
        /* COLOR index 0 → location 2, COLOR index 1 → location 3 */
        if (usageIndex <= 1)
            return 2 + usageIndex;
        return -1;
    case D3DDECLUSAGE_TEXCOORD:
        /* TEXCOORD index N → location 4+N (up to 8 sets) */
        if (usageIndex < 8)
            return 4 + usageIndex;
        return -1;
    case D3DDECLUSAGE_BLENDWEIGHT:
        return 12;
    case D3DDECLUSAGE_BLENDINDICES:
        return 13;
    case D3DDECLUSAGE_TANGENT:
        return 14;
    case D3DDECLUSAGE_BINORMAL:
        return 15;
    default:
        return -1;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldTexCoordComponentCount
 *
 * Helper to convert the 2-bit texture coordinate size code from the FVF
 * into the actual number of float components.
 *
 * FVF encoding (per D3D9 SDK):
 *   0 = D3DFVF_TEXTUREFORMAT2 → 2 floats (default)
 *   1 = D3DFVF_TEXTUREFORMAT3 → 3 floats
 *   2 = D3DFVF_TEXTUREFORMAT4 → 4 floats
 *   3 = D3DFVF_TEXTUREFORMAT1 → 1 float
 *****************************************************************************/
static int gldTexCoordComponentCount(int sizeCode)
{
    switch (sizeCode) {
    case D3DFVF_TEXTUREFORMAT2: return 2;   /* 0 → 2D (default) */
    case D3DFVF_TEXTUREFORMAT3: return 3;   /* 1 → 3D */
    case D3DFVF_TEXTUREFORMAT4: return 4;   /* 2 → 4D */
    case D3DFVF_TEXTUREFORMAT1: return 1;   /* 3 → 1D */
    default:                    return 2;   /* fallback to 2D */
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldDecodeFVF
 *
 * Parse a DX9 FVF bitmask and produce an array of vertex attribute
 * descriptors.  The attributes are emitted in the order they appear in
 * the vertex data (position first, then normal, diffuse, specular,
 * texture coordinates).
 *
 * Attribute location mapping (matches GLSL layout qualifiers):
 *   Location 0  — Position  (XYZ: 3 floats, XYZRHW: 4 floats)
 *   Location 1  — Normal    (3 floats)
 *   Location 2  — Diffuse   (4 unsigned bytes, normalised)
 *   Location 3  — Specular  (4 unsigned bytes, normalised)
 *   Locations 4–11 — Texture coordinates (float count per
 *                    D3DFVF_TEXCOORDSIZE, default 2)
 *****************************************************************************/
void gldDecodeFVF(DWORD fvf, GLD_vertexAttrib *attribs,
                  int *attribCount, int *vertexSize)
{
    int count  = 0;
    int offset = 0;
    int numTex;
    int i;

    if (!attribs || !attribCount || !vertexSize) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldDecodeFVF: NULL output parameter");
        return;
    }

    memset(attribs, 0, sizeof(GLD_vertexAttrib) * GLD_MAX_VERTEX_ATTRIBS);

    /* ----------------------------------------------------------------
     * Position — location 0
     * D3DFVF_XYZ:   untransformed position (x, y, z) — 3 floats
     * D3DFVF_XYZRHW: transformed position (x, y, z, rhw) — 4 floats
     * ---------------------------------------------------------------- */
    if (fvf & D3DFVF_XYZRHW) {
        attribs[count].attribIndex    = 0;
        attribs[count].componentCount = 4;
        attribs[count].glType         = GL_FLOAT;
        attribs[count].normalized     = GL_FALSE;
        attribs[count].offset         = offset;
        offset += 4 * sizeof(float);
        count++;
    } else if (fvf & D3DFVF_XYZ) {
        attribs[count].attribIndex    = 0;
        attribs[count].componentCount = 3;
        attribs[count].glType         = GL_FLOAT;
        attribs[count].normalized     = GL_FALSE;
        attribs[count].offset         = offset;
        offset += 3 * sizeof(float);
        count++;
    }

    /* ----------------------------------------------------------------
     * Normal — location 1
     * D3DFVF_NORMAL: vertex normal (nx, ny, nz) — 3 floats
     * ---------------------------------------------------------------- */
    if (fvf & D3DFVF_NORMAL) {
        attribs[count].attribIndex    = 1;
        attribs[count].componentCount = 3;
        attribs[count].glType         = GL_FLOAT;
        attribs[count].normalized     = GL_FALSE;
        attribs[count].offset         = offset;
        offset += 3 * sizeof(float);
        count++;
    }

    /* ----------------------------------------------------------------
     * Diffuse color — location 2
     * D3DFVF_DIFFUSE: ARGB packed DWORD — 4 unsigned bytes, normalised
     * ---------------------------------------------------------------- */
    if (fvf & D3DFVF_DIFFUSE) {
        attribs[count].attribIndex    = 2;
        attribs[count].componentCount = 4;
        attribs[count].glType         = GL_UNSIGNED_BYTE;
        attribs[count].normalized     = GL_TRUE;
        attribs[count].offset         = offset;
        offset += 4 * sizeof(GLubyte);
        count++;
    }

    /* ----------------------------------------------------------------
     * Specular color — location 3
     * D3DFVF_SPECULAR: ARGB packed DWORD — 4 unsigned bytes, normalised
     * ---------------------------------------------------------------- */
    if (fvf & D3DFVF_SPECULAR) {
        attribs[count].attribIndex    = 3;
        attribs[count].componentCount = 4;
        attribs[count].glType         = GL_UNSIGNED_BYTE;
        attribs[count].normalized     = GL_TRUE;
        attribs[count].offset         = offset;
        offset += 4 * sizeof(GLubyte);
        count++;
    }

    /* ----------------------------------------------------------------
     * Texture coordinates — locations 4 through 11
     *
     * The number of texture coordinate sets is encoded in bits [11:8]
     * of the FVF code.  The dimensionality of each set is encoded in
     * the upper 16 bits via D3DFVF_TEXCOORDSIZE.
     * ---------------------------------------------------------------- */
    numTex = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    if (numTex > 8)
        numTex = 8;

    for (i = 0; i < numTex; i++) {
        int sizeCode   = GLD_FVF_TEXCOORDSIZE(fvf, i);
        int components = gldTexCoordComponentCount(sizeCode);

        attribs[count].attribIndex    = (GLuint)(4 + i);
        attribs[count].componentCount = components;
        attribs[count].glType         = GL_FLOAT;
        attribs[count].normalized     = GL_FALSE;
        attribs[count].offset         = offset;
        offset += components * sizeof(float);
        count++;
    }

    /* ----------------------------------------------------------------
     * Fill in the stride field for all attributes (equals total vertex
     * size) and write out the results.
     * ---------------------------------------------------------------- */
    for (i = 0; i < count; i++) {
        attribs[i].stride = offset;
    }

    *attribCount = count;
    *vertexSize  = offset;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldDecodeFVF: FVF=0x%08X → %d attribs, vertexSize=%d bytes",
        fvf, count, offset);
}

// ***********************************************************************

/*****************************************************************************
 * gldCreateVAO
 *
 * Create a VAO with a VBO containing the supplied vertex data.
 *
 * Steps:
 *   1. Generate and bind a VAO via glGenVertexArrays / glBindVertexArray.
 *   2. Generate and bind a VBO via glGenBuffers / glBindBuffer(GL_ARRAY_BUFFER).
 *   3. Upload vertex data via glBufferData with GL_STATIC_DRAW.
 *   4. Configure each vertex attribute via glVertexAttribPointer and
 *      glEnableVertexAttribArray.
 *
 * Returns the VAO id (GLuint), or 0 on failure.
 *****************************************************************************/
GLuint gldCreateVAO(void *ctx, const void *vertexData,
                    int vertexCount, int vertexSize,
                    const GLD_vertexAttrib *attribs, int attribCount)
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizeiptr bufferSize;
    int i;

    (void)ctx;  /* reserved for future per-context tracking */

    if (!vertexData || vertexCount <= 0 || vertexSize <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateVAO: invalid vertex data (data=%p, count=%d, size=%d)",
            vertexData, vertexCount, vertexSize);
        return 0;
    }

    if (!attribs || attribCount <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateVAO: invalid attrib array (attribs=%p, count=%d)",
            attribs, attribCount);
        return 0;
    }

    /* ----------------------------------------------------------------
     * 1. Create and bind the VAO.
     * ---------------------------------------------------------------- */
    glGenVertexArrays(1, &vao);
    GLD_CHECK_GL("glGenVertexArrays", "CreateVAO");
    if (vao == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateVAO: glGenVertexArrays returned 0");
        return 0;
    }

    glBindVertexArray(vao);
    GLD_CHECK_GL("glBindVertexArray", "CreateVAO");

    /* ----------------------------------------------------------------
     * 2. Create and bind the VBO.
     * ---------------------------------------------------------------- */
    glGenBuffers(1, &vbo);
    GLD_CHECK_GL("glGenBuffers", "CreateVAO");
    if (vbo == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateVAO: glGenBuffers returned 0");
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        return 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLD_CHECK_GL("glBindBuffer", "CreateVAO");

    /* ----------------------------------------------------------------
     * 3. Upload vertex data with GL_STATIC_DRAW.
     * ---------------------------------------------------------------- */
    bufferSize = (GLsizeiptr)vertexCount * (GLsizeiptr)vertexSize;
    glBufferData(GL_ARRAY_BUFFER, bufferSize, vertexData, GL_STATIC_DRAW);
    GLD_CHECK_GL("glBufferData", "CreateVAO");

    /* ----------------------------------------------------------------
     * 4. Configure vertex attributes.
     * ---------------------------------------------------------------- */
    for (i = 0; i < attribCount; i++) {
        const GLD_vertexAttrib *a = &attribs[i];

        glVertexAttribPointer(
            a->attribIndex,
            a->componentCount,
            a->glType,
            a->normalized,
            vertexSize,
            (const void *)(intptr_t)a->offset
        );
        GLD_CHECK_GL("glVertexAttribPointer", "CreateVAO");

        glEnableVertexAttribArray(a->attribIndex);
        GLD_CHECK_GL("glEnableVertexAttribArray", "CreateVAO");
    }

    /* Unbind the VAO (leave VBO bound to it via the VAO state). */
    glBindVertexArray(0);

    gldLogPrintf(GLDLOG_DEBUG,
        "gldCreateVAO: created VAO=%u, VBO=%u (%d verts, %d bytes each, %d attribs)",
        vao, vbo, vertexCount, vertexSize, attribCount);

    return vao;
}

// ***********************************************************************

/*****************************************************************************
 * gldParseVertexDeclaration
 *
 * Parse a DX9 vertex declaration (D3DVERTEXELEMENT9 array) and produce
 * an array of vertex attribute descriptors.  The element array is
 * terminated by the D3DDECL_END sentinel (Stream=0xFF, Type=17).
 *
 * Each element's D3DDECLTYPE is mapped to a GL type and component count,
 * and each element's D3DDECLUSAGE + UsageIndex is mapped to a GLSL
 * attribute location matching the FVF convention.
 *
 * The vertex size is computed as the maximum of (Offset + element byte
 * size) across all elements, which gives the tightly-packed stride.
 *****************************************************************************/
void gldParseVertexDeclaration(const void *elements,
                               GLD_vertexAttrib *attribs,
                               int *attribCount, int *vertexSize)
{
    const GLD_D3DVERTEXELEMENT9 *elem;
    int count  = 0;
    int maxEnd = 0;
    int i;

    if (!elements || !attribs || !attribCount || !vertexSize) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldParseVertexDeclaration: NULL parameter");
        return;
    }

    memset(attribs, 0, sizeof(GLD_vertexAttrib) * GLD_MAX_VERTEX_ATTRIBS);

    elem = (const GLD_D3DVERTEXELEMENT9 *)elements;

    while (!GLD_IS_DECL_END(elem)) {
        GLenum    glType;
        int       componentCount;
        int       byteSize;
        GLboolean normalized;
        int       location;

        if (count >= GLD_MAX_VERTEX_ATTRIBS) {
            gldLogPrintf(GLDLOG_WARN,
                "gldParseVertexDeclaration: too many elements, max %d",
                GLD_MAX_VERTEX_ATTRIBS);
            break;
        }

        /* Map the D3DDECLTYPE to GL type info */
        if (!gldDeclTypeToGL(elem->Type, &glType, &componentCount,
                             &byteSize, &normalized)) {
            gldLogPrintf(GLDLOG_WARN,
                "gldParseVertexDeclaration: unsupported D3DDECLTYPE %d, skipping",
                (int)elem->Type);
            elem++;
            continue;
        }

        /* Map the D3DDECLUSAGE + UsageIndex to an attribute location */
        location = gldUsageToAttribLocation(elem->Usage, elem->UsageIndex);
        if (location < 0) {
            gldLogPrintf(GLDLOG_WARN,
                "gldParseVertexDeclaration: unsupported usage %d index %d, skipping",
                (int)elem->Usage, (int)elem->UsageIndex);
            elem++;
            continue;
        }

        attribs[count].attribIndex    = (GLuint)location;
        attribs[count].componentCount = componentCount;
        attribs[count].glType         = glType;
        attribs[count].normalized     = normalized;
        attribs[count].offset         = (GLsizei)elem->Offset;

        /* Track the end of the furthest element for vertex size */
        if ((int)elem->Offset + byteSize > maxEnd)
            maxEnd = (int)elem->Offset + byteSize;

        count++;
        elem++;
    }

    /* Fill in the stride field for all attributes (equals total vertex size) */
    for (i = 0; i < count; i++) {
        attribs[i].stride = maxEnd;
    }

    *attribCount = count;
    *vertexSize  = maxEnd;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldParseVertexDeclaration: %d attribs, vertexSize=%d bytes",
        count, maxEnd);
}

// ***********************************************************************

/*****************************************************************************
 * gldCreateIndexBuffer
 *
 * Create an OpenGL element (index) buffer.
 *
 * Steps:
 *   1. Generate a buffer via glGenBuffers.
 *   2. Bind it to GL_ELEMENT_ARRAY_BUFFER.
 *   3. Upload index data via glBufferData with GL_STATIC_DRAW.
 *
 * Returns the GL buffer id (GLuint), or 0 on failure.
 *****************************************************************************/
GLuint gldCreateIndexBuffer(void *ctx, const void *indexData,
                            int indexCount, BOOL is32Bit)
{
    GLuint ebo = 0;
    GLsizeiptr bufferSize;
    int indexSize;

    (void)ctx;  /* reserved for future per-context tracking */

    if (!indexData || indexCount <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateIndexBuffer: invalid index data (data=%p, count=%d)",
            indexData, indexCount);
        return 0;
    }

    indexSize  = is32Bit ? (int)sizeof(GLuint) : (int)sizeof(GLushort);
    bufferSize = (GLsizeiptr)indexCount * (GLsizeiptr)indexSize;

    /* 1. Generate the buffer */
    glGenBuffers(1, &ebo);
    GLD_CHECK_GL("glGenBuffers", "CreateIndexBuffer");
    if (ebo == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldCreateIndexBuffer: glGenBuffers returned 0");
        return 0;
    }

    /* 2. Bind to GL_ELEMENT_ARRAY_BUFFER */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    GLD_CHECK_GL("glBindBuffer", "CreateIndexBuffer");

    /* 3. Upload index data */
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferSize, indexData, GL_STATIC_DRAW);
    GLD_CHECK_GL("glBufferData", "CreateIndexBuffer");

    /* Unbind the element buffer (caller will re-bind as needed) */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    gldLogPrintf(GLDLOG_DEBUG,
        "gldCreateIndexBuffer: created EBO=%u (%d indices, %s, %d bytes)",
        ebo, indexCount, is32Bit ? "32-bit" : "16-bit", (int)bufferSize);

    return ebo;
}

// ***********************************************************************

/*****************************************************************************
 * gldMapD3DPrimType
 *
 * Map a DX9 D3DPRIMITIVETYPE value to the equivalent OpenGL primitive
 * mode constant.
 *
 * Returns GL_TRIANGLES as a safe fallback for unrecognised types.
 *****************************************************************************/
GLenum gldMapD3DPrimType(DWORD d3dPrimType)
{
    switch (d3dPrimType) {
    case D3DPT_POINTLIST:       return GL_POINTS;
    case D3DPT_LINELIST:        return GL_LINES;
    case D3DPT_LINESTRIP:       return GL_LINE_STRIP;
    case D3DPT_TRIANGLELIST:    return GL_TRIANGLES;
    case D3DPT_TRIANGLESTRIP:   return GL_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN:     return GL_TRIANGLE_FAN;
    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldMapD3DPrimType: unrecognised D3DPT %u, defaulting to GL_TRIANGLES",
            (unsigned)d3dPrimType);
        return GL_TRIANGLES;
    }
}

// ***********************************************************************

/*****************************************************************************
 * gldPrimCountToVertexCount
 *
 * Convert a DX9 primitive count to the number of vertices required
 * for the given primitive type.
 *
 * Returns 0 for unrecognised primitive types.
 *****************************************************************************/
UINT gldPrimCountToVertexCount(DWORD primType, UINT primCount)
{
    switch (primType) {
    case D3DPT_POINTLIST:       return primCount;
    case D3DPT_LINELIST:        return primCount * 2;
    case D3DPT_LINESTRIP:       return primCount + 1;
    case D3DPT_TRIANGLELIST:    return primCount * 3;
    case D3DPT_TRIANGLESTRIP:   return primCount + 2;
    case D3DPT_TRIANGLEFAN:     return primCount + 2;
    default:
        gldLogPrintf(GLDLOG_WARN,
            "gldPrimCountToVertexCount: unrecognised D3DPT %u",
            (unsigned)primType);
        return 0;
    }
}

// ***********************************************************************

/*****************************************************************************
 * Streaming VBO state
 *
 * A single streaming VBO is maintained for DrawPrimitiveUP calls.
 * Buffer orphaning is used to avoid GPU stalls: each upload first
 * calls glBufferData with NULL data (discarding old storage), then
 * glBufferSubData to upload the new data.  The buffer is allocated
 * with GL_STREAM_DRAW to hint that data changes every frame.
 *****************************************************************************/
static GLuint  s_streamingVBO     = 0;
static GLsizei s_streamingVBOSize = 0;

/*****************************************************************************
 * gldUploadStreamingVBO
 *
 * Upload immediate vertex data into a streaming VBO for DrawPrimitiveUP
 * calls.  Uses buffer orphaning for efficient streaming.
 *
 * Returns the VBO id, or 0 on failure.
 *****************************************************************************/
GLuint gldUploadStreamingVBO(void *ctx, const void *data, int dataSize)
{
    GLsizeiptr allocSize;

    (void)ctx;  /* reserved for future per-context tracking */

    if (!data || dataSize <= 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldUploadStreamingVBO: invalid data (data=%p, size=%d)",
            data, dataSize);
        return 0;
    }

    /* ----------------------------------------------------------------
     * 1. Create the streaming VBO on first use.
     * ---------------------------------------------------------------- */
    if (s_streamingVBO == 0) {
        glGenBuffers(1, &s_streamingVBO);
        GLD_CHECK_GL("glGenBuffers", "UploadStreamingVBO");
        if (s_streamingVBO == 0) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldUploadStreamingVBO: glGenBuffers returned 0");
            return 0;
        }
        s_streamingVBOSize = 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_streamingVBO);
    GLD_CHECK_GL("glBindBuffer", "UploadStreamingVBO");

    /* ----------------------------------------------------------------
     * 2. Determine allocation size.  Use at least
     *    GLD_STREAMING_VBO_SIZE to reduce reallocations.
     * ---------------------------------------------------------------- */
    allocSize = (GLsizeiptr)dataSize;
    if (allocSize < GLD_STREAMING_VBO_SIZE)
        allocSize = GLD_STREAMING_VBO_SIZE;

    /* ----------------------------------------------------------------
     * 3. Buffer orphaning: call glBufferData with NULL data to
     *    discard the old storage.  If the requested size exceeds the
     *    current allocation, grow the buffer.
     * ---------------------------------------------------------------- */
    if (s_streamingVBOSize < allocSize) {
        /* Allocate (or grow) the buffer */
        glBufferData(GL_ARRAY_BUFFER, allocSize, NULL, GL_STREAM_DRAW);
        GLD_CHECK_GL("glBufferData(alloc)", "UploadStreamingVBO");
        s_streamingVBOSize = (GLsizei)allocSize;
    } else {
        /* Orphan the existing buffer (same size, NULL data) */
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)s_streamingVBOSize,
                     NULL, GL_STREAM_DRAW);
        GLD_CHECK_GL("glBufferData(orphan)", "UploadStreamingVBO");
    }

    /* ----------------------------------------------------------------
     * 4. Upload the new vertex data into the orphaned buffer.
     * ---------------------------------------------------------------- */
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)dataSize, data);
    GLD_CHECK_GL("glBufferSubData", "UploadStreamingVBO");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldUploadStreamingVBO: uploaded %d bytes into VBO=%u (capacity=%d)",
        dataSize, s_streamingVBO, (int)s_streamingVBOSize);

    return s_streamingVBO;
}

// ***********************************************************************

/*****************************************************************************
 * gldDrawPrimitive46
 *
 * Issue a non-indexed draw call corresponding to DX9 DrawPrimitive.
 *
 * Converts the DX9 primitive type and count to GL equivalents and
 * calls glDrawArrays.  The caller is responsible for ensuring the
 * correct VAO is bound before calling this function.
 *****************************************************************************/
void gldDrawPrimitive46(void *ctx, DWORD primType,
                        UINT startVertex, UINT primCount)
{
    GLenum glMode;
    UINT   vertexCount;

    (void)ctx;  /* reserved for future per-context tracking */

    glMode      = gldMapD3DPrimType(primType);
    vertexCount = gldPrimCountToVertexCount(primType, primCount);

    if (vertexCount == 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldDrawPrimitive46: 0 vertices for primType=%u, primCount=%u",
            (unsigned)primType, (unsigned)primCount);
        return;
    }

    glDrawArrays(glMode, (GLint)startVertex, (GLsizei)vertexCount);
    GLD_CHECK_GL("glDrawArrays", "DrawPrimitive");

    gldLogPrintf(GLDLOG_DEBUG,
        "gldDrawPrimitive46: glDrawArrays(mode=0x%X, first=%u, count=%u)",
        glMode, startVertex, vertexCount);
}

// ***********************************************************************

/*****************************************************************************
 * gldDrawIndexedPrimitive46
 *
 * Issue an indexed draw call corresponding to DX9 DrawIndexedPrimitive.
 *
 * Converts the DX9 primitive type and count to GL equivalents and
 * calls glDrawElementsBaseVertex (or glDrawElements when baseVertex
 * is 0).  The caller is responsible for ensuring the correct VAO and
 * element buffer are bound before calling this function.
 *****************************************************************************/
void gldDrawIndexedPrimitive46(void *ctx, DWORD primType,
                               INT baseVertex, UINT startIndex,
                               UINT primCount, GLenum indexType)
{
    GLenum glMode;
    UINT   vertexCount;
    int    indexSize;
    const void *indexOffset;

    (void)ctx;  /* reserved for future per-context tracking */

    glMode      = gldMapD3DPrimType(primType);
    vertexCount = gldPrimCountToVertexCount(primType, primCount);

    if (vertexCount == 0) {
        gldLogPrintf(GLDLOG_WARN,
            "gldDrawIndexedPrimitive46: 0 vertices for primType=%u, primCount=%u",
            (unsigned)primType, (unsigned)primCount);
        return;
    }

    /* Compute the byte offset into the index buffer */
    indexSize   = (indexType == GL_UNSIGNED_INT) ? (int)sizeof(GLuint)
                                                : (int)sizeof(GLushort);
    indexOffset = (const void *)(intptr_t)(startIndex * indexSize);

    if (baseVertex != 0) {
        glDrawElementsBaseVertex(glMode, (GLsizei)vertexCount, indexType,
                                 indexOffset, (GLint)baseVertex);
        GLD_CHECK_GL("glDrawElementsBaseVertex", "DrawIndexedPrimitive");
    } else {
        glDrawElements(glMode, (GLsizei)vertexCount, indexType, indexOffset);
        GLD_CHECK_GL("glDrawElements", "DrawIndexedPrimitive");
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldDrawIndexedPrimitive46: mode=0x%X, count=%u, indexType=0x%X, "
        "startIndex=%u, baseVertex=%d",
        glMode, vertexCount, indexType, startIndex, baseVertex);
}
