/*********************************************************************************
*
*  arb_asm_translator.c - ARB assembly shader -> GLSL source translation.
*
*  See arb_asm_translator.h for why the output is GLSL text rather than D3D9
*  bytecode.  The shape of this file deliberately mirrors gldGenerateGLSL() in
*  shader_translator.c: parse into an opcode/register form, then emit C-like
*  shader text one instruction at a time.
*
*  Two details of the downstream transpiler (glsl_to_hlsl.c) drive the naming
*  choices here:
*
*   - Vertex inputs and interpolators get their HLSL semantic from the variable
*     name.  A name containing "_SEM_<SEMANTIC>" pins the semantic exactly,
*     which is what lets an ARB program's result.texcoord[3] and the matching
*     fragment.texcoord[3] land on the same interpolator.  Hand-written GLSL
*     never contains that marker, so real GLSL shaders are unaffected.
*
*   - The only vertex data the draw path assembles is GLS_D3DVertex, i.e.
*     position, normal, one diffuse colour, two texture coordinate sets, and
*     two further 4-component sets reserved for generic attributes 6 and 7.
*     Those last two are the only indices ARB_vertex_program leaves without a
*     conventional alias, so rather than inventing one they are backed by real
*     per-vertex data taken straight from the bound VAO.  ARB inputs outside
*     that set cannot be supplied at all; they are reported as notes and read
*     as (0,0,0,1) rather than being silently dropped or declared as a
*     vertex-shader input D3D9 has no data for.
*
*********************************************************************************/

#include "arb_asm_translator.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/*---------------------- Limits ----------------------*/

#define ARB_MAX_SYMBOLS     512
#define ARB_NAME_LEN        64
#define ARB_EXPR_LEN        160
#define ARB_MAX_TOKENS      64
#define ARB_BIG_EXPR        2048
#define ARB_TOKEN_LEN       128

/* Conventional vertex/fragment bindings, in declaration order. */
enum {
    ARB_IN_POSITION = 0,
    ARB_IN_WEIGHT,
    ARB_IN_NORMAL,
    ARB_IN_COLOR0,
    ARB_IN_COLOR1,
    ARB_IN_FOGCOORD,
    ARB_IN_GENERIC6,            /* vertex.attrib[6]: no conventional alias  */
    ARB_IN_GENERIC7,            /* vertex.attrib[7]: no conventional alias  */
    ARB_IN_TEXCOORD0,           /* .. +7 */
    ARB_IN_COUNT = ARB_IN_TEXCOORD0 + 8
};

enum {
    ARB_OUT_POSITION = 0,
    ARB_OUT_COLOR0,
    ARB_OUT_COLOR1,
    ARB_OUT_FOGCOORD,
    ARB_OUT_POINTSIZE,
    ARB_OUT_TEXCOORD0,          /* .. +7 */
    ARB_OUT_COUNT = ARB_OUT_TEXCOORD0 + 8
};

/* Symbol kinds. */
enum {
    ARBSYM_TEMP = 0,
    ARBSYM_ADDRESS,
    ARBSYM_SCALARLIKE,          /* ATTRIB/PARAM/OUTPUT alias: plain expression */
    ARBSYM_ARRAY                /* PARAM name[n] = { ... }                     */
};

typedef struct {
    char name[ARB_NAME_LEN];
    int  kind;
    char expr[ARB_EXPR_LEN];    /* GLSL expression / lvalue, or array base name */
    int  arrayBase;             /* first element index inside expr[]           */
    int  arrayLen;
} ARBSymbol;

typedef struct {
    int   target;
    ARBSymbol syms[ARB_MAX_SYMBOLS];
    int   symCount;

    char  temps[ARB_MAX_SYMBOLS][ARB_NAME_LEN];
    int   tempCount;
    char  addrs[ARB_MAX_SYMBOLS][ARB_NAME_LEN];
    int   addrCount;

    BOOL  inUsed[ARB_IN_COUNT];
    BOOL  outUsed[ARB_OUT_COUNT];
    BOOL  samplerUsed[8];

    int   maxEnv;               /* highest program.env[] index referenced, -1  */
    int   maxLocal;
    BOOL  stateUsed[ARB_STATE_REG_COUNT];

    BOOL  usesFragPosition;
    BOOL  ccUsed;
    BOOL  fragDepthWritten;
    int   maxFragData;          /* highest result.color[n] index + 1           */

    char *body;                 /* emitted statements                          */
    int   bodyLen;
    int   bodyCap;

    ARBTranslation *info;
    BOOL  failed;
} ARBCtx;


/*---------------------- Small helpers ----------------------*/

static void arbFail(ARBCtx *c, const char *fmt, ...)
{
    va_list ap;
    if (c->failed) return;              /* keep the first, most specific cause */
    c->failed = TRUE;
    va_start(ap, fmt);
    _vsnprintf(c->info->error, ARB_MAX_ERROR - 1, fmt, ap);
    va_end(ap);
    c->info->error[ARB_MAX_ERROR - 1] = '\0';
}

static void arbNote(ARBCtx *c, const char *fmt, ...)
{
    va_list ap;
    int used = (int)strlen(c->info->notes);
    int room = ARB_MAX_NOTES - used - 2;
    if (room <= 16) return;
    if (used > 0) { c->info->notes[used++] = '\n'; c->info->notes[used] = '\0'; room--; }
    va_start(ap, fmt);
    _vsnprintf(c->info->notes + used, room, fmt, ap);
    va_end(ap);
    c->info->notes[ARB_MAX_NOTES - 1] = '\0';
}

static void arbEmit(ARBCtx *c, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (c->failed) return;
    while (c->bodyLen + 16384 >= c->bodyCap) {
        int newCap = c->bodyCap * 2;
        char *nb = (char *)realloc(c->body, (size_t)newCap);
        if (!nb) { arbFail(c, "out of memory building GLSL body"); return; }
        c->body = nb;
        c->bodyCap = newCap;
    }
    va_start(ap, fmt);
    n = _vsnprintf(c->body + c->bodyLen, (size_t)(c->bodyCap - c->bodyLen - 1), fmt, ap);
    va_end(ap);
    if (n > 0) c->bodyLen += n;
    else       c->bodyLen = c->bodyCap - 1;      /* truncated: stop growing */
    c->body[c->bodyLen] = '\0';
}

/* Bounded append that never lets a truncated _vsnprintf corrupt the offset. */
static void arbAppend(char *buf, int *off, int size, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (*off >= size - 1) return;
    va_start(ap, fmt);
    n = _vsnprintf(buf + *off, (size_t)(size - *off - 1), fmt, ap);
    va_end(ap);
    if (n < 0) *off = size - 1;
    else       *off += n;
    buf[size - 1] = '\0';
}

static BOOL arbIsIdentChar(char ch)
{
    return (isalnum((unsigned char)ch) || ch == '_' || ch == '.' ||
            ch == '[' || ch == ']');
}

/* TRUE when every character is a swizzle/writemask letter. */
static BOOL arbIsSwizzleText(const char *s)
{
    int n = 0;
    if (!*s) return FALSE;
    for (; *s; s++, n++) {
        if (!strchr("xyzwrgba", *s)) return FALSE;
        if (n >= 4) return FALSE;
    }
    return TRUE;
}

/* Normalise an ARB swizzle/writemask (rgba spelling included) to xyzw. */
static void arbNormalizeSwizzle(const char *in, char *out)
{
    int i;
    for (i = 0; in[i] && i < 4; i++) {
        switch (in[i]) {
        case 'r': out[i] = 'x'; break;
        case 'g': out[i] = 'y'; break;
        case 'b': out[i] = 'z'; break;
        case 'a': out[i] = 'w'; break;
        default:  out[i] = in[i]; break;
        }
    }
    out[i] = '\0';
}

static ARBSymbol *arbFindSymbol(ARBCtx *c, const char *name)
{
    int i;
    for (i = 0; i < c->symCount; i++)
        if (strcmp(c->syms[i].name, name) == 0)
            return &c->syms[i];
    return NULL;
}

static ARBSymbol *arbAddSymbol(ARBCtx *c, const char *name, int kind)
{
    ARBSymbol *s;
    if (c->symCount >= ARB_MAX_SYMBOLS) {
        arbFail(c, "too many declarations (limit %d)", ARB_MAX_SYMBOLS);
        return NULL;
    }
    s = &c->syms[c->symCount++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, ARB_NAME_LEN - 1);
    s->kind = kind;
    return s;
}


/*---------------------- Binding name tables ----------------------*/

/* Defined with the interpolator names below; both name generators share it. */
static char *arbScratchName(void);

/*
 * GLSL variable names for the conventional vertex-program inputs.  The
 * "_SEM_" marker pins the HLSL semantic; see the file header.
 */
static const char *arbVertexInName(int slot)
{
    char *buf = arbScratchName();
    switch (slot) {
    case ARB_IN_POSITION: return "arb_in_SEM_POSITION";
    case ARB_IN_NORMAL:   return "arb_in_SEM_NORMAL";
    case ARB_IN_COLOR0:   return "arb_in_SEM_COLOR0";
    case ARB_IN_COLOR1:   return "arb_in_SEM_COLOR1";
    case ARB_IN_WEIGHT:   return "arb_in_weight";
    case ARB_IN_FOGCOORD: return "arb_in_fogcoord";
    /* The two generic attributes ride the spare texture coordinate sets the
     * vertex format carries for them.  The "generic6"/"generic7" infix keeps
     * these distinct from the conventional vertex.texcoord[2]/[3] names the
     * default branch below produces, which share the same HLSL semantic. */
    case ARB_IN_GENERIC6: return "arb_in_generic6_SEM_TEXCOORD2";
    case ARB_IN_GENERIC7: return "arb_in_generic7_SEM_TEXCOORD3";
    default:
        sprintf(buf, "arb_in_SEM_TEXCOORD%d", slot - ARB_IN_TEXCOORD0);
        return buf;
    }
}

/*
 * Scratch storage for the generated slot names below.
 *
 * These used to be one `static char buf[48]` per function, which is wrong two
 * ways.  A single buffer means two calls in the same expression - and the
 * emitters routinely name a source and a destination in one format string -
 * both return the same pointer, so the second name silently overwrites the
 * first and the emitted GLSL names the same slot twice.  It is also shared
 * across threads, and an application is free to translate programs from more
 * than one (id Tech 4 does once SMP is on), which lets two translations
 * interleave inside sprintf and produce a half-written name.
 *
 * Thread-local, and rotating so several live names can coexist in one
 * expression.  Still a bounded resource, but eight deep is far past what any
 * single format string here uses.
 */
#define ARB_SCRATCH_NAMES   8
#define ARB_SCRATCH_LEN     48

static char *arbScratchName(void)
{
    static __declspec(thread) char bufs[ARB_SCRATCH_NAMES][ARB_SCRATCH_LEN];
    static __declspec(thread) unsigned next = 0;
    char *p = bufs[next % ARB_SCRATCH_NAMES];
    next++;
    p[0] = '\0';
    return p;
}

/* Interpolators.  Vertex outputs and fragment inputs must agree exactly, so
 * both stages use this one table. */
static const char *arbVaryingName(int slot)
{
    char *buf = arbScratchName();
    switch (slot) {
    case ARB_OUT_COLOR0:   return "arb_v_SEM_COLOR0";
    case ARB_OUT_COLOR1:   return "arb_v_SEM_COLOR1";
    case ARB_OUT_FOGCOORD: return "arb_v_SEM_TEXCOORD8";
    default:
        sprintf(buf, "arb_v_SEM_TEXCOORD%d", slot - ARB_OUT_TEXCOORD0);
        return buf;
    }
}

/*
 * Which inputs GLS_D3DVertex actually carries.  Anything else has no data
 * behind it in this pipeline, so declaring it as a vertex-shader input would
 * make D3D9 reject the draw outright.
 *
 * Besides the conventional set, the vertex format carries two extra
 * 4-component texture coordinate sets reserved for generic attributes 6 and 7
 * — the only two indices ARB_vertex_program leaves without a conventional
 * alias, filled straight from the bound VAO by _glsResolveVertexSources.
 */
static BOOL arbVertexInAvailable(int slot)
{
    return (slot == ARB_IN_POSITION || slot == ARB_IN_NORMAL ||
            slot == ARB_IN_COLOR0 ||
            slot == ARB_IN_TEXCOORD0 || slot == ARB_IN_TEXCOORD0 + 1 ||
            slot == ARB_IN_GENERIC6 || slot == ARB_IN_GENERIC7);
}

static const char *arbInDescription(int slot)
{
    static char buf[48];
    switch (slot) {
    case ARB_IN_POSITION: return "vertex.position";
    case ARB_IN_NORMAL:   return "vertex.normal";
    case ARB_IN_COLOR0:   return "vertex.color.primary";
    case ARB_IN_COLOR1:   return "vertex.color.secondary";
    case ARB_IN_WEIGHT:   return "vertex.weight";
    case ARB_IN_FOGCOORD: return "vertex.fogcoord";
    case ARB_IN_GENERIC6: return "vertex.attrib[6]";
    case ARB_IN_GENERIC7: return "vertex.attrib[7]";
    default:
        sprintf(buf, "vertex.texcoord[%d]", slot - ARB_IN_TEXCOORD0);
        return buf;
    }
}


/*---------------------- Tokenizer ----------------------*/

/*
 * Statements are separated by ';'.  Inside one statement a token is either a
 * run of identifier characters (which keeps "vertex.texcoord[0]" and "R0.xyz"
 * whole), a braced constant vector, or a single punctuation character.
 */
static int arbTokenizeStatement(const char *stmt, char tokens[][ARB_TOKEN_LEN])
{
    int count = 0;
    const char *p = stmt;

    while (*p && count < ARB_MAX_TOKENS) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (*p == '{') {
            /* Braced initialiser: kept whole (nesting included, spaces dropped)
             * so a PARAM's operand list survives the comma splitting below. */
            int n = 0;
            int depth = 0;
            while (*p && n < ARB_TOKEN_LEN - 1) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                if (!isspace((unsigned char)*p)) tokens[count][n++] = *p;
                p++;
                if (depth == 0) break;
            }
            tokens[count][n] = '\0';
            count++;
            continue;
        }

        if (arbIsIdentChar(*p)) {
            int n = 0;
            int bracket = 0;
            /* Everything between [ and ] stays in the token so that relative
             * addressing ("bones[A0.x + 3]") survives as one operand. */
            while (*p && n < ARB_TOKEN_LEN - 1) {
                if (bracket > 0 && isspace((unsigned char)*p)) { p++; continue; }
                if (bracket == 0 && !arbIsIdentChar(*p)) break;
                if (*p == '[') bracket++;
                else if (*p == ']') bracket--;
                tokens[count][n++] = *p++;
            }
            tokens[count][n] = '\0';
            count++;
            continue;
        }

        tokens[count][0] = *p++;
        tokens[count][1] = '\0';
        count++;
    }

    return count;
}


/*---------------------- Source / destination resolution ----------------------*/

typedef struct {
    char expr[ARB_BIG_EXPR];        /* GLSL expression yielding a vec4 */
} ARBOperand;

static void arbMarkState(ARBCtx *c, int reg, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (reg + i >= 0 && reg + i < ARB_STATE_REG_COUNT)
            c->stateUsed[reg + i] = TRUE;
}

/* Parse "name[3]" into base name and index.  Returns -1 when not indexed. */
static int arbSplitIndex(const char *tok, char *baseOut, int baseSize)
{
    const char *lb = strchr(tok, '[');
    int n;
    if (!lb) {
        strncpy(baseOut, tok, baseSize - 1);
        baseOut[baseSize - 1] = '\0';
        return -1;
    }
    n = (int)(lb - tok);
    if (n >= baseSize) n = baseSize - 1;
    memcpy(baseOut, tok, n);
    baseOut[n] = '\0';
    return atoi(lb + 1);
}

/*
 * Resolve a state.matrix.* path to its slot in arb_state[].
 * Returns -1 when the path is not a matrix.
 */
static int arbStateMatrixSlot(const char *path, int *pRow)
{
    /* path looks like "matrix.mvp.inverse.row[2]" (leading "state." removed) */
    char work[ARB_TOKEN_LEN];
    char *rowPart;
    int base = -1;
    int variant = 0;

    strncpy(work, path, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    *pRow = -1;
    rowPart = strstr(work, ".row[");
    if (rowPart) {
        *pRow = atoi(rowPart + 5);
        *rowPart = '\0';
    }

    if (strncmp(work, "matrix.", 7) != 0) return -1;

    if      (strncmp(work + 7, "mvp", 3) == 0)        { base = ARB_STATE_MAT_MVP;  variant = 3; }
    else if (strncmp(work + 7, "modelview", 9) == 0)  { base = ARB_STATE_MAT_MV;   variant = 9; }
    else if (strncmp(work + 7, "projection", 10) == 0){ base = ARB_STATE_MAT_PROJ; variant = 10; }
    else if (strncmp(work + 7, "texture", 7) == 0) {
        const char *lb = strchr(work + 7, '[');
        int unit = lb ? atoi(lb + 1) : 0;
        if (unit < 0 || unit > 7) unit = 0;
        return ARB_STATE_MAT_TEX0 + unit;   /* no inverse/transpose variants kept */
    }
    else return -1;

    {
        const char *rest = work + 7 + variant;
        /* "modelview[0]" — only matrix 0 is tracked, which is all GL exposes
         * without ARB_vertex_blend. */
        if (*rest == '[') { const char *rb = strchr(rest, ']'); rest = rb ? rb + 1 : rest; }
        if      (strcmp(rest, ".inverse")   == 0) base += 1;
        else if (strcmp(rest, ".transpose") == 0) base += 2;
        else if (strcmp(rest, ".invtrans")  == 0) base += 3;
    }
    return base;
}

/*
 * Resolve one ARB register reference (no sign / absolute-value decoration) to
 * a GLSL expression that yields a vec4.  `swizzleOut` receives the trailing
 * swizzle, normalised to xyzw, or an empty string.
 */
static BOOL arbResolveRegister(ARBCtx *c, const char *tok, char *exprOut,
                               int exprSize, char *swizzleOut)
{
    char work[ARB_TOKEN_LEN];
    char *lastDot;
    char base[ARB_TOKEN_LEN];

    swizzleOut[0] = '\0';
    strncpy(work, tok, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    /* Constant vector or scalar literal. */
    if (work[0] == '{') {
        float v[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        int n = 0;
        char *p = work + 1;
        while (*p && *p != '}' && n < 4) {
            v[n++] = (float)atof(p);
            while (*p && *p != ',' && *p != '}') p++;
            if (*p == ',') p++;
        }
        if (n == 1) { v[1] = v[2] = v[3] = v[0]; }
        _snprintf(exprOut, exprSize - 1, "vec4(%g,%g,%g,%g)", v[0], v[1], v[2], v[3]);
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }
    if (isdigit((unsigned char)work[0]) ||
        (work[0] == '.' && isdigit((unsigned char)work[1]))) {
        _snprintf(exprOut, exprSize - 1, "vec4(%g)", atof(work));
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }

    /* Split off a trailing swizzle, but only when the suffix is made purely of
     * swizzle letters — "vertex.position" must not lose its "position". */
    lastDot = strrchr(work, '.');
    if (lastDot && arbIsSwizzleText(lastDot + 1)) {
        arbNormalizeSwizzle(lastDot + 1, swizzleOut);
        *lastDot = '\0';
    }

    /* Conventional bindings. */
    if (strncmp(work, "vertex.", 7) == 0) {
        int slot = -1;
        const char *b = work + 7;
        if (c->target != ARB_TARGET_VERTEX) {
            arbFail(c, "vertex.* binding used in a fragment program");
            return FALSE;
        }
        if      (strcmp(b, "position") == 0)          slot = ARB_IN_POSITION;
        else if (strcmp(b, "normal") == 0)            slot = ARB_IN_NORMAL;
        else if (strcmp(b, "color") == 0 ||
                 strcmp(b, "color.primary") == 0)     slot = ARB_IN_COLOR0;
        else if (strcmp(b, "color.secondary") == 0)   slot = ARB_IN_COLOR1;
        else if (strcmp(b, "fogcoord") == 0)          slot = ARB_IN_FOGCOORD;
        else if (strcmp(b, "weight") == 0)            slot = ARB_IN_WEIGHT;
        else if (strncmp(b, "texcoord", 8) == 0) {
            int unit = (b[8] == '[') ? atoi(b + 9) : 0;
            if (unit < 0 || unit > 7) unit = 0;
            slot = ARB_IN_TEXCOORD0 + unit;
        } else if (strncmp(b, "attrib[", 7) == 0) {
            /* Generic attributes alias the conventional ones, matching the
             * aliasing the draw path already uses (_glsResolveVertexSources).
             * Indices 6 and 7 are the only two ARB_vertex_program leaves with
             * no conventional alias, so they get slots of their own instead. */
            int idx = atoi(b + 7);
            switch (idx) {
            case 0:  slot = ARB_IN_POSITION; break;
            case 1:  slot = ARB_IN_WEIGHT;   break;
            case 2:  slot = ARB_IN_NORMAL;   break;
            case 3:  slot = ARB_IN_COLOR0;   break;
            case 4:  slot = ARB_IN_COLOR1;   break;
            case 5:  slot = ARB_IN_FOGCOORD; break;
            case 6:  slot = ARB_IN_GENERIC6; break;
            case 7:  slot = ARB_IN_GENERIC7; break;
            default:
                if (idx >= 8 && idx <= 15) slot = ARB_IN_TEXCOORD0 + (idx - 8);
                break;
            }
            if (slot < 0) {
                arbFail(c, "vertex.attrib[%d] has no conventional binding", idx);
                return FALSE;
            }
        }
        if (slot < 0) { arbFail(c, "unknown vertex binding '%s'", work); return FALSE; }
        c->inUsed[slot] = TRUE;
        strncpy(exprOut, arbVertexInName(slot), exprSize - 1);
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }

    if (strncmp(work, "fragment.", 9) == 0) {
        const char *b = work + 9;
        int slot = -1;
        if (c->target != ARB_TARGET_FRAGMENT) {
            arbFail(c, "fragment.* binding used in a vertex program");
            return FALSE;
        }
        if (strcmp(b, "position") == 0) {
            c->usesFragPosition = TRUE;
            strncpy(exprOut, "gl_FragCoord", exprSize - 1);
            exprOut[exprSize - 1] = '\0';
            return TRUE;
        }
        if      (strcmp(b, "color") == 0 ||
                 strcmp(b, "color.primary") == 0)   slot = ARB_OUT_COLOR0;
        else if (strcmp(b, "color.secondary") == 0) slot = ARB_OUT_COLOR1;
        else if (strcmp(b, "fogcoord") == 0)        slot = ARB_OUT_FOGCOORD;
        else if (strncmp(b, "texcoord", 8) == 0) {
            int unit = (b[8] == '[') ? atoi(b + 9) : 0;
            if (unit < 0 || unit > 7) unit = 0;
            slot = ARB_OUT_TEXCOORD0 + unit;
        }
        if (slot < 0) { arbFail(c, "unknown fragment binding '%s'", work); return FALSE; }
        c->outUsed[slot] = TRUE;
        strncpy(exprOut, arbVaryingName(slot), exprSize - 1);
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }

    if (strncmp(work, "program.env[", 12) == 0 ||
        strncmp(work, "program.local[", 14) == 0) {
        BOOL isEnv = (work[8] == 'e');
        int idx = atoi(strchr(work, '[') + 1);
        if (idx < 0 || idx >= ARB_MAX_PROGRAM_PARAMS) {
            arbFail(c, "program.%s[%d] out of range", isEnv ? "env" : "local", idx);
            return FALSE;
        }
        if (isEnv) { if (idx > c->maxEnv)   c->maxEnv = idx; }
        else       { if (idx > c->maxLocal) c->maxLocal = idx; }
        _snprintf(exprOut, exprSize - 1, "%s[%d]",
                  isEnv ? ARB_ENV_UNIFORM_NAME : ARB_LOCAL_UNIFORM_NAME, idx);
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }

    if (strncmp(work, "state.", 6) == 0) {
        int row = -1;
        int slot = arbStateMatrixSlot(work + 6, &row);
        if (slot >= 0) {
            int reg = slot * 4;
            c->info->usesStateMatrices = TRUE;
            if (row >= 0 && row <= 3) {
                arbMarkState(c, reg + row, 1);
                _snprintf(exprOut, exprSize - 1, "%s[%d]", ARB_STATE_UNIFORM_NAME, reg + row);
            } else {
                /* Bare matrix reference: only meaningful through a PARAM array,
                 * which arbDeclParam handles.  Fall back to row 0. */
                arbMarkState(c, reg, 4);
                _snprintf(exprOut, exprSize - 1, "%s[%d]", ARB_STATE_UNIFORM_NAME, reg);
            }
            exprOut[exprSize - 1] = '\0';
            return TRUE;
        }
        {
            const char *b = work + 6;
            int reg = -1;
            if (strncmp(b, "light[", 6) == 0) {
                int li = atoi(b + 6);
                const char *dot = strchr(b, ']');
                const char *what = dot ? dot + 1 : "";
                if (li < 0 || li > 7) li = 0;
                c->info->usesStateLight = TRUE;
                if      (strcmp(what, ".position") == 0) reg = ARB_STATE_LIGHT0_POS + li;
                else if (strcmp(what, ".ambient")  == 0) reg = ARB_STATE_LIGHT0_AMBIENT + li;
                else if (strcmp(what, ".diffuse")  == 0) reg = ARB_STATE_LIGHT0_DIFFUSE + li;
                else if (strcmp(what, ".specular") == 0) reg = ARB_STATE_LIGHT0_SPECULAR + li;
            } else if (strncmp(b, "material", 8) == 0) {
                const char *what = strrchr(b, '.');
                c->info->usesStateLight = TRUE;
                if (what) {
                    if      (strcmp(what, ".ambient")   == 0) reg = ARB_STATE_MAT_F_AMBIENT;
                    else if (strcmp(what, ".diffuse")   == 0) reg = ARB_STATE_MAT_F_DIFFUSE;
                    else if (strcmp(what, ".specular")  == 0) reg = ARB_STATE_MAT_F_SPECULAR;
                    else if (strcmp(what, ".emission")  == 0) reg = ARB_STATE_MAT_F_EMISSION;
                    else if (strcmp(what, ".shininess") == 0) reg = ARB_STATE_MAT_F_SHININESS;
                }
            } else if (strcmp(b, "fog.color") == 0) {
                c->info->usesStateFog = TRUE; reg = ARB_STATE_FOG_COLOR;
            } else if (strcmp(b, "fog.params") == 0) {
                c->info->usesStateFog = TRUE; reg = ARB_STATE_FOG_PARAMS;
            } else if (strcmp(b, "lightmodel.ambient") == 0) {
                c->info->usesStateLight = TRUE; reg = ARB_STATE_LIGHTMODEL_AMB;
            }
            if (reg < 0) {
                arbFail(c, "state binding '%s' has no tracked GL state equivalent", work);
                return FALSE;
            }
            arbMarkState(c, reg, 1);
            _snprintf(exprOut, exprSize - 1, "%s[%d]", ARB_STATE_UNIFORM_NAME, reg);
            exprOut[exprSize - 1] = '\0';
            return TRUE;
        }
    }

    /* Declared symbol, possibly indexed (PARAM arrays). */
    {
        int idx;
        ARBSymbol *sym;

        /* A PARAM array of literal vectors registers one symbol per element
         * under its bracketed name, so try the exact spelling first. */
        sym = arbFindSymbol(c, work);
        if (sym && sym->kind != ARBSYM_ARRAY) {
            strncpy(exprOut, sym->expr, exprSize - 1);
            exprOut[exprSize - 1] = '\0';
            return TRUE;
        }

        idx = arbSplitIndex(work, base, sizeof(base));
        sym = arbFindSymbol(c, base);
        if (!sym) {
            arbFail(c, "undeclared identifier '%s'", base);
            return FALSE;
        }
        if (sym->kind == ARBSYM_ARRAY) {
            /*
             * Relative addressing: "bones[A0.x + 3]".  The offset is clamped
             * to the declared array so a stray address register cannot walk
             * off into a neighbouring program's constant registers.
             */
            const char *lb = strchr(work, '[');
            if (lb && !isdigit((unsigned char)lb[1])) {
                char addrName[ARB_NAME_LEN];
                const char *plus;
                ARBSymbol *addr;
                int bias = 0;
                int n = 0;

                lb++;
                while (*lb && *lb != '.' && *lb != '+' && *lb != '-' && *lb != ']' &&
                       n < ARB_NAME_LEN - 1)
                    addrName[n++] = *lb++;
                addrName[n] = '\0';
                addr = arbFindSymbol(c, addrName);
                if (!addr || addr->kind != ARBSYM_ADDRESS) {
                    arbFail(c, "'%s' is not an ADDRESS register", addrName);
                    return FALSE;
                }
                plus = strpbrk(lb, "+-");
                if (plus) bias = atoi(plus);

                if (strcmp(sym->expr, ARB_ENV_UNIFORM_NAME) == 0) {
                    if (sym->arrayBase + sym->arrayLen - 1 > c->maxEnv)
                        c->maxEnv = sym->arrayBase + sym->arrayLen - 1;
                } else if (strcmp(sym->expr, ARB_LOCAL_UNIFORM_NAME) == 0) {
                    if (sym->arrayBase + sym->arrayLen - 1 > c->maxLocal)
                        c->maxLocal = sym->arrayBase + sym->arrayLen - 1;
                } else if (strcmp(sym->expr, ARB_STATE_UNIFORM_NAME) == 0) {
                    arbMarkState(c, sym->arrayBase, sym->arrayLen);
                }

                _snprintf(exprOut, exprSize - 1, "%s[clamp(%s + %d, %d, %d)]",
                          sym->expr, addr->expr, sym->arrayBase + bias,
                          sym->arrayBase, sym->arrayBase + sym->arrayLen - 1);
                exprOut[exprSize - 1] = '\0';
                return TRUE;
            }

            if (idx < 0) idx = 0;
            if (idx >= sym->arrayLen) {
                arbFail(c, "index %d past the end of '%s[%d]'", idx, base, sym->arrayLen);
                return FALSE;
            }
            if (strcmp(sym->expr, ARB_ENV_UNIFORM_NAME) == 0) {
                if (sym->arrayBase + idx > c->maxEnv) c->maxEnv = sym->arrayBase + idx;
            } else if (strcmp(sym->expr, ARB_LOCAL_UNIFORM_NAME) == 0) {
                if (sym->arrayBase + idx > c->maxLocal) c->maxLocal = sym->arrayBase + idx;
            } else if (strcmp(sym->expr, ARB_STATE_UNIFORM_NAME) == 0) {
                arbMarkState(c, sym->arrayBase + idx, 1);
            }
            _snprintf(exprOut, exprSize - 1, "%s[%d]", sym->expr, sym->arrayBase + idx);
            exprOut[exprSize - 1] = '\0';
            return TRUE;
        }
        strncpy(exprOut, sym->expr, exprSize - 1);
        exprOut[exprSize - 1] = '\0';
        return TRUE;
    }
}

/*
 * Resolve one source operand, applying negation, absolute value and swizzle.
 * `tokens` is the operand's token run.
 */
static BOOL arbResolveSource(ARBCtx *c, char tokens[][ARB_TOKEN_LEN],
                             int first, int last, ARBOperand *out)
{
    BOOL negate = FALSE, absolute = FALSE;
    char raw[ARB_EXPR_LEN];
    char swizzle[8];
    int i = first;

    while (i <= last && (tokens[i][0] == '-' || tokens[i][0] == '+' ||
                         tokens[i][0] == '|')) {
        if (tokens[i][0] == '-') negate = !negate;
        if (tokens[i][0] == '|') absolute = TRUE;
        i++;
    }
    if (i > last) { arbFail(c, "malformed source operand"); return FALSE; }

    if (!arbResolveRegister(c, tokens[i], raw, sizeof(raw), swizzle))
        return FALSE;

    /* A one-letter swizzle is replicated across all four components. */
    if (swizzle[0] && !swizzle[1]) {
        char ch = swizzle[0];
        swizzle[0] = ch; swizzle[1] = ch; swizzle[2] = ch; swizzle[3] = ch;
        swizzle[4] = '\0';
    }

    _snprintf(out->expr, sizeof(out->expr) - 1, "%s%s(%s)%s%s",
              negate ? "-" : "",
              absolute ? "abs" : "",
              raw,
              swizzle[0] ? "." : "",
              swizzle);
    out->expr[sizeof(out->expr) - 1] = '\0';
    return TRUE;
}

/* Destination: an lvalue, a writemask and any condition-code decoration. */
typedef struct {
    char lvalue[ARB_EXPR_LEN];
    char mask[8];               /* "" means all four components */
    char predicate[96];         /* "" means write unconditionally */
    BOOL predicateAlwaysFalse;
    BOOL updateCC;
    BOOL isFragData;
    int  fragDataIndex;
} ARBDest;

static BOOL arbResolveDest(ARBCtx *c, const char *tok, ARBDest *out)
{
    char work[ARB_TOKEN_LEN];
    char *lastDot;

    memset(out, 0, sizeof(*out));
    out->fragDataIndex = -1;
    strncpy(work, tok, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    lastDot = strrchr(work, '.');
    if (lastDot && arbIsSwizzleText(lastDot + 1)) {
        arbNormalizeSwizzle(lastDot + 1, out->mask);
        *lastDot = '\0';
    }

    if (strncmp(work, "result.", 7) == 0) {
        const char *b = work + 7;
        if (c->target == ARB_TARGET_VERTEX) {
            int slot = -1;
            if (strcmp(b, "position") == 0) {
                c->outUsed[ARB_OUT_POSITION] = TRUE;
                strcpy(out->lvalue, "gl_Position");
                return TRUE;
            }
            if (strcmp(b, "pointsize") == 0) {
                c->outUsed[ARB_OUT_POINTSIZE] = TRUE;
                strcpy(out->lvalue, "gl_PointSize");
                if (!out->mask[0]) strcpy(out->mask, "x");
                return TRUE;
            }
            if      (strcmp(b, "color") == 0 ||
                     strcmp(b, "color.primary") == 0 ||
                     strcmp(b, "color.front") == 0 ||
                     strcmp(b, "color.front.primary") == 0)   slot = ARB_OUT_COLOR0;
            else if (strcmp(b, "color.secondary") == 0 ||
                     strcmp(b, "color.front.secondary") == 0) slot = ARB_OUT_COLOR1;
            else if (strncmp(b, "color.back", 10) == 0) {
                arbNote(c, "result.color.back has no D3D9 equivalent "
                           "(two-sided lighting output), write discarded");
                strcpy(out->lvalue, "arb_discard");
                return TRUE;
            }
            else if (strcmp(b, "fogcoord") == 0)              slot = ARB_OUT_FOGCOORD;
            else if (strncmp(b, "texcoord", 8) == 0) {
                int unit = (b[8] == '[') ? atoi(b + 9) : 0;
                if (unit < 0 || unit > 7) unit = 0;
                slot = ARB_OUT_TEXCOORD0 + unit;
            }
            if (slot < 0) { arbFail(c, "unknown vertex result '%s'", work); return FALSE; }
            c->outUsed[slot] = TRUE;
            strncpy(out->lvalue, arbVaryingName(slot), sizeof(out->lvalue) - 1);
            return TRUE;
        } else {
            if (strcmp(b, "depth") == 0) {
                /* SM3 can write oDepth, but the HLSL wrapper glsl_to_hlsl.c
                 * builds has no depth output, so the value would be silently
                 * dropped.  Refuse rather than pretend. */
                arbFail(c, "result.depth (fragment depth output) is not "
                           "supported by the Shader Model 3 back end");
                return FALSE;
            }
            if (strncmp(b, "color", 5) == 0) {
                int idx = (b[5] == '[') ? atoi(b + 6) : 0;
                if (idx < 0 || idx > 3) idx = 0;
                if (idx + 1 > c->maxFragData) c->maxFragData = idx + 1;
                if (b[5] == '[') {
                    out->isFragData = TRUE;
                    out->fragDataIndex = idx;
                    sprintf(out->lvalue, "gl_FragData[%d]", idx);
                } else {
                    strcpy(out->lvalue, "gl_FragColor");
                }
                return TRUE;
            }
            arbFail(c, "unknown fragment result '%s'", work);
            return FALSE;
        }
    }

    {
        char base[ARB_TOKEN_LEN];
        int idx = arbSplitIndex(work, base, sizeof(base));
        ARBSymbol *sym = arbFindSymbol(c, base);
        (void)idx;
        if (!sym) { arbFail(c, "undeclared destination '%s'", base); return FALSE; }
        if (sym->kind == ARBSYM_ARRAY) {
            arbFail(c, "'%s' is a PARAM array and cannot be written", base);
            return FALSE;
        }
        strncpy(out->lvalue, sym->expr, sizeof(out->lvalue) - 1);
        out->lvalue[sizeof(out->lvalue) - 1] = '\0';
        return TRUE;
    }
}


/*---------------------- Declarations ----------------------*/

static BOOL arbDeclTemp(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count)
{
    int i;
    for (i = 1; i < count; i++) {
        ARBSymbol *sym;
        if (tokens[i][0] == ',') continue;
        if (c->tempCount >= ARB_MAX_SYMBOLS) { arbFail(c, "too many TEMP registers"); return FALSE; }
        sym = arbAddSymbol(c, tokens[i], ARBSYM_TEMP);
        if (!sym) return FALSE;
        _snprintf(sym->expr, ARB_EXPR_LEN - 1, "arb_t_%s", tokens[i]);
        strncpy(c->temps[c->tempCount++], sym->expr, ARB_NAME_LEN - 1);
    }
    return TRUE;
}

static BOOL arbDeclAddress(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count)
{
    int i;
    for (i = 1; i < count; i++) {
        ARBSymbol *sym;
        if (tokens[i][0] == ',') continue;
        if (c->addrCount >= ARB_MAX_SYMBOLS) { arbFail(c, "too many ADDRESS registers"); return FALSE; }
        sym = arbAddSymbol(c, tokens[i], ARBSYM_ADDRESS);
        if (!sym) return FALSE;
        _snprintf(sym->expr, ARB_EXPR_LEN - 1, "arb_a_%s", tokens[i]);
        strncpy(c->addrs[c->addrCount++], sym->expr, ARB_NAME_LEN - 1);
    }
    return TRUE;
}

/* ATTRIB name = <binding>;  /  OUTPUT name = <binding>;  /  ALIAS a = b; */
static BOOL arbDeclAlias(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count, BOOL isOutput)
{
    ARBSymbol *sym;
    char expr[ARB_EXPR_LEN];
    char swizzle[8];

    if (count < 4 || tokens[2][0] != '=') {
        arbFail(c, "malformed %s declaration", isOutput ? "OUTPUT" : "ATTRIB");
        return FALSE;
    }

    if (isOutput) {
        ARBDest d;
        if (!arbResolveDest(c, tokens[3], &d)) return FALSE;
        sym = arbAddSymbol(c, tokens[1], ARBSYM_SCALARLIKE);
        if (!sym) return FALSE;
        strncpy(sym->expr, d.lvalue, ARB_EXPR_LEN - 1);
        return TRUE;
    }

    if (!arbResolveRegister(c, tokens[3], expr, sizeof(expr), swizzle))
        return FALSE;
    sym = arbAddSymbol(c, tokens[1], ARBSYM_SCALARLIKE);
    if (!sym) return FALSE;
    if (swizzle[0])
        _snprintf(sym->expr, ARB_EXPR_LEN - 1, "(%s).%s", expr, swizzle);
    else
        strncpy(sym->expr, expr, ARB_EXPR_LEN - 1);
    sym->expr[ARB_EXPR_LEN - 1] = '\0';
    return TRUE;
}

/*
 * PARAM name = <item>;
 * PARAM name[n] = { <item>, <item>, ... };
 *
 * The array form is what carries state.matrix.mvp and program.env ranges, so
 * it maps onto one of the uniform arrays rather than becoming a local.
 */
static BOOL arbDeclParam(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count)
{
    char base[ARB_NAME_LEN];
    int declLen;
    int eq = -1, i;
    ARBSymbol *sym;

    if (count < 4) { arbFail(c, "malformed PARAM declaration"); return FALSE; }
    declLen = arbSplitIndex(tokens[1], base, sizeof(base));
    for (i = 2; i < count; i++) if (tokens[i][0] == '=') { eq = i; break; }
    if (eq < 0 || eq + 1 >= count) { arbFail(c, "PARAM '%s' has no initialiser", base); return FALSE; }

    /* Array form: PARAM m[4] = { state.matrix.mvp }; */
    if (declLen >= 0 || tokens[eq + 1][0] == '{' ||
        (eq + 2 < count && tokens[eq + 2][0] == ',')) {
        char item[ARB_TOKEN_LEN];
        char arrayName[ARB_NAME_LEN];
        int arrayBase = -1, arrayLen = 0;
        int row;
        int slot;

        /* Grab the first initialiser item; ranges "a..b" and matrix names both
         * start there.  Multiple heterogeneous items are not representable as
         * one contiguous constant range. */
        strncpy(item, tokens[eq + 1], sizeof(item) - 1);
        item[sizeof(item) - 1] = '\0';

        if (item[0] == '{') {
            char inner[ARB_TOKEN_LEN];
            int len;
            strncpy(inner, item + 1, sizeof(inner) - 1);
            inner[sizeof(inner) - 1] = '\0';
            len = (int)strlen(inner);
            if (len > 0 && inner[len - 1] == '}') inner[len - 1] = '\0';

            if (inner[0] == '{') {
                /* { {..}, {..} } — an array of literal vectors.  Each element
                 * becomes its own symbol under the bracketed spelling, which
                 * is how constant-indexed reads resolve. */
                char *p = inner;
                int elem = 0;
                while (*p == '{') {
                    char one[ARB_TOKEN_LEN];
                    char expr[ARB_EXPR_LEN], sw[8];
                    char elemName[ARB_NAME_LEN];
                    int n = 0;
                    while (*p && *p != '}' && n < (int)sizeof(one) - 2) one[n++] = *p++;
                    if (*p == '}') { one[n++] = '}'; p++; }
                    one[n] = '\0';
                    if (*p == ',') p++;
                    if (!arbResolveRegister(c, one, expr, sizeof(expr), sw)) return FALSE;
                    sprintf(elemName, "%.40s[%d]", base, elem);
                    sym = arbAddSymbol(c, elemName, ARBSYM_SCALARLIKE);
                    if (!sym) return FALSE;
                    strncpy(sym->expr, expr, ARB_EXPR_LEN - 1);
                    elem++;
                    if (elem >= 64) break;
                }
                /* Element 0 doubles as the unindexed spelling. */
                sym = arbAddSymbol(c, base, ARBSYM_SCALARLIKE);
                if (!sym) return FALSE;
                {
                    char elemName[ARB_NAME_LEN];
                    ARBSymbol *first;
                    sprintf(elemName, "%.40s[0]", base);
                    first = arbFindSymbol(c, elemName);
                    strncpy(sym->expr, first ? first->expr : "vec4(0.0,0.0,0.0,1.0)",
                            ARB_EXPR_LEN - 1);
                }
                return TRUE;
            }

            if (isdigit((unsigned char)inner[0]) || inner[0] == '-' || inner[0] == '.') {
                /* {a,b,c,d} literal vector used as a 1-element array */
                char expr[ARB_EXPR_LEN], sw[8];
                if (!arbResolveRegister(c, item, expr, sizeof(expr), sw)) return FALSE;
                sym = arbAddSymbol(c, base, ARBSYM_SCALARLIKE);
                if (!sym) return FALSE;
                strncpy(sym->expr, expr, ARB_EXPR_LEN - 1);
                return TRUE;
            }

            /* A braced binding list.  Only one contiguous binding can be
             * expressed as a constant-register range. */
            if (strchr(inner, ',')) {
                arbFail(c, "PARAM '%s': an array initialised from several "
                           "different bindings is not supported", base);
                return FALSE;
            }
            strncpy(item, inner, sizeof(item) - 1);
            item[sizeof(item) - 1] = '\0';
        }

        if (strncmp(item, "state.", 6) == 0) {
            slot = arbStateMatrixSlot(item + 6, &row);
            if (slot < 0) { arbFail(c, "PARAM array from '%s' is not a matrix", item); return FALSE; }
            c->info->usesStateMatrices = TRUE;
            strcpy(arrayName, ARB_STATE_UNIFORM_NAME);
            arrayBase = slot * 4;
            arrayLen = (declLen > 0 && declLen < 4) ? declLen : 4;
            arbMarkState(c, arrayBase, arrayLen);
        } else if (strncmp(item, "program.env[", 12) == 0 ||
                   strncmp(item, "program.local[", 14) == 0) {
            BOOL isEnv = (item[8] == 'e');
            const char *lb = strchr(item, '[');
            int lo = atoi(lb + 1);
            const char *dots = strstr(lb, "..");
            int hi = dots ? atoi(dots + 2) : lo;
            if (lo < 0) lo = 0;
            if (hi < lo) hi = lo;
            if (hi >= ARB_MAX_PROGRAM_PARAMS) hi = ARB_MAX_PROGRAM_PARAMS - 1;
            strcpy(arrayName, isEnv ? ARB_ENV_UNIFORM_NAME : ARB_LOCAL_UNIFORM_NAME);
            arrayBase = lo;
            arrayLen = hi - lo + 1;
            if (isEnv) { if (hi > c->maxEnv)   c->maxEnv   = hi; }
            else       { if (hi > c->maxLocal) c->maxLocal = hi; }
        } else {
            arbFail(c, "PARAM array initialiser '%s' is not a supported binding", item);
            return FALSE;
        }

        sym = arbAddSymbol(c, base, ARBSYM_ARRAY);
        if (!sym) return FALSE;
        strncpy(sym->expr, arrayName, ARB_EXPR_LEN - 1);
        sym->arrayBase = arrayBase;
        sym->arrayLen  = (declLen > 0) ? declLen : arrayLen;
        if (sym->arrayLen > arrayLen) sym->arrayLen = arrayLen;
        return TRUE;
    }

    /* Scalar form. */
    {
        char expr[ARB_EXPR_LEN], sw[8];
        if (!arbResolveRegister(c, tokens[eq + 1], expr, sizeof(expr), sw)) return FALSE;
        sym = arbAddSymbol(c, base, ARBSYM_SCALARLIKE);
        if (!sym) return FALSE;
        if (sw[0]) _snprintf(sym->expr, ARB_EXPR_LEN - 1, "(%s).%s", expr, sw);
        else       strncpy(sym->expr, expr, ARB_EXPR_LEN - 1);
        sym->expr[ARB_EXPR_LEN - 1] = '\0';
        return TRUE;
    }
}

static BOOL arbDeclOption(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count)
{
    if (count < 2) { arbFail(c, "OPTION with no name"); return FALSE; }

    if (strcmp(tokens[1], "ARB_position_invariant") == 0) {
        c->info->positionInvariant = TRUE;
        c->info->usesStateMatrices = TRUE;
        c->inUsed[ARB_IN_POSITION] = TRUE;
        arbMarkState(c, ARB_STATE_MAT_MVP * 4, 4);
        return TRUE;
    }
    if (strcmp(tokens[1], "ARB_precision_hint_fastest") == 0 ||
        strcmp(tokens[1], "ARB_precision_hint_nicest") == 0) {
        /* Precision hints carry no observable semantics; ignoring them is
         * exactly what the specification permits. */
        return TRUE;
    }
    if (strcmp(tokens[1], "ARB_fog_exp") == 0 ||
        strcmp(tokens[1], "ARB_fog_exp2") == 0 ||
        strcmp(tokens[1], "ARB_fog_linear") == 0) {
        arbNote(c, "OPTION %s: fog applied by fixed-function D3D9 state, "
                   "not folded into the program", tokens[1]);
        return TRUE;
    }
    if (strcmp(tokens[1], "ARB_draw_buffers") == 0)
        return TRUE;

    arbFail(c, "OPTION %s is not supported", tokens[1]);
    return FALSE;
}


/*---------------------- Instructions ----------------------*/

/* Every opcode in the ARB_vertex_program / ARB_fragment_program core sets. */
static BOOL arbOpcodeKnown(const char *op)
{
    static const char *kOps[] = {
        "ABS","ADD","ARL","CMP","COS","DP3","DP4","DPH","DST","EX2","EXP",
        "FLR","FRC","KIL","LG2","LIT","LOG","LRP","MAD","MAX","MIN","MOV",
        "MUL","POW","RCP","RSQ","SCS","SGE","SIN","SLT","SUB","SWZ","TEX",
        "TXB","TXP","XPD", NULL
    };
    int i;
    for (i = 0; kOps[i]; i++)
        if (strcmp(kOps[i], op) == 0) return TRUE;
    return FALSE;
}

/*
 * Parse a destination operand including its condition-code decoration:
 *
 *   R0.xy (GT.x)
 *
 * ARB has one condition-code register, written by the "C" opcode suffix and
 * read by any later predicated write — tracking it as a single vec4 makes the
 * distance between the two irrelevant.  A second condition-code register is an
 * NV extension and is refused.
 */
static BOOL arbParseDest(ARBCtx *c, char tokens[][ARB_TOKEN_LEN],
                         int first, int last, BOOL updateCC, ARBDest *out)
{
    int i;

    if (!arbResolveDest(c, tokens[first], out)) return FALSE;
    out->updateCC = updateCC;
    if (updateCC) c->ccUsed = TRUE;

    for (i = first + 1; i <= last; i++) {
        if (tokens[i][0] != '(') continue;
        if (i + 1 > last) { arbFail(c, "empty condition code test"); return FALSE; }
        {
            char cond[ARB_TOKEN_LEN];
            char swizzle[8] = "xyzw";
            char *dot;
            const char *cmp = NULL;

            strncpy(cond, tokens[i + 1], sizeof(cond) - 1);
            cond[sizeof(cond) - 1] = '\0';
            dot = strchr(cond, '.');
            if (dot) {
                char norm[8];
                arbNormalizeSwizzle(dot + 1, norm);
                if (norm[0] && !norm[1]) {
                    swizzle[0] = swizzle[1] = swizzle[2] = swizzle[3] = norm[0];
                    swizzle[4] = '\0';
                } else {
                    strcpy(swizzle, norm);
                }
                *dot = '\0';
            }

            if      (!strcmp(cond, "EQ")) cmp = "==";
            else if (!strcmp(cond, "NE") || !strcmp(cond, "NEQ")) cmp = "!=";
            else if (!strcmp(cond, "LT")) cmp = "<";
            else if (!strcmp(cond, "GE")) cmp = ">=";
            else if (!strcmp(cond, "LE")) cmp = "<=";
            else if (!strcmp(cond, "GT")) cmp = ">";
            else if (!strcmp(cond, "TR")) return TRUE;          /* always writes */
            else if (!strcmp(cond, "FL")) { out->predicateAlwaysFalse = TRUE; return TRUE; }
            else {
                arbFail(c, "condition code test '%s' is not part of the ARB core set", cond);
                return FALSE;
            }

            c->ccUsed = TRUE;
            _snprintf(out->predicate, sizeof(out->predicate) - 1,
                      "arb_cc.%s %s 0.0", swizzle, cmp);
            out->predicate[sizeof(out->predicate) - 1] = '\0';
        }
        return TRUE;
    }
    return TRUE;
}

/*
 * Write `expr` (a vec4-valued expression) into `d`, honouring its writemask,
 * its optional condition-code predicate, and any request to update the
 * condition code from the result.
 */
static void arbStore(ARBCtx *c, const ARBDest *d, const char *expr, BOOL saturate)
{
    char value[ARB_BIG_EXPR * 2];
    const char *mask = (d->mask[0] && strcmp(d->mask, "xyzw") != 0) ? d->mask : NULL;

    if (strcmp(d->lvalue, "arb_discard") == 0)
        return;                                     /* already reported as a note */

    if (d->predicateAlwaysFalse)
        return;                                     /* the write cannot happen */

    if (saturate)
        _snprintf(value, sizeof(value) - 1, "clamp(%s, 0.0, 1.0)", expr);
    else
        _snprintf(value, sizeof(value) - 1, "%s", expr);
    value[sizeof(value) - 1] = '\0';

    if (d->predicate[0]) {
        /* Component-wise select: only the components the condition code
         * admits are replaced. */
        if (mask)
            arbEmit(c, "    %s.%s = ((%s) ? (%s) : (%s)).%s;\n",
                    d->lvalue, mask, d->predicate, value, d->lvalue, mask);
        else
            arbEmit(c, "    %s = ((%s) ? (%s) : (%s));\n",
                    d->lvalue, d->predicate, value, d->lvalue);
    } else if (mask) {
        arbEmit(c, "    %s.%s = (%s).%s;\n", d->lvalue, mask, value, mask);
    } else {
        arbEmit(c, "    %s = %s;\n", d->lvalue, value);
    }

    if (d->updateCC) {
        if (mask) arbEmit(c, "    arb_cc.%s = (%s).%s;\n", mask, d->lvalue, mask);
        else      arbEmit(c, "    arb_cc = %s;\n", d->lvalue);
    }
}

/* Texture target keyword -> GLSL sampling call around `coord`. */
static BOOL arbEmitTexture(ARBCtx *c, const char *texTarget, int unit,
                           const char *coord, const char *mode, char *out, int outSize)
{
    char samp[32];
    sprintf(samp, "arb_tex%d", unit);

    if (strcmp(texTarget, "2D") == 0 || strcmp(texTarget, "RECT") == 0 ||
        strcmp(texTarget, "1D") == 0) {
        if (strcmp(mode, "proj") == 0)
            _snprintf(out, outSize - 1, "texture2D(%s, (%s).xy / (%s).w)", samp, coord, coord);
        else if (strcmp(mode, "bias") == 0)
            _snprintf(out, outSize - 1, "tex2Dbias(%s, vec4((%s).xy, 0.0, (%s).w))", samp, coord, coord);
        else
            _snprintf(out, outSize - 1, "texture2D(%s, (%s).xy)", samp, coord);
    } else if (strcmp(texTarget, "CUBE") == 0) {
        if (strcmp(mode, "proj") == 0)
            _snprintf(out, outSize - 1, "textureCube(%s, (%s).xyz / (%s).w)", samp, coord, coord);
        else
            _snprintf(out, outSize - 1, "textureCube(%s, (%s).xyz)", samp, coord);
    } else if (strcmp(texTarget, "3D") == 0) {
        if (strcmp(mode, "proj") == 0)
            _snprintf(out, outSize - 1, "texture3D(%s, (%s).xyz / (%s).w)", samp, coord, coord);
        else
            _snprintf(out, outSize - 1, "texture3D(%s, (%s).xyz)", samp, coord);
    } else {
        arbFail(c, "texture target '%s' has no D3D9 sampling equivalent", texTarget);
        return FALSE;
    }
    out[outSize - 1] = '\0';
    return TRUE;
}

/*
 * Emit one instruction.  `tokens[0]` is the opcode (already stripped of any
 * _SAT suffix, reported through `saturate`).
 */
static BOOL arbEmitInstruction(ARBCtx *c, const char *op, BOOL saturate, BOOL updateCC,
                               char tokens[][ARB_TOKEN_LEN], int count)
{
    ARBDest dst;
    ARBOperand src[3];
    int opFirst[4], opLast[4];
    int nOps = 0;
    int i, start;
    char expr[ARB_BIG_EXPR];

    if (!arbOpcodeKnown(op)) {
        arbFail(c, "opcode %s is not part of the ARB_vertex_program / "
                   "ARB_fragment_program core instruction set", op);
        return FALSE;
    }

    /* Split the operand list on commas. */
    start = 1;
    for (i = 1; i <= count; i++) {
        if (i == count || tokens[i][0] == ',') {
            if (nOps < 4 && i > start) {
                opFirst[nOps] = start;
                opLast[nOps]  = i - 1;
                nOps++;
            }
            start = i + 1;
        }
    }

    /* KIL is the only instruction with no destination. */
    if (strcmp(op, "KIL") == 0) {
        if (nOps < 1) { arbFail(c, "KIL needs one operand"); return FALSE; }
        if (!arbResolveSource(c, tokens, opFirst[0], opLast[0], &src[0])) return FALSE;
        /* ARB kills the fragment when any component is negative, which is
         * exactly HLSL clip(). */
        arbEmit(c, "    clip(%s);\n", src[0].expr);
        return TRUE;
    }

    if (nOps < 1) { arbFail(c, "instruction %s has no operands", op); return FALSE; }
    if (!arbParseDest(c, tokens, opFirst[0], opLast[0], updateCC, &dst)) return FALSE;

    /* TEX/TXP/TXB take (dst, coord, texture[n], target). */
    if (strcmp(op, "TEX") == 0 || strcmp(op, "TXP") == 0 || strcmp(op, "TXB") == 0) {
        int unit = 0;
        const char *mode = (strcmp(op, "TXP") == 0) ? "proj" :
                           (strcmp(op, "TXB") == 0) ? "bias" : "";
        if (c->target != ARB_TARGET_FRAGMENT) {
            arbFail(c, "%s is only valid in a fragment program", op);
            return FALSE;
        }
        if (nOps < 4) { arbFail(c, "%s needs coord, texture unit and target", op); return FALSE; }
        if (!arbResolveSource(c, tokens, opFirst[1], opLast[1], &src[0])) return FALSE;
        {
            const char *t = tokens[opFirst[2]];
            const char *lb = strchr(t, '[');
            unit = lb ? atoi(lb + 1) : 0;
            if (unit < 0 || unit >= 8) {
                arbFail(c, "texture unit %d is outside the 8 units D3D9 exposes here", unit);
                return FALSE;
            }
        }
        c->samplerUsed[unit] = TRUE;
        if (!arbEmitTexture(c, tokens[opFirst[3]], unit, src[0].expr, mode,
                            expr, sizeof(expr)))
            return FALSE;
        arbStore(c, &dst, expr, saturate);
        return TRUE;
    }

    for (i = 1; i < nOps && i <= 3; i++)
        if (!arbResolveSource(c, tokens, opFirst[i], opLast[i], &src[i - 1]))
            return FALSE;

    #define S0 src[0].expr
    #define S1 src[1].expr
    #define S2 src[2].expr

    if (nOps < 2) { arbFail(c, "instruction %s needs a source operand", op); return FALSE; }

    if      (!strcmp(op, "MOV")) _snprintf(expr, sizeof(expr) - 1, "%s", S0);
    else if (!strcmp(op, "ABS")) _snprintf(expr, sizeof(expr) - 1, "abs(%s)", S0);
    else if (!strcmp(op, "FLR")) _snprintf(expr, sizeof(expr) - 1, "floor(%s)", S0);
    else if (!strcmp(op, "FRC")) _snprintf(expr, sizeof(expr) - 1, "frac(%s)", S0);
    else if (!strcmp(op, "RCP")) _snprintf(expr, sizeof(expr) - 1, "vec4(1.0 / (%s).x)", S0);
    else if (!strcmp(op, "RSQ")) _snprintf(expr, sizeof(expr) - 1, "vec4(inversesqrt(abs((%s).x)))", S0);
    else if (!strcmp(op, "EX2")) _snprintf(expr, sizeof(expr) - 1, "vec4(exp2((%s).x))", S0);
    else if (!strcmp(op, "LG2")) _snprintf(expr, sizeof(expr) - 1, "vec4(log2((%s).x))", S0);
    else if (!strcmp(op, "COS")) _snprintf(expr, sizeof(expr) - 1, "vec4(cos((%s).x))", S0);
    else if (!strcmp(op, "SIN")) _snprintf(expr, sizeof(expr) - 1, "vec4(sin((%s).x))", S0);
    else if (!strcmp(op, "SCS")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(cos((%s).x), sin((%s).x), 0.0, 0.0)", S0, S0);
    else if (!strcmp(op, "EXP")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(exp2(floor((%s).x)), (%s).x - floor((%s).x),"
                                           " exp2((%s).x), 1.0)", S0, S0, S0, S0);
    else if (!strcmp(op, "LOG")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(floor(log2(abs((%s).x))),"
                                           " abs((%s).x) / exp2(floor(log2(abs((%s).x)))),"
                                           " log2(abs((%s).x)), 1.0)", S0, S0, S0, S0);
    else if (!strcmp(op, "LIT")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(1.0, max((%s).x, 0.0),"
                                           " ((%s).x > 0.0 ? pow(max((%s).y, 0.0),"
                                           " clamp((%s).w, -128.0, 128.0)) : 0.0), 1.0)",
                                           S0, S0, S0, S0);
    else if (nOps < 3) { arbFail(c, "instruction %s needs two source operands", op); return FALSE; }
    else if (!strcmp(op, "ADD")) _snprintf(expr, sizeof(expr) - 1, "(%s + %s)", S0, S1);
    else if (!strcmp(op, "SUB")) _snprintf(expr, sizeof(expr) - 1, "(%s - %s)", S0, S1);
    else if (!strcmp(op, "MUL")) _snprintf(expr, sizeof(expr) - 1, "(%s * %s)", S0, S1);
    else if (!strcmp(op, "MIN")) _snprintf(expr, sizeof(expr) - 1, "min(%s, %s)", S0, S1);
    else if (!strcmp(op, "MAX")) _snprintf(expr, sizeof(expr) - 1, "max(%s, %s)", S0, S1);
    else if (!strcmp(op, "SGE")) _snprintf(expr, sizeof(expr) - 1, "step(%s, %s)", S1, S0);
    else if (!strcmp(op, "SLT")) _snprintf(expr, sizeof(expr) - 1, "(vec4(1.0) - step(%s, %s))", S1, S0);
    else if (!strcmp(op, "DP3")) _snprintf(expr, sizeof(expr) - 1, "vec4(dot((%s).xyz, (%s).xyz))", S0, S1);
    else if (!strcmp(op, "DP4")) _snprintf(expr, sizeof(expr) - 1, "vec4(dot(%s, %s))", S0, S1);
    else if (!strcmp(op, "DPH")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(dot(vec4((%s).xyz, 1.0), %s))", S0, S1);
    else if (!strcmp(op, "DST")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(1.0, (%s).y * (%s).y, (%s).z, (%s).w)", S0, S1, S0, S1);
    else if (!strcmp(op, "XPD")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(cross((%s).xyz, (%s).xyz), 1.0)", S0, S1);
    else if (!strcmp(op, "POW")) _snprintf(expr, sizeof(expr) - 1,
                                           "vec4(pow((%s).x, (%s).x))", S0, S1);
    else if (nOps < 4) { arbFail(c, "instruction %s needs three source operands", op); return FALSE; }
    else if (!strcmp(op, "MAD")) _snprintf(expr, sizeof(expr) - 1, "(%s * %s + %s)", S0, S1, S2);
    else if (!strcmp(op, "LRP")) _snprintf(expr, sizeof(expr) - 1,
                                           "(%s * %s + (vec4(1.0) - %s) * %s)", S0, S1, S0, S2);
    else if (!strcmp(op, "CMP")) _snprintf(expr, sizeof(expr) - 1,
                                           /* ARB compares each component of src0
                                            * against zero, unlike D3D9's CMP. */
                                           "((%s) < 0.0 ? (%s) : (%s))", S0, S1, S2);
    else if (!strcmp(op, "SWZ")) { arbFail(c, "SWZ is handled separately"); return FALSE; }
    else {
        arbFail(c, "opcode %s is not part of the ARB_vertex_program / "
                   "ARB_fragment_program core instruction set", op);
        return FALSE;
    }

    #undef S0
    #undef S1
    #undef S2

    expr[sizeof(expr) - 1] = '\0';
    arbStore(c, &dst, expr, saturate);
    return TRUE;
}

/*
 * SWZ dst, src, <c>, <c>, <c>, <c>;
 *
 * Each component selector is an independent swizzle letter or the constants
 * 0 / 1, optionally negated — none of which a single GLSL swizzle can express,
 * so the result is built one component at a time.
 */
static BOOL arbEmitSwizzle(ARBCtx *c, BOOL saturate, BOOL updateCC,
                           char tokens[][ARB_TOKEN_LEN], int count)
{
    ARBDest dst;
    ARBOperand base;
    int opFirst[8], opLast[8];
    int nOps = 0, i, start;
    char parts[4][64];
    char expr[ARB_BIG_EXPR];

    start = 1;
    for (i = 1; i <= count; i++) {
        if (i == count || tokens[i][0] == ',') {
            if (nOps < 8 && i > start) { opFirst[nOps] = start; opLast[nOps] = i - 1; nOps++; }
            start = i + 1;
        }
    }
    if (nOps < 6) { arbFail(c, "SWZ needs a destination, a source and four selectors"); return FALSE; }
    if (!arbParseDest(c, tokens, opFirst[0], opLast[0], updateCC, &dst)) return FALSE;
    if (!arbResolveSource(c, tokens, opFirst[1], opLast[1], &base)) return FALSE;

    for (i = 0; i < 4; i++) {
        BOOL neg = FALSE;
        int t = opFirst[2 + i];
        while (t <= opLast[2 + i] && (tokens[t][0] == '-' || tokens[t][0] == '+')) {
            if (tokens[t][0] == '-') neg = !neg;
            t++;
        }
        if (t > opLast[2 + i]) { arbFail(c, "SWZ selector %d is empty", i); return FALSE; }
        if (tokens[t][0] == '0')
            sprintf(parts[i], "%s0.0", neg ? "-" : "");
        else if (tokens[t][0] == '1')
            sprintf(parts[i], "%s1.0", neg ? "-" : "");
        else {
            char sw[8];
            if (!arbIsSwizzleText(tokens[t]) || tokens[t][1] != '\0') {
                arbFail(c, "SWZ selector '%s' is not a component", tokens[t]);
                return FALSE;
            }
            arbNormalizeSwizzle(tokens[t], sw);
            _snprintf(parts[i], sizeof(parts[i]) - 1, "%s(%s).%s", neg ? "-" : "", base.expr, sw);
            parts[i][sizeof(parts[i]) - 1] = '\0';
        }
    }

    _snprintf(expr, sizeof(expr) - 1, "vec4(%s, %s, %s, %s)",
              parts[0], parts[1], parts[2], parts[3]);
    expr[sizeof(expr) - 1] = '\0';
    arbStore(c, &dst, expr, saturate);
    return TRUE;
}

/*
 * ARL addr, src;  — the address register feeds relative PARAM array reads.
 */
static BOOL arbEmitARL(ARBCtx *c, char tokens[][ARB_TOKEN_LEN], int count)
{
    ARBSymbol *sym;
    ARBOperand s;
    int opFirst[4], opLast[4], nOps = 0, i, start = 1;

    for (i = 1; i <= count; i++) {
        if (i == count || tokens[i][0] == ',') {
            if (nOps < 4 && i > start) { opFirst[nOps] = start; opLast[nOps] = i - 1; nOps++; }
            start = i + 1;
        }
    }
    if (nOps < 2) { arbFail(c, "ARL needs a destination and a source"); return FALSE; }

    {
        char base[ARB_NAME_LEN];
        arbSplitIndex(tokens[opFirst[0]], base, sizeof(base));
        {
            char *dot = strchr(base, '.');
            if (dot) *dot = '\0';
        }
        sym = arbFindSymbol(c, base);
        if (!sym || sym->kind != ARBSYM_ADDRESS) {
            arbFail(c, "ARL destination '%s' is not an ADDRESS register", base);
            return FALSE;
        }
    }
    if (!arbResolveSource(c, tokens, opFirst[1], opLast[1], &s)) return FALSE;
    arbEmit(c, "    %s = int(floor((%s).x));\n", sym->expr, s.expr);
    return TRUE;
}


/*---------------------- Driver ----------------------*/

static void arbStripComments(char *src)
{
    char *p = src;
    while (*p) {
        if (*p == '#') {
            while (*p && *p != '\n') *p++ = ' ';
        } else {
            p++;
        }
    }
}

static BOOL arbParseHeader(const char *src, int *pTarget)
{
    const char *p = src;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "!!ARBvp1.0", 10) == 0) { *pTarget = ARB_TARGET_VERTEX;   return TRUE; }
    if (strncmp(p, "!!ARBfp1.0", 10) == 0) { *pTarget = ARB_TARGET_FRAGMENT; return TRUE; }
    return FALSE;
}

/* Uppercase copy of a token, used so opcodes compare case-insensitively. */
static void arbUpper(const char *in, char *out, int outSize)
{
    int i;
    for (i = 0; in[i] && i < outSize - 1; i++)
        out[i] = (char)toupper((unsigned char)in[i]);
    out[i] = '\0';
}

static void arbBuildDeclarations(ARBCtx *c, char *out, int outSize)
{
    int off = 0;
    int i;

    out[0] = '\0';

    if (c->target == ARB_TARGET_VERTEX) {
        for (i = 0; i < ARB_IN_COUNT; i++) {
            if (!c->inUsed[i] || !arbVertexInAvailable(i)) continue;
            arbAppend(out, &off, outSize, "attribute vec4 %s;\n", arbVertexInName(i));
        }
        for (i = 0; i < ARB_OUT_COUNT; i++) {
            if (!c->outUsed[i]) continue;
            if (i == ARB_OUT_POSITION || i == ARB_OUT_POINTSIZE) continue;
            arbAppend(out, &off, outSize, "varying vec4 %s;\n", arbVaryingName(i));
        }
    } else {
        for (i = 0; i < ARB_OUT_COUNT; i++) {
            if (!c->outUsed[i]) continue;
            if (i == ARB_OUT_POSITION || i == ARB_OUT_POINTSIZE) continue;
            arbAppend(out, &off, outSize, "varying vec4 %s;\n", arbVaryingName(i));
        }
        /*
         * D3D9 pins a sampler to its register, and the draw path binds the GL
         * texture of unit N to D3D9 sampler stage N.  Naming the register in
         * the declaration is what keeps texture[N] on stage N regardless of
         * the order the compiler happens to see the samplers in.  glsl_to_hlsl
         * emits a sampler uniform's name verbatim, so the register binding
         * rides along with it.
         */
        for (i = 0; i < 8; i++) {
            if (!c->samplerUsed[i]) continue;
            arbAppend(out, &off, outSize,
                      "uniform sampler2D arb_tex%d:register(s%d);\n", i, i);
        }
    }

    if (c->info->envArraySize > 0)
        arbAppend(out, &off, outSize, "uniform vec4 %s[%d];\n",
                  ARB_ENV_UNIFORM_NAME, c->info->envArraySize);
    if (c->info->localArraySize > 0)
        arbAppend(out, &off, outSize, "uniform vec4 %s[%d];\n",
                  ARB_LOCAL_UNIFORM_NAME, c->info->localArraySize);
    if (c->info->usesStateMatrices || c->info->usesStateLight || c->info->usesStateFog)
        arbAppend(out, &off, outSize, "uniform vec4 %s[%d];\n",
                  ARB_STATE_UNIFORM_NAME, ARB_STATE_REG_COUNT);

    out[outSize - 1] = '\0';
}

BOOL arbTranslateProgram(const char *asmSource, int asmLen,
                         char *glslOut, int glslOutSize,
                         ARBTranslation *info)
{
    ARBCtx ctx;
    char *work = NULL;
    char *decls = NULL;
    char *stmt;
    int off, i;
    BOOL ok = TRUE;

    if (!info) return FALSE;
    memset(info, 0, sizeof(*info));
    info->target = ARB_TARGET_VERTEX;

    if (!asmSource || asmLen <= 0 || !glslOut || glslOutSize < 1024) {
        strcpy(info->error, "empty or unusable program string");
        return FALSE;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.info     = info;
    ctx.maxEnv   = -1;
    ctx.maxLocal = -1;
    ctx.bodyCap  = 16 * 1024;
    ctx.body     = (char *)malloc(ctx.bodyCap);
    work         = (char *)malloc((size_t)asmLen + 2);
    decls        = (char *)malloc(8 * 1024);
    if (!ctx.body || !work || !decls) {
        free(ctx.body); free(work); free(decls);
        strcpy(info->error, "out of memory");
        return FALSE;
    }
    ctx.body[0] = '\0';

    memcpy(work, asmSource, (size_t)asmLen);
    work[asmLen] = '\0';
    arbStripComments(work);

    if (!arbParseHeader(work, &ctx.target)) {
        strcpy(info->error, "missing !!ARBvp1.0 / !!ARBfp1.0 header");
        free(ctx.body); free(work); free(decls);
        return FALSE;
    }
    info->target = ctx.target;

    /* Blank the header so it is not parsed as a statement. */
    {
        char *hdr = strstr(work, "!!ARB");
        if (hdr) { int n; for (n = 0; n < 10 && hdr[n]; n++) hdr[n] = ' '; }
    }

    /* Walk the statements. */
    stmt = strtok(work, ";");
    while (stmt && !ctx.failed) {
        char tokens[ARB_MAX_TOKENS][ARB_TOKEN_LEN];
        int count = arbTokenizeStatement(stmt, tokens);
        char opcode[ARB_TOKEN_LEN];
        BOOL saturate = FALSE;
        BOOL updateCC = FALSE;

        stmt = strtok(NULL, ";");
        if (count == 0) continue;

        arbUpper(tokens[0], opcode, sizeof(opcode));
        if (strcmp(opcode, "END") == 0) break;

        if      (strcmp(opcode, "TEMP") == 0)    { arbDeclTemp(&ctx, tokens, count);      continue; }
        else if (strcmp(opcode, "ADDRESS") == 0) { arbDeclAddress(&ctx, tokens, count);   continue; }
        else if (strcmp(opcode, "ATTRIB") == 0)  { arbDeclAlias(&ctx, tokens, count, FALSE); continue; }
        else if (strcmp(opcode, "OUTPUT") == 0)  { arbDeclAlias(&ctx, tokens, count, TRUE);  continue; }
        else if (strcmp(opcode, "ALIAS") == 0)   { arbDeclAlias(&ctx, tokens, count, FALSE); continue; }
        else if (strcmp(opcode, "PARAM") == 0)   { arbDeclParam(&ctx, tokens, count);     continue; }
        else if (strcmp(opcode, "OPTION") == 0)  { arbDeclOption(&ctx, tokens, count);    continue; }

        /* Strip a _SAT suffix, then a trailing C (the condition-code update
         * form).  The NV-only _R/_H/_X precision suffixes are not part of the
         * ARB core set and are refused rather than approximated. */
        {
            int len = (int)strlen(opcode);
            if (len > 4 && strcmp(opcode + len - 4, "_SAT") == 0) {
                saturate = TRUE;
                opcode[len - 4] = '\0';
                len -= 4;
            } else if (len > 2 && opcode[len - 2] == '_') {
                arbFail(&ctx, "opcode suffix '%s' is an NV extension, not ARB core",
                        opcode + len - 2);
                break;
            }
            /* "FRC" is itself an opcode, so only strip the C when what is left
             * is a real opcode and the whole spelling is not. */
            if (len > 3 && opcode[len - 1] == 'C' && !arbOpcodeKnown(opcode)) {
                char trimmed[ARB_TOKEN_LEN];
                strcpy(trimmed, opcode);
                trimmed[len - 1] = '\0';
                if (arbOpcodeKnown(trimmed)) {
                    updateCC = TRUE;
                    strcpy(opcode, trimmed);
                }
            }
        }

        if (strcmp(opcode, "SWZ") == 0)      arbEmitSwizzle(&ctx, saturate, updateCC, tokens, count);
        else if (strcmp(opcode, "ARL") == 0) arbEmitARL(&ctx, tokens, count);
        else                                 arbEmitInstruction(&ctx, opcode, saturate, updateCC, tokens, count);
    }

    if (ctx.failed) {
        free(ctx.body); free(work); free(decls);
        return FALSE;
    }

    /* Note every input the fixed vertex format cannot supply. */
    if (ctx.target == ARB_TARGET_VERTEX) {
        for (i = 0; i < ARB_IN_COUNT; i++) {
            if (ctx.inUsed[i] && !arbVertexInAvailable(i))
                arbNote(&ctx, "%s is not carried by the vertex format "
                              "(position/normal/color/texcoord0/texcoord1 and "
                              "generic attribs 6/7 only), "
                              "reading (0,0,0,1)", arbInDescription(i));
        }
    }

    info->envArraySize   = (ctx.maxEnv   >= 0) ? ctx.maxEnv   + 1 : 0;
    info->localArraySize = (ctx.maxLocal >= 0) ? ctx.maxLocal + 1 : 0;

    arbBuildDeclarations(&ctx, decls, 8 * 1024);

    /* Assemble the final GLSL. */
    off = 0;
    arbAppend(glslOut, &off, glslOutSize,
              "/* generated from ARB %s program assembly */\n%s\nvoid main() {\n",
              ctx.target == ARB_TARGET_VERTEX ? "vertex" : "fragment", decls);

    for (i = 0; i < ctx.tempCount; i++)
        arbAppend(glslOut, &off, glslOutSize,
                  "    vec4 %s = vec4(0.0, 0.0, 0.0, 0.0);\n", ctx.temps[i]);
    for (i = 0; i < ctx.addrCount; i++)
        arbAppend(glslOut, &off, glslOutSize, "    int %s = 0;\n", ctx.addrs[i]);
    if (ctx.ccUsed)
        arbAppend(glslOut, &off, glslOutSize,
                  "    vec4 arb_cc = vec4(0.0, 0.0, 0.0, 0.0);\n");

    /* Inputs with no data behind them read as the GL default vertex attribute. */
    if (ctx.target == ARB_TARGET_VERTEX) {
        for (i = 0; i < ARB_IN_COUNT; i++)
            if (ctx.inUsed[i] && !arbVertexInAvailable(i))
                arbAppend(glslOut, &off, glslOutSize,
                          "    vec4 %s = vec4(0.0, 0.0, 0.0, 1.0);\n",
                          arbVertexInName(i));
    }

    if (info->positionInvariant) {
        /* The program must not write result.position itself; the fixed
         * modelview-projection transform is applied on its behalf. */
        const char *pos = arbVertexInName(ARB_IN_POSITION);
        arbAppend(glslOut, &off, glslOutSize,
                  "    gl_Position = vec4(dot(%s[%d], %s), dot(%s[%d], %s),"
                  " dot(%s[%d], %s), dot(%s[%d], %s));\n",
                  ARB_STATE_UNIFORM_NAME, ARB_STATE_MAT_MVP * 4 + 0, pos,
                  ARB_STATE_UNIFORM_NAME, ARB_STATE_MAT_MVP * 4 + 1, pos,
                  ARB_STATE_UNIFORM_NAME, ARB_STATE_MAT_MVP * 4 + 2, pos,
                  ARB_STATE_UNIFORM_NAME, ARB_STATE_MAT_MVP * 4 + 3, pos);
    }

    arbAppend(glslOut, &off, glslOutSize, "%s", ctx.body);
    arbAppend(glslOut, &off, glslOutSize, "}\n");
    glslOut[glslOutSize - 1] = '\0';

    if (ctx.target == ARB_TARGET_VERTEX && !ctx.outUsed[ARB_OUT_POSITION] &&
        !info->positionInvariant) {
        strcpy(info->error, "vertex program never writes result.position");
        ok = FALSE;
    }

    free(ctx.body); free(work); free(decls);
    return ok;
}
