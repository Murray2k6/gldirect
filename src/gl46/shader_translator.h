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
* Description:  DX9 shader bytecode to GLSL 4.60 translation.
*               Parses DX9 Shader Model 2.0/3.0 bytecode token streams,
*               translates arithmetic, texture, and flow-control instructions
*               to GLSL 4.60, compiles/links shader programs, and caches
*               linked programs keyed by bytecode hash.
*
*********************************************************************************/

#ifndef __GL46_SHADER_TRANSLATOR_H
#define __GL46_SHADER_TRANSLATOR_H

#include <windows.h>
#include <glad/gl.h>

/*---------------------- Macros and type definitions ----------------------*/

/* Maximum limits for parsed shader structures */
#define GLD_MAX_SHADER_INSTRUCTIONS     512
#define GLD_MAX_SHADER_DECLARATIONS     64
#define GLD_MAX_SHADER_CONSTANTS        256
#define GLD_MAX_SHADER_SRC_REGS         4
#define GLD_MAX_SHADER_TEMP_REGS        32
#define GLD_MAX_SHADER_SAMPLER_REGS     16

/* Maximum GLSL output buffer size */
#define GLD_MAX_GLSL_SOURCE             (64 * 1024)

/* Shader program cache size */
#define GLD_SHADER_CACHE_SIZE           256

/*
 * DX9 shader bytecode version token constants.
 * The version token is the first DWORD in the bytecode stream.
 * Bits [7:0]   = minor version
 * Bits [15:8]  = major version
 * Bits [31:16] = shader type (0xFFFE = vertex, 0xFFFF = pixel)
 */
#define GLD_VS_TYPE_MASK                0xFFFE0000
#define GLD_PS_TYPE_MASK                0xFFFF0000
#define GLD_SHADER_VERSION_MAJOR(tok)   (((tok) >> 8) & 0xFF)
#define GLD_SHADER_VERSION_MINOR(tok)   ((tok) & 0xFF)
#define GLD_SHADER_IS_VERTEX(tok)       (((tok) & 0xFFFF0000) == 0xFFFE0000)
#define GLD_SHADER_IS_PIXEL(tok)        (((tok) & 0xFFFF0000) == 0xFFFF0000)

/*
 * DX9 shader instruction opcodes (D3DSIO_* subset).
 * Only the most common SM2.0/3.0 opcodes are listed here.
 */
#define D3DSIO_NOP          0x0000
#define D3DSIO_MOV          0x0001
#define D3DSIO_ADD          0x0002
#define D3DSIO_SUB          0x0003
#define D3DSIO_MAD          0x0004
#define D3DSIO_MUL          0x0005
#define D3DSIO_RCP          0x0006
#define D3DSIO_RSQ          0x0007
#define D3DSIO_DP3          0x0008
#define D3DSIO_DP4          0x0009
#define D3DSIO_MIN          0x000A
#define D3DSIO_MAX          0x000B
#define D3DSIO_SLT          0x000C
#define D3DSIO_SGE          0x000D
#define D3DSIO_EXP          0x000E
#define D3DSIO_LOG          0x000F
#define D3DSIO_LIT          0x0010
#define D3DSIO_DST          0x0011
#define D3DSIO_LRP          0x0012
#define D3DSIO_FRC          0x0013
#define D3DSIO_POW          0x0019
#define D3DSIO_CRS          0x0021
#define D3DSIO_SGN          0x0022
#define D3DSIO_ABS          0x0023
#define D3DSIO_NRM          0x0024
#define D3DSIO_SINCOS       0x0025
#define D3DSIO_REP          0x0026
#define D3DSIO_ENDREP       0x0027
#define D3DSIO_IF           0x0028
#define D3DSIO_IFC          0x0029
#define D3DSIO_ELSE         0x002A
#define D3DSIO_ENDIF        0x002B
#define D3DSIO_BREAK        0x002C
#define D3DSIO_BREAKC       0x002D
#define D3DSIO_MOVA         0x002E
#define D3DSIO_LOOP         0x001B
#define D3DSIO_ENDLOOP      0x001D
#define D3DSIO_CALL         0x0018
#define D3DSIO_RET          0x001C
#define D3DSIO_CMP          0x0058
#define D3DSIO_DP2ADD       0x005A
#define D3DSIO_TEX          0x0042
#define D3DSIO_TEXLDL       0x005F
#define D3DSIO_TEXLDB       0x0060  /* texture with bias */
#define D3DSIO_TEXKILL      0x0041
#define D3DSIO_DCL          0x001F
#define D3DSIO_DEF          0x0051
#define D3DSIO_DEFI         0x0034
#define D3DSIO_DEFB         0x0035
#define D3DSIO_END          0xFFFF

/*
 * DX9 shader register types.
 * Register type is encoded across two bit fields in the token:
 *   bits [30:28] = type low (3 bits)
 *   bits [12:11] = type high (2 bits)
 *   Full type = (typeHigh << 3) | typeLow
 */
#define GLD_REG_TEMP            0   /* Temporary register (r#) */
#define GLD_REG_INPUT           1   /* Input register (v#) */
#define GLD_REG_CONST           2   /* Float constant register (c#) */
#define GLD_REG_ADDR            3   /* Address register (a0) / loop counter */
#define GLD_REG_TEXTURE         3   /* Texture register (t#) — PS only, same as ADDR */
#define GLD_REG_RASTOUT         4   /* Rasterizer output (oPos, oFog, oPts) */
#define GLD_REG_ATTROUT         5   /* Attribute output (oD#) */
#define GLD_REG_OUTPUT          6   /* Output register (o#) — SM3.0 */
#define GLD_REG_CONSTINT        7   /* Integer constant register (i#) */
#define GLD_REG_COLOROUT        8   /* Color output (oC#) — PS */
#define GLD_REG_DEPTHOUT        9   /* Depth output (oDepth) — PS */
#define GLD_REG_SAMPLER         10  /* Sampler register (s#) */
#define GLD_REG_CONSTBOOL       11  /* Boolean constant register (b#) */
#define GLD_REG_LOOP            12  /* Loop counter register (aL) */
#define GLD_REG_TEMPFLOAT16     13  /* 16-bit float temp */
#define GLD_REG_MISCTYPE        14  /* Misc type (vPos, vFace) */
#define GLD_REG_LABEL           15  /* Label for call/ret */
#define GLD_REG_PREDICATE       16  /* Predicate register (p0) */

/*
 * DX9 shader source modifiers.
 * Encoded in bits [27:24] of the source register token.
 */
#define GLD_SRCMOD_NONE         0x0
#define GLD_SRCMOD_NEGATE       0x1
#define GLD_SRCMOD_BIAS         0x2
#define GLD_SRCMOD_BIASNEGATE   0x3
#define GLD_SRCMOD_SIGN         0x4
#define GLD_SRCMOD_SIGNNEGATE   0x5
#define GLD_SRCMOD_COMP         0x6
#define GLD_SRCMOD_X2           0x7
#define GLD_SRCMOD_X2NEGATE     0x8
#define GLD_SRCMOD_DZ           0x9
#define GLD_SRCMOD_DW           0xA
#define GLD_SRCMOD_ABS          0xB
#define GLD_SRCMOD_ABSNEGATE    0xC
#define GLD_SRCMOD_NOT          0xD

/*
 * DX9 shader result/instruction modifiers.
 * Encoded in bits [23:20] of the destination register token.
 */
#define GLD_DSTMOD_NONE         0x0
#define GLD_DSTMOD_SATURATE     0x1
#define GLD_DSTMOD_PARTIALPRECISION 0x2
#define GLD_DSTMOD_CENTROID     0x4

/*
 * DX9 DCL usage values (for D3DSIO_DCL).
 */
#define GLD_DCLUSAGE_POSITION       0
#define GLD_DCLUSAGE_BLENDWEIGHT    1
#define GLD_DCLUSAGE_BLENDINDICES   2
#define GLD_DCLUSAGE_NORMAL         3
#define GLD_DCLUSAGE_PSIZE          4
#define GLD_DCLUSAGE_TEXCOORD       5
#define GLD_DCLUSAGE_TANGENT        6
#define GLD_DCLUSAGE_BINORMAL       7
#define GLD_DCLUSAGE_TESSFACTOR     8
#define GLD_DCLUSAGE_POSITIONT      9
#define GLD_DCLUSAGE_COLOR          10
#define GLD_DCLUSAGE_FOG            11
#define GLD_DCLUSAGE_DEPTH          12
#define GLD_DCLUSAGE_SAMPLE         13

/*
 * Sampler type values (from DCL sampler token).
 */
#define GLD_SAMPLER_2D              2
#define GLD_SAMPLER_CUBE            3
#define GLD_SAMPLER_VOLUME          4

/*
 * GLD_shaderRegister — describes a single source or destination register
 * reference within a shader instruction.
 */
typedef struct {
    int     regType;        /* GLD_REG_* register type */
    int     regIndex;       /* Register number */
    int     writeMask;      /* Destination write mask (bits 0-3 = xyzw) */
    int     swizzle;        /* Source swizzle (8 bits: 2 per component) */
    int     srcModifier;    /* Source modifier (GLD_SRCMOD_*) */
    int     dstModifier;    /* Destination modifier (GLD_DSTMOD_*) */
} GLD_shaderRegister;

/*
 * GLD_shaderInstruction — a single parsed shader instruction.
 */
typedef struct {
    int                 opcode;         /* D3DSIO_* opcode */
    GLD_shaderRegister  destReg;        /* Destination register */
    GLD_shaderRegister  srcRegs[GLD_MAX_SHADER_SRC_REGS]; /* Source registers */
    int                 srcCount;       /* Number of source registers */
    BOOL                bCoIssue;       /* Co-issue flag (PS) */
    BOOL                bPredicated;    /* Predicated instruction */
    int                 comparison;     /* Comparison type for IFC/BREAKC */
} GLD_shaderInstruction;

/*
 * GLD_shaderDecl — a shader input/output declaration (from D3DSIO_DCL).
 */
typedef struct {
    int     usage;          /* GLD_DCLUSAGE_* semantic usage */
    int     usageIndex;     /* Semantic usage index (e.g. TEXCOORD0 vs TEXCOORD1) */
    int     regType;        /* Register type (input, output, sampler) */
    int     regIndex;       /* Register number */
    int     samplerType;    /* Sampler type (GLD_SAMPLER_*) — only for sampler DCLs */
} GLD_shaderDecl;

/*
 * GLD_shaderConstDef — a constant definition (from D3DSIO_DEF/DEFI/DEFB).
 */
typedef struct {
    int     regIndex;       /* Constant register index */
    int     type;           /* 0=float, 1=int, 2=bool */
    union {
        float   f[4];       /* Float constant (DEF) */
        int     i[4];       /* Integer constant (DEFI) */
        BOOL    b;          /* Boolean constant (DEFB) */
    } value;
} GLD_shaderConstDef;

/*
 * GLD_parsedShader — the complete result of parsing a DX9 shader
 * bytecode stream.
 */
typedef struct {
    BOOL                    isVertexShader;     /* TRUE=VS, FALSE=PS */
    int                     majorVersion;       /* Shader model major version */
    int                     minorVersion;       /* Shader model minor version */

    GLD_shaderInstruction   instructions[GLD_MAX_SHADER_INSTRUCTIONS];
    int                     instructionCount;

    GLD_shaderDecl          declarations[GLD_MAX_SHADER_DECLARATIONS];
    int                     declCount;

    GLD_shaderConstDef      constantDefs[GLD_MAX_SHADER_CONSTANTS];
    int                     constDefCount;
} GLD_parsedShader;

/*
 * GLD_shaderCacheEntry — a single entry in the shader program cache.
 */
typedef struct {
    DWORD   hash;           /* FNV-1a hash of combined VS+PS bytecode */
    GLuint  program;        /* Linked GL program object */
    BOOL    bUsed;          /* TRUE if this slot is occupied */
} GLD_shaderCacheEntry;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Parse a DX9 shader bytecode token stream into a GLD_parsedShader.
 *
 * Reads the version token, iterates instruction tokens, parses
 * destination/source register tokens, handles DCL/DEF/DEFI/DEFB
 * special instructions, and terminates on D3DSIO_END.
 *
 * Parameters:
 *   bytecode      — pointer to the DWORD bytecode token array.
 *   bytecodeSize  — size of the bytecode in bytes.
 *   outShader     — receives the parsed shader data.
 *
 * Returns:
 *   TRUE on success, FALSE on parse error.
 */
BOOL gldParseShaderBytecode(const DWORD *bytecode, int bytecodeSize,
                            GLD_parsedShader *outShader);

/*
 * Generate GLSL 4.60 source code from a parsed DX9 shader.
 *
 * Emits #version 460 core, translates declarations to in/out/uniform
 * variables, translates instructions to GLSL operations, handles
 * write masks, swizzles, source modifiers, saturate, flow control,
 * and texture sampling.
 *
 * Parameters:
 *   shader      — the parsed shader to translate.
 *   glslBuffer  — output buffer for the GLSL source string.
 *   bufferSize  — size of the output buffer in bytes.
 *
 * Returns:
 *   TRUE on success, FALSE if the buffer is too small or an
 *   unsupported instruction is encountered.
 */
BOOL gldGenerateGLSL(const GLD_parsedShader *shader,
                     char *glslBuffer, int bufferSize);

/*
 * Compile a GLSL shader source string.
 *
 * Calls glCreateShader, glShaderSource, glCompileShader, and checks
 * GL_COMPILE_STATUS.  On failure, retrieves and logs the info log.
 *
 * Parameters:
 *   shaderType  — GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 *   glslSource  — null-terminated GLSL source string.
 *
 * Returns:
 *   The GL shader object id on success, or 0 on failure.
 */
GLuint gldCompileShader46(GLenum shaderType, const char *glslSource);

/*
 * Link a vertex shader and fragment shader into a program.
 *
 * Calls glCreateProgram, glAttachShader (×2), glLinkProgram, and
 * checks GL_LINK_STATUS.  On failure, retrieves and logs the info log.
 *
 * Parameters:
 *   vertShader  — compiled vertex shader object id.
 *   fragShader  — compiled fragment shader object id.
 *
 * Returns:
 *   The GL program object id on success, or 0 on failure.
 */
GLuint gldLinkProgram46(GLuint vertShader, GLuint fragShader);

/*
 * Look up a cached shader program by bytecode hash.
 *
 * Computes an FNV-1a hash of the combined vertex and pixel shader
 * bytecode and searches the cache for a matching entry.
 *
 * Parameters:
 *   vsBytecode  — vertex shader bytecode (may be NULL).
 *   vsSize      — vertex shader bytecode size in bytes.
 *   psBytecode  — pixel shader bytecode (may be NULL).
 *   psSize      — pixel shader bytecode size in bytes.
 *
 * Returns:
 *   The cached GL program id, or 0 if not found.
 */
GLuint gldGetCachedProgram(const DWORD *vsBytecode, int vsSize,
                           const DWORD *psBytecode, int psSize);

/*
 * Store a linked program in the shader cache.
 *
 * Parameters:
 *   vsBytecode  — vertex shader bytecode (may be NULL).
 *   vsSize      — vertex shader bytecode size in bytes.
 *   psBytecode  — pixel shader bytecode (may be NULL).
 *   psSize      — pixel shader bytecode size in bytes.
 *   program     — the linked GL program object to cache.
 */
void gldCacheProgram(const DWORD *vsBytecode, int vsSize,
                     const DWORD *psBytecode, int psSize,
                     GLuint program);

/*
 * Clear the entire shader program cache, deleting all cached programs.
 */
void gldClearShaderCache(void);

#ifdef  __cplusplus
}
#endif

#endif
