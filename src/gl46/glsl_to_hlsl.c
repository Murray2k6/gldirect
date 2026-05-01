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
* Description:  GLSL to HLSL Shader Model 3.0 transpiler for D3D9.
*               Text-based GLSL->HLSL conversion with D3DCompile integration.
*
*********************************************************************************/

#pragma warning(disable: 4996 4244)

#include "glsl_to_hlsl.h"
#include "../gld_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*---------------------- Macros and type definitions ----------------------*/

#define GLSL_MAX_SOURCE         (256 * 1024)
#define HLSL_MAX_OUTPUT         (1024 * 1024)
#define GLSL_MAX_VARS           128
#define GLSL_MAX_NAME_LEN       128
#define GLSL_MAX_TYPE_LEN       64
#define GLSL_MAX_FRAGDATA       4
#define GLSL_MAX_SAMPLERS       8

#define GLSL_SHADER_VERTEX      0
#define GLSL_SHADER_PIXEL       1

/*
 * ID3DBlob COM interface — minimal definition for accessing
 * D3DCompile output without linking d3dcompiler.lib.
 */
typedef struct ID3DBlob ID3DBlob;
typedef struct ID3DBlobVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ID3DBlob*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ID3DBlob*);
    ULONG   (STDMETHODCALLTYPE *Release)(ID3DBlob*);
    void*   (STDMETHODCALLTYPE *GetBufferPointer)(ID3DBlob*);
    SIZE_T  (STDMETHODCALLTYPE *GetBufferSize)(ID3DBlob*);
} ID3DBlobVtbl;
struct ID3DBlob {
    ID3DBlobVtbl *lpVtbl;
};

typedef HRESULT (WINAPI *PFN_D3DCompile)(
    const void *pSrcData, SIZE_T SrcDataSize,
    const char *pSourceName, const void *pDefines,
    const void *pInclude, const char *pEntrypoint,
    const char *pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs);

/*
 * Variable declaration extracted from GLSL source.
 */
typedef struct {
    char    qualifier[32];
    char    type[GLSL_MAX_TYPE_LEN];
    char    name[GLSL_MAX_NAME_LEN];
    int     location;
    BOOL    isFlat;
} glslVarDecl;

/*
 * Type replacement table entry.
 */
typedef struct {
    const char *glsl;
    const char *hlsl;
} glslTypeMap;

/*---------------------- Module-level state ----------------------*/

static HMODULE          s_hD3DCompiler  = NULL;
static PFN_D3DCompile   s_pfnD3DCompile = NULL;
static BOOL             s_bInitialized  = FALSE;

/*---------------------- Type replacement table ----------------------*/

static const glslTypeMap s_typeReplacements[] = {
    { "vec2",       "float2"    },
    { "vec3",       "float3"    },
    { "vec4",       "float4"    },
    { "ivec2",      "int2"      },
    { "ivec3",      "int3"      },
    { "ivec4",      "int4"      },
    { "uvec2",      "uint2"     },
    { "uvec3",      "uint3"     },
    { "uvec4",      "uint4"     },
    { "bvec2",      "bool2"     },
    { "bvec3",      "bool3"     },
    { "bvec4",      "bool4"     },
    { "mat2",       "float2x2"  },
    { "mat3",       "float3x3"  },
    { "mat4",       "float4x4"  },
    { "mat2x2",     "float2x2"  },
    { "mat3x3",     "float3x3"  },
    { "mat4x4",     "float4x4"  },
    { "mat2x3",     "float2x3"  },
    { "mat2x4",     "float2x4"  },
    { "mat3x2",     "float3x2"  },
    { "mat3x4",     "float3x4"  },
    { "mat4x2",     "float4x2"  },
    { "mat4x3",     "float4x3"  },
    { "samplerCube","samplerCUBE"},
    { NULL, NULL }
};

/*---------------------- Forward declarations ----------------------*/

static BOOL glslDoTranspile(int shaderType, const char *glslSource,
                            char *hlslOut, int hlslBufSize);
static BOOL glslIsWordBoundary(char c);
static char *glslSkipWhitespace(const char *p);
static void glslReplaceWord(char *text, const char *oldWord, const char *newWord);
static void glslReplaceAll(char *text, const char *oldStr, const char *newStr);
static int  glslFindMatchingParen(const char *text, int startPos);
static void glslStripVersionLines(char *src);
static void glslStripPrecisionQualifiers(char *src);
static void glslStripLayoutQualifiers(char *src, glslVarDecl *vars, int *pVarCount);
static void glslApplyTypeReplacements(char *text);
static void glslApplyFunctionReplacements(char *text);
static void glslApplyTextureLodRewrite(char *text);
static void glslReplaceBuiltinVars(char *text, int shaderType);
static const char *glslConvertType(const char *glslType);
static int  glslParseDeclarations(const char *src, int shaderType,
                                  glslVarDecl *attributes, int *pAttrCount,
                                  glslVarDecl *varyings, int *pVaryCount,
                                  glslVarDecl *uniforms, int *pUnifCount);
static void glslExtractMainBody(const char *src, char *body, int bodySize);
static void glslRemoveDeclarationLines(char *src);
static void glslBuildVertexShader(const glslVarDecl *attrs, int attrCount,
                                  const glslVarDecl *varyings, int varyCount,
                                  const glslVarDecl *uniforms, int unifCount,
                                  const char *mainBody, char *hlslOut, int hlslBufSize);
static void glslBuildPixelShader(const glslVarDecl *varyings, int varyCount,
                                 const glslVarDecl *uniforms, int unifCount,
                                 const char *mainBody, char *hlslOut, int hlslBufSize,
                                 BOOL usesFragData, int maxFragData,
                                 BOOL usesFragCoord, BOOL usesFrontFacing);


/**********************************************************************/
/*****            Public API: Init / Shutdown                     *****/
/**********************************************************************/

BOOL glslTranspilerInit(void)
{
    if (s_bInitialized)
        return TRUE;

    s_hD3DCompiler = LoadLibraryA("d3dcompiler_47.dll");
    if (!s_hD3DCompiler) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspilerInit: Failed to load d3dcompiler_47.dll (error %u)\n",
                     GetLastError());
        return FALSE;
    }

    s_pfnD3DCompile = (PFN_D3DCompile)GetProcAddress(s_hD3DCompiler, "D3DCompile");
    if (!s_pfnD3DCompile) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspilerInit: D3DCompile not found in d3dcompiler_47.dll\n");
        FreeLibrary(s_hD3DCompiler);
        s_hD3DCompiler = NULL;
        return FALSE;
    }

    s_bInitialized = TRUE;
    gldLogPrintf(GLDLOG_INFO, "glslTranspilerInit: d3dcompiler_47.dll loaded successfully\n");
    return TRUE;
}

void glslTranspilerShutdown(void)
{
    if (s_hD3DCompiler) {
        FreeLibrary(s_hD3DCompiler);
        s_hD3DCompiler = NULL;
    }
    s_pfnD3DCompile = NULL;
    s_bInitialized = FALSE;
}

/**********************************************************************/
/*****            Public API: Transpile and Compile               *****/
/**********************************************************************/

BOOL glslTranspileAndCompile(int shaderType, const char *glslSource,
                             void **ppBytecode, DWORD *pBytecodeSize)
{
    char *hlslSource = NULL;
    ID3DBlob *pCode = NULL;
    ID3DBlob *pErrors = NULL;
    const char *profile;
    HRESULT hr;
    void *bytecodeData;
    SIZE_T bytecodeLen;

    if (!ppBytecode || !pBytecodeSize || !glslSource)
        return FALSE;

    *ppBytecode = NULL;
    *pBytecodeSize = 0;

    if (!s_bInitialized) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: Transpiler not initialized\n");
        return FALSE;
    }

    if (shaderType != GLSL_SHADER_VERTEX && shaderType != GLSL_SHADER_PIXEL) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: Unsupported shader type %d "
                     "(geometry/tessellation/compute not supported in SM3.0)\n", shaderType);
        return FALSE;
    }

    hlslSource = (char *)malloc(HLSL_MAX_OUTPUT);
    if (!hlslSource) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: Out of memory for HLSL buffer\n");
        return FALSE;
    }

    if (!glslDoTranspile(shaderType, glslSource, hlslSource, HLSL_MAX_OUTPUT)) {
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: GLSL to HLSL transpilation failed\n");
        free(hlslSource);
        return FALSE;
    }

    gldLogPrintf(GLDLOG_DEBUG, "glslTranspileAndCompile: Transpiled HLSL:\n%s\n", hlslSource);

    profile = (shaderType == GLSL_SHADER_VERTEX) ? "vs_3_0" : "ps_3_0";

    hr = s_pfnD3DCompile(
        hlslSource, strlen(hlslSource),
        NULL, NULL, NULL,
        "main",
        profile,
        0, 0,
        &pCode, &pErrors);

    if (FAILED(hr)) {
        if (pErrors) {
            const char *errMsg = (const char *)pErrors->lpVtbl->GetBufferPointer(pErrors);
            gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: D3DCompile failed (0x%08X):\n%s\n",
                         hr, errMsg ? errMsg : "(no error message)");
            pErrors->lpVtbl->Release(pErrors);
        } else {
            gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: D3DCompile failed (0x%08X)\n", hr);
        }
        free(hlslSource);
        return FALSE;
    }

    if (pErrors) {
        const char *warnMsg = (const char *)pErrors->lpVtbl->GetBufferPointer(pErrors);
        if (warnMsg && warnMsg[0])
            gldLogPrintf(GLDLOG_WARN, "glslTranspileAndCompile: D3DCompile warnings:\n%s\n", warnMsg);
        pErrors->lpVtbl->Release(pErrors);
    }

    bytecodeData = pCode->lpVtbl->GetBufferPointer(pCode);
    bytecodeLen = pCode->lpVtbl->GetBufferSize(pCode);

    *ppBytecode = malloc((size_t)bytecodeLen);
    if (!*ppBytecode) {
        pCode->lpVtbl->Release(pCode);
        free(hlslSource);
        return FALSE;
    }
    memcpy(*ppBytecode, bytecodeData, (size_t)bytecodeLen);
    *pBytecodeSize = (DWORD)bytecodeLen;

    pCode->lpVtbl->Release(pCode);
    free(hlslSource);

    gldLogPrintf(GLDLOG_INFO, "glslTranspileAndCompile: Compiled %s (%u bytes bytecode)\n",
                 profile, (unsigned)*pBytecodeSize);
    return TRUE;
}

/**********************************************************************/
/*****            Public API: Create D3D9 Shaders                *****/
/**********************************************************************/

BOOL glslCreateVertexShader(IDirect3DDevice9 *pDev, const void *bytecode, DWORD size,
                            IDirect3DVertexShader9 **ppShader)
{
    HRESULT hr;
    if (!pDev || !bytecode || !ppShader || size == 0)
        return FALSE;
    (void)size;
    hr = IDirect3DDevice9_CreateVertexShader(pDev, (const DWORD *)bytecode, ppShader);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "glslCreateVertexShader: CreateVertexShader failed (0x%08X)\n", hr);
        return FALSE;
    }
    return TRUE;
}

BOOL glslCreatePixelShader(IDirect3DDevice9 *pDev, const void *bytecode, DWORD size,
                           IDirect3DPixelShader9 **ppShader)
{
    HRESULT hr;
    if (!pDev || !bytecode || !ppShader || size == 0)
        return FALSE;
    (void)size;
    hr = IDirect3DDevice9_CreatePixelShader(pDev, (const DWORD *)bytecode, ppShader);
    if (FAILED(hr)) {
        gldLogPrintf(GLDLOG_ERROR, "glslCreatePixelShader: CreatePixelShader failed (0x%08X)\n", hr);
        return FALSE;
    }
    return TRUE;
}

void glslFreeBytecode(void *pBytecode)
{
    if (pBytecode)
        free(pBytecode);
}


/**********************************************************************/
/*****            String Utility Helpers                          *****/
/**********************************************************************/

static BOOL glslIsWordBoundary(char c)
{
    if (c == '\0') return TRUE;
    if (c == '_')  return FALSE;
    if (isalnum((unsigned char)c)) return FALSE;
    return TRUE;
}

static char *glslSkipWhitespace(const char *p)
{
    while (*p && (*p == ' ' || *p == '\t'))
        p++;
    return (char *)p;
}

static void glslReplaceWord(char *text, const char *oldWord, const char *newWord)
{
    char *buf;
    int textLen, oldLen, newLen, bufLen;
    char *src, *dst, *found;

    if (!text || !oldWord || !newWord)
        return;
    oldLen = (int)strlen(oldWord);
    newLen = (int)strlen(newWord);
    if (oldLen == 0)
        return;

    textLen = (int)strlen(text);
    bufLen = textLen * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT)
        bufLen = HLSL_MAX_OUTPUT;

    buf = (char *)malloc(bufLen);
    if (!buf) return;

    src = text;
    dst = buf;

    while ((found = strstr(src, oldWord)) != NULL) {
        BOOL leftOk  = (found == text) || glslIsWordBoundary(*(found - 1));
        BOOL rightOk = glslIsWordBoundary(*(found + oldLen));

        if (leftOk && rightOk) {
            int prefixLen = (int)(found - src);
            if (dst + prefixLen + newLen >= buf + bufLen - 1) break;
            memcpy(dst, src, prefixLen);
            dst += prefixLen;
            memcpy(dst, newWord, newLen);
            dst += newLen;
            src = found + oldLen;
        } else {
            int prefixLen = (int)(found - src) + 1;
            if (dst + prefixLen >= buf + bufLen - 1) break;
            memcpy(dst, src, prefixLen);
            dst += prefixLen;
            src = found + 1;
        }
    }
    {
        int remaining = (int)strlen(src);
        if (dst + remaining < buf + bufLen) {
            memcpy(dst, src, remaining);
            dst += remaining;
        }
    }
    *dst = '\0';
    strcpy(text, buf);
    free(buf);
}

static void glslReplaceAll(char *text, const char *oldStr, const char *newStr)
{
    char *buf;
    int textLen, oldLen, newLen, bufLen;
    char *src, *dst, *found;

    if (!text || !oldStr || !newStr)
        return;
    oldLen = (int)strlen(oldStr);
    newLen = (int)strlen(newStr);
    if (oldLen == 0)
        return;

    textLen = (int)strlen(text);
    bufLen = textLen * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT)
        bufLen = HLSL_MAX_OUTPUT;

    buf = (char *)malloc(bufLen);
    if (!buf) return;

    src = text;
    dst = buf;

    while ((found = strstr(src, oldStr)) != NULL) {
        int prefixLen = (int)(found - src);
        if (dst + prefixLen + newLen >= buf + bufLen - 1) break;
        memcpy(dst, src, prefixLen);
        dst += prefixLen;
        memcpy(dst, newStr, newLen);
        dst += newLen;
        src = found + oldLen;
    }
    {
        int remaining = (int)strlen(src);
        if (dst + remaining < buf + bufLen) {
            memcpy(dst, src, remaining);
            dst += remaining;
        }
    }
    *dst = '\0';
    strcpy(text, buf);
    free(buf);
}

static int glslFindMatchingParen(const char *text, int startPos)
{
    int depth = 0;
    int i = startPos;
    if (text[i] != '(') return -1;
    for (; text[i]; i++) {
        if (text[i] == '(') depth++;
        else if (text[i] == ')') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}


/**********************************************************************/
/*****            Preprocessing                                  *****/
/**********************************************************************/

static void glslStripVersionLines(char *src)
{
    char *line = src;
    while (line && *line) {
        char *next = strchr(line, '\n');
        char *trimmed = glslSkipWhitespace(line);
        if (strncmp(trimmed, "#version", 8) == 0) {
            char *end = next ? next : (line + strlen(line));
            while (line < end) { *line = ' '; line++; }
        }
        line = next ? next + 1 : NULL;
    }
}

static void glslStripPrecisionQualifiers(char *src)
{
    char *line, *next;

    /* Remove standalone precision statements */
    line = src;
    while (line && *line) {
        next = strchr(line, '\n');
        {
            char *trimmed = glslSkipWhitespace(line);
            if (strncmp(trimmed, "precision ", 10) == 0) {
                char *end = next ? next : (line + strlen(line));
                while (line < end) { *line = ' '; line++; }
            }
        }
        line = next ? next + 1 : NULL;
    }

    /* Remove inline precision qualifiers */
    glslReplaceWord(src, "highp",   "");
    glslReplaceWord(src, "mediump", "");
    glslReplaceWord(src, "lowp",    "");
}

/*
 * glslStripLayoutQualifiers — parse and remove layout(location=N) qualifiers.
 * Stores the location value in the vars array for later semantic assignment.
 */
static void glslStripLayoutQualifiers(char *src, glslVarDecl *vars, int *pVarCount)
{
    char *p = src;
    (void)vars;
    (void)pVarCount;

    /* Remove layout(...) prefixes from lines, but we parse locations
     * during the declaration parsing phase instead. Here we just strip
     * the layout(...) text so it doesn't confuse later parsing. */
    while ((p = strstr(p, "layout")) != NULL) {
        char *start = p;
        char *paren;
        /* Check it's a word boundary */
        if (p != src && !glslIsWordBoundary(*(p - 1))) {
            p++;
            continue;
        }
        p += 6; /* skip "layout" */
        p = glslSkipWhitespace(p);
        if (*p != '(') continue;
        paren = strchr(p, ')');
        if (!paren) { p++; continue; }
        /* Blank out from "layout" to closing ")" inclusive */
        while (start <= paren) { *start = ' '; start++; }
        p = paren + 1;
    }
}


/**********************************************************************/
/*****            Type and Function Conversions                  *****/
/**********************************************************************/

static const char *glslConvertType(const char *glslType)
{
    int i;
    for (i = 0; s_typeReplacements[i].glsl; i++) {
        if (strcmp(glslType, s_typeReplacements[i].glsl) == 0)
            return s_typeReplacements[i].hlsl;
    }
    return glslType;
}

static void glslApplyTypeReplacements(char *text)
{
    int i;
    for (i = 0; s_typeReplacements[i].glsl; i++) {
        glslReplaceWord(text, s_typeReplacements[i].glsl, s_typeReplacements[i].hlsl);
    }
}

/*
 * glslApplyTextureLodRewrite — rewrite textureLod(s, uv, lod) to
 * tex2Dlod(s, float4((uv), 0, (lod))).
 */
static void glslApplyTextureLodRewrite(char *text)
{
    char *buf;
    int bufLen;
    char *src, *dst;
    const char *funcName = "textureLod";
    int funcLen = (int)strlen(funcName);

    bufLen = (int)strlen(text) * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT) bufLen = HLSL_MAX_OUTPUT;
    buf = (char *)malloc(bufLen);
    if (!buf) return;

    src = text;
    dst = buf;

    while (*src) {
        char *found = strstr(src, funcName);
        if (!found) {
            /* Copy remainder */
            int rem = (int)strlen(src);
            if (dst + rem < buf + bufLen) {
                memcpy(dst, src, rem);
                dst += rem;
            }
            break;
        }

        /* Check word boundary */
        {
            BOOL leftOk  = (found == text) || glslIsWordBoundary(*(found - 1));
            BOOL rightOk = glslIsWordBoundary(*(found + funcLen));

            if (!leftOk || !rightOk) {
                int prefixLen = (int)(found - src) + 1;
                if (dst + prefixLen >= buf + bufLen - 1) break;
                memcpy(dst, src, prefixLen);
                dst += prefixLen;
                src = found + 1;
                continue;
            }
        }

        /* Copy text before the match */
        {
            int prefixLen = (int)(found - src);
            if (dst + prefixLen >= buf + bufLen - 64) break;
            memcpy(dst, src, prefixLen);
            dst += prefixLen;
        }

        /* Find the opening paren */
        {
            char *parenStart = found + funcLen;
            while (*parenStart == ' ' || *parenStart == '\t') parenStart++;
            if (*parenStart == '(') {
                int closeIdx = glslFindMatchingParen(text, (int)(parenStart - text));
                if (closeIdx > 0) {
                    /* Parse the 3 arguments: sampler, uv, lod */
                    char argBuf[2048];
                    int argLen = closeIdx - (int)(parenStart - text) - 1;
                    char *args[3];
                    int argCount = 0;
                    int depth = 0;
                    int i;
                    char *ap;

                    if (argLen > 0 && argLen < (int)sizeof(argBuf) - 1) {
                        memcpy(argBuf, parenStart + 1, argLen);
                        argBuf[argLen] = '\0';

                        /* Split by commas at depth 0 */
                        args[0] = argBuf;
                        argCount = 1;
                        for (i = 0; argBuf[i] && argCount < 3; i++) {
                            if (argBuf[i] == '(' || argBuf[i] == '[') depth++;
                            else if (argBuf[i] == ')' || argBuf[i] == ']') depth--;
                            else if (argBuf[i] == ',' && depth == 0) {
                                argBuf[i] = '\0';
                                args[argCount++] = &argBuf[i + 1];
                            }
                        }

                        if (argCount == 3) {
                            /* Trim whitespace from args */
                            for (i = 0; i < 3; i++) {
                                args[i] = glslSkipWhitespace(args[i]);
                                ap = args[i] + strlen(args[i]) - 1;
                                while (ap > args[i] && (*ap == ' ' || *ap == '\t')) {
                                    *ap = '\0';
                                    ap--;
                                }
                            }
                            /* Emit: tex2Dlod(sampler, float4((uv), 0, (lod))) */
                            dst += sprintf(dst, "tex2Dlod(%s, float4((%s), 0, (%s)))",
                                           args[0], args[1], args[2]);
                            src = text + closeIdx + 1;
                            continue;
                        }
                    }
                }
            }
        }

        /* Fallback: just replace the function name */
        {
            const char *replacement = "tex2Dlod";
            int repLen = (int)strlen(replacement);
            memcpy(dst, replacement, repLen);
            dst += repLen;
            src = found + funcLen;
        }
    }

    *dst = '\0';
    strcpy(text, buf);
    free(buf);
}

static void glslApplyFunctionReplacements(char *text)
{
    /* Simple word-for-word renames */
    glslReplaceWord(text, "mix",            "lerp");
    glslReplaceWord(text, "fract",          "frac");
    glslReplaceWord(text, "mod",            "fmod");
    glslReplaceWord(text, "inversesqrt",    "rsqrt");
    glslReplaceWord(text, "dFdx",           "ddx");
    glslReplaceWord(text, "dFdy",           "ddy");

    /* Texture sampling — order matters: specific names before generic */
    glslReplaceWord(text, "texture2D",      "tex2D");
    glslReplaceWord(text, "textureCube",    "texCUBE");
    glslReplaceWord(text, "texture3D",      "tex3D");

    /* textureLod needs argument rewriting — do it before generic "texture" */
    glslApplyTextureLodRewrite(text);

    /* Generic texture() -> tex2D() (GLSL 1.30+ with sampler2D) */
    glslReplaceWord(text, "texture",        "tex2D");

    /* atan(y, x) -> atan2(y, x) */
    glslReplaceWord(text, "atan",           "atan2");
}

/*
 * glslReplaceBuiltinVars — replace GLSL built-in variables with
 * HLSL equivalents. The actual struct member names are set up
 * during shader wrapping; here we just do the text substitution
 * for the main body.
 */
static void glslReplaceBuiltinVars(char *text, int shaderType)
{
    if (shaderType == GLSL_SHADER_VERTEX) {
        glslReplaceWord(text, "gl_Position",    "_glsl_out.pos");
        glslReplaceWord(text, "gl_PointSize",   "_glsl_out.psize");
        glslReplaceWord(text, "gl_VertexID",    "_glsl_in.vertexId");
    } else {
        glslReplaceWord(text, "gl_FragColor",   "_glsl_fragColor");
        glslReplaceWord(text, "gl_FragCoord",   "_glsl_in.fragCoord");
        glslReplaceWord(text, "gl_FrontFacing", "_glsl_in.frontFacing");
    }

    /* gl_FragData[N] -> _glsl_fragData[N] (handled in pixel shader wrapper) */
    glslReplaceAll(text, "gl_FragData", "_glsl_fragData");
}


/**********************************************************************/
/*****            Declaration Parsing                             *****/
/**********************************************************************/

/*
 * glslTryParseLocation — attempt to parse layout(location=N) from a line.
 * Returns the location value, or -1 if not found.
 */
static int glslTryParseLocation(const char *line)
{
    const char *p = strstr(line, "location");
    if (!p) return -1;
    p += 8;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return -1;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (isdigit((unsigned char)*p))
        return atoi(p);
    return -1;
}

/*
 * glslParseDeclarations — scan GLSL source for attribute, varying, uniform,
 * in, and out declarations. Populates the provided arrays.
 *
 * Returns the total number of declarations found.
 */
static int glslParseDeclarations(const char *src, int shaderType,
                                 glslVarDecl *attributes, int *pAttrCount,
                                 glslVarDecl *varyings, int *pVaryCount,
                                 glslVarDecl *uniforms, int *pUnifCount)
{
    char lineBuf[1024];
    const char *lineStart;
    int totalFound = 0;

    *pAttrCount = 0;
    *pVaryCount = 0;
    *pUnifCount = 0;

    lineStart = src;
    while (lineStart && *lineStart) {
        const char *lineEnd = strchr(lineStart, '\n');
        int lineLen;
        char *trimmed;
        char qualifier[32] = {0};
        char typeName[GLSL_MAX_TYPE_LEN] = {0};
        char varName[GLSL_MAX_NAME_LEN] = {0};
        int location = -1;
        BOOL isFlat = FALSE;
        char *tok;
        char *saveptr;
        int tokenIdx;

        if (lineEnd)
            lineLen = (int)(lineEnd - lineStart);
        else
            lineLen = (int)strlen(lineStart);

        if (lineLen >= (int)sizeof(lineBuf))
            lineLen = (int)sizeof(lineBuf) - 1;

        memcpy(lineBuf, lineStart, lineLen);
        lineBuf[lineLen] = '\0';
        lineStart = lineEnd ? lineEnd + 1 : NULL;

        /* Check for layout location before stripping */
        location = glslTryParseLocation(lineBuf);

        trimmed = glslSkipWhitespace(lineBuf);

        /* Skip preprocessor, comments, empty lines */
        if (trimmed[0] == '#' || trimmed[0] == '/' || trimmed[0] == '\0')
            continue;
        /* Skip lines inside function bodies (heuristic: contains '{' or starts with common statements) */
        if (strchr(trimmed, '{') || strchr(trimmed, '}'))
            continue;

        /* Check for flat qualifier */
        if (strncmp(trimmed, "flat ", 5) == 0) {
            isFlat = TRUE;
            trimmed += 5;
            trimmed = glslSkipWhitespace(trimmed);
        }

        /* Determine qualifier */
        if (strncmp(trimmed, "attribute ", 10) == 0) {
            strcpy(qualifier, "attribute");
        } else if (strncmp(trimmed, "varying ", 8) == 0) {
            strcpy(qualifier, "varying");
        } else if (strncmp(trimmed, "uniform ", 8) == 0) {
            strcpy(qualifier, "uniform");
        } else if (strncmp(trimmed, "in ", 3) == 0) {
            /* GLSL 1.30+: "in" = attribute in VS, varying in PS */
            if (shaderType == GLSL_SHADER_VERTEX)
                strcpy(qualifier, "attribute");
            else
                strcpy(qualifier, "varying");
        } else if (strncmp(trimmed, "out ", 4) == 0) {
            /* "out" = varying in VS, fragdata in PS */
            if (shaderType == GLSL_SHADER_VERTEX)
                strcpy(qualifier, "varying");
            else
                strcpy(qualifier, "fragout");
        } else {
            continue;
        }

        /* Remove layout(...) text, semicolons, array brackets for parsing */
        {
            char *lp = strstr(trimmed, "layout");
            if (lp) {
                char *rp = strchr(lp, ')');
                if (rp) {
                    while (lp <= rp) { *lp = ' '; lp++; }
                }
            }
        }
        {
            char *semi = strchr(trimmed, ';');
            if (semi) *semi = '\0';
        }

        /* Tokenize: [qualifier] type name */
        tokenIdx = 0;
        tok = strtok(trimmed, " \t");
        while (tok) {
            /* Skip the qualifier word itself */
            if (strcmp(tok, "attribute") == 0 || strcmp(tok, "varying") == 0 ||
                strcmp(tok, "uniform") == 0 || strcmp(tok, "in") == 0 ||
                strcmp(tok, "out") == 0 || strcmp(tok, "flat") == 0) {
                tok = strtok(NULL, " \t");
                continue;
            }
            if (tokenIdx == 0) {
                strncpy(typeName, tok, GLSL_MAX_TYPE_LEN - 1);
                tokenIdx++;
            } else if (tokenIdx == 1) {
                /* Remove array suffix like [4] */
                char *bracket = strchr(tok, '[');
                if (bracket) *bracket = '\0';
                strncpy(varName, tok, GLSL_MAX_NAME_LEN - 1);
                tokenIdx++;
                break;
            }
            tok = strtok(NULL, " \t");
        }

        if (tokenIdx < 2 || typeName[0] == '\0' || varName[0] == '\0')
            continue;

        /* Store the declaration */
        {
            glslVarDecl *decl = NULL;
            if (strcmp(qualifier, "attribute") == 0 && *pAttrCount < GLSL_MAX_VARS) {
                decl = &attributes[(*pAttrCount)++];
            } else if (strcmp(qualifier, "varying") == 0 && *pVaryCount < GLSL_MAX_VARS) {
                decl = &varyings[(*pVaryCount)++];
            } else if (strcmp(qualifier, "uniform") == 0 && *pUnifCount < GLSL_MAX_VARS) {
                decl = &uniforms[(*pUnifCount)++];
            } else if (strcmp(qualifier, "fragout") == 0 && *pVaryCount < GLSL_MAX_VARS) {
                /* Fragment outputs stored as varyings with "fragout" qualifier */
                decl = &varyings[(*pVaryCount)++];
            }
            if (decl) {
                strcpy(decl->qualifier, qualifier);
                strncpy(decl->type, typeName, GLSL_MAX_TYPE_LEN - 1);
                strncpy(decl->name, varName, GLSL_MAX_NAME_LEN - 1);
                decl->location = location;
                decl->isFlat = isFlat;
                totalFound++;
            }
        }
    }

    return totalFound;
}


/**********************************************************************/
/*****            Main Body Extraction                           *****/
/**********************************************************************/

/*
 * glslExtractMainBody — extract the body of void main() { ... }
 * from the GLSL source. Strips the function signature and outer braces.
 */
static void glslExtractMainBody(const char *src, char *body, int bodySize)
{
    const char *mainStart;
    const char *braceOpen;
    int depth;
    const char *p;
    const char *bodyStart;
    int bodyLen;

    body[0] = '\0';

    /* Find "void main" or "void  main" */
    mainStart = strstr(src, "void");
    while (mainStart) {
        const char *after = mainStart + 4;
        after = glslSkipWhitespace(after);
        if (strncmp(after, "main", 4) == 0 && glslIsWordBoundary(*(after + 4))) {
            break;
        }
        mainStart = strstr(mainStart + 1, "void");
    }

    if (!mainStart) return;

    /* Find the opening brace */
    braceOpen = strchr(mainStart, '{');
    if (!braceOpen) return;

    /* Find the matching closing brace */
    depth = 0;
    for (p = braceOpen; *p; p++) {
        if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) break;
        }
    }
    if (depth != 0) return;

    /* Extract body between braces */
    bodyStart = braceOpen + 1;
    bodyLen = (int)(p - bodyStart);
    if (bodyLen >= bodySize)
        bodyLen = bodySize - 1;
    if (bodyLen > 0)
        memcpy(body, bodyStart, bodyLen);
    body[bodyLen] = '\0';
}

/*
 * glslRemoveDeclarationLines — blank out attribute/varying/uniform/in/out
 * declaration lines from the source so they don't appear in the main body.
 * Also removes the void main() wrapper.
 */
static void glslRemoveDeclarationLines(char *src)
{
    char *line = src;
    while (line && *line) {
        char *next = strchr(line, '\n');
        char *trimmed = glslSkipWhitespace(line);

        /* Skip "flat" prefix */
        if (strncmp(trimmed, "flat ", 5) == 0)
            trimmed = glslSkipWhitespace(trimmed + 5);

        if (strncmp(trimmed, "attribute ", 10) == 0 ||
            strncmp(trimmed, "varying ",   8) == 0 ||
            strncmp(trimmed, "uniform ",   8) == 0 ||
            (strncmp(trimmed, "in ",  3) == 0 && !strstr(trimmed, "int ")) ||
            strncmp(trimmed, "out ",  4) == 0) {
            /* Check it's a declaration (has a semicolon) not a function param */
            if (strchr(trimmed, ';')) {
                char *end = next ? next : (line + strlen(line));
                while (line < end) { *line = ' '; line++; }
            }
        }

        line = next ? next + 1 : NULL;
    }
}


/**********************************************************************/
/*****            HLSL Shader Construction                       *****/
/**********************************************************************/

/*
 * glslGetSemanticForAttr — determine the HLSL semantic for a vertex
 * shader input attribute based on its name and location.
 */
static const char *glslGetSemanticForAttr(const glslVarDecl *attr, int index)
{
    static char semBuf[64];

    /* If location is specified, use TEXCOORD[N] (location 0 can be POSITION) */
    if (attr->location >= 0) {
        if (attr->location == 0) {
            /* Common convention: location 0 = position */
            return "POSITION";
        }
        sprintf(semBuf, "TEXCOORD%d", attr->location);
        return semBuf;
    }

    /* Heuristic: guess from name */
    if (strstr(attr->name, "osition") || strstr(attr->name, "pos") ||
        strstr(attr->name, "Pos") || strstr(attr->name, "vertex")) {
        return "POSITION";
    }
    if (strstr(attr->name, "ormal") || strstr(attr->name, "norm")) {
        return "NORMAL";
    }
    if (strstr(attr->name, "olor") || strstr(attr->name, "col")) {
        sprintf(semBuf, "COLOR%d", index > 0 ? 1 : 0);
        return semBuf;
    }

    /* Default: TEXCOORD[index] */
    sprintf(semBuf, "TEXCOORD%d", index);
    return semBuf;
}

/*
 * glslGetSemanticForVarying — determine the HLSL semantic for a
 * varying (VS output / PS input).
 */
static const char *glslGetSemanticForVarying(const glslVarDecl *vary, int index)
{
    static char semBuf[64];

    if (vary->location >= 0) {
        sprintf(semBuf, "TEXCOORD%d", vary->location);
        return semBuf;
    }

    sprintf(semBuf, "TEXCOORD%d", index);
    return semBuf;
}

/*
 * glslBuildVertexShader — construct a complete HLSL vertex shader from
 * parsed declarations and the transpiled main body.
 *
 * Output structure:
 *   - Global uniforms
 *   - VS_INPUT struct (from attributes)
 *   - VS_OUTPUT struct (from varyings + gl_Position)
 *   - void main(VS_INPUT _glsl_in) : VS_OUTPUT { ... }
 */
static void glslBuildVertexShader(const glslVarDecl *attrs, int attrCount,
                                  const glslVarDecl *varyings, int varyCount,
                                  const glslVarDecl *uniforms, int unifCount,
                                  const char *mainBody, char *hlslOut, int hlslBufSize)
{
    int off = 0;
    int i;
    BOOL usesPointSize = (strstr(mainBody, "_glsl_out.psize") != NULL);

    hlslOut[0] = '\0';

    /* Emit uniforms as globals */
    for (i = 0; i < unifCount; i++) {
        off += sprintf(hlslOut + off, "%s %s;\n",
                       glslConvertType(uniforms[i].type), uniforms[i].name);
    }
    off += sprintf(hlslOut + off, "\n");

    /* Emit VS_INPUT struct */
    off += sprintf(hlslOut + off, "struct VS_INPUT {\n");
    for (i = 0; i < attrCount; i++) {
        off += sprintf(hlslOut + off, "    %s %s : %s;\n",
                       glslConvertType(attrs[i].type),
                       attrs[i].name,
                       glslGetSemanticForAttr(&attrs[i], i));
    }
    /* gl_VertexID if used */
    if (strstr(mainBody, "_glsl_in.vertexId")) {
        off += sprintf(hlslOut + off, "    int vertexId : SV_VertexID;\n");
    }
    off += sprintf(hlslOut + off, "};\n\n");

    /* Emit VS_OUTPUT struct */
    off += sprintf(hlslOut + off, "struct VS_OUTPUT {\n");
    off += sprintf(hlslOut + off, "    float4 pos : POSITION;\n");
    if (usesPointSize) {
        off += sprintf(hlslOut + off, "    float psize : PSIZE;\n");
    }
    for (i = 0; i < varyCount; i++) {
        off += sprintf(hlslOut + off, "    %s %s : %s;\n",
                       glslConvertType(varyings[i].type),
                       varyings[i].name,
                       glslGetSemanticForVarying(&varyings[i], i));
    }
    off += sprintf(hlslOut + off, "};\n\n");

    /* Emit main function */
    off += sprintf(hlslOut + off, "VS_OUTPUT main(VS_INPUT _glsl_in) {\n");
    off += sprintf(hlslOut + off, "    VS_OUTPUT _glsl_out = (VS_OUTPUT)0;\n");

    /* Declare local aliases for attributes so the body can reference them by name */
    for (i = 0; i < attrCount; i++) {
        off += sprintf(hlslOut + off, "    %s %s = _glsl_in.%s;\n",
                       glslConvertType(attrs[i].type),
                       attrs[i].name, attrs[i].name);
    }

    /* Declare local aliases for varyings (will be written to output) */
    for (i = 0; i < varyCount; i++) {
        off += sprintf(hlslOut + off, "    %s %s;\n",
                       glslConvertType(varyings[i].type),
                       varyings[i].name);
    }

    off += sprintf(hlslOut + off, "\n");

    /* Insert the transpiled main body */
    off += sprintf(hlslOut + off, "%s\n", mainBody);

    /* Copy varying locals to output struct */
    for (i = 0; i < varyCount; i++) {
        off += sprintf(hlslOut + off, "    _glsl_out.%s = %s;\n",
                       varyings[i].name, varyings[i].name);
    }

    off += sprintf(hlslOut + off, "    return _glsl_out;\n");
    off += sprintf(hlslOut + off, "}\n");
}

/*
 * glslBuildPixelShader — construct a complete HLSL pixel shader from
 * parsed declarations and the transpiled main body.
 *
 * Output structure:
 *   - Global uniforms
 *   - Sampler declarations
 *   - PS_INPUT struct (from varyings)
 *   - PS_OUTPUT struct (COLOR0..N for MRT, or single COLOR0)
 *   - float4 main(PS_INPUT _glsl_in) : COLOR0 { ... }
 */
static void glslBuildPixelShader(const glslVarDecl *varyings, int varyCount,
                                 const glslVarDecl *uniforms, int unifCount,
                                 const char *mainBody, char *hlslOut, int hlslBufSize,
                                 BOOL usesFragData, int maxFragData,
                                 BOOL usesFragCoord, BOOL usesFrontFacing)
{
    int off = 0;
    int i;

    hlslOut[0] = '\0';

    /* Emit uniforms as globals (separate samplers from regular uniforms) */
    for (i = 0; i < unifCount; i++) {
        if (strcmp(uniforms[i].type, "sampler2D") == 0 ||
            strcmp(uniforms[i].type, "samplerCube") == 0 ||
            strcmp(uniforms[i].type, "samplerCUBE") == 0 ||
            strcmp(uniforms[i].type, "sampler3D") == 0) {
            /* Sampler declaration — keep as-is for HLSL SM3.0 */
            off += sprintf(hlslOut + off, "sampler %s;\n", uniforms[i].name);
        } else {
            off += sprintf(hlslOut + off, "%s %s;\n",
                           glslConvertType(uniforms[i].type), uniforms[i].name);
        }
    }
    off += sprintf(hlslOut + off, "\n");

    /* Emit PS_INPUT struct */
    off += sprintf(hlslOut + off, "struct PS_INPUT {\n");
    for (i = 0; i < varyCount; i++) {
        /* Skip fragment outputs — they go in PS_OUTPUT */
        if (strcmp(varyings[i].qualifier, "fragout") == 0)
            continue;
        off += sprintf(hlslOut + off, "    %s %s : %s;\n",
                       glslConvertType(varyings[i].type),
                       varyings[i].name,
                       glslGetSemanticForVarying(&varyings[i], i));
    }
    if (usesFragCoord) {
        off += sprintf(hlslOut + off, "    float4 fragCoord : VPOS;\n");
    }
    if (usesFrontFacing) {
        off += sprintf(hlslOut + off, "    float frontFacing : VFACE;\n");
    }
    off += sprintf(hlslOut + off, "};\n\n");

    if (usesFragData && maxFragData > 1) {
        /* MRT output struct */
        off += sprintf(hlslOut + off, "struct PS_OUTPUT {\n");
        for (i = 0; i < maxFragData && i < GLSL_MAX_FRAGDATA; i++) {
            off += sprintf(hlslOut + off, "    float4 color%d : COLOR%d;\n", i, i);
        }
        off += sprintf(hlslOut + off, "};\n\n");

        off += sprintf(hlslOut + off, "PS_OUTPUT main(PS_INPUT _glsl_in) {\n");
        off += sprintf(hlslOut + off, "    PS_OUTPUT _glsl_psout = (PS_OUTPUT)0;\n");
        for (i = 0; i < maxFragData && i < GLSL_MAX_FRAGDATA; i++) {
            off += sprintf(hlslOut + off, "    float4 _glsl_fragData%d = float4(0,0,0,0);\n", i);
        }
    } else {
        /* Single output */
        off += sprintf(hlslOut + off, "float4 main(PS_INPUT _glsl_in) : COLOR0 {\n");
        off += sprintf(hlslOut + off, "    float4 _glsl_fragColor = float4(0,0,0,0);\n");
    }

    /* Declare local aliases for varyings */
    for (i = 0; i < varyCount; i++) {
        if (strcmp(varyings[i].qualifier, "fragout") == 0)
            continue;
        off += sprintf(hlslOut + off, "    %s %s = _glsl_in.%s;\n",
                       glslConvertType(varyings[i].type),
                       varyings[i].name, varyings[i].name);
    }

    off += sprintf(hlslOut + off, "\n");

    /* Insert the transpiled main body */
    off += sprintf(hlslOut + off, "%s\n", mainBody);

    if (usesFragData && maxFragData > 1) {
        /* Copy fragData locals to output struct */
        for (i = 0; i < maxFragData && i < GLSL_MAX_FRAGDATA; i++) {
            off += sprintf(hlslOut + off, "    _glsl_psout.color%d = _glsl_fragData%d;\n", i, i);
        }
        off += sprintf(hlslOut + off, "    return _glsl_psout;\n");
    } else {
        off += sprintf(hlslOut + off, "    return _glsl_fragColor;\n");
    }

    off += sprintf(hlslOut + off, "}\n");
}


/**********************************************************************/
/*****            Core Transpilation                             *****/
/**********************************************************************/

/*
 * glslDetectFragDataUsage — scan for gl_FragData[N] usage and determine
 * the maximum index used.
 */
static BOOL glslDetectFragDataUsage(const char *src, int *pMaxIndex)
{
    const char *p = src;
    int maxIdx = 0;
    BOOL found = FALSE;

    while ((p = strstr(p, "gl_FragData")) != NULL) {
        const char *bracket = p + 11; /* skip "gl_FragData" */
        while (*bracket == ' ' || *bracket == '\t') bracket++;
        if (*bracket == '[') {
            bracket++;
            while (*bracket == ' ' || *bracket == '\t') bracket++;
            if (isdigit((unsigned char)*bracket)) {
                int idx = atoi(bracket);
                if (idx >= maxIdx) maxIdx = idx + 1;
                found = TRUE;
            }
        }
        p++;
    }

    *pMaxIndex = maxIdx;
    return found;
}

/*
 * glslRewriteFragDataAccess — rewrite gl_FragData[N] to _glsl_fragDataN
 * for the pixel shader output.
 */
static void glslRewriteFragDataAccess(char *text, int maxFragData)
{
    int i;
    char oldPat[64], newPat[64];

    for (i = 0; i < maxFragData && i < GLSL_MAX_FRAGDATA; i++) {
        sprintf(oldPat, "_glsl_fragData[%d]", i);
        sprintf(newPat, "_glsl_fragData%d", i);
        glslReplaceAll(text, oldPat, newPat);
    }
}

/*
 * glslStripHelperFunctions — extract any helper functions defined before main()
 * and return them separately. These need to be emitted before the main function
 * in the HLSL output.
 *
 * For simplicity, we detect functions by looking for patterns like:
 *   type name(...) {
 * that are NOT "void main".
 */
static void glslExtractHelperFunctions(const char *src, char *helpers, int helpersSize)
{
    const char *p = src;
    int off = 0;

    helpers[0] = '\0';

    /* Look for function definitions that aren't main() */
    while (*p) {
        const char *lineStart = p;
        const char *lineEnd = strchr(p, '\n');
        char lineBuf[1024];
        int lineLen;

        if (lineEnd)
            lineLen = (int)(lineEnd - lineStart);
        else
            lineLen = (int)strlen(lineStart);

        if (lineLen >= (int)sizeof(lineBuf))
            lineLen = (int)sizeof(lineBuf) - 1;

        memcpy(lineBuf, lineStart, lineLen);
        lineBuf[lineLen] = '\0';

        /* Check if this looks like a function definition (has parens and opening brace) */
        if (strchr(lineBuf, '(') && !strstr(lineBuf, "void main") &&
            !strstr(lineBuf, "attribute ") && !strstr(lineBuf, "varying ") &&
            !strstr(lineBuf, "uniform ") && !strstr(lineBuf, "#")) {
            /* Check if next non-whitespace after ')' is '{' */
            char *closeParen = strchr(lineBuf, ')');
            if (closeParen) {
                char *afterParen = glslSkipWhitespace(closeParen + 1);
                if (*afterParen == '{' || *afterParen == '\0') {
                    /* This might be a helper function — extract it */
                    const char *funcStart = lineStart;
                    const char *braceOpen = strchr(funcStart, '{');
                    if (braceOpen) {
                        int depth = 0;
                        const char *scan = braceOpen;
                        for (; *scan; scan++) {
                            if (*scan == '{') depth++;
                            else if (*scan == '}') {
                                depth--;
                                if (depth == 0) {
                                    int funcLen = (int)(scan - funcStart) + 1;
                                    if (off + funcLen + 2 < helpersSize) {
                                        memcpy(helpers + off, funcStart, funcLen);
                                        off += funcLen;
                                        helpers[off++] = '\n';
                                        helpers[off] = '\0';
                                    }
                                    p = scan + 1;
                                    goto next_iter;
                                }
                            }
                        }
                    }
                }
            }
        }

        p = lineEnd ? lineEnd + 1 : p + strlen(p);
next_iter:;
    }
}

/*
 * glslDoTranspile — the main transpilation pipeline.
 *
 * Steps:
 *   1. Copy and preprocess the GLSL source
 *   2. Parse declarations (attributes, varyings, uniforms)
 *   3. Detect built-in variable usage
 *   4. Extract the main() body
 *   5. Apply type and function replacements
 *   6. Replace built-in variables
 *   7. Build the complete HLSL shader with structs and entry point
 */
static BOOL glslDoTranspile(int shaderType, const char *glslSource,
                            char *hlslOut, int hlslBufSize)
{
    char *workBuf = NULL;
    char *mainBody = NULL;
    char *helpers = NULL;
    glslVarDecl attributes[GLSL_MAX_VARS];
    glslVarDecl varyings[GLSL_MAX_VARS];
    glslVarDecl uniforms[GLSL_MAX_VARS];
    int attrCount = 0, varyCount = 0, unifCount = 0;
    int srcLen;
    BOOL usesFragData = FALSE;
    int maxFragData = 0;
    BOOL usesFragCoord = FALSE;
    BOOL usesFrontFacing = FALSE;

    if (!glslSource || !hlslOut || hlslBufSize < 256)
        return FALSE;

    srcLen = (int)strlen(glslSource);
    if (srcLen == 0 || srcLen > GLSL_MAX_SOURCE)
        return FALSE;

    /* Allocate working buffers */
    workBuf = (char *)malloc(HLSL_MAX_OUTPUT);
    mainBody = (char *)malloc(HLSL_MAX_OUTPUT / 2);
    helpers = (char *)malloc(HLSL_MAX_OUTPUT / 4);
    if (!workBuf || !mainBody || !helpers) {
        free(workBuf);
        free(mainBody);
        free(helpers);
        return FALSE;
    }

    /* Step 1: Copy and preprocess */
    strcpy(workBuf, glslSource);

    /* Detect built-in usage before any transformations */
    if (shaderType == GLSL_SHADER_PIXEL) {
        usesFragData = glslDetectFragDataUsage(workBuf, &maxFragData);
        usesFragCoord = (strstr(workBuf, "gl_FragCoord") != NULL);
        usesFrontFacing = (strstr(workBuf, "gl_FrontFacing") != NULL);
    }

    /* Parse declarations from original source (before stripping) */
    glslParseDeclarations(workBuf, shaderType,
                          attributes, &attrCount,
                          varyings, &varyCount,
                          uniforms, &unifCount);

    gldLogPrintf(GLDLOG_DEBUG, "glslDoTranspile: Found %d attributes, %d varyings, %d uniforms\n",
                 attrCount, varyCount, unifCount);

    /* Strip preprocessor directives and qualifiers */
    glslStripVersionLines(workBuf);
    glslStripPrecisionQualifiers(workBuf);
    glslStripLayoutQualifiers(workBuf, NULL, NULL);

    /* Remove declaration lines */
    glslRemoveDeclarationLines(workBuf);

    /* Extract helper functions (before main) */
    glslExtractHelperFunctions(workBuf, helpers, HLSL_MAX_OUTPUT / 4);

    /* Extract main() body */
    glslExtractMainBody(workBuf, mainBody, HLSL_MAX_OUTPUT / 2);
    if (mainBody[0] == '\0') {
        gldLogPrintf(GLDLOG_ERROR, "glslDoTranspile: Could not find void main() in GLSL source\n");
        free(workBuf);
        free(mainBody);
        free(helpers);
        return FALSE;
    }

    /* Step 5: Apply type replacements to main body and helpers */
    glslApplyTypeReplacements(mainBody);
    glslApplyTypeReplacements(helpers);

    /* Step 6: Apply function replacements */
    glslApplyFunctionReplacements(mainBody);
    glslApplyFunctionReplacements(helpers);

    /* Step 7: Replace built-in variables */
    glslReplaceBuiltinVars(mainBody, shaderType);

    /* For pixel shaders, rewrite gl_FragData[N] access */
    if (shaderType == GLSL_SHADER_PIXEL && usesFragData) {
        glslRewriteFragDataAccess(mainBody, maxFragData);
    }

    /* Step 8: Build the complete HLSL shader */
    hlslOut[0] = '\0';

    /* Prepend helper functions if any */
    if (helpers[0] != '\0') {
        /* Helper functions need type replacements applied too */
        int helpLen = (int)strlen(helpers);
        if (helpLen > 0) {
            /* We'll prepend helpers to the final output after building the shader.
             * For now, store them and we'll combine later. */
        }
    }

    if (shaderType == GLSL_SHADER_VERTEX) {
        glslBuildVertexShader(attributes, attrCount,
                              varyings, varyCount,
                              uniforms, unifCount,
                              mainBody, hlslOut, hlslBufSize);
    } else {
        glslBuildPixelShader(varyings, varyCount,
                             uniforms, unifCount,
                             mainBody, hlslOut, hlslBufSize,
                             usesFragData, maxFragData,
                             usesFragCoord, usesFrontFacing);
    }

    /* Insert helper functions before the main function */
    if (helpers[0] != '\0') {
        char *mainPos = strstr(hlslOut, "VS_OUTPUT main(");
        if (!mainPos)
            mainPos = strstr(hlslOut, "float4 main(");
        if (!mainPos)
            mainPos = strstr(hlslOut, "PS_OUTPUT main(");

        if (mainPos) {
            /* Insert helpers before main */
            int hlslLen = (int)strlen(hlslOut);
            int helpLen = (int)strlen(helpers);
            int insertPos = (int)(mainPos - hlslOut);

            if (hlslLen + helpLen + 2 < hlslBufSize) {
                memmove(mainPos + helpLen + 1, mainPos, hlslLen - insertPos + 1);
                memcpy(mainPos, helpers, helpLen);
                mainPos[helpLen] = '\n';
            }
        }
    }

    free(workBuf);
    free(mainBody);
    free(helpers);
    return TRUE;
}
