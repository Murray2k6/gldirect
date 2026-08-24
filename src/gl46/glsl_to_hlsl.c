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
/* Shader compilation diagnostics go to the diagnostic log as well as the
 * normal one. gldirect.log is buffered, so when a game stops responding after
 * a failed compile - which is exactly what an engine does when it cannot load
 * its shaders - the process is killed with the explanation still sitting in
 * an unflushed buffer. gldDiagLog flushes every line, so the reason survives. */
#include "../gld_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

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

/* Longest profile ladder any device tier produces (2_a/2_b + 2_0 + slack). */
#define GLSL_MAX_LADDER         4

/* Samplers a single shader may have texelFetch/textureSize lowered against.
 * Matches GLSL_MAX_SAMPLERS ??? a shader cannot reference more samplers than
 * D3D9 has stages to bind them to. */
#define GLSL_MAX_TEXDIM         GLSL_MAX_SAMPLERS

/* Object-like "#define NAME <integer>" lines kept from one shader.  Only ever
 * consulted to give an array uniform its extent, so a modest table is enough:
 * a shader that declares more than this many integer constants is not going to
 * have used the ones past the cap to size an array. */
#define GLSL_MAX_DEFINES        64

/* glslVarDecl::arraySize when a declaration *is* an array but its extent could
 * not be worked out.  Distinct from 0 ("not an array"), because emitting a
 * scalar for an array is a silent miscompile: HLSL accepts foo[0..3] against a
 * scalar float4 foo as a component swizzle, so the shader compiles and then
 * reads the wrong values.  Transpilation stops instead. */
#define GLSL_ARRAY_UNRESOLVED   (-1)

/*
 * ID3DBlob COM interface ??? minimal definition for accessing
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
    int     arraySize;      /* 0 when the declaration is not an array,
                             * GLSL_ARRAY_UNRESOLVED when it is one whose
                             * extent could not be resolved */
    BOOL    isFlat;
} glslVarDecl;

/* Fragment shader outputs need their own statement-level reflection.  The
 * general declaration parser predates GLSL 1.30 and deliberately consumes
 * one declaration per source line.  Modern generators commonly emit an input
 * and a layout-qualified output on the same line, so using that parser alone
 * loses the output name and leaves assignments to an undeclared HLSL symbol. */
typedef struct {
    char    name[GLSL_MAX_NAME_LEN];
    int     location;
} glslFragOutput;

/*
 * One object-like "#define NAME <integer>" collected from the GLSL source.
 *
 * Kept for exactly one purpose: resolving "uniform vec4 bones[MAX_BONES];" to
 * its real extent.  Defines used anywhere else in the shader are not resolved
 * here ??? #define lines never survive into the emitted HLSL at all, which is a
 * separate and wider gap than this table is meant to close.
 */
typedef struct {
    char    name[GLSL_MAX_NAME_LEN];
    int     value;
} glslDefine;

/* A deliberately small preprocessor subset for object-like scalar macros.
 * Real compatibility-profile shaders commonly use numeric tuning constants
 * (for example "#define UVCoordScale 1.0").  Preprocessor lines are removed
 * before HLSL emission, so keeping the spelling in the body leaves an
 * undeclared identifier.  Function-like and expression-valued macros remain
 * outside this lightweight translator and take the software path. */
typedef struct {
    char name[GLSL_MAX_NAME_LEN];
    char replacement[64];
} glslObjectDefine;

/*
 * Type replacement table entry.
 */
typedef struct {
    const char *glsl;
    const char *hlsl;
} glslTypeMap;

/*
 * Samplers whose texelFetch/textureSize calls were lowered onto tex2Dlod, and
 * which therefore need a synthesized _glsl_texdim_<name> uniform carrying
 * (width, height, 1/width, 1/height).  Collected while the function rewrites
 * run and consumed when the HLSL globals are emitted.
 */
typedef struct {
    char    names[GLSL_MAX_TEXDIM][GLSL_MAX_NAME_LEN];
    int     count;
} glslTexDimSet;

/*---------------------- Module-level state ----------------------*/

static HMODULE          s_hD3DCompiler  = NULL;
static PFN_D3DCompile   s_pfnD3DCompile = NULL;
static volatile LONG    s_bInitialized  = FALSE;
static volatile LONG    s_compilerLoadFailureLogged = FALSE;
static SRWLOCK          s_compilerLock = SRWLOCK_INIT;

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

/*
 * Load the architecture-correct compiler from the Windows system directory.
 * A bare LoadLibrary("d3dcompiler_47.dll") can be redirected to, or poisoned by,
 * the game directory and was observed failing with ERROR_INVALID_HANDLE in
 * Amnesia. The absolute-path fallback supports systems where the secure search
 * flag is unavailable.
 */
static HMODULE glslLoadSystemCompiler(char *resolvedPath,
                                      size_t resolvedPathSize,
                                      char *attemptedPath,
                                      size_t attemptedPathSize,
                                      DWORD *loadError)
{
    HMODULE module = NULL;
    DWORD error = ERROR_MOD_NOT_FOUND;
    char systemPath[MAX_PATH];
    UINT systemPathLength;

    if (resolvedPath && resolvedPathSize > 0)
        resolvedPath[0] = '\0';
    if (attemptedPath && attemptedPathSize > 0)
        attemptedPath[0] = '\0';

    SetLastError(ERROR_SUCCESS);
    module = LoadLibraryExA("d3dcompiler_47.dll", NULL,
                            LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
        error = GetLastError();

    if (!module) {
        systemPathLength = GetSystemDirectoryA(systemPath, MAX_PATH);
        if (systemPathLength > 0 &&
            systemPathLength < MAX_PATH &&
            systemPathLength + sizeof("\\d3dcompiler_47.dll") <= MAX_PATH) {
            memcpy(systemPath + systemPathLength,
                   "\\d3dcompiler_47.dll",
                   sizeof("\\d3dcompiler_47.dll"));

            if (attemptedPath && attemptedPathSize > 0) {
                strncpy(attemptedPath, systemPath, attemptedPathSize - 1);
                attemptedPath[attemptedPathSize - 1] = '\0';
            }

            SetLastError(ERROR_SUCCESS);
            module = LoadLibraryExA(systemPath, NULL,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!module)
                error = GetLastError();
        }
    }

    if (module && resolvedPath && resolvedPathSize > 0) {
        DWORD length = GetModuleFileNameA(module, resolvedPath,
                                          (DWORD)resolvedPathSize);
        if (length == 0 || length >= resolvedPathSize)
            resolvedPath[0] = '\0';
    }

    if (loadError)
        *loadError = module ? ERROR_SUCCESS : error;

    return module;
}

/* Live device shader-model ceiling, pushed in by glslSetDeviceCaps.
 * Invalid means "assume vs_3_0/ps_3_0", the behaviour that predates the
 * caps being consulted at all. */
static D3DCAPS9         s_deviceCaps;
static BOOL             s_bDeviceCapsValid = FALSE;

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
    /* The two languages name a non-square matrix by opposite axes: GLSL's
     * matCxR is C columns of R rows, HLSL's floatRxC is R rows of C columns.
     * So the same matrix is mat2x3 in GLSL and float3x2 in HLSL, and mapping
     * mat2x3 onto float2x3 would hand the compiler the transpose ??? which is
     * why "vec3 * uniform mat2x3" arrived as an operand-size error rather than
     * merely as the missing-mul() error it really was.  Square types are
     * unaffected; the pair only differs when C != R. */
    { "mat2x3",     "float3x2"  },
    { "mat2x4",     "float4x2"  },
    { "mat3x2",     "float2x3"  },
    { "mat3x4",     "float4x3"  },
    { "mat4x2",     "float2x4"  },
    { "mat4x3",     "float3x4"  },
    { "samplerCube","samplerCUBE"},
    /* GLSL 1.30's integer samplers describe how the *application* interprets
     * what it sampled; SM3 has one untyped sampler and no return-type
     * distinction, so both collapse onto it.  Whether the integers come back
     * intact is decided entirely by the D3D9 format the (untouched) texture
     * upload path chose for the GL internal format ??? D3D9 has no true integer
     * texture formats, so a wide integer format such as GL_R32UI has no
     * correct representation at all and is not claimed to work here. */
    { "usampler2D", "sampler"   },
    { "isampler2D", "sampler"   },
    /* GLSL's shadow samplers exist for depth-comparison sampling, which SM3
     * expresses on an ordinary sampler plus a manual compare.  A helper
     * overload like "vec4 tex2D( sampler2DShadow sampler, vec3 texcoord )"
     * only ever appears dead in the shipped shaders here ??? every texture
     * uniform in the game is a plain sampler2D ??? but the declaration alone
     * fails D3DCompile, so the shadow spelling is folded onto the plain
     * sampler and the comparison the caller really wants is done in the
     * shader arithmetic. */
    { "sampler1DShadow",  "sampler"    },
    { "sampler2DShadow",  "sampler"    },
    { "samplerCubeShadow","samplerCUBE"},
    /* GLSL multisample samplers have no SM3 equivalent; the declaration alone
     * fails D3DCompile (unrecognized type 'sampler2DMS'), so fold onto the
     * plain sampler the way the integer and shadow variants are. */
    { "sampler2DMS",      "sampler"    },
    { "sampler2DMSArray", "sampler"    },
    { NULL, NULL }
};

/*---------------------- Forward declarations ----------------------*/

/* Bitwise lowering (glslRewriteBitwise): D3DCompile rejects &, |, ^, ~, <<
 * and >> on every Shader Model 3 profile (error X3535), so glslDoTranspile
 * rewrites them into float-domain bit arithmetic before D3DCompile ever sees
 * the text.  The float domain represents every integer below 2^24 exactly
 * (16777216 is the first integer float cannot hold), so the lowering is exact
 * for the values shaders actually use.  Constants are turned into
 * single-operand helper functions that only test the bits the constant sets;
 * non-constant operands fall back to general 24-bit helpers.
 *
 * The collector records, across both buffers that get rewritten, which helper
 * functions the rewritten text needs; glslBitEmitHelpers then appends exactly
 * those functions to the helpers buffer. */
#define GLSL_BIT_MAX_LITS 48

typedef enum {
    GLSL_BIT_NONE = 0,
    /* binary (levels follow GLSL precedence: low to high) */
    GLSL_BIT_TERN,
    GLSL_BIT_LOR,
    GLSL_BIT_LAND,
    GLSL_BIT_OR,
    GLSL_BIT_XOR,
    GLSL_BIT_AND,
    GLSL_BIT_EQ,
    GLSL_BIT_NE,
    GLSL_BIT_LT,
    GLSL_BIT_GT,
    GLSL_BIT_LE,
    GLSL_BIT_GE,
    GLSL_BIT_SHL,
    GLSL_BIT_SHR,
    GLSL_BIT_ADD,
    GLSL_BIT_SUB,
    GLSL_BIT_MUL,
    GLSL_BIT_DIV,
    GLSL_BIT_MOD,
    /* unary */
    GLSL_BIT_NOT,
    /* compound assignments (scanner entry points only) */
    GLSL_BIT_AND_EQ,
    GLSL_BIT_OR_EQ,
    GLSL_BIT_XOR_EQ,
    GLSL_BIT_SHL_EQ,
    GLSL_BIT_SHR_EQ
} glslBitOp;

typedef struct {
    BOOL hasGeneralAnd;
    BOOL hasGeneralOr;
    BOOL hasGeneralXor;
    BOOL hasGeneralShl;
    BOOL hasGeneralShr;
    unsigned int andConst[GLSL_BIT_MAX_LITS]; int andCount;
    unsigned int orConst[GLSL_BIT_MAX_LITS];  int orCount;
    unsigned int xorConst[GLSL_BIT_MAX_LITS]; int xorCount;
    BOOL warnedRange;
    BOOL warnedFallback;
} glslBitCollector;

typedef struct {
    glslBitCollector *col;
    BOOL failed;
} glslBitCtx;

/* Debug helper: if verbose, dump the head-line containing `vec3` that is
 * likely a multi-line initializer.  Used by the pipeline instrumentation
 * to find which pass deletes it. */
static void glslDbgDumpAmbient(const char *tag, const char *text)
{
    const char *hit;
    const char *start;
    int len;
    if (!gldDiagVerboseGet() || !text) return;
    /* Search for `vec3 ambientLight` — the line that gets deleted. */
    hit = strstr(text, "ambientLight");
    if (!hit) return;
    start = hit;
    while (start > text && start[-1] != '\n') start--;
    { const char *end = strchr(start, '\n'); len = end ? (int)(end - start) : 64; }
    gldDiagLog("DBG [%s] ambient: '%.*s'", tag, len, start);
}

static BOOL glslDoTranspile(int shaderType, const char *glslSource,
                            const glslAttributeBinding *bindings,
                            int bindingCount,
                            char *hlslOut, int hlslBufSize);
static BOOL glslIsWordBoundary(char c);
static char *glslSkipWhitespace(const char *p);
static void glslReplaceWord(char *text, const char *oldWord, const char *newWord);
static void glslReplaceAll(char *text, const char *oldStr, const char *newStr);
static int  glslFindMatchingParen(const char *text, int startPos);
static void glslStripVersionLines(char *src);
static void glslStripPrecisionQualifiers(char *src);
static void glslStripLayoutQualifiers(char *src, glslVarDecl *vars, int *pVarCount);
static void glslRewriteVectorSplats(char *text, int textSize);
static void glslRewriteMatrixDiagonal(char *text, int textSize,
                                      const char (*matNames)[GLSL_MAX_NAME_LEN],
                                      int matCount);
static void glslRewriteMatrixProducts(char *text, int textSize,
                                      const char (*matNames)[GLSL_MAX_NAME_LEN],
                                      int matCount);
static int  glslCollectMatrixSymbols(const char *src,
                                     char (*out)[GLSL_MAX_NAME_LEN], int maxOut);
static void glslApplyTypeReplacements(char *text);
static void glslApplyFunctionReplacements(char *text, int textSize,
                                           glslTexDimSet *texDim,
                                           const glslVarDecl *uniforms,
                                           int uniformCount);
static void glslApplyTextureLodRewrite(char *text,
                                       const glslVarDecl *uniforms,
                                       int uniformCount);
static void glslApplyTexelFetchRewrite(char *text, int textSize, glslTexDimSet *texDim);
static void glslApplyTextureSizeRewrite(char *text, int textSize, glslTexDimSet *texDim);
static void glslDetectUnlowerableConstructs(const char *src);
static void glslReplaceBuiltinVars(char *text, int shaderType);
static void glslRewriteBitwise(char *text, int textSize, glslBitCollector *col);
static BOOL glslBitNearUintLiteral(const char *text, int pos);
static void glslBitEmitHelpers(glslBitCollector *col, char *helpers, int helpersSize);
static const char *glslConvertType(const char *glslType);
static int  glslParseDeclarations(const char *src, int shaderType,
                                  glslVarDecl *attributes, int *pAttrCount,
                                  glslVarDecl *varyings, int *pVaryCount,
                                  glslVarDecl *uniforms, int *pUnifCount,
                                  const glslDefine *defines, int defineCount,
                                  BOOL *pUnresolvedArray);
static int  glslCollectDefines(const char *src, glslDefine *out, int maxOut);
static int  glslCollectObjectDefines(const char *src, glslObjectDefine *out,
                                     int maxOut);
static void glslApplyObjectDefines(char *text,
                                   const glslObjectDefine *defines, int count);
static void glslRenameReservedWords(char *text);
static const char *glslReservedWordRenameOf(const char *name);
static BOOL glslNameCollidesWithBuiltin(const char *name);
static void glslExtractMainBody(const char *src, char *body, int bodySize);
static void glslRemoveDeclarationLines(char *src);
static void glslJoinContinuationLines(char *src);
static int  glslCollectFragmentOutputs(const char *src,
                                       glslFragOutput *outputs, int maxOutputs);
static void glslBuildVertexShader(const glslVarDecl *attrs, int attrCount,
                                  const glslVarDecl *varyings, int varyCount,
                                  const glslVarDecl *uniforms, int unifCount,
                                  const char *mainBody, char *hlslOut, int hlslBufSize,
                                  const glslTexDimSet *texDim);
static void glslBuildPixelShader(const glslVarDecl *varyings, int varyCount,
                                 const glslVarDecl *uniforms, int unifCount,
                                 const char *mainBody, char *hlslOut, int hlslBufSize,
                                 BOOL usesFragData, int maxFragData,
                                 BOOL usesFragCoord, BOOL usesFrontFacing,
                                 const glslTexDimSet *texDim);


/**********************************************************************/
/*****            Public API: Init / Shutdown                     *****/
/**********************************************************************/

BOOL glslTranspilerInit(void)
{
    BOOL initialized = FALSE;
    HMODULE compiler = NULL;
    PFN_D3DCompile compileProc = NULL;
    char compilerPath[MAX_PATH];
    char attemptedPath[MAX_PATH];
    DWORD error = ERROR_SUCCESS;

    if (InterlockedCompareExchange(&s_bInitialized, FALSE, FALSE))
        return TRUE;

    AcquireSRWLockExclusive(&s_compilerLock);
    __try {
        if (InterlockedCompareExchange(&s_bInitialized, FALSE, FALSE)) {
            initialized = TRUE;
            __leave;
        }

        compiler = glslLoadSystemCompiler(compilerPath, sizeof(compilerPath),
                                          attemptedPath, sizeof(attemptedPath),
                                          &error);
        if (!compiler) {
            if (InterlockedCompareExchange(&s_compilerLoadFailureLogged,
                                           TRUE, FALSE) == FALSE) {
                const char *path = attemptedPath[0]
                    ? attemptedPath
                    : "Windows system directory\\d3dcompiler_47.dll";
                gldLogPrintf(GLDLOG_ERROR,
                             "glslTranspilerInit: failed to load system compiler '%s' (error %lu)",
                             path, error);
                gldDiagLog("shader: failed to load system compiler '%s' (error %lu)",
                           path, error);
            }
            __leave;
        }

        SetLastError(ERROR_SUCCESS);
        compileProc = (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile");
        if (!compileProc) {
            error = GetLastError();
            if (error == ERROR_SUCCESS)
                error = ERROR_PROC_NOT_FOUND;
            if (InterlockedCompareExchange(&s_compilerLoadFailureLogged,
                                           TRUE, FALSE) == FALSE) {
                const char *path = compilerPath[0]
                    ? compilerPath
                    : "d3dcompiler_47.dll";
                gldLogPrintf(GLDLOG_ERROR,
                             "glslTranspilerInit: D3DCompile missing from '%s' (error %lu)",
                             path, error);
                gldDiagLog("shader: D3DCompile missing from '%s' (error %lu)",
                           path, error);
            }
            FreeLibrary(compiler);
            compiler = NULL;
            __leave;
        }

        s_hD3DCompiler = compiler;
        s_pfnD3DCompile = compileProc;
        InterlockedExchange(&s_compilerLoadFailureLogged, FALSE);
        InterlockedExchange(&s_bInitialized, TRUE);
        gldLogPrintf(GLDLOG_INFO,
                     "glslTranspilerInit: loaded system compiler '%s'",
                     compilerPath[0] ? compilerPath : "d3dcompiler_47.dll");
        gldDiagLog("shader: loaded system compiler '%s'",
                   compilerPath[0] ? compilerPath : "d3dcompiler_47.dll");
        initialized = TRUE;
    }
    __finally {
        ReleaseSRWLockExclusive(&s_compilerLock);
    }

    return initialized;
}

void glslTranspilerShutdown(void)
{
    AcquireSRWLockExclusive(&s_compilerLock);
    __try {
        InterlockedExchange(&s_bInitialized, FALSE);
        s_pfnD3DCompile = NULL;
        if (s_hD3DCompiler) {
            FreeLibrary(s_hD3DCompiler);
            s_hD3DCompiler = NULL;
        }
    }
    __finally {
        ReleaseSRWLockExclusive(&s_compilerLock);
    }
}

void glslSetDeviceCaps(const D3DCAPS9 *pCaps)
{
    if (pCaps) {
        memcpy(&s_deviceCaps, pCaps, sizeof(s_deviceCaps));
        s_bDeviceCapsValid = TRUE;
    } else {
        memset(&s_deviceCaps, 0, sizeof(s_deviceCaps));
        s_bDeviceCapsValid = FALSE;
    }
}

/**********************************************************************/
/*****            Compile-profile ladder                          *****/
/**********************************************************************/

/*
 * The device's reported shader version is a hard ceiling, not a hint:
 * CreateVertexShader/CreatePixelShader reject bytecode above it outright.  So
 * the ladder starts at the highest profile the device admits to and only ever
 * descends from there.
 *
 * There is no point putting lower rungs under a 3.0 ceiling.  Recompiling the
 * same generated HLSL at a lower profile helps only when the ceiling itself is
 * the problem: HLSL's language rules do not change between profiles, so a
 * translation defect produces the identical error at every rung and the retries
 * are pure cost.  What changes across profiles is the instruction/register
 * budget, and that only bites once the ceiling is already below 3.0.
 *
 * The 2_a / 2_b sub-tiers are detected best-effort from the documented
 * capability bits.  The exact thresholds D3DX9's D3DXGetPixelShaderProfile used
 * are not in the public d3d9caps.h and D3DX9 is not in this tree, so the
 * detection is deliberately biased to under-detect: an unrecognised 2_x device
 * falls back to plain 2_0, which every 2_x device can run.
 *
 * Returns the number of rungs written.  Zero means the device cannot run
 * anything this compiler can emit.
 */
static int glslBuildVSLadder(const D3DCAPS9 *caps, const char **ladder, int maxRungs)
{
    int n = 0;

    if (maxRungs <= 0)
        return 0;

    /* No caps known ??? assume the ceiling this transpiler always assumed. */
    if (!caps) {
        ladder[n++] = "vs_3_0";
        return n;
    }

    if (caps->VertexShaderVersion >= D3DVS_VERSION(3, 0)) {
        ladder[n++] = "vs_3_0";
    } else if (caps->VertexShaderVersion >= D3DVS_VERSION(2, 0)) {
        if ((caps->VS20Caps.Caps & D3DVS20CAPS_PREDICATION) && n < maxRungs)
            ladder[n++] = "vs_2_a";
        if (n < maxRungs) ladder[n++] = "vs_2_0";
    } else if (caps->VertexShaderVersion >= D3DVS_VERSION(1, 1)) {
        ladder[n++] = "vs_1_1";
    }
    /* Below vs_1_1 there is no programmable vertex pipeline at all. */

    return n;
}

static int glslBuildPSLadder(const D3DCAPS9 *caps, const char **ladder, int maxRungs)
{
    int n = 0;

    if (maxRungs <= 0)
        return 0;

    if (!caps) {
        ladder[n++] = "ps_3_0";
        return n;
    }

    if (caps->PixelShaderVersion >= D3DPS_VERSION(3, 0)) {
        ladder[n++] = "ps_3_0";
    } else if (caps->PixelShaderVersion >= D3DPS_VERSION(2, 0)) {
        if ((caps->PS20Caps.Caps & D3DPS20CAPS_ARBITRARYSWIZZLE) &&
            (caps->PS20Caps.Caps & D3DPS20CAPS_GRADIENTINSTRUCTIONS)) {
            if (n < maxRungs) ladder[n++] = "ps_2_a";
        } else if (caps->PS20Caps.Caps & D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT) {
            if (n < maxRungs) ladder[n++] = "ps_2_b";
        }
        if (n < maxRungs) ladder[n++] = "ps_2_0";
    }
    /* ps_1_1 through ps_1_4 are deliberately never appended.  d3dcompiler_47
     * refuses all four unconditionally ??? "error X3539: ps_1_x is no longer
     * supported", hr = E_NOTIMPL ??? no matter what the shader contains, so a
     * rung for them would only ever produce a confusing failure instead of the
     * accurate "this device is below what we can target" message. */

    return n;
}

/**********************************************************************/
/*****            Public API: Transpile and Compile               *****/
/**********************************************************************/

BOOL glslTranspileAndCompile(int shaderType, const char *glslSource,
                             void **ppBytecode, DWORD *pBytecodeSize)
{
    return glslTranspileAndCompileBound(shaderType, glslSource, NULL, 0,
                                        ppBytecode, pBytecodeSize);
}

BOOL glslTranspileAndCompileBound(int shaderType, const char *glslSource,
                                  const glslAttributeBinding *bindings,
                                  int bindingCount,
                                  void **ppBytecode, DWORD *pBytecodeSize)
{
    char *hlslSource = NULL;
    ID3DBlob *pCode = NULL;
    ID3DBlob *pErrors = NULL;
    const char *profile = NULL;
    const char *ladder[GLSL_MAX_LADDER];
    const D3DCAPS9 *caps;
    int rungCount, rung;
    HRESULT hr = E_FAIL;
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

    /* Name anything in the GLSL that has no SM3 lowering at all before the
     * compiler gets a chance to report it as a syntax error two rewrites
     * downstream.  Purely a diagnostic: transpilation and compilation then
     * proceed exactly as they would have. */
    glslDetectUnlowerableConstructs(glslSource);
    if (!glslDoTranspile(shaderType, glslSource, bindings, bindingCount,
                         hlslSource, HLSL_MAX_OUTPUT)) {
        gldDiagLog("GLSL->HLSL: transpilation failed before the compiler was reached "
                   "(%s stage)", shaderType == 0 ? "vertex" : "fragment");
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: GLSL to HLSL transpilation failed\n");
        free(hlslSource);
        return FALSE;
    }
    {
        char match[128];
        DWORD matchLength = GetEnvironmentVariableA(
            "GLDIRECT_DUMP_HLSL_MATCH", match, sizeof(match));
        if (matchLength > 0 && matchLength < sizeof(match) &&
            strstr(glslSource, match)) {
            gldDiagLog("GLSL->HLSL: matched '%s' %s HLSL BEGIN\n%s\n"
                       "GLSL->HLSL: matched '%s' HLSL END",
                       match,
                       shaderType == GLSL_SHADER_VERTEX ? "vertex" : "pixel",
                       hlslSource, match);
        }
    }
    caps = s_bDeviceCapsValid ? &s_deviceCaps : NULL;
    rungCount = (shaderType == GLSL_SHADER_VERTEX)
              ? glslBuildVSLadder(caps, ladder, GLSL_MAX_LADDER)
              : glslBuildPSLadder(caps, ladder, GLSL_MAX_LADDER);

    if (rungCount == 0) {
        /* Calling D3DCompile here would be pointless: whatever it produced
         * could not be turned into a shader object on this device.  Say so
         * once, precisely, instead of dumping a compiler transcript that
         * describes the wrong problem. */
        gldDiagLog("D3DCompile skipped: device reports %s 0x%08X, below the "
                   "lowest profile this compiler can target (%s). No %s bytecode "
                   "this device could load can be produced.",
                   shaderType == GLSL_SHADER_VERTEX ? "VertexShaderVersion" : "PixelShaderVersion",
                   (unsigned)(shaderType == GLSL_SHADER_VERTEX
                              ? (caps ? caps->VertexShaderVersion : 0)
                              : (caps ? caps->PixelShaderVersion : 0)),
                   shaderType == GLSL_SHADER_VERTEX ? "vs_1_1" : "ps_2_0",
                   shaderType == GLSL_SHADER_VERTEX ? "vertex shader" : "pixel shader");
        gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: device shader model is below "
                     "the lowest profile D3DCompile can target\n");
        free(hlslSource);
        return FALSE;
    }

    /* Highest supported profile first, descending.  Every rung is within the
     * device's ceiling, so a rung that compiles produces bytecode the device
     * can actually load. */
    for (rung = 0; rung < rungCount; rung++) {
        profile = ladder[rung];
        pCode   = NULL;
        pErrors = NULL;

        hr = s_pfnD3DCompile(
            hlslSource, strlen(hlslSource),
            NULL, NULL, NULL,
            "main",
            profile,
            0, 0,
            &pCode, &pErrors);
        if (SUCCEEDED(hr)) {
            /* A compiled rung still has to clear the ceiling in bytecode, not
             * just in name.  d3dcompiler_47 stamps vs_2_a/ps_2_a output with a
             * 2.1 version token, and a device reporting a 2.0 ceiling refuses
             * that at CreateVertexShader/CreatePixelShader time ??? the very
             * rejection this ladder exists to avoid.  So an overshooting rung
             * counts as a failed rung whenever there is a lower one to fall
             * to; on the last rung it is kept, since something the device may
             * refuse still beats nothing at all. */
            DWORD ceiling, token;

            if (!caps || rung + 1 >= rungCount || !pCode ||
                pCode->lpVtbl->GetBufferSize(pCode) < sizeof(DWORD))
                break;

            ceiling = (shaderType == GLSL_SHADER_VERTEX)
                    ? caps->VertexShaderVersion : caps->PixelShaderVersion;
            token   = *(const DWORD *)pCode->lpVtbl->GetBufferPointer(pCode);
            if (token <= ceiling)
                break;

            gldDiagLog("D3DCompile: profile=%s compiled but produced a %u.%u version "
                       "token, above this device's %u.%u ceiling; dropping to %s",
                       profile,
                       (unsigned)D3DSHADER_VERSION_MAJOR(token),
                       (unsigned)D3DSHADER_VERSION_MINOR(token),
                       (unsigned)D3DSHADER_VERSION_MAJOR(ceiling),
                       (unsigned)D3DSHADER_VERSION_MINOR(ceiling),
                       ladder[rung + 1]);
            if (pErrors) { pErrors->lpVtbl->Release(pErrors); pErrors = NULL; }
            pCode->lpVtbl->Release(pCode);
            pCode = NULL;
            hr = E_FAIL;
            continue;
        }

        if (rung + 1 >= rungCount)
            break;      /* last rung ??? fall through to the full failure report */

        /* Another rung left: drop this rung's diagnostics and try lower.  Only
         * the final rung's error text is worth keeping, since it is the one
         * that describes what the device could actually have run. */
        if (pErrors) { pErrors->lpVtbl->Release(pErrors); pErrors = NULL; }
        if (pCode)   { pCode->lpVtbl->Release(pCode);     pCode   = NULL; }
        gldDiagLog("D3DCompile FAILED (0x%08X) profile=%s; retrying at the next lower "
                   "profile this device supports (%s)", hr, profile, ladder[rung + 1]);
    }

    if (FAILED(hr)) {
        if (rungCount > 1)
            gldDiagLog("D3DCompile: exhausted all %d profile rungs the device supports; "
                       "the diagnostics below are from the last one (%s)",
                       rungCount, profile);
        if (pErrors) {
            const char *errMsg = (const char *)pErrors->lpVtbl->GetBufferPointer(pErrors);
            const char *fatal = errMsg ? strstr(errMsg, "error X") : NULL;
            if (fatal) {
                char fatalLine[768];
                size_t fatalLen = 0;
                while (fatal[fatalLen] && fatal[fatalLen] != '\r' &&
                       fatal[fatalLen] != '\n' &&
                       fatalLen + 1 < sizeof(fatalLine))
                    ++fatalLen;
                memcpy(fatalLine, fatal, fatalLen);
                fatalLine[fatalLen] = '\0';
                gldDiagLog("D3DCompile first fatal diagnostic: %s", fatalLine);
            }
            /* The compiler's own text says which line and construct it rejected;
             * without it every failure looks alike and the only way forward is
             * guesswork. Truncated because a bad translation can produce
             * hundreds of cascading errors and the first ones are the real ones. */
            gldDiagLog("D3DCompile FAILED (0x%08X) profile=%s:\n%.1200s",
                       hr, profile, errMsg ? errMsg : "(no error message)");
            gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: D3DCompile failed (0x%08X):\n%s\n",
                         hr, errMsg ? errMsg : "(no error message)");
            pErrors->lpVtbl->Release(pErrors);
        } else {
            gldDiagLog("D3DCompile FAILED (0x%08X) profile=%s, compiler returned no message",
                       hr, profile);
            gldLogPrintf(GLDLOG_ERROR, "glslTranspileAndCompile: D3DCompile failed (0x%08X)\n", hr);
        }
        /* The HLSL the compiler rejected is the other half of the picture.
         * It has to arrive whole: the compiler names a line and column, and a
         * truncated dump that stops short of that line turns an actionable
         * diagnostic into a guess.  A shader that overruns even this bound is
         * far past anything the SM3 profiles can compile anyway.  gldDiagLog
         * vfprintf's straight to the file, so there is no intermediate buffer
         * this width has to fit inside.
         *
         * Both full-text dumps are gated on verbose diagnostics.  They were
         * always-on once, and a title whose shaders repeatedly fall back to
         * software GL (a normal, expected situation) grew gldirect_diag.log
         * to multiple gigabytes in a single session â€” ~130 KB of source text
         * per failed compile.  The always-on line that names the failure and
         * profile is written before this point; these two dumps are the
         * deep-dive, which is exactly what verbose mode is for. */
        if (gldDiagVerboseGet()) {
            gldDiagLog("D3DCompile: rejected HLSL follows ---\n%.65000s\n--- end HLSL", hlslSource);
            /* The HLSL alone only shows the symptom.  When the fault is in the
             * translation rather than in the shader, the input GLSL is what names
             * the construct that was mishandled, and an application's GLSL is not
             * recoverable from anywhere else afterwards. */
            gldDiagLog("D3DCompile: input GLSL follows ---\n%.65000s\n--- end GLSL", glslSource);
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
/*****            Public API: Constant table reflection           *****/
/**********************************************************************/

/*
 * Parse the CTAB constant table embedded in Shader Model 3 bytecode.
 *
 * SM3 bytecode is a stream of DWORD tokens.  Comment tokens have their low
 * 16 bits set to 0xFFFE and encode their length (in DWORDs, excluding the
 * token itself) in bits 16..30.  The compiler emits the constant table as
 * one such comment whose payload begins with the 'CTAB' FourCC, followed by
 * a header giving the offset and count of an array of constant records.
 * All offsets inside are relative to the start of the header.
 *
 * D3DReflect does not handle SM3, and D3DXGetShaderConstantTable would
 * require the D3DX9 redistributable, so the table is read directly here.
 */
int glslReflectConstants(const void *bytecode, DWORD size,
                         glslUniformMap *out, int maxOut)
{
    const DWORD *tok    = (const DWORD *)bytecode;
    const DWORD *end;
    DWORD        dwords;
    int          found  = 0;

    if (!bytecode || !out || maxOut <= 0 || size < 8)
        return 0;

    dwords = size / 4;
    end    = tok + dwords;
    tok++;                      /* skip the version token */

    while (tok < end) {
        DWORD token = *tok;

        /* Comment token? */
        if ((token & 0x0000FFFF) == 0x0000FFFE) {
            DWORD commentLen = (token >> 16) & 0x7FFF;
            const unsigned char *base = (const unsigned char *)(tok + 1);

            if (tok + 1 + commentLen > end)
                break;

            /* 'CTAB' little-endian */
            if (commentLen >= 7 && *(const DWORD *)base == 0x42415443) {
                const unsigned char *ctab = base + 4;   /* offsets are relative to here */
                DWORD ctabBytes = (commentLen - 1) * 4;
                DWORD constCount, constInfoOff;
                DWORD i;

                /* Header: Size, Creator, Version, Constants, ConstantInfo, Flags, Target */
                constCount   = ((const DWORD *)ctab)[3];
                constInfoOff = ((const DWORD *)ctab)[4];

                if (constInfoOff >= ctabBytes)
                    return 0;

                for (i = 0; i < constCount && found < maxOut; i++) {
                    /* D3DXSHADER_CONSTANTINFO is 20 bytes:
                     *   DWORD Name; WORD RegisterSet; WORD RegisterIndex;
                     *   WORD RegisterCount; WORD Reserved;
                     *   DWORD TypeInfo; DWORD DefaultValue; */
                    const unsigned char *rec = ctab + constInfoOff + i * 20;
                    DWORD nameOff;
                    const char *name;

                    if ((DWORD)(constInfoOff + (i + 1) * 20) > ctabBytes)
                        break;

                    nameOff = *(const DWORD *)(rec + 0);
                    if (nameOff >= ctabBytes)
                        continue;

                    name = (const char *)(ctab + nameOff);

                    strncpy(out[found].name, name, GLSL_UNIFORM_NAME_LEN - 1);
                    out[found].name[GLSL_UNIFORM_NAME_LEN - 1] = '\0';
                    out[found].registerSet   = *(const WORD *)(rec + 4);
                    out[found].registerIndex = *(const WORD *)(rec + 6);
                    out[found].registerCount = *(const WORD *)(rec + 8);
                    found++;
                }

                return found;
            }

            tok += 1 + commentLen;
            continue;
        }

        /* 0x0000FFFF is the end token */
        if (token == 0x0000FFFF)
            break;

        tok++;
    }

    return found;
}


/**********************************************************************/
/*****            Public API: Attribute reflection                *****/
/**********************************************************************/

/* GLSL type name -> the GL enum glGetActiveAttrib reports. */
static int glslTypeNameToGLEnum(const char *type)
{
    if (!strcmp(type, "float")) return 0x1406;  /* GL_FLOAT       */
    if (!strcmp(type, "vec2"))  return 0x8B50;  /* GL_FLOAT_VEC2  */
    if (!strcmp(type, "vec3"))  return 0x8B51;  /* GL_FLOAT_VEC3  */
    if (!strcmp(type, "vec4"))  return 0x8B52;  /* GL_FLOAT_VEC4  */
    if (!strcmp(type, "int"))   return 0x1404;  /* GL_INT         */
    if (!strcmp(type, "ivec2")) return 0x8B53;
    if (!strcmp(type, "ivec3")) return 0x8B54;
    if (!strcmp(type, "ivec4")) return 0x8B55;
    if (!strcmp(type, "mat2"))  return 0x8B5A;  /* GL_FLOAT_MAT2  */
    if (!strcmp(type, "mat3"))  return 0x8B5B;
    if (!strcmp(type, "mat4"))  return 0x8B5C;
    return 0x8B52;                              /* default vec4   */
}

int glslReflectAttributes(const char *glslSource, glslAttribInfo *out, int maxOut)
{
    glslVarDecl *attributes = NULL, *varyings = NULL, *uniforms = NULL;
    int attrCount = 0, varyCount = 0, unifCount = 0;
    int i, n = 0;

    if (!glslSource || !out || maxOut <= 0) return 0;

    attributes = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    varyings   = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    uniforms   = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    if (!attributes || !varyings || !uniforms) {
        free(attributes); free(varyings); free(uniforms);
        return 0;
    }

    /* Attribute reflection answers glGetActiveAttrib, which only ever reports
     * a name, a type and an element count.  An unresolved array extent is a
     * transpilation-stopping condition, and glslDoTranspile is where it stops;
     * repeating the check here would only make glGetActiveAttrib fail early
     * for a shader that is about to fail anyway, with a worse message. */
    glslParseDeclarations(glslSource, GLSL_SHADER_VERTEX,
                          attributes, &attrCount,
                          varyings, &varyCount,
                          uniforms, &unifCount,
                          NULL, 0, NULL);

    for (i = 0; i < attrCount && n < maxOut; i++) {
        strncpy(out[n].name, attributes[i].name, GLSL_UNIFORM_NAME_LEN - 1);
        out[n].name[GLSL_UNIFORM_NAME_LEN - 1] = '\0';
        out[n].glType    = glslTypeNameToGLEnum(attributes[i].type);
        out[n].arraySize = attributes[i].arraySize > 0 ? attributes[i].arraySize : 1;
        n++;
    }

    free(attributes); free(varyings); free(uniforms);
    return n;
}

static int glslTypeComponents(const char *type)
{
    const char *p;
    if (!type) return 4;
    if (!strncmp(type, "mat", 3)) return 4;
    p = type + strlen(type);
    if (p > type && p[-1] >= '2' && p[-1] <= '4') return p[-1] - '0';
    return 1;
}

int glslReflectVaryings(const char *glslSource, int shaderType,
                        glslVaryingInfo *out, int maxOut)
{
    glslVarDecl *attributes = NULL, *varyings = NULL, *uniforms = NULL;
    int attrCount = 0, varyCount = 0, unifCount = 0;
    int i, n = 0;

    if (!glslSource || !out || maxOut <= 0) return 0;
    attributes = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    varyings = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    uniforms = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    if (!attributes || !varyings || !uniforms) {
        free(attributes); free(varyings); free(uniforms);
        return 0;
    }

    glslParseDeclarations(glslSource,
                          shaderType ? GLSL_SHADER_PIXEL : GLSL_SHADER_VERTEX,
                          attributes, &attrCount, varyings, &varyCount,
                          uniforms, &unifCount, NULL, 0, NULL);
    for (i = 0; i < varyCount && n < maxOut; ++i) {
        const char *type = varyings[i].type;
        if (shaderType && !strcmp(varyings[i].qualifier, "fragout"))
            continue;
        strncpy(out[n].name, varyings[i].name, GLSL_UNIFORM_NAME_LEN - 1);
        out[n].name[GLSL_UNIFORM_NAME_LEN - 1] = '\0';
        out[n].glType = glslTypeNameToGLEnum(type);
        out[n].components = glslTypeComponents(type);
        out[n].location = varyings[i].location;
        out[n].arraySize = varyings[i].arraySize > 0 ? varyings[i].arraySize : 1;
        out[n].isFlat = varyings[i].isFlat;
        out[n].isUnsigned = (!strncmp(type, "uvec", 4) || !strcmp(type, "uint"));
        out[n].isInteger = out[n].isUnsigned || !strncmp(type, "ivec", 4) ||
                           !strcmp(type, "int") || !strncmp(type, "bvec", 4) ||
                           !strcmp(type, "bool");
        ++n;
    }
    free(attributes); free(varyings); free(uniforms);
    return n;
}

/* Constant registers one uniform of this type occupies, per element.  D3D9
 * registers are four floats wide, so everything up to a vec4 costs one and a
 * matrix costs one per row. */
static int glslTypeRegisterRows(const char *type)
{
    if (!strncmp(type, "mat", 3)) {
        /* matCxR is C columns of R rows in GLSL and floatRxC in HLSL, which
         * packs one register per row - that is C registers, the first digit. */
        if (isdigit((unsigned char)type[3]))
            return type[3] - '0';
        return 4;
    }
    return 1;
}

int glslReflectDeclaredUniforms(const char *glslSource, glslUniformMap *out, int maxOut)
{
    glslVarDecl *attributes = NULL, *varyings = NULL, *uniforms = NULL;
    int attrCount = 0, varyCount = 0, unifCount = 0;
    int i, n = 0;

    if (!glslSource || !out || maxOut <= 0) return 0;

    attributes = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    varyings   = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    uniforms   = (glslVarDecl *)malloc(sizeof(glslVarDecl) * GLSL_MAX_VARS);
    if (!attributes || !varyings || !uniforms) {
        free(attributes); free(varyings); free(uniforms);
        return 0;
    }

    /* Same reasoning as glslReflectAttributes: an unresolved array extent stops
     * transpilation in glslDoTranspile, so it is not re-checked here. */
    glslParseDeclarations(glslSource, GLSL_SHADER_VERTEX,
                          attributes, &attrCount,
                          varyings, &varyCount,
                          uniforms, &unifCount,
                          NULL, 0, NULL);

    for (i = 0; i < unifCount && n < maxOut; i++) {
        const char *type = uniforms[i].type;
        int elems = uniforms[i].arraySize > 0 ? uniforms[i].arraySize : 1;
        BOOL isSampler = (strstr(type, "sampler") != NULL);

        strncpy(out[n].name, uniforms[i].name, GLSL_UNIFORM_NAME_LEN - 1);
        out[n].name[GLSL_UNIFORM_NAME_LEN - 1] = '\0';
        out[n].registerSet   = isSampler ? GLSL_RS_SAMPLER : GLSL_RS_FLOAT4;
        out[n].registerCount = isSampler ? elems
                                         : glslTypeRegisterRows(type) * elems;
        /* -1 marks "declared, but the compiler assigned it no register".  The
         * caller turns that into a GL location that accepts uploads and drops
         * them, which is what an inactive uniform must do. */
        out[n].registerIndex = -1;
        n++;
    }

    free(attributes); free(varyings); free(uniforms);
    return n;
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

/*
 * glslSkipDeclQualifiers ??? step past anything that may sit in front of a
 * declaration's storage qualifier.
 *
 * GLSL 1.30+ lets a layout block and the interpolation/auxiliary qualifiers
 * appear in any combination ahead of attribute/varying/uniform/in/out, so
 * "layout(location = 1) out vec2 v;" and "smooth out vec2 v;" are both
 * ordinary declarations.  A caller that looked only at the line's first token
 * would take neither for a declaration and drop it.
 *
 * The scan never crosses a newline, so it is safe on a pointer into a whole
 * source buffer as well as on a single extracted line.  Keywords are matched
 * on a word boundary, so an identifier that merely starts with one of them
 * (flatten, into, outer) is left alone.  isFlat, when non-NULL, reports
 * whether a "flat" qualifier was among them.
 */
static char *glslSkipDeclQualifiers(char *p, BOOL *isFlat)
{
    static const char *kQualifiers[] = {
        "flat", "smooth", "noperspective", "centroid", "invariant"
    };
    BOOL progressed = TRUE;

    p = glslSkipWhitespace(p);

    while (progressed) {
        int i;
        progressed = FALSE;

        if (strncmp(p, "layout", 6) == 0 && glslIsWordBoundary(p[6])) {
            char *rp = p + 6;
            while (*rp && *rp != ')' && *rp != '\n')
                rp++;
            if (*rp == ')') {
                p = glslSkipWhitespace(rp + 1);
                progressed = TRUE;
                continue;
            }
        }

        for (i = 0; i < (int)(sizeof(kQualifiers) / sizeof(kQualifiers[0])); i++) {
            size_t len = strlen(kQualifiers[i]);
            if (strncmp(p, kQualifiers[i], len) == 0 && glslIsWordBoundary(p[len])) {
                if (isFlat && strcmp(kQualifiers[i], "flat") == 0)
                    *isFlat = TRUE;
                p = glslSkipWhitespace(p + len);
                progressed = TRUE;
                break;
            }
        }
    }

    return p;
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

/*
 * glslReplaceWordNotCalled ??? glslReplaceWord, except that an occurrence which
 * is immediately followed (past spaces and tabs) by '(' is left alone.
 *
 * Exists for exactly one word.  "texture" is both a perfectly ordinary GLSL
 * identifier and, since GLSL 1.30, the name of the generic sampling builtin;
 * only the identifier spelling collides with HLSL's reserved "texture", and
 * only the call spelling is what glslApplyFunctionReplacements later rewrites
 * to tex2D.  Renaming both would break sampling; renaming neither leaves the
 * X3000 this whole pass exists to remove.
 */
static void glslReplaceWordNotCalled(char *text, const char *oldWord, const char *newWord)
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
        BOOL isCall  = FALSE;

        if (leftOk && rightOk) {
            const char *after = glslSkipWhitespace(found + oldLen);
            isCall = (*after == '(');
        }

        if (leftOk && rightOk && !isCall) {
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

/**********************************************************************/
/*****            Reserved-word and builtin-collision renames     *****/
/**********************************************************************/

/*
 * Words that are legal identifiers in GLSL and reserved in HLSL.
 *
 * A shader is free to name a parameter "sampler", a local "matrix" or a
 * varying "vector"; D3DCompile answers every one of them with "error X3000:
 * syntax error: unexpected token" at the declaration and then "error X3004:
 * undeclared identifier" at every use, which describes the *rewritten HLSL*
 * and names nothing the shader author would recognise.  Renaming them here is
 * safe in a way a type-aware fix would not need to be: the rename is
 * whole-word (glslIsWordBoundary), so "sampler2D", "textureCube" and
 * "column_majorish" are all untouched, and the renamed spelling is uniform
 * across declarations and uses because the pass runs on the whole source
 * before it is split into declarations, main body and helpers.
 *
 * The list is the set confirmed against d3dcompiler_47 to fail as bare
 * identifiers, plus "half", which compiles as one but is a reserved type word
 * in every HLSL reference and costs nothing to move out of the way.
 */
static const glslTypeMap kGlslReservedWords[] = {
    { "sampler",      "_glsl_kw_sampler"      },
    { "texture",      "_glsl_kw_texture"      },
    { "matrix",       "_glsl_kw_matrix"       },
    { "vector",       "_glsl_kw_vector"       },
    { "half",         "_glsl_kw_half"         },
    { "string",       "_glsl_kw_string"       },
    { "technique",    "_glsl_kw_technique"    },
    { "pass",         "_glsl_kw_pass"         },
    { "compile",      "_glsl_kw_compile"      },
    { "pixelshader",  "_glsl_kw_pixelshader"  },
    { "vertexshader", "_glsl_kw_vertexshader" },
    { "row_major",    "_glsl_kw_row_major"    },
    { "column_major", "_glsl_kw_column_major" },
    { NULL,           NULL                    }
};

static void glslRenameReservedWords(char *text)
{
    int i;

    if (!text) return;

    for (i = 0; kGlslReservedWords[i].glsl; i++) {
        if (strcmp(kGlslReservedWords[i].glsl, "texture") == 0)
            glslReplaceWordNotCalled(text, kGlslReservedWords[i].glsl,
                                     kGlslReservedWords[i].hlsl);
        else
            glslReplaceWord(text, kGlslReservedWords[i].glsl,
                            kGlslReservedWords[i].hlsl);
    }
}

/* The renamed spelling of `name`, or NULL when it is not a reserved word.
 * Lets a name collected from the *original* source be brought into the
 * renamed spelling the rewritten body now uses. */
static const char *glslReservedWordRenameOf(const char *name)
{
    int i;

    if (!name) return NULL;

    for (i = 0; kGlslReservedWords[i].glsl; i++)
        if (strcmp(name, kGlslReservedWords[i].glsl) == 0)
            return kGlslReservedWords[i].hlsl;
    return NULL;
}

/*
 * Names a user-defined function must not keep.
 *
 * Three overlapping sets, all of which end the same way if a shader defines a
 * function of that name:
 *
 *   - the GLSL builtins glslApplyFunctionReplacements matches.  A user
 *     function called "textureLod" is rewritten *at its own definition* ???
 *     glslApplyTextureLodRewrite's fallback branch fires because a definition
 *     header does not parse as a 3-argument call ??? producing HLSL like
 *     "float4 tex2Dlod( sampler2D sampler, ... )".
 *   - the HLSL names those become.  Same outcome by a different route: the
 *     definition survives intact and shadows the intrinsic.
 *   - the rest of the SM1-3 intrinsic set.  D3DCompile tolerates a shadowing
 *     definition right up until it is reachable from main() and refers to
 *     itself, at which point it is "error X3500: recursive functions not
 *     allowed" ??? so the same shader compiles or does not depending on whether
 *     the wrapper is dead code, which is not a distinction worth relying on.
 *
 * Renaming is uniform: every occurrence of the name becomes _glsl_userfn_<name>
 * in both the main body and the helpers.  A shader that expects some call
 * sites of one name to resolve to a user overload and others to the builtin
 * cannot be served by a whole-word rename, and is not served here.
 */
static const char *const kGlslCollisionNames[] = {
    /* GLSL-side names glslApplyFunctionReplacements matches */
    "mix", "fract", "mod", "inversesqrt", "dFdx", "dFdy",
    "texture2D", "textureCube", "texture3D", "textureLod",
    "texelFetch", "textureSize", "texture", "atan",
    "lessThan", "greaterThan", "lessThanEqual", "greaterThanEqual",
    "equal", "notEqual",
    /* HLSL-side names they become */
    "lerp", "frac", "fmod", "rsqrt", "ddx", "ddy",
    "tex2D", "texCUBE", "tex3D", "tex2Dlod", "atan2",
    /* the rest of the SM1-3 intrinsic set */
    "abs", "acos", "asin", "ceil", "clamp", "clip", "cos", "cosh", "cross",
    "degrees", "determinant", "distance", "dot", "exp", "exp2", "faceforward",
    "floor", "frexp", "isnan", "isinf", "ldexp", "length", "log", "log2",
    "max", "min", "modf", "mul", "normalize", "pow", "radians", "reflect",
    "refract", "round", "saturate", "sign", "sin", "sincos", "smoothstep",
    "sqrt", "step", "tan", "tanh", "tex1D", "tex2Dbias", "tex2Dgrad",
    "tex2Dproj", "transpose", "trunc",
    "tex1Dproj", "tex3Dproj", "tex1Dbias", "tex3Dbias", "texCUBEbias",
    "tex1Dlod", "tex3Dlod", "texCUBElod",
    NULL
};

static BOOL glslNameCollidesWithBuiltin(const char *name)
{
    int i;

    if (!name || !name[0]) return FALSE;

    for (i = 0; kGlslCollisionNames[i]; i++)
        if (strcmp(name, kGlslCollisionNames[i]) == 0)
            return TRUE;
    return FALSE;
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
 * glslStripLayoutQualifiers ??? parse and remove layout(location=N) qualifiers.
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

/*
 * glslJoinContinuationLines -- collapse multi-line statements onto one line.
 *
 * GLSL treats newlines as whitespace, so a statement can span multiple lines:
 *
 *     vec3 ambientLight = vec3( 0.0 )
 *     + max( ... )
 *     + max( ... );
 *
 * The transpiler's line-oriented passes (glslRemoveDeclarationLines, etc.)
 * operate on individual lines and can accidentally blank the head of such a
 * multi-line statement, orphaning the continuation lines.  Joining all
 * continuation lines onto the head eliminates the class entirely: every
 * expression becomes a single line that no pass can partially delete.
 *
 * A continuation is any line whose last non-whitespace character is not a
 * statement terminator (';', '{', '}').  Blank lines are left alone.
 * Runs in-place: write pointer never exceeds read pointer.
 */
static void glslJoinContinuationLines(char *src)
{
    char *rd = src;
    char *wr = src;

    while (*rd) {
        char *nl = strchr(rd, '\n');
        char *last;

        if (!nl) {
            /* Last line -- copy verbatim. */
            while (*rd) *wr++ = *rd++;
            break;
        }

        /* Find the last non-space/tab before the newline. */
        last = nl - 1;
        while (last >= rd && (*last == ' ' || *last == '\t'))
            last--;

        if (last < rd || *last == ';' || *last == '{' || *last == '}') {
            /* Blank line or line ending with a terminator -- keep as-is. */
            int len = (int)(nl - rd) + 1;     /* include '\n' */
            memcpy(wr, rd, (size_t)len);
            wr += len;
            rd = nl + 1;
        } else {
            /* Continuation -- join with the next line (replace '\n' with ' '). */
            int len = (int)(nl - rd);
            memcpy(wr, rd, (size_t)len);
            wr += len;
            *wr++ = ' ';
            rd = nl + 1;
        }
    }
    *wr = '\0';
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

static const char *glslSamplerType(const glslVarDecl *uniforms, int uniformCount,
                                   const char *expression)
{
    char name[GLSL_MAX_NAME_LEN];
    int i, length = 0;

    if (!uniforms || !expression) return NULL;
    expression = glslSkipWhitespace(expression);
    while ((isalnum((unsigned char)expression[length]) || expression[length] == '_') &&
           length < GLSL_MAX_NAME_LEN - 1)
        ++length;
    if (!length) return NULL;
    memcpy(name, expression, (size_t)length);
    name[length] = '\0';
    for (i = 0; i < uniformCount; ++i)
        if (!strcmp(uniforms[i].name, name))
            return uniforms[i].type;
    return NULL;
}

static const char *glslTextureIntrinsic(const char *samplerType,
                                        BOOL lod, BOOL bias)
{
    const char *base = "tex2D";
    if (samplerType) {
        if (strstr(samplerType, "Cube") || strstr(samplerType, "CUBE"))
            base = "texCUBE";
        else if (strstr(samplerType, "3D"))
            base = "tex3D";
        else if (strstr(samplerType, "1D"))
            base = "tex1D";
    }
    if (lod) {
        if (!strcmp(base, "texCUBE")) return "texCUBElod";
        if (!strcmp(base, "tex3D")) return "tex3Dlod";
        if (!strcmp(base, "tex1D")) return "tex1Dlod";
        return "tex2Dlod";
    }
    if (bias) {
        if (!strcmp(base, "texCUBE")) return "texCUBEbias";
        if (!strcmp(base, "tex3D")) return "tex3Dbias";
        if (!strcmp(base, "tex1D")) return "tex1Dbias";
        return "tex2Dbias";
    }
    return base;
}

static BOOL glslIsSamplerType(const char *type)
{
    return type && strstr(type, "sampler") != NULL;
}

static void glslApplyTypeReplacements(char *text)
{
    int i;
    for (i = 0; s_typeReplacements[i].glsl; i++) {
        glslReplaceWord(text, s_typeReplacements[i].glsl, s_typeReplacements[i].hlsl);
    }
}

/* Number of comma-separated arguments at paren depth 1 in `args`, which must
 * start at the constructor's '(' and end at its matching ')'. */
static int glslConstructorArgCount(const char *args, int len)
{
    int depth = 0, count = 0, i;
    BOOL sawText = FALSE;

    for (i = 0; i < len; i++) {
        char c = args[i];
        if (c == '(' || c == '[') {
            depth++;
            if (depth == 1) continue;       /* the constructor's own paren */
        } else if (c == ')' || c == ']') {
            depth--;
            if (depth == 0) break;
        } else if (c == ',' && depth == 1) {
            count++;
            continue;
        }
        if (depth >= 1 && c != ' ' && c != '\t' && c != '\n' && c != '\r')
            sawText = TRUE;
    }
    return sawText ? count + 1 : 0;
}

/*
 * glslRewriteVectorSplats ??? turn a one-argument vector constructor into a cast.
 *
 * GLSL lets a vector constructor take a single value: vec4(x) splats a scalar
 * across all four components and vec3(v) truncates a wider vector.  HLSL has no
 * such rule.  Its constructors take exactly as many components as the type has,
 * so float4(dot(a, b)) is "error X3014: incorrect number of arguments to
 * numeric-type constructor" ??? which is what every DP3/DP4/DPH/RCP/RSQ/EX2/LG2/
 * SIN/COS/POW an ARB program lowers to was hitting, and what any hand-written
 * GLSL that says vec4(0.0) would hit too.
 *
 * An HLSL cast has exactly the GLSL constructor's meaning in both cases: a
 * scalar cast to a vector replicates, and a wider vector cast to a narrower one
 * truncates.  So the rewrite is vecN(expr) -> ((vecN)(expr)), left in GLSL
 * spelling so the type-replacement pass that runs next renames it to floatN.
 *
 * Only vector types are rewritten.  mat4(x) builds a diagonal matrix in GLSL
 * while (float4x4)x replicates in HLSL, so matrices are deliberately left alone
 * rather than silently mistranslated into something that compiles but is wrong.
 *
 * One pass only rewrites the outermost constructor on any given expression, so
 * the whole scan repeats until it stops changing anything: a scalar PARAM
 * literal reaching DP4 produces vec4(dot(vec4(0.5), b)), and the inner splat
 * only becomes visible once the outer one is no longer a constructor.
 */
static void glslRewriteVectorSplats(char *text, int textSize)
{
    static const char *const vecTypes[] = {
        "vec2", "vec3", "vec4",
        "ivec2", "ivec3", "ivec4",
        "uvec2", "uvec3", "uvec4",
        "bvec2", "bvec3", "bvec4",
        NULL
    };
    char *buf;
    int bufLen;
    int pass;

    if (!text || textSize < 2)
        return;

    bufLen = (int)strlen(text) * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT)
        bufLen = HLSL_MAX_OUTPUT;
    buf = (char *)malloc(bufLen);
    if (!buf)
        return;

    /* Bounded rather than "until stable" so a pathological input cannot spin
     * here; nesting deeper than this does not occur in generated GLSL. */
    for (pass = 0; pass < 8; pass++) {
        const char *src = text;
        char *dst = buf;
        BOOL changed = FALSE;
        int i;

        while (*src) {
            const char *name = NULL;
            int nameLen = 0;
            int openIdx = -1;
            int closeIdx = -1;

            if (src == text || glslIsWordBoundary(*(src - 1))) {
                for (i = 0; vecTypes[i]; i++) {
                    int len = (int)strlen(vecTypes[i]);
                    if (strncmp(src, vecTypes[i], (size_t)len) != 0)
                        continue;
                    {
                        const char *q = glslSkipWhitespace(src + len);
                        if (*q == '(') {
                            name = vecTypes[i];
                            nameLen = len;
                            openIdx = (int)(q - text);
                        }
                    }
                    break;
                }
            }

            if (name)
                closeIdx = glslFindMatchingParen(text, openIdx);

            if (name && closeIdx > openIdx + 1 &&
                glslConstructorArgCount(text + openIdx, closeIdx - openIdx + 1) == 1) {
                int innerLen = closeIdx - openIdx - 1;
                if (dst + innerLen + nameLen + 8 >= buf + bufLen - 1)
                    break;
                *dst++ = '(';
                *dst++ = '(';
                memcpy(dst, name, (size_t)nameLen);
                dst += nameLen;
                *dst++ = ')';
                *dst++ = '(';
                memcpy(dst, text + openIdx + 1, (size_t)innerLen);
                dst += innerLen;
                *dst++ = ')';
                *dst++ = ')';
                src = text + closeIdx + 1;
                changed = TRUE;
                continue;
            }

            if (dst >= buf + bufLen - 1)
                break;
            *dst++ = *src++;
        }
        *dst = '\0';

        if (!changed)
            break;
        /* A scan that ran out of room stopped part-way through the shader.
         * Writing that back would silently truncate it into something that
         * compiles to the wrong thing, so the original is kept instead and the
         * compiler gets to report the real X3014 it would have reported. */
        if (*src != '\0' || (int)(dst - buf) >= textSize)
            break;
        strcpy(text, buf);
    }

    free(buf);
}

/**********************************************************************/
/*****            Matrix lowering                                 *****/
/**********************************************************************/

/*
 * GLSL spells matrix arithmetic with the ordinary operators; HLSL spells the
 * same arithmetic with mul() and gives '*' the component-wise meaning instead.
 * So "uMVP * aPos" is a matrix product in one language and a component-wise
 * multiply in the other ??? which is why it arrives as "error X3020: type
 * mismatch" the moment the operand shapes stop agreeing, and would silently
 * compute the wrong thing whenever they happen to agree.
 *
 * Operand order is preserved.  The matrices themselves are not transposed
 * anywhere: GL hands over column-major data, HLSL's default constant packing is
 * column-major, and _glsUploadMatrices copies straight through, so the HLSL
 * matrix is the same matrix the GLSL source declared.  What differs is only
 * how the two languages *name* a non-square shape, which the type table
 * accounts for.
 *
 * Listed longest-first purely for readability; the word-boundary test is what
 * actually stops "mat2x3" being read as "mat2".
 */
static const char *const kGlslMatrixTypes[] = {
    "mat2x2", "mat2x3", "mat2x4",
    "mat3x2", "mat3x3", "mat3x4",
    "mat4x2", "mat4x3", "mat4x4",
    "mat2", "mat3", "mat4",
    NULL
};

/*
 * glslCollectMatrixSymbols ??? the names this file is willing to treat as
 * matrices.
 *
 * There is no type system here, so the set is whatever a "matN name" spelling
 * appears for anywhere in the source: uniform declarations, local declarations,
 * and function parameters all match the same pattern.  Anything reached
 * indirectly ??? a struct field, a helper's return value ??? is simply not
 * recognised and keeps today's untouched '*', which is a compile failure rather
 * than a silent miscompile.
 */
static int glslCollectMatrixSymbols(const char *src,
                                    char (*out)[GLSL_MAX_NAME_LEN], int maxOut)
{
    int count = 0;
    const char *p;

    if (!src || !out || maxOut <= 0)
        return 0;

    for (p = src; *p; ) {
        BOOL matched = FALSE;
        int i;

        if (p == src || glslIsWordBoundary(*(p - 1))) {
            for (i = 0; kGlslMatrixTypes[i]; i++) {
                int len = (int)strlen(kGlslMatrixTypes[i]);
                const char *q;

                if (strncmp(p, kGlslMatrixTypes[i], (size_t)len) != 0)
                    continue;
                if (!glslIsWordBoundary(p[len]))
                    break;              /* e.g. "mat2" inside "mat2x3" */

                q = glslSkipWhitespace(p + len);
                if (isalpha((unsigned char)*q) || *q == '_') {
                    const char *nameStart = q;
                    int nameLen;
                    while (isalnum((unsigned char)*q) || *q == '_') q++;
                    nameLen = (int)(q - nameStart);
                    if (nameLen < GLSL_MAX_NAME_LEN) {
                        int j;
                        BOOL dup = FALSE;
                        for (j = 0; j < count; j++)
                            if ((int)strlen(out[j]) == nameLen &&
                                strncmp(out[j], nameStart, (size_t)nameLen) == 0) {
                                dup = TRUE;
                                break;
                            }
                        if (!dup && count < maxOut) {
                            memcpy(out[count], nameStart, (size_t)nameLen);
                            out[count][nameLen] = '\0';
                            count++;
                        }
                    }
                    p = q;
                    matched = TRUE;
                }
                break;
            }
        }

        if (!matched)
            p++;
    }

    return count;
}

/* Trim `a`/`b` to the non-whitespace span they bracket. */
static void glslTrimSpan(const char *text, int *a, int *b)
{
    while (*a < *b && (text[*a] == ' ' || text[*a] == '\t' ||
                       text[*a] == '\n' || text[*a] == '\r')) (*a)++;
    while (*b > *a && (text[*b - 1] == ' ' || text[*b - 1] == '\t' ||
                       text[*b - 1] == '\n' || text[*b - 1] == '\r')) (*b)--;
}

/* Does text[a,b) name something this pass is prepared to call a matrix? */
static BOOL glslSpanIsMatrix(const char *text, int a, int b,
                             const char (*matNames)[GLSL_MAX_NAME_LEN], int matCount)
{
    int len, i;

    glslTrimSpan(text, &a, &b);
    len = b - a;
    if (len <= 0) return FALSE;

    /* A mul() this same rewrite produced on an earlier pass ??? that is how
     * chains like "uProj * uView * aPos" reassociate correctly. */
    if (len > 4 && strncmp(text + a, "mul", 3) == 0 &&
        *glslSkipWhitespace(text + a + 3) == '(')
        return TRUE;

    /* A matrix constructor.  Still in GLSL spelling: this pass deliberately
     * runs ahead of glslApplyTypeReplacements. */
    for (i = 0; kGlslMatrixTypes[i]; i++) {
        int tl = (int)strlen(kGlslMatrixTypes[i]);
        if (len > tl && strncmp(text + a, kGlslMatrixTypes[i], (size_t)tl) == 0 &&
            glslIsWordBoundary(text[a + tl]) &&
            *glslSkipWhitespace(text + a + tl) == '(')
            return TRUE;
    }

    for (i = 0; i < matCount; i++)
        if ((int)strlen(matNames[i]) == len &&
            strncmp(text + a, matNames[i], (size_t)len) == 0)
            return TRUE;

    return FALSE;
}

/* A numeric literal, so that "M * 2.0" is left alone: scaling a matrix by a
 * scalar is component-wise in both languages already. */
static BOOL glslSpanIsNumericLiteral(const char *text, int a, int b)
{
    int i;

    glslTrimSpan(text, &a, &b);
    if (b <= a) return FALSE;
    if (!isdigit((unsigned char)text[a]) && text[a] != '.') return FALSE;

    for (i = a; i < b; i++) {
        char c = text[i];
        if (isxdigit((unsigned char)c) || c == '.' || c == 'x' || c == 'X' ||
            c == 'u' || c == 'U' || c == 'l' || c == 'L' ||
            c == '+' || c == '-')
            continue;
        return FALSE;
    }
    return TRUE;
}

/*
 * Walk backwards from `end` over one postfix expression (identifier, call,
 * subscript, member chain).  Returns its first index, or -1 when there is no
 * operand there ??? an operator, an open bracket, a statement boundary.
 */
static int glslScanOperandBackward(const char *text, int end)
{
    int i = end - 1;
    int start = -1;

    while (i >= 0 && (text[i] == ' ' || text[i] == '\t' ||
                      text[i] == '\n' || text[i] == '\r')) i--;
    if (i < 0) return -1;

    for (;;) {
        if (text[i] == ')' || text[i] == ']') {
            char close = text[i];
            char open  = (close == ')') ? '(' : '[';
            int depth  = 0;
            while (i >= 0) {
                if (text[i] == close) depth++;
                else if (text[i] == open) { depth--; if (depth == 0) break; }
                i--;
            }
            if (i < 0) return -1;
            start = i;
            i--;
            /* the callee / array name in front of the bracket */
            while (i >= 0 && (isalnum((unsigned char)text[i]) || text[i] == '_')) {
                start = i;
                i--;
            }
        } else if (isalnum((unsigned char)text[i]) || text[i] == '_' || text[i] == '.') {
            while (i >= 0 && (isalnum((unsigned char)text[i]) ||
                              text[i] == '_' || text[i] == '.')) {
                start = i;
                i--;
            }
        } else {
            break;
        }

        /* a member access or a further bracket continues the same postfix chain */
        if (i >= 0 && (text[i] == '.' || text[i] == ')' || text[i] == ']'))
            continue;
        break;
    }

    return start;
}

/*
 * Walk forwards from `start` over one postfix expression, or a parenthesised
 * sub-expression.  Returns one past its last index, or -1.  A leading unary
 * sign is deliberately not consumed: leaving such an operand unrecognised keeps
 * today's behaviour rather than guessing at the grouping.
 */
static int glslScanOperandForward(const char *text, int start)
{
    int i = start;
    int first;

    while (text[i] == ' ' || text[i] == '\t' ||
           text[i] == '\n' || text[i] == '\r') i++;
    first = i;
    if (!text[i]) return -1;

    for (;;) {
        if (isalnum((unsigned char)text[i]) || text[i] == '_' || text[i] == '.') {
            while (isalnum((unsigned char)text[i]) || text[i] == '_' || text[i] == '.') i++;
        } else if (text[i] == '(' || text[i] == '[') {
            char open  = text[i];
            char close = (open == '(') ? ')' : ']';
            int depth  = 0;
            while (text[i]) {
                if (text[i] == open) depth++;
                else if (text[i] == close) { depth--; if (depth == 0) { i++; break; } }
                i++;
            }
            if (depth != 0) return -1;
        } else {
            break;
        }

        if (text[i] == '(' || text[i] == '[' || text[i] == '.')
            continue;
        break;
    }

    return (i > first) ? i : -1;
}

/*
 * glslRewriteMatrixProducts ??? turn "A * B" into "mul(A, B)" whenever either
 * operand is a recognised matrix.
 *
 * Bounded multi-pass, one rewrite per pass, in the same spirit as
 * glslRewriteVectorSplats: rewriting the leftmost product and re-scanning is
 * what makes a chain reassociate the way GLSL's left-associative '*' does ???
 * "uProj * uView * aPos" becomes mul(uProj, uView) * aPos and then
 * mul(mul(uProj, uView), aPos).  A pass that runs out of buffer abandons the
 * whole rewrite rather than writing back a partial one.
 */
static void glslRewriteMatrixProducts(char *text, int textSize,
                                      const char (*matNames)[GLSL_MAX_NAME_LEN],
                                      int matCount)
{
    char *buf;
    int bufLen, pass;

    if (!text || !text[0] || textSize < 2)
        return;
    if (!strchr(text, '*'))
        return;

    bufLen = (int)strlen(text) * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT) bufLen = HLSL_MAX_OUTPUT;
    buf = (char *)malloc(bufLen);
    if (!buf) return;

    for (pass = 0; pass < 16; pass++) {
        int i;
        BOOL rewrote = FALSE;

        for (i = 0; text[i]; i++) {
            int ls, rs, re, la, lb, ra, rb, len;
            BOOL leftIsMatrix, rightIsMatrix;

            if (text[i] != '*') continue;
            /* '*=' , '**' and the two comment delimiters are not products */
            if (text[i + 1] == '=' || text[i + 1] == '*' || text[i + 1] == '/') continue;
            if (i > 0 && (text[i - 1] == '*' || text[i - 1] == '/')) continue;

            ls = glslScanOperandBackward(text, i);
            if (ls < 0) continue;
            rs = i + 1;
            re = glslScanOperandForward(text, rs);
            if (re < 0) continue;

            leftIsMatrix  = glslSpanIsMatrix(text, ls, i,  matNames, matCount);
            rightIsMatrix = glslSpanIsMatrix(text, rs, re, matNames, matCount);
            if (!leftIsMatrix && !rightIsMatrix) continue;
            if (!leftIsMatrix  && glslSpanIsNumericLiteral(text, ls, i))  continue;
            if (!rightIsMatrix && glslSpanIsNumericLiteral(text, rs, re)) continue;

            la = ls; lb = i;  glslTrimSpan(text, &la, &lb);
            ra = rs; rb = re; glslTrimSpan(text, &ra, &rb);
            if (lb <= la || rb <= ra) continue;

            len = ls + (int)strlen(text + re) + (lb - la) + (rb - ra) + 8;
            if (len >= bufLen || len >= textSize) break;

            {
                char *dst = buf;
                memcpy(dst, text, (size_t)ls);              dst += ls;
                memcpy(dst, "mul(", 4);                     dst += 4;
                memcpy(dst, text + la, (size_t)(lb - la));  dst += lb - la;
                memcpy(dst, ", ", 2);                       dst += 2;
                memcpy(dst, text + ra, (size_t)(rb - ra));  dst += rb - ra;
                *dst++ = ')';
                strcpy(dst, text + re);
            }

            strcpy(text, buf);
            rewrote = TRUE;
            break;
        }

        if (!rewrote)
            break;
    }

    free(buf);
}

/*
 * glslRewriteMatrixDiagonal ??? matN(x) builds a diagonal matrix in GLSL.
 *
 * This is the case glslRewriteVectorSplats deliberately refuses: an HLSL cast
 * (float4x4)x replicates x into all sixteen components, which compiles and is
 * wrong, so matrix constructors were left to fail as X3014 instead.  Spelling
 * the diagonal out explicitly is the actual lowering.
 *
 * The argument text is duplicated N times, so an argument with a side effect
 * would run N times.  Nothing else in this file duplicates an expression; it is
 * accepted here because GLSL's scalar matrix constructor is, in practice,
 * always given a literal or a plain variable read.
 *
 * A single argument that is itself a matrix is a GLSL matrix-conversion
 * constructor, not a diagonal one, and is left alone.
 */
static void glslRewriteMatrixDiagonal(char *text, int textSize,
                                      const char (*matNames)[GLSL_MAX_NAME_LEN],
                                      int matCount)
{
    static const struct { const char *name; int dim; } kSquare[] = {
        { "mat2x2", 2 }, { "mat3x3", 3 }, { "mat4x4", 4 },
        { "mat2",   2 }, { "mat3",   3 }, { "mat4",   4 },
        { NULL, 0 }
    };
    char *buf;
    int bufLen, pass;

    if (!text || !text[0] || textSize < 2)
        return;
    if (!strstr(text, "mat"))
        return;

    bufLen = (int)strlen(text) * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT) bufLen = HLSL_MAX_OUTPUT;
    buf = (char *)malloc(bufLen);
    if (!buf) return;

    for (pass = 0; pass < 8; pass++) {
        const char *src = text;
        char *dst = buf;
        BOOL changed = FALSE;

        while (*src) {
            const char *name = NULL;
            int nameLen = 0, dim = 0, openIdx = -1, closeIdx = -1;
            int i;

            if (src == text || glslIsWordBoundary(*(src - 1))) {
                for (i = 0; kSquare[i].name; i++) {
                    int len = (int)strlen(kSquare[i].name);
                    if (strncmp(src, kSquare[i].name, (size_t)len) != 0)
                        continue;
                    if (!glslIsWordBoundary(src[len]))
                        break;
                    if (*glslSkipWhitespace(src + len) == '(') {
                        name    = kSquare[i].name;
                        nameLen = len;
                        dim     = kSquare[i].dim;
                        openIdx = (int)(glslSkipWhitespace(src + len) - text);
                    }
                    break;
                }
            }

            if (name)
                closeIdx = glslFindMatchingParen(text, openIdx);

            if (name && closeIdx > openIdx + 1 &&
                glslConstructorArgCount(text + openIdx, closeIdx - openIdx + 1) == 1 &&
                !glslSpanIsMatrix(text, openIdx + 1, closeIdx, matNames, matCount)) {
                int a = openIdx + 1, b = closeIdx;
                int argLen, r, c;

                glslTrimSpan(text, &a, &b);
                argLen = b - a;
                if (argLen <= 0) goto copyChar;
                /* dim*dim slots, each either the argument or "0", plus commas */
                if (dst + nameLen + dim * dim * (argLen + 4) + 8 >= buf + bufLen - 1)
                    break;

                memcpy(dst, name, (size_t)nameLen);
                dst += nameLen;
                *dst++ = '(';
                for (r = 0; r < dim; r++) {
                    for (c = 0; c < dim; c++) {
                        if (r || c) { *dst++ = ','; *dst++ = ' '; }
                        if (r == c) {
                            *dst++ = '(';
                            memcpy(dst, text + a, (size_t)argLen);
                            dst += argLen;
                            *dst++ = ')';
                        } else {
                            *dst++ = '0';
                        }
                    }
                }
                *dst++ = ')';
                src = text + closeIdx + 1;
                changed = TRUE;
                continue;
            }

copyChar:
            if (dst >= buf + bufLen - 1)
                break;
            *dst++ = *src++;
        }
        *dst = '\0';

        if (!changed)
            break;
        /* Same rule as glslRewriteVectorSplats: a scan that stopped short is
         * discarded whole rather than written back truncated. */
        if (*src != '\0' || (int)(dst - buf) >= textSize)
            break;
        strcpy(text, buf);
    }

    free(buf);
}

/*
 * glslApplyTextureLodRewrite ??? rewrite textureLod using the declared sampler
 * dimensionality. SM3 carries explicit LOD in the final coordinate component.
 */
static void glslApplyTextureLodRewrite(char *text,
                                       const glslVarDecl *uniforms,
                                       int uniformCount)
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
                            {
                                const char *samplerType = glslSamplerType(
                                    uniforms, uniformCount, args[0]);
                                if (!samplerType) {
                                    /* Helper parameter: the declared dimension
                                     * sits in the nearest preceding function
                                     * header. */
                                    static const char *const dims[] = {
                                        "sampler1D", "sampler2D",
                                        "sampler3D", "samplerCube", "samplerCUBE" };
                                    const char *start = found;
                                    const char *q;
                                    int di;
                                    while (start > text && start[-1] != '}') start--;
                                    q = found;
                                    while (q > start) {
                                        q--;
                                        for (di = 0; di < 5; ++di) {
                                            size_t len = strlen(dims[di]);
                                            if ((size_t)(found - q) >= len &&
                                                !strncmp(q, dims[di], len) &&
                                                (q == text ||
                                                 !(isalnum((unsigned char)q[-1]) ||
                                                   q[-1] == '_')) &&
                                                !(isalnum((unsigned char)q[len]) ||
                                                  q[len] == '_')) {
                                                samplerType = dims[di];
                                                break;
                                            }
                                        }
                                        if (samplerType) break;
                                    }
                                }
                                const char *intrinsic = glslTextureIntrinsic(
                                    samplerType, TRUE, FALSE);
                                if (samplerType && strstr(samplerType, "1D"))
                                    dst += sprintf(dst,
                                        "%s(%s, float4((%s), 0, 0, (%s)))",
                                        intrinsic, args[0], args[1], args[2]);
                                else if (samplerType &&
                                         (strstr(samplerType, "3D") ||
                                          strstr(samplerType, "Cube") ||
                                          strstr(samplerType, "CUBE")))
                                    dst += sprintf(dst,
                                        "%s(%s, float4((%s), (%s)))",
                                        intrinsic, args[0], args[1], args[2]);
                                else
                                    dst += sprintf(dst,
                                        "%s(%s, float4((%s), 0, (%s)))",
                                        intrinsic, args[0], args[1], args[2]);
                            }
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

/*---------------------- texelFetch / textureSize lowering ----------------------*/

/*
 * SM3 has no integer texel addressing and no runtime size query.  Both are
 * recoverable from one extra constant: (width, height, 1/width, 1/height) for
 * the texture bound to that sampler, pushed per draw by the caller.  With that
 * in hand texelFetch is an ordinary tex2Dlod at an explicit LOD, and
 * textureSize is a swizzle.
 *
 * Only the sampler2D / usampler2D / isampler2D (2D) forms are lowered; the
 * 3D/array/cube overloads are mechanically similar but are not implemented.
 */

static void glslTexDimAdd(glslTexDimSet *set, const char *name)
{
    int i;
    if (!set || !name || !name[0]) return;
    for (i = 0; i < set->count; i++)
        if (strcmp(set->names[i], name) == 0) return;
    if (set->count >= GLSL_MAX_TEXDIM) return;
    strncpy(set->names[set->count], name, GLSL_MAX_NAME_LEN - 1);
    set->names[set->count][GLSL_MAX_NAME_LEN - 1] = '\0';
    set->count++;
}

static BOOL glslTexDimHas(const glslTexDimSet *set, const char *name)
{
    int i;
    if (!set || !name) return FALSE;
    for (i = 0; i < set->count; i++)
        if (strcmp(set->names[i], name) == 0) return TRUE;
    return FALSE;
}

static BOOL glslIsPlainIdentifier(const char *s)
{
    if (!s || !(isalpha((unsigned char)*s) || *s == '_')) return FALSE;
    for (s++; *s; s++)
        if (!(isalnum((unsigned char)*s) || *s == '_')) return FALSE;
    return TRUE;
}

/*
 * Split the argument list of a call whose '(' sits at text[openIdx] and whose
 * matching ')' sits at text[closeIdx].  The arguments are copied into `buf`,
 * NUL-terminated at each depth-0 comma and trimmed.  Returns the argument
 * count, or -1 when the list does not fit, has too many arguments, or contains
 * an empty one.
 */
static int glslSplitCallArgs(const char *text, int openIdx, int closeIdx,
                             char *buf, int bufSize, char **args, int maxArgs)
{
    int argLen = closeIdx - openIdx - 1;
    int depth = 0, count = 0, i;

    if (argLen <= 0 || argLen >= bufSize || maxArgs <= 0)
        return -1;

    memcpy(buf, text + openIdx + 1, (size_t)argLen);
    buf[argLen] = '\0';

    args[count++] = buf;
    for (i = 0; buf[i]; i++) {
        if (buf[i] == '(' || buf[i] == '[') depth++;
        else if (buf[i] == ')' || buf[i] == ']') depth--;
        else if (buf[i] == ',' && depth == 0) {
            if (count >= maxArgs) return -1;
            buf[i] = '\0';
            args[count++] = &buf[i + 1];
        }
    }

    for (i = 0; i < count; i++) {
        char *ap;
        args[i] = glslSkipWhitespace(args[i]);
        ap = args[i] + strlen(args[i]);
        while (ap > args[i] &&
               (ap[-1] == ' ' || ap[-1] == '\t' || ap[-1] == '\n' || ap[-1] == '\r'))
            *--ap = '\0';
        if (!args[i][0]) return -1;
    }
    return count;
}

/* Writes the replacement for one call into `dst`, or returns -1 to leave the
 * call exactly as it was found (which keeps today's compiler error rather than
 * inventing a wrong translation). */
typedef int (*glslCallEmitFn)(char *dst, int dstRoom, char **args, int argCount,
                              glslTexDimSet *texDim,
                              const glslVarDecl *uniforms, int uniformCount,
                              const char *textBase, const char *callSite);

/*
 * GLSL comparison intrinsics ??? lessThan/greaterThan/lessThanEqual/
 * greaterThanEqual/equal/notEqual ??? have no HLSL names, but the relational
 * operators HLSL does have are componentwise on vectors and yield a bool
 * vector, which is exactly the type any()/all() consume.  So lessThan(a, b)
 * becomes (a < b) and its siblings map 1:1.  Two-argument form only, which is
 * the whole GLSL set.
 */
static int glslEmitCompare(char *dst, int dstRoom, char **args, int argCount,
                           const char *op)
{
    int off;
    if (argCount != 2 || dstRoom < 16) return -1;
    off = sprintf(dst, "(%s %s %s)", args[0], op, args[1]);
    return off <= dstRoom ? off : -1;
}

#define GLSL_COMPARE_EMIT(fn, op) \
static int fn(char *dst, int dstRoom, char **args, int argCount, \
              glslTexDimSet *texDim, const glslVarDecl *uniforms, \
              int uniformCount, const char *textBase, const char *callSite) \
{ \
    (void)texDim; (void)uniforms; (void)uniformCount; (void)textBase; (void)callSite; \
    return glslEmitCompare(dst, dstRoom, args, argCount, op); \
}

GLSL_COMPARE_EMIT(glslEmitLessThan,         "<")
GLSL_COMPARE_EMIT(glslEmitGreaterThan,      ">")
GLSL_COMPARE_EMIT(glslEmitLessThanEqual,    "<=")
GLSL_COMPARE_EMIT(glslEmitGreaterThanEqual, ">=")
GLSL_COMPARE_EMIT(glslEmitEqual,            "==")
GLSL_COMPARE_EMIT(glslEmitNotEqual,         "!=")

#undef GLSL_COMPARE_EMIT

/*
 * Rewrite every call to `funcName` in `text` through `emit`.
 *
 * Same shape as glslApplyTextureLodRewrite: scan, split the arguments, emit a
 * replacement, and fall back to copying the call through untouched when the
 * shape is not one the rewrite understands.  A scan that runs out of buffer
 * abandons the whole rewrite and leaves `text` alone rather than writing back
 * a half-rewritten shader.
 */
static void glslRewriteCall(char *text, int textSize, const char *funcName,
                            glslCallEmitFn emit, glslTexDimSet *texDim,
                            const glslVarDecl *uniforms, int uniformCount)
{
    char *buf;
    int bufLen;
    const char *src;
    char *dst;
    int funcLen = (int)strlen(funcName);

    if (!text || !text[0] || !strstr(text, funcName))
        return;

    bufLen = (int)strlen(text) * 2 + 256;
    if (bufLen > HLSL_MAX_OUTPUT) bufLen = HLSL_MAX_OUTPUT;
    buf = (char *)malloc(bufLen);
    if (!buf) return;

    src = text;
    dst = buf;

    while (*src) {
        const char *found = strstr(src, funcName);
        int prefixLen;

        if (!found) {
            int rem = (int)strlen(src);
            if (dst + rem >= buf + bufLen - 1) { free(buf); return; }
            memcpy(dst, src, (size_t)rem);
            dst += rem;
            break;
        }

        if (!((found == text) || glslIsWordBoundary(*(found - 1))) ||
            !glslIsWordBoundary(*(found + funcLen))) {
            prefixLen = (int)(found - src) + 1;
            if (dst + prefixLen >= buf + bufLen - 1) { free(buf); return; }
            memcpy(dst, src, (size_t)prefixLen);
            dst += prefixLen;
            src = found + 1;
            continue;
        }

        prefixLen = (int)(found - src);
        if (dst + prefixLen >= buf + bufLen - 1) { free(buf); return; }
        memcpy(dst, src, (size_t)prefixLen);
        dst += prefixLen;

        {
            const char *parenStart = glslSkipWhitespace(found + funcLen);
            int openIdx  = (int)(parenStart - text);
            int closeIdx = (*parenStart == '(')
                         ? glslFindMatchingParen(text, openIdx) : -1;
            char argBuf[2048];
            char *args[4];
            int argCount = (closeIdx > openIdx)
                         ? glslSplitCallArgs(text, openIdx, closeIdx, argBuf,
                                             (int)sizeof(argBuf), args, 4)
                         : -1;
            int written = -1;

            if (argCount > 0)
                written = emit(dst, (int)(buf + bufLen - 1 - dst), args, argCount,
                               texDim, uniforms, uniformCount, text, found);

            if (written >= 0) {
                dst += written;
                src = text + closeIdx + 1;
                continue;
            }
        }

        if (dst + funcLen >= buf + bufLen - 1) { free(buf); return; }
        memcpy(dst, funcName, (size_t)funcLen);
        dst += funcLen;
        src = found + funcLen;
    }

    *dst = '\0';
    /* A rewrite that outgrew its target is discarded whole rather than
     * copied truncated past the end of `text` ??? the sibling rewrites apply
     * the same rule (glslApplyTextureLodRewrite, glslRewriteMatrixDiagonal).
     * Without it an expanding rewrite against a large shader body would
     * strcpy past the heap block `text` lives in. */
    if ((int)(dst - buf) >= textSize) { free(buf); return; }
    strcpy(text, buf);
    free(buf);
}

/* texelFetch(s, coord[, lod]) -> tex2Dlod at the texel centre.
 *
 * The +0.5 puts the sample at the centre of the addressed texel rather than on
 * its corner, which is where D3D9's half-texel-offset rule would otherwise land
 * it; the multiply by the reciprocal in .zw is the same thing as dividing by
 * the size, done as a multiply because the reciprocal is already sitting in the
 * constant. */
static int glslEmitTexelFetch(char *dst, int dstRoom, char **args, int argCount,
                              glslTexDimSet *texDim,
                              const glslVarDecl *uniforms, int uniformCount,
                              const char *textBase, const char *callSite)
{
    const char *lod = (argCount == 3) ? args[2] : "0";
    int need;

    (void)uniforms;
    (void)uniformCount;
    (void)callSite; (void)textBase;
    if (argCount != 2 && argCount != 3) return -1;
    if (!glslIsPlainIdentifier(args[0]))  return -1;   /* cannot name a uniform after it */

    need = 96 + 2 * (int)strlen(args[0]) + (int)strlen(args[1]) + (int)strlen(lod);
    if (dstRoom < need) return -1;

    glslTexDimAdd(texDim, args[0]);
    return sprintf(dst,
                   "tex2Dlod(%s, float4((((float2)(%s)) + 0.5) * _glsl_texdim_%s.zw, 0, (%s)))",
                   args[0], args[1], args[0], lod);
}

/* textureSize(s[, lod]) -> the dimension uniform's .xy.
 *
 * The lod argument is dropped: SM3 cannot query a mip level's size at runtime,
 * and the constant carries level 0's size.  Correct for lod == 0, which is what
 * essentially every call site asks for; a non-zero lod silently gets level 0's
 * size rather than failing to compile. */
static int glslEmitTextureSize(char *dst, int dstRoom, char **args, int argCount,
                               glslTexDimSet *texDim,
                               const glslVarDecl *uniforms, int uniformCount,
                               const char *textBase, const char *callSite)
{
    (void)uniforms;
    (void)uniformCount;
    (void)callSite; (void)textBase;
    if (argCount != 1 && argCount != 2) return -1;
    if (!glslIsPlainIdentifier(args[0])) return -1;
    if (dstRoom < (int)strlen(args[0]) + 32) return -1;

    glslTexDimAdd(texDim, args[0]);
    return sprintf(dst, "_glsl_texdim_%s.xy", args[0]);
}

static int glslEmitTexture(char *dst, int dstRoom, char **args, int argCount,
                           glslTexDimSet *texDim,
                           const glslVarDecl *uniforms, int uniformCount,
                           const char *textBase, const char *callSite)
{
    const char *samplerType;
    const char *intrinsic;
    int need;

    (void)texDim;
    (void)callSite; (void)textBase;
    if (argCount != 2 && argCount != 3) return -1;
    samplerType = glslSamplerType(uniforms, uniformCount, args[0]);
    if (!samplerType) {
        /* Not a uniform ??? a helper parameter.  The nearest '}' before the
         * call closes the previous function (or the start of the buffer);
         * this helper's header, with its sampler parameter type, lies
         * between that brace and the call site.  Scan that window; the
         * declared dimension is authoritative there, unlike a bare swizzle
         * count, which cannot tell tex3D from texCUBE (both take .xyz). */
        static const char *const dims[] = { "sampler1D", "sampler2D",
                                            "sampler3D", "samplerCube", "samplerCUBE",
                                            "sampler" };
        const char *start = callSite;
        const char *p;
        while (start > textBase && start[-1] != '}') start--;
        p = callSite;
        while (p > start) {
            int i;
            p--;
            for (i = 0; i < (int)(sizeof(dims) / sizeof(dims[0])); ++i) {
                size_t len = strlen(dims[i]);
                if ((size_t)(callSite - p) >= len &&
                    !strncmp(p, dims[i], len) &&
                    (p == textBase ||
                     !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
                    !(isalnum((unsigned char)p[len]) || p[len] == '_')) {
                    samplerType = dims[i];
                    break;
                }
            }
            if (samplerType) break;
        }
        if (!samplerType) {
            /* Not found ??? fall back to the coordinate swizzle's component
             * count: tex2D helpers pass .xy, texCUBE/tex3D helpers .xyz. */
            const char *sw = args[1];
            int comps = 0;
            const char *dot = NULL;
            while (*sw) {
                if (*sw == '.') dot = sw;
                sw++;
            }
            if (dot) {
                const char *q = dot + 1;
                while (*q == 'x' || *q == 'y' || *q == 'z' || *q == 'w') { comps++; q++; }
            }
            if (comps == 1)      samplerType = "sampler1D";
            else if (comps >= 3) samplerType = "samplerCube";
            else                 samplerType = "sampler2D";
        }
    }
    intrinsic = glslTextureIntrinsic(samplerType, FALSE, argCount == 3);
    need = (int)strlen(intrinsic) + (int)strlen(args[0]) +
           (int)strlen(args[1]) + 32;
    if (argCount == 3) need += (int)strlen(args[2]) + 20;
    if (dstRoom < need) return -1;

    if (argCount == 2)
        return sprintf(dst, "%s(%s, (%s))", intrinsic, args[0], args[1]);

    if (samplerType && strstr(samplerType, "1D"))
        return sprintf(dst, "%s(%s, float4((%s), 0, 0, (%s)))",
                       intrinsic, args[0], args[1], args[2]);
    if (samplerType &&
        (strstr(samplerType, "3D") ||
         strstr(samplerType, "Cube") || strstr(samplerType, "CUBE")))
        return sprintf(dst, "%s(%s, float4((%s), (%s)))",
                       intrinsic, args[0], args[1], args[2]);
    return sprintf(dst, "%s(%s, float4((%s), 0, (%s)))",
                   intrinsic, args[0], args[1], args[2]);
}

static void glslApplyTexelFetchRewrite(char *text, int textSize, glslTexDimSet *texDim)
{
    glslRewriteCall(text, textSize, "texelFetch", glslEmitTexelFetch, texDim, NULL, 0);
}

static void glslApplyTextureSizeRewrite(char *text, int textSize, glslTexDimSet *texDim)
{
    glslRewriteCall(text, textSize, "textureSize", glslEmitTextureSize, texDim, NULL, 0);
}

static void glslApplyTextureRewrite(char *text, int textSize, glslTexDimSet *texDim,
                                    const glslVarDecl *uniforms, int uniformCount)
{
    glslRewriteCall(text, textSize, "texture", glslEmitTexture, texDim,
                    uniforms, uniformCount);
}

static int glslEmitTextureProj(char *dst, int dstRoom, char **args, int argCount,
                               glslTexDimSet *texDim,
                               const glslVarDecl *uniforms, int uniformCount,
                               const char *textBase, const char *callSite)
{
    const char *samplerType;
    const char *intrinsic;
    int need;

    (void)texDim;
    if (argCount != 2) return -1;
    /* GLSL textureProj dispatch: sampler1D takes vec2, sampler2D vec3,
     * sampler3D/samplerCube vec4.  The sampler argument is usually a helper
     * parameter (all proj helpers in the corpus are dead code: never called),
     * so look the dimension up in the helper's own header, which is the
     * nearest preceding "sampler#D" token before the call site in the text
     * buffer.  When the sampler is a declared uniform, its type is
     * authoritative. */
    samplerType = glslSamplerType(uniforms, uniformCount, args[0]);
    if (!samplerType) {
        static const char *const dims[] = { "sampler1D", "sampler2D",
                                            "sampler3D", "samplerCube", "samplerCUBE" };
        /* The nearest '}' before the call closes the previous function (or
         * the start of the buffer); this helper's header, with its sampler
         * parameter type, lies between that brace and the call site.  Scan
         * that window only, so an unrelated sampler in an earlier helper
         * cannot be mistaken for this one's. */
        const char *start = callSite;
        const char *p;
        while (start > textBase && start[-1] != '}') start--;
        p = callSite;
        while (p > start) {
            int i;
            p--;
            for (i = 0; i < 5; ++i) {
                size_t len = strlen(dims[i]);
                if ((size_t)(callSite - p) >= len &&
                    !strncmp(p, dims[i], len) &&
                    (p == textBase ||
                     !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
                    !(isalnum((unsigned char)p[len]) || p[len] == '_')) {
                    samplerType = dims[i];
                    break;
                }
            }
            if (samplerType) break;
        }
        if (!samplerType) samplerType = "sampler2D";
        fprintf(stderr, "DEBUG proj: arg=%s -> %s\n", args[0], samplerType);
    }
    if (strstr(samplerType, "1D"))         intrinsic = "tex1Dproj";
    else if (strstr(samplerType, "3D"))    intrinsic = "tex3Dproj";
    else if (strstr(samplerType, "Cube") ||
             strstr(samplerType, "CUBE"))  intrinsic = "texCUBEproj";
    else                                   intrinsic = "tex2Dproj";

    if (!strcmp(intrinsic, "tex2Dproj")) {
        /* HLSL tex2Dproj takes float4 and divides by w; GLSL's float3 form
         * divides by z, hence float4(xy, 1, z).  When the coordinate is
         * already 4 components (e.g. a samplerCubeShadow helper), leave it. */
        need = (int)strlen(intrinsic) + (int)strlen(args[0]) +
               2 * (int)strlen(args[1]) + 40;
        if (dstRoom < need) return -1;
        return sprintf(dst, "tex2Dproj(%s, float4(((%s).xy), 1, ((%s).z)))",
                       args[0], args[1], args[1]);
    }
    if (!strcmp(intrinsic, "tex1Dproj")) {
        /* HLSL tex1Dproj takes float4 and divides by w; GLSL's vec2 form
         * divides by y, hence float4(x, 0, 0, y). */
        need = (int)strlen(intrinsic) + (int)strlen(args[0]) +
               2 * (int)strlen(args[1]) + 40;
        if (dstRoom < need) return -1;
        return sprintf(dst, "tex1Dproj(%s, float4(((%s).x), 0, 0, ((%s).y)))",
                       args[0], args[1], args[1]);
    }
    need = (int)strlen(intrinsic) + (int)strlen(args[0]) +
           (int)strlen(args[1]) + 16;
    if (dstRoom < need) return -1;
    return sprintf(dst, "%s(%s, %s)", intrinsic, args[0], args[1]);
}

/*
 * Rewrite user-defined clip() helpers to call the HLSL clip() intrinsic.
 *
 * GLSL has no standard clip(); engines ship their own:
 *
 *     void clip( float v ) { if ( v < 0.0 ) { discard; } }
 *     void clip( vec4 v )  { if ( any( v < vec4( 0.0 ) ) ) { discard; } }
 *
 * which is exactly HLSL's clip(x): discard the fragment when any component of
 * x is negative.  And vs_3_0 has no discard instruction at all, so the helper
 * body must become the intrinsic for vertex shaders to compile.  The match
 * targets the intrinsic's own body shape after type replacement, i.e.
 *
 *     void _glsl_userfn_clip( float4 v ) {
 *         if ( any( (v < ((float4)( 0.0 )) ) ) ) { discard; } }
 *
 * and rewrites the statement to `clip( v );`.  Only the helper-function
 * spelling (_glsl_userfn_clip) is touched â€” a hand-written discard elsewhere
 * in a fragment shader keeps its meaning.
 */
static void glslRewriteClipHelpers(char *text)
{
    char *hit = text;
    if (!text) return;
    while ((hit = strstr(hit, "_glsl_userfn_clip(")) != NULL) {
        char *body = strchr(hit, '{');
        char *p1, *end;
        char param[GLSL_MAX_NAME_LEN];
        int len;
        if (!body) break;
        p1 = hit + strlen("_glsl_userfn_clip");
        p1 = strchr(p1, '(');
        if (!p1) break;
        p1++;
        while (*p1 == ' ') p1++;
        /* Skip the parameter type to reach the name: "float4 v". */
        end = p1;
        while (*end && *end != ',' && *end != ')' && *end != ' ') end++;
        p1 = end;
        while (*p1 == ' ') p1++;
        end = p1;
        while (*end && *end != ',' && *end != ')' && *end != ' ') end++;
        len = (int)(end - p1);
        if (len <= 0 || len >= GLSL_MAX_NAME_LEN) break;
        memcpy(param, p1, (size_t)len);
        param[len] = '\0';
        /* Replace every "discard;" inside this helper with "clip( param );".
         * The wrapping "if ( v < 0.0 )" becomes redundant â€” clip() tests the
         * same condition â€” but it is harmless, so leave it. */
        while ((p1 = strstr(body, "discard;")) != NULL) {
            char replacement[GLSL_MAX_NAME_LEN + 16];
            int repLen = sprintf(replacement, "clip( %s );", param);
            int tailLen = (int)strlen(p1 + 8);
            memmove(p1 + repLen, p1 + 8, (size_t)tailLen + 1);
            memcpy(p1, replacement, (size_t)repLen);
        }
        hit = body;
    }
}

static void glslApplyFunctionReplacements(char *text, int textSize, glslTexDimSet *texDim,
                                           const glslVarDecl *uniforms,
                                           int uniformCount)
{
    /* Simple word-for-word renames */
    glslReplaceWord(text, "mix",            "lerp");
    glslReplaceWord(text, "fract",          "frac");
    glslReplaceWord(text, "mod",            "fmod");
    glslReplaceWord(text, "inversesqrt",    "rsqrt");
    glslReplaceWord(text, "dFdx",           "ddx");
    glslReplaceWord(text, "dFdy",           "ddy");

    /* A user-defined clip() â€” the GLSL ARB_clip_volume-style helper some
     * engines ship as "void clip( vecN v ) { if ( v < 0 ) { discard; } }" â€”
     * is semantically identical to HLSL's clip() intrinsic, which is also
     * the only form vs_3_0 accepts (discard itself has no vertex-shader
     * instruction).  Rewrite the helper body to call the intrinsic so both
     * profiles compile. */
    glslRewriteClipHelpers(text);

    /* Texture sampling ??? order matters: specific names before generic */
    glslReplaceWord(text, "texture2D",      "tex2D");
    glslReplaceWord(text, "textureCube",    "texCUBE");
    glslReplaceWord(text, "texture3D",      "tex3D");
    glslReplaceWord(text, "textureGrad",    "tex2Dgrad");

    /* textureLod needs argument rewriting ??? do it before generic "texture" */
    glslApplyTextureLodRewrite(text, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "textureProj", glslEmitTextureProj,
                    texDim, uniforms, uniformCount);

    /* texelFetch/textureSize likewise, and they also register the samplers
     * that need a synthesized dimension uniform. */
    glslApplyTexelFetchRewrite(text, textSize, texDim);
    glslApplyTextureSizeRewrite(text, textSize, texDim);

    /* Generic texture() dispatches by the declared sampler dimensionality. */
    glslApplyTextureRewrite(text, textSize, texDim, uniforms, uniformCount);

    /* atan(y, x) -> atan2(y, x) */
    glslReplaceWord(text, "atan",           "atan2");

    /* Comparison intrinsics: GLSL names HLSL does not have.  Relational
     * operators are componentwise on vectors in HLSL and the result feeds
     * any()/all() as GLSL intends.  After the word-for-word renames above,
     * because these names cannot survive verbatim either. */
    glslRewriteCall(text, textSize, "lessThan",         glslEmitLessThan, texDim, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "greaterThan",      glslEmitGreaterThan, texDim, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "lessThanEqual",    glslEmitLessThanEqual, texDim, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "greaterThanEqual", glslEmitGreaterThanEqual, texDim, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "equal",            glslEmitEqual, texDim, uniforms, uniformCount);
    glslRewriteCall(text, textSize, "notEqual",         glslEmitNotEqual, texDim, uniforms, uniformCount);
}

/*
 * glslReplaceBuiltinVars ??? replace GLSL built-in variables with
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

        /* GLSL 1.x compatibility-profile vertex inputs.  The Draw* paths
         * already submit an FVF containing POSITION, NORMAL, COLOR0/1 and
         * TEXCOORD0/1, so these aliases expose that same stream to translated
         * shaders instead of leaving the built-in names for D3DCompile to
         * reject.  D3D expands XYZ/FLOAT2 declarations to float4 inputs using
         * the conventional w=1 defaults. */
        glslReplaceWord(text, "gl_Vertex",              "_glsl_in.glVertex");
        glslReplaceWord(text, "gl_Normal",              "_glsl_in.glNormal");
        glslReplaceWord(text, "gl_Color",               "_glsl_in.glColor");
        glslReplaceWord(text, "gl_SecondaryColor",      "_glsl_in.glSecondaryColor");
        glslReplaceWord(text, "gl_MultiTexCoord0",      "_glsl_in.glMultiTexCoord0");
        glslReplaceWord(text, "gl_MultiTexCoord1",      "_glsl_in.glMultiTexCoord1");
        glslReplaceWord(text, "gl_ModelViewProjectionMatrix", "_glsl_builtinMVP");
        glslReplaceWord(text, "gl_ModelViewMatrix",     "_glsl_builtinModelView");
        glslReplaceWord(text, "gl_ProjectionMatrix",    "_glsl_builtinProjection");
    } else {
        glslReplaceWord(text, "gl_FragColor",   "_glsl_fragColor");
        glslReplaceWord(text, "gl_FrontFacing", "_glsl_in.frontFacing");

        /* In ps_3_0, VPOS (vPos) only exposes .xy — the z and w components
         * are not available (error X5631).  Rewrite any .z or .w swizzle on
         * gl_FragCoord to safe constants before the general word replacement
         * maps the whole identifier onto the struct member.
         *
         *   gl_FragCoord.z  → 0.5  (mid-range depth approximation)
         *   gl_FragCoord.w  → 1.0  (reciprocal clip-w for non-persp is 1)
         *
         * The .xy swizzle and bare uses are left for the general replacement
         * that follows.  The order matters: compound .zw first, then single
         * .z/.w, then the general word rename. */
        glslReplaceAll(text, "gl_FragCoord.zw", "float2(0.5, 1.0)");
        glslReplaceAll(text, "gl_FragCoord.z",  "0.5");
        glslReplaceAll(text, "gl_FragCoord.w",  "1.0");
        /* VPOS itself is only float2 in ps_3_0.  Bare vec4 uses read the
         * synthesized local emitted by glslBuildPixelShader instead. */
        glslReplaceWord(text, "gl_FragCoord",   "_glsl_fragCoord");
    }

    /* gl_FragData[N] -> _glsl_fragData[N] (handled in pixel shader wrapper) */
    glslReplaceAll(text, "gl_FragData", "_glsl_fragData");
}


/**********************************************************************/
/*****            Declaration Parsing                             *****/
/**********************************************************************/

/*
 * glslTryParseLocation ??? attempt to parse layout(location=N) from a line.
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
 * glslParseUIntToken ??? read a whole token as an unsigned decimal or 0x-hex
 * integer.  Succeeds only when the token is *entirely* consumed, so "4" and
 * "0x10u" resolve and "4 + N", "N" and "" do not.
 *
 * Deliberately stricter than atoi, which is what this replaces: atoi("N")
 * answers 0, and 0 is the same value that means "not an array" ??? that is the
 * whole shape of the silent miscompile being closed here.
 */
static BOOL glslParseUIntToken(const char *tok, int *pValue)
{
    const char *p = tok;
    long v = 0;
    int digits = 0;

    if (!tok || !pValue) return FALSE;

    while (*p == ' ' || *p == '\t') p++;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        while (isxdigit((unsigned char)*p)) {
            int d = isdigit((unsigned char)*p)
                  ? (*p - '0')
                  : (tolower((unsigned char)*p) - 'a' + 10);
            if (v > (0x7FFFFFF0L - d) / 16) return FALSE;   /* would overflow */
            v = v * 16 + d;
            digits++;
            p++;
        }
    } else {
        while (isdigit((unsigned char)*p)) {
            int d = *p - '0';
            if (v > (0x7FFFFFF0L - d) / 10) return FALSE;   /* would overflow */
            v = v * 10 + d;
            digits++;
            p++;
        }
    }

    if (digits == 0) return FALSE;

    while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') p++;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (*p != '\0') return FALSE;

    *pValue = (int)v;
    return TRUE;
}

/*
 * glslCollectDefines ??? gather object-like "#define NAME <integer>" lines.
 *
 * Scoped to what an array uniform's extent can be spelled as, and no further:
 * function-like macros are skipped, and so is any value that is not wholly an
 * integer literal.  Resolving defines anywhere else in a shader is a separate
 * and much wider gap ??? #define lines do not survive into the emitted HLSL at
 * all ??? and widening this table would not close it.
 */
static int glslCollectDefines(const char *src, glslDefine *out, int maxOut)
{
    int count = 0;
    const char *lineStart;

    if (!src || !out || maxOut <= 0) return 0;

    lineStart = src;
    while (lineStart && *lineStart) {
        const char *lineEnd = strchr(lineStart, '\n');
        char lineBuf[1024];
        char nameBuf[GLSL_MAX_NAME_LEN];
        int lineLen, nameLen = 0, value = 0, i;
        char *p;
        BOOL dup = FALSE;

        if (lineEnd)
            lineLen = (int)(lineEnd - lineStart);
        else
            lineLen = (int)strlen(lineStart);

        if (lineLen >= (int)sizeof(lineBuf))
            lineLen = (int)sizeof(lineBuf) - 1;

        memcpy(lineBuf, lineStart, lineLen);
        lineBuf[lineLen] = '\0';
        lineStart = lineEnd ? lineEnd + 1 : NULL;

        /* "  #  define NAME 4" is as legal as "#define NAME 4". */
        p = glslSkipWhitespace(lineBuf);
        if (*p != '#') continue;
        p = glslSkipWhitespace(p + 1);
        if (strncmp(p, "define", 6) != 0 || !glslIsWordBoundary(p[6])) continue;
        p = glslSkipWhitespace(p + 6);

        if (!isalpha((unsigned char)*p) && *p != '_') continue;
        while ((isalnum((unsigned char)*p) || *p == '_') &&
               nameLen < GLSL_MAX_NAME_LEN - 1)
            nameBuf[nameLen++] = *p++;
        nameBuf[nameLen] = '\0';

        /* A '(' touching the name makes it function-like: never an extent. */
        if (*p == '(') continue;

        p = glslSkipWhitespace(p);
        if (!glslParseUIntToken(p, &value)) continue;

        for (i = 0; i < count; i++) {
            if (strcmp(out[i].name, nameBuf) == 0) {
                out[i].value = value;       /* a later #define wins */
                dup = TRUE;
                break;
            }
        }
        if (!dup && count < maxOut) {
            strcpy(out[count].name, nameBuf);
            out[count].value = value;
            count++;
        }
    }

    return count;
}

/* Collect only a single numeric token.  This closes the shipped-shader case
 * without pretending to be a complete C preprocessor: a value containing an
 * operator, identifier or function call is intentionally rejected. */
static int glslCollectObjectDefines(const char *src, glslObjectDefine *out,
                                    int maxOut)
{
    int count = 0;
    const char *lineStart = src;

    if (!src || !out || maxOut <= 0) return 0;

    while (lineStart && *lineStart) {
        const char *lineEnd = strchr(lineStart, '\n');
        char lineBuf[1024], nameBuf[GLSL_MAX_NAME_LEN], valueBuf[64];
        int lineLen = lineEnd ? (int)(lineEnd - lineStart)
                              : (int)strlen(lineStart);
        int nameLen = 0, valueLen = 0, i;
        char *p, *end;
        BOOL duplicate = FALSE;

        if (lineLen >= (int)sizeof(lineBuf)) lineLen = (int)sizeof(lineBuf) - 1;
        memcpy(lineBuf, lineStart, (size_t)lineLen);
        lineBuf[lineLen] = '\0';
        lineStart = lineEnd ? lineEnd + 1 : NULL;

        p = glslSkipWhitespace(lineBuf);
        if (*p != '#') continue;
        p = glslSkipWhitespace(p + 1);
        if (strncmp(p, "define", 6) != 0 || !glslIsWordBoundary(p[6])) continue;
        p = glslSkipWhitespace(p + 6);
        if (!isalpha((unsigned char)*p) && *p != '_') continue;
        while ((isalnum((unsigned char)*p) || *p == '_') &&
               nameLen < GLSL_MAX_NAME_LEN - 1)
            nameBuf[nameLen++] = *p++;
        nameBuf[nameLen] = '\0';
        if (*p == '(') continue;                 /* function-like macro */

        p = glslSkipWhitespace(p);
        if (!*p) continue;
        (void)strtod(p, &end);
        if (end == p) continue;
        while (*end == 'f' || *end == 'F' || *end == 'u' || *end == 'U' ||
               *end == 'l' || *end == 'L')
            end++;
        while (*end == ' ' || *end == '\t' || *end == '\r') end++;
        if (*end && strncmp(end, "//", 2) != 0 && strncmp(end, "/*", 2) != 0)
            continue;

        valueLen = (int)(end - p);
        while (valueLen > 0 && (p[valueLen - 1] == ' ' ||
                                p[valueLen - 1] == '\t' ||
                                p[valueLen - 1] == '\r'))
            valueLen--;
        if (valueLen <= 0 || valueLen >= (int)sizeof(valueBuf)) continue;
        memcpy(valueBuf, p, (size_t)valueLen);
        valueBuf[valueLen] = '\0';

        for (i = 0; i < count; ++i) {
            if (!strcmp(out[i].name, nameBuf)) {
                strcpy(out[i].replacement, valueBuf);
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate && count < maxOut) {
            strcpy(out[count].name, nameBuf);
            strcpy(out[count].replacement, valueBuf);
            count++;
        }
    }
    return count;
}

static void glslApplyObjectDefines(char *text,
                                   const glslObjectDefine *defines, int count)
{
    int i;
    if (!text || !defines) return;
    for (i = 0; i < count; ++i)
        glslReplaceWord(text, defines[i].name, defines[i].replacement);
}

/*
 * glslParseDeclarations ??? scan GLSL source for attribute, varying, uniform,
 * in, and out declarations. Populates the provided arrays.
 *
 * `defines` (nullable) resolves an array extent written as a macro name.
 * `pUnresolvedArray` (nullable) is set TRUE when a declaration is an array
 * whose extent could be worked out from neither a literal nor `defines`; the
 * caller is expected to stop rather than emit HLSL from a declaration set it
 * knows to be wrong.  Both follow glslStripLayoutQualifiers' convention of
 * being safe to pass as NULL.
 *
 * Returns the total number of declarations found.
 */
/*
 * glslFlattenUniformBlocks ??? rewrite uniform block declarations
 *
 *   uniform matrices_ubo { vec4 matrices[768]; };
 *   uniform name { vec4 a; mat3 b[2]; } instance;
 *
 * into plain uniform declarations of the block's members.  D3D9 has no uniform
 * buffers, so the block wrapper is dropped and every member is promoted to a
 * top-level uniform under its own name.  The block name and any instance name
 * are discarded ??? the application cannot query a member through an instance
 * here anyway, and the wrapper's GL entry points answer UBO queries with the
 * software path the block binding is tracked in.
 *
 * The input is parsed once, depth-counted, so "type member[N];" lists with
 * commas, multiple members and stray whitespace all come out right.
 */
static void glslFlattenUniformBlocks(char *dst, int dstSize, const char *src)
{
    const char *p = src;
    int off = 0;

    dst[0] = '\0';
    if (!src) return;

    while (*p) {
        const char *uni = strstr(p, "uniform");
        const char *after;
        const char *idEnd;
        const char *scan;
        int depth;

        if (!uni) {
            int rem = (int)strlen(p);
            if (off + rem + 1 >= dstSize) break;
            memcpy(dst + off, p, (size_t)rem);
            off += rem;
            break;
        }
        if (uni != p && !glslIsWordBoundary(uni[-1])) {
            int step = (int)(uni - p) + 1;
            if (off + step >= dstSize) break;
            memcpy(dst + off, p, (size_t)step);
            off += step;
            p = uni + 1;
            continue;
        }

        after = glslSkipWhitespace(uni + 7);
        idEnd = after;
        while (*idEnd == '_' || isalnum((unsigned char)*idEnd)) idEnd++;
        if (idEnd == after || !glslIsWordBoundary(*idEnd)) {
            int step = (int)(uni - p) + 1;
            if (off + step >= dstSize) break;
            memcpy(dst + off, p, (size_t)step);
            off += step;
            p = uni + 1;
            continue;
        }
        after = glslSkipWhitespace(idEnd);
        if (*after != '{') {
            /* Plain uniform; copy the whole statement through its terminating
             * ';' (parens/brackets depth-counted, so "uniform vec4 m[3];" and
             * "uniform vec4 v = vec4( 1.0 );" come out intact) and move on. */
            const char *semi = after;
            int d = 0;
            for (; *semi; semi++) {
                if (*semi == '(' || *semi == '[') d++;
                else if (*semi == ')' || *semi == ']') { if (d > 0) d--; }
                else if (*semi == ';' && d == 0) break;
            }
            if (*semi == ';') {
                int step = (int)(semi - p) + 1;
                if (off + step >= dstSize) break;
                memcpy(dst + off, p, (size_t)step);
                off += step;
                p = semi + 1;
            } else {
                /* No ';' before EOF; copy the rest through and stop. */
                int rem = (int)strlen(p);
                if (off + rem + 1 >= dstSize) break;
                memcpy(dst + off, p, (size_t)rem);
                off += rem;
                break;
            }
            continue;
        }

        /* It is a block.  Keep the text before the block, find the matching
         * '}' with brace counting, then promote the members. */
        {
            int step = (int)(uni - p);
            if (off + step >= dstSize) break;
            memcpy(dst + off, p, (size_t)step);
            off += step;
        }
        depth = 0;
        scan = after;
        while (*scan) {
            if (*scan == '{') depth++;
            else if (*scan == '}') {
                depth--;
                if (depth == 0) break;
            }
            scan++;
        }
        if (*scan != '}') {
            /* Unterminated block; copy the rest through and stop. */
            int rem = (int)strlen(scan);
            if (off + rem + 1 >= dstSize) break;
            memcpy(dst + off, scan, (size_t)rem);
            off += rem;
            break;
        }

        /* Emit each member as "uniform <member>;".  Members are terminated by
         * ';' at nesting depth 0 inside the braces. */
        {
            const char *mStart = after + 1;
            const char *mCur = mStart;
            while (mCur < scan) {
                const char *semi = mCur;
                int d = 0;
                for (; semi < scan && *semi; semi++) {
                    if (*semi == '(' || *semi == '[' || *semi == '{') d++;
                    else if (*semi == ')' || *semi == ']' || *semi == '}') d--;
                    else if (*semi == ';' && d == 0) break;
                }
                if (semi >= scan) break;

                {
                    const char *ms = glslSkipWhitespace(mCur);
                    const char *me = semi;
                    while (me > ms && (me[-1] == ' ' || me[-1] == '\t' ||
                                       me[-1] == '\n' || me[-1] == '\r'))
                        me--;
                    if (me > ms) {
                        int len = (int)(me - ms);
                        if (off + len + 12 >= dstSize) { mCur = semi + 1; break; }
                        memcpy(dst + off, "uniform ", 8);
                        off += 8;
                        memcpy(dst + off, ms, (size_t)len);
                        off += len;
                        dst[off++] = ';';
                        dst[off++] = '\n';
                    }
                }
                mCur = semi + 1;
            }
        }

        /* Skip the closing brace, any instance name (and array suffix), and
         * the terminating ';'. */
        after = glslSkipWhitespace(scan + 1);
        {
            const char *instEnd = after;
            while (*instEnd == '_' || isalnum((unsigned char)*instEnd)) instEnd++;
            if (instEnd != after) {
                after = glslSkipWhitespace(instEnd);
                if (*after == '[') {
                    int d = 0;
                    while (*after) {
                        if (*after == '[') d++;
                        else if (*after == ']') { d--; if (d == 0) { after++; break; } }
                        after++;
                    }
                    after = glslSkipWhitespace(after);
                }
            }
        }
        if (*after == ';') after++;
        p = after;
    }

    if (off < dstSize) dst[off] = '\0';
}

static int glslParseDeclarations(const char *src, int shaderType,
                                 glslVarDecl *attributes, int *pAttrCount,
                                 glslVarDecl *varyings, int *pVaryCount,
                                 glslVarDecl *uniforms, int *pUnifCount,
                                 const glslDefine *defines, int defineCount,
                                 BOOL *pUnresolvedArray)
{
    char lineBuf[1024];
    char *statementSource;
    const char *lineStart;
    int totalFound = 0;
    int sourceLength, sourceIndex, braceDepth = 0;
    BOOL lineComment = FALSE, blockComment = FALSE;

    *pAttrCount = 0;
    *pVaryCount = 0;
    *pUnifCount = 0;

    if (!src) return 0;
    sourceLength = (int)strlen(src);
    statementSource = (char *)malloc((size_t)sourceLength * 2 + 1);
    if (!statementSource) return 0;

    /* Uniform blocks first: their members must read as plain uniforms below,
     * and the block's own line is otherwise discarded as a function body. */
    glslFlattenUniformBlocks(statementSource, sourceLength * 2 + 1, src);
    sourceLength = (int)strlen(statementSource);

    /* Declarations are statements, not lines. Minified game shaders routinely
     * put several globals before main() on one physical line. Turn top-level
     * semicolons into line boundaries while preserving comments and function
     * bodies, allowing the established parser below to consume every global. */
    for (sourceIndex = 0; sourceIndex < sourceLength; ++sourceIndex) {
        char c = statementSource[sourceIndex];
        char n = sourceIndex + 1 < sourceLength ? statementSource[sourceIndex + 1] : '\0';
        if (lineComment) {
            if (c == '\n') lineComment = FALSE;
            continue;
        }
        if (blockComment) {
            if (c == '*' && n == '/') { blockComment = FALSE; ++sourceIndex; }
            continue;
        }
        if (c == '/' && n == '/') { lineComment = TRUE; ++sourceIndex; continue; }
        if (c == '/' && n == '*') { blockComment = TRUE; ++sourceIndex; continue; }
        if (c == '{') { ++braceDepth; continue; }
        if (c == '}') { if (braceDepth > 0) --braceDepth; continue; }
        if (c == ';' && braceDepth == 0) statementSource[sourceIndex] = '\n';
    }

    lineStart = statementSource;
    while (lineStart && *lineStart) {
        const char *lineEnd = strchr(lineStart, '\n');
        int lineLen;
        char *trimmed;
        char qualifier[32] = {0};
        char typeName[GLSL_MAX_TYPE_LEN] = {0};
        char varName[GLSL_MAX_NAME_LEN] = {0};
        char arrayTok[GLSL_MAX_NAME_LEN] = {0};
        BOOL hasBracket = FALSE;
        int location = -1;
        int arraySize = 0;
        BOOL isFlat = FALSE;
        char *tok;
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

        /* Step past any layout(...) block and interpolation qualifiers so the
         * storage qualifier below is always the first token examined. */
        trimmed = glslSkipDeclQualifiers(lineBuf, &isFlat);

        /* Skip preprocessor, comments, empty lines */
        if (trimmed[0] == '#' || trimmed[0] == '/' || trimmed[0] == '\0')
            continue;
        /* Skip lines inside function bodies (heuristic: contains '{' or starts with common statements) */
        if (strchr(trimmed, '{') || strchr(trimmed, '}'))
            continue;

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

        /* Remove semicolons and array brackets for parsing.  Any layout(...)
         * block is already behind `trimmed` ??? glslSkipDeclQualifiers stepped
         * over it before the storage qualifier was matched. */
        {
            char *semi = strchr(trimmed, ';');
            if (semi) *semi = '\0';
        }

        /* Excise an array suffix like [4] before tokenizing, wherever it sits.
         *
         * The bracket must not be left for strtok to find.  "uniform vec4
         * _va_ [4];" ??? the spelling a real shipped shader used ??? tokenizes as
         * "_va_" and "[4]", and the name branch below stops at the name and
         * never reads the second token, so the extent was silently lost and
         * the uniform emitted as a scalar.  With the bracket gone up front,
         * strtok only ever sees "type name", whatever the whitespace does.
         *
         * An unterminated '[' is treated as an unresolved extent rather than
         * ignored: the declaration is an array of *something*, and guessing is
         * exactly what this is here to stop. */
        {
            char *open = strchr(trimmed, '[');
            if (open) {
                char *close = strchr(open, ']');
                int inner = close ? (int)(close - open) - 1 : (int)strlen(open + 1);
                if (inner < 0) inner = 0;
                if (inner > (int)sizeof(arrayTok) - 1)
                    inner = (int)sizeof(arrayTok) - 1;
                if (inner > 0)
                    memcpy(arrayTok, open + 1, (size_t)inner);
                arrayTok[inner] = '\0';
                hasBracket = TRUE;
                if (close)
                    memmove(open, close + 1, strlen(close + 1) + 1);
                else
                    *open = '\0';
            }
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
                strncpy(varName, tok, GLSL_MAX_NAME_LEN - 1);
                tokenIdx++;
                break;
            }
            tok = strtok(NULL, " \t");
        }

        if (tokenIdx < 2 || typeName[0] == '\0' || varName[0] == '\0')
            continue;

        /* Resolve the extent.  A literal first (the common case), then an
         * object-like #define, and otherwise nothing ??? an array uniform whose
         * extent is unknown cannot be emitted, because emitting it as a scalar
         * compiles (HLSL reads foo[0..3] on a scalar float4 as a swizzle) and
         * silently returns the wrong values for every element. */
        if (hasBracket) {
            char *at = arrayTok;
            char *ae;

            while (*at == ' ' || *at == '\t') at++;
            ae = at + strlen(at);
            while (ae > at && (ae[-1] == ' ' || ae[-1] == '\t' || ae[-1] == '\r'))
                ae--;
            *ae = '\0';

            if (!glslParseUIntToken(at, &arraySize)) {
                int d;
                arraySize = GLSL_ARRAY_UNRESOLVED;
                for (d = 0; d < defineCount && defines; d++) {
                    if (strcmp(defines[d].name, at) == 0) {
                        arraySize = defines[d].value;
                        break;
                    }
                }
            }
            if (arraySize < 0) {
                arraySize = GLSL_ARRAY_UNRESOLVED;
                if (pUnresolvedArray) *pUnresolvedArray = TRUE;
                gldDiagLog("GLSL->HLSL: '%s %s %s[%s]' - the array extent '%s' is "
                           "neither an integer literal nor an object-like "
                           "'#define %s <integer>' in this shader. Emitting the "
                           "uniform without its extent would put every element "
                           "past the first on the wrong constant register and "
                           "would still compile, so the shader is not translated.",
                           qualifier, typeName, varName, at, at, at);
            }
        }

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
                decl->arraySize = arraySize;
                decl->isFlat = isFlat;
                totalFound++;
            }
        }
    }

    free(statementSource);
    return totalFound;
}

/* Collect global fragment outputs statement-by-statement rather than
 * line-by-line.  This accepts both
 *
 *     in vec4 color; layout(location=0) out vec4 result;
 *
 * on one physical line and the more usual one-declaration-per-line spelling.
 * Function parameters using the `out` direction qualifier are rejected by
 * requiring the storage word to appear outside parentheses and braces. */
static int glslCollectFragmentOutputs(const char *src,
                                      glslFragOutput *outputs, int maxOutputs)
{
    const char *statement = src;
    const char *p = src;
    int braceDepth = 0;
    int parenDepth = 0;
    int count = 0;
    BOOL used[GLSL_MAX_FRAGDATA];
    int i;

    memset(used, 0, sizeof(used));
    if (!src || !outputs || maxOutputs <= 0)
        return 0;

    while (*p) {
        if (*p == '{') braceDepth++;
        else if (*p == '}' && braceDepth > 0) braceDepth--;
        else if (*p == '(') parenDepth++;
        else if (*p == ')' && parenDepth > 0) parenDepth--;

        if (*p == ';' && braceDepth == 0) {
            const char *q = statement;
            const char *outWord = NULL;
            int localParen = 0;

            while (q < p) {
                if (*q == '(') localParen++;
                else if (*q == ')' && localParen > 0) localParen--;
                else if (localParen == 0 && q + 3 <= p &&
                         q[0] == 'o' && q[1] == 'u' && q[2] == 't' &&
                         (q == statement || glslIsWordBoundary(q[-1])) &&
                         (q + 3 == p || glslIsWordBoundary(q[3]))) {
                    outWord = q;
                    break;
                }
                q++;
            }

            if (outWord && count < maxOutputs) {
                const char *t = outWord + 3;
                const char *typeStart, *typeEnd, *nameStart, *nameEnd;
                int location = -1;
                const char *loc = statement;

                /* Read an optional explicit location anywhere before `out`. */
                while (loc < outWord) {
                    const char *hit = strstr(loc, "location");
                    if (!hit || hit >= outWord) break;
                    if ((hit == statement || glslIsWordBoundary(hit[-1])) &&
                        glslIsWordBoundary(hit[8])) {
                        const char *eq = hit + 8;
                        while (eq < outWord && *eq != '=') eq++;
                        if (eq < outWord) {
                            char *endNum;
                            long parsed;
                            eq++;
                            while (eq < outWord && isspace((unsigned char)*eq)) eq++;
                            parsed = strtol(eq, &endNum, 10);
                            if (endNum != eq && parsed >= 0 && parsed < 0x7fffffffL)
                                location = (int)parsed;
                        }
                    }
                    loc = hit + 8;
                }

                while (t < p && isspace((unsigned char)*t)) t++;
                /* Precision can legally follow the storage qualifier. */
                for (;;) {
                    const char *wordEnd = t;
                    int wordLen;
                    while (wordEnd < p &&
                           (isalnum((unsigned char)*wordEnd) || *wordEnd == '_'))
                        wordEnd++;
                    wordLen = (int)(wordEnd - t);
                    if ((wordLen == 5 && !strncmp(t, "highp", 5)) ||
                        (wordLen == 7 && !strncmp(t, "mediump", 7)) ||
                        (wordLen == 4 && !strncmp(t, "lowp", 4)) ||
                        (wordLen == 7 && !strncmp(t, "precise", 7))) {
                        t = wordEnd;
                        while (t < p && isspace((unsigned char)*t)) t++;
                        continue;
                    }
                    break;
                }

                typeStart = t;
                while (t < p && (isalnum((unsigned char)*t) || *t == '_')) t++;
                typeEnd = t;
                while (t < p && isspace((unsigned char)*t)) t++;
                nameStart = t;
                while (t < p && (isalnum((unsigned char)*t) || *t == '_')) t++;
                nameEnd = t;

                if (typeEnd > typeStart && nameEnd > nameStart &&
                    (nameEnd - nameStart) < GLSL_MAX_NAME_LEN) {
                    int duplicate = -1;
                    for (i = 0; i < count; ++i) {
                        int len = (int)(nameEnd - nameStart);
                        if ((int)strlen(outputs[i].name) == len &&
                            !strncmp(outputs[i].name, nameStart, (size_t)len)) {
                            duplicate = i;
                            break;
                        }
                    }
                    if (duplicate < 0) {
                        int len = (int)(nameEnd - nameStart);
                        memcpy(outputs[count].name, nameStart, (size_t)len);
                        outputs[count].name[len] = '\0';
                        outputs[count].location = location;
                        if (location >= 0 && location < GLSL_MAX_FRAGDATA)
                            used[location] = TRUE;
                        count++;
                    }
                }
            }

            statement = p + 1;
            parenDepth = 0;
        }
        p++;
    }

    /* GLSL permits the linker to choose locations for outputs without an
     * explicit layout qualifier.  Choose the lowest free slot, matching the
     * declaration-order assignment used by the rest of this translator. */
    for (i = 0; i < count; ++i) {
        if (outputs[i].location < 0) {
            int slot;
            for (slot = 0; slot < GLSL_MAX_FRAGDATA && used[slot]; ++slot) {}
            outputs[i].location = slot;
            if (slot < GLSL_MAX_FRAGDATA) used[slot] = TRUE;
        }
    }
    return count;
}


/**********************************************************************/
/*****            Main Body Extraction                           *****/
/**********************************************************************/

/*
 * glslExtractMainBody ??? extract the body of void main() { ... }
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
 * glslRemoveDeclarationLines ??? blank out attribute/varying/uniform/in/out
 * declaration lines from the source so they don't appear in the main body.
 * Also removes the void main() wrapper.
 */
static void glslRemoveDeclarationLines(char *src)
{
    char *line = src;
    while (line && *line) {
        char *next = strchr(line, '\n');
        /* Same leading layout(...)/interpolation-qualifier skip the declaration
         * parser uses, so exactly the lines it consumed are the lines blanked. */
        char *trimmed = glslSkipDeclQualifiers(line, NULL);

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
 * glslGetSemanticForAttr ??? determine the HLSL semantic for a vertex
 * shader input attribute based on its name and location.
 */
/*
 * glslExplicitSemantic ??? a variable whose name contains "_SEM_<SEMANTIC>"
 * names its HLSL semantic outright.
 *
 * Generated shaders (arb_asm_translator.c) need inputs and interpolators
 * pinned to exact semantics: an ARB program's result.texcoord[3] and the
 * fragment program's fragment.texcoord[3] are separate compilations that must
 * still meet on the same interpolator, which neither the declaration-order nor
 * the name-guessing rule below can guarantee.  Hand-written GLSL never
 * contains this marker, so those rules are untouched for real GLSL.
 */
static const char *glslExplicitSemantic(const char *name, char *semBuf, int bufSize)
{
    const char *marker = strstr(name, "_SEM_");
    if (!marker) return NULL;
    strncpy(semBuf, marker + 5, bufSize - 1);
    semBuf[bufSize - 1] = '\0';
    return semBuf[0] ? semBuf : NULL;
}

static const char *glslGetSemanticForAttr(const glslVarDecl *attr, int index)
{
    static char semBuf[64];

    {
        const char *explicitSem = glslExplicitSemantic(attr->name, semBuf, sizeof(semBuf));
        if (explicitSem) return explicitSem;
    }

    /* Match the conventional GL attribute aliases to the declaration consumed
     * by gl_impl.c.  id Tech binds UV/tangent at 8/9 and colors at 3/4; using
     * declaration-order TEXCOORD indices makes a valid draw read the wrong
     * stream fields. */
    if (attr->location >= 0) {
        switch (attr->location) {
        case 0: return "POSITION";
        case 1: return "TEXCOORD0";
        case 2: return "NORMAL";
        case 3: return "COLOR0";
        case 4: return "COLOR1";
        case 5: return "TEXCOORD2";
        case 8: return "TEXCOORD0";
        case 9: return "TEXCOORD1";
        case 10: return "TEXCOORD3";
        case 6: return "TEXCOORD2";
        case 7: return "TEXCOORD3";
        default:
            sprintf(semBuf, "TEXCOORD%d", attr->location);
            return semBuf;
        }
    }

    /* Heuristic: guess from name */
    if (strstr(attr->name, "osition") || strstr(attr->name, "pos") ||
        strstr(attr->name, "Pos") || strstr(attr->name, "vertex")) {
        return "POSITION";
    }
    if (strstr(attr->name, "ormal") || strstr(attr->name, "norm")) {
        return "NORMAL";
    }
    if (strstr(attr->name, "angent") || strstr(attr->name, "tangent"))
        return "TEXCOORD1";
    if (strstr(attr->name, "exCoord") || strstr(attr->name, "excoord") ||
        strstr(attr->name, "texcoord") || strstr(attr->name, "texCoord"))
        return "TEXCOORD0";
    if (strstr(attr->name, "olor") || strstr(attr->name, "col")) {
        if (strstr(attr->name, "olor2") || strstr(attr->name, "olor1") ||
            strstr(attr->name, "Color2") || strstr(attr->name, "Color1"))
            return "COLOR1";
        return "COLOR0";
    }

    /* Default: TEXCOORD[index] */
    sprintf(semBuf, "TEXCOORD%d", index);
    return semBuf;
}

/*
 * glslGetSemanticForVarying ??? determine the HLSL semantic for a
 * varying (VS output / PS input).
 */
static const char *glslGetSemanticForVarying(const glslVarDecl *vary, int index)
{
    static char semBuf[64];

    {
        const char *explicitSem = glslExplicitSemantic(vary->name, semBuf, sizeof(semBuf));
        if (explicitSem) return explicitSem;
    }

    if (vary->location >= 0) {
        sprintf(semBuf, "TEXCOORD%d", vary->location);
        return semBuf;
    }

    sprintf(semBuf, "TEXCOORD%d", index);
    return semBuf;
}

/*
 * glslBuildVertexShader ??? construct a complete HLSL vertex shader from
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
                                  const char *mainBody, char *hlslOut, int hlslBufSize,
                                  const glslTexDimSet *texDim)
{
    int off = 0;
    int i;
    BOOL usesPointSize = (strstr(mainBody, "_glsl_out.psize") != NULL);
    BOOL pretransformed = (strstr(mainBody, "gldPos_SEM_POSITION") != NULL);
    BOOL usesBuiltinMVP = (strstr(mainBody, "_glsl_builtinMVP") != NULL);
    BOOL usesBuiltinModelView = (strstr(mainBody, "_glsl_builtinModelView") != NULL);
    BOOL usesBuiltinProjection = (strstr(mainBody, "_glsl_builtinProjection") != NULL);

    hlslOut[0] = '\0';

    /* D3D9 cannot represent an OpenGL viewport that extends beyond the render
     * target. The draw path clips the physical D3D viewport and supplies the
     * affine clip-space correction which preserves GL's original mapping. */
    if (!pretransformed)
        off += sprintf(hlslOut + off, "float4 _glsl_viewportAdjust;\n");
    if (usesBuiltinMVP)
        off += sprintf(hlslOut + off, "float4x4 _glsl_builtinMVP;\n");
    if (usesBuiltinModelView)
        off += sprintf(hlslOut + off, "float4x4 _glsl_builtinModelView;\n");
    if (usesBuiltinProjection)
        off += sprintf(hlslOut + off, "float4x4 _glsl_builtinProjection;\n");

    /* Emit uniforms as globals */
    for (i = 0; i < unifCount; i++) {
        if (glslIsSamplerType(uniforms[i].type))
            off += sprintf(hlslOut + off, "sampler %s;\n", uniforms[i].name);
        else if (uniforms[i].arraySize > 0)
            off += sprintf(hlslOut + off, "%s %s[%d];\n",
                           glslConvertType(uniforms[i].type), uniforms[i].name,
                           uniforms[i].arraySize);
        else
            off += sprintf(hlslOut + off, "%s %s;\n",
                           glslConvertType(uniforms[i].type), uniforms[i].name);

        /* Vertex texture fetch is rare but legal, so the dimension uniform a
         * lowered texelFetch/textureSize needs is emitted here too. */
        if (glslTexDimHas(texDim, uniforms[i].name))
            off += sprintf(hlslOut + off, "float4 _glsl_texdim_%s;\n", uniforms[i].name);
    }
    /* A texture helper may query a sampler parameter (commonly named
     * "image") rather than a global uniform.  It still needs a declaration;
     * link-time reflection pairs this otherwise-unmatched helper constant to
     * the program's sampler fallback. */
    if (texDim) {
        for (i = 0; i < texDim->count; ++i) {
            int j, matched = 0;
            for (j = 0; j < unifCount; ++j)
                if (strcmp(texDim->names[i], uniforms[j].name) == 0) {
                    matched = 1;
                    break;
                }
            if (!matched)
                off += sprintf(hlslOut + off, "float4 _glsl_texdim_%s;\n",
                               texDim->names[i]);
        }
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
    if (strstr(mainBody, "_glsl_in.glVertex"))
        off += sprintf(hlslOut + off, "    float4 glVertex : POSITION;\n");
    if (strstr(mainBody, "_glsl_in.glNormal"))
        off += sprintf(hlslOut + off, "    float3 glNormal : NORMAL;\n");
    if (strstr(mainBody, "_glsl_in.glColor"))
        off += sprintf(hlslOut + off, "    float4 glColor : COLOR0;\n");
    if (strstr(mainBody, "_glsl_in.glSecondaryColor"))
        off += sprintf(hlslOut + off, "    float4 glSecondaryColor : COLOR1;\n");
    if (strstr(mainBody, "_glsl_in.glMultiTexCoord0"))
        off += sprintf(hlslOut + off, "    float4 glMultiTexCoord0 : TEXCOORD0;\n");
    if (strstr(mainBody, "_glsl_in.glMultiTexCoord1"))
        off += sprintf(hlslOut + off, "    float4 glMultiTexCoord1 : TEXCOORD1;\n");
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

    /* Declare local aliases for varyings (will be written to output).
     *
     * Zero-initialized, the same way the temp registers and the pixel shader's
     * _glsl_fragColor/_glsl_fragData locals are.  A GLSL (or lowered ARB)
     * program is free to write only part of a varying ??? result.texcoord[0].xy,
     * say ??? and the copy-out at the end of this function then reads the whole
     * vector back, which HLSL rejects as "error X4000: used without having been
     * completely initialized".  Zero is also the right value: GL says the
     * components the program never wrote are undefined, and reading them as
     * zero is a legal choice, so this is the GL semantic rather than a
     * workaround for the compiler.
     *
     * The (TYPE)0 cast form is used instead of a spelled-out float4(0,0,0,0)
     * because glslConvertType can return any of float2/float3/float4/int4/...
     * here; it mirrors the (VS_OUTPUT)0 initializer a few lines above. */
    for (i = 0; i < varyCount; i++) {
        const char *vType = glslConvertType(varyings[i].type);
        off += sprintf(hlslOut + off, "    %s %s = (%s)0;\n",
                       vType, varyings[i].name, vType);
    }

    off += sprintf(hlslOut + off, "\n");

    /* Insert the transpiled main body */
    off += sprintf(hlslOut + off, "%s\n", mainBody);

    /* Copy varying locals to output struct */
    for (i = 0; i < varyCount; i++) {
        off += sprintf(hlslOut + off, "    _glsl_out.%s = %s;\n",
                       varyings[i].name, varyings[i].name);
    }

    if (!pretransformed)
        off += sprintf(hlslOut + off,
                       "    float4 _gld_clip = _glsl_out.pos;\n"
                       "    _glsl_out.pos.x = _gld_clip.x * _glsl_viewportAdjust.x + _gld_clip.w * _glsl_viewportAdjust.z;\n"
                       "    _glsl_out.pos.y = _gld_clip.y * _glsl_viewportAdjust.y + _gld_clip.w * _glsl_viewportAdjust.w;\n"
                       "    _glsl_out.pos.z = 0.5 * (_gld_clip.z + _gld_clip.w);\n");

    off += sprintf(hlslOut + off, "    return _glsl_out;\n");
    off += sprintf(hlslOut + off, "}\n");
}

/*
 * glslBuildPixelShader ??? construct a complete HLSL pixel shader from
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
                                 BOOL usesFragCoord, BOOL usesFrontFacing,
                                 const glslTexDimSet *texDim)
{
    int off = 0;
    int i;

    hlslOut[0] = '\0';

    /* Emit uniforms as globals (separate samplers from regular uniforms) */
    for (i = 0; i < unifCount; i++) {
        if (glslIsSamplerType(uniforms[i].type)) {
            /* SM3 sampler registers are untyped; dimensionality is carried by
             * the tex1D/tex2D/tex3D/texCUBE intrinsic selected at each call. */
            off += sprintf(hlslOut + off, "sampler %s;\n", uniforms[i].name);
            /* (width, height, 1/width, 1/height) for the texture bound to this
             * sampler, pushed once per draw.  An ordinary named uniform, so it
             * reaches the driver through the existing CTAB reflection with no
             * new reflection code. */
            if (glslTexDimHas(texDim, uniforms[i].name))
                off += sprintf(hlslOut + off, "float4 _glsl_texdim_%s;\n", uniforms[i].name);
        } else if (uniforms[i].arraySize > 0) {
            off += sprintf(hlslOut + off, "%s %s[%d];\n",
                           glslConvertType(uniforms[i].type), uniforms[i].name,
                           uniforms[i].arraySize);
        } else {
            off += sprintf(hlslOut + off, "%s %s;\n",
                           glslConvertType(uniforms[i].type), uniforms[i].name);
        }
    }
    if (texDim) {
        for (i = 0; i < texDim->count; ++i) {
            int j, matched = 0;
            for (j = 0; j < unifCount; ++j)
                if (strcmp(texDim->names[i], uniforms[j].name) == 0) {
                    matched = 1;
                    break;
                }
            if (!matched)
                off += sprintf(hlslOut + off, "float4 _glsl_texdim_%s;\n",
                               texDim->names[i]);
        }
    }
    off += sprintf(hlslOut + off, "\n");

    /* Emit PS_INPUT struct */
    off += sprintf(hlslOut + off, "struct PS_INPUT {\n");
    for (i = 0; i < varyCount; i++) {
        /* Skip fragment outputs ??? they go in PS_OUTPUT */
        if (strcmp(varyings[i].qualifier, "fragout") == 0)
            continue;
        off += sprintf(hlslOut + off, "    %s %s : %s;\n",
                       glslConvertType(varyings[i].type),
                       varyings[i].name,
                       glslGetSemanticForVarying(&varyings[i], i));
    }
    if (usesFragCoord)
        off += sprintf(hlslOut + off, "    float2 fragCoordXY : VPOS;\n");
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

    if (usesFragCoord)
        off += sprintf(hlslOut + off,
                       "    float4 _glsl_fragCoord = float4(_glsl_in.fragCoordXY, 0.5, 1.0);\n");

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
 * glslDetectFragDataUsage ??? scan for gl_FragData[N] usage and determine
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
 * glslRewriteFragDataAccess ??? rewrite gl_FragData[N] to _glsl_fragDataN
 * for the pixel shader output.
 */
static void glslRewriteFragDataAccess(char *text, int maxFragData)
{
    int i;
    char oldPat[64], newPat[64];

    for (i = 0; i < maxFragData && i < GLSL_MAX_FRAGDATA; i++) {
        sprintf(oldPat, "_glsl_fragData[%d]", i);
        if (maxFragData <= 1)
            strcpy(newPat, "_glsl_fragColor");
        else
            sprintf(newPat, "_glsl_fragData%d", i);
        glslReplaceAll(text, oldPat, newPat);
    }
}

/*
 * glslRecordFnName ??? add the identifier immediately before `open` to a
 * deduplicated name table.  No-ops when the table is absent or full.
 *
 * A definition names a return type before the function name, so an identifier
 * that starts the line is rejected: that shape is a call or a control-flow
 * keyword, not a definition.
 */
static void glslRecordFnName(const char *line, const char *open,
                             char (*fnNames)[GLSL_MAX_NAME_LEN],
                             int *pFnCount, int maxFn)
{
    const char *e, *b;
    int len, i;

    if (!fnNames || !pFnCount || !line || !open) return;
    if (*pFnCount >= maxFn) return;

    e = open;
    while (e > line && (e[-1] == ' ' || e[-1] == '\t')) e--;
    b = e;
    while (b > line && (isalnum((unsigned char)b[-1]) || b[-1] == '_')) b--;

    len = (int)(e - b);
    if (len <= 0 || len >= GLSL_MAX_NAME_LEN) return;
    if (b == line) return;                          /* no return type in front */
    if (isdigit((unsigned char)*b)) return;         /* not an identifier */

    for (i = 0; i < *pFnCount; i++) {
        if ((int)strlen(fnNames[i]) == len &&
            strncmp(fnNames[i], b, (size_t)len) == 0)
            return;
    }

    memcpy(fnNames[*pFnCount], b, (size_t)len);
    fnNames[*pFnCount][len] = '\0';
    (*pFnCount)++;
}

/*
 * glslStripHelperFunctions ??? extract any helper functions defined before main()
 * and return them separately. These need to be emitted before the main function
 * in the HLSL output.
 *
 * For simplicity, we detect functions by looking for patterns like:
 *   type name(...) {
 * that are NOT "void main".
 *
 * `fnNames`/`pFnCount` (nullable, following glslStripLayoutQualifiers'
 * convention) collect the extracted functions' names, deduplicated.  The
 * caller needs them to rename any that would collide with a GLSL or HLSL
 * builtin before the rewrite passes see them.  The heuristic above also
 * matches things that are not function definitions at all ??? an indented
 * "if (...) {" inside main() among them ??? so the names it yields are a
 * superset; the caller filters by an explicit collision list rather than
 * trusting every captured name to be a function.
 */
static void glslExtractHelperFunctions(const char *src, char *helpers, int helpersSize,
                                       char (*fnNames)[GLSL_MAX_NAME_LEN],
                                       int *pFnCount, int maxFn)
{
    const char *p = src;
    int off = 0;
    BOOL seenMain = FALSE;

    helpers[0] = '\0';
    if (pFnCount) *pFnCount = 0;

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

        if (strstr(lineBuf, "void main"))
            seenMain = TRUE;

        /* File-scope const declarations ("const vec4 foo = vec4(...);") are
         * used by main() but live outside it, so the main-body extraction
         * drops them and the declaration-line stripper leaves them behind
         * as orphan text.  They are real globals HLSL accepts verbatim
         * (after the type rename), so pull them into the helpers buffer.
         * Consts that appear inside main()'s body are left alone: the
         * scan reaches them after the "void main" line, so stop there.
         *
         * The declaration is emitted as "static const": a bare global
         * "const float4 x = ...;" in Shader Model 3 declares an external
         * constant that must be set per-frame via SetPixelShaderConstantF,
         * which nothing here does, so the value would be garbage at
         * runtime (D3DCompile only warns X3207).  "static const" is a
         * literal the compiler folds in. */
        {
            char *tok = glslSkipWhitespace(lineBuf);
            if (strncmp(tok, "const ", 6) == 0 && strchr(lineBuf, ';') &&
                !seenMain) {
                const char *end = lineEnd ? lineEnd : lineStart + strlen(lineStart);
                int declLen = (int)(end - lineStart);
                if (off + declLen + 16 < helpersSize) {
                    memcpy(helpers + off, "static ", 7);
                    off += 7;
                    memcpy(helpers + off, lineStart, (size_t)declLen);
                    off += declLen;
                    helpers[off++] = '\n';
                    helpers[off] = '\0';
                }
                p = lineEnd ? lineEnd + 1 : p + strlen(p);
                continue;
            }
        }

        /* Control-flow lines that end in '{' are statements, not function
         * definitions: a "for (float i = 0.0; i < 8.0; i += 1.0) {" line
         * parses as a fake function header, and the brace scan below would
         * drag the whole loop body into the helpers buffer, where HLSL has
         * no place for statements. */
        {
            static const char *const kControl[] = {
                "for", "while", "if", "else", "switch", "do", "return",
                "case", "break", "continue", "discard", "default",
                "{", "}", NULL
            };
            char *tok = glslSkipWhitespace(lineBuf);
            int k;
            BOOL control = FALSE;
            for (k = 0; kControl[k]; k++) {
                size_t l = strlen(kControl[k]);
                if (strncmp(tok, kControl[k], l) == 0 && glslIsWordBoundary(tok[l])) {
                    control = TRUE;
                    break;
                }
            }
            if (control) {
                p = lineEnd ? lineEnd + 1 : p + strlen(p);
                continue;
            }
        }

        /* Check if this looks like a function definition (has parens and opening brace) */
        if (strchr(lineBuf, '(') && !strstr(lineBuf, "void main") &&
            !strstr(lineBuf, "attribute ") && !strstr(lineBuf, "varying ") &&
            !strstr(lineBuf, "uniform ") && !strstr(lineBuf, "#")) {
            /* Check if next non-whitespace after ')' is '{' */
            char *openParen  = strchr(lineBuf, '(');
            char *closeParen = strchr(lineBuf, ')');
            if (closeParen) {
                char *afterParen = glslSkipWhitespace(closeParen + 1);
                if (*afterParen == '{' || *afterParen == '\0') {
                    /* A function header is "type name ( params )" ??? a ';' or
                     * '=' before the first ')' marks a statement, not a
                     * definition.  "float4 vpos = in_Position; if( _va_[0 ].x
                     * > 0 ) {" parses as a fake header, and the brace scan
                     * below would drag the whole if-block into the helpers
                     * buffer, where HLSL has no place for statements.
                     *
                     * The scan stops at the ')' so a one-line body like
                     * "{ return color; }" is not mistaken for a statement. */
                    {
                        const char *hdr = glslSkipWhitespace(lineBuf);
                        BOOL stmtLike = FALSE;
                        while (hdr < closeParen) {
                            if (*hdr == ';' || *hdr == '=') { stmtLike = TRUE; break; }
                            hdr++;
                        }
                        if (!stmtLike) {
                        /* This might be a helper function ??? extract it */
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
                                        glslRecordFnName(lineBuf, openParen,
                                                         fnNames, pFnCount, maxFn);
                                        p = scan + 1;
                                        goto next_iter;
                                    }
                                }
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

/**********************************************************************/
/*****            Bitwise operator lowering                        *****/
/**********************************************************************/

/*
 * D3DCompile rejects &, |, ^, ~, << and >> on every Shader Model 3 profile
 * with "error X3535: Bitwise operations not supported on target ps_3_0",
 * whatever the operands are.  This section rewrites them into float-domain
 * arithmetic before D3DCompile sees the text.
 *
 * The float domain holds every integer below 2^24 exactly (2^24 itself is the
 * first integer that rounds), so all results are exact for the values real
 * shaders use.  The identities are:
 *
 *   x & y : sum over the bits both operands set
 *   x | y : x + y - (x & y)
 *   x ^ y : x + y - 2 * (x & y)
 *   ~x    : -x - 1            (exact, GLSL defines bitwise not as -x-1)
 *   x << y: x * 2^y
 *   x >> y: floor(x / 2^y)
 *
 * When one operand is an integer constant, the & / | / ^ forms collapse onto
 * a single-operand helper that only tests the bits the constant sets
 * (_glsl_and_0000FF(a)); constants outside 0..2^24-1 are folded to their low
 * 24 bits, which is exact for & (the variable operand has no bits above 23)
 * and the closest representable approximation for | and ^.  Shift counts are
 * clamped to 24 bits.  The helpers are emitted once, into the helpers buffer,
 * by glslBitEmitHelpers.
 *
 * Not covered, and left to surface as D3DCompile's own error (or, for the
 * 823-name fallback path, to the software rasterizer):
 *   - vector operands: the helpers take scalars, so a vector bitwise
 *     expression becomes an HLSL type error, which is at least honest.
 *   - compound assignments whose lvalue has side effects (a[i++] |= 2) are
 *     expanded textually, so the lvalue is evaluated twice.  Simple
 *     identifiers and array/member lvalues are exact.
 */

/* glslBitSprintf / glslBitAppendf / glslBitAppendN ??? bounded text building. */

static int glslBitSprintf(char *out, int outSize, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (!out || outSize <= 1) return -1;
    va_start(ap, fmt);
    n = _vsnprintf(out, outSize - 1, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= outSize - 1) return -1;
    return n;
}

static int glslBitAppendN(char *out, int outSize, const char *s, int n)
{
    int len = (int)strlen(out);
    if (len + n + 1 > outSize) return -1;
    memcpy(out + len, s, n);
    out[len + n] = '\0';
    return n;
}

static int glslBitAppendf(char *out, int outSize, const char *fmt, ...)
{
    va_list ap;
    int n, base;
    if (!out || outSize <= 1) return -1;
    base = (int)strlen(out);
    va_start(ap, fmt);
    n = _vsnprintf(out + base, outSize - 1 - base, fmt, ap);
    va_end(ap);
    if (n < 0 || base + n >= outSize - 1) return -1;
    return n;
}

/* glslBitSkipSpace ??? whitespace and comments, for token separation. */

static const char *glslBitSkipSpace(const char *p)
{
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { p++; continue; }
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        break;
    }
    return p;
}

/* glslBitIsKeyword ??? is [word, word+len) a GLSL keyword?  Used by the left
 * walk: a keyword ends an expression, so the rewrite never starts inside a
 * declaration or a control-flow keyword's operand list. */

static BOOL glslBitIsKeyword(const char *word, int len)
{
    static const char *const kWords[] = {
        "return", "if", "for", "while", "do", "else", "case", "switch",
        "break", "continue", "discard", "in", "out", "inout", "const",
        "uniform", "attribute", "varying", "struct", "void",
        "float", "int", "uint", "bool", "double",
        "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4",
        "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4",
        "dvec2", "dvec3", "dvec4",
        "mat2", "mat3", "mat4", "mat2x2", "mat2x3", "mat2x4",
        "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4",
        "sampler1D", "sampler2D", "sampler3D", "samplerCube",
        "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow",
        "sampler1DArray", "sampler2DArray", "sampler1DArrayShadow",
        "sampler2DArrayShadow", "samplerCubeArray", "samplerCubeArrayShadow",
        "sampler2DMS", "sampler2DMSArray",
        "isampler1D", "isampler2D", "isampler3D", "isamplerCube",
        "usampler1D", "usampler2D", "usampler3D", "usamplerCube",
        "flat", "smooth", "noperspective", "centroid", "invariant",
        "layout", "precision", "highp", "mediump", "lowp",
        "true", "false",
        NULL
    };
    int i;
    if (len <= 0) return FALSE;
    for (i = 0; kWords[i]; i++) {
        if ((int)strlen(kWords[i]) == len && memcmp(kWords[i], word, len) == 0)
            return TRUE;
    }
    return FALSE;
}

/* glslBitParseIntLiteral ??? parse a pure integer literal (decimal or 0x, with
 * an optional sign and u/l suffixes).  The whole text must be the literal.
 * Used both to fold constant-only expressions and to specialize the helper
 * on a constant operand. */

static BOOL glslBitParseIntLiteral(const char *text, long long *pVal)
{
    const char *p = text;
    BOOL neg = FALSE;
    int base = 10;
    long long v = 0;

    if (!text || !pVal) return FALSE;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { neg = TRUE; p++; }
    else if (*p == '+') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    if (!((base == 16 && isxdigit((unsigned char)*p)) ||
          (base == 10 && isdigit((unsigned char)*p))))
        return FALSE;
    while (base == 16 ? isxdigit((unsigned char)*p) : isdigit((unsigned char)*p)) {
        int d = isdigit((unsigned char)*p)
              ? (*p - '0')
              : (tolower((unsigned char)*p) - 'a' + 10);
        v = v * base + d;
        p++;
    }
    while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\0') return FALSE;
    *pVal = neg ? -v : v;
    return TRUE;
}

/* glslBitAddConst / glslBitHasConst ??? the constant-helper table. */

static BOOL glslBitAddConst(unsigned int *arr, int *count, unsigned int v)
{
    int i;
    for (i = 0; i < *count; i++)
        if (arr[i] == v) return TRUE;
    if (*count >= GLSL_BIT_MAX_LITS) return FALSE;
    arr[(*count)++] = v;
    return TRUE;
}

static BOOL glslBitHasConst(const unsigned int *arr, int count, unsigned int v)
{
    int i;
    for (i = 0; i < count; i++)
        if (arr[i] == v) return TRUE;
    return FALSE;
}

/* glslBitEmitMasked ??? emit _glsl_and_<hex>(operand) (or or/xor), registering
 * the constant's helper.  Out-of-range constants warn once and use their low
 * 24 bits; a full constant table falls back to the general helper with the
 * value as a float literal, which is exact below 2^24. */

static BOOL glslBitEmitMasked(glslBitCtx *ctx, int kind, long long raw,
                              const char *operand, char *out, int outSize)
{
    unsigned int v = (unsigned int)raw & 0xFFFFFFu;
    char hex[16];
    const char *fn;
    unsigned int *arr;
    int *count;

    if (raw != (long long)v && !ctx->col->warnedRange) {
        ctx->col->warnedRange = TRUE;
        gldDiagLog("GLSL->HLSL: bitwise constant %lld carries bits above the "
                   "24-bit exact-integer ceiling; its low 24 bits are used.",
                   raw);
    }

    switch (kind) {
    case GLSL_BIT_AND:
        fn = "_glsl_and_"; arr = ctx->col->andConst; count = &ctx->col->andCount;
        break;
    case GLSL_BIT_OR:
        fn = "_glsl_or_";  arr = ctx->col->orConst;  count = &ctx->col->orCount;
        break;
    default:
        fn = "_glsl_xor_"; arr = ctx->col->xorConst; count = &ctx->col->xorCount;
        break;
    }

    sprintf(hex, "%06X", v);

    if (!glslBitAddConst(arr, count, v)) {
        /* constant table full: use the general helper with a float literal */
        switch (kind) {
        case GLSL_BIT_AND:
            ctx->col->hasGeneralAnd = TRUE;
            return glslBitSprintf(out, outSize, "_glsl_bitand(%s, %u.0)", operand, v) >= 0;
        case GLSL_BIT_OR:
            ctx->col->hasGeneralOr = TRUE;
            ctx->col->hasGeneralAnd = TRUE;
            return glslBitSprintf(out, outSize, "_glsl_bitor(%s, %u.0)", operand, v) >= 0;
        default:
            ctx->col->hasGeneralXor = TRUE;
            ctx->col->hasGeneralAnd = TRUE;
            return glslBitSprintf(out, outSize, "_glsl_bitxor(%s, %u.0)", operand, v) >= 0;
        }
    }

    /* the or/xor helpers reference the and helper for the same constant */
    if (kind != GLSL_BIT_AND)
        glslBitAddConst(ctx->col->andConst, &ctx->col->andCount, v);

    return glslBitSprintf(out, outSize, "%s%s(%s)", fn, hex, operand) >= 0;
}

/* glslBitOpText ??? the verbatim spelling for the ops SM3 accepts natively. */

static const char *glslBitOpText(int opId)
{
    switch (opId) {
    case GLSL_BIT_LOR: return "||";
    case GLSL_BIT_LAND: return "&&";
    case GLSL_BIT_EQ: return "==";
    case GLSL_BIT_NE: return "!=";
    case GLSL_BIT_LT: return "<";
    case GLSL_BIT_GT: return ">";
    case GLSL_BIT_LE: return "<=";
    case GLSL_BIT_GE: return ">=";
    case GLSL_BIT_ADD: return "+";
    case GLSL_BIT_SUB: return "-";
    case GLSL_BIT_MUL: return "*";
    case GLSL_BIT_DIV: return "/";
    case GLSL_BIT_MOD: return "%";
    default: return "";
    }
}

/* glslBitCombine ??? combine a binary operator's two (already rewritten)
 * operand texts into the lowered form.  Returns FALSE on buffer overflow.
 * The default branch is the verbatim spelling for the operators SM3 accepts
 * natively (||, &&, comparisons, +, -, *, /, %). */

static BOOL glslBitCombine(glslBitCtx *ctx, int opId,
                           const char *lhs, const char *rhs,
                           char *out, int outSize)
{
    long long v1 = 0, v2 = 0;
    BOOL c1 = glslBitParseIntLiteral(lhs, &v1);
    BOOL c2 = glslBitParseIntLiteral(rhs, &v2);

    switch (opId) {
    case GLSL_BIT_AND:
        if (c1 && c2)
            return glslBitSprintf(out, outSize, "%u",
                ((unsigned int)v1 & 0xFFFFFFu) & ((unsigned int)v2 & 0xFFFFFFu)) >= 0;
        if (c2) return glslBitEmitMasked(ctx, GLSL_BIT_AND, v2, lhs, out, outSize);
        if (c1) return glslBitEmitMasked(ctx, GLSL_BIT_AND, v1, rhs, out, outSize);
        ctx->col->hasGeneralAnd = TRUE;
        return glslBitSprintf(out, outSize, "_glsl_bitand(%s, %s)", lhs, rhs) >= 0;
    case GLSL_BIT_OR:
        if (c1 && c2)
            return glslBitSprintf(out, outSize, "%u",
                ((unsigned int)v1 & 0xFFFFFFu) | ((unsigned int)v2 & 0xFFFFFFu)) >= 0;
        if (c2) return glslBitEmitMasked(ctx, GLSL_BIT_OR, v2, lhs, out, outSize);
        if (c1) return glslBitEmitMasked(ctx, GLSL_BIT_OR, v1, rhs, out, outSize);
        ctx->col->hasGeneralOr = TRUE;
        ctx->col->hasGeneralAnd = TRUE;   /* _glsl_bitor calls _glsl_bitand */
        return glslBitSprintf(out, outSize, "_glsl_bitor(%s, %s)", lhs, rhs) >= 0;
    case GLSL_BIT_XOR:
        if (c1 && c2)
            return glslBitSprintf(out, outSize, "%u",
                ((unsigned int)v1 & 0xFFFFFFu) ^ ((unsigned int)v2 & 0xFFFFFFu)) >= 0;
        if (c2) return glslBitEmitMasked(ctx, GLSL_BIT_XOR, v2, lhs, out, outSize);
        if (c1) return glslBitEmitMasked(ctx, GLSL_BIT_XOR, v1, rhs, out, outSize);
        ctx->col->hasGeneralXor = TRUE;
        ctx->col->hasGeneralAnd = TRUE;   /* _glsl_bitxor calls _glsl_bitand */
        return glslBitSprintf(out, outSize, "_glsl_bitxor(%s, %s)", lhs, rhs) >= 0;
    case GLSL_BIT_SHL:
        if (c1 && c2) {
            if (v2 < 0 || v2 > 23) return glslBitSprintf(out, outSize, "0") >= 0;
            return glslBitSprintf(out, outSize, "%lld", v1 << (int)v2) >= 0;
        }
        if (c2) {
            if (v2 < 0 || v2 > 23) return glslBitSprintf(out, outSize, "(0.0)") >= 0;
            return glslBitSprintf(out, outSize, "(%s * %llu.0)", lhs, 1ULL << (int)v2) >= 0;
        }
        ctx->col->hasGeneralShl = TRUE;
        return glslBitSprintf(out, outSize, "_glsl_shl(%s, %s)", lhs, rhs) >= 0;
    case GLSL_BIT_SHR:
        if (c1 && c2) {
            if (v2 < 0 || v2 > 23 || v1 < 0)
                return glslBitSprintf(out, outSize, "0") >= 0;
            return glslBitSprintf(out, outSize, "%lld", v1 >> (int)v2) >= 0;
        }
        if (c2) {
            if (v2 < 0 || v2 > 23) return glslBitSprintf(out, outSize, "(0.0)") >= 0;
            return glslBitSprintf(out, outSize, "(floor(%s / %llu.0))", lhs, 1ULL << (int)v2) >= 0;
        }
        ctx->col->hasGeneralShr = TRUE;
        return glslBitSprintf(out, outSize, "_glsl_shr(%s, %s)", lhs, rhs) >= 0;
    case GLSL_BIT_NOT:
        return glslBitSprintf(out, outSize, "(-(%s) - 1.0)", lhs) >= 0;
    default:
        return glslBitSprintf(out, outSize, "%s %s %s", lhs, glslBitOpText(opId), rhs) >= 0;
    }
}

/* glslBitOpLen ??? token length of a binary operator. */

static int glslBitOpLen(int opId)
{
    switch (opId) {
    case GLSL_BIT_LOR: case GLSL_BIT_LAND:
    case GLSL_BIT_EQ: case GLSL_BIT_NE:
    case GLSL_BIT_LE: case GLSL_BIT_GE:
    case GLSL_BIT_SHL: case GLSL_BIT_SHR:
        return 2;
    default:
        return 1;
    }
}

/* glslBitPeekBinOp ??? the operator the text at p starts with, if it belongs to
 * exactly this precedence level (levels follow GLSL: 0 = ternary, 1 = ||,
 * 2 = &&, 3 = |, 4 = ^, 5 = &, 6 = ==/!=, 7 = relational, 8 = shift,
 * 9 = additive, 10 = multiplicative).  The two-character guards keep &&, ||,
 * <=, >=, ==, !=, <<, >> and the compound assignments out of each other's
 * way. */

static int glslBitPeekBinOp(const char *p, int level)
{
    switch (level) {
    case 0: return (p[0] == '?') ? GLSL_BIT_TERN : GLSL_BIT_NONE;
    case 1: return (p[0] == '|' && p[1] == '|') ? GLSL_BIT_LOR : GLSL_BIT_NONE;
    case 2: return (p[0] == '&' && p[1] == '&') ? GLSL_BIT_LAND : GLSL_BIT_NONE;
    case 3: return (p[0] == '|' && p[1] != '|' && p[1] != '=') ? GLSL_BIT_OR : GLSL_BIT_NONE;
    case 4: return (p[0] == '^' && p[1] != '^' && p[1] != '=') ? GLSL_BIT_XOR : GLSL_BIT_NONE;
    case 5: return (p[0] == '&' && p[1] != '&' && p[1] != '=') ? GLSL_BIT_AND : GLSL_BIT_NONE;
    case 6:
        if (p[0] == '=' && p[1] == '=') return GLSL_BIT_EQ;
        if (p[0] == '!' && p[1] == '=') return GLSL_BIT_NE;
        return GLSL_BIT_NONE;
    case 7:
        if (p[0] == '<' && p[1] == '=') return GLSL_BIT_LE;
        if (p[0] == '>' && p[1] == '=') return GLSL_BIT_GE;
        if (p[0] == '<' && p[1] != '<') return GLSL_BIT_LT;
        if (p[0] == '>' && p[1] != '>') return GLSL_BIT_GT;
        return GLSL_BIT_NONE;
    case 8:
        if (p[0] == '<' && p[1] == '<' && p[2] != '=') return GLSL_BIT_SHL;
        if (p[0] == '>' && p[1] == '>' && p[2] != '=') return GLSL_BIT_SHR;
        return GLSL_BIT_NONE;
    case 9:
        if (p[0] == '+') return GLSL_BIT_ADD;
        if (p[0] == '-') return GLSL_BIT_SUB;
        return GLSL_BIT_NONE;
    case 10:
        if (p[0] == '*') return GLSL_BIT_MUL;
        if (p[0] == '/') return GLSL_BIT_DIV;
        if (p[0] == '%') return GLSL_BIT_MOD;
        return GLSL_BIT_NONE;
    default:
        return GLSL_BIT_NONE;
    }
}

#define GLSL_BIT_FRAME 2048
#define GLSL_BIT_TEMP  4096

static BOOL glslBitParseExpr(glslBitCtx *ctx, const char *p, int level,
                             char *out, int outSize, const char **pEnd);

/* glslBitParsePrimary ??? literal, identifier, or parenthesized expression.
 * Literals and identifiers are copied verbatim; nested expressions are
 * parsed (and therefore lowered) recursively. */

static BOOL glslBitParsePrimary(glslBitCtx *ctx, const char *p,
                                char *out, int outSize, const char **pEnd)
{
    const char *q = glslBitSkipSpace(p);
    char c = *q;

    if (c == '(') {
        char inner[GLSL_BIT_TEMP];
        const char *iEnd, *r;
        if (!glslBitParseExpr(ctx, q + 1, 0, inner, sizeof(inner), &iEnd))
            return FALSE;
        r = glslBitSkipSpace(iEnd);
        if (*r != ')') return FALSE;
        if (glslBitSprintf(out, outSize, "(%s)", inner) < 0) return FALSE;
        *pEnd = r + 1;
        return TRUE;
    }

    if (isdigit((unsigned char)c) ||
        (c == '.' && isdigit((unsigned char)q[1]))) {
        const char *s = q;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s += 2;
            while (isxdigit((unsigned char)*s)) s++;
        } else {
            while (isdigit((unsigned char)*s)) s++;
            if (*s == '.') { s++; while (isdigit((unsigned char)*s)) s++; }
            if (*s == 'e' || *s == 'E') {
                s++;
                if (*s == '+' || *s == '-') s++;
                while (isdigit((unsigned char)*s)) s++;
            }
        }
        while (*s == 'u' || *s == 'U' || *s == 'l' || *s == 'L' ||
               *s == 'f' || *s == 'F') s++;
        if (s - q >= outSize) return FALSE;
        memcpy(out, q, s - q);
        out[s - q] = '\0';
        *pEnd = s;
        return TRUE;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        const char *s = q;
        while (isalnum((unsigned char)*s) || *s == '_') s++;
        if (s - q >= outSize) return FALSE;
        memcpy(out, q, s - q);
        out[s - q] = '\0';
        *pEnd = s;
        return TRUE;
    }

    return FALSE;
}

/* glslBitParsePostfix ??? primary plus [index], .member, call(args) and
 * postfix ++/-- chains.  Call arguments and array indices are expressions
 * in their own right, so bitwise operators inside them are lowered here. */

static BOOL glslBitParsePostfix(glslBitCtx *ctx, const char *p,
                                char *out, int outSize, const char **pEnd)
{
    if (!glslBitParsePrimary(ctx, p, out, outSize, pEnd))
        return FALSE;
    p = *pEnd;
    for (;;) {
        const char *q = glslBitSkipSpace(p);
        if (*q == '[') {
            char idx[GLSL_BIT_FRAME];
            const char *iEnd, *r;
            if (!glslBitParseExpr(ctx, q + 1, 0, idx, sizeof(idx), &iEnd))
                return FALSE;
            r = glslBitSkipSpace(iEnd);
            if (*r != ']') return FALSE;
            if (glslBitAppendf(out, outSize, "[%s]", idx) < 0) return FALSE;
            p = r + 1;
        } else if (*q == '.') {
            const char *s = q + 1;
            while (isalnum((unsigned char)*s) || *s == '_') s++;
            if (s == q + 1) return FALSE;
            if (glslBitAppendf(out, outSize, ".") < 0) return FALSE;
            if (glslBitAppendN(out, outSize, q + 1, (int)(s - q - 1)) < 0) return FALSE;
            p = s;
        } else if (*q == '(') {
            char args[GLSL_BIT_TEMP];
            int aLen = 0;
            const char *a = q + 1;
            BOOL first = TRUE;
            BOOL closed = FALSE;
            args[0] = '\0';
            for (;;) {
                const char *ae;
                a = glslBitSkipSpace(a);
                if (*a == ')') { p = a + 1; closed = TRUE; break; }
                if (!first) {
                    if (*a != ',') return FALSE;
                    if (aLen + 2 >= (int)sizeof(args)) return FALSE;
                    args[aLen++] = ',';
                    args[aLen++] = ' ';
                    args[aLen] = '\0';
                    a = glslBitSkipSpace(a + 1);
                }
                if (!glslBitParseExpr(ctx, a, 0, args + aLen,
                                      (int)sizeof(args) - aLen, &ae))
                    return FALSE;
                aLen = (int)strlen(args);
                a = ae;
                first = FALSE;
            }
            if (!closed) return FALSE;
            if (glslBitAppendf(out, outSize, "(%s)", args) < 0) return FALSE;
        } else if ((q[0] == '+' && q[1] == '+') ||
                   (q[0] == '-' && q[1] == '-')) {
            if (glslBitAppendf(out, outSize, "%c%c", q[0], q[1]) < 0)
                return FALSE;
            p = q + 2;
        } else {
            break;
        }
    }
    *pEnd = p;
    return TRUE;
}

/* glslBitParseUnary ??? prefix + - ! ~ ++ -- then the postfix chain.  ~ is the
 * only prefix operator needing lowering: -(operand) - 1 is exact for every
 * integer in the float domain. */

static BOOL glslBitParseUnary(glslBitCtx *ctx, const char *p,
                              char *out, int outSize, const char **pEnd)
{
    const char *q = glslBitSkipSpace(p);

    if (q[0] == '~') {
        char op[GLSL_BIT_FRAME];
        const char *oEnd;
        if (!glslBitParseExpr(ctx, q + 1, 12, op, sizeof(op), &oEnd))
            return FALSE;
        if (glslBitCombine(ctx, GLSL_BIT_NOT, op, "", out, outSize) < 0)
            return FALSE;
        *pEnd = oEnd;
        return TRUE;
    }
    if (q[0] == '+' || q[0] == '-' || q[0] == '!') {
        char op[GLSL_BIT_FRAME];
        const char *oEnd;
        if (!glslBitParseExpr(ctx, q + 1, 12, op, sizeof(op), &oEnd))
            return FALSE;
        if (glslBitSprintf(out, outSize, "%c%s", q[0], op) < 0)
            return FALSE;
        *pEnd = oEnd;
        return TRUE;
    }
    if ((q[0] == '+' && q[1] == '+') || (q[0] == '-' && q[1] == '-')) {
        char op[GLSL_BIT_FRAME];
        const char *oEnd;
        if (!glslBitParseExpr(ctx, q + 2, 12, op, sizeof(op), &oEnd))
            return FALSE;
        if (glslBitSprintf(out, outSize, "%c%c%s", q[0], q[1], op) < 0)
            return FALSE;
        *pEnd = oEnd;
        return TRUE;
    }
    return glslBitParseExpr(ctx, q, 12, out, outSize, pEnd);
}

/* glslBitParseExpr ??? recursive descent over the precedence ladder.  The
 * caller's out buffer accumulates the rebuilt expression; each binary level
 * parses its left operand into out, then combines left+operator+right through
 * glslBitCombine (which lowers the bitwise operators and copies everything
 * else verbatim).  Returns FALSE on any syntax or buffer failure. */

static BOOL glslBitParseExpr(glslBitCtx *ctx, const char *p, int level,
                             char *out, int outSize, const char **pEnd)
{
    if (level >= 13)
        return glslBitParsePrimary(ctx, p, out, outSize, pEnd);
    if (level == 12)
        return glslBitParsePostfix(ctx, p, out, outSize, pEnd);
    if (level == 11)
        return glslBitParseUnary(ctx, p, out, outSize, pEnd);

    if (!glslBitParseExpr(ctx, p, level + 1, out, outSize, pEnd))
        return FALSE;
    p = *pEnd;

    for (;;) {
        int opId;
        const char *q = glslBitSkipSpace(p);
        if (!*q) break;
        opId = glslBitPeekBinOp(q, level);
        if (!opId) break;

        if (level == 0 && opId == GLSL_BIT_TERN) {
            char trueBuf[GLSL_BIT_FRAME], falseBuf[GLSL_BIT_FRAME];
            char tmp[GLSL_BIT_TEMP];
            const char *tEnd, *c, *fEnd;
            if (!glslBitParseExpr(ctx, q + 1, 0, trueBuf, sizeof(trueBuf), &tEnd))
                return FALSE;
            c = glslBitSkipSpace(tEnd);
            if (*c != ':') return FALSE;
            if (!glslBitParseExpr(ctx, c + 1, 0, falseBuf, sizeof(falseBuf), &fEnd))
                return FALSE;
            if (glslBitSprintf(tmp, sizeof(tmp), "%s ? %s : %s",
                               out, trueBuf, falseBuf) < 0)
                return FALSE;
            if ((int)strlen(tmp) + 1 > outSize) return FALSE;
            strcpy(out, tmp);
            p = fEnd;
            break;
        }

        {
            char rhsBuf[GLSL_BIT_FRAME], tmp[GLSL_BIT_TEMP];
            const char *rEnd;
            if (!glslBitParseExpr(ctx, q + glslBitOpLen(opId), level,
                                  rhsBuf, sizeof(rhsBuf), &rEnd))
                return FALSE;
            if (!glslBitCombine(ctx, opId, out, rhsBuf, tmp, sizeof(tmp)))
                return FALSE;
            if ((int)strlen(tmp) + 1 > outSize) return FALSE;
            strcpy(out, tmp);
            p = rEnd;
        }
    }
    *pEnd = p;
    return TRUE;
}

/* glslBitWalkLeft ??? find where the expression containing the operator at
 * 'pos' starts.  Walks left across identifiers, calls, member access and
 * parenthesized groups; stops at statement-level punctuation, keywords,
 * relational/logical operators (which bind looser than the bitwise ones),
 * binary arithmetic operators and comments.  Unary + and - (and therefore
 * the ++/-- pair) are not stops: they belong to the expression. */

static int glslBitWalkLeft(const char *text, int pos)
{
    int depth = 0;
    int q = pos - 1;

    while (q >= 0) {
        char c = text[q];

        if (c == ')') { depth++; q--; continue; }
        if (c == '(') {
            if (depth == 0) return q + 1;
            depth--; q--; continue;
        }
        if (depth > 0) { q--; continue; }

        if (c == ';' || c == '{' || c == '}' || c == ',' ||
            c == '=' || c == '?' || c == ':')
            return q + 1;

        if (c == '&' && (text[q + 1] == '&' || (q > 0 && text[q - 1] == '&')))
            return q + 1;
        if (c == '|' && (text[q + 1] == '|' || (q > 0 && text[q - 1] == '|')))
            return q + 1;

        if (c == '/' && (text[q + 1] == '/' || text[q - 1] == '/')) {
            while (q >= 0 && text[q] != '\n') q--;
            continue;
        }
        if ((c == '/' && text[q + 1] == '*') || (c == '*' && text[q + 1] == '/')) {
            int r = q;
            while (r > 0 && !(text[r - 1] == '/' && text[r] == '*')) r--;
            q = r - 1;
            continue;
        }

        if (c == '<' && (text[q + 1] == '<' || (q > 0 && text[q - 1] == '<'))) {
            q--; continue;
        }
        if (c == '>' && (text[q + 1] == '>' || (q > 0 && text[q - 1] == '>'))) {
            q--; continue;
        }
        if (c == '<' || c == '>')
            return q + 1;

        if (c == '+' || c == '-') {
            int r = q - 1;
            while (r >= 0 && (text[r] == ' ' || text[r] == '\t' ||
                              text[r] == '\r' || text[r] == '\n'))
                r--;
            if (r >= 0 && (isalnum((unsigned char)text[r]) ||
                           text[r] == ')' || text[r] == ']'))
                return q + 1;   /* binary: binds looser than bitwise */
            q--; continue;      /* unary: belongs to the expression */
        }
        if (c == '*' || c == '/' || c == '%')
            return q + 1;

        if (c == ' ') {
            int ws = q - 1;
            while (ws >= 0 && (isalnum((unsigned char)text[ws]) || text[ws] == '_'))
                ws--;
            if (glslBitIsKeyword(text + ws + 1, q - ws - 1))
                return q;
            q--;
            continue;
        }

        q--;
    }
    return 0;
}

/* glslBitProcess ??? one operator found by the scanner.  Parses the left
 * operand (up to the operator) and the right operand (from after it),
 * combines them, and splices the result back into the buffer.  Returns the
 * next scan position, or -1 to leave the text untouched. */

static int glslBitProcess(glslBitCtx *ctx, char *text, int textSize,
                          int pos, int opId, int opLen)
{
    const char *t = text;
    int level;

    switch (opId) {
    case GLSL_BIT_AND: level = 5; break;
    case GLSL_BIT_OR:  level = 3; break;
    case GLSL_BIT_XOR: level = 4; break;
    case GLSL_BIT_SHL:
    case GLSL_BIT_SHR: level = 8; break;
    default:           level = 0; break;
    }

    if (opId == GLSL_BIT_NOT) {
        const char *opP = glslBitSkipSpace(t + pos + 1);
        char *opBuf, *combined;
        const char *opEnd;
        int spanStart = pos, spanEnd, cLen, delta;

        opBuf = (char *)malloc((textSize - (int)(opP - t)) * 4 + 128);
        if (!opBuf) return -1;
        if (!glslBitParseExpr(ctx, opP, 11, opBuf,
                              (textSize - (int)(opP - t)) * 4 + 128, &opEnd) ||
            opEnd == opP) {
            free(opBuf);
            return -1;
        }
        combined = (char *)malloc((int)strlen(opBuf) + 64);
        if (!combined) { free(opBuf); return -1; }
        if (!glslBitCombine(ctx, GLSL_BIT_NOT, opBuf, "", combined,
                            (int)strlen(opBuf) + 64)) {
            free(opBuf); free(combined); return -1;
        }
        spanEnd = (int)(opEnd - t);
        cLen = (int)strlen(combined);
        delta = cLen - (spanEnd - spanStart);
        if (delta > 0 && spanEnd + delta > textSize) {
            free(opBuf); free(combined); return -1;
        }
        memmove(text + spanStart + cLen, text + spanEnd, strlen(text + spanEnd) + 1);
        memcpy(text + spanStart, combined, cLen);
        free(opBuf); free(combined);
        return spanStart + cLen;
    }

    if (opId == GLSL_BIT_AND_EQ || opId == GLSL_BIT_OR_EQ ||
        opId == GLSL_BIT_XOR_EQ || opId == GLSL_BIT_SHL_EQ ||
        opId == GLSL_BIT_SHR_EQ) {
        /* compound assignment:  lhs <op>= rhs  ->  lhs = lhs <op> rhs */
        int baseOp;
        int L = glslBitWalkLeft(text, pos);
        int spanStart;
        char *lhsBuf, *rhsBuf, *combined, *result;
        const char *lhsEnd, *pe, *rhsP, *rhsEnd;
        int spanEnd, cLen, rLen, delta;

        switch (opId) {
        case GLSL_BIT_AND_EQ: baseOp = GLSL_BIT_AND; break;
        case GLSL_BIT_OR_EQ:  baseOp = GLSL_BIT_OR;  break;
        case GLSL_BIT_XOR_EQ: baseOp = GLSL_BIT_XOR; break;
        case GLSL_BIT_SHL_EQ: baseOp = GLSL_BIT_SHL; break;
        default:              baseOp = GLSL_BIT_SHR; break;
        }
        if (L > pos) return -1;
        /* The walk can stop on a keyword or '=' with whitespace between it
         * and the expression; keep that whitespace out of the replaced span
         * ("return a & 3;" must stay "return _glsl_and_3(a);", not
         * "return_glsl_and_3(a);"). */
        spanStart = L;
        while (text[spanStart] == ' ' || text[spanStart] == '\t' ||
               text[spanStart] == '\r' || text[spanStart] == '\n')
            spanStart++;
        lhsBuf = (char *)malloc((pos - L) * 4 + 128);
        if (!lhsBuf) return -1;
        if (!glslBitParseExpr(ctx, t + L, 0, lhsBuf, (pos - L) * 4 + 128, &lhsEnd)) {
            free(lhsBuf); return -1;
        }
        pe = glslBitSkipSpace(lhsEnd);
        if (pe != t + pos) { free(lhsBuf); return -1; }
        rhsP = glslBitSkipSpace(t + pos + opLen);
        if (!*rhsP) { free(lhsBuf); return -1; }
        rhsBuf = (char *)malloc((textSize - (int)(rhsP - t)) * 4 + 128);
        if (!rhsBuf) { free(lhsBuf); return -1; }
        if (!glslBitParseExpr(ctx, rhsP, 0, rhsBuf,
                              (textSize - (int)(rhsP - t)) * 4 + 128, &rhsEnd)) {
            free(lhsBuf); free(rhsBuf); return -1;
        }
        combined = (char *)malloc((pos - L) * 4 + (int)strlen(rhsBuf) + 256);
        if (!combined) { free(lhsBuf); free(rhsBuf); return -1; }
        if (!glslBitCombine(ctx, baseOp, lhsBuf, rhsBuf, combined,
                            (pos - L) * 4 + (int)strlen(rhsBuf) + 256)) {
            free(lhsBuf); free(rhsBuf); free(combined); return -1;
        }
        rLen = (int)strlen(combined);
        result = (char *)malloc((int)strlen(lhsBuf) + rLen + 16);
        if (!result) { free(lhsBuf); free(rhsBuf); free(combined); return -1; }
        if (glslBitSprintf(result, (int)strlen(lhsBuf) + rLen + 16,
                           "%s = %s", lhsBuf, combined) < 0) {
            free(lhsBuf); free(rhsBuf); free(combined); free(result); return -1;
        }
        spanStart = L;
        while (text[spanStart] == ' ' || text[spanStart] == '\t' ||
               text[spanStart] == '\r' || text[spanStart] == '\n')
            spanStart++;
        spanEnd = (int)(rhsEnd - t);
        cLen = (int)strlen(result);
        delta = cLen - (spanEnd - spanStart);
        if (delta > 0 && spanEnd + delta > textSize) {
            free(lhsBuf); free(rhsBuf); free(combined); free(result); return -1;
        }
        memmove(text + spanStart + cLen, text + spanEnd, strlen(text + spanEnd) + 1);
        memcpy(text + spanStart, result, cLen);
        free(lhsBuf); free(rhsBuf); free(combined); free(result);
        return spanStart + cLen;
    }

    /* simple binary operator */
    {
        int L = glslBitWalkLeft(text, pos);
        int spanStart;
        char *lhsBuf, *rhsBuf, *combined;
        const char *lhsEnd, *pe, *rhsP, *rhsEnd;
        int spanEnd, cLen, delta;

        if (L > pos) return -1;
        /* see the compound path above: keep the walk's leading whitespace */
        spanStart = L;
        while (text[spanStart] == ' ' || text[spanStart] == '\t' ||
               text[spanStart] == '\r' || text[spanStart] == '\n')
            spanStart++;
        lhsBuf = (char *)malloc((pos - L) * 4 + 128);
        if (!lhsBuf) return -1;
        if (!glslBitParseExpr(ctx, t + L, level + 1, lhsBuf,
                              (pos - L) * 4 + 128, &lhsEnd)) {
            free(lhsBuf); return -1;
        }
        pe = glslBitSkipSpace(lhsEnd);
        if (pe != t + pos) { free(lhsBuf); return -1; }
        rhsP = glslBitSkipSpace(t + pos + opLen);
        if (!*rhsP) { free(lhsBuf); return -1; }
        rhsBuf = (char *)malloc((textSize - (int)(rhsP - t)) * 4 + 128);
        if (!rhsBuf) { free(lhsBuf); return -1; }
        if (!glslBitParseExpr(ctx, rhsP, level, rhsBuf,
                              (textSize - (int)(rhsP - t)) * 4 + 128, &rhsEnd)) {
            free(lhsBuf); free(rhsBuf); return -1;
        }
        combined = (char *)malloc((pos - L) * 4 + (int)strlen(rhsBuf) + 256);
        if (!combined) { free(lhsBuf); free(rhsBuf); return -1; }
        if (!glslBitCombine(ctx, opId, lhsBuf, rhsBuf, combined,
                            (pos - L) * 4 + (int)strlen(rhsBuf) + 256)) {
            free(lhsBuf); free(rhsBuf); free(combined); return -1;
        }
        spanStart = L;
        while (text[spanStart] == ' ' || text[spanStart] == '\t' ||
               text[spanStart] == '\r' || text[spanStart] == '\n')
            spanStart++;
        spanEnd = (int)(rhsEnd - t);
        cLen = (int)strlen(combined);
        delta = cLen - (spanEnd - spanStart);
        if (delta > 0 && spanEnd + delta > textSize) {
            free(lhsBuf); free(rhsBuf); free(combined); return -1;
        }
        memmove(text + spanStart + cLen, text + spanEnd, strlen(text + spanEnd) + 1);
        memcpy(text + spanStart, combined, cLen);
        free(lhsBuf); free(rhsBuf); free(combined);
        return spanStart + cLen;
    }
}

/* glslRewriteBitwise ??? scan a buffer for bitwise operators (outside comments)
 * and lower each one.  Successful replacements splice in the rewritten text;
 * failures are left verbatim and warn once, so a genuinely unlowerable
 * construct still reaches D3DCompile with its own error message. */

static void glslRewriteBitwise(char *text, int textSize, glslBitCollector *col)
{
    glslBitCtx ctx;
    int i = 0;

    ctx.col = col;
    ctx.failed = FALSE;

    while (text[i]) {
        char c = text[i];
        int opId = 0, opLen = 0, newPos;

        if (c == '/' && text[i + 1] == '/') {
            while (text[i] && text[i] != '\n') i++;
            continue;
        }
        if (c == '/' && text[i + 1] == '*') {
            i += 2;
            while (text[i] && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (text[i]) i += 2;
            continue;
        }

        if (c == '&') {
            if (text[i + 1] == '=') { opId = GLSL_BIT_AND_EQ; opLen = 2; }
            else if (text[i + 1] != '&') { opId = GLSL_BIT_AND; opLen = 1; }
        } else if (c == '|') {
            if (text[i + 1] == '=') { opId = GLSL_BIT_OR_EQ; opLen = 2; }
            else if (text[i + 1] != '|') { opId = GLSL_BIT_OR; opLen = 1; }
        } else if (c == '^') {
            if (text[i + 1] == '=') { opId = GLSL_BIT_XOR_EQ; opLen = 2; }
            else if (text[i + 1] != '^') { opId = GLSL_BIT_XOR; opLen = 1; }
        } else if (c == '~') {
            opId = GLSL_BIT_NOT; opLen = 1;
        } else if (c == '<' && text[i + 1] == '<') {
            if (text[i + 2] == '=') { opId = GLSL_BIT_SHL_EQ; opLen = 3; }
            else { opId = GLSL_BIT_SHL; opLen = 2; }
        } else if (c == '>' && text[i + 1] == '>') {
            if (text[i + 2] == '=') { opId = GLSL_BIT_SHR_EQ; opLen = 3; }
            else { opId = GLSL_BIT_SHR; opLen = 2; }
        }

        if (opId) {
            newPos = glslBitProcess(&ctx, text, textSize, i, opId, opLen);
            if (newPos > i) { i = newPos; continue; }
            if (!ctx.failed && !glslBitNearUintLiteral(text, i)) {
                ctx.failed = TRUE;
                if (!col->warnedFallback) {
                    col->warnedFallback = TRUE;
                    gldDiagLog("GLSL->HLSL: a bitwise expression was not "
                               "lowered (complex or vector operand); leaving "
                               "it for D3DCompile to report.");
                }
            }
        }
        i++;
    }
}

/* A u/U suffix on an integer literal marks a uint-typed expression.  SM3
 * compiles uint bitwise operators natively and exactly, so an expression the
 * lowering left alone because of such a literal needs no warning: D3DCompile
 * will not report anything and the native result is correct. */
static BOOL glslBitNearUintLiteral(const char *text, int pos)
{
    int lo = pos - 64, hi = pos + 64;
    int j;

    if (lo < 0) lo = 0;
    for (j = lo; text[j] && j < hi; j++) {
        if (text[j] == 'u' || text[j] == 'U') {
            if (j > lo && text[j - 1] >= '0' && text[j - 1] <= '9')
                return TRUE;
        }
    }
    return FALSE;
}

/* glslBitEmitHelpers ??? append exactly the helper functions the rewritten
 * text calls to the helpers buffer.  All are float-domain bit arithmetic:
 * the general forms handle any operand below 2^24, the constant-specialized
 * forms only test the bits their constant sets. */

static void glslBitEmitHelpers(glslBitCollector *col, char *helpers, int helpersSize)
{
    char tmp[16384];
    int n = 0;
    int i;

    if (col->hasGeneralAnd || col->hasGeneralOr || col->hasGeneralXor) {
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_bitand(float a, float b)\n"
            "{\n"
            "    float r = 0.0;\n");
        for (i = 0; i < 24; i++) {
            unsigned long long p1 = 1ULL << i;
            unsigned long long p2 = 1ULL << (i + 1);
            n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
                "    r += %llu.0 * (floor(a / %llu.0) - 2.0 * floor(a / %llu.0)) * "
                "(floor(b / %llu.0) - 2.0 * floor(b / %llu.0));\n",
                p1, p1, p2, p1, p2);
        }
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "    return r;\n"
            "}\n");
        if (col->hasGeneralOr) {
            n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
                "float _glsl_bitor(float a, float b)\n"
                "{\n"
                "    return a + b - _glsl_bitand(a, b);\n"
                "}\n");
        }
        if (col->hasGeneralXor) {
            n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
                "float _glsl_bitxor(float a, float b)\n"
                "{\n"
                "    return a + b - 2.0 * _glsl_bitand(a, b);\n"
                "}\n");
        }
    }
    if (col->hasGeneralShl) {
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_shl(float a, float b)\n"
            "{\n"
            "    return a * pow(2.0, min(b, 24.0));\n"
            "}\n");
    }
    if (col->hasGeneralShr) {
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_shr(float a, float b)\n"
            "{\n"
            "    return floor(a / pow(2.0, min(b, 24.0)));\n"
            "}\n");
    }

    for (i = 0; i < col->andCount; i++) {
        unsigned int v = col->andConst[i];
        char hex[16];
        int b;
        sprintf(hex, "%06X", v);
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_and_%s(float a)\n"
            "{\n"
            "    float r = 0.0;\n", hex);
        for (b = 0; b < 24; b++) {
            if (!((v >> b) & 1u)) continue;
            n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
                "    r += %llu.0 * (floor(a / %llu.0) - 2.0 * floor(a / %llu.0));\n",
                1ULL << b, 1ULL << b, 1ULL << (b + 1));
        }
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n, "    return r;\n}\n");
    }
    for (i = 0; i < col->orCount; i++) {
        char hex[16];
        sprintf(hex, "%06X", col->orConst[i]);
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_or_%s(float a)\n"
            "{\n"
            "    return a + %u.0 - _glsl_and_%s(a);\n"
            "}\n", hex, col->orConst[i], hex);
    }
    for (i = 0; i < col->xorCount; i++) {
        char hex[16];
        sprintf(hex, "%06X", col->xorConst[i]);
        n += glslBitSprintf(tmp + n, (int)sizeof(tmp) - n,
            "float _glsl_xor_%s(float a)\n"
            "{\n"
            "    return a + %u.0 - 2.0 * _glsl_and_%s(a);\n"
            "}\n", hex, col->xorConst[i], hex);
    }

    if (n <= 0) return;

    {
        int hLen = (int)strlen(helpers);
        if (n + hLen + 2 >= helpersSize) {
            gldDiagLog("GLSL->HLSL: bitwise helper functions exceed the helper "
                       "buffer; the shader will fail at D3DCompile.");
            return;
        }
        /* The generated helpers must precede the shader's own functions:
         * D3DCompile on SM3 profiles cannot resolve a call to a function
         * defined later in the file (X3004), and the user's helper functions
         * are exactly what now calls them.  Prepend, keeping the buffer's
         * own functions after. */
        memmove(helpers + n + 1, helpers, hLen + 1);
        memcpy(helpers, tmp, n);
        helpers[n] = '\n';
        helpers[n + hLen + 1] = '\0';
    }
}

/*
 * glslDetectUnlowerableConstructs ??? name the GLSL constructs that have no
 * Shader Model 3 lowering at all, once each, before the compiler gets to them.
 *
 * This changes nothing about what compiles.  Transpilation and D3DCompile run
 * exactly as they would have, and a shader using any of these still fails with
 * the compiler's own message.  What it adds is a line that names the actual
 * construct, because the compiler's message describes the *rewritten HLSL*, and
 * by then the reason has usually turned into a syntax error some distance from
 * the GLSL that caused it.
 *
 * Bitwise operators used to head this list: D3DCompile rejects &, |, ^, ~,
 * << and >> on every SM3 profile (error X3535).  They are lowered now, by
 * glslRewriteBitwise, into float-domain bit arithmetic that is exact below
 * the 2^24 ceiling.  What is *not* lowered ??? vector operands (the helper
 * functions are scalar, so the rewritten code fails D3DCompile with a type
 * error) and compound assignments with side-effecting lvalues (expanded
 * textually, evaluating the lvalue twice) ??? is a narrow enough gap that the
 * compiler's own message still names the spot.  (Plain int/uint arithmetic is
 * fine and needs nothing: D3DCompile lowers +, -, *, / and % to float
 * operations itself.)
 *
 * The list is not exhaustive and is not meant to be.  Anything it misses still
 * produces D3DCompile's own error text, exactly as before.
 */
static BOOL glslHasWord(const char *src, const char *word)
{
    int len = (int)strlen(word);
    const char *p = src;
    while ((p = strstr(p, word)) != NULL) {
        BOOL leftOk  = (p == src) || glslIsWordBoundary(*(p - 1));
        if (leftOk && glslIsWordBoundary(p[len]))
            return TRUE;
        p++;
    }
    return FALSE;
}

static BOOL glslHasWordPrefix(const char *src, const char *prefix)
{
    int len = (int)strlen(prefix);
    const char *p = src;
    while ((p = strstr(p, prefix)) != NULL) {
        if ((p == src) || glslIsWordBoundary(*(p - 1)))
            return TRUE;
        p++;
    }
    return FALSE;
}

static void glslDetectUnlowerableConstructs(const char *src)
{
    static BOOL warnedBuffer  = FALSE;
    static BOOL warnedImage   = FALSE;
    static BOOL warnedAtomic  = FALSE;
    static BOOL warnedStage   = FALSE;

    static const char *const kImageTypes[] = {
        "image1D", "image2D", "image3D", "imageCube",
        "iimage1D", "iimage2D", "iimage3D", "iimageCube",
        "uimage1D", "uimage2D", "uimage3D", "uimageCube",
        NULL
    };
    static const char *const kStageOnly[] = {
        "gl_in", "gl_out", "gl_TessCoord", "gl_InvocationID", "gl_PrimitiveIDIn",
        NULL
    };
    int i;

    if (!src) return;

    if (!warnedBuffer && glslHasWord(src, "buffer")) {
        warnedBuffer = TRUE;
        gldDiagLog("GLSL->HLSL: shader uses the 'buffer' storage qualifier (shader "
                   "storage block). D3D9 has no shader storage buffers and no "
                   "lowering exists.");
    }

    if (!warnedImage) {
        for (i = 0; kImageTypes[i]; i++) {
            if (glslHasWord(src, kImageTypes[i])) {
                warnedImage = TRUE;
                gldDiagLog("GLSL->HLSL: shader declares '%s' (image load/store). "
                           "D3D9 has no read-write images and no lowering exists.",
                           kImageTypes[i]);
                break;
            }
        }
    }

    if (!warnedAtomic && glslHasWordPrefix(src, "atomic")) {
        warnedAtomic = TRUE;
        gldDiagLog("GLSL->HLSL: shader calls an atomic* builtin. D3D9 shaders have "
                   "no atomic operations and no lowering exists.");
    }

    if (!warnedStage) {
        for (i = 0; kStageOnly[i]; i++) {
            if (glslHasWord(src, kStageOnly[i])) {
                warnedStage = TRUE;
                gldDiagLog("GLSL->HLSL: shader uses '%s', which only exists in a "
                           "geometry or tessellation stage. D3D9 has neither stage "
                           "and no lowering exists.", kStageOnly[i]);
                break;
            }
        }
    }
}

/*
 * glslDoTranspile ??? the main transpilation pipeline.
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
/* Debug aid: in verbose mode, dump the single source line containing the
 * first "ambientLight" mention.  Used to find which pipeline pass deletes the
 * head of a multi-line `vec3 x = vec3(0.0) + ...` initializer: one line per
 * stage per shader, and the stage where the head vanishes is the culprit. */
static BOOL glslDoTranspile(int shaderType, const char *glslSource,
                            const glslAttributeBinding *bindings,
                            int bindingCount,
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
    glslFragOutput fragOutputs[GLSL_MAX_FRAGDATA + 1];
    int fragOutputCount = 0;
    glslTexDimSet texDim;
    char matNames[GLSL_MAX_VARS][GLSL_MAX_NAME_LEN];
    int matCount = 0;
    char fnNames[GLSL_MAX_VARS][GLSL_MAX_NAME_LEN];
    int fnCount = 0;
    glslDefine defines[GLSL_MAX_DEFINES];
    int defineCount = 0;
    glslObjectDefine objectDefines[GLSL_MAX_DEFINES];
    int objectDefineCount = 0;
    BOOL unresolvedArray = FALSE;
    int idx;
    glslBitCollector bitCol;

    texDim.count = 0;

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

    /* Rename GLSL identifiers that HLSL reserves, before anything else looks
     * at the source.  It has to be here rather than on mainBody/helpers later:
     * the declaration parse, the VS_INPUT field emission and the uniform
     * globals all come off this same buffer, and renaming only the bodies
     * would leave the declaration under its original, illegal spelling and
     * turn one X3000 into a pile of X3004s. */
    glslRenameReservedWords(workBuf);

    /* Detect built-in usage before any transformations */
    if (shaderType == GLSL_SHADER_PIXEL) {
        usesFragData = glslDetectFragDataUsage(workBuf, &maxFragData);
        usesFragCoord = (strstr(workBuf, "gl_FragCoord") != NULL);
        usesFrontFacing = (strstr(workBuf, "gl_FrontFacing") != NULL);
        fragOutputCount = glslCollectFragmentOutputs(
            workBuf, fragOutputs, GLSL_MAX_FRAGDATA + 1);
        for (idx = 0; idx < fragOutputCount; ++idx) {
            int required = fragOutputs[idx].location + 1;
            if (required > GLSL_MAX_FRAGDATA) {
                gldDiagLog("GLSL->HLSL: fragment output '%s' uses color location %d, "
                           "but D3D9 exposes only %d simultaneous color targets",
                           fragOutputs[idx].name, fragOutputs[idx].location,
                           GLSL_MAX_FRAGDATA);
                free(workBuf);
                free(mainBody);
                free(helpers);
                return FALSE;
            }
            if (required > maxFragData) maxFragData = required;
        }
        if (maxFragData > 1) usesFragData = TRUE;
    }

    /* Integer #defines, read from the untouched source: an array uniform is
     * as likely to be sized by a macro as by a literal. */
    defineCount = glslCollectDefines(glslSource, defines, GLSL_MAX_DEFINES);
    objectDefineCount = glslCollectObjectDefines(
        glslSource, objectDefines, GLSL_MAX_DEFINES);

    /* Parse declarations from original source (before stripping) */
    glslParseDeclarations(workBuf, shaderType,
                          attributes, &attrCount,
                          varyings, &varyCount,
                          uniforms, &unifCount,
                          defines, defineCount, &unresolvedArray);

    /* glBindAttribLocation is a link-time input. Apply it after parsing so
     * the HLSL input declaration is compiled with the semantic belonging to
     * the application's real array index. */
    if (shaderType == GLSL_SHADER_VERTEX && bindings && bindingCount > 0) {
        int a, b;
        for (a = 0; a < attrCount; ++a) {
            for (b = bindingCount - 1; b >= 0; --b) {
                if (bindings[b].name &&
                    strcmp(attributes[a].name, bindings[b].name) == 0) {
                    attributes[a].location = bindings[b].location;
                    break;
                }
            }
        }
    }

    /* An array whose extent could not be resolved stops here.  Carrying on
     * would emit the uniform as a scalar, which D3DCompile *accepts* ??? it
     * reads foo[0..3] on a scalar float4 as a component swizzle ??? so the
     * shader would compile and then read the wrong values for every element
     * with nothing to show for it.  glslParseDeclarations has already named
     * the offending declaration in the diagnostic log. */
    if (unresolvedArray) {
        free(workBuf);
        free(mainBody);
        free(helpers);
        return FALSE;
    }

    gldLogPrintf(GLDLOG_DEBUG, "glslDoTranspile: Found %d attributes, %d varyings, %d uniforms\n",
                 attrCount, varyCount, unifCount);

    /* Strip preprocessor directives and qualifiers */
    glslStripVersionLines(workBuf);
    glslStripPrecisionQualifiers(workBuf);
    glslStripLayoutQualifiers(workBuf, NULL, NULL);

    /* Join continuation lines: GLSL allows newlines inside expressions, so
     * a multi-line statement like
     *   vec3 ambientLight = vec3( 0.0 )
     *   + max( ... )
     *   + max( ... );
     * has its head line blanked by glslRemoveDeclarationLines, which only
     * blanks complete lines that match a declaration pattern.  Collapsing
     * all continuation lines onto the head makes every statement occupy a
     * single line, so no pass can partially delete it. */
    glslJoinContinuationLines(workBuf);

    /* Remove declaration lines */
    glslRemoveDeclarationLines(workBuf);

    /* Extract helper functions (before main) */
    glslExtractHelperFunctions(workBuf, helpers, HLSL_MAX_OUTPUT / 4,
                               fnNames, &fnCount, GLSL_MAX_VARS);

    /* Extract main() body */
    glslExtractMainBody(workBuf, mainBody, HLSL_MAX_OUTPUT / 2);
    if (mainBody[0] == '\0') {
        gldLogPrintf(GLDLOG_ERROR, "glslDoTranspile: Could not find void main() in GLSL source\n");
        free(workBuf);
        free(mainBody);
        free(helpers);
        return FALSE;
    }

    /* #define lines were blanked with the other preprocessor directives.
     * Substitute the safe scalar subset into executable text before any
     * lowering pass needs to understand the surrounding expression. */
    glslApplyObjectDefines(mainBody, objectDefines, objectDefineCount);
    glslApplyObjectDefines(helpers, objectDefines, objectDefineCount);

    /* Move any user-defined function whose name is also a GLSL or HLSL builtin
     * out of the way, before a single rewrite pass has looked at the body.
     *
     * Order is the whole point.  Run afterwards, the function-name rewrites
     * would have already mangled the user function's own definition ??? a
     * "vec4 textureLod(sampler2D s, vec2 uv, float lod) {" header does not
     * parse as a 3-argument call, so glslApplyTextureLodRewrite takes its
     * fallback branch and renames the definition to tex2Dlod, shadowing the
     * intrinsic its own body then calls.  Renaming first leaves the wrapper
     * intact under a name nothing else matches, while the genuine builtin
     * calls inside it are still lowered normally. */
    for (idx = 0; idx < fnCount; idx++) {
        char renamed[GLSL_MAX_NAME_LEN + 16];

        if (!glslNameCollidesWithBuiltin(fnNames[idx]))
            continue;

        sprintf(renamed, "_glsl_userfn_%s", fnNames[idx]);
        /* Per-shader detail, one line per collision: gated on verbose like
         * every other per-call diagnostic, because a title whose shaders
         * routinely shadow a builtin would otherwise write this same line
         * for every shader it compiles. */
        gldDiagLogV("GLSL->HLSL: shader defines its own '%s', which is also a "
                    "builtin this translation rewrites; renamed to '%s' so the "
                    "definition survives and the builtin stays reachable.",
                    fnNames[idx], renamed);
        glslReplaceWord(mainBody, fnNames[idx], renamed);
        glslReplaceWord(helpers,  fnNames[idx], renamed);
    }


    /* Step 4.5: Lower bitwise operators to Shader Model 3 arithmetic.
     * Runs on the GLSL spelling, before the type and function replacement
     * passes below can touch the text: &, |, ^, ~, << and >> become
     * float-domain bit arithmetic, exact below the 2^24 ceiling, with
     * constant-specialized helpers for the common masks.  The generated
     * helper functions are appended to the helpers buffer by
     * glslBitEmitHelpers after the shader is built, so no later rewrite
     * pass can touch them. */
    memset(&bitCol, 0, sizeof(bitCol));
    glslRewriteBitwise(mainBody, HLSL_MAX_OUTPUT / 2, &bitCol);
    glslRewriteBitwise(helpers, HLSL_MAX_OUTPUT / 4, &bitCol);

    /* Step 5: Apply type replacements to main body and helpers.
     * The splat rewrite has to run first: it works on the GLSL spelling and
     * relies on the rename that follows to turn the cast it leaves behind
     * ((vec4)(x)) into HLSL ((float4)(x)). */
    glslRewriteVectorSplats(mainBody, HLSL_MAX_OUTPUT / 2);
    glslRewriteVectorSplats(helpers, HLSL_MAX_OUTPUT / 4);

    /* The two matrix rewrites also work on the GLSL spelling, and both run
     * before the rename for the same reason.  The symbol set is collected from
     * the original source, which still holds the declarations the body's
     * matrix names come from ??? glslRemoveDeclarationLines has blanked them out
     * of workBuf by now.  Diagonal constructors go first so that a
     * "mat4(1.0) * v" is already a recognisable matrix constructor by the time
     * the product rewrite looks at it. */
    matCount = glslCollectMatrixSymbols(glslSource, matNames, GLSL_MAX_VARS);
    if (strstr(glslSource, "gl_ModelViewProjectionMatrix") && matCount < GLSL_MAX_VARS)
        strcpy(matNames[matCount++], "gl_ModelViewProjectionMatrix");
    if (strstr(glslSource, "gl_ModelViewMatrix") && matCount < GLSL_MAX_VARS)
        strcpy(matNames[matCount++], "gl_ModelViewMatrix");
    if (strstr(glslSource, "gl_ProjectionMatrix") && matCount < GLSL_MAX_VARS)
        strcpy(matNames[matCount++], "gl_ProjectionMatrix");

    /* The symbol set comes off the original source, which still spells a
     * matrix named after an HLSL reserved word ("mat4 matrix;") the way the
     * author wrote it, while mainBody/helpers are already in the renamed
     * spelling.  Bring the names across, or the two rewrites below would look
     * for a name that is no longer there. */
    for (idx = 0; idx < matCount; idx++) {
        const char *kw = glslReservedWordRenameOf(matNames[idx]);
        if (kw) {
            strncpy(matNames[idx], kw, GLSL_MAX_NAME_LEN - 1);
            matNames[idx][GLSL_MAX_NAME_LEN - 1] = '\0';
        }
    }

    glslRewriteMatrixDiagonal(mainBody, HLSL_MAX_OUTPUT / 2, matNames, matCount);
    glslRewriteMatrixDiagonal(helpers,  HLSL_MAX_OUTPUT / 4, matNames, matCount);
    glslRewriteMatrixProducts(mainBody, HLSL_MAX_OUTPUT / 2, matNames, matCount);
    glslRewriteMatrixProducts(helpers,  HLSL_MAX_OUTPUT / 4, matNames, matCount);

    glslApplyTypeReplacements(mainBody);
    glslApplyTypeReplacements(helpers);

    /* Step 6: Apply function replacements */
    glslApplyFunctionReplacements(mainBody, HLSL_MAX_OUTPUT / 2, &texDim, uniforms, unifCount);
    glslApplyFunctionReplacements(helpers, HLSL_MAX_OUTPUT / 4, &texDim, uniforms, unifCount);

    /* idTech's tex2Ddepth helper is used from data-dependent SSR loops.  An
     * implicit-gradient tex2D in such a loop makes the SM3 compiler attempt to
     * unroll an unbounded iteration count and reject the shader (X3570).  A
     * depth lookup is intentionally mip zero, and ps_3_0 permits tex2Dlod in
     * dynamic flow, so make that semantic explicit without changing ordinary
     * colour texture sampling elsewhere in the shader. */
    if (shaderType == GLSL_SHADER_PIXEL && strstr(helpers, "tex2Ddepth")) {
        glslReplaceAll(helpers,
            "return _glsl_userfn_tex2D( image, texcoord ).x;",
            "return tex2Dlod( image, float4( texcoord.x, texcoord.y, 0.0, 0.0 ) ).x;");
    }

    /* Step 7: Replace built-in variables */
    glslReplaceBuiltinVars(mainBody, shaderType);

    /* Named GLSL 1.30+ fragment outputs are local aliases for the same D3D9
     * COLOR registers used by gl_FragColor/gl_FragData.  Perform this after
     * extracting main so declaration text can never be rewritten into an
     * invalid HLSL declaration. */
    if (shaderType == GLSL_SHADER_PIXEL) {
        for (idx = 0; idx < fragOutputCount; ++idx) {
            char target[64];
            if (maxFragData <= 1)
                strcpy(target, "_glsl_fragColor");
            else
                sprintf(target, "_glsl_fragData%d", fragOutputs[idx].location);
            glslReplaceWord(mainBody, fragOutputs[idx].name, target);
        }
    }

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
                              mainBody, hlslOut, hlslBufSize,
                              &texDim);
    } else {
        glslBuildPixelShader(varyings, varyCount,
                             uniforms, unifCount,
                             mainBody, hlslOut, hlslBufSize,
                             usesFragData, maxFragData,
                             usesFragCoord, usesFrontFacing,
                             &texDim);
    }

    /* Append the bitwise helper functions, if the rewrite needed any. */
    glslBitEmitHelpers(&bitCol, helpers, HLSL_MAX_OUTPUT / 4);

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
