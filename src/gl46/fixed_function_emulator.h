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
* Description:  Fixed-function pipeline emulation for OpenGL 4.6 core profile.
*               Generates internal GLSL 4.60 vertex and fragment shaders that
*               replicate DX9 fixed-function behaviour: per-vertex lighting,
*               texture environment stages, fog, alpha test, and transform
*               matrix management.  Shader variants are cached by state key
*               to avoid redundant compilation.
*
*********************************************************************************/

#ifndef __GL46_FIXED_FUNCTION_EMULATOR_H
#define __GL46_FIXED_FUNCTION_EMULATOR_H

#include <windows.h>
#include <glad/gl.h>
#include "state_translator.h"

/*---------------------- Macros and type definitions ----------------------*/

/* Maximum number of cached fixed-function shader variants */
#define GLD_FF_CACHE_SIZE           128

/* Maximum number of simultaneous lights */
#define GLD_FF_MAX_LIGHTS           8

/* Maximum number of texture stages */
#define GLD_FF_MAX_TEX_STAGES       8

/* Maximum GLSL source buffer size for generated shaders */
#define GLD_FF_MAX_GLSL_SOURCE      (16 * 1024)

/*
 * D3DTS_* transform type constants from the DirectX 9 SDK.
 * Defined locally so we don't require the DX9 SDK headers.
 */
#ifndef D3DTS_VIEW
#define D3DTS_VIEW                  2
#endif
#ifndef D3DTS_PROJECTION
#define D3DTS_PROJECTION            3
#endif
#ifndef D3DTS_TEXTURE0
#define D3DTS_TEXTURE0              16
#endif
#ifndef D3DTS_TEXTURE1
#define D3DTS_TEXTURE1              17
#endif
#ifndef D3DTS_TEXTURE2
#define D3DTS_TEXTURE2              18
#endif
#ifndef D3DTS_TEXTURE3
#define D3DTS_TEXTURE3              19
#endif
#ifndef D3DTS_TEXTURE4
#define D3DTS_TEXTURE4              20
#endif
#ifndef D3DTS_TEXTURE5
#define D3DTS_TEXTURE5              21
#endif
#ifndef D3DTS_TEXTURE6
#define D3DTS_TEXTURE6              22
#endif
#ifndef D3DTS_TEXTURE7
#define D3DTS_TEXTURE7              23
#endif
#ifndef D3DTS_WORLD
#define D3DTS_WORLD                 256
#endif

/*
 * GLD_ffLightType — DX9 light types.
 */
#define GLD_FF_LIGHT_DIRECTIONAL    0
#define GLD_FF_LIGHT_POINT          1
#define GLD_FF_LIGHT_SPOT           2

/*
 * GLD_ffStateKey — encodes the current fixed-function pipeline state
 * for shader variant selection and caching.
 *
 * Two state keys that compare equal (memcmp) produce identical shader
 * source, so the same compiled program can be reused.
 */
typedef struct {
    BYTE    numLights;          /* Number of active lights (0-8)            */
    BYTE    lightTypesMask;     /* 2 bits per light: 00=dir, 01=point, 10=spot */
    BYTE    fogMode;            /* 0=none, 1=exp, 2=exp2, 3=linear         */
    BYTE    numTexStages;       /* Number of active texture stages (0-8)   */
    BYTE    alphaTestEnable;    /* 1 if alpha test is active               */
    BYTE    pointSpriteEnable;  /* 1 if rendering point primitives         */
    BYTE    _pad[2];            /* Padding for alignment                   */
} GLD_ffStateKey;

/*
 * GLD_ffCacheEntry — a single entry in the fixed-function shader cache.
 */
typedef struct {
    GLD_ffStateKey  key;        /* State key for this variant              */
    DWORD           hash;       /* FNV-1a hash of the key                  */
    GLuint          program;    /* Linked GL program object                */
    BOOL            bUsed;      /* TRUE if this slot is occupied           */
} GLD_ffCacheEntry;

/*
 * GLD_fixedFuncCache — per-context fixed-function emulator state.
 *
 * Holds the transform matrices, the shader variant cache, and the
 * current fixed-function state key.  Embedded in GLD_glContext via
 * the fixedFuncCache pointer.
 */
typedef struct GLD_fixedFuncCache_s {
    /* Transform matrices — stored in column-major (GL) order */
    float   worldMatrix[16];
    float   viewMatrix[16];
    float   projMatrix[16];
    float   texMatrix[GLD_FF_MAX_TEX_STAGES][16];

    /* Derived matrices — recomputed when world/view/proj change */
    float   mvMatrix[16];       /* view * world                            */
    float   mvpMatrix[16];      /* proj * view * world                     */
    float   normalMatrix[9];    /* inverse transpose of upper-left 3x3 MV  */

    /* Dirty flag — set when any matrix changes, cleared after upload */
    BOOL    bMatricesDirty;

    /* Shader variant cache */
    GLD_ffCacheEntry    cache[GLD_FF_CACHE_SIZE];
    int                 cacheCount;

    /* Currently active fixed-function program */
    GLuint  activeProgram;
} GLD_fixedFuncCache;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Initialize a GLD_fixedFuncCache structure to default state.
 * Sets all matrices to identity and clears the shader cache.
 *
 * Parameters:
 *   cache — pointer to the cache structure to initialize.
 */
void gldInitFixedFuncCache(GLD_fixedFuncCache *cache);

/*
 * Destroy a GLD_fixedFuncCache, deleting all cached GL programs.
 *
 * Parameters:
 *   cache — pointer to the cache structure to destroy.
 */
void gldDestroyFixedFuncCache(GLD_fixedFuncCache *cache);

/*
 * Generate a GLSL 4.60 vertex shader for the given fixed-function state.
 *
 * Produces a complete vertex shader with:
 *   - MVP transform (gl_Position = uMVP * position)
 *   - Per-vertex Blinn-Phong lighting for directional lights
 *   - Texture coordinate pass-through
 *   - gl_PointSize output when point sprites are enabled
 *   - Material properties and matrices as uniforms
 *
 * Parameters:
 *   key        — the fixed-function state key.
 *   glslBuffer — output buffer for the GLSL source string.
 *   bufferSize — size of the output buffer in bytes.
 *
 * Returns:
 *   TRUE on success, FALSE if the buffer is too small.
 */
BOOL gldGenerateFFVertexShader(const GLD_ffStateKey *key,
                               char *glslBuffer, int bufferSize);

/*
 * Generate a GLSL 4.60 fragment shader for the given fixed-function state.
 *
 * Produces a complete fragment shader with:
 *   - Texture sampling and modulation with vertex color
 *   - Fog computation (linear/exp/exp2)
 *   - Alpha test via discard
 *
 * Parameters:
 *   key        — the fixed-function state key.
 *   glslBuffer — output buffer for the GLSL source string.
 *   bufferSize — size of the output buffer in bytes.
 *
 * Returns:
 *   TRUE on success, FALSE if the buffer is too small.
 */
BOOL gldGenerateFFFragmentShader(const GLD_ffStateKey *key,
                                 char *glslBuffer, int bufferSize);

/*
 * Select (or generate+compile+cache) the fixed-function shader program
 * matching the given state key, and bind it via glUseProgram.
 *
 * On cache hit, the existing program is bound directly.
 * On cache miss, vertex and fragment shaders are generated, compiled,
 * linked, cached, and then bound.
 *
 * Parameters:
 *   cache — the per-context fixed-function cache.
 *   key   — the current fixed-function state key.
 *
 * Returns:
 *   The GL program id on success, or 0 on failure.
 */
GLuint gldSelectFFProgram(GLD_fixedFuncCache *cache,
                          const GLD_ffStateKey *key);

/*
 * Store or update a DX9 transform matrix.
 *
 * Transposes the matrix from DX9 row-major to GL column-major order,
 * stores it in the cache, and marks derived matrices as dirty.
 *
 * Supported transform types:
 *   D3DTS_WORLD (256)      — world (model) matrix
 *   D3DTS_VIEW  (2)        — view (camera) matrix
 *   D3DTS_PROJECTION (3)   — projection matrix
 *   D3DTS_TEXTURE0-7 (16-23) — texture transform matrices
 *
 * Parameters:
 *   cache         — the per-context fixed-function cache.
 *   transformType — D3DTS_* constant identifying which matrix to set.
 *   matrix        — pointer to 16 floats in DX9 row-major order.
 */
void gldSetTransform46(GLD_fixedFuncCache *cache,
                       DWORD transformType, const float *matrix);

/*
 * Recompute derived matrices (MV, MVP, normal) if dirty, and upload
 * all transform uniforms to the given GL program.
 *
 * Should be called before each draw call when using the fixed-function
 * pipeline.
 *
 * Parameters:
 *   cache   — the per-context fixed-function cache.
 *   program — the currently active GL program to upload uniforms to.
 */
void gldUploadFFTransforms(GLD_fixedFuncCache *cache, GLuint program);

#ifdef  __cplusplus
}
#endif

#endif
