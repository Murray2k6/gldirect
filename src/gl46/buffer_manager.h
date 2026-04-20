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

#ifndef __GL46_BUFFER_MANAGER_H
#define __GL46_BUFFER_MANAGER_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/* Maximum number of vertex attributes we can decode from an FVF or
 * vertex declaration.  Position + Normal + Diffuse + Specular + 8 tex = 12. */
#define GLD_MAX_VERTEX_ATTRIBS  16

/*
 * D3DFVF constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally with #ifndef guards so we don't require the DX9 SDK
 * headers in all build configurations.
 */
#ifndef D3DFVF_XYZ
#define D3DFVF_XYZ             0x002
#endif
#ifndef D3DFVF_XYZRHW
#define D3DFVF_XYZRHW          0x004
#endif
#ifndef D3DFVF_NORMAL
#define D3DFVF_NORMAL           0x010
#endif
#ifndef D3DFVF_DIFFUSE
#define D3DFVF_DIFFUSE          0x040
#endif
#ifndef D3DFVF_SPECULAR
#define D3DFVF_SPECULAR         0x080
#endif
#ifndef D3DFVF_TEX0
#define D3DFVF_TEX0             0x000
#endif
#ifndef D3DFVF_TEX1
#define D3DFVF_TEX1             0x100
#endif
#ifndef D3DFVF_TEX2
#define D3DFVF_TEX2             0x200
#endif
#ifndef D3DFVF_TEX3
#define D3DFVF_TEX3             0x300
#endif
#ifndef D3DFVF_TEX4
#define D3DFVF_TEX4             0x400
#endif
#ifndef D3DFVF_TEX5
#define D3DFVF_TEX5             0x500
#endif
#ifndef D3DFVF_TEX6
#define D3DFVF_TEX6             0x600
#endif
#ifndef D3DFVF_TEX7
#define D3DFVF_TEX7             0x700
#endif
#ifndef D3DFVF_TEX8
#define D3DFVF_TEX8             0x800
#endif
#ifndef D3DFVF_TEXCOUNT_MASK
#define D3DFVF_TEXCOUNT_MASK    0xF00
#endif
#ifndef D3DFVF_TEXCOUNT_SHIFT
#define D3DFVF_TEXCOUNT_SHIFT   8
#endif

/*
 * D3DFVF_TEXCOORDSIZE macros from the DirectX 9 SDK.
 * These encode the dimensionality of each texture coordinate set
 * in the upper 16 bits of the FVF code.
 *
 * Bits [17:16] = texcoord set 0 size, [19:18] = set 1, etc.
 * Values: 0 = 2D (default), 1 = 3D, 2 = 4D, 3 = 1D
 */
#ifndef D3DFVF_TEXTUREFORMAT1
#define D3DFVF_TEXTUREFORMAT1   3   /* 1D texture coordinate */
#endif
#ifndef D3DFVF_TEXTUREFORMAT2
#define D3DFVF_TEXTUREFORMAT2   0   /* 2D texture coordinate (default) */
#endif
#ifndef D3DFVF_TEXTUREFORMAT3
#define D3DFVF_TEXTUREFORMAT3   1   /* 3D texture coordinate */
#endif
#ifndef D3DFVF_TEXTUREFORMAT4
#define D3DFVF_TEXTUREFORMAT4   2   /* 4D texture coordinate */
#endif

/*
 * Helper macro to extract the texture coordinate size for a given
 * texture coordinate index from the FVF code.
 * Returns 0 (2D), 1 (3D), 2 (4D), or 3 (1D).
 */
#define GLD_FVF_TEXCOORDSIZE(fvf, coordIndex) \
    (((fvf) >> (16 + 2 * (coordIndex))) & 0x3)

/*
 * GLD_vertexAttrib — describes a single vertex attribute for VAO
 * configuration.
 *
 * Fields:
 *   attribIndex    — GLSL vertex attribute location (layout qualifier).
 *   componentCount — number of components (1, 2, 3, or 4).
 *   glType         — OpenGL data type (GL_FLOAT, GL_UNSIGNED_BYTE, etc.).
 *   normalized     — GL_TRUE if integer data should be normalised to [0,1].
 *   offset         — byte offset of this attribute within the vertex.
 *   stride         — byte stride between consecutive vertices (set after
 *                    all attributes are decoded; equals the total vertex size).
 */
typedef struct {
    GLuint  attribIndex;
    GLint   componentCount;
    GLenum  glType;
    GLboolean normalized;
    GLsizei offset;
    GLsizei stride;
} GLD_vertexAttrib;

/*
 * D3DVERTEXELEMENT9 — describes a single element in a vertex declaration.
 * Defined locally so we don't require the DX9 SDK headers.
 */
typedef struct {
    WORD  Stream;       /* Stream index */
    WORD  Offset;       /* Byte offset within the stream */
    BYTE  Type;         /* D3DDECLTYPE */
    BYTE  Method;       /* D3DDECLMETHOD (usually 0 = DEFAULT) */
    BYTE  Usage;        /* D3DDECLUSAGE */
    BYTE  UsageIndex;   /* Usage index (e.g. TEXCOORD0, TEXCOORD1) */
} GLD_D3DVERTEXELEMENT9;

/*
 * D3DDECLTYPE constants — vertex element data types.
 */
#ifndef D3DDECLTYPE_FLOAT1
#define D3DDECLTYPE_FLOAT1      0
#endif
#ifndef D3DDECLTYPE_FLOAT2
#define D3DDECLTYPE_FLOAT2      1
#endif
#ifndef D3DDECLTYPE_FLOAT3
#define D3DDECLTYPE_FLOAT3      2
#endif
#ifndef D3DDECLTYPE_FLOAT4
#define D3DDECLTYPE_FLOAT4      3
#endif
#ifndef D3DDECLTYPE_D3DCOLOR
#define D3DDECLTYPE_D3DCOLOR    4
#endif
#ifndef D3DDECLTYPE_UBYTE4
#define D3DDECLTYPE_UBYTE4      5
#endif
#ifndef D3DDECLTYPE_SHORT2
#define D3DDECLTYPE_SHORT2      6
#endif
#ifndef D3DDECLTYPE_SHORT4
#define D3DDECLTYPE_SHORT4      8
#endif

/*
 * D3DDECLUSAGE constants — vertex element semantic usage.
 */
#ifndef D3DDECLUSAGE_POSITION
#define D3DDECLUSAGE_POSITION       0
#endif
#ifndef D3DDECLUSAGE_BLENDWEIGHT
#define D3DDECLUSAGE_BLENDWEIGHT    1
#endif
#ifndef D3DDECLUSAGE_BLENDINDICES
#define D3DDECLUSAGE_BLENDINDICES   2
#endif
#ifndef D3DDECLUSAGE_NORMAL
#define D3DDECLUSAGE_NORMAL         3
#endif
#ifndef D3DDECLUSAGE_PSIZE
#define D3DDECLUSAGE_PSIZE          4
#endif
#ifndef D3DDECLUSAGE_TEXCOORD
#define D3DDECLUSAGE_TEXCOORD       5
#endif
#ifndef D3DDECLUSAGE_TANGENT
#define D3DDECLUSAGE_TANGENT        6
#endif
#ifndef D3DDECLUSAGE_BINORMAL
#define D3DDECLUSAGE_BINORMAL       7
#endif
#ifndef D3DDECLUSAGE_TESSFACTOR
#define D3DDECLUSAGE_TESSFACTOR     8
#endif
#ifndef D3DDECLUSAGE_POSITIONT
#define D3DDECLUSAGE_POSITIONT      9
#endif
#ifndef D3DDECLUSAGE_COLOR
#define D3DDECLUSAGE_COLOR          10
#endif
#ifndef D3DDECLUSAGE_FOG
#define D3DDECLUSAGE_FOG            11
#endif
#ifndef D3DDECLUSAGE_DEPTH
#define D3DDECLUSAGE_DEPTH          12
#endif
#ifndef D3DDECLUSAGE_SAMPLE
#define D3DDECLUSAGE_SAMPLE         13
#endif

/*
 * D3DDECL_END sentinel — marks the end of a vertex element array.
 * Stream=0xFF, Offset=0, Type=17 (D3DDECLTYPE_UNUSED), Method=0,
 * Usage=0, UsageIndex=0.
 */
#define D3DDECL_END_STREAM      0xFF
#define D3DDECLTYPE_UNUSED      17

/*
 * Macro to check if a vertex element is the D3DDECL_END sentinel.
 */
#define GLD_IS_DECL_END(elem) \
    ((elem)->Stream == D3DDECL_END_STREAM && (elem)->Type == D3DDECLTYPE_UNUSED)

/*
 * D3DPRIMITIVETYPE constants from the DirectX 9 SDK (d3d9types.h).
 * Defined locally with #ifndef guards so we don't require the DX9 SDK
 * headers in all build configurations.
 */
#ifndef D3DPT_POINTLIST
#define D3DPT_POINTLIST         1
#endif
#ifndef D3DPT_LINELIST
#define D3DPT_LINELIST          2
#endif
#ifndef D3DPT_LINESTRIP
#define D3DPT_LINESTRIP         3
#endif
#ifndef D3DPT_TRIANGLELIST
#define D3DPT_TRIANGLELIST      4
#endif
#ifndef D3DPT_TRIANGLESTRIP
#define D3DPT_TRIANGLESTRIP     5
#endif
#ifndef D3DPT_TRIANGLEFAN
#define D3DPT_TRIANGLEFAN       6
#endif

/*
 * Default size for the streaming VBO used by DrawPrimitiveUP.
 * 4 MB is large enough for most immediate-mode draw calls while
 * keeping GPU memory usage reasonable.
 */
#define GLD_STREAMING_VBO_SIZE  (4 * 1024 * 1024)

/*
 * Forward declaration — GLD_glContext is defined in gld_context.h.
 */
struct GLD_glContext_s;
typedef struct GLD_glContext_s GLD_glContext_fwd;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Decode a DX9 Flexible Vertex Format (FVF) bitmask into an array of
 * vertex attribute descriptors.
 *
 * The FVF bitmask encodes which vertex components are present and their
 * order in the vertex data.  This function parses the bitmask and
 * produces an array of GLD_vertexAttrib structs that can be used to
 * configure a VAO.
 *
 * Attribute location mapping:
 *   Location 0  — Position (D3DFVF_XYZ: 3 floats, D3DFVF_XYZRHW: 4 floats)
 *   Location 1  — Normal   (D3DFVF_NORMAL: 3 floats)
 *   Location 2  — Diffuse  (D3DFVF_DIFFUSE: 4 ubytes, normalised)
 *   Location 3  — Specular (D3DFVF_SPECULAR: 4 ubytes, normalised)
 *   Locations 4–11 — Texture coordinates (D3DFVF_TEX1–TEX8: 2 floats each
 *                    by default, or as specified by D3DFVF_TEXCOORDSIZE)
 *
 * Parameters:
 *   fvf         — the DX9 FVF bitmask.
 *   attribs     — output array of GLD_vertexAttrib structs.  Must have
 *                 room for at least GLD_MAX_VERTEX_ATTRIBS entries.
 *   attribCount — receives the number of attributes decoded.
 *   vertexSize  — receives the total vertex size in bytes.
 */
void gldDecodeFVF(DWORD fvf, GLD_vertexAttrib *attribs,
                  int *attribCount, int *vertexSize);

/*
 * Parse a DX9 vertex declaration (D3DVERTEXELEMENT9 array) into an
 * array of vertex attribute descriptors.
 *
 * The vertex declaration is a more flexible alternative to FVF codes,
 * allowing arbitrary element types and orderings.  This function
 * iterates the element array until the D3DDECL_END sentinel and
 * produces GLD_vertexAttrib structs suitable for VAO configuration.
 *
 * D3DDECLTYPE → GL type mapping:
 *   FLOAT1  → 1 float          FLOAT2  → 2 floats
 *   FLOAT3  → 3 floats         FLOAT4  → 4 floats
 *   D3DCOLOR→ 4 ubytes (norm)  UBYTE4  → 4 ubytes
 *   SHORT2  → 2 shorts         SHORT4  → 4 shorts
 *
 * D3DDECLUSAGE → attribute location mapping (matches FVF convention):
 *   POSITION/POSITIONT → 0     NORMAL    → 1
 *   COLOR(index 0)     → 2     COLOR(index 1) → 3
 *   TEXCOORD(index N)  → 4+N   BLENDWEIGHT    → 12
 *   BLENDINDICES       → 13    TANGENT        → 14
 *   BINORMAL           → 15
 *
 * Parameters:
 *   elements    — pointer to the D3DVERTEXELEMENT9 array (terminated
 *                 by D3DDECL_END sentinel).
 *   attribs     — output array of GLD_vertexAttrib structs.  Must have
 *                 room for at least GLD_MAX_VERTEX_ATTRIBS entries.
 *   attribCount — receives the number of attributes decoded.
 *   vertexSize  — receives the total vertex size in bytes.
 */
void gldParseVertexDeclaration(const void *elements,
                               GLD_vertexAttrib *attribs,
                               int *attribCount, int *vertexSize);

/*
 * Create a VAO with a VBO containing the supplied vertex data.
 *
 * Creates a Vertex Array Object via glGenVertexArrays/glBindVertexArray,
 * creates a Vertex Buffer Object via glGenBuffers/glBindBuffer/glBufferData
 * with GL_STATIC_DRAW usage, and configures vertex attributes via
 * glVertexAttribPointer/glEnableVertexAttribArray.
 *
 * Parameters:
 *   ctx         — the GL46 context (used for error checking; may be NULL
 *                 for standalone use).
 *   vertexData  — pointer to the vertex data to upload.
 *   vertexCount — number of vertices.
 *   vertexSize  — size of a single vertex in bytes (stride).
 *   attribs     — array of vertex attribute descriptors.
 *   attribCount — number of entries in the attribs array.
 *
 * Returns:
 *   The VAO id (GLuint).  Returns 0 on failure.
 */
GLuint gldCreateVAO(void *ctx, const void *vertexData,
                    int vertexCount, int vertexSize,
                    const GLD_vertexAttrib *attribs, int attribCount);

/*
 * Create an OpenGL element (index) buffer.
 *
 * Generates a GL buffer object, binds it to GL_ELEMENT_ARRAY_BUFFER,
 * and uploads the index data via glBufferData with GL_STATIC_DRAW.
 *
 * Parameters:
 *   ctx        — the GL46 context (reserved; may be NULL).
 *   indexData  — pointer to the index data to upload.
 *   indexCount — number of indices.
 *   is32Bit    — TRUE for 32-bit (GL_UNSIGNED_INT) indices,
 *                FALSE for 16-bit (GL_UNSIGNED_SHORT) indices.
 *
 * Returns:
 *   The GL buffer id (GLuint).  Returns 0 on failure.
 */
GLuint gldCreateIndexBuffer(void *ctx, const void *indexData,
                            int indexCount, BOOL is32Bit);

/*
 * Map a DX9 D3DPRIMITIVETYPE to the equivalent OpenGL primitive mode.
 *
 * Mapping:
 *   D3DPT_POINTLIST     → GL_POINTS
 *   D3DPT_LINELIST      → GL_LINES
 *   D3DPT_LINESTRIP     → GL_LINE_STRIP
 *   D3DPT_TRIANGLELIST  → GL_TRIANGLES
 *   D3DPT_TRIANGLESTRIP → GL_TRIANGLE_STRIP
 *   D3DPT_TRIANGLEFAN   → GL_TRIANGLE_FAN
 *
 * Parameters:
 *   d3dPrimType — the DX9 primitive type (D3DPT_* constant).
 *
 * Returns:
 *   The GL primitive mode (GLenum).  Returns GL_TRIANGLES as a
 *   fallback for unrecognised types (with a warning logged).
 */
GLenum gldMapD3DPrimType(DWORD d3dPrimType);

/*
 * Convert a DX9 primitive count to the number of vertices required
 * for the given primitive type.
 *
 * Conversion rules:
 *   POINTLIST:     vertexCount = primCount
 *   LINELIST:      vertexCount = primCount * 2
 *   LINESTRIP:     vertexCount = primCount + 1
 *   TRIANGLELIST:  vertexCount = primCount * 3
 *   TRIANGLESTRIP: vertexCount = primCount + 2
 *   TRIANGLEFAN:   vertexCount = primCount + 2
 *
 * Parameters:
 *   primType  — the DX9 primitive type (D3DPT_* constant).
 *   primCount — the number of primitives.
 *
 * Returns:
 *   The number of vertices.  Returns 0 for unrecognised types.
 */
UINT gldPrimCountToVertexCount(DWORD primType, UINT primCount);

/*
 * Upload immediate vertex data into a streaming VBO for
 * DrawPrimitiveUP calls.
 *
 * Uses buffer orphaning (glBufferData with NULL data followed by
 * glBufferSubData) with GL_STREAM_DRAW usage hint for efficient
 * streaming without GPU stalls.  A single streaming VBO is reused
 * across calls; if the requested data exceeds the current buffer
 * capacity, the buffer is reallocated.
 *
 * Parameters:
 *   ctx      — the GL46 context (reserved; may be NULL).
 *   data     — pointer to the vertex data to upload.
 *   dataSize — size of the vertex data in bytes.
 *
 * Returns:
 *   The VBO id (GLuint).  Returns 0 on failure.
 */
GLuint gldUploadStreamingVBO(void *ctx, const void *data, int dataSize);

/*
 * Issue a non-indexed draw call (DrawPrimitive).
 *
 * Binds the currently active VAO and calls glDrawArrays with the
 * mapped GL primitive mode.
 *
 * Parameters:
 *   ctx         — the GL46 context (reserved; may be NULL).
 *   primType    — the DX9 primitive type (D3DPT_* constant).
 *   startVertex — the index of the first vertex to draw.
 *   primCount   — the number of primitives to draw.
 */
void gldDrawPrimitive46(void *ctx, DWORD primType,
                        UINT startVertex, UINT primCount);

/*
 * Issue an indexed draw call (DrawIndexedPrimitive).
 *
 * Calls glDrawElementsBaseVertex (or glDrawElements when baseVertex
 * is 0) with the mapped GL primitive mode and index type.
 *
 * Parameters:
 *   ctx         — the GL46 context (reserved; may be NULL).
 *   primType    — the DX9 primitive type (D3DPT_* constant).
 *   baseVertex  — value added to each index before fetching vertex data.
 *   startIndex  — offset (in indices) into the index buffer.
 *   primCount   — the number of primitives to draw.
 *   indexType   — GL_UNSIGNED_SHORT or GL_UNSIGNED_INT.
 */
void gldDrawIndexedPrimitive46(void *ctx, DWORD primType,
                               INT baseVertex, UINT startIndex,
                               UINT primCount, GLenum indexType);

#ifdef  __cplusplus
}
#endif

#endif
