/*********************************************************************************
*  gl_state.c - OpenGL state machine implementation
*********************************************************************************/

#include "gl_state.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Single global GL state */
static GLS_State g_glState;

/* Forward declaration */
void glsInitState(void);

GLS_State* glsGetState(void)
{
    if (!g_glState.initialized)
        glsInitState();
    return &g_glState;
}

void glsMatrixIdentity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void glsMatrixMultiply(float *out, const float *a, const float *b)
{
    float tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j*4+i] = 0.0f;
            for (k = 0; k < 4; k++)
                tmp[j*4+i] += a[k*4+i] * b[j*4+k];
        }
    }
    memcpy(out, tmp, 16 * sizeof(float));
}

GLS_MatrixStack* glsGetCurrentMatrixStack(void)
{
    GLS_State *s = &g_glState;
    switch (s->matrixMode) {
    case 0x1701: /* GL_MODELVIEW */  return &s->modelviewStack;
    case 0x1702: /* GL_PROJECTION */ return &s->projectionStack;
    case 0x1702 + 0x100: /* GL_TEXTURE (0x1702) - actually 0x1702 is projection */
    default:
        break;
    }
    /* GL_TEXTURE = 0x1702? No, GL_TEXTURE = 0x1702 is wrong. Let me use correct values */
    /* GL_MODELVIEW = 0x1700, GL_PROJECTION = 0x1701, GL_TEXTURE = 0x1702 */
    /* Correction: */
    switch (s->matrixMode) {
    case 0x1700: return &s->modelviewStack;
    case 0x1701: return &s->projectionStack;
    case 0x1702: {
        int unit = (s->activeTexUnit >= 0x84C0) ? (s->activeTexUnit - 0x84C0) : 0;
        if (unit < 0 || unit >= GLS_MAX_TEX_UNITS) unit = 0;
        return &s->textureStack[unit];
    }
    }
    return &s->modelviewStack;
}

static void _initMatrixStack(GLS_MatrixStack *stack)
{
    stack->top = 0;
    glsMatrixIdentity(stack->stack[0].m);
}

static void _initMaterial(GLS_Material *mat, BOOL front)
{
    mat->ambient[0] = 0.2f; mat->ambient[1] = 0.2f; mat->ambient[2] = 0.2f; mat->ambient[3] = 1.0f;
    mat->diffuse[0] = 0.8f; mat->diffuse[1] = 0.8f; mat->diffuse[2] = 0.8f; mat->diffuse[3] = 1.0f;
    mat->specular[0] = 0.0f; mat->specular[1] = 0.0f; mat->specular[2] = 0.0f; mat->specular[3] = 1.0f;
    mat->emission[0] = 0.0f; mat->emission[1] = 0.0f; mat->emission[2] = 0.0f; mat->emission[3] = 1.0f;
    mat->shininess = 0.0f;
}

static void _initLight(GLS_Light *light, int index)
{
    memset(light, 0, sizeof(GLS_Light));
    light->ambient[3] = 1.0f;
    if (index == 0) {
        light->diffuse[0] = light->diffuse[1] = light->diffuse[2] = light->diffuse[3] = 1.0f;
        light->specular[0] = light->specular[1] = light->specular[2] = light->specular[3] = 1.0f;
    }
    light->position[2] = 1.0f; /* Default: directional from +Z */
    light->spotDirection[2] = -1.0f;
    light->spotCutoff = 180.0f;
    light->constantAttenuation = 1.0f;
}

void glsInitState(void)
{
    GLS_State *s = &g_glState;
    int i;

    memset(s, 0, sizeof(GLS_State));

    /* ID counters start at 1 (0 is reserved/default) */
    s->nextTexId = 1;
    s->nextBufId = 1;
    s->nextVaoId = 1;
    s->nextFboId = 1;
    s->nextRboId = 1;
    s->nextShaderId = 1;
    s->nextProgramId = 1;
    s->nextSamplerId = 1;
    s->nextQueryId = 1;

    /* Default render state */
    s->clearColor[0] = s->clearColor[1] = s->clearColor[2] = 0.0f;
    s->clearColor[3] = 0.0f;
    s->clearDepth = 1.0;
    s->clearStencil = 0;

    s->blendSrcRGB = 1;    /* GL_ONE */
    s->blendDstRGB = 0;    /* GL_ZERO */
    s->blendSrcAlpha = 1;
    s->blendDstAlpha = 0;
    s->blendEquationRGB = 0x8006;   /* GL_FUNC_ADD */
    s->blendEquationAlpha = 0x8006;

    s->depthFunc = 0x0201;  /* GL_LESS */
    s->depthMask = 1;       /* GL_TRUE */
    s->depthRangeNear = 0.0f;
    s->depthRangeFar = 1.0f;

    s->cullFaceMode = 0x0405;   /* GL_BACK */
    s->frontFace = 0x0901;      /* GL_CCW */

    s->colorMask[0] = s->colorMask[1] = s->colorMask[2] = s->colorMask[3] = 1;

    s->polygonModeFront = s->polygonModeBack = 0x1B02; /* GL_FILL */
    s->lineWidth = 1.0f;
    s->pointSize = 1.0f;

    s->alphaFunc = 0x0207;  /* GL_ALWAYS */
    s->alphaRef = 0.0f;

    s->stencilFunc = 0x0207;    /* GL_ALWAYS */
    s->stencilRef = 0;
    s->stencilMask = 0xFFFFFFFF;
    s->stencilFail = 0x1E00;    /* GL_KEEP */
    s->stencilZFail = 0x1E00;
    s->stencilZPass = 0x1E00;
    s->stencilWriteMask = 0xFFFFFFFF;

    /* Matrix stacks */
    s->matrixMode = 0x1700; /* GL_MODELVIEW */
    _initMatrixStack(&s->modelviewStack);
    _initMatrixStack(&s->projectionStack);
    for (i = 0; i < GLS_MAX_TEX_UNITS; i++)
        _initMatrixStack(&s->textureStack[i]);

    /* Lighting */
    for (i = 0; i < GLS_MAX_LIGHTS; i++)
        _initLight(&s->lights[i], i);
    _initMaterial(&s->materialFront, TRUE);
    _initMaterial(&s->materialBack, FALSE);
    s->lightModelAmbient[0] = s->lightModelAmbient[1] = s->lightModelAmbient[2] = 0.2f;
    s->lightModelAmbient[3] = 1.0f;

    /* Fog */
    s->fogMode = 0x0800;   /* GL_EXP */
    s->fogDensity = 1.0f;
    s->fogStart = 0.0f;
    s->fogEnd = 1.0f;

    /* Immediate mode */
    s->currentColor[0] = s->currentColor[1] = s->currentColor[2] = s->currentColor[3] = 1.0f;
    s->currentNormal[2] = 1.0f; /* Default normal = (0,0,1) */
    s->immVertexCapacity = 1024;
    s->immVertices = (GLS_ImmVertex *)malloc(s->immVertexCapacity * sizeof(GLS_ImmVertex));

    /* Pixel store */
    s->unpackAlignment = 4;
    s->packAlignment = 4;

    /* Active texture unit */
    s->activeTexUnit = 0x84C0; /* GL_TEXTURE0 */

    s->lastError = 0; /* GL_NO_ERROR */
    s->initialized = TRUE;
}

/* ===== Object lookup helpers ===== */

GLS_Texture* glsFindTexture(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_TEXTURES) return NULL;
    if (!g_glState.textures[id].allocated) return NULL;
    return &g_glState.textures[id];
}

GLS_Buffer* glsFindBuffer(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_BUFFERS) return NULL;
    if (!g_glState.buffers[id].allocated) return NULL;
    return &g_glState.buffers[id];
}

GLS_VAO* glsFindVAO(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_VAOS) return NULL;
    if (!g_glState.vaos[id].allocated) return NULL;
    return &g_glState.vaos[id];
}

GLS_FBO* glsFindFBO(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_FBOS) return NULL;
    if (!g_glState.fbos[id].allocated) return NULL;
    return &g_glState.fbos[id];
}

GLS_RBO* glsFindRBO(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_RBOS) return NULL;
    if (!g_glState.rbos[id].allocated) return NULL;
    return &g_glState.rbos[id];
}

GLS_Shader* glsFindShader(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_SHADERS) return NULL;
    if (!g_glState.shaders[id].allocated) return NULL;
    return &g_glState.shaders[id];
}

GLS_Program* glsFindProgram(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_PROGRAMS) return NULL;
    if (!g_glState.programs[id].allocated) return NULL;
    return &g_glState.programs[id];
}

GLS_Sampler* glsFindSampler(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_SAMPLERS) return NULL;
    if (!g_glState.samplers[id].allocated) return NULL;
    return &g_glState.samplers[id];
}

GLS_Query* glsFindQuery(GLuint_t id)
{
    if (id == 0 || id >= GLS_MAX_QUERIES) return NULL;
    if (!g_glState.queries[id].allocated) return NULL;
    return &g_glState.queries[id];
}
