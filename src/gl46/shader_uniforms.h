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
* Description:  Shader uniform and constant management for DX9-to-GL translation.
*               Maintains CPU-side shadow buffers for DX9 shader constant
*               registers, tracks dirty ranges, and uploads only modified
*               constants to the active GL program via glUniform4fv.
*
*********************************************************************************/

#ifndef __GL46_SHADER_UNIFORMS_H
#define __GL46_SHADER_UNIFORMS_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/* DX9 constant register limits */
#define GLD_VS_FLOAT_CONST_COUNT    256
#define GLD_VS_INT_CONST_COUNT      16
#define GLD_VS_BOOL_CONST_COUNT     16
#define GLD_PS_FLOAT_CONST_COUNT    32
#define GLD_PS_INT_CONST_COUNT      16
#define GLD_PS_BOOL_CONST_COUNT     16

/*
 * GLD_shaderConstants — CPU-side shadow buffers for all DX9 shader
 * constant registers, with dirty tracking and cached uniform locations.
 *
 * The shadow buffers mirror the DX9 constant register file:
 *   - Vertex shader: c0-c255 (float4), i0-i15 (int4), b0-b15 (bool)
 *   - Pixel shader:  c0-c31  (float4), i0-i15 (int4), b0-b15 (bool)
 *
 * Dirty ranges track the min/max register index modified since the last
 * upload, so only the changed range is sent to the GPU each draw call.
 */
typedef struct {
    /* Vertex shader constant shadow buffers */
    float   vsConstantsF[GLD_VS_FLOAT_CONST_COUNT][4];
    int     vsConstantsI[GLD_VS_INT_CONST_COUNT][4];
    BOOL    vsConstantsB[GLD_VS_BOOL_CONST_COUNT];

    /* Pixel shader constant shadow buffers */
    float   psConstantsF[GLD_PS_FLOAT_CONST_COUNT][4];
    int     psConstantsI[GLD_PS_INT_CONST_COUNT][4];
    BOOL    psConstantsB[GLD_PS_BOOL_CONST_COUNT];

    /* Dirty range tracking for float constants */
    int     vsDirtyMinF;    /* Min modified VS float register (-1 = clean) */
    int     vsDirtyMaxF;    /* Max modified VS float register (-1 = clean) */
    int     psDirtyMinF;    /* Min modified PS float register (-1 = clean) */
    int     psDirtyMaxF;    /* Max modified PS float register (-1 = clean) */

    /* Dirty flags for integer and boolean constants */
    BOOL    vsIntDirty;     /* TRUE if any VS integer constant changed */
    BOOL    vsBoolDirty;    /* TRUE if any VS boolean constant changed */
    BOOL    psIntDirty;     /* TRUE if any PS integer constant changed */
    BOOL    psBoolDirty;    /* TRUE if any PS boolean constant changed */

    /* Cached uniform locations for the "c" (float4) array in VS and PS */
    GLint   vsConstLocF;    /* glGetUniformLocation result for VS "c[0]" */
    GLint   psConstLocF;    /* glGetUniformLocation result for PS "c[0]" */

    /* Cached uniform locations for integer constant arrays */
    GLint   vsConstLocI;    /* glGetUniformLocation result for VS "i[0]" */
    GLint   psConstLocI;    /* glGetUniformLocation result for PS "i[0]" */

    /* Cached uniform locations for boolean constant arrays */
    GLint   vsConstLocB;    /* glGetUniformLocation result for VS "b[0]" */
    GLint   psConstLocB;    /* glGetUniformLocation result for PS "b[0]" */

    /* Program these locations were cached for */
    GLuint  lastProgram;    /* GL program ID the cached locations belong to */
} GLD_shaderConstants;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Initialize a GLD_shaderConstants structure to default state.
 * Zeros all shadow buffers and marks all ranges as clean.
 *
 * Parameters:
 *   constants — pointer to the structure to initialize.
 */
void gldInitShaderConstants(GLD_shaderConstants *constants);

/*
 * Set vertex shader float constant registers.
 * Updates the shadow buffer and marks the affected range as dirty.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-255).
 *   data      — pointer to float4 data (4 floats per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetVertexShaderConstantF46(GLD_shaderConstants *constants,
                                   UINT startReg, const float *data,
                                   UINT count);

/*
 * Set pixel shader float constant registers.
 * Updates the shadow buffer and marks the affected range as dirty.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-31).
 *   data      — pointer to float4 data (4 floats per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetPixelShaderConstantF46(GLD_shaderConstants *constants,
                                  UINT startReg, const float *data,
                                  UINT count);

/*
 * Set vertex shader integer constant registers.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-15).
 *   data      — pointer to int4 data (4 ints per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetVertexShaderConstantI46(GLD_shaderConstants *constants,
                                   UINT startReg, const int *data,
                                   UINT count);

/*
 * Set pixel shader integer constant registers.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-15).
 *   data      — pointer to int4 data (4 ints per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetPixelShaderConstantI46(GLD_shaderConstants *constants,
                                  UINT startReg, const int *data,
                                  UINT count);

/*
 * Set vertex shader boolean constant registers.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-15).
 *   data      — pointer to BOOL data (1 BOOL per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetVertexShaderConstantB46(GLD_shaderConstants *constants,
                                   UINT startReg, const BOOL *data,
                                   UINT count);

/*
 * Set pixel shader boolean constant registers.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   startReg  — first register index to update (0-15).
 *   data      — pointer to BOOL data (1 BOOL per register).
 *   count     — number of registers to update.
 *
 * Returns:
 *   TRUE on success, FALSE if the register range is out of bounds.
 */
BOOL gldSetPixelShaderConstantB46(GLD_shaderConstants *constants,
                                  UINT startReg, const BOOL *data,
                                  UINT count);

/*
 * Upload dirty shader constants to the active GL program.
 * Called before each draw call. Only uploads registers that have been
 * modified since the last upload. Caches uniform locations on first
 * lookup or when the active program changes.
 *
 * Parameters:
 *   constants — the shader constants state.
 *   program   — the currently active GL program object.
 */
void gldUploadConstants46(GLD_shaderConstants *constants, GLuint program);

#ifdef  __cplusplus
}
#endif

#endif
