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
/* D3D9 Shader Model 3 exposes sixteen pixel-sampler stages (s0-s15).
 * Clamping OpenGL units 8-15 back onto unit zero made id Tech's later image
 * binds overwrite the UI/font texture and left Remix reporting "Texture 0
 * without valid hash". */
#define GLS_MAX_TEX_UNITS       16
#define GLS_MAX_VERTEX_ATTRIBS  16
#define GLS_MAX_MATRIX_STACK    32
#define GLS_MAX_LIGHTS          8
#define GLS_MAX_CLIP_PLANES     6
#define GLS_MAX_IMAGE_UNITS     8
#define GLS_MAX_BUFFER_BINDINGS 16
#define GLS_MAX_STAGE_VARYINGS  8

/* ===== Texture object ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;         /* GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc. */
    GLint_t             width, height, depth;
    GLint_t             samples;
    GLboolean_t         fixedSampleLocations;
    GLenum_t            internalFormat;
    GLenum_t            minFilter, magFilter;
    GLenum_t            wrapS, wrapT, wrapR;
    GLint_t             baseLevel;      /* GL_TEXTURE_BASE_LEVEL (0 default) */
    GLint_t             maxLevel;       /* GL_TEXTURE_MAX_LEVEL (1000 default) */
    float               minLod;         /* GL_TEXTURE_MIN_LOD (-1000 default)   */
    float               maxLod;         /* GL_TEXTURE_MAX_LOD (+1000 default)   */
    float               maxAnisotropy;  /* GL_TEXTURE_MAX_ANISOTROPY_EXT (1.0)  */
    IDirect3DTexture9   *pTex;          /* D3D9 texture (NULL until data uploaded) */
    /* RTX Remix forces mip filtering back on whenever linear min/mag
     * filtering is requested.  A one-level view of a GL texture whose
     * BASE_LEVEL == MAX_LEVEL prevents that override from reading undefined
     * allocation-tail mips while preserving linear filtering. */
    IDirect3DTexture9   *pSingleLevelTex;
    BOOL                singleLevelDirty;
    IDirect3DCubeTexture9 *pCubeTex;
    IDirect3DVolumeTexture9 *pVolTex;   /* GL_TEXTURE_3D storage */
    GLuint_t            bufferObject;  /* GL_TEXTURE_BUFFER source buffer */
    GLintptr_t          bufferOffset;  /* first byte exposed by TexBufferRange */
    GLsizeiptr_t        bufferSize;    /* zero means the complete buffer */
    void                *pixelData;     /* CPU-side copy for readback */
    GLsizei_t           pixelDataSize;
    BOOL                allocated;
} GLS_Texture;

typedef struct {
    GLuint_t            texture;
    GLint_t             level;
    GLboolean_t         layered;
    GLint_t             layer;
    GLenum_t            access;
    GLenum_t            format;
} GLS_ImageBinding;

typedef struct {
    GLuint_t            buffer;
    GLintptr_t          offset;
    GLsizeiptr_t        size;
} GLS_IndexedBufferBinding;

/* ===== Buffer object ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;         /* GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc. */
    GLenum_t            usage;
    GLbitfield_t        storageFlags;
    BOOL                immutable;
    void                *data;          /* CPU-side buffer data */
    GLsizeiptr_t        size;
    IDirect3DVertexBuffer9 *pVB;
    IDirect3DIndexBuffer9  *pIB;
    BOOL                mapped;
    GLintptr_t          mapOffset;
    GLsizeiptr_t        mapLength;
    GLbitfield_t        mapAccess;
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
    GLuint_t            bindingIndex;   /* GL 4.3 vertex binding slot */
    GLuint_t            relativeOffset; /* byte offset inside that slot */
} GLS_VertexAttrib;

/* GL 4.3 split vertex-format/buffer binding model.  D3D9 exposes the same
 * concept as vertex streams, while the wrapper's CPU assembly path consumes
 * it by resolving each attribute to buffer + offset + stride before a draw. */
typedef struct {
    GLuint_t            buffer;
    GLintptr_t          offset;
    GLsizei_t           stride;
    GLuint_t            divisor;
} GLS_VertexBinding;

/* ===== VAO ===== */
typedef struct {
    GLuint_t            id;
    GLS_VertexAttrib    attribs[GLS_MAX_VERTEX_ATTRIBS];
    GLS_VertexBinding   bindings[GLS_MAX_VERTEX_ATTRIBS];
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
    /* D3D9 cannot bind the wrapper's D3DPOOL_MANAGED sampling textures as
     * render targets.  Each active colour attachment therefore owns a
     * DEFAULT-pool render surface while it is attached; gl_impl resolves that
     * surface back into the sampling texture when the draw FBO is left. */
    IDirect3DSurface9   *colorRenderTarget[4];
    BOOL                colorRenderTargetDirty[4];
    GLuint_t            depthAttachment;
    GLuint_t            stencilAttachment;
    GLuint_t            depthStencilAttachment;
    GLuint_t            depthAttachRB;
    GLuint_t            stencilAttachRB;
    GLuint_t            depthStencilAttachRB;
    IDirect3DSurface9   *depthStencilTarget;
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

/* ===== ARB assembly program (ARB_vertex_program / ARB_fragment_program) =====
 *
 * Hung off GLS_Shader rather than living in a pool of its own: ARB programs
 * share the shader ID space (glGenProgramsARB hands out shader IDs), and a
 * second pool would mean a second, colliding ID space.  It is allocated on
 * demand because only a handful of the 1024 shader slots are ever ARB
 * programs, and the two 96-entry parameter arrays are too large to give every
 * slot unconditionally.
 */
#define GLS_MAX_PROGRAM_PARAMS 96

typedef struct {
    IDirect3DVertexShader9  *pVS;       /* one of the two, per program target */
    IDirect3DPixelShader9   *pPS;

    /* Constant registers the translated program's uniform arrays landed on,
     * discovered by reflecting the compiled bytecode's constant table.
     * -1 means the program does not read that array at all. */
    int                 envBaseReg,   envRegCount;
    int                 localBaseReg, localRegCount;
    int                 stateBaseReg, stateRegCount;

    float               envParams[GLS_MAX_PROGRAM_PARAMS][4];
    float               localParams[GLS_MAX_PROGRAM_PARAMS][4];

    BOOL                usesStateMatrices;
    BOOL                usesStateLight;
    BOOL                usesStateFog;
} GLS_ARBProgram;

/* ===== Shader ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            type;           /* GL_VERTEX_SHADER, GL_FRAGMENT_SHADER */
    char                *source;
    BOOL                compiled;
    BOOL                allocated;
    GLS_ARBProgram      *arb;           /* non-NULL once glProgramStringARB ran */
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

typedef struct {
    char                name[64];
    GLint_t             components;
    GLint_t             location;
    BOOL                isFlat;
    BOOL                isInteger;
    BOOL                isUnsigned;
} GLS_StageVarying;

typedef struct {
    char                name[64];
    GLenum_t            type;
    GLint_t             components;
    GLint_t             arraySize;
    GLint_t             offset;
    GLint_t             bytes;
    GLint_t             userIndex;
} GLS_CaptureField;

/* ===== Resolved uniform =====
 *
 * One GL uniform name and the D3D9 constant registers it landed on.  The
 * vertex and pixel shaders are compiled separately and the HLSL compiler
 * allocates registers independently, so the same name usually sits at a
 * different register in each — hence both are tracked, with -1 meaning the
 * uniform is not present in that shader.
 */
typedef struct {
    char                name[64];
    int                 vsRegister;
    int                 psRegister;
    int                 registerCount;
    int                 registerSet;    /* GLSL_RS_* */
} GLS_ResolvedUniform;

/* ===== Synthesized texture-dimension uniform =====
 *
 * The GLSL->HLSL transpiler lowers texelFetch/textureSize onto tex2Dlod plus a
 * "_glsl_texdim_<sampler>" uniform holding (width, height, 1/width, 1/height):
 * SM3 has neither integer texel addressing nor a runtime size query, but both
 * fall out of that one constant.  Nothing in GL sets it, so the draw path has
 * to, which means knowing which constant register it landed on and which
 * sampler's texture it describes.  Both come from the reflected constant table
 * when the program links.
 */
#define GLS_MAX_TEXDIM  GLS_MAX_TEX_UNITS
typedef struct {
    int                 vsRegister;     /* register of _glsl_texdim_<name>, -1 if absent */
    int                 psRegister;
    int                 samplerPsRegister;  /* the sampler's own register = D3D9 stage */
} GLS_TexDimBinding;

/* ===== Active vertex attribute (glGetActiveAttrib) ===== */
typedef struct {
    char                name[64];
    GLenum_t            type;           /* GL_FLOAT_VEC4 etc. */
    GLint_t             size;           /* array elements, 1 for a scalar */
} GLS_ActiveAttrib;

/* ===== Program ===== */
typedef struct {
    GLuint_t            id;
    GLuint_t            vertShader;
    GLuint_t            fragShader;
    GLuint_t            geomShader;
    GLuint_t            tessControlShader;
    GLuint_t            tessEvalShader;
    GLuint_t            computeShader;
    BOOL                linked;
    BOOL                validated;
    BOOL                separable;
    BOOL                binaryRetrievable;
    BOOL                allocated;
    GLS_Uniform         uniforms[GLS_MAX_UNIFORMS];
    int                 uniformCount;
    GLS_AttribBinding   attribBindings[GLS_MAX_ATTRIB_BINDINGS];
    int                 attribBindingCount;

    /* Compiled D3D9 shader objects and their uniform register mapping */
    IDirect3DVertexShader9 *pVS;
    IDirect3DPixelShader9  *pPS;
    GLS_ResolvedUniform resolved[GLS_MAX_UNIFORMS];
    int                 resolvedCount;

    /* Sampler uniforms are program state in GL, while D3D9 texture stages are
     * device-global.  Keeping this mapping on GLS_State made the last program
     * to set sampler s0 silently redirect every other program's s0 after a
     * glUseProgram switch — exactly the missing/pink UI texture failure in
     * id Tech. */
    unsigned char       samplerStageUnit[GLS_MAX_TEX_UNITS];
    BOOL                samplerStageSet[GLS_MAX_TEX_UNITS];
    BOOL                samplerMissingLogged;

    /* Synthesized _glsl_texdim_* uniforms this program's shaders declare.
     * Empty for every program that does not use texelFetch/textureSize, which
     * is what keeps the per-draw cost at a single count check. */
    GLS_TexDimBinding   texDim[GLS_MAX_TEXDIM];
    int                 texDimCount;
    int                 viewportRegister; /* synthesized VS clip/viewport adjustment */
    int                 builtinMvpRegister;
    int                 builtinModelViewRegister;
    int                 builtinProjectionRegister;

    /* Vertex attribute names.  These do not survive into D3D9 bytecode,
     * so they are captured from the GLSL source when the program links;
     * glGetActiveAttrib has no other source for them. */
    GLS_ActiveAttrib    activeAttribs[GLS_MAX_ATTRIB_BINDINGS];
    int                 activeAttribCount;
    BOOL                softwareGraphicsStages;
    BOOL                softwareVertexExecution;
    BOOL                softwareFragmentExecution;

    /* Both stages are structurally equivalent to what the D3D9 fixed-function
     * pipeline can produce (conservative scan performed at link time).  A
     * program marked TRUE may be degraded to FFP while Remix is active so the
     * scene reconstruction sees a classic, reliably classified draw. */
    BOOL                ffpEquivalent;
    GLS_StageVarying    stageVaryings[GLS_MAX_STAGE_VARYINGS];
    int                 stageVaryingCount;
    int                 stageCaptureWords;
    GLS_CaptureField    captureFields[2 + GLS_MAX_STAGE_VARYINGS + GLS_MAX_VERTEX_ATTRIBS];
    int                 captureFieldCount;
    int                 captureStride;
    GLenum_t            transformFeedbackMode;
    int                 transformFeedbackCount;
    char                transformFeedbackVaryings[GLS_MAX_VERTEX_ATTRIBS][64];
    char                infoLog[512];
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

/* ===== Texture environment (fixed-function combiners) =====
 *
 * One per texture unit.  These drive D3D9's texture stage states, which are
 * the direct analogue of GL's texture environment: GL_COMBINE's source and
 * operand triples map onto D3DTSS_COLORARG1/2/0 and their alpha counterparts.
 */
typedef struct {
    GLenum_t            mode;               /* MODULATE/REPLACE/DECAL/BLEND/ADD/COMBINE */
    float               envColor[4];
    GLenum_t            combineRGB, combineAlpha;
    GLenum_t            srcRGB[3], srcAlpha[3];
    GLenum_t            operandRGB[3], operandAlpha[3];
    float               rgbScale, alphaScale;
    float               lodBias;
} GLS_TexEnv;

/* ===== Evaluators (GL_MAP1_* / GL_MAP2_*) =====
 *
 * GL requires support for order 8; 16 is carried here so ordinary patch data
 * fits without a heap allocation per map.
 */
#define GLS_MAX_EVAL_ORDER  16
#define GLS_NUM_MAP_TARGETS 9       /* VERTEX_3/4, INDEX, COLOR_4, NORMAL, TEXTURE_COORD_1..4 */

typedef struct {
    BOOL                defined;
    BOOL                enabled;
    float               u1, u2;
    int                 order;
    int                 components;
    float               points[GLS_MAX_EVAL_ORDER * 4];
} GLS_Map1;

typedef struct {
    BOOL                defined;
    BOOL                enabled;
    float               u1, u2, v1, v2;
    int                 uorder, vorder;
    int                 components;
    float               points[GLS_MAX_EVAL_ORDER * GLS_MAX_EVAL_ORDER * 4];
} GLS_Map2;

/* ===== Selection / feedback ===== */
#define GLS_MAX_NAME_STACK  64

/* ===== Pixel transfer maps ===== */
#define GLS_NUM_PIXEL_MAPS  10
#define GLS_MAX_PIXEL_MAP   256

/* ===== Query ===== */
typedef struct {
    GLuint_t            id;
    GLenum_t            target;
    BOOL                active;
    GLuint_t            result;
    BOOL                resultReady;    /* result holds a value read back from D3D */
    IDirect3DQuery9     *pQuery;        /* NULL when the target has no D3D9 analogue */
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

/* ===== Legacy client-side vertex array (GL 1.1) =====
 *
 * glVertexPointer and friends.  Distinct from the generic attribs on a VAO:
 * these have no VAO, and `pointer` is either a raw CPU address or, if
 * bufferBinding is non-zero, an offset into that buffer object.
 */
typedef struct {
    GLint_t             size;           /* components: 1-4 */
    GLenum_t            type;           /* GL_FLOAT, GL_UNSIGNED_BYTE, ... */
    GLsizei_t           stride;         /* 0 means tightly packed */
    const void          *pointer;
    GLuint_t            bufferBinding;  /* VBO bound when this was set, 0 = client memory */
    BOOL                enabled;        /* glEnableClientState */
} GLS_ClientArray;

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
    /* GL_TEXTURE_3D has its own binding point; GL_TEXTURE_1D shares the 2D one
     * because a 1D texture is stored here as a 2D texture one row high. */
    GLuint_t            boundTexture3D[GLS_MAX_TEX_UNITS];
    GLuint_t            boundTextureBuffer[GLS_MAX_TEX_UNITS];
    GLuint_t            boundArrayBuffer;
    GLuint_t            boundElementBuffer;
    GLuint_t            boundTextureBufferObject;
    GLuint_t            boundPixelPackBuffer;
    GLuint_t            boundPixelUnpackBuffer;
    GLuint_t            boundVAO;
    /* GL_READ_FRAMEBUFFER and GL_DRAW_FRAMEBUFFER have been separate
     * bindings since GL 3.0; glBlitFramebuffer is meaningless without
     * the distinction. */
    GLuint_t            boundReadFBO;
    GLuint_t            boundDrawFBO;
    GLuint_t            boundRBO;
    GLuint_t            boundProgram;
    GLuint_t            boundProgramPipeline;
    GLuint_t            pipelineActiveProgram;
    GLuint_t            pipelineVertexProgram;
    GLuint_t            pipelineFragmentProgram;
    GLuint_t            pipelineComputeProgram;
    GLenum_t            activeTexUnit;  /* GL_TEXTURE0 + n */

    /* ARB assembly programs.  Kept apart from boundProgram: the two ARB
     * targets bind and enable independently of each other and of
     * glUseProgram, and GLS_Shader / GLS_Program are separate ID spaces whose
     * numbers can collide. */
    GLuint_t            boundVertexProgramARB;
    GLuint_t            boundFragmentProgramARB;
    BOOL                enableVertexProgramARB;
    BOOL                enableFragmentProgramARB;

    /* Which GL texture unit each D3D9 sampler stage should read.
     * Set by glUniform1i on a sampler uniform; identity until then. */
    int                 samplerStageUnit[GLS_MAX_TEX_UNITS];

    /* Legacy client-side vertex arrays (GL 1.1) */
    GLS_ClientArray     clientVertexArray;
    GLS_ClientArray     clientNormalArray;
    GLS_ClientArray     clientColorArray;
    GLS_ClientArray     clientTexCoordArray[GLS_MAX_TEX_UNITS];
    GLenum_t            clientActiveTexUnit; /* GL_TEXTURE0 + n, for glTexCoordPointer */

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
    float               fogCoord;       /* glFogCoord* value; see _glsFogCoordf */

    /* ---- Fixed-function texture environment ---- */
    GLS_TexEnv          texEnv[GLS_MAX_TEX_UNITS];

    /* ---- Raster position (glRasterPos / glWindowPos / glBitmap) ---- */
    float               rasterPos[4];       /* window coords x,y,z,w */
    float               rasterColor[4];
    float               rasterTexCoord[4];
    float               rasterIndex;
    GLboolean_t         rasterPosValid;
    float               pixelZoomX, pixelZoomY;

    /* ---- Evaluators ---- */
    GLS_Map1            map1[GLS_NUM_MAP_TARGETS];
    GLS_Map2            map2[GLS_NUM_MAP_TARGETS];
    int                 mapGrid1n;
    float               mapGrid1u1, mapGrid1u2;
    int                 mapGrid2un, mapGrid2vn;
    float               mapGrid2u1, mapGrid2u2, mapGrid2v1, mapGrid2v2;
    GLboolean_t         autoNormal;

    /* ---- Selection and feedback ---- */
    GLenum_t            renderMode;
    unsigned int        *selectBuffer;
    GLsizei_t           selectBufferSize;
    GLsizei_t           selectIndex;        /* write cursor into selectBuffer */
    unsigned int        nameStack[GLS_MAX_NAME_STACK];
    int                 nameStackDepth;
    GLboolean_t         selectHitPending;
    unsigned int        selectHitMinZ, selectHitMaxZ;
    GLsizei_t           selectHitRecord;    /* index of the open hit's name count */
    int                 selectHits;
    float               *feedbackBuffer;
    GLsizei_t           feedbackBufferSize;
    GLsizei_t           feedbackIndex;
    GLenum_t            feedbackType;
    GLboolean_t         feedbackOverflow;

    /* ---- Accumulation buffer ---- */
    float               accumClear[4];
    float               *accumBuffer;
    int                 accumWidth, accumHeight;

    /* ---- Pixel transfer ---- */
    float               pixelMap[GLS_NUM_PIXEL_MAPS][GLS_MAX_PIXEL_MAP];
    int                 pixelMapSize[GLS_NUM_PIXEL_MAPS];
    float               redScale, greenScale, blueScale, alphaScale, depthScale;
    float               redBias, greenBias, blueBias, alphaBias, depthBias;
    float               indexShift, indexOffset;
    GLboolean_t         mapColorFlag, mapStencilFlag;

    /* ---- Stipple ---- */
    unsigned char       polygonStipple[128];
    int                 lineStippleFactor;
    unsigned short      lineStipplePattern;

    /* ---- Colour index mode ---- */
    float               currentIndex;
    float               clearIndexValue;
    unsigned int        indexWriteMask;

    /* ---- Edge flags ---- */
    GLboolean_t         edgeFlag;

    /* ---- Secondary (specular) colour ---- */
    float               secondaryColor[3];
    GLboolean_t         secondaryColorUsed;

    /* ---- Multisample coverage ---- */
    float               sampleCoverageValue;
    GLboolean_t         sampleCoverageInvert;
    float               minSampleShading;

    /* ---- Conditional render ---- */
    GLboolean_t         conditionalRenderSkip;
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
    /* glActiveStencilFaceEXT: which face the single-face glStencilFunc /
     * glStencilOp write to.  GL_FRONT unless a program says otherwise. */
    GLenum_t            activeStencilFace;

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
    GLsizeiptr_t        transformFeedbackWriteOffset[GLS_MAX_BUFFER_BINDINGS];

    /* Buffer binding points (UBO, TFB, etc.) */
    GLuint_t            boundUniformBuffer;
    GLuint_t            boundCopyReadBuffer;
    GLuint_t            boundCopyWriteBuffer;
    GLuint_t            boundTransformFeedbackBuffer;
    GLuint_t            boundShaderStorageBuffer;
    GLuint_t            boundAtomicCounterBuffer;
    GLuint_t            boundDrawIndirectBuffer;
    GLuint_t            boundDispatchIndirectBuffer;
    GLuint_t            boundParameterBuffer;
    GLS_IndexedBufferBinding uniformBindings[GLS_MAX_BUFFER_BINDINGS];
    GLS_IndexedBufferBinding transformFeedbackBindings[GLS_MAX_BUFFER_BINDINGS];
    GLS_IndexedBufferBinding shaderStorageBindings[GLS_MAX_BUFFER_BINDINGS];
    GLS_IndexedBufferBinding atomicCounterBindings[GLS_MAX_BUFFER_BINDINGS];
    GLS_ImageBinding    imageBindings[GLS_MAX_IMAGE_UNITS];

    /* Primitive restart */
    GLuint_t            primitiveRestartIndex;
    BOOL                enablePrimitiveRestart;

    /* Tessellation state is consumed by the software patch-expansion layer. */
    GLint_t             patchVertices;
    float               patchDefaultOuter[4];
    float               patchDefaultInner[2];

    /* Base-instance is exposed to the shader-lowering layer while an
     * instanced draw is expanded into ordinary D3D9 draws. */
    GLuint_t            currentBaseInstance;

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
    BOOL                enableRasterizerDiscard;
    BOOL                enableStencilTestTwoSide; /* GL_STENCIL_TEST_TWO_SIDE_EXT */
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

    /* TRUE once any glLight* call has run.  Programs that never configure GL
     * lights (e.g. OpenMW, which does all lighting in GLSL) fall back to
     * synthesizing D3D lights from the program's light uniforms instead. */
    BOOL                lightsEverConfigured;

    /* Texture coordinate generation, per unit, per coordinate (S,T,R,Q) */
    GLenum_t            texGenMode[GLS_MAX_TEX_UNITS][4];
    BOOL                texGenEnabled[GLS_MAX_TEX_UNITS][4];
    float               texGenObjectPlane[GLS_MAX_TEX_UNITS][4][4];
    float               texGenEyePlane[GLS_MAX_TEX_UNITS][4][4];

    /* Compiled vertex arrays (GL_EXT_compiled_vertex_array) */
    GLint_t             lockedArrayFirst;
    GLsizei_t           lockedArrayCount;
    BOOL                arraysLocked;

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
