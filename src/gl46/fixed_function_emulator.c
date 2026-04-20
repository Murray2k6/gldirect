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

#define STRICT
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glad/gl.h>
#include "fixed_function_emulator.h"
#include "error_handler.h"
#include "gld_log.h"

// ***********************************************************************
// Internal helpers — safe string append
// ***********************************************************************

/*
 * Append a string to a buffer, tracking the current write position.
 * Returns FALSE if the buffer would overflow.
 */
static BOOL sAppend(char *buf, int bufSize, int *pos, const char *str)
{
    int len = (int)strlen(str);
    if (*pos + len >= bufSize)
        return FALSE;
    memcpy(buf + *pos, str, len);
    *pos += len;
    buf[*pos] = '\0';
    return TRUE;
}

/*
 * Append a formatted string to a buffer.
 * Returns FALSE if the buffer would overflow.
 */
static BOOL sAppendf(char *buf, int bufSize, int *pos, const char *fmt, ...)
{
    va_list args;
    int remaining = bufSize - *pos;
    int written;

    if (remaining <= 0)
        return FALSE;

    va_start(args, fmt);
    written = vsnprintf(buf + *pos, remaining, fmt, args);
    va_end(args);

    if (written < 0 || written >= remaining)
        return FALSE;

    *pos += written;
    return TRUE;
}

// ***********************************************************************
// Identity matrix constant
// ***********************************************************************

static const float s_identity[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

// ***********************************************************************
// FNV-1a hash of a GLD_ffStateKey
// ***********************************************************************

static DWORD sHashStateKey(const GLD_ffStateKey *key)
{
    const unsigned char *data = (const unsigned char *)key;
    DWORD hash = 2166136261u;
    int i;

    for (i = 0; i < (int)sizeof(GLD_ffStateKey); i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}


// ***********************************************************************
// 4x4 matrix helpers (column-major storage)
// ***********************************************************************

/*
 * Multiply two 4x4 column-major matrices: out = A * B.
 * out may NOT alias A or B.
 */
static void sMat4Mul(float *out, const float *A, const float *B)
{
    int i, j, k;
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++) {
                sum += A[k * 4 + i] * B[j * 4 + k];
            }
            out[j * 4 + i] = sum;
        }
    }
}

/*
 * Transpose a 4x4 row-major matrix into column-major order.
 * src and dst may NOT alias.
 */
static void sMat4Transpose(float *dst, const float *src)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            dst[j * 4 + i] = src[i * 4 + j];
        }
    }
}

/*
 * Compute the 3x3 normal matrix from a 4x4 model-view matrix.
 * The normal matrix is the inverse transpose of the upper-left 3x3.
 * Output is 9 floats in column-major order.
 *
 * For simplicity, we compute the cofactor matrix of the 3x3 and
 * divide by the determinant.  If the determinant is zero, we fall
 * back to the upper-left 3x3 directly (non-uniform scale edge case).
 */
static void sComputeNormalMatrix(float *out9, const float *mv16)
{
    /* Extract upper-left 3x3 from column-major 4x4 */
    float a00 = mv16[0], a01 = mv16[4], a02 = mv16[8];
    float a10 = mv16[1], a11 = mv16[5], a12 = mv16[9];
    float a20 = mv16[2], a21 = mv16[6], a22 = mv16[10];

    /* Cofactors */
    float c00 =  (a11 * a22 - a12 * a21);
    float c01 = -(a10 * a22 - a12 * a20);
    float c02 =  (a10 * a21 - a11 * a20);
    float c10 = -(a01 * a22 - a02 * a21);
    float c11 =  (a00 * a22 - a02 * a20);
    float c12 = -(a00 * a21 - a01 * a20);
    float c20 =  (a01 * a12 - a02 * a11);
    float c21 = -(a00 * a12 - a02 * a10);
    float c22 =  (a00 * a11 - a01 * a10);

    float det = a00 * c00 + a01 * c01 + a02 * c02;

    if (det != 0.0f && det != -0.0f) {
        float invDet = 1.0f / det;
        /* Inverse transpose = cofactor / det, stored column-major */
        out9[0] = c00 * invDet;  out9[3] = c01 * invDet;  out9[6] = c02 * invDet;
        out9[1] = c10 * invDet;  out9[4] = c11 * invDet;  out9[7] = c12 * invDet;
        out9[2] = c20 * invDet;  out9[5] = c21 * invDet;  out9[8] = c22 * invDet;
    } else {
        /* Degenerate — just use the upper-left 3x3 directly */
        out9[0] = a00;  out9[3] = a01;  out9[6] = a02;
        out9[1] = a10;  out9[4] = a11;  out9[7] = a12;
        out9[2] = a20;  out9[5] = a21;  out9[8] = a22;
    }
}

// ***********************************************************************
// gldInitFixedFuncCache
// ***********************************************************************

void gldInitFixedFuncCache(GLD_fixedFuncCache *cache)
{
    int i;

    if (!cache)
        return;

    memset(cache, 0, sizeof(GLD_fixedFuncCache));

    /* Set all matrices to identity */
    memcpy(cache->worldMatrix, s_identity, sizeof(s_identity));
    memcpy(cache->viewMatrix,  s_identity, sizeof(s_identity));
    memcpy(cache->projMatrix,  s_identity, sizeof(s_identity));
    memcpy(cache->mvMatrix,    s_identity, sizeof(s_identity));
    memcpy(cache->mvpMatrix,   s_identity, sizeof(s_identity));

    for (i = 0; i < GLD_FF_MAX_TEX_STAGES; i++) {
        memcpy(cache->texMatrix[i], s_identity, sizeof(s_identity));
    }

    /* Normal matrix = identity 3x3 */
    cache->normalMatrix[0] = 1.0f; cache->normalMatrix[1] = 0.0f; cache->normalMatrix[2] = 0.0f;
    cache->normalMatrix[3] = 0.0f; cache->normalMatrix[4] = 1.0f; cache->normalMatrix[5] = 0.0f;
    cache->normalMatrix[6] = 0.0f; cache->normalMatrix[7] = 0.0f; cache->normalMatrix[8] = 1.0f;

    cache->bMatricesDirty = TRUE;
    cache->cacheCount     = 0;
    cache->activeProgram  = 0;
}

// ***********************************************************************
// gldDestroyFixedFuncCache
// ***********************************************************************

void gldDestroyFixedFuncCache(GLD_fixedFuncCache *cache)
{
    int i;

    if (!cache)
        return;

    for (i = 0; i < GLD_FF_CACHE_SIZE; i++) {
        if (cache->cache[i].bUsed && cache->cache[i].program != 0) {
            glDeleteProgram(cache->cache[i].program);
        }
    }

    memset(cache, 0, sizeof(GLD_fixedFuncCache));
}


// ***********************************************************************
// Task 15.1 — Fixed-function vertex shader generation
// ***********************************************************************

/*****************************************************************************
 * gldGenerateFFVertexShader
 *
 * Generate a GLSL 4.60 vertex shader for the given fixed-function state.
 *
 * The shader implements:
 *   - MVP transform: gl_Position = uMVP * vec4(aPosition, 1.0)
 *   - Per-vertex Blinn-Phong lighting for directional lights
 *   - Vertex color pass-through
 *   - Texture coordinate pass-through for active stages
 *   - gl_PointSize output when point sprites are enabled
 *   - Material properties and matrices as uniforms
 *
 * Attribute locations match the Buffer_Manager convention:
 *   location 0 = aPosition (vec3)
 *   location 1 = aNormal   (vec3)
 *   location 2 = aColor0   (vec4)
 *   location 3 = aColor1   (vec4)
 *   location 4-11 = aTexCoord0-7 (vec2)
 *****************************************************************************/
BOOL gldGenerateFFVertexShader(const GLD_ffStateKey *key,
                               char *glslBuffer, int bufferSize)
{
    int pos = 0;
    int i;
    BOOL ok = TRUE;

    if (!key || !glslBuffer || bufferSize < 256)
        return FALSE;

    glslBuffer[0] = '\0';

    /* Version and precision */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "#version 460 core\n\n");

    /* Vertex attributes */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec4 aColor0;\n"
        "layout(location = 3) in vec4 aColor1;\n");

    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "layout(location = %d) in vec2 aTexCoord%d;\n",
            4 + i, i);
    }

    ok = ok && sAppend(glslBuffer, bufferSize, &pos, "\n");

    /* Uniforms — transform matrices */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "uniform mat4 uMVP;\n"
        "uniform mat4 uMV;\n"
        "uniform mat3 uNormalMatrix;\n");

    /* Texture transform matrices for active stages */
    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "uniform mat4 uTexMatrix%d;\n", i);
    }

    /* Point size uniform */
    if (key->pointSpriteEnable) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "uniform float uPointSize;\n");
    }

    /* Lighting uniforms */
    if (key->numLights > 0) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\n/* Material properties */\n"
            "uniform vec4 uMatAmbient;\n"
            "uniform vec4 uMatDiffuse;\n"
            "uniform vec4 uMatSpecular;\n"
            "uniform vec4 uMatEmissive;\n"
            "uniform float uMatPower;\n"
            "\n/* Scene ambient */\n"
            "uniform vec4 uSceneAmbient;\n"
            "\n/* Light parameters — directional lights */\n");

        for (i = 0; i < key->numLights; i++) {
            ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
                "uniform vec3 uLightDir%d;\n"
                "uniform vec4 uLightAmbient%d;\n"
                "uniform vec4 uLightDiffuse%d;\n"
                "uniform vec4 uLightSpecular%d;\n",
                i, i, i, i);
        }
    }

    ok = ok && sAppend(glslBuffer, bufferSize, &pos, "\n");

    /* Outputs to fragment shader */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "out vec4 vColor;\n");

    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "out vec2 vTexCoord%d;\n", i);
    }

    if (key->fogMode != D3DFOG_NONE) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "out float vFogDist;\n");
    }

    /* Main function */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "\nvoid main() {\n"
        "    gl_Position = uMVP * vec4(aPosition, 1.0);\n");

    /* Fog distance — eye-space Z distance */
    if (key->fogMode != D3DFOG_NONE) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "    vec4 eyePos = uMV * vec4(aPosition, 1.0);\n"
            "    vFogDist = abs(eyePos.z);\n");
    }

    /* Point size */
    if (key->pointSpriteEnable) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "    gl_PointSize = uPointSize;\n");
    }

    /* Lighting computation */
    if (key->numLights > 0) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\n    /* Per-vertex Blinn-Phong lighting */\n"
            "    vec3 N = normalize(uNormalMatrix * aNormal);\n"
            "    vec4 ambient  = uMatEmissive + uSceneAmbient * uMatAmbient;\n"
            "    vec4 diffuse  = vec4(0.0);\n"
            "    vec4 specular = vec4(0.0);\n\n");

        for (i = 0; i < key->numLights; i++) {
            ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
                "    /* Light %d — directional */\n"
                "    {\n"
                "        vec3 L = normalize(-uLightDir%d);\n"
                "        float NdotL = max(dot(N, L), 0.0);\n"
                "        ambient += uLightAmbient%d * uMatAmbient;\n"
                "        diffuse += uLightDiffuse%d * uMatDiffuse * NdotL;\n"
                "        if (NdotL > 0.0) {\n"
                "            vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));\n"
                "            float NdotH = max(dot(N, H), 0.0);\n"
                "            specular += uLightSpecular%d * uMatSpecular * pow(NdotH, uMatPower);\n"
                "        }\n"
                "    }\n",
                i, i, i, i, i);
        }

        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "    vColor = clamp(ambient + diffuse + specular, 0.0, 1.0);\n"
            "    vColor.a = uMatDiffuse.a;\n");
    } else {
        /* No lighting — pass through vertex color */
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "    vColor = aColor0;\n");
    }

    /* Texture coordinate pass-through with optional transform */
    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "    vTexCoord%d = (uTexMatrix%d * vec4(aTexCoord%d, 0.0, 1.0)).xy;\n",
            i, i, i);
    }

    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "}\n");

    if (!ok) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldGenerateFFVertexShader: GLSL buffer overflow (%d bytes)",
            bufferSize);
        return FALSE;
    }

    return TRUE;
}


// ***********************************************************************
// Task 15.2 — Fixed-function fragment shader generation
// ***********************************************************************

/*****************************************************************************
 * gldGenerateFFFragmentShader
 *
 * Generate a GLSL 4.60 fragment shader for the given fixed-function state.
 *
 * The shader implements:
 *   - Texture sampling and modulation with vertex color
 *   - Fog computation (linear/exp/exp2)
 *   - Alpha test via discard
 *
 * For simplicity, each active texture stage samples its texture and
 * modulates (multiplies) with the running color.  The first stage
 * starts with the interpolated vertex color.
 *
 * Fog is applied after all texture stages.
 * Alpha test is applied last, before output.
 *****************************************************************************/
BOOL gldGenerateFFFragmentShader(const GLD_ffStateKey *key,
                                 char *glslBuffer, int bufferSize)
{
    int pos = 0;
    int i;
    BOOL ok = TRUE;

    if (!key || !glslBuffer || bufferSize < 256)
        return FALSE;

    glslBuffer[0] = '\0';

    /* Version */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "#version 460 core\n\n");

    /* Inputs from vertex shader */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "in vec4 vColor;\n");

    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "in vec2 vTexCoord%d;\n", i);
    }

    if (key->fogMode != D3DFOG_NONE) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "in float vFogDist;\n");
    }

    ok = ok && sAppend(glslBuffer, bufferSize, &pos, "\n");

    /* Texture sampler uniforms */
    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "uniform sampler2D uTexture%d;\n", i);
    }

    /* Fog uniforms */
    if (key->fogMode != D3DFOG_NONE) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\nuniform vec4  uFogColor;\n"
            "uniform float uFogStart;\n"
            "uniform float uFogEnd;\n"
            "uniform float uFogDensity;\n");
    }

    /* Alpha test uniforms */
    if (key->alphaTestEnable) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\nuniform int   uAlphaFunc;\n"
            "uniform float uAlphaRef;\n");
    }

    /* Output */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "\nlayout(location = 0) out vec4 fragColor;\n");

    /* Main function */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "\nvoid main() {\n"
        "    vec4 color = vColor;\n\n");

    /* Texture stage operations — simple modulate (multiply) */
    for (i = 0; i < key->numTexStages; i++) {
        ok = ok && sAppendf(glslBuffer, bufferSize, &pos,
            "    /* Texture stage %d — MODULATE */\n"
            "    {\n"
            "        vec4 texel = texture(uTexture%d, vTexCoord%d);\n"
            "        color *= texel;\n"
            "    }\n",
            i, i, i);
    }

    /* Fog computation */
    if (key->fogMode != D3DFOG_NONE) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\n    /* Fog computation */\n"
            "    float fogFactor = 1.0;\n");

        switch (key->fogMode) {
        case D3DFOG_LINEAR:
            ok = ok && sAppend(glslBuffer, bufferSize, &pos,
                "    fogFactor = clamp((uFogEnd - vFogDist) / (uFogEnd - uFogStart), 0.0, 1.0);\n");
            break;
        case D3DFOG_EXP:
            ok = ok && sAppend(glslBuffer, bufferSize, &pos,
                "    fogFactor = clamp(exp(-uFogDensity * vFogDist), 0.0, 1.0);\n");
            break;
        case D3DFOG_EXP2:
            ok = ok && sAppend(glslBuffer, bufferSize, &pos,
                "    {\n"
                "        float f = uFogDensity * vFogDist;\n"
                "        fogFactor = clamp(exp(-(f * f)), 0.0, 1.0);\n"
                "    }\n");
            break;
        }

        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "    color.rgb = mix(uFogColor.rgb, color.rgb, fogFactor);\n");
    }

    /* Alpha test via discard */
    if (key->alphaTestEnable) {
        ok = ok && sAppend(glslBuffer, bufferSize, &pos,
            "\n    /* Alpha test */\n"
            "    bool alphaPass = true;\n"
            "    if      (uAlphaFunc == 1) alphaPass = false;                       /* NEVER  */\n"
            "    else if (uAlphaFunc == 2) alphaPass = (color.a <  uAlphaRef);      /* LESS   */\n"
            "    else if (uAlphaFunc == 3) alphaPass = (color.a == uAlphaRef);      /* EQUAL  */\n"
            "    else if (uAlphaFunc == 4) alphaPass = (color.a <= uAlphaRef);      /* LEQUAL */\n"
            "    else if (uAlphaFunc == 5) alphaPass = (color.a >  uAlphaRef);      /* GREATER*/\n"
            "    else if (uAlphaFunc == 6) alphaPass = (color.a != uAlphaRef);      /* NOTEQUAL*/\n"
            "    else if (uAlphaFunc == 7) alphaPass = (color.a >= uAlphaRef);      /* GEQUAL */\n"
            "    /* uAlphaFunc == 8: ALWAYS — alphaPass stays true */\n"
            "    if (!alphaPass) discard;\n");
    }

    /* Final output */
    ok = ok && sAppend(glslBuffer, bufferSize, &pos,
        "\n    fragColor = color;\n"
        "}\n");

    if (!ok) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldGenerateFFFragmentShader: GLSL buffer overflow (%d bytes)",
            bufferSize);
        return FALSE;
    }

    return TRUE;
}


// ***********************************************************************
// Task 15.3 — Shader variant caching and state-driven selection
// ***********************************************************************

/*****************************************************************************
 * sCompileAndLinkFF
 *
 * Internal helper: compile vertex + fragment shader sources and link
 * them into a program.  Returns the program id, or 0 on failure.
 *****************************************************************************/
static GLuint sCompileAndLinkFF(const char *vsSrc, const char *fsSrc)
{
    GLuint vs, fs, prog;
    GLint status;
    char infoLog[1024];

    /* Compile vertex shader */
    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (!status) {
        glGetShaderInfoLog(vs, sizeof(infoLog), NULL, infoLog);
        gldLogPrintf(GLDLOG_ERROR,
            "FF vertex shader compile failed: %s", infoLog);
        glDeleteShader(vs);
        return 0;
    }

    /* Compile fragment shader */
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (!status) {
        glGetShaderInfoLog(fs, sizeof(infoLog), NULL, infoLog);
        gldLogPrintf(GLDLOG_ERROR,
            "FF fragment shader compile failed: %s", infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    /* Link program */
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        glGetProgramInfoLog(prog, sizeof(infoLog), NULL, infoLog);
        gldLogPrintf(GLDLOG_ERROR,
            "FF program link failed: %s", infoLog);
        glDeleteProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    /* Shaders can be detached and deleted after linking */
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLD_CHECK_GL("sCompileAndLinkFF", "FixedFunctionEmulator");

    return prog;
}

// ***********************************************************************

/*****************************************************************************
 * gldSelectFFProgram
 *
 * Select (or generate+compile+cache) the fixed-function shader program
 * matching the given state key, and bind it via glUseProgram.
 *
 * Cache lookup:
 *   1. Compute FNV-1a hash of the state key.
 *   2. Linear scan the cache for a matching hash + key.
 *   3. On hit, bind and return.
 *   4. On miss, generate shaders, compile, link, cache, bind.
 *
 * If the cache is full (GLD_FF_CACHE_SIZE entries), the oldest entry
 * (index 0) is evicted.
 *****************************************************************************/
GLuint gldSelectFFProgram(GLD_fixedFuncCache *cache,
                          const GLD_ffStateKey *key)
{
    DWORD hash;
    int i;
    GLuint program;
    char vsSrc[GLD_FF_MAX_GLSL_SOURCE];
    char fsSrc[GLD_FF_MAX_GLSL_SOURCE];

    if (!cache || !key)
        return 0;

    hash = sHashStateKey(key);

    /* Search cache for existing variant */
    for (i = 0; i < GLD_FF_CACHE_SIZE; i++) {
        if (cache->cache[i].bUsed &&
            cache->cache[i].hash == hash &&
            memcmp(&cache->cache[i].key, key, sizeof(GLD_ffStateKey)) == 0)
        {
            /* Cache hit */
            program = cache->cache[i].program;
            if (program != cache->activeProgram) {
                glUseProgram(program);
                GLD_CHECK_GL("glUseProgram", "SelectFFProgram");
                cache->activeProgram = program;
            }

            /* Enable/disable point size based on state */
            if (key->pointSpriteEnable) {
                glEnable(GL_PROGRAM_POINT_SIZE);
                GLD_CHECK_GL("glEnable(GL_PROGRAM_POINT_SIZE)", "SelectFFProgram");
            }

            return program;
        }
    }

    /* Cache miss — generate, compile, link */
    gldLogPrintf(GLDLOG_DEBUG,
        "gldSelectFFProgram: cache miss, generating FF shaders "
        "(lights=%d, fog=%d, tex=%d, alpha=%d, point=%d)",
        key->numLights, key->fogMode, key->numTexStages,
        key->alphaTestEnable, key->pointSpriteEnable);

    if (!gldGenerateFFVertexShader(key, vsSrc, sizeof(vsSrc))) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSelectFFProgram: failed to generate vertex shader");
        return 0;
    }

    if (!gldGenerateFFFragmentShader(key, fsSrc, sizeof(fsSrc))) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSelectFFProgram: failed to generate fragment shader");
        return 0;
    }

    program = sCompileAndLinkFF(vsSrc, fsSrc);
    if (program == 0) {
        gldLogPrintf(GLDLOG_ERROR,
            "gldSelectFFProgram: failed to compile/link FF program");
        return 0;
    }

    /* Insert into cache */
    if (cache->cacheCount < GLD_FF_CACHE_SIZE) {
        /* Append to next free slot */
        i = cache->cacheCount;
        cache->cacheCount++;
    } else {
        /* Cache full — evict slot 0 (oldest), shift everything down */
        if (cache->cache[0].program != 0) {
            glDeleteProgram(cache->cache[0].program);
        }
        memmove(&cache->cache[0], &cache->cache[1],
                (GLD_FF_CACHE_SIZE - 1) * sizeof(GLD_ffCacheEntry));
        i = GLD_FF_CACHE_SIZE - 1;
        /* cacheCount stays at GLD_FF_CACHE_SIZE */
    }

    cache->cache[i].key     = *key;
    cache->cache[i].hash    = hash;
    cache->cache[i].program = program;
    cache->cache[i].bUsed   = TRUE;

    /* Bind the new program */
    glUseProgram(program);
    GLD_CHECK_GL("glUseProgram", "SelectFFProgram");
    cache->activeProgram = program;

    /* Enable point size if needed */
    if (key->pointSpriteEnable) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        GLD_CHECK_GL("glEnable(GL_PROGRAM_POINT_SIZE)", "SelectFFProgram");
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldSelectFFProgram: created and cached FF program %u", program);

    return program;
}


// ***********************************************************************
// Task 15.4 — Transform matrix translation
// ***********************************************************************

/*****************************************************************************
 * gldSetTransform46
 *
 * Store a DX9 transform matrix, transposing from DX9 row-major to GL
 * column-major order.  Marks derived matrices as dirty so they will be
 * recomputed before the next draw call.
 *
 * Supported transform types:
 *   D3DTS_WORLD (256)        — world (model) matrix
 *   D3DTS_VIEW  (2)          — view (camera) matrix
 *   D3DTS_PROJECTION (3)     — projection matrix
 *   D3DTS_TEXTURE0-7 (16-23) — per-stage texture transform matrices
 *****************************************************************************/
void gldSetTransform46(GLD_fixedFuncCache *cache,
                       DWORD transformType, const float *matrix)
{
    if (!cache || !matrix) {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetTransform46: NULL parameter (cache=%p, matrix=%p)",
            (void *)cache, (const void *)matrix);
        return;
    }

    if (transformType == D3DTS_WORLD) {
        sMat4Transpose(cache->worldMatrix, matrix);
        cache->bMatricesDirty = TRUE;
    }
    else if (transformType == D3DTS_VIEW) {
        sMat4Transpose(cache->viewMatrix, matrix);
        cache->bMatricesDirty = TRUE;
    }
    else if (transformType == D3DTS_PROJECTION) {
        sMat4Transpose(cache->projMatrix, matrix);
        cache->bMatricesDirty = TRUE;
    }
    else if (transformType >= D3DTS_TEXTURE0 && transformType <= D3DTS_TEXTURE7) {
        int stage = (int)(transformType - D3DTS_TEXTURE0);
        sMat4Transpose(cache->texMatrix[stage], matrix);
        cache->bMatricesDirty = TRUE;
    }
    else {
        gldLogPrintf(GLDLOG_WARN,
            "gldSetTransform46: unrecognised transform type %lu",
            (unsigned long)transformType);
    }
}

// ***********************************************************************

/*****************************************************************************
 * sRecomputeDerivedMatrices
 *
 * Recompute MV (view * world), MVP (proj * MV), and the 3x3 normal
 * matrix (inverse transpose of upper-left 3x3 of MV).
 *****************************************************************************/
static void sRecomputeDerivedMatrices(GLD_fixedFuncCache *cache)
{
    /* MV = view * world */
    sMat4Mul(cache->mvMatrix, cache->viewMatrix, cache->worldMatrix);

    /* MVP = proj * MV */
    sMat4Mul(cache->mvpMatrix, cache->projMatrix, cache->mvMatrix);

    /* Normal matrix = inverse transpose of upper-left 3x3 of MV */
    sComputeNormalMatrix(cache->normalMatrix, cache->mvMatrix);

    cache->bMatricesDirty = FALSE;
}

// ***********************************************************************

/*****************************************************************************
 * gldUploadFFTransforms
 *
 * Recompute derived matrices if dirty, then upload all transform
 * uniforms to the given GL program.
 *
 * Uploaded uniforms:
 *   uMVP          — mat4 model-view-projection
 *   uMV           — mat4 model-view
 *   uNormalMatrix — mat3 normal matrix
 *   uTexMatrix0-7 — mat4 per-stage texture transforms (for active stages)
 *   uPointSize    — float (if the program uses it)
 *****************************************************************************/
void gldUploadFFTransforms(GLD_fixedFuncCache *cache, GLuint program)
{
    GLint loc;
    int i;
    char name[32];

    if (!cache || program == 0)
        return;

    /* Recompute derived matrices if any source matrix changed */
    if (cache->bMatricesDirty) {
        sRecomputeDerivedMatrices(cache);
    }

    /* Upload MVP */
    loc = glGetUniformLocation(program, "uMVP");
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, cache->mvpMatrix);
    }

    /* Upload MV */
    loc = glGetUniformLocation(program, "uMV");
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, cache->mvMatrix);
    }

    /* Upload normal matrix */
    loc = glGetUniformLocation(program, "uNormalMatrix");
    if (loc >= 0) {
        glUniformMatrix3fv(loc, 1, GL_FALSE, cache->normalMatrix);
    }

    /* Upload texture transform matrices */
    for (i = 0; i < GLD_FF_MAX_TEX_STAGES; i++) {
        sprintf(name, "uTexMatrix%d", i);
        loc = glGetUniformLocation(program, name);
        if (loc >= 0) {
            glUniformMatrix4fv(loc, 1, GL_FALSE, cache->texMatrix[i]);
        }
    }

    GLD_CHECK_GL("gldUploadFFTransforms", "SetTransform");
}
