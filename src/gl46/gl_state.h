/*********************************************************************************
*
*  gl_state.h - OpenGL state machine for the DX9 wrapper
*
*  Tracks GL objects (textures, buffers, shaders, VAOs, FBOs) and render state.
*  All GL calls go through this state machine, which translates to D3D9.
*
*********************************************************************************/

#ifndef GL_STATE_H
#define GL_STATE_H

#include <windows.h>
#include <d3d9.h>

/* Minimal GL types — avoid glad macro conflicts */
typedef unsigned int GLenum_t;
typedef int GLint_t;
typedef unsigned int GLuint_t;
typedef int GLsizei_t;
typedef float GLfloat_t;
typedef unsigned char GLboolean_t;
typedef unsigned int GLbitfield_t;
typedef ptrdiff_t GLsizeiptr_t;
typedef ptrdiff_t GLintptr_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Object limits ===== */
#define GLS_MAX_TEXTURES        4096
#define GLS_MAX_BUFFERS         4096
#define GLS_MAX_VAOS            256
#define GLS_MAX_FBOS            256
#define GLS_MAX_RBOS            256
#define GLS_MAX_SHADERS         1024
#define GLS_MAX_PROGRAMS        512
#define GLS_MAX_QUERIES         256
#define GLS_MAX_SAMPLERS        64
#define GLS_MAX_TEX_UNITS       8
#define GLS_MAX_VERTEX_ATTRIBS  16
#define GLS_MAX_MATRIX_STACK    32
#define GLS_MAX_LIGHTS          8
#define GLS_MAX_CLIP_PLANES     6

/* ===== Texture object ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;         /* GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc. */
    GLint_t             width, height, depth;
    GLenum_t            internalFormat;
    GLenum_t            minFilter, magFilter;
    GLenum_t            wrapS, wrapT, wrapR;
    IDirect3DTexture9   *pTex;          /* D3D9 texture (NULL until data uploaded) */
    IDirect3DCubeTexture9 *pCubeTex;
    void                *pixelData;     /* CPU-side copy for readback */
    GLsizei_t           pixelDataSize;
    BOOL                allocated;
} GLS_Texture;

/* ===== Buffer object ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;         /* GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc. */
    GLenum_t            usage;
    void                *data;          /* CPU-side buffer data */
    GLsizeiptr_t        size;
    IDirect3DVertexBuffer9 *pVB;
    IDirect3DIndexBuffer9  *pIB;
    BOOL                mapped;
    BOOL                allocated;
} GLS_Buffer;

/* ===== Vertex attrib pointer ===== */
typedef struct {
    GLint_t             size;           /* 1-4 */
    GLenum_t            type;           /* GL_FLOAT, GL_UNSIGNED_BYTE, etc. */
    GLboolean_t         normalized;
    GLsizei_t           stride;
    const void          *pointer;
    GLuint_t            bufferBinding;  /* VBO bound when this was set */
    BOOL                enabled;
    BOOL                integer;        /* set via VertexAttribIPointer */
    float               defaultValue[4]; /* default attrib value (x,y,z,w) */
    GLuint_t            divisor;        /* instanced rendering divisor */
} GLS_VertexAttrib;

/* ===== VAO ===== */
typedef struct {
    GLuint_t            id;
    GLS_VertexAttrib    attribs[GLS_MAX_VERTEX_ATTRIBS];
    GLuint_t            elementBuffer;  /* GL_ELEMENT_ARRAY_BUFFER binding */
    BOOL                allocated;
} GLS_VAO;

/* ===== FBO ===== */
#define GLS_MAX_DRAW_BUFFERS 8
typedef struct {
    GLuint_t            id;
    GLuint_t            colorAttachment[4];
    GLenum_t            colorAttachTarget[4];
    GLint_t             colorAttachLevel[4];
    GLuint_t            colorAttachRB[4];       /* renderbuffer attachments */
    GLuint_t            depthAttachment;
    GLuint_t            stencilAttachment;
    GLuint_t            depthStencilAttachment;
    GLuint_t            depthAttachRB;
    GLuint_t            stencilAttachRB;
    GLuint_t            depthStencilAttachRB;
    BOOL                allocated;
} GLS_FBO;

/* ===== RBO ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            internalFormat;
    GLsizei_t           width, height;
    GLsizei_t           samples;
    BOOL                allocated;
} GLS_RBO;

/* ===== Shader ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            type;           /* GL_VERTEX_SHADER, GL_FRAGMENT_SHADER */
    char                *source;
    BOOL                compiled;
    BOOL                allocated;
} GLS_Shader;

/* ===== Uniform storage ===== */
#define GLS_MAX_UNIFORMS 256
typedef struct {
    int                 location;
    int                 type;           /* 0=int, 1=float, 2=vec2, 3=vec3, 4=vec4, 5=mat2, 6=mat3, 7=mat4 */
    float               data[16];       /* enough for a mat4 */
    BOOL                set;
} GLS_Uniform;

/* ===== Attrib binding ===== */
#define GLS_MAX_ATTRIB_BINDINGS 16
typedef struct {
    GLuint_t            index;
    char                name[64];
    BOOL                set;
} GLS_AttribBinding;

/* ===== Program ===== */
typedef struct {
    GLuint_t            id;
    GLuint_t            vertShader;
    GLuint_t            fragShader;
    BOOL                linked;
    BOOL                validated;
    BOOL                allocated;
    GLS_Uniform         uniforms[GLS_MAX_UNIFORMS];
    int                 uniformCount;
    GLS_AttribBinding   attribBindings[GLS_MAX_ATTRIB_BINDINGS];
    int                 attribBindingCount;
} GLS_Program;

/* ===== Sampler ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            minFilter, magFilter;
    GLenum_t            wrapS, wrapT, wrapR;
    float               borderColor[4];
    float               minLod, maxLod;
    float               lodBias;
    GLenum_t            compareMode, compareFunc;
    float               maxAnisotropy;
    BOOL                allocated;
} GLS_Sampler;

/* ===== Query ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;
    BOOL                active;
    GLuint_t            result;
    BOOL                allocated;
} GLS_Query;

/* ===== Matrix stack ===== */
typedef struct {
    float               m[16];          /* Column-major 4x4 */
} GLS_Matrix;

typedef struct {
    GLS_Matrix          stack[GLS_MAX_MATRIX_STACK];
    int                 top;            /* Index of current matrix */
} GLS_MatrixStack;

/* ===== Light ===== */
typedef struct {
    BOOL                enabled;
    float               ambient[4];
    float               diffuse[4];
    float               specular[4];
    float               position[4];
    float               spotDirection[3];
    float               spotExponent;
    float               spotCutoff;
    float               constantAttenuation;
    float               linearAttenuation;
    float               quadraticAttenuation;
} GLS_Light;

/* ===== Material ===== */
typedef struct {
    float               ambient[4];
    float               diffuse[4];
    float               specular[4];
    float               emission[4];
    float               shininess;
} GLS_Material;

/* ===== Immediate mode vertex ===== */
typedef struct {
    float               pos[4];
    float               color[4];
    float               normal[3];
    float               texcoord[GLS_MAX_TEX_UNITS][4];
} GLS_ImmVertex;

/* ===== Main GL state ===== */
typedef struct {
    /* Object pools */
    GLS_Texture         textures[GLS_MAX_TEXTURES];
    GLS_Buffer          buffers[GLS_MAX_BUFFERS];
    GLS_VAO             vaos[GLS_MAX_VAOS];
    GLS_FBO             fbos[GLS_MAX_FBOS];
    GLS_RBO             rbos[GLS_MAX_RBOS];
    GLS_Shader          shaders[GLS_MAX_SHADERS];
    GLS_Program         programs[GLS_MAX_PROGRAMS];
    GLS_Sampler         samplers[GLS_MAX_SAMPLERS];
    GLS_Query           queries[GLS_MAX_QUERIES];

    /* ID counters */
    GLuint_t            nextTexId;
    GLuint_t            nextBufId;
    GLuint_t            nextVaoId;
    GLuint_t            nextFboId;
    GLuint_t            nextRboId;
    GLuint_t            nextShaderId;
    GLuint_t            nextProgramId;
    GLuint_t            nextSamplerId;
    GLuint_t            nextQueryId;

    /* Current bindings */
    GLuint_t            boundTexture2D[GLS_MAX_TEX_UNITS];
    GLuint_t            boundTextureCube[GLS_MAX_TEX_UNITS];
    GLuint_t            boundArrayBuffer;
    GLuint_t            boundElementBuffer;
    GLuint_t            boundVAO;
    GLuint_t            boundFBO;
    GLuint_t            boundRBO;
    GLuint_t            boundProgram;
    GLenum_t            activeTexUnit;  /* GL_TEXTURE0 + n */

    /* Render state */
    float               clearColor[4];
    float               clearDepth;
    int                 clearStencil;
    GLenum_t            blendSrcRGB, blendDstRGB;
    GLenum_t            blendSrcAlpha, blendDstAlpha;
    GLenum_t            blendEquationRGB, blendEquationAlpha;
    GLenum_t            depthFunc;
    GLboolean_t         depthMask;
    float               depthRangeNear, depthRangeFar;
    GLenum_t            cullFaceMode;
    GLenum_t            frontFace;
    GLboolean_t         colorMask[4];
    GLenum_t            polygonModeFront, polygonModeBack;
    float               polygonOffsetFactor, polygonOffsetUnits;
    float               lineWidth;
    float               pointSize;
    GLenum_t            alphaFunc;
    float               alphaRef;
    GLenum_t            stencilFunc;
    int                 stencilRef;
    unsigned int        stencilMask;
    GLenum_t            stencilFail, stencilZFail, stencilZPass;
    unsigned int        stencilWriteMask;

    /* Back-face stencil (separate) */
    GLenum_t            stencilBackFunc;
    int                 stencilBackRef;
    unsigned int        stencilBackMask;
    GLenum_t            stencilBackFail, stencilBackZFail, stencilBackZPass;
    unsigned int        stencilBackWriteMask;

    int                 scissorX, scissorY, scissorW, scissorH;
    int                 viewportX, viewportY, viewportW, viewportH;

    /* Draw buffers */
    GLenum_t            drawBuffers[GLS_MAX_DRAW_BUFFERS];
    int                 drawBufferCount;

    /* Blend equation */
    GLenum_t            blendColor[4]; /* stored as float in clearColor style */
    float               blendColorF[4];

    /* Transform feedback */
    BOOL                transformFeedbackActive;
    GLenum_t            transformFeedbackMode;

    /* Buffer binding points (UBO, TFB, etc.) */
    GLuint_t            boundUniformBuffer;
    GLuint_t            boundCopyReadBuffer;
    GLuint_t            boundCopyWriteBuffer;
    GLuint_t            boundTransformFeedbackBuffer;

    /* Primitive restart */
    GLuint_t            primitiveRestartIndex;
    BOOL                enablePrimitiveRestart;

    /* Sync */
    GLuint_t            nextSyncId;

    /* Sampler bindings */
    GLuint_t            boundSampler[GLS_MAX_TEX_UNITS];

    /* Clip control (GL 4.5) */
    GLenum_t            clipOrigin;
    GLenum_t            clipDepthMode;

    /* Debug callback (GL 4.3) */
    void                *debugCallback;
    const void          *debugUserParam;

    /* Provoking vertex */
    GLenum_t            provokingVertexMode;

    /* Enable flags */
    BOOL                enableDepthTest;
    BOOL                enableBlend;
    BOOL                enableCullFace;
    BOOL                enableScissorTest;
    BOOL                enableStencilTest;
    BOOL                enableAlphaTest;
    BOOL                enableFog;
    BOOL                enableLighting;
    BOOL                enableTexture2D[GLS_MAX_TEX_UNITS];
    BOOL                enableTextureCubeMap[GLS_MAX_TEX_UNITS];
    BOOL                enablePolygonOffsetFill;
    BOOL                enableMultisample;
    BOOL                enableColorMaterial;
    BOOL                enableNormalize;

    /* Matrix state */
    GLenum_t            matrixMode;     /* GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE */
    GLS_MatrixStack     modelviewStack;
    GLS_MatrixStack     projectionStack;
    GLS_MatrixStack     textureStack[GLS_MAX_TEX_UNITS];

    /* Lighting */
    GLS_Light           lights[GLS_MAX_LIGHTS];
    GLS_Material        materialFront;
    GLS_Material        materialBack;
    float               lightModelAmbient[4];
    BOOL                lightModelTwoSide;

    /* Fog */
    GLenum_t            fogMode;
    float               fogColor[4];
    float               fogDensity;
    float               fogStart;
    float               fogEnd;

    /* Immediate mode */
    BOOL                inBeginEnd;
    GLenum_t            beginMode;
    GLS_ImmVertex       *immVertices;
    int                 immVertexCount;
    int                 immVertexCapacity;
    float               currentColor[4];
    float               currentNormal[3];
    float               currentTexCoord[GLS_MAX_TEX_UNITS][4];

    /* Pixel store */
    int                 unpackAlignment;
    int                 packAlignment;
    int                 unpackRowLength;
    int                 packRowLength;

    /* Error */
    GLenum_t            lastError;

    /* Initialized flag */
    BOOL                initialized;
} GLS_State;

/* ===== Global state accessor ===== */
GLS_State* glsGetState(void);

/* ===== Initialization ===== */
void glsInitState(void);

/* ===== ID helpers ===== */
GLS_Texture*  glsFindTexture(GLuint_t id);
GLS_Buffer*   glsFindBuffer(GLuint_t id);
GLS_VAO*      glsFindVAO(GLuint_t id);
GLS_FBO*      glsFindFBO(GLuint_t id);
GLS_RBO*      glsFindRBO(GLuint_t id);
GLS_Shader*   glsFindShader(GLuint_t id);
GLS_Program*  glsFindProgram(GLuint_t id);
GLS_Sampler*  glsFindSampler(GLuint_t id);
GLS_Query*    glsFindQuery(GLuint_t id);

/* ===== Matrix helpers ===== */
void glsMatrixIdentity(float *m);
void glsMatrixMultiply(float *out, const float *a, const float *b);
GLS_MatrixStack* glsGetCurrentMatrixStack(void);

#ifdef __cplusplus
}
#endif

#endif /* GL_STATE_H */
