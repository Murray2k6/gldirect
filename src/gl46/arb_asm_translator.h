/*********************************************************************************
*
*  arb_asm_translator.h - ARB_vertex_program / ARB_fragment_program assembly
*                         to GLSL source translation.
*
*  glProgramStringARB hands us a block of ARB assembly text.  D3D9 cannot
*  consume that, but this tree already owns a working GLSL-text -> HLSL-text ->
*  D3DCompile -> Shader Model 3 bytecode pipeline (glsl_to_hlsl.c) driven by
*  glLinkProgram.  Rather than build a second, parallel back end that would
*  have to reimplement constant-table reflection and shader-object creation,
*  this translator lowers ARB assembly to GLSL source text and feeds it into
*  that existing pipeline.
*
*  Nothing here is game-specific: the whole ARB_vertex_program /
*  ARB_fragment_program core instruction set is translated the same way for
*  every caller.  Constructs that genuinely have no D3D9 / Shader Model 3
*  equivalent are reported through ARBTranslation::error so the caller can log
*  them and leave the program uncompiled — never silently accepted.
*
*********************************************************************************/

#ifndef ARB_ASM_TRANSLATOR_H
#define ARB_ASM_TRANSLATOR_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Program targets, matching GL_VERTEX_PROGRAM_ARB / GL_FRAGMENT_PROGRAM_ARB. */
#define ARB_TARGET_VERTEX       0
#define ARB_TARGET_FRAGMENT     1

/* Highest program.env[]/program.local[] index the translator will accept.
 *
 * 96 is the *guaranteed minimum* MAX_PROGRAM_ENV_PARAMETERS_ARB, not the value
 * real drivers report - NVIDIA and AMD both expose 256 - and programs written
 * against those drivers use the room. id Tech 4 addresses program.env[96]
 * directly, which the 96 limit rejected as out of range, failing the whole
 * program and leaving the renderer with nothing bound.
 *
 * 256 matches both what those drivers advertise and what vs_3_0 has room for
 * (c0-c255). ps_3_0 is smaller at 224, but the emitted uniform array is sized
 * from the indices a program actually touches rather than from this constant,
 * so a fragment program only pays for what it uses and one that genuinely
 * exceeds 224 fails in the compiler with a specific message instead of being
 * rejected here. */
#define ARB_MAX_PROGRAM_PARAMS  256

/* Space for the notes the translator accumulates about constructs it had to
 * approximate (an input the fixed vertex format cannot supply, for example).
 * These are warnings, not failures; the caller logs them verbatim. */
#define ARB_MAX_NOTES           2048
#define ARB_MAX_ERROR           256

typedef struct {
    int   target;               /* ARB_TARGET_*, taken from the !!ARB header  */
    int   envArraySize;         /* elements declared in arb_env[],  0 if none */
    int   localArraySize;       /* elements declared in arb_loc[],  0 if none */
    BOOL  usesStateMatrices;    /* program reads state.matrix.*               */
    BOOL  usesStateLight;       /* program reads state.light[n].* / material  */
    BOOL  usesStateFog;         /* program reads state.fog.*                  */
    BOOL  positionInvariant;    /* OPTION ARB_position_invariant was given    */
    char  error[ARB_MAX_ERROR]; /* set only when translation failed           */
    char  notes[ARB_MAX_NOTES]; /* newline-separated approximation warnings   */
} ARBTranslation;

/*
 * Translate one ARB assembly program to GLSL source text.
 *
 * `asmSource` is the exact text passed to glProgramStringARB (it does not have
 * to be NUL terminated inside `asmLen`).  `glslOut` receives GLSL suitable for
 * glslTranspileAndCompile().  `info` is always filled in; on failure it returns
 * FALSE with info->error describing precisely what could not be translated.
 */
BOOL arbTranslateProgram(const char *asmSource, int asmLen,
                         char *glslOut, int glslOutSize,
                         ARBTranslation *info);

/* Names the translator gives the uniform arrays holding program.env[] and
 * program.local[].  glProgramEnvParameter4fvARB resolves these through the
 * compiled shader's constant table to find the D3D9 register they landed on. */
#define ARB_ENV_UNIFORM_NAME    "arb_env"
#define ARB_LOCAL_UNIFORM_NAME  "arb_loc"

/* Uniform array holding the GL matrices a program reads through state.matrix.*
 * Four consecutive float4 rows per matrix; see ARB_STATE_MAT_* for the slots. */
#define ARB_STATE_UNIFORM_NAME  "arb_state"

/* Row-quads inside arb_state[].  Each entry is 4 registers wide. */
#define ARB_STATE_MAT_MVP           0   /* modelview * projection            */
#define ARB_STATE_MAT_MVP_INV       1
#define ARB_STATE_MAT_MVP_TRANS     2
#define ARB_STATE_MAT_MVP_INVTRANS  3
#define ARB_STATE_MAT_MV            4   /* modelview                         */
#define ARB_STATE_MAT_MV_INV        5
#define ARB_STATE_MAT_MV_TRANS      6
#define ARB_STATE_MAT_MV_INVTRANS   7
#define ARB_STATE_MAT_PROJ          8   /* projection                        */
#define ARB_STATE_MAT_PROJ_INV      9
#define ARB_STATE_MAT_PROJ_TRANS    10
#define ARB_STATE_MAT_PROJ_INVTRANS 11
#define ARB_STATE_MAT_TEX0          12  /* texture matrix, units 0..7        */
#define ARB_STATE_MAT_COUNT         20
#define ARB_STATE_MATRIX_REGS       (ARB_STATE_MAT_COUNT * 4)

/* Non-matrix GL state follows the matrices in the same array, one float4 each. */
#define ARB_STATE_LIGHT0_POS        (ARB_STATE_MATRIX_REGS + 0)   /* 8 lights */
#define ARB_STATE_LIGHT0_AMBIENT    (ARB_STATE_MATRIX_REGS + 8)
#define ARB_STATE_LIGHT0_DIFFUSE    (ARB_STATE_MATRIX_REGS + 16)
#define ARB_STATE_LIGHT0_SPECULAR   (ARB_STATE_MATRIX_REGS + 24)
#define ARB_STATE_MAT_F_AMBIENT     (ARB_STATE_MATRIX_REGS + 32)
#define ARB_STATE_MAT_F_DIFFUSE     (ARB_STATE_MATRIX_REGS + 33)
#define ARB_STATE_MAT_F_SPECULAR    (ARB_STATE_MATRIX_REGS + 34)
#define ARB_STATE_MAT_F_EMISSION    (ARB_STATE_MATRIX_REGS + 35)
#define ARB_STATE_MAT_F_SHININESS   (ARB_STATE_MATRIX_REGS + 36)
#define ARB_STATE_FOG_COLOR         (ARB_STATE_MATRIX_REGS + 37)
#define ARB_STATE_FOG_PARAMS        (ARB_STATE_MATRIX_REGS + 38)
#define ARB_STATE_LIGHTMODEL_AMB    (ARB_STATE_MATRIX_REGS + 39)
#define ARB_STATE_REG_COUNT         (ARB_STATE_MATRIX_REGS + 40)

#ifdef __cplusplus
}
#endif

#endif /* ARB_ASM_TRANSLATOR_H */
