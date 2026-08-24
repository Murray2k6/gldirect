/*********************************************************************************
*
*  semantic_overlay.c - Per-draw D3D9 fixed-function state narration for Remix
*
*  See semantic_overlay.h for the design.  In short: every draw publishes the
*  GL fixed-function state the application configured - lights, material,
*  ambient, fog, texture stages and sampler state - as D3D9 fixed-function
*  state immediately before submission.  Programs that are conservatively
*  FFP-equivalent additionally have their translated shaders dropped so the
*  draw runs as pure D3D9 fixed function.
*
*  Safety model:
*
*    - Narration runs under the wrapper's standard __try/__except guard, so a
*      device problem cannot take the game down.
*    - Narration is read-only over GL state; the only GL-side mutation is the
*      shader drop of an FFP-equivalent program, which restores itself on the
*      next non-degrading draw.
*    - The degrade guard refuses anything the D3D9 fixed-function pipeline
*      cannot reproduce exactly (software stages, texgen, cube textures, more
*      than two texture stages, fragment shaders that read the framebuffer).
*    - Software-executed stages (the GL 4.6 worker path) can never degrade:
*      their submission owns its own pipeline (g_postStageVS + program->pPS).
*
*********************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_overlay.h"
#include "gl_impl.h"
#include "glsl_to_hlsl.h"
#include "context_manager.h"
#include "gld_diag.h"

#define GLD_OVERLAY_MAX_SAMPLERS 2    /* GLS_D3DVertex carries two texcoord sets */

#define GLD_DEG2RAD 0.017453292519943295f

/* ===================================================================
 *  Enablement
 * =================================================================== */

BOOL gldSemanticOverlayEnabled(void)
{
    char buf[8];
    DWORD n;

    if (gldIsRemixDetected()) return TRUE;

    /* Harness gate: GLDIRECT_SEMANTIC_DIAG=1 runs the overlay without Remix
     * so the smoke test can prove narration and byte-stable geometry. */
    n = GetEnvironmentVariableA("GLDIRECT_SEMANTIC_DIAG", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf) && buf[0] != '\0' && buf[0] != '0')
        return TRUE;

    return FALSE;
}

/* A translated shader with no sampler is legal, but Remix's D3D9 ingestion
 * rejects a draw whose texture slot zero has no hash.  A white texture is a
 * rendering no-op for untextured/FVF colour draws and is ignored by shaders
 * that do not sample; it merely gives the downstream wrapper a stable resource
 * identity so those menu primitives are not discarded. */
static IDirect3DTexture9 *g_overlayWhiteTexture = NULL;

/* Opt-in texture narration used when a game reaches its front end but one of
 * Remix's hashed-texture draws disappears.  Log each program/texture pair only
 * once: an id Tech frame can submit the same atlas thousands of times and a
 * per-draw trace would bury the useful transition in megabytes of noise. */
static BOOL _overlayTextureTraceEnabled(void)
{
    static int cached = -1;
    char value[8];
    DWORD length;

    if (cached >= 0) return cached ? TRUE : FALSE;
    length = GetEnvironmentVariableA("GLDIRECT_TEXTURE_TRACE", value,
                                     sizeof(value));
    cached = (length > 0 && length < sizeof(value) &&
              value[0] != '\0' && value[0] != '0') ? 1 : 0;
    return cached ? TRUE : FALSE;
}

static BOOL _overlayBootstrapCameraEnabled(void)
{
    char value[8];
    DWORD length = GetEnvironmentVariableA("GLDIRECT_BOOTSTRAP_CAMERA", value,
                                           sizeof(value));
    return (length > 0 && length < sizeof(value) &&
            value[0] != '\0' && value[0] != '0') ? TRUE : FALSE;
}

static BOOL _overlayTraceTexturePairOnce(unsigned int program,
                                         unsigned int texture)
{
    /* Open-addressed diagnostic set.  Zero is the empty marker, so offset the
     * packed key by one.  A full table merely stops additional diagnostics. */
    static unsigned __int64 seen[4096];
    unsigned __int64 key = (((unsigned __int64)program) << 32) |
                           (unsigned __int64)texture;
    unsigned int slot = (program * 16777619u ^ texture * 2166136261u) & 4095u;
    unsigned int probe;

    key++;
    for (probe = 0; probe < 4096; ++probe) {
        unsigned int index = (slot + probe) & 4095u;
        if (seen[index] == key) return FALSE;
        if (seen[index] == 0) {
            seen[index] = key;
            return TRUE;
        }
    }
    return FALSE;
}

static IDirect3DTexture9 *_overlayWhiteTexture(IDirect3DDevice9 *pDev)
{
    D3DLOCKED_RECT locked;
    HRESULT hr;

    if (g_overlayWhiteTexture || !pDev) return g_overlayWhiteTexture;
    /* Remix does not assign a useful asset hash to a 1x1 placeholder.  A 4x4
     * uniform-white resource is still sampling-neutral at every UV/filter mode
     * but is large enough to carry stable texture identity downstream. */
    hr = IDirect3DDevice9_CreateTexture(pDev, 4, 4, 1, 0,
                                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                        &g_overlayWhiteTexture, NULL);
    if (FAILED(hr) || !g_overlayWhiteTexture) {
        g_overlayWhiteTexture = NULL;
        gldDiagLog("GL: Remix fallback white texture creation failed (hr=0x%08X)",
                   (unsigned)hr);
        return NULL;
    }
    memset(&locked, 0, sizeof(locked));
    hr = IDirect3DTexture9_LockRect(g_overlayWhiteTexture, 0, &locked, NULL, 0);
    if (SUCCEEDED(hr)) {
        int y;
        for (y = 0; y < 4; ++y) {
            DWORD *row = (DWORD *)((unsigned char *)locked.pBits +
                                   (ptrdiff_t)y * locked.Pitch);
            row[0] = row[1] = row[2] = row[3] = 0xFFFFFFFFu;
        }
        IDirect3DTexture9_UnlockRect(g_overlayWhiteTexture, 0);
        /* MANAGED textures are hashed by Remix when their system-memory copy
         * is uploaded.  Force that upload now so the very first untextured draw
         * cannot reach Remix while this neutral resource still has hash zero. */
        IDirect3DTexture9_PreLoad(g_overlayWhiteTexture);
    }
    gldDiagLog("GL: Remix 4x4 fallback white texture created for untextured draws");
    return g_overlayWhiteTexture;
}

void gldSemanticOverlayReleaseResources(void)
{
    if (g_overlayWhiteTexture) {
        IDirect3DTexture9_Release(g_overlayWhiteTexture);
        g_overlayWhiteTexture = NULL;
    }
}

/* ===================================================================
 *  FFP-equivalence scan
 * =================================================================== */

static int _overlayTokenEquals(const char *p, int len, const char *tok)
{
    int n = (int)strlen(tok);
    return (len == n && strncmp(p, tok, n) == 0);
}

/* GLSL engines name their light uniforms in camelCase ("sunDiffuseColor",
 * "lightPosition"), so role detection is case-insensitive. */
static BOOL _overlayTokenContains(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    if (!haystack || !needle || n == 0) return FALSE;
    while (*haystack) {
        if (_strnicmp(haystack, needle, n) == 0)
            return TRUE;
        haystack++;
    }
    return FALSE;
}

BOOL gldDetectFFPEquivalent(const char *vsSource, const char *fsSource)
{
    static const char *const disqualifiers[] = {
        /* Control flow: anything conditional or iterative makes the program's
         * output depend on data the FFP cannot read. */
        "if", "else", "for", "while", "do", "switch", "case",
        "break", "continue",
        /* Fragment discard and read-modify-write of the framebuffer. */
        "discard", "gl_FragDepth",
        /* Texel-level access: D3D9 FFP sampling cannot address texels. */
        "texelFetch", "textureSize", "textureLod", "textureGrad",
        "textureProj", "textureOffset", "texture2DLod", "texture2DGrad",
        "shadow2D", "shadow2DProj",
        /* Non-2D samplers: cube needs 3-component coordinates and 3D needs
         * a third texcoord set; the fat vertex carries only two sets. */
        "samplerCube", "sampler3D", "sampler2DShadow", "sampler1DShadow",
        "isampler2D", "usampler2D", "isamplerCube", "usamplerCube",
        /* Per-instance / per-vertex IDs have no FFP analogue. */
        "gl_VertexID", "gl_InstanceID", "gl_DrawID", "gl_BaseInstance",
        "gl_ClipDistance", "gl_ClipVertex", "gl_PointSize",
        "gl_FragCoord", "gl_PrimitiveID", "gl_PrimitiveIDIn",
        "gl_TessCoord", "gl_InvocationID", "gl_ViewportIndex", "gl_Layer",
        /* Aggregates and templates imply arbitrary computation.  mat4 is
         * deliberately here even though the list has long named mat2/mat3:
         * a GLSL camera is almost always "uniform mat4 mvp/uModelViewProj",
         * and D3D9 fixed function has no per-draw matrix uniform — its
         * vertex transform is the separate WORLD/VIEW/PROJECTION states,
         * which a GLSL engine never touches.  Degrading such a program
         * drops the camera matrix and renders with an identity transform,
         * which is exactly the blank/off-screen output seen in
         * Wolfenstein TNO's menu.  If a program contains any matrix type,
         * keep its translated shaders. */
        "struct", "mat2", "mat3", "mat4",
        "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4",
        "mat4x2", "mat4x3", "mat4x4",
        "dmat2", "dmat3", "dmat4",
        "dvec2", "dvec3", "dvec4", "double"
    };
    const char *sources[2];
    int samplerCount = 0;
    int si;

    if (!vsSource || !fsSource) return FALSE;

    sources[0] = vsSource;
    sources[1] = fsSource;

    for (si = 0; si < 2; si++) {
        const char *p = sources[si];

        while (*p) {
            unsigned char c = (unsigned char)*p;

            if (c == '#') {
                /* Preprocessor conditionals and macros can restructure the
                 * source at compile time; anything past #version/#extension
                 * is not something a source scan can vouch for. */
                if (strncmp(p, "#define", 7) == 0 ||
                    strncmp(p, "#if", 3) == 0 ||
                    strncmp(p, "#undef", 6) == 0 ||
                    strncmp(p, "#include", 8) == 0 ||
                    strncmp(p, "#pragma", 7) == 0)
                    return FALSE;
                while (*p && *p != '\n') p++;
                continue;
            }

            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                const char *start = p;
                int len;
                int i;

                while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                       (*p >= '0' && *p <= '9') || *p == '_')
                    p++;
                len = (int)(p - start);

                for (i = 0; i < (int)(sizeof(disqualifiers) / sizeof(disqualifiers[0])); i++) {
                    if (_overlayTokenEquals(start, len, disqualifiers[i]))
                        return FALSE;
                }
                if (_overlayTokenEquals(start, len, "sampler2D")) {
                    if (++samplerCount > GLD_OVERLAY_MAX_SAMPLERS)
                        return FALSE;
                }
                continue;
            }

            p++;
        }
    }

    return TRUE;
}

/* ===================================================================
 *  Color / light helpers
 * =================================================================== */

static DWORD _overlayColorValue(const float rgba[4])
{
    if (!rgba) return 0;
    return D3DCOLOR_COLORVALUE(rgba[0], rgba[1], rgba[2], rgba[3]);
}

static void _overlaySetColor(D3DCOLORVALUE *dst, const float rgba[4])
{
    dst->r = rgba[0];
    dst->g = rgba[1];
    dst->b = rgba[2];
    dst->a = rgba[3];
}

static void _overlayInitLight(D3DLIGHT9 *l)
{
    memset(l, 0, sizeof(*l));
    l->Type    = D3DLIGHT_POINT;
    _overlaySetColor(&l->Diffuse, (const float[4]){ 1.0f, 1.0f, 1.0f, 1.0f });
    _overlaySetColor(&l->Ambient, (const float[4]){ 0.0f, 0.0f, 0.0f, 1.0f });
    _overlaySetColor(&l->Specular, (const float[4]){ 1.0f, 1.0f, 1.0f, 1.0f });
    l->Range    = 1.0e10f;
    l->Attenuation0 = 1.0f;
    l->Theta    = 0.0f;
    l->Phi      = 180.0f * GLD_DEG2RAD;
    l->Falloff  = 0.0f;
}

/* ===================================================================
 *  Mirrored GL lights
 * =================================================================== */

static int _overlayEmitMirroredLights(IDirect3DDevice9 *pDev, GLS_State *s)
{
    int i, count = 0;

    __try {
        for (i = 0; i < GLS_MAX_LIGHTS; i++) {
            GLS_Light *l = &s->lights[i];

            if (l->enabled) {
                D3DLIGHT9 d3d;

                _overlayInitLight(&d3d);
                _overlaySetColor(&d3d.Ambient, l->ambient);
                _overlaySetColor(&d3d.Diffuse, l->diffuse);
                _overlaySetColor(&d3d.Specular, l->specular);
                d3d.Attenuation0 = l->constantAttenuation;
                d3d.Attenuation1 = l->linearAttenuation;
                d3d.Attenuation2 = l->quadraticAttenuation;

                if (l->position[3] == 0.0f) {
                    /* GL directional light: position.w == 0 and the vector is
                     * the direction the light travels.  D3D9 negates the
                     * directional vector in its lighting equation, so the
                     * mirrored direction must be inverted. */
                    d3d.Type = D3DLIGHT_DIRECTIONAL;
                    d3d.Direction.x = -l->position[0];
                    d3d.Direction.y = -l->position[1];
                    d3d.Direction.z = -l->position[2];
                } else if (l->spotCutoff < 180.0f) {
                    d3d.Type = D3DLIGHT_SPOT;
                    d3d.Position.x = l->position[0];
                    d3d.Position.y = l->position[1];
                    d3d.Position.z = l->position[2];
                    d3d.Direction.x = -l->spotDirection[0];
                    d3d.Direction.y = -l->spotDirection[1];
                    d3d.Direction.z = -l->spotDirection[2];
                    d3d.Phi     = l->spotCutoff * GLD_DEG2RAD;
                    d3d.Falloff = l->spotExponent;
                } else {
                    d3d.Type = D3DLIGHT_POINT;
                    d3d.Position.x = l->position[0];
                    d3d.Position.y = l->position[1];
                    d3d.Position.z = l->position[2];
                }

                IDirect3DDevice9_SetLight(pDev, i, &d3d);
                IDirect3DDevice9_LightEnable(pDev, i, TRUE);
                count++;
            } else {
                IDirect3DDevice9_LightEnable(pDev, i, FALSE);
            }
        }
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    return count;
}

/* ===================================================================
 *  Uniform-synthesized lights (OpenMW pattern: all lighting in GLSL)
 * =================================================================== */

#define GLD_OVERLAY_ROLE_NONE       0
#define GLD_OVERLAY_ROLE_POSITION   1
#define GLD_OVERLAY_ROLE_DIRECTION  2
#define GLD_OVERLAY_ROLE_DIFFUSE    3
#define GLD_OVERLAY_ROLE_AMBIENT    4
#define GLD_OVERLAY_ROLE_SPECULAR   5
#define GLD_OVERLAY_ROLE_ATTENUATION 6
#define GLD_OVERLAY_ROLE_CUTOFF     7
#define GLD_OVERLAY_ROLE_FALLOFF    8
#define GLD_OVERLAY_ROLE_COLOR      9   /* generic "lightColor" */

static const struct {
    const char *token;
    int         role;
} GLD_OVERLAY_ROLE_TOKENS[] = {
    { "direction",      GLD_OVERLAY_ROLE_DIRECTION  },
    { "diffuse",        GLD_OVERLAY_ROLE_DIFFUSE    },
    { "ambient",        GLD_OVERLAY_ROLE_AMBIENT    },
    { "specular",       GLD_OVERLAY_ROLE_SPECULAR   },
    { "attenuation",    GLD_OVERLAY_ROLE_ATTENUATION },
    { "position",       GLD_OVERLAY_ROLE_POSITION   },
    { "cutoff",         GLD_OVERLAY_ROLE_CUTOFF     },
    { "falloff",        GLD_OVERLAY_ROLE_FALLOFF    },
    { "exponent",       GLD_OVERLAY_ROLE_FALLOFF    },
    { "color",          GLD_OVERLAY_ROLE_COLOR      },
    { "colour",         GLD_OVERLAY_ROLE_COLOR      },
    { "pos",            GLD_OVERLAY_ROLE_POSITION   },
    { "dir",            GLD_OVERLAY_ROLE_DIRECTION  },
    { "attn",           GLD_OVERLAY_ROLE_ATTENUATION },
};

static int _overlayUniformSlot(const char *name)
{
    /* "lights[3].position" -> 3 (digits after '['), "lightPosition2" -> 2
     * (last digit run), "sunDirection" -> 0. */
    const char *p = name;
    const char *runStart = NULL;
    const char *bracket = NULL;

    while (*p) {
        if (*p == '[') bracket = p;
        p++;
    }
    if (bracket) {
        p = bracket + 1;
        if (*p >= '0' && *p <= '9') return *p - '0';
    }
    p = name;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            runStart = p;
            while (p[1] >= '0' && p[1] <= '9') p++;
        }
        p++;
    }
    if (!runStart) return 0;
    return *runStart - '0';
}

static GLS_Uniform *_overlayUniformAt(GLS_Program *prog, int location)
{
    int i;

    if (!prog || location < 0 || location >= prog->resolvedCount) return NULL;
    for (i = 0; i < prog->uniformCount; i++) {
        if (prog->uniforms[i].location == location && prog->uniforms[i].set)
            return &prog->uniforms[i];
    }
    return NULL;
}

static int _overlaySynthLightCount(IDirect3DDevice9 *pDev, GLS_State *s,
                                   GLS_Program *prog)
{
    D3DLIGHT9 lights[GLS_MAX_LIGHTS];
    BOOL used[GLS_MAX_LIGHTS];
    BOOL hasDiffuse[GLS_MAX_LIGHTS];
    int i, count = 0;

    memset(lights, 0, sizeof(lights));
    memset(used, 0, sizeof(used));
    memset(hasDiffuse, 0, sizeof(hasDiffuse));
    for (i = 0; i < GLS_MAX_LIGHTS; i++) {
        _overlayInitLight(&lights[i]);
        lights[i].Phi = 180.0f * GLD_DEG2RAD;
    }

    for (i = 0; i < prog->resolvedCount && i < GLS_MAX_UNIFORMS; i++) {
        const char *name = prog->resolved[i].name;
        GLS_Uniform *u = _overlayUniformAt(prog, i);
        D3DLIGHT9 *l;
        int slot = 0, role = GLD_OVERLAY_ROLE_NONE, ti;

        if (!u || !name[0]) continue;

        for (ti = 0; ti < (int)(sizeof(GLD_OVERLAY_ROLE_TOKENS) /
                               sizeof(GLD_OVERLAY_ROLE_TOKENS[0])); ti++) {
            if (_overlayTokenContains(name, GLD_OVERLAY_ROLE_TOKENS[ti].token)) {
                role = GLD_OVERLAY_ROLE_TOKENS[ti].role;
                break;
            }
        }
        if (role == GLD_OVERLAY_ROLE_NONE) continue;

        slot = _overlayUniformSlot(name);
        if (slot < 0 || slot >= GLS_MAX_LIGHTS) continue;

        used[slot] = TRUE;
        l = &lights[slot];

        switch (role) {
        case GLD_OVERLAY_ROLE_POSITION:
            l->Position.x = u->data[0];
            l->Position.y = u->data[1];
            l->Position.z = u->data[2];
            if (u->type >= 4 && u->data[3] == 0.0f) {
                /* GL directional light expressed as a position vector. */
                l->Type = D3DLIGHT_DIRECTIONAL;
                l->Direction.x = -u->data[0];
                l->Direction.y = -u->data[1];
                l->Direction.z = -u->data[2];
            }
            break;
        case GLD_OVERLAY_ROLE_DIRECTION:
            /* GLSL lights use the direction the light travels, like GL;
             * D3D9 negates it in its lighting equation. */
            l->Direction.x = -u->data[0];
            l->Direction.y = -u->data[1];
            l->Direction.z = -u->data[2];
            l->Type = D3DLIGHT_DIRECTIONAL;
            break;
        case GLD_OVERLAY_ROLE_DIFFUSE:
            _overlaySetColor(&l->Diffuse, u->data);
            hasDiffuse[slot] = TRUE;
            break;
        case GLD_OVERLAY_ROLE_AMBIENT:
            _overlaySetColor(&l->Ambient, u->data);
            break;
        case GLD_OVERLAY_ROLE_SPECULAR:
            _overlaySetColor(&l->Specular, u->data);
            break;
        case GLD_OVERLAY_ROLE_COLOR:
            /* Generic "lightColor" — diffuse unless a dedicated diffuse was
             * already seen for this slot ("diffuseColor" resolves to DIFFUSE
             * first because its token appears earlier in the scan order). */
            if (!hasDiffuse[slot])
                _overlaySetColor(&l->Diffuse, u->data);
            break;
        case GLD_OVERLAY_ROLE_ATTENUATION:
            l->Attenuation0 = u->data[0];
            l->Attenuation1 = u->data[1];
            l->Attenuation2 = u->data[2];
            break;
        case GLD_OVERLAY_ROLE_CUTOFF:
            if (u->data[0] > 0.0f && u->data[0] < 180.0f) {
                l->Type = D3DLIGHT_SPOT;
                l->Phi  = u->data[0] * GLD_DEG2RAD;
            }
            break;
        case GLD_OVERLAY_ROLE_FALLOFF:
            l->Falloff = u->data[0];
            break;
        default:
            break;
        }
    }

    __try {
        for (i = 0; i < GLS_MAX_LIGHTS; i++) {
            if (used[i]) {
                IDirect3DDevice9_SetLight(pDev, i, &lights[i]);
                IDirect3DDevice9_LightEnable(pDev, i, TRUE);
                count++;
            } else {
                IDirect3DDevice9_LightEnable(pDev, i, FALSE);
            }
        }
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    return count;
}

/* ===================================================================
 *  Material / ambient / specular
 * =================================================================== */

static void _overlayEmitMaterial(IDirect3DDevice9 *pDev, GLS_State *s)
{
    D3DMATERIAL9 mat;

    memset(&mat, 0, sizeof(mat));
    _overlaySetColor(&mat.Ambient, s->materialFront.ambient);
    _overlaySetColor(&mat.Diffuse, s->materialFront.diffuse);
    _overlaySetColor(&mat.Specular, s->materialFront.specular);
    _overlaySetColor(&mat.Emissive, s->materialFront.emission);
    mat.Power    = s->materialFront.shininess;

    /* GL_COLOR_MATERIAL: the tracked material is overridden by the current
     * color the same way GL folds it in. */
    if (s->enableColorMaterial) {
        _overlaySetColor(&mat.Ambient, s->currentColor);
        _overlaySetColor(&mat.Diffuse, s->currentColor);
    }

    __try {
        IDirect3DDevice9_SetMaterial(pDev, &mat);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_AMBIENT,
                                        _overlayColorValue(s->lightModelAmbient));
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_SPECULARENABLE,
                                        s->enableLighting ? TRUE : FALSE);
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_NORMALIZENORMALS,
                                        s->enableNormalize ? TRUE : FALSE);
        /* D3D9 defaults to COLORVERTEX=TRUE: the vertex diffuse then replaces
         * the material's diffuse/ambient/specular in lighting, which GL never
         * does (current color only folds in through GL_COLOR_MATERIAL, already
         * folded into the material above).  Pin it off for lit draws; the
         * unlit path ignores it either way. */
        IDirect3DDevice9_SetRenderState(pDev, D3DRS_COLORVERTEX,
                                        s->enableLighting ? FALSE : TRUE);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

/* ===================================================================
 *  Texture stages
 * =================================================================== */

/* Return the GL texture target required by a live pixel-sampler register:
 * 0=inactive, 1=2D/1D/shadow, 2=cube, 3=3D.  D3D9 sampler registers are typed
 * by the shader declaration, while GL keeps simultaneous bindings for every
 * target on one unit.  Selecting whichever GL target happens to be non-null
 * binds stale cube maps into unrelated 2D/UI draws; Remix cannot content-hash
 * cube textures and consequently rejects the complete draw. */
static int _overlaySamplerKindForStage(GLS_Program *prog, int stage)
{
    GLS_Shader *fragment;
    int i;

    if (!prog || stage < 0 || stage >= GLS_MAX_TEX_UNITS) return 0;
    fragment = glsFindShader(prog->fragShader);

    for (i = 0; i < prog->resolvedCount; ++i) {
        GLS_ResolvedUniform *u = &prog->resolved[i];
        const char *source;
        const char *name;
        size_t nameLength;

        if (u->registerSet != GLSL_RS_SAMPLER || u->psRegister < 0 ||
            stage < u->psRegister || stage >= u->psRegister + u->registerCount)
            continue;

        /* Reflection has already proven this is a live sampler.  Recover its
         * dimensionality from the original GLSL declaration. */
        source = (fragment && fragment->source) ? fragment->source : NULL;
        name = u->name;
        nameLength = strcspn(name, "[");
        if (source && nameLength) {
            const char *match = source;
            while ((match = strstr(match, name)) != NULL) {
                const char *begin = match;
                const char *p;
                BOOL tokenStart = (match == source ||
                    !((match[-1] >= 'a' && match[-1] <= 'z') ||
                      (match[-1] >= 'A' && match[-1] <= 'Z') ||
                      (match[-1] >= '0' && match[-1] <= '9') || match[-1] == '_'));
                BOOL tokenEnd = !((match[nameLength] >= 'a' && match[nameLength] <= 'z') ||
                                  (match[nameLength] >= 'A' && match[nameLength] <= 'Z') ||
                                  (match[nameLength] >= '0' && match[nameLength] <= '9') ||
                                   match[nameLength] == '_');
                if (!tokenStart || !tokenEnd) { match += nameLength; continue; }
                while (begin > source && begin[-1] != ';' && begin[-1] != '\n' &&
                       begin[-1] != '\r') --begin;
                p = begin;
                if (strstr(p, "samplerCube") && strstr(p, "samplerCube") < match)
                    return 2;
                if (strstr(p, "isamplerCube") && strstr(p, "isamplerCube") < match)
                    return 2;
                if (strstr(p, "usamplerCube") && strstr(p, "usamplerCube") < match)
                    return 2;
                if (strstr(p, "sampler3D") && strstr(p, "sampler3D") < match)
                    return 3;
                if (strstr(p, "isampler3D") && strstr(p, "isampler3D") < match)
                    return 3;
                if (strstr(p, "usampler3D") && strstr(p, "usampler3D") < match)
                    return 3;
                return 1;
            }
        }
        /* SM3's common case is 2D.  Falling back to it is safer than binding a
         * stale cube map, whose Remix image hash is guaranteed to remain zero. */
        return 1;
    }
    return 0;
}

static unsigned int _overlayStageUnit(GLS_State *s, GLS_Program *prog, int stage)
{
    unsigned int unit;

    (void)s;
    /* Sampler values belong to the program object.  A sampler never written
     * by the application has GL's initial value zero, not stage==unit. */
    if (!prog || stage < 0 || stage >= GLS_MAX_TEX_UNITS)
        return (unsigned int)stage;
    if (prog->samplerStageSet[stage]) {
        unit = prog->samplerStageUnit[stage];
        if (unit < GLS_MAX_TEX_UNITS) return unit;
    }
    /* Reflected samplers default to texture unit zero.  Stages that are not
     * used by this program retain stage==unit solely for Remix narration;
     * the shader cannot observe those extra bindings. */
    {
        int i;
        for (i = 0; i < prog->resolvedCount; ++i)
            if (prog->resolved[i].registerSet == GLSL_RS_SAMPLER &&
                prog->resolved[i].psRegister >= 0 &&
                stage >= prog->resolved[i].psRegister &&
                stage < prog->resolved[i].psRegister +
                        prog->resolved[i].registerCount)
                return 0;
    }
    return (unsigned int)stage;
}

/* Returns the last stage with a bound texture (-1 if none) and records
 * disqualifiers for the FFP degrade decision. */
static int _overlayEmitStages(IDirect3DDevice9 *pDev, GLS_State *s,
                              GLS_Program *prog, BOOL *pStageOverflow,
                              BOOL *pCubeUsed)
{
    int lastStage = -1;
    int st;
    BOOL missingActiveSampler = FALSE;

    for (st = 0; st < GLS_MAX_TEX_UNITS; st++) {
        unsigned int unit = _overlayStageUnit(s, prog, st);
        GLS_Texture *tex;
        IDirect3DBaseTexture9 *baseTex = NULL;
        int texKind = 0;    /* 0 = none, 1 = 2D, 2 = cube, 3 = 3D */
        int requiredKind = prog ? _overlaySamplerKindForStage(prog, st) : -1;

        if (unit >= GLS_MAX_TEX_UNITS) continue;

        tex = NULL;
        if (requiredKind == 1 || requiredKind < 0)
            tex = glsFindTexture(s->boundTexture2D[unit]);
        if (tex && tex->pTex) {
            baseTex = (IDirect3DBaseTexture9 *)_glsGetSingleLevelTexture(tex);
            texKind = 1;
        }
        if (!baseTex && (requiredKind == 2 || requiredKind < 0)) {
            tex = glsFindTexture(s->boundTextureCube[unit]);
            if (tex && tex->pCubeTex) {
                baseTex = (IDirect3DBaseTexture9 *)tex->pCubeTex;
                texKind = 2;
            }
        }
        if (!baseTex && (requiredKind == 3 || requiredKind < 0)) {
            tex = glsFindTexture(s->boundTexture3D[unit]);
            if (tex && tex->pVolTex) {
                baseTex = (IDirect3DBaseTexture9 *)tex->pVolTex;
                texKind = 3;
            }
        }

        if (st == 0 && _overlayTextureTraceEnabled()) {
            unsigned int programId = prog ? prog->id : 0;
            unsigned int textureId = tex ? tex->id : 0;
            if (_overlayTraceTexturePairOnce(programId, textureId)) {
                D3DSURFACE_DESC desc;
                HRESULT descHr = E_FAIL;
                unsigned int levels = 0;
                ZeroMemory(&desc, sizeof(desc));
                if (tex && tex->pTex) {
                    descHr = IDirect3DTexture9_GetLevelDesc(tex->pTex, 0, &desc);
                    levels = IDirect3DTexture9_GetLevelCount(tex->pTex);
                }
                gldDiagLog("GL: texture trace program=%u stage=0 unit=%u tex=%u "
                           "kind=%d gl=%dx%d fmt=0x%X cpuBytes=%d "
                           "d3dDesc=%d d3d=%ux%u fmt=%d usage=0x%X pool=%d levels=%u",
                           programId, unit, textureId, texKind,
                           tex ? tex->width : 0, tex ? tex->height : 0,
                           tex ? tex->internalFormat : 0,
                           tex ? tex->pixelDataSize : 0,
                           SUCCEEDED(descHr) ? 1 : 0,
                           desc.Width, desc.Height, (int)desc.Format,
                           (unsigned)desc.Usage, (int)desc.Pool, levels);
            }
        }

        if (texKind == 2) *pCubeUsed = TRUE;
        if (texKind && st >= GLD_OVERLAY_MAX_SAMPLERS) *pStageOverflow = TRUE;

        __try {
            if (baseTex) {
                IDirect3DDevice9_SetTexture(pDev, st, baseTex);
                if (s->boundSampler[unit])
                    _glsApplySamplerObjectToD3D((unsigned int)st,
                                                glsFindSampler(s->boundSampler[unit]));
                else
                    _glsApplyTextureObjectSamplingToD3D((unsigned int)st, tex);
                /* A sampler object controls filtering but the texture still
                 * owns BASE_LEVEL/MAX_LEVEL.  Apply that range after either
                 * state path so a MAX_LEVEL==BASE_LEVEL texture cannot sample
                 * D3D allocation tail levels which OpenGL excludes. */
                _glsApplyTextureLevelRangeToD3D((unsigned int)st, tex);
                if (texKind == 1 && baseTex != (IDirect3DBaseTexture9 *)tex->pTex)
                    IDirect3DDevice9_SetSamplerState(pDev, st,
                                                     D3DSAMP_MAXMIPLEVEL, 0);

                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_TEXCOORDINDEX,
                                                      (DWORD)st);
                if (st == 0) {
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLOROP,
                                                          D3DTOP_MODULATE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG1,
                                                          D3DTA_TEXTURE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG2,
                                                          D3DTA_DIFFUSE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAOP,
                                                          D3DTOP_MODULATE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG1,
                                                          D3DTA_TEXTURE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG2,
                                                          D3DTA_DIFFUSE);
                } else {
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLOROP,
                                                          D3DTOP_MODULATE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG1,
                                                          D3DTA_TEXTURE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG2,
                                                          D3DTA_CURRENT);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAOP,
                                                          D3DTOP_MODULATE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG1,
                                                          D3DTA_TEXTURE);
                    IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG2,
                                                          D3DTA_CURRENT);
                }
                lastStage = st;
            } else if (st == 0) {
                IDirect3DTexture9 *white = _overlayWhiteTexture(pDev);
                /* No application texture on stage 0: the FFP output is still
                 * the vertex colour (white samples are neutral), while Remix
                 * sees a hashable resource instead of skipping the draw. */
                IDirect3DDevice9_SetTexture(pDev, st,
                    white ? (IDirect3DBaseTexture9 *)white : NULL);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLOROP,
                                                      D3DTOP_SELECTARG2);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG1,
                                                      D3DTA_DIFFUSE);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLORARG2,
                                                      D3DTA_DIFFUSE);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAOP,
                                                      D3DTOP_SELECTARG2);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG1,
                                                      D3DTA_DIFFUSE);
                IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAARG2,
                                                      D3DTA_DIFFUSE);
                if (white) lastStage = 0;
            } else {
                int i;
                for (i = 0; prog && i < prog->resolvedCount; ++i) {
                    GLS_ResolvedUniform *u = &prog->resolved[i];
                    if (u->registerSet == GLSL_RS_SAMPLER &&
                        u->psRegister >= 0 && st >= u->psRegister &&
                        st < u->psRegister + u->registerCount) {
                        missingActiveSampler = TRUE;
                        if (!prog->samplerMissingLogged)
                            gldDiagLog("GL: program %u active sampler '%s' stage %d maps "
                                       "to empty GL unit %u",
                                       prog->id, u->name, st, unit);
                        break;
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    if (prog && missingActiveSampler)
        prog->samplerMissingLogged = TRUE;

    /* Disable every stage past the last bound texture so a degraded draw
     * cannot inherit a stale stage. */
    __try {
        for (st = lastStage + 1; st < GLS_MAX_TEX_UNITS; st++) {
            IDirect3DDevice9_SetTexture(pDev, st, NULL);
            IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_COLOROP,
                                                  D3DTOP_DISABLE);
            IDirect3DDevice9_SetTextureStageState(pDev, st, D3DTSS_ALPHAOP,
                                                  D3DTOP_DISABLE);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }

    return lastStage;
}

/* ===================================================================
 *  FFP degrade decision
 * =================================================================== */

static BOOL _overlayFFPEligible(GLS_State *s, GLS_Program *prog,
                                BOOL stageOverflow, BOOL cubeUsed)
{
    char optIn[8];
    DWORD optInLength;
    int u, c;

    /* Dropping translated shaders is an image-changing optimisation, not a
     * prerequisite for Remix narration.  A source token scan cannot prove
     * arbitrary GLSL is fixed-function equivalent: id Tech's generated UI
     * shaders carry their transforms and colours in vec4 arrays, passed the
     * old scan, and became flat pink rectangles when their shaders were
     * removed.  Preserve shaders by default.  The legacy degrade remains an
     * explicit diagnostic/compatibility opt-in for known-simple programs. */
    optInLength = GetEnvironmentVariableA("GLDIRECT_REMIX_FFP_DEGRADE",
                                           optIn, sizeof(optIn));
    if (optInLength == 0 || optInLength >= sizeof(optIn) ||
        optIn[0] == '\0' || optIn[0] == '0')
        return FALSE;

    if (!prog || !prog->ffpEquivalent) return FALSE;
    if (prog->softwareGraphicsStages || prog->softwareVertexExecution ||
        prog->softwareFragmentExecution)
        return FALSE;
    if (stageOverflow || cubeUsed) return FALSE;

    /* Texgen would need D3D9's own texgen, which the draw path does not
     * publish (the shader handles it); degrading would change the image. */
    for (u = 0; u < GLS_MAX_TEX_UNITS; u++)
        for (c = 0; c < 4; c++)
            if (s->texGenEnabled[u][c]) return FALSE;

    return TRUE;
}

/* ===================================================================
 *  Shader-camera publication
 *
 *  Remix reconstructs its camera from the D3D9 transform state
 *  (WORLD/VIEW/PROJECTION).  Shader-era engines (Wolfenstein TNO, id
 *  Tech 5/6) never touch the fixed-function GL matrix stacks: the whole
 *  camera is a "uniform mat4 mvp" evaluated in the vertex shader, so
 *  the transforms GLDirect pushes from the GL stacks stay at identity
 *  and Remix sees no camera at all.
 *
 *  When the bound program carries an MVP-style mat4 uniform, publish it
 *  as D3DTS_PROJECTION (with the GL [-1,1] -> D3D9 [0,1] clip-depth
 *  remap and the half-pixel correction, the same math as
 *  _glsBuildD3DProjection) and pin WORLD/VIEW to identity.  A D3D9
 *  vertex shader ignores the fixed-function transforms, so this is
 *  rendering-neutral for the shader draw itself; it only supplies the
 *  camera Remix needs.  A later fixed-function draw re-applies the
 *  GL-stack transforms in _glsApplyTransforms, so nothing leaks.
 * =================================================================== */

static BOOL _overlayMatrixIsPerspectiveCandidate(const float m[16])
{
    int i;
    int row, col;
    if (!m) return FALSE;
    for (i = 0; i < 16; ++i) {
        /* NaN, infinity and absurd sentinel values are never camera data. */
        if (m[i] != m[i] || m[i] > 1.0e20f || m[i] < -1.0e20f)
            return FALSE;
    }

    /* An affine/orthographic matrix has mathematical last row (0,0,0,1).
     * idTech's UI programs use exactly that form.  Publishing it after a 3D
     * draw overwrites the real camera and makes Remix reject the whole frame,
     * so only non-affine clip transforms are camera candidates. */
    if (m[3]  > -0.000001f && m[3]  < 0.000001f &&
        m[7]  > -0.000001f && m[7]  < 0.000001f &&
        m[11] > -0.000001f && m[11] < 0.000001f &&
        m[15] >  0.999999f && m[15] < 1.000001f)
        return FALSE;

    /* Packed id Tech vertex parameters are not guaranteed to begin with a
     * matrix.  Four unrelated vec4 values can satisfy the non-affine test but
     * still contain an all-zero row or column, as its screen-space programs
     * do.  A real projection/MVP is invertible, so reject those degenerate
     * blocks before they replace the known-good bootstrap camera. */
    for (row = 0; row < 4; ++row) {
        float magnitude = 0.0f;
        for (col = 0; col < 4; ++col) {
            float v = m[col * 4 + row];
            magnitude += (v < 0.0f) ? -v : v;
        }
        if (magnitude < 0.0000001f) return FALSE;
    }
    for (col = 0; col < 4; ++col) {
        float magnitude = 0.0f;
        for (row = 0; row < 4; ++row) {
            float v = m[col * 4 + row];
            magnitude += (v < 0.0f) ? -v : v;
        }
        if (magnitude < 0.0000001f) return FALSE;
    }
    return TRUE;
}

/* Find `packed [ row ... ]` without depending on the generator's whitespace
 * or on comments following the numeric index. */
static BOOL _overlaySourceUsesPackedRow(const char *source,
                                        const char *packed, int row)
{
    const char *p = source;
    size_t nameLen;
    if (!source || !packed || row < 0) return FALSE;
    nameLen = strlen(packed);

    while ((p = strstr(p, packed)) != NULL) {
        const char *q = p + nameLen;
        char *end = NULL;
        long index;
        if ((p > source &&
             ((p[-1] >= 'a' && p[-1] <= 'z') ||
              (p[-1] >= 'A' && p[-1] <= 'Z') ||
              (p[-1] >= '0' && p[-1] <= '9') || p[-1] == '_'))) {
            p = q;
            continue;
        }
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        if (*q++ != '[') { p = q; continue; }
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
        index = strtol(q, &end, 10);
        if (end != q && index == row) return TRUE;
        p = q;
    }
    return FALSE;
}

static BOOL _overlayFindCameraMatrix(GLS_Program *prog, float mvp[16],
                                     const char **matchedName)
{
    static unsigned char cameraCandidateLogged[GLS_MAX_PROGRAMS];
    static const char *const cameraNames[] = {
        "mvp", "modelViewProj", "modelviewproj", "modelViewProjection",
        "mViewProj", "viewProj", "viewProjection", "uMVP",
        "uModelViewProj", "uViewProjection", "matModelViewProj",
        "matModelViewProjection", "gl_ModelViewProjectionMatrix",
        "worldViewProj", "worldViewProjection", "wvp", "mvpMatrix",
        "cameraMatrix", "cameraToClip", "viewToClip", "projMatrix",
        NULL
    };
    int i;

    if (matchedName) *matchedName = NULL;
    if (!prog) return FALSE;

    for (i = 0; i < prog->resolvedCount && i < GLS_MAX_UNIFORMS; i++) {
        const char *name = prog->resolved[i].name;
        GLS_Uniform *u;
        int ni;

        if (!name[0]) continue;

        for (ni = 0; cameraNames[ni]; ni++) {
            if (_overlayTokenContains(name, cameraNames[ni])) {
                u = _overlayUniformAt(prog, i);
                if (u && u->set && u->type == 7) {  /* mat4 */
                    BOOL perspective;
                    memcpy(mvp, u->data, 16 * sizeof(float));
                    perspective = _overlayMatrixIsPerspectiveCandidate(mvp);
                    if (prog->id < GLS_MAX_PROGRAMS &&
                        !cameraCandidateLogged[prog->id]) {
                        cameraCandidateLogged[prog->id] = 1;
                        gldDiagLog("GL: camera candidate program=%u uniform=%s "
                                   "perspective=%d rows=[%.4g %.4g %.4g %.4g] "
                                   "[%.4g %.4g %.4g %.4g] [%.4g %.4g %.4g %.4g] "
                                   "[%.4g %.4g %.4g %.4g]",
                                   prog->id, name, perspective ? 1 : 0,
                                   mvp[0], mvp[4], mvp[8], mvp[12],
                                   mvp[1], mvp[5], mvp[9], mvp[13],
                                   mvp[2], mvp[6], mvp[10], mvp[14],
                                   mvp[3], mvp[7], mvp[11], mvp[15]);
                    }
                    if (!perspective)
                        continue;
                    if (matchedName) *matchedName = name;
                    return TRUE;
                }
            }
        }
    }

    /* Generated idTech GLSL packs vertex render parameters into a vec4 array
     * instead of exposing a mat4.  The game strips the helpful rpMVPmatrix
     * comments from some runtime variants, but its ABI is still stable: the
     * first four _va_ vectors are the four MVP rows and arrive together in one
     * glUniform4fv upload.  Accept the reflected four-register array and let
     * the matrix-shape test reject UI/orthographic parameter blocks.  Row
     * vectors are transposed into the column-major layout consumed below. */
    {
        const char *packedNames[] = { "_va_", "vertexParms", "vertexParams", NULL };
        int pi;

        for (pi = 0; packedNames[pi]; ++pi) {
            const char *packed = packedNames[pi];
            for (i = 0; i < prog->resolvedCount && i < GLS_MAX_UNIFORMS; ++i) {
                GLS_Uniform *u;
                int r, c;
                if (strcmp(prog->resolved[i].name, packed) != 0 ||
                    prog->resolved[i].registerCount < 4)
                    continue;
                u = _overlayUniformAt(prog, i);
                if (!u || !u->set || u->type != 4) continue;
                for (c = 0; c < 4; ++c)
                    for (r = 0; r < 4; ++r)
                        mvp[c * 4 + r] = u->data[r * 4 + c];
                if (prog->id < GLS_MAX_PROGRAMS &&
                    !cameraCandidateLogged[prog->id]) {
                    cameraCandidateLogged[prog->id] = 1;
                    gldDiagLog("GL: camera candidate program=%u packed=%s "
                               "perspective=%d rows=[%.4g %.4g %.4g %.4g] "
                               "[%.4g %.4g %.4g %.4g] [%.4g %.4g %.4g %.4g] "
                               "[%.4g %.4g %.4g %.4g]",
                               prog->id, prog->resolved[i].name,
                               _overlayMatrixIsPerspectiveCandidate(mvp) ? 1 : 0,
                               mvp[0], mvp[4], mvp[8], mvp[12],
                               mvp[1], mvp[5], mvp[9], mvp[13],
                               mvp[2], mvp[6], mvp[10], mvp[14],
                               mvp[3], mvp[7], mvp[11], mvp[15]);
                }
                if (!_overlayMatrixIsPerspectiveCandidate(mvp))
                    continue;
                if (matchedName) *matchedName = prog->resolved[i].name;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOL _overlayLegacyProjectionIsActive(GLS_State *s, GLS_Program *prog)
{
    const float *projection;
    int i;
    BOOL nonIdentity = FALSE;

    if (!s || !prog ||
        (prog->builtinMvpRegister < 0 &&
         prog->builtinModelViewRegister < 0 &&
         prog->builtinProjectionRegister < 0))
        return FALSE;

    projection = s->projectionStack.stack[s->projectionStack.top].m;
    for (i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        float delta = projection[i] - expected;
        if (delta > 0.000001f || delta < -0.000001f) {
            nonIdentity = TRUE;
            break;
        }
    }
    /* A non-identity legacy stack is often just the orthographic GUI matrix.
     * Do not let that permanently disable the bootstrap perspective before a
     * real 3D frame appears. */
    return nonIdentity && _overlayMatrixIsPerspectiveCandidate(projection);
}

static BOOL _overlayApplyCameraMatrix(IDirect3DDevice9 *pDev, GLS_State *s,
                                      const float mvp[16])
{
    D3DMATRIX proj;
    D3DMATRIX ident;
    D3DVIEWPORT9 viewport;
    IDirect3DSurface9 *pRT = NULL;
    D3DSURFACE_DESC desc;
    HRESULT hr;
    double desiredLeft, desiredTop;
    float adjust[4];
    float x1, x2, x3, x4, y1, y2, y3, y4;

    if (!pDev || !s || !mvp) return FALSE;

    /* _glsApplyTransforms has already installed the clipped D3D9 viewport for
     * this draw.  Read it back so the shader camera gets exactly the same
     * sub-viewport/clipping transform instead of assuming a full back buffer.
     * That distinction matters for split views, mirrors, HUD passes and FBOs. */
    ZeroMemory(&viewport, sizeof(viewport));
    ZeroMemory(&desc, sizeof(desc));
    __try {
        hr = IDirect3DDevice9_GetViewport(pDev, &viewport);
        if (FAILED(hr) || viewport.Width == 0 || viewport.Height == 0)
            return FALSE;
        if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(pDev, 0, &pRT)) && pRT) {
            IDirect3DSurface9_GetDesc(pRT, &desc);
            IDirect3DSurface9_Release(pRT);
            pRT = NULL;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        if (pRT) {
            IDirect3DSurface9_Release(pRT);
            pRT = NULL;
        }
        return FALSE;
    }

    /* Rebuild the same four viewport coefficients as
     * _glsBuildClippedViewport.  The render-target-height fallback makes the
     * desired top coincide with the viewport returned by D3D9; it preserves
     * the half-pixel correction even if GetDesc failed during device loss. */
    desiredLeft = (double)s->viewportX;
    if (desc.Height > 0) {
        desiredTop = (double)desc.Height -
                     ((double)s->viewportY + (double)s->viewportH);
    } else {
        desiredTop = (double)viewport.Y;
    }
    adjust[0] = (float)((double)s->viewportW / (double)viewport.Width);
    adjust[1] = (float)((double)s->viewportH / (double)viewport.Height);
    adjust[2] = (float)((2.0 * desiredLeft + (double)s->viewportW -
                         2.0 * (double)viewport.X - (double)viewport.Width) /
                        (double)viewport.Width -
                        1.0 / (double)viewport.Width);
    adjust[3] = (float)((2.0 * desiredTop + (double)s->viewportH -
                         2.0 * (double)viewport.Y - (double)viewport.Height) /
                        (double)viewport.Height +
                        1.0 / (double)viewport.Height);

    /* GL column-major floats -> D3DMATRIX (1:1 storage order). */
    proj._11 = mvp[0];  proj._12 = mvp[1];  proj._13 = mvp[2];  proj._14 = mvp[3];
    proj._21 = mvp[4];  proj._22 = mvp[5];  proj._23 = mvp[6];  proj._24 = mvp[7];
    proj._31 = mvp[8];  proj._32 = mvp[9];  proj._33 = mvp[10]; proj._34 = mvp[11];
    proj._41 = mvp[12]; proj._42 = mvp[13]; proj._43 = mvp[14]; proj._44 = mvp[15];

    /* GL [-1,1] -> D3D9 [0,1] clip depth, same rows as _glsBuildD3DProjection. */
    proj._13 = 0.5f * (proj._13 + proj._14);
    proj._23 = 0.5f * (proj._23 + proj._24);
    proj._33 = 0.5f * (proj._33 + proj._34);
    proj._43 = 0.5f * (proj._43 + proj._44);

    /* The translated VS already applies its viewport adjustment.  Keep the
     * semantic projection clean under Remix so its camera decomposition does
     * not mistake the DX9 half-pixel offset for projection shear. */
    if (!gldIsRemixDetected()) {
        x1 = proj._11; x2 = proj._21; x3 = proj._31; x4 = proj._41;
        y1 = proj._12; y2 = proj._22; y3 = proj._32; y4 = proj._42;

        proj._11 = adjust[0] * x1 + adjust[2] * proj._14;
        proj._21 = adjust[0] * x2 + adjust[2] * proj._24;
        proj._31 = adjust[0] * x3 + adjust[2] * proj._34;
        proj._41 = adjust[0] * x4 + adjust[2] * proj._44;

        proj._12 = adjust[1] * y1 + adjust[3] * proj._14;
        proj._22 = adjust[1] * y2 + adjust[3] * proj._24;
        proj._32 = adjust[1] * y3 + adjust[3] * proj._34;
        proj._42 = adjust[1] * y4 + adjust[3] * proj._44;
    }

    ZeroMemory(&ident, sizeof(ident));
    ident._11 = ident._22 = ident._33 = ident._44 = 1.0f;

    __try {
        if (FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &ident)) ||
            FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &ident)) ||
            FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &proj)))
            return FALSE;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }

    return TRUE;
}

/* Optional diagnostic bootstrap camera.  This must not be enabled by default:
 * id Tech starts with screen-space splash/menu programs, and presenting a fake
 * perspective at that point makes Remix classify the 2D front end as the main
 * 3D scene.  The result is a ray-traced magenta/menu-less frame even though the
 * UI textures themselves are valid.  Real perspective uniforms are published
 * unconditionally by the normal path above. */
static BOOL _overlayApplyBootstrapCamera(IDirect3DDevice9 *pDev)
{
    D3DVIEWPORT9 viewport;
    D3DMATRIX proj, ident;
    float aspect;

    if (!pDev) return FALSE;
    ZeroMemory(&viewport, sizeof(viewport));
    ZeroMemory(&proj, sizeof(proj));
    ZeroMemory(&ident, sizeof(ident));
    ident._11 = ident._22 = ident._33 = ident._44 = 1.0f;

    __try {
        if (FAILED(IDirect3DDevice9_GetViewport(pDev, &viewport)) ||
            viewport.Width == 0 || viewport.Height == 0)
            return FALSE;

        /* Right-handed D3D9 perspective: 75-degree vertical FOV, 0.1 to
         * 10000.  These values are deliberately ordinary so Remix's camera
         * decomposition has no shear or near-zero-FOV edge case. */
        aspect = (float)viewport.Width / (float)viewport.Height;
        proj._11 = 1.3032254f / aspect;
        proj._22 = 1.3032254f;
        proj._33 = -1.0000100f;
        proj._34 = -1.0f;
        proj._43 = -0.1000010f;

        if (FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &ident)) ||
            FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &ident)) ||
            FAILED(IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &proj)))
            return FALSE;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}

/* ===================================================================
 *  Per-draw entry point
 * =================================================================== */

BOOL gldApplySemanticOverlay(GLS_State *s, GLS_Program *prog)
{
    IDirect3DDevice9 *pDev;
    static BOOL s_armedLogged = FALSE;
    static BOOL s_lastWasDegraded = FALSE;
    static BOOL s_realCameraSeen = FALSE;
    static BOOL s_bootstrapCameraLogged = FALSE;
    int lightCount = 0, synthCount = 0;
    int lastStage;
    BOOL stageOverflow = FALSE, cubeUsed = FALSE;
    BOOL anyLight = FALSE, degrade;
    BOOL cameraPublished = FALSE;
    const char *cameraName = NULL;
    float cameraMatrix[16];
    int i;

    if (!s) return FALSE;
    pDev = gldGetD3DDevice46();
    if (!pDev) return FALSE;

    /* Camera/viewport publication is part of the core GL-to-D3D9 bridge, not
     * merely Remix's material overlay.  Keep it active for every D3D9 wrapper
     * layered below GLDirect.  The translated vertex shader ignores these
     * fixed-function transforms, so publishing them does not alter rendering. */
    if (_overlayFindCameraMatrix(prog, cameraMatrix, &cameraName)) {
        cameraPublished = _overlayApplyCameraMatrix(pDev, s, cameraMatrix);
        if (cameraPublished)
            s_realCameraSeen = TRUE;
        gldDiagLogV("GL: DX9 camera uniform=%s published=%d viewport=%d,%d %dx%d",
                    cameraName ? cameraName : "?", cameraPublished ? 1 : 0,
                    s->viewportX, s->viewportY, s->viewportW, s->viewportH);
    } else if (_overlayLegacyProjectionIsActive(s, prog)) {
        /* _glsApplyTransforms published the GL stacks immediately before this
         * draw.  Legacy GLSL built-ins consume the same matrices as native
         * shader constants, so report the already-active D3D9 camera instead
         * of the misleading camera=0 used by the uniform-only detector. */
        cameraPublished = TRUE;
        cameraName = "legacy GLSL matrix stack";
        gldDiagLogV("GL: DX9 camera source=%s published=1 viewport=%d,%d %dx%d",
                    cameraName, s->viewportX, s->viewportY,
                    s->viewportW, s->viewportH);
        s_realCameraSeen = TRUE;
    } else if (gldIsRemixDetected() && !s_realCameraSeen &&
               _overlayBootstrapCameraEnabled()) {
        cameraPublished = _overlayApplyBootstrapCamera(pDev);
        cameraName = "bootstrap perspective";
        if (cameraPublished && !s_bootstrapCameraLogged) {
            s_bootstrapCameraLogged = TRUE;
            gldDiagLog("GL: DX9 bootstrap perspective camera published for startup/menu draws");
        }
    }

    if (!gldSemanticOverlayEnabled()) return FALSE;

    if (!s_armedLogged) {
        s_armedLogged = TRUE;
        gldDiagLog("GL: semantic overlay armed (RTX Remix or GLDIRECT_SEMANTIC_DIAG)");
    }

    /* ---- Lights ---------------------------------------------------- */
    if (s->enableLighting) {
        for (i = 0; i < GLS_MAX_LIGHTS; i++) {
            if (s->lights[i].enabled) { anyLight = TRUE; break; }
        }
        if (anyLight) {
            lightCount = _overlayEmitMirroredLights(pDev, s);
        } else if (!s->lightsEverConfigured && prog && prog->linked) {
            /* OpenMW pattern: lighting entirely in GLSL, no glLight* ever ran.
             * Synthesize D3D lights from the program's light uniforms so Remix
             * sees the scene's lights even though the app never configured GL
             * lights. */
            synthCount = _overlaySynthLightCount(pDev, s, prog);
        } else {
            __try {
                for (i = 0; i < GLS_MAX_LIGHTS; i++)
                    IDirect3DDevice9_LightEnable(pDev, i, FALSE);
                IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, TRUE);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
        }
    } else {
        __try {
            for (i = 0; i < GLS_MAX_LIGHTS; i++)
                IDirect3DDevice9_LightEnable(pDev, i, FALSE);
            IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
    }

    /* ---- Material / ambient / specular ----------------------------- */
    _overlayEmitMaterial(pDev, s);

    /* ---- Fog -------------------------------------------------------- */
    _glsApplyFogState();

    /* ---- Texture stages -------------------------------------------- */
    lastStage = _overlayEmitStages(pDev, s, prog, &stageOverflow, &cubeUsed);

    /* ---- FFP degrade ------------------------------------------------ */
    degrade = _overlayFFPEligible(s, prog, stageOverflow, cubeUsed);
    if (degrade) {
        __try {
            IDirect3DDevice9_SetVertexShader(pDev, NULL);
            IDirect3DDevice9_SetPixelShader(pDev, NULL);
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
        s_lastWasDegraded = TRUE;
        gldDiagLogV("GL: semantic overlay FFP degrade program=%u", prog->id);
    } else if (s_lastWasDegraded) {
        /* A degrading draw left the pipeline shader-less; put it back for
         * this draw.  NULL programs were already fixed function. */
        if (prog && prog->pVS) {
            __try {
                IDirect3DDevice9_SetVertexShader(pDev, prog->pVS);
                IDirect3DDevice9_SetPixelShader(pDev, prog->pPS);
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
            gldDiagLogV("GL: semantic overlay re-bound translated shaders program=%u",
                        prog->id);
        }
        s_lastWasDegraded = FALSE;
    }

    gldDiagLogV("GL: semantic overlay draw lights=%d synth=%d material=%d fog=%d "
                "stages=%d degrade=%d camera=%d viewport=%d,%d %dx%d",
                lightCount, synthCount, 1, s->enableFog ? 1 : 0,
                lastStage + 1, degrade ? 1 : 0, cameraPublished ? 1 : 0,
                s->viewportX, s->viewportY, s->viewportW, s->viewportH);

    return degrade;
}

/* ===================================================================
 *  Geometry hash diagnostic
 * =================================================================== */

void gldLogDrawGeometry(D3DPRIMITIVETYPE primType, int primCount,
                        const void *verts, int vertCount,
                        const unsigned int *indices, int indexCount,
                        int indexFmt, const char *tag)
{
    unsigned int hash = 0x811C9DC5u;
    const unsigned char *p;
    size_t n, i;

    if (!gldSemanticOverlayEnabled()) return;
    if (!verts || !indices || vertCount <= 0 || indexCount <= 0) return;

    /* FNV-1a over: the exact vertex bytes, the exact index bytes, the
     * primitive type, the FVF, and the chosen index format.  Two draws are
     * byte-identical submissions iff the hashes match. */
    p = (const unsigned char *)verts;
    n = (size_t)vertCount * sizeof(GLS_D3DVertex);
    for (i = 0; i < n; i++) {
        hash ^= p[i];
        hash *= 0x01000193u;
    }
    p = (const unsigned char *)indices;
    n = (size_t)indexCount * sizeof(unsigned int);
    for (i = 0; i < n; i++) {
        hash ^= p[i];
        hash *= 0x01000193u;
    }
    hash ^= (unsigned int)primType;
    hash *= 0x01000193u;
    hash ^= GLS_D3DFVF;
    hash *= 0x01000193u;
    hash ^= (unsigned int)indexFmt;
    hash *= 0x01000193u;

    gldDiagLogV("GL: draw submit tag=%s prim=%d primCount=%d verts=%d idx=%d "
                "indexFmt=%d geoHash=%08X",
                tag ? tag : "?", (int)primType, primCount, vertCount, indexCount,
                indexFmt, hash);
}
