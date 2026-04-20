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
*
*********************************************************************************/

#include "shader_translator.h"
#include "error_handler.h"
#include "gld_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/*---------------------- Internal helpers ----------------------*/

/* Shader program cache — simple linear array */
static GLD_shaderCacheEntry s_shaderCache[GLD_SHADER_CACHE_SIZE];
static BOOL s_cacheInitialized = FALSE;

/*
 * FNV-1a hash for arbitrary byte data.
 */
static DWORD gldFNV1aHash(const BYTE *data, int size)
{
    DWORD hash = 0x811C9DC5u; /* FNV offset basis */
    int i;
    for (i = 0; i < size; i++) {
        hash ^= (DWORD)data[i];
        hash *= 0x01000193u; /* FNV prime */
    }
    return hash;
}

/*
 * Compute a combined hash for VS + PS bytecode pair.
 */
static DWORD gldComputeShaderPairHash(const DWORD *vsBytecode, int vsSize,
                                      const DWORD *psBytecode, int psSize)
{
    DWORD hash = 0x811C9DC5u;
    int i;

    if (vsBytecode && vsSize > 0) {
        const BYTE *p = (const BYTE *)vsBytecode;
        for (i = 0; i < vsSize; i++) {
            hash ^= (DWORD)p[i];
            hash *= 0x01000193u;
        }
    }

    /* Separator to avoid collisions between VS-only and PS-only */
    hash ^= 0xFFu;
    hash *= 0x01000193u;

    if (psBytecode && psSize > 0) {
        const BYTE *p = (const BYTE *)psBytecode;
        for (i = 0; i < psSize; i++) {
            hash ^= (DWORD)p[i];
            hash *= 0x01000193u;
        }
    }

    return hash;
}

/*
 * Extract register type from a register token.
 * Type is split across bits [30:28] (low 3 bits) and [12:11] (high 2 bits).
 */
static int gldGetRegType(DWORD token)
{
    int typeLow  = (token >> 28) & 0x7;
    int typeHigh = (token >> 11) & 0x3;
    return (typeHigh << 3) | typeLow;
}

/*
 * Extract register index from a register token.
 * Bits [10:0] = register number.
 */
static int gldGetRegIndex(DWORD token)
{
    return (int)(token & 0x7FF);
}

/*
 * Extract write mask from a destination register token.
 * Bits [19:16] = write mask (bit 0=x, 1=y, 2=z, 3=w).
 */
static int gldGetWriteMask(DWORD token)
{
    return (int)((token >> 16) & 0xF);
}

/*
 * Extract destination modifier from a destination register token.
 * Bits [23:20] = result modifier.
 */
static int gldGetDstModifier(DWORD token)
{
    return (int)((token >> 20) & 0xF);
}

/*
 * Extract swizzle from a source register token.
 * Bits [23:16] = swizzle (2 bits per component: x, y, z, w).
 */
static int gldGetSwizzle(DWORD token)
{
    return (int)((token >> 16) & 0xFF);
}

/*
 * Extract source modifier from a source register token.
 * Bits [27:24] = source modifier.
 */
static int gldGetSrcModifier(DWORD token)
{
    return (int)((token >> 24) & 0xF);
}

/*
 * Parse a destination register token into a GLD_shaderRegister.
 */
static void gldParseDestToken(DWORD token, GLD_shaderRegister *reg)
{
    memset(reg, 0, sizeof(*reg));
    reg->regType     = gldGetRegType(token);
    reg->regIndex    = gldGetRegIndex(token);
    reg->writeMask   = gldGetWriteMask(token);
    reg->dstModifier = gldGetDstModifier(token);
    reg->swizzle     = 0xE4; /* identity swizzle (xyzw) */
    reg->srcModifier = GLD_SRCMOD_NONE;
}

/*
 * Parse a source register token into a GLD_shaderRegister.
 */
static void gldParseSrcToken(DWORD token, GLD_shaderRegister *reg)
{
    memset(reg, 0, sizeof(*reg));
    reg->regType     = gldGetRegType(token);
    reg->regIndex    = gldGetRegIndex(token);
    reg->writeMask   = 0xF; /* not applicable for source */
    reg->swizzle     = gldGetSwizzle(token);
    reg->srcModifier = gldGetSrcModifier(token);
    reg->dstModifier = GLD_DSTMOD_NONE;
}

/*
 * Get instruction length from the instruction token.
 * For SM2.0+, bits [27:24] encode the instruction length in DWORDs
 * (number of additional tokens after the opcode token).
 * For SM1.x, we use a lookup table.
 */
static int gldGetInstructionLength(DWORD token, int majorVersion)
{
    int opcode = token & 0xFFFF;

    /* SM2.0+ encodes length in bits [27:24] */
    if (majorVersion >= 2) {
        return (int)((token >> 24) & 0xF);
    }

    /* SM1.x fallback — common opcodes */
    switch (opcode) {
    case D3DSIO_NOP:     return 0;
    case D3DSIO_MOV:     return 2;  /* dest + src */
    case D3DSIO_ADD:     return 3;
    case D3DSIO_SUB:     return 3;
    case D3DSIO_MAD:     return 4;
    case D3DSIO_MUL:     return 3;
    case D3DSIO_RCP:     return 2;
    case D3DSIO_RSQ:     return 2;
    case D3DSIO_DP3:     return 3;
    case D3DSIO_DP4:     return 3;
    case D3DSIO_MIN:     return 3;
    case D3DSIO_MAX:     return 3;
    case D3DSIO_SLT:     return 3;
    case D3DSIO_SGE:     return 3;
    case D3DSIO_EXP:     return 2;
    case D3DSIO_LOG:     return 2;
    case D3DSIO_FRC:     return 2;
    case D3DSIO_ABS:     return 2;
    case D3DSIO_NRM:     return 2;
    case D3DSIO_SINCOS:  return 4;
    case D3DSIO_POW:     return 3;
    case D3DSIO_LRP:     return 4;
    case D3DSIO_CMP:     return 4;
    case D3DSIO_TEX:     return 2;  /* SM1.x tex is simpler */
    case D3DSIO_TEXKILL: return 1;
    case D3DSIO_DCL:     return 2;
    case D3DSIO_DEF:     return 5;
    case D3DSIO_DEFI:    return 5;
    case D3DSIO_DEFB:    return 2;
    default:             return 0;
    }
}


/*======================== Task 13.1: Bytecode Parser ========================*/

BOOL gldParseShaderBytecode(const DWORD *bytecode, int bytecodeSize,
                            GLD_parsedShader *outShader)
{
    int tokenCount;
    int pos;
    DWORD versionToken;

    if (!bytecode || bytecodeSize < 4 || !outShader) {
        gldLogPrintf(GLDLOG_ERROR, "gldParseShaderBytecode: invalid parameters");
        return FALSE;
    }

    memset(outShader, 0, sizeof(*outShader));
    tokenCount = bytecodeSize / (int)sizeof(DWORD);

    /* Read version token */
    versionToken = bytecode[0];
    outShader->majorVersion = GLD_SHADER_VERSION_MAJOR(versionToken);
    outShader->minorVersion = GLD_SHADER_VERSION_MINOR(versionToken);
    outShader->isVertexShader = GLD_SHADER_IS_VERTEX(versionToken);

    gldLogPrintf(GLDLOG_INFO,
        "gldParseShaderBytecode: %s shader version %d.%d",
        outShader->isVertexShader ? "vertex" : "pixel",
        outShader->majorVersion, outShader->minorVersion);

    pos = 1; /* Skip version token */

    while (pos < tokenCount) {
        DWORD instrToken = bytecode[pos];
        int opcode = instrToken & 0xFFFF;
        int instrLen;

        /* Check for end token */
        if (opcode == D3DSIO_END) {
            gldLogPrintf(GLDLOG_DEBUG, "gldParseShaderBytecode: END token at pos %d", pos);
            break;
        }

        instrLen = gldGetInstructionLength(instrToken, outShader->majorVersion);

        /* Handle D3DSIO_DCL — declaration */
        if (opcode == D3DSIO_DCL) {
            if (outShader->declCount < GLD_MAX_SHADER_DECLARATIONS && pos + instrLen < tokenCount) {
                GLD_shaderDecl *decl = &outShader->declarations[outShader->declCount];
                DWORD dclToken = bytecode[pos + 1]; /* DCL usage token */
                DWORD regToken = bytecode[pos + 2]; /* Register token */

                decl->regType  = gldGetRegType(regToken);
                decl->regIndex = gldGetRegIndex(regToken);

                if (decl->regType == GLD_REG_SAMPLER) {
                    /* Sampler declaration: bits [27:25] of dclToken = sampler type */
                    decl->samplerType = (dclToken >> 25) & 0x7;
                    decl->usage       = 0;
                    decl->usageIndex  = 0;
                } else {
                    /* Input/output declaration: bits [4:0] = usage, [19:16] = usage index */
                    decl->usage      = dclToken & 0x1F;
                    decl->usageIndex = (dclToken >> 16) & 0xF;
                    decl->samplerType = 0;
                }

                outShader->declCount++;
            }
            pos += 1 + instrLen;
            continue;
        }

        /* Handle D3DSIO_DEF — float constant definition */
        if (opcode == D3DSIO_DEF) {
            if (outShader->constDefCount < GLD_MAX_SHADER_CONSTANTS && pos + 5 <= tokenCount) {
                GLD_shaderConstDef *def = &outShader->constantDefs[outShader->constDefCount];
                DWORD regToken = bytecode[pos + 1];
                def->regIndex = gldGetRegIndex(regToken);
                def->type = 0; /* float */
                memcpy(def->value.f, &bytecode[pos + 2], 4 * sizeof(float));
                outShader->constDefCount++;
            }
            pos += 1 + instrLen;
            continue;
        }

        /* Handle D3DSIO_DEFI — integer constant definition */
        if (opcode == D3DSIO_DEFI) {
            if (outShader->constDefCount < GLD_MAX_SHADER_CONSTANTS && pos + 5 <= tokenCount) {
                GLD_shaderConstDef *def = &outShader->constantDefs[outShader->constDefCount];
                DWORD regToken = bytecode[pos + 1];
                def->regIndex = gldGetRegIndex(regToken);
                def->type = 1; /* int */
                memcpy(def->value.i, &bytecode[pos + 2], 4 * sizeof(int));
                outShader->constDefCount++;
            }
            pos += 1 + instrLen;
            continue;
        }

        /* Handle D3DSIO_DEFB — boolean constant definition */
        if (opcode == D3DSIO_DEFB) {
            if (outShader->constDefCount < GLD_MAX_SHADER_CONSTANTS && pos + 2 <= tokenCount) {
                GLD_shaderConstDef *def = &outShader->constantDefs[outShader->constDefCount];
                DWORD regToken = bytecode[pos + 1];
                def->regIndex = gldGetRegIndex(regToken);
                def->type = 2; /* bool */
                def->value.b = (bytecode[pos + 2] != 0) ? TRUE : FALSE;
                outShader->constDefCount++;
            }
            pos += 1 + instrLen;
            continue;
        }

        /* General instruction */
        if (outShader->instructionCount < GLD_MAX_SHADER_INSTRUCTIONS) {
            GLD_shaderInstruction *instr = &outShader->instructions[outShader->instructionCount];
            int tokIdx = pos + 1;

            memset(instr, 0, sizeof(*instr));
            instr->opcode = opcode;
            instr->bCoIssue = (instrToken & 0x40000000) ? TRUE : FALSE;
            instr->bPredicated = (instrToken & 0x10000000) ? TRUE : FALSE;

            /* Opcodes with no operands */
            if (opcode == D3DSIO_NOP || opcode == D3DSIO_ELSE ||
                opcode == D3DSIO_ENDIF || opcode == D3DSIO_ENDLOOP ||
                opcode == D3DSIO_ENDREP || opcode == D3DSIO_RET) {
                /* No operands to parse */
            }
            /* Opcodes with only source operands (no dest) */
            else if (opcode == D3DSIO_IF || opcode == D3DSIO_CALL ||
                     opcode == D3DSIO_REP || opcode == D3DSIO_LOOP ||
                     opcode == D3DSIO_BREAK) {
                if (instrLen >= 1 && tokIdx < tokenCount) {
                    gldParseSrcToken(bytecode[tokIdx], &instr->srcRegs[0]);
                    instr->srcCount = 1;
                    if (opcode == D3DSIO_LOOP && instrLen >= 2 && tokIdx + 1 < tokenCount) {
                        gldParseSrcToken(bytecode[tokIdx + 1], &instr->srcRegs[1]);
                        instr->srcCount = 2;
                    }
                }
            }
            /* IFC / BREAKC — two source operands + comparison */
            else if (opcode == D3DSIO_IFC || opcode == D3DSIO_BREAKC) {
                instr->comparison = (instrToken >> 16) & 0x7;
                if (instrLen >= 2 && tokIdx + 1 < tokenCount) {
                    gldParseSrcToken(bytecode[tokIdx], &instr->srcRegs[0]);
                    gldParseSrcToken(bytecode[tokIdx + 1], &instr->srcRegs[1]);
                    instr->srcCount = 2;
                }
            }
            /* TEXKILL — one dest operand only */
            else if (opcode == D3DSIO_TEXKILL) {
                if (instrLen >= 1 && tokIdx < tokenCount) {
                    gldParseDestToken(bytecode[tokIdx], &instr->destReg);
                }
            }
            /* Standard instructions: dest + N source operands */
            else {
                int srcIdx;
                if (instrLen >= 1 && tokIdx < tokenCount) {
                    gldParseDestToken(bytecode[tokIdx], &instr->destReg);
                    tokIdx++;
                }
                instr->srcCount = instrLen - 1;
                if (instr->srcCount > GLD_MAX_SHADER_SRC_REGS)
                    instr->srcCount = GLD_MAX_SHADER_SRC_REGS;
                for (srcIdx = 0; srcIdx < instr->srcCount && tokIdx < tokenCount; srcIdx++) {
                    gldParseSrcToken(bytecode[tokIdx], &instr->srcRegs[srcIdx]);
                    tokIdx++;
                }
            }

            outShader->instructionCount++;
        } else {
            gldLogPrintf(GLDLOG_WARN,
                "gldParseShaderBytecode: instruction limit (%d) reached, truncating",
                GLD_MAX_SHADER_INSTRUCTIONS);
        }

        pos += 1 + instrLen;
    }

    gldLogPrintf(GLDLOG_INFO,
        "gldParseShaderBytecode: parsed %d instructions, %d declarations, %d constants",
        outShader->instructionCount, outShader->declCount, outShader->constDefCount);

    return TRUE;
}

/*==================== Task 13.2 & 13.3: GLSL Generation ====================*/

/*
 * Append a formatted string to the GLSL output buffer.
 * Returns the number of characters written, or -1 on overflow.
 */
static int gldGLSLPrintf(char *buf, int *offset, int bufSize,
                          const char *fmt, ...)
{
    va_list args;
    int written;
    int remaining = bufSize - *offset;

    if (remaining <= 0)
        return -1;

    va_start(args, fmt);
    written = _vsnprintf(buf + *offset, remaining, fmt, args);
    va_end(args);

    if (written < 0 || written >= remaining) {
        *offset = bufSize; /* mark as full */
        return -1;
    }

    *offset += written;
    return written;
}

/*
 * Emit a swizzle suffix string for a source register.
 * The swizzle is 8 bits: 2 bits per component (x=0, y=1, z=2, w=3).
 * Identity swizzle (0xE4 = xyzw) is omitted for cleaner output.
 */
static void gldEmitSwizzle(char *out, int maxLen, int swizzle, int writeMask)
{
    static const char comp[] = "xyzw";
    int x = (swizzle >> 0) & 3;
    int y = (swizzle >> 2) & 3;
    int z = (swizzle >> 4) & 3;
    int w = (swizzle >> 6) & 3;

    /* If identity swizzle and full mask, emit nothing */
    if (swizzle == 0xE4 && writeMask == 0xF) {
        out[0] = '\0';
        return;
    }

    out[0] = '.';
    out[1] = comp[x];
    out[2] = comp[y];
    out[3] = comp[z];
    out[4] = comp[w];
    out[5] = '\0';

    /* Trim trailing components if they match a shorter pattern */
    /* e.g. .xxxx -> .xxxx, .xyzw -> (empty), .xy -> .xy */
}

/*
 * Emit a write mask suffix string for a destination register.
 * writeMask bits: 0=x, 1=y, 2=z, 3=w.
 * Full mask (0xF) is omitted.
 */
static void gldEmitWriteMask(char *out, int maxLen, int writeMask)
{
    int pos = 0;

    if (writeMask == 0xF) {
        out[0] = '\0';
        return;
    }

    out[pos++] = '.';
    if (writeMask & 1) out[pos++] = 'x';
    if (writeMask & 2) out[pos++] = 'y';
    if (writeMask & 4) out[pos++] = 'z';
    if (writeMask & 8) out[pos++] = 'w';
    out[pos] = '\0';
}

/*
 * Emit a register name string (e.g. "r0", "v1", "c[5]", "oC0").
 */
static void gldEmitRegName(char *out, int maxLen,
                           const GLD_shaderRegister *reg,
                           BOOL isVertexShader)
{
    switch (reg->regType) {
    case GLD_REG_TEMP:
        _snprintf(out, maxLen, "r%d", reg->regIndex);
        break;
    case GLD_REG_INPUT:
        _snprintf(out, maxLen, "v%d", reg->regIndex);
        break;
    case GLD_REG_CONST:
        _snprintf(out, maxLen, "c[%d]", reg->regIndex);
        break;
    case GLD_REG_ADDR:
        /* In VS: address register a0; in PS: texture register t# */
        if (isVertexShader)
            _snprintf(out, maxLen, "a%d", reg->regIndex);
        else
            _snprintf(out, maxLen, "t%d", reg->regIndex);
        break;
    case GLD_REG_RASTOUT:
        /* oPos=0, oFog=1, oPts=2 */
        if (reg->regIndex == 0)
            _snprintf(out, maxLen, "gl_Position");
        else if (reg->regIndex == 1)
            _snprintf(out, maxLen, "vs_fog");
        else
            _snprintf(out, maxLen, "gl_PointSize");
        break;
    case GLD_REG_ATTROUT:
        _snprintf(out, maxLen, "oD%d", reg->regIndex);
        break;
    case GLD_REG_OUTPUT:
        _snprintf(out, maxLen, "o%d", reg->regIndex);
        break;
    case GLD_REG_CONSTINT:
        _snprintf(out, maxLen, "i[%d]", reg->regIndex);
        break;
    case GLD_REG_COLOROUT:
        _snprintf(out, maxLen, "fragColor_%d", reg->regIndex);
        break;
    case GLD_REG_DEPTHOUT:
        _snprintf(out, maxLen, "gl_FragDepth");
        break;
    case GLD_REG_SAMPLER:
        _snprintf(out, maxLen, "s%d", reg->regIndex);
        break;
    case GLD_REG_CONSTBOOL:
        _snprintf(out, maxLen, "b[%d]", reg->regIndex);
        break;
    case GLD_REG_LOOP:
        _snprintf(out, maxLen, "aL");
        break;
    case GLD_REG_MISCTYPE:
        if (reg->regIndex == 0)
            _snprintf(out, maxLen, "gl_FragCoord");
        else
            _snprintf(out, maxLen, "gl_FrontFacing ? 1.0 : -1.0");
        break;
    case GLD_REG_PREDICATE:
        _snprintf(out, maxLen, "p%d", reg->regIndex);
        break;
    default:
        _snprintf(out, maxLen, "unknown_%d_%d", reg->regType, reg->regIndex);
        break;
    }
    out[maxLen - 1] = '\0';
}

/*
 * Emit a full source operand expression with swizzle and modifiers.
 * E.g. "-abs(r0.xyzw)", "c[5].xxxx", "v0.xy"
 */
static void gldEmitSrcOperand(char *out, int maxLen,
                              const GLD_shaderRegister *reg,
                              BOOL isVertexShader)
{
    char regName[64];
    char swiz[8];
    int pos = 0;

    gldEmitRegName(regName, sizeof(regName), reg, isVertexShader);
    gldEmitSwizzle(swiz, sizeof(swiz), reg->swizzle, 0xF);

    switch (reg->srcModifier) {
    case GLD_SRCMOD_NEGATE:
        pos = _snprintf(out, maxLen, "(-%s%s)", regName, swiz);
        break;
    case GLD_SRCMOD_ABS:
        pos = _snprintf(out, maxLen, "abs(%s%s)", regName, swiz);
        break;
    case GLD_SRCMOD_ABSNEGATE:
        pos = _snprintf(out, maxLen, "(-abs(%s%s))", regName, swiz);
        break;
    case GLD_SRCMOD_COMP:
        pos = _snprintf(out, maxLen, "(vec4(1.0) - %s%s)", regName, swiz);
        break;
    default:
        pos = _snprintf(out, maxLen, "%s%s", regName, swiz);
        break;
    }
    if (pos < 0) pos = 0;
    out[maxLen - 1] = '\0';
}

/*
 * Emit a destination operand with write mask.
 * E.g. "r0.xy", "fragColor_0", "gl_Position"
 */
static void gldEmitDstOperand(char *out, int maxLen,
                              const GLD_shaderRegister *reg,
                              BOOL isVertexShader)
{
    char regName[64];
    char mask[8];

    gldEmitRegName(regName, sizeof(regName), reg, isVertexShader);
    gldEmitWriteMask(mask, sizeof(mask), reg->writeMask);

    _snprintf(out, maxLen, "%s%s", regName, mask);
    out[maxLen - 1] = '\0';
}

/*
 * Find the sampler type for a given sampler register index.
 * Returns GLD_SAMPLER_2D by default.
 */
static int gldFindSamplerType(const GLD_parsedShader *shader, int samplerIndex)
{
    int i;
    for (i = 0; i < shader->declCount; i++) {
        if (shader->declarations[i].regType == GLD_REG_SAMPLER &&
            shader->declarations[i].regIndex == samplerIndex) {
            return shader->declarations[i].samplerType;
        }
    }
    return GLD_SAMPLER_2D;
}

/*
 * Map a VS input semantic to a GLSL layout location.
 * Matches the Buffer_Manager convention.
 */
static int gldSemanticToLocation(int usage, int usageIndex)
{
    switch (usage) {
    case GLD_DCLUSAGE_POSITION:
    case GLD_DCLUSAGE_POSITIONT:
        return 0;
    case GLD_DCLUSAGE_NORMAL:
        return 1;
    case GLD_DCLUSAGE_COLOR:
        return 2 + usageIndex; /* COLOR0=2, COLOR1=3 */
    case GLD_DCLUSAGE_TEXCOORD:
        return 4 + usageIndex; /* TEXCOORD0=4 .. TEXCOORD7=11 */
    case GLD_DCLUSAGE_BLENDWEIGHT:
        return 12;
    case GLD_DCLUSAGE_BLENDINDICES:
        return 13;
    case GLD_DCLUSAGE_TANGENT:
        return 14;
    case GLD_DCLUSAGE_BINORMAL:
        return 15;
    case GLD_DCLUSAGE_PSIZE:
        return 4; /* fallback to texcoord0 slot */
    case GLD_DCLUSAGE_FOG:
        return 5; /* fallback */
    default:
        return 0;
    }
}

/*
 * Emit the GLSL header: version, precision, declarations.
 */
static BOOL gldEmitGLSLHeader(const GLD_parsedShader *shader,
                              char *buf, int *offset, int bufSize)
{
    int i;
    BOOL usedTempRegs[GLD_MAX_SHADER_TEMP_REGS];
    BOOL usedSamplers[GLD_MAX_SHADER_SAMPLER_REGS];
    int maxConstReg = -1;
    int maxIntConstReg = -1;
    int maxBoolConstReg = -1;

    /* Version directive */
    gldGLSLPrintf(buf, offset, bufSize, "#version 460 core\n\n");

    /* Scan instructions to find used temp registers, samplers, and max const */
    memset(usedTempRegs, 0, sizeof(usedTempRegs));
    memset(usedSamplers, 0, sizeof(usedSamplers));

    for (i = 0; i < shader->instructionCount; i++) {
        const GLD_shaderInstruction *instr = &shader->instructions[i];
        int s;

        if (instr->destReg.regType == GLD_REG_TEMP &&
            instr->destReg.regIndex < GLD_MAX_SHADER_TEMP_REGS)
            usedTempRegs[instr->destReg.regIndex] = TRUE;

        for (s = 0; s < instr->srcCount; s++) {
            const GLD_shaderRegister *src = &instr->srcRegs[s];
            if (src->regType == GLD_REG_TEMP && src->regIndex < GLD_MAX_SHADER_TEMP_REGS)
                usedTempRegs[src->regIndex] = TRUE;
            if (src->regType == GLD_REG_CONST && src->regIndex > maxConstReg)
                maxConstReg = src->regIndex;
            if (src->regType == GLD_REG_CONSTINT && src->regIndex > maxIntConstReg)
                maxIntConstReg = src->regIndex;
            if (src->regType == GLD_REG_CONSTBOOL && src->regIndex > maxBoolConstReg)
                maxBoolConstReg = src->regIndex;
            if (src->regType == GLD_REG_SAMPLER && src->regIndex < GLD_MAX_SHADER_SAMPLER_REGS)
                usedSamplers[src->regIndex] = TRUE;
        }
    }

    /* Also check constant definitions for max register */
    for (i = 0; i < shader->constDefCount; i++) {
        if (shader->constantDefs[i].type == 0 && shader->constantDefs[i].regIndex > maxConstReg)
            maxConstReg = shader->constantDefs[i].regIndex;
        if (shader->constantDefs[i].type == 1 && shader->constantDefs[i].regIndex > maxIntConstReg)
            maxIntConstReg = shader->constantDefs[i].regIndex;
    }

    /* Emit input declarations */
    if (shader->isVertexShader) {
        /* VS inputs: layout(location=N) in vec4 v#; */
        for (i = 0; i < shader->declCount; i++) {
            const GLD_shaderDecl *decl = &shader->declarations[i];
            if (decl->regType == GLD_REG_INPUT) {
                int loc = gldSemanticToLocation(decl->usage, decl->usageIndex);
                gldGLSLPrintf(buf, offset, bufSize,
                    "layout(location = %d) in vec4 v%d;\n",
                    loc, decl->regIndex);
            }
        }
    } else {
        /* PS inputs: in vec4 v#; (matched by location from VS outputs) */
        for (i = 0; i < shader->declCount; i++) {
            const GLD_shaderDecl *decl = &shader->declarations[i];
            if (decl->regType == GLD_REG_INPUT) {
                int loc = gldSemanticToLocation(decl->usage, decl->usageIndex);
                gldGLSLPrintf(buf, offset, bufSize,
                    "layout(location = %d) in vec4 v%d;\n",
                    loc, decl->regIndex);
            }
        }
    }

    /* Emit output declarations */
    if (shader->isVertexShader) {
        /* VS outputs: out vec4 o#; */
        for (i = 0; i < shader->declCount; i++) {
            const GLD_shaderDecl *decl = &shader->declarations[i];
            if (decl->regType == GLD_REG_OUTPUT) {
                int loc = gldSemanticToLocation(decl->usage, decl->usageIndex);
                gldGLSLPrintf(buf, offset, bufSize,
                    "layout(location = %d) out vec4 o%d;\n",
                    loc, decl->regIndex);
            }
        }
        /* SM2.0 VS uses oD# (ATTROUT) and oT# (TEXTURE) for outputs */
        /* Emit standard VS outputs for SM2.0 */
        if (shader->majorVersion <= 2) {
            gldGLSLPrintf(buf, offset, bufSize,
                "layout(location = 0) out vec4 oD0;\n"
                "layout(location = 1) out vec4 oD1;\n");
        }
    } else {
        /* PS outputs: layout(location=N) out vec4 fragColor_N; */
        /* Scan for used color outputs */
        BOOL usedColorOut[4] = { FALSE, FALSE, FALSE, FALSE };
        for (i = 0; i < shader->instructionCount; i++) {
            if (shader->instructions[i].destReg.regType == GLD_REG_COLOROUT &&
                shader->instructions[i].destReg.regIndex < 4)
                usedColorOut[shader->instructions[i].destReg.regIndex] = TRUE;
        }
        for (i = 0; i < 4; i++) {
            if (usedColorOut[i]) {
                gldGLSLPrintf(buf, offset, bufSize,
                    "layout(location = %d) out vec4 fragColor_%d;\n", i, i);
            }
        }
    }

    gldGLSLPrintf(buf, offset, bufSize, "\n");

    /* Emit uniform constant arrays */
    if (maxConstReg >= 0) {
        gldGLSLPrintf(buf, offset, bufSize,
            "uniform vec4 c[%d];\n", maxConstReg + 1);
    }
    if (maxIntConstReg >= 0) {
        gldGLSLPrintf(buf, offset, bufSize,
            "uniform ivec4 i[%d];\n", maxIntConstReg + 1);
    }
    if (maxBoolConstReg >= 0) {
        gldGLSLPrintf(buf, offset, bufSize,
            "uniform bool b[%d];\n", maxBoolConstReg + 1);
    }

    /* Emit sampler uniforms */
    for (i = 0; i < shader->declCount; i++) {
        const GLD_shaderDecl *decl = &shader->declarations[i];
        if (decl->regType == GLD_REG_SAMPLER) {
            const char *samplerTypeName;
            switch (decl->samplerType) {
            case GLD_SAMPLER_CUBE:   samplerTypeName = "samplerCube"; break;
            case GLD_SAMPLER_VOLUME: samplerTypeName = "sampler3D";   break;
            default:                 samplerTypeName = "sampler2D";   break;
            }
            gldGLSLPrintf(buf, offset, bufSize,
                "uniform %s s%d;\n", samplerTypeName, decl->regIndex);
        }
    }
    /* Also emit samplers found in instructions but not declared */
    for (i = 0; i < GLD_MAX_SHADER_SAMPLER_REGS; i++) {
        if (usedSamplers[i]) {
            BOOL declared = FALSE;
            int j;
            for (j = 0; j < shader->declCount; j++) {
                if (shader->declarations[j].regType == GLD_REG_SAMPLER &&
                    shader->declarations[j].regIndex == i) {
                    declared = TRUE;
                    break;
                }
            }
            if (!declared) {
                gldGLSLPrintf(buf, offset, bufSize,
                    "uniform sampler2D s%d;\n", i);
            }
        }
    }

    gldGLSLPrintf(buf, offset, bufSize, "\nvoid main() {\n");

    /* Declare temp registers as local vec4 variables */
    for (i = 0; i < GLD_MAX_SHADER_TEMP_REGS; i++) {
        if (usedTempRegs[i]) {
            gldGLSLPrintf(buf, offset, bufSize,
                "    vec4 r%d = vec4(0.0);\n", i);
        }
    }

    /* Emit inline constant definitions */
    for (i = 0; i < shader->constDefCount; i++) {
        const GLD_shaderConstDef *def = &shader->constantDefs[i];
        if (def->type == 0) {
            gldGLSLPrintf(buf, offset, bufSize,
                "    c[%d] = vec4(%g, %g, %g, %g);\n",
                def->regIndex,
                def->value.f[0], def->value.f[1],
                def->value.f[2], def->value.f[3]);
        }
    }

    if (shader->isVertexShader && shader->majorVersion <= 2) {
        /* Initialize SM2.0 VS attribute outputs */
        gldGLSLPrintf(buf, offset, bufSize,
            "    oD0 = vec4(0.0);\n"
            "    oD1 = vec4(0.0);\n");
    }

    gldGLSLPrintf(buf, offset, bufSize, "\n");

    return (*offset < bufSize);
}

/*
 * Emit GLSL for a single instruction.
 */
static BOOL gldEmitInstruction(const GLD_parsedShader *shader,
                               const GLD_shaderInstruction *instr,
                               char *buf, int *offset, int bufSize)
{
    char dst[128], src0[128], src1[128], src2[128], src3[128];
    char expr[512];
    BOOL saturate;

    saturate = (instr->destReg.dstModifier & GLD_DSTMOD_SATURATE) != 0;

    /* Prepare operand strings for instructions that have them */
    if (instr->opcode != D3DSIO_NOP && instr->opcode != D3DSIO_ELSE &&
        instr->opcode != D3DSIO_ENDIF && instr->opcode != D3DSIO_ENDLOOP &&
        instr->opcode != D3DSIO_ENDREP && instr->opcode != D3DSIO_RET &&
        instr->opcode != D3DSIO_IF && instr->opcode != D3DSIO_IFC &&
        instr->opcode != D3DSIO_CALL && instr->opcode != D3DSIO_REP &&
        instr->opcode != D3DSIO_LOOP && instr->opcode != D3DSIO_BREAK &&
        instr->opcode != D3DSIO_BREAKC) {

        gldEmitDstOperand(dst, sizeof(dst), &instr->destReg, shader->isVertexShader);

        if (instr->srcCount >= 1)
            gldEmitSrcOperand(src0, sizeof(src0), &instr->srcRegs[0], shader->isVertexShader);
        if (instr->srcCount >= 2)
            gldEmitSrcOperand(src1, sizeof(src1), &instr->srcRegs[1], shader->isVertexShader);
        if (instr->srcCount >= 3)
            gldEmitSrcOperand(src2, sizeof(src2), &instr->srcRegs[2], shader->isVertexShader);
        if (instr->srcCount >= 4)
            gldEmitSrcOperand(src3, sizeof(src3), &instr->srcRegs[3], shader->isVertexShader);
    }

    switch (instr->opcode) {

    /* No-op */
    case D3DSIO_NOP:
        break;

    /* MOV dest, src */
    case D3DSIO_MOV:
    case D3DSIO_MOVA:
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, src0);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, src0);
        break;

    /* ADD dest, src0, src1 */
    case D3DSIO_ADD:
        _snprintf(expr, sizeof(expr), "%s + %s", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* SUB dest, src0, src1 */
    case D3DSIO_SUB:
        _snprintf(expr, sizeof(expr), "%s - %s", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* MUL dest, src0, src1 */
    case D3DSIO_MUL:
        _snprintf(expr, sizeof(expr), "%s * %s", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* MAD dest, src0, src1, src2 -> dest = src0 * src1 + src2 */
    case D3DSIO_MAD:
        _snprintf(expr, sizeof(expr), "%s * %s + %s", src0, src1, src2);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* RCP dest, src -> dest = 1.0 / src */
    case D3DSIO_RCP:
        _snprintf(expr, sizeof(expr), "vec4(1.0 / %s.x)", src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* RSQ dest, src -> dest = inversesqrt(src) */
    case D3DSIO_RSQ:
        _snprintf(expr, sizeof(expr), "vec4(inversesqrt(abs(%s.x)))", src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* DP3 dest, src0, src1 -> dest = dot(src0.xyz, src1.xyz) */
    case D3DSIO_DP3:
        _snprintf(expr, sizeof(expr), "vec4(dot(%s.xyz, %s.xyz))", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* DP4 dest, src0, src1 -> dest = dot(src0, src1) */
    case D3DSIO_DP4:
        _snprintf(expr, sizeof(expr), "vec4(dot(%s, %s))", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* MIN dest, src0, src1 */
    case D3DSIO_MIN:
        _snprintf(expr, sizeof(expr), "min(%s, %s)", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* MAX dest, src0, src1 */
    case D3DSIO_MAX:
        _snprintf(expr, sizeof(expr), "max(%s, %s)", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* SLT dest, src0, src1 -> dest = (src0 < src1) ? 1.0 : 0.0 */
    case D3DSIO_SLT:
        _snprintf(expr, sizeof(expr), "vec4(lessThan(%s, %s))", src0, src1);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* SGE dest, src0, src1 -> dest = (src0 >= src1) ? 1.0 : 0.0 */
    case D3DSIO_SGE:
        _snprintf(expr, sizeof(expr), "vec4(greaterThanEqual(%s, %s))", src0, src1);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* EXP dest, src -> dest = exp2(src) */
    case D3DSIO_EXP:
        _snprintf(expr, sizeof(expr), "vec4(exp2(%s.x))", src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* LOG dest, src -> dest = log2(src) */
    case D3DSIO_LOG:
        _snprintf(expr, sizeof(expr), "vec4(log2(abs(%s.x)))", src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* FRC dest, src -> dest = fract(src) */
    case D3DSIO_FRC:
        _snprintf(expr, sizeof(expr), "fract(%s)", src0);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* ABS dest, src -> dest = abs(src) */
    case D3DSIO_ABS:
        _snprintf(expr, sizeof(expr), "abs(%s)", src0);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* NRM dest, src -> dest = normalize(src.xyz) */
    case D3DSIO_NRM:
        _snprintf(expr, sizeof(expr), "vec4(normalize(%s.xyz), 0.0)", src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* SINCOS dest, src -> dest.x = cos(src.x), dest.y = sin(src.x) */
    case D3DSIO_SINCOS:
        gldGLSLPrintf(buf, offset, bufSize,
            "    %s = vec4(cos(%s.x), sin(%s.x), 0.0, 0.0);\n",
            dst, src0, src0);
        break;

    /* POW dest, src0, src1 -> dest = pow(abs(src0.x), src1.x) */
    case D3DSIO_POW:
        _snprintf(expr, sizeof(expr), "vec4(pow(abs(%s.x), %s.x))", src0, src1);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* LRP dest, src0, src1, src2 -> dest = mix(src2, src1, src0) */
    case D3DSIO_LRP:
        _snprintf(expr, sizeof(expr), "mix(%s, %s, %s)", src2, src1, src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* CMP dest, src0, src1, src2 -> dest = (src0 >= 0) ? src1 : src2 */
    case D3DSIO_CMP:
        _snprintf(expr, sizeof(expr),
            "mix(%s, %s, vec4(greaterThanEqual(%s, vec4(0.0))))",
            src2, src1, src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* CRS dest, src0, src1 -> dest.xyz = cross(src0.xyz, src1.xyz) */
    case D3DSIO_CRS:
        _snprintf(expr, sizeof(expr), "vec4(cross(%s.xyz, %s.xyz), 0.0)", src0, src1);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* LIT dest, src -> lighting coefficient computation */
    case D3DSIO_LIT:
        gldGLSLPrintf(buf, offset, bufSize,
            "    %s = vec4(1.0, max(%s.x, 0.0), "
            "(%s.x > 0.0) ? pow(max(%s.y, 0.0), clamp(%s.w, -128.0, 128.0)) : 0.0, "
            "1.0);\n",
            dst, src0, src0, src0, src0);
        break;

    /* DST dest, src0, src1 -> distance vector */
    case D3DSIO_DST:
        gldGLSLPrintf(buf, offset, bufSize,
            "    %s = vec4(1.0, %s.y * %s.y, %s.z, %s.w);\n",
            dst, src0, src1, src0, src1);
        break;

    /* DP2ADD dest, src0, src1, src2 -> dest = dot(src0.xy, src1.xy) + src2.x */
    case D3DSIO_DP2ADD:
        _snprintf(expr, sizeof(expr),
            "vec4(dot(%s.xy, %s.xy) + %s.x)", src0, src1, src2);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    /* TEX — texture sampling */
    case D3DSIO_TEX: {
        /* src0 = texture coordinates, src1 = sampler */
        int samplerIdx = (instr->srcCount >= 2) ? instr->srcRegs[1].regIndex : instr->destReg.regIndex;
        int samplerType = gldFindSamplerType(shader, samplerIdx);

        switch (samplerType) {
        case GLD_SAMPLER_CUBE:
            _snprintf(expr, sizeof(expr), "texture(s%d, %s.xyz)", samplerIdx, src0);
            break;
        case GLD_SAMPLER_VOLUME:
            _snprintf(expr, sizeof(expr), "texture(s%d, %s.xyz)", samplerIdx, src0);
            break;
        default:
            _snprintf(expr, sizeof(expr), "texture(s%d, %s.xy)", samplerIdx, src0);
            break;
        }
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;
    }

    /* TEXLDL — texture sample with explicit LOD */
    case D3DSIO_TEXLDL: {
        int samplerIdx = (instr->srcCount >= 2) ? instr->srcRegs[1].regIndex : 0;
        int samplerType = gldFindSamplerType(shader, samplerIdx);

        switch (samplerType) {
        case GLD_SAMPLER_CUBE:
            _snprintf(expr, sizeof(expr), "textureLod(s%d, %s.xyz, %s.w)", samplerIdx, src0, src0);
            break;
        case GLD_SAMPLER_VOLUME:
            _snprintf(expr, sizeof(expr), "textureLod(s%d, %s.xyz, %s.w)", samplerIdx, src0, src0);
            break;
        default:
            _snprintf(expr, sizeof(expr), "textureLod(s%d, %s.xy, %s.w)", samplerIdx, src0, src0);
            break;
        }
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;
    }

    /* TEXLDB — texture sample with bias */
    case D3DSIO_TEXLDB: {
        int samplerIdx = (instr->srcCount >= 2) ? instr->srcRegs[1].regIndex : 0;
        _snprintf(expr, sizeof(expr), "texture(s%d, %s.xy, %s.w)", samplerIdx, src0, src0);
        if (saturate)
            gldGLSLPrintf(buf, offset, bufSize, "    %s = clamp(%s, 0.0, 1.0);\n", dst, expr);
        else
            gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;
    }

    /* TEXKILL — discard if any component < 0 */
    case D3DSIO_TEXKILL: {
        char killReg[128];
        gldEmitRegName(killReg, sizeof(killReg), &instr->destReg, shader->isVertexShader);
        gldGLSLPrintf(buf, offset, bufSize,
            "    if (any(lessThan(%s.xyz, vec3(0.0)))) discard;\n", killReg);
        break;
    }

    /* Flow control — Task 13.3 */
    case D3DSIO_IF: {
        char cond[128];
        gldEmitSrcOperand(cond, sizeof(cond), &instr->srcRegs[0], shader->isVertexShader);
        gldGLSLPrintf(buf, offset, bufSize,
            "    if (%s.x != 0.0) {\n", cond);
        break;
    }

    case D3DSIO_IFC: {
        char lhs[128], rhs[128];
        const char *cmpOp;
        gldEmitSrcOperand(lhs, sizeof(lhs), &instr->srcRegs[0], shader->isVertexShader);
        gldEmitSrcOperand(rhs, sizeof(rhs), &instr->srcRegs[1], shader->isVertexShader);
        switch (instr->comparison) {
        case 1: cmpOp = ">"; break;
        case 2: cmpOp = "=="; break;
        case 3: cmpOp = ">="; break;
        case 4: cmpOp = "<"; break;
        case 5: cmpOp = "!="; break;
        case 6: cmpOp = "<="; break;
        default: cmpOp = "=="; break;
        }
        gldGLSLPrintf(buf, offset, bufSize,
            "    if (%s.x %s %s.x) {\n", lhs, cmpOp, rhs);
        break;
    }

    case D3DSIO_ELSE:
        gldGLSLPrintf(buf, offset, bufSize, "    } else {\n");
        break;

    case D3DSIO_ENDIF:
        gldGLSLPrintf(buf, offset, bufSize, "    }\n");
        break;

    case D3DSIO_LOOP: {
        /* LOOP src0, src1 — src1 is integer constant with (count, initial, step) */
        char loopSrc[128];
        gldEmitSrcOperand(loopSrc, sizeof(loopSrc), &instr->srcRegs[1], shader->isVertexShader);
        gldGLSLPrintf(buf, offset, bufSize,
            "    for (int aL = %s.y; aL < %s.x + %s.y; aL += %s.z) {\n",
            loopSrc, loopSrc, loopSrc, loopSrc);
        break;
    }

    case D3DSIO_ENDLOOP:
        gldGLSLPrintf(buf, offset, bufSize, "    }\n");
        break;

    case D3DSIO_REP: {
        char repSrc[128];
        gldEmitSrcOperand(repSrc, sizeof(repSrc), &instr->srcRegs[0], shader->isVertexShader);
        gldGLSLPrintf(buf, offset, bufSize,
            "    for (int rep_i = 0; rep_i < %s.x; rep_i++) {\n", repSrc);
        break;
    }

    case D3DSIO_ENDREP:
        gldGLSLPrintf(buf, offset, bufSize, "    }\n");
        break;

    case D3DSIO_BREAK:
        gldGLSLPrintf(buf, offset, bufSize, "    break;\n");
        break;

    case D3DSIO_BREAKC: {
        char lhs[128], rhs[128];
        const char *cmpOp;
        gldEmitSrcOperand(lhs, sizeof(lhs), &instr->srcRegs[0], shader->isVertexShader);
        gldEmitSrcOperand(rhs, sizeof(rhs), &instr->srcRegs[1], shader->isVertexShader);
        switch (instr->comparison) {
        case 1: cmpOp = ">"; break;
        case 2: cmpOp = "=="; break;
        case 3: cmpOp = ">="; break;
        case 4: cmpOp = "<"; break;
        case 5: cmpOp = "!="; break;
        case 6: cmpOp = "<="; break;
        default: cmpOp = "=="; break;
        }
        gldGLSLPrintf(buf, offset, bufSize,
            "    if (%s.x %s %s.x) break;\n", lhs, cmpOp, rhs);
        break;
    }

    case D3DSIO_CALL:
    case D3DSIO_RET:
        /* Subroutine call/return — emit as comment for now */
        gldGLSLPrintf(buf, offset, bufSize,
            "    /* %s (subroutine not yet supported) */\n",
            instr->opcode == D3DSIO_CALL ? "CALL" : "RET");
        break;

    case D3DSIO_SGN:
        _snprintf(expr, sizeof(expr), "sign(%s)", src0);
        gldGLSLPrintf(buf, offset, bufSize, "    %s = %s;\n", dst, expr);
        break;

    default:
        gldGLSLPrintf(buf, offset, bufSize,
            "    /* unsupported opcode 0x%04X */\n", instr->opcode);
        gldLogPrintf(GLDLOG_WARN,
            "gldGenerateGLSL: unsupported opcode 0x%04X", instr->opcode);
        break;
    }

    return (*offset < bufSize);
}

/*
 * gldGenerateGLSL — main GLSL generation entry point.
 * Combines header emission and per-instruction emission.
 */
BOOL gldGenerateGLSL(const GLD_parsedShader *shader,
                     char *glslBuffer, int bufferSize)
{
    int offset = 0;
    int i;

    if (!shader || !glslBuffer || bufferSize <= 0) {
        gldLogPrintf(GLDLOG_ERROR, "gldGenerateGLSL: invalid parameters");
        return FALSE;
    }

    memset(glslBuffer, 0, bufferSize);

    /* Emit header: version, declarations, uniforms, main() opening */
    if (!gldEmitGLSLHeader(shader, glslBuffer, &offset, bufferSize)) {
        gldLogPrintf(GLDLOG_ERROR, "gldGenerateGLSL: buffer overflow during header");
        return FALSE;
    }

    /* Emit each instruction */
    for (i = 0; i < shader->instructionCount; i++) {
        if (!gldEmitInstruction(shader, &shader->instructions[i],
                                glslBuffer, &offset, bufferSize)) {
            gldLogPrintf(GLDLOG_ERROR,
                "gldGenerateGLSL: buffer overflow at instruction %d", i);
            return FALSE;
        }
    }

    /* Close main() */
    gldGLSLPrintf(glslBuffer, &offset, bufferSize, "}\n");

    if (offset >= bufferSize) {
        gldLogPrintf(GLDLOG_ERROR, "gldGenerateGLSL: output buffer too small");
        return FALSE;
    }

    gldLogPrintf(GLDLOG_INFO,
        "gldGenerateGLSL: generated %d bytes of GLSL for %s shader %d.%d",
        offset,
        shader->isVertexShader ? "vertex" : "pixel",
        shader->majorVersion, shader->minorVersion);

    return TRUE;
}

/*==================== Task 13.4: Compilation, Linking, Caching ====================*/

GLuint gldCompileShader46(GLenum shaderType, const char *glslSource)
{
    GLuint shader;
    GLint status;
    GLint logLen;

    if (!glslSource) {
        gldLogPrintf(GLDLOG_ERROR, "gldCompileShader46: NULL source");
        return 0;
    }

    shader = glCreateShader(shaderType);
    if (shader == 0) {
        gldLogPrintf(GLDLOG_ERROR, "gldCompileShader46: glCreateShader failed");
        return 0;
    }

    glShaderSource(shader, 1, &glslSource, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            char *infoLog = (char *)malloc(logLen + 1);
            if (infoLog) {
                glGetShaderInfoLog(shader, logLen, NULL, infoLog);
                infoLog[logLen] = '\0';
                gldLogPrintf(GLDLOG_ERROR,
                    "gldCompileShader46: compilation failed for %s shader:\n%s",
                    (shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment",
                    infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    gldLogPrintf(GLDLOG_DEBUG,
        "gldCompileShader46: %s shader compiled successfully (id=%u)",
        (shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment",
        shader);

    return shader;
}

GLuint gldLinkProgram46(GLuint vertShader, GLuint fragShader)
{
    GLuint program;
    GLint status;
    GLint logLen;

    program = glCreateProgram();
    if (program == 0) {
        gldLogPrintf(GLDLOG_ERROR, "gldLinkProgram46: glCreateProgram failed");
        return 0;
    }

    if (vertShader != 0)
        glAttachShader(program, vertShader);
    if (fragShader != 0)
        glAttachShader(program, fragShader);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            char *infoLog = (char *)malloc(logLen + 1);
            if (infoLog) {
                glGetProgramInfoLog(program, logLen, NULL, infoLog);
                infoLog[logLen] = '\0';
                gldLogPrintf(GLDLOG_ERROR,
                    "gldLinkProgram46: link failed:\n%s", infoLog);
                free(infoLog);
            }
        }
        glDeleteProgram(program);
        return 0;
    }

    /* Detach shaders after successful link — they can be deleted by caller */
    if (vertShader != 0)
        glDetachShader(program, vertShader);
    if (fragShader != 0)
        glDetachShader(program, fragShader);

    gldLogPrintf(GLDLOG_DEBUG,
        "gldLinkProgram46: program linked successfully (id=%u)", program);

    return program;
}

/*
 * Initialize the shader cache if not already done.
 */
static void gldInitShaderCache(void)
{
    if (!s_cacheInitialized) {
        memset(s_shaderCache, 0, sizeof(s_shaderCache));
        s_cacheInitialized = TRUE;
    }
}

GLuint gldGetCachedProgram(const DWORD *vsBytecode, int vsSize,
                           const DWORD *psBytecode, int psSize)
{
    DWORD hash;
    int i;

    gldInitShaderCache();

    hash = gldComputeShaderPairHash(vsBytecode, vsSize, psBytecode, psSize);

    for (i = 0; i < GLD_SHADER_CACHE_SIZE; i++) {
        if (s_shaderCache[i].bUsed && s_shaderCache[i].hash == hash) {
            gldLogPrintf(GLDLOG_DEBUG,
                "gldGetCachedProgram: cache hit (hash=0x%08X, program=%u)",
                hash, s_shaderCache[i].program);
            return s_shaderCache[i].program;
        }
    }

    return 0; /* Cache miss */
}

void gldCacheProgram(const DWORD *vsBytecode, int vsSize,
                     const DWORD *psBytecode, int psSize,
                     GLuint program)
{
    DWORD hash;
    int i;
    int emptySlot = -1;

    gldInitShaderCache();

    hash = gldComputeShaderPairHash(vsBytecode, vsSize, psBytecode, psSize);

    /* Check if already cached */
    for (i = 0; i < GLD_SHADER_CACHE_SIZE; i++) {
        if (s_shaderCache[i].bUsed && s_shaderCache[i].hash == hash) {
            /* Update existing entry */
            s_shaderCache[i].program = program;
            return;
        }
        if (!s_shaderCache[i].bUsed && emptySlot < 0) {
            emptySlot = i;
        }
    }

    if (emptySlot >= 0) {
        s_shaderCache[emptySlot].hash    = hash;
        s_shaderCache[emptySlot].program = program;
        s_shaderCache[emptySlot].bUsed   = TRUE;
        gldLogPrintf(GLDLOG_DEBUG,
            "gldCacheProgram: cached program %u at slot %d (hash=0x%08X)",
            program, emptySlot, hash);
    } else {
        /* Cache full — evict slot 0 (simple strategy) */
        gldLogPrintf(GLDLOG_WARN,
            "gldCacheProgram: cache full, evicting slot 0");
        if (s_shaderCache[0].program != 0)
            glDeleteProgram(s_shaderCache[0].program);
        s_shaderCache[0].hash    = hash;
        s_shaderCache[0].program = program;
        s_shaderCache[0].bUsed   = TRUE;
    }
}

void gldClearShaderCache(void)
{
    int i;

    for (i = 0; i < GLD_SHADER_CACHE_SIZE; i++) {
        if (s_shaderCache[i].bUsed && s_shaderCache[i].program != 0) {
            glDeleteProgram(s_shaderCache[i].program);
        }
    }

    memset(s_shaderCache, 0, sizeof(s_shaderCache));
    s_cacheInitialized = TRUE;

    gldLogPrintf(GLDLOG_INFO, "gldClearShaderCache: cache cleared");
}
