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

#include "shader_uniforms.h"
#include "error_handler.h"
#include "gld_log.h"
#include <string.h>

/* *********************************************************************** */

/*
 * Initialize a GLD_shaderConstants structure to default state.
 * Zeros all shadow buffers and marks all ranges as clean.
 */
void gldInitShaderConstants(GLD_shaderConstants *constants)
{
    if (!constants)
        return;

    memset(constants->vsConstantsF, 0, sizeof(constants->vsConstantsF));
    memset(constants->vsConstantsI, 0, sizeof(constants->vsConstantsI));
    memset(constants->vsConstantsB, 0, sizeof(constants->vsConstantsB));

    memset(constants->psConstantsF, 0, sizeof(constants->psConstantsF));
    memset(constants->psConstantsI, 0, sizeof(constants->psConstantsI));
    memset(constants->psConstantsB, 0, sizeof(constants->psConstantsB));

    /* Mark all dirty ranges as clean (-1 = nothing dirty) */
    constants->vsDirtyMinF = -1;
    constants->vsDirtyMaxF = -1;
    constants->psDirtyMinF = -1;
    constants->psDirtyMaxF = -1;

    constants->vsIntDirty  = FALSE;
    constants->vsBoolDirty = FALSE;
    constants->psIntDirty  = FALSE;
    constants->psBoolDirty = FALSE;

    /* Invalidate cached uniform locations */
    constants->vsConstLocF = -1;
    constants->psConstLocF = -1;
    constants->vsConstLocI = -1;
    constants->psConstLocI = -1;
    constants->vsConstLocB = -1;
    constants->psConstLocB = -1;
    constants->lastProgram = 0;
}

/* *********************************************************************** */

/*
 * Set vertex shader float constant registers.
 */
BOOL gldSetVertexShaderConstantF46(GLD_shaderConstants *constants,
                                   UINT startReg, const float *data,
                                   UINT count)
{
    UINT endReg;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantF46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_VS_FLOAT_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantF46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_VS_FLOAT_CONST_COUNT);
        return FALSE;
    }

    /* Copy data into shadow buffer (4 floats per register) */
    memcpy(&constants->vsConstantsF[startReg][0], data,
           count * 4 * sizeof(float));

    /* Expand dirty range */
    if (constants->vsDirtyMinF < 0 || (int)startReg < constants->vsDirtyMinF)
        constants->vsDirtyMinF = (int)startReg;
    if ((int)(endReg - 1) > constants->vsDirtyMaxF)
        constants->vsDirtyMaxF = (int)(endReg - 1);

    return TRUE;
}

/* *********************************************************************** */

/*
 * Set pixel shader float constant registers.
 */
BOOL gldSetPixelShaderConstantF46(GLD_shaderConstants *constants,
                                  UINT startReg, const float *data,
                                  UINT count)
{
    UINT endReg;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantF46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_PS_FLOAT_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantF46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_PS_FLOAT_CONST_COUNT);
        return FALSE;
    }

    /* Copy data into shadow buffer (4 floats per register) */
    memcpy(&constants->psConstantsF[startReg][0], data,
           count * 4 * sizeof(float));

    /* Expand dirty range */
    if (constants->psDirtyMinF < 0 || (int)startReg < constants->psDirtyMinF)
        constants->psDirtyMinF = (int)startReg;
    if ((int)(endReg - 1) > constants->psDirtyMaxF)
        constants->psDirtyMaxF = (int)(endReg - 1);

    return TRUE;
}

/* *********************************************************************** */

/*
 * Set vertex shader integer constant registers.
 */
BOOL gldSetVertexShaderConstantI46(GLD_shaderConstants *constants,
                                   UINT startReg, const int *data,
                                   UINT count)
{
    UINT endReg;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantI46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_VS_INT_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantI46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_VS_INT_CONST_COUNT);
        return FALSE;
    }

    /* Copy data into shadow buffer (4 ints per register) */
    memcpy(&constants->vsConstantsI[startReg][0], data,
           count * 4 * sizeof(int));

    constants->vsIntDirty = TRUE;

    return TRUE;
}

/* *********************************************************************** */

/*
 * Set pixel shader integer constant registers.
 */
BOOL gldSetPixelShaderConstantI46(GLD_shaderConstants *constants,
                                  UINT startReg, const int *data,
                                  UINT count)
{
    UINT endReg;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantI46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_PS_INT_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantI46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_PS_INT_CONST_COUNT);
        return FALSE;
    }

    /* Copy data into shadow buffer (4 ints per register) */
    memcpy(&constants->psConstantsI[startReg][0], data,
           count * 4 * sizeof(int));

    constants->psIntDirty = TRUE;

    return TRUE;
}

/* *********************************************************************** */

/*
 * Set vertex shader boolean constant registers.
 */
BOOL gldSetVertexShaderConstantB46(GLD_shaderConstants *constants,
                                   UINT startReg, const BOOL *data,
                                   UINT count)
{
    UINT endReg;
    UINT i;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantB46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_VS_BOOL_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetVertexShaderConstantB46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_VS_BOOL_CONST_COUNT);
        return FALSE;
    }

    for (i = 0; i < count; i++)
        constants->vsConstantsB[startReg + i] = data[i] ? TRUE : FALSE;

    constants->vsBoolDirty = TRUE;

    return TRUE;
}

/* *********************************************************************** */

/*
 * Set pixel shader boolean constant registers.
 */
BOOL gldSetPixelShaderConstantB46(GLD_shaderConstants *constants,
                                  UINT startReg, const BOOL *data,
                                  UINT count)
{
    UINT endReg;
    UINT i;

    if (!constants || !data) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantB46: NULL parameter");
        return FALSE;
    }

    endReg = startReg + count;
    if (endReg > GLD_PS_BOOL_CONST_COUNT) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSetPixelShaderConstantB46: register range [%u..%u) exceeds limit %d",
            startReg, endReg, GLD_PS_BOOL_CONST_COUNT);
        return FALSE;
    }

    for (i = 0; i < count; i++)
        constants->psConstantsB[startReg + i] = data[i] ? TRUE : FALSE;

    constants->psBoolDirty = TRUE;

    return TRUE;
}

/* *********************************************************************** */

/*
 * Refresh cached uniform locations for the given program.
 * Called when the active program changes or locations have not been
 * looked up yet.
 *
 * The GLSL shaders generated by shader_translator declare:
 *   uniform vec4  c[N];   — float constants
 *   uniform ivec4 i[N];   — integer constants
 *   uniform bool  b[N];   — boolean constants
 *
 * We look up "c[0]", "i[0]", "b[0]" — glUniform4fv with a count
 * will upload consecutive array elements starting from that location.
 */
static void gldRefreshUniformLocations(GLD_shaderConstants *constants,
                                       GLuint program)
{
    constants->vsConstLocF = glGetUniformLocation(program, "c[0]");
    constants->psConstLocF = glGetUniformLocation(program, "c[0]");
    constants->vsConstLocI = glGetUniformLocation(program, "i[0]");
    constants->psConstLocI = glGetUniformLocation(program, "i[0]");
    constants->vsConstLocB = glGetUniformLocation(program, "b[0]");
    constants->psConstLocB = glGetUniformLocation(program, "b[0]");
    constants->lastProgram = program;

    gldLogPrintf(GLDLOG_DEBUG,
        "gldRefreshUniformLocations: program %u — c=%d, i=%d, b=%d",
        program, constants->vsConstLocF, constants->vsConstLocI,
        constants->vsConstLocB);
}

/* *********************************************************************** */

/*
 * Upload dirty shader constants to the active GL program.
 * Only uploads registers that have been modified since the last upload.
 * Caches uniform locations on first lookup or when the program changes.
 */
void gldUploadConstants46(GLD_shaderConstants *constants, GLuint program)
{
    if (!constants || program == 0)
        return;

    /* Refresh cached uniform locations if the program changed */
    if (program != constants->lastProgram) {
        gldRefreshUniformLocations(constants, program);
    }

    /*
     * Upload dirty VS/PS float constants.
     *
     * The translated GLSL shaders use a single "uniform vec4 c[N]" array
     * shared between VS and PS (each shader has its own c[] declaration).
     * Since a linked program combines both, we upload the full dirty range
     * from whichever shader stage was modified.
     *
     * glUniform4fv uploads `count` consecutive vec4 values starting at
     * the uniform location for c[startReg].
     */

    /* Upload VS float constants */
    if (constants->vsDirtyMinF >= 0 && constants->vsConstLocF >= 0) {
        int minReg = constants->vsDirtyMinF;
        int maxReg = constants->vsDirtyMaxF;
        int count  = maxReg - minReg + 1;
        GLint loc  = constants->vsConstLocF + minReg;

        glUniform4fv(loc, count, &constants->vsConstantsF[minReg][0]);
        GLD_CHECK_GL("glUniform4fv", "SetVertexShaderConstantF");

        /* Clear dirty range */
        constants->vsDirtyMinF = -1;
        constants->vsDirtyMaxF = -1;
    }

    /* Upload PS float constants */
    if (constants->psDirtyMinF >= 0 && constants->psConstLocF >= 0) {
        int minReg = constants->psDirtyMinF;
        int maxReg = constants->psDirtyMaxF;
        int count  = maxReg - minReg + 1;
        GLint loc  = constants->psConstLocF + minReg;

        glUniform4fv(loc, count, &constants->psConstantsF[minReg][0]);
        GLD_CHECK_GL("glUniform4fv", "SetPixelShaderConstantF");

        /* Clear dirty range */
        constants->psDirtyMinF = -1;
        constants->psDirtyMaxF = -1;
    }

    /* Upload VS integer constants (all 16 registers when dirty) */
    if (constants->vsIntDirty && constants->vsConstLocI >= 0) {
        glUniform4iv(constants->vsConstLocI, GLD_VS_INT_CONST_COUNT,
                     &constants->vsConstantsI[0][0]);
        GLD_CHECK_GL("glUniform4iv", "SetVertexShaderConstantI");
        constants->vsIntDirty = FALSE;
    }

    /* Upload PS integer constants (all 16 registers when dirty) */
    if (constants->psIntDirty && constants->psConstLocI >= 0) {
        glUniform4iv(constants->psConstLocI, GLD_PS_INT_CONST_COUNT,
                     &constants->psConstantsI[0][0]);
        GLD_CHECK_GL("glUniform4iv", "SetPixelShaderConstantI");
        constants->psIntDirty = FALSE;
    }

    /*
     * Upload boolean constants.
     * GLSL bool uniforms are set via glUniform1i (0 or 1).
     * We upload each boolean individually since glUniform1iv
     * expects int values and BOOL may differ in size.
     */
    if (constants->vsBoolDirty && constants->vsConstLocB >= 0) {
        int idx;
        for (idx = 0; idx < GLD_VS_BOOL_CONST_COUNT; idx++) {
            glUniform1i(constants->vsConstLocB + idx,
                        constants->vsConstantsB[idx] ? 1 : 0);
        }
        GLD_CHECK_GL("glUniform1i", "SetVertexShaderConstantB");
        constants->vsBoolDirty = FALSE;
    }

    if (constants->psBoolDirty && constants->psConstLocB >= 0) {
        int idx;
        for (idx = 0; idx < GLD_PS_BOOL_CONST_COUNT; idx++) {
            glUniform1i(constants->psConstLocB + idx,
                        constants->psConstantsB[idx] ? 1 : 0);
        }
        GLD_CHECK_GL("glUniform1i", "SetPixelShaderConstantB");
        constants->psBoolDirty = FALSE;
    }
}
