/*********************************************************************************
*
*  ===============================================================================
*  |                  GLDirect: Direct3D Device Driver for Mesa.                 |
*  |                                                                             |
*  |                Copyright (C) 1997-2007 SciTech Software, Inc.               |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x/2000/XP (Win32)
*
* Description:  Mesa compatibility shim — stub implementations.
*
*               These stubs provide just enough functionality for the DX9
*               backend to compile and link. They are NOT full Mesa
*               implementations. The DX9 backend will be replaced by GL46
*               modules, at which point this file can be removed.
*
*********************************************************************************/

#include "mesa_compat.h"
#include "gld_log.h"
#include "gld_context.h"

//---------------------------------------------------------------------------
// GET_CURRENT_CONTEXT helper
//---------------------------------------------------------------------------

/*
 * The DX9 backend uses GET_CURRENT_CONTEXT(ctx) to obtain a GLcontext*.
 * We maintain a thread-local pointer that is set when gldMakeCurrent is
 * called. For now, we use a simple static — the DX9 backend is single-
 * threaded in practice.
 */
static GLcontext *_mesa_compat_current_ctx = NULL;

void _mesa_compat_set_current_context(GLcontext *ctx)
{
    _mesa_compat_current_ctx = ctx;
}

GLcontext* _mesa_compat_get_current_context(void)
{
    return _mesa_compat_current_ctx;
}

//---------------------------------------------------------------------------
// Texture format globals
//---------------------------------------------------------------------------

/* Minimal texture format descriptors. The DX9 backend references these
 * by name when choosing a Mesa-compatible format for D3D textures.
 * The fetch function pointers are NULL — the DX9 backend defines its
 * own fetch functions for the formats it actually uses. */

static void _noop_fetch(const struct gl_texture_image *img,
                         GLint i, GLint j, GLint k, GLvoid *texel)
{
    /* no-op */
    (void)img; (void)i; (void)j; (void)k; (void)texel;
}

const struct gl_texture_format _mesa_texformat_argb8888 = {
    MESA_FORMAT_ARGB8888, GL_RGBA,
    8, 8, 8, 8, 0, 0, 0, 0, 4,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_rgb888 = {
    MESA_FORMAT_RGB888, GL_RGB,
    8, 8, 8, 0, 0, 0, 0, 0, 3,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_rgb565 = {
    MESA_FORMAT_RGB565, GL_RGB,
    5, 6, 5, 0, 0, 0, 0, 0, 2,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_argb4444 = {
    MESA_FORMAT_ARGB4444, GL_RGBA,
    4, 4, 4, 4, 0, 0, 0, 0, 2,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_argb1555 = {
    MESA_FORMAT_ARGB1555, GL_RGBA,
    5, 5, 5, 1, 0, 0, 0, 0, 2,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_al88 = {
    MESA_FORMAT_AL88, GL_LUMINANCE_ALPHA,
    0, 0, 0, 8, 8, 0, 0, 0, 2,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_a8 = {
    MESA_FORMAT_A8, GL_ALPHA,
    0, 0, 0, 8, 0, 0, 0, 0, 1,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_l8 = {
    MESA_FORMAT_L8, GL_LUMINANCE,
    0, 0, 0, 0, 8, 0, 0, 0, 1,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_i8 = {
    MESA_FORMAT_I8, GL_INTENSITY,
    0, 0, 0, 0, 0, 8, 0, 0, 1,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_ci8 = {
    MESA_FORMAT_CI8, GL_COLOR_INDEX,
    0, 0, 0, 0, 0, 0, 8, 0, 1,
    _noop_fetch, _noop_fetch, _noop_fetch
};

const struct gl_texture_format _mesa_texformat_rgb332 = {
    MESA_FORMAT_RGB332, GL_RGB,
    3, 3, 2, 0, 0, 0, 0, 0, 1,
    _noop_fetch, _noop_fetch, _noop_fetch
};

//---------------------------------------------------------------------------
// _mesa_error — log GL errors via gldLogPrintf
//---------------------------------------------------------------------------

void _mesa_error(GLcontext *ctx, GLenum error, const char *msg, ...)
{
    char buf[1024];
    va_list args;

    (void)ctx;

    va_start(args, msg);
    _vsnprintf(buf, sizeof(buf) - 1, msg, args);
    buf[sizeof(buf) - 1] = '\0';
    va_end(args);

    gldLogPrintf(GLDLOG_ERROR, "Mesa error 0x%04X: %s", (unsigned)error, buf);
}

//---------------------------------------------------------------------------
// _mesa_printf — log via gldLogPrintf
//---------------------------------------------------------------------------

void _mesa_printf(const char *msg, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, msg);
    _vsnprintf(buf, sizeof(buf) - 1, msg, args);
    buf[sizeof(buf) - 1] = '\0';
    va_end(args);

    gldLogPrintf(GLDLOG_INFO, "%s", buf);
}

//---------------------------------------------------------------------------
// _mesa_lookup_enum_by_nr — return a static string for an enum value
//---------------------------------------------------------------------------

const char* _mesa_lookup_enum_by_nr(int nr)
{
    static char buf[32];
    sprintf(buf, "GL_ENUM_0x%04X", (unsigned)nr);
    return buf;
}

//---------------------------------------------------------------------------
// Extension stubs — no-ops since we're not using Mesa's extension system
//---------------------------------------------------------------------------

void _mesa_enable_extension(GLcontext *ctx, const char *name)
{
    (void)ctx;
    gldLogPrintf(GLDLOG_INFO, "mesa_compat: enable_extension(%s) [stub]", name ? name : "(null)");
}

void _mesa_disable_extension(GLcontext *ctx, const char *name)
{
    (void)ctx;
    gldLogPrintf(GLDLOG_INFO, "mesa_compat: disable_extension(%s) [stub]", name ? name : "(null)");
}

void _mesa_enable_1_3_extensions(GLcontext *ctx)
{
    (void)ctx;
    gldLogPrintf(GLDLOG_INFO, "mesa_compat: enable_1_3_extensions [stub]");
}

void _mesa_enable_imaging_extensions(GLcontext *ctx)
{
    (void)ctx;
    gldLogPrintf(GLDLOG_INFO, "mesa_compat: enable_imaging_extensions [stub]");
}

//---------------------------------------------------------------------------
// _mesa_update_state — no-op; state is managed by the DX9 backend
//---------------------------------------------------------------------------

void _mesa_update_state(GLcontext *ctx)
{
    if (ctx && ctx->NewState && ctx->Driver.UpdateState) {
        GLuint new_state = ctx->NewState;
        ctx->NewState = 0;
        ctx->Driver.UpdateState(ctx, new_state);
    }
}

//---------------------------------------------------------------------------
// _mesa_noop_vtxfmt_init — fill vertex format with no-op stubs
//---------------------------------------------------------------------------

static void GLAPIENTRY _noop_Begin(GLenum m) { (void)m; }
static void GLAPIENTRY _noop_End(void) {}
static void GLAPIENTRY _noop_Vertex2f(GLfloat x, GLfloat y) { (void)x; (void)y; }
static void GLAPIENTRY _noop_Vertex2fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_Vertex3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
static void GLAPIENTRY _noop_Vertex3fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_Vertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)x; (void)y; (void)z; (void)w; }
static void GLAPIENTRY _noop_Vertex4fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_Color3f(GLfloat r, GLfloat g, GLfloat b) { (void)r; (void)g; (void)b; }
static void GLAPIENTRY _noop_Color3fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
static void GLAPIENTRY _noop_Color4fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_Normal3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
static void GLAPIENTRY _noop_Normal3fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_TexCoord2f(GLfloat s, GLfloat t) { (void)s; (void)t; }
static void GLAPIENTRY _noop_TexCoord2fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_TexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) { (void)s; (void)t; (void)r; (void)q; }
static void GLAPIENTRY _noop_TexCoord4fv(const GLfloat *v) { (void)v; }
static void GLAPIENTRY _noop_MultiTexCoord2fARB(GLenum e, GLfloat s, GLfloat t) { (void)e; (void)s; (void)t; }
static void GLAPIENTRY _noop_EvalCoord1f(GLfloat u) { (void)u; }
static void GLAPIENTRY _noop_EvalCoord1fv(const GLfloat *u) { (void)u; }
static void GLAPIENTRY _noop_EvalCoord2f(GLfloat u, GLfloat v) { (void)u; (void)v; }
static void GLAPIENTRY _noop_EvalCoord2fv(const GLfloat *u) { (void)u; }
static void GLAPIENTRY _noop_EvalPoint1(GLint i) { (void)i; }
static void GLAPIENTRY _noop_EvalPoint2(GLint i, GLint j) { (void)i; (void)j; }
static void GLAPIENTRY _noop_Materialfv(GLenum f, GLenum p, const GLfloat *v) { (void)f; (void)p; (void)v; }
static void GLAPIENTRY _noop_Rectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
static void GLAPIENTRY _noop_DrawArrays(GLenum m, GLint f, GLsizei c) { (void)m; (void)f; (void)c; }
static void GLAPIENTRY _noop_DrawElements(GLenum m, GLsizei c, GLenum t, const GLvoid *i) { (void)m; (void)c; (void)t; (void)i; }
static void GLAPIENTRY _noop_DrawRangeElements(GLenum m, GLuint s, GLuint e, GLsizei c, GLenum t, const GLvoid *i) { (void)m; (void)s; (void)e; (void)c; (void)t; (void)i; }
static void GLAPIENTRY _noop_VertexAttrib1fNV(GLuint i, GLfloat x) { (void)i; (void)x; }
static void GLAPIENTRY _noop_VertexAttrib1fvNV(GLuint i, const GLfloat *v) { (void)i; (void)v; }
static void GLAPIENTRY _noop_VertexAttrib2fNV(GLuint i, GLfloat x, GLfloat y) { (void)i; (void)x; (void)y; }
static void GLAPIENTRY _noop_VertexAttrib2fvNV(GLuint i, const GLfloat *v) { (void)i; (void)v; }
static void GLAPIENTRY _noop_VertexAttrib3fNV(GLuint i, GLfloat x, GLfloat y, GLfloat z) { (void)i; (void)x; (void)y; (void)z; }
static void GLAPIENTRY _noop_VertexAttrib3fvNV(GLuint i, const GLfloat *v) { (void)i; (void)v; }
static void GLAPIENTRY _noop_VertexAttrib4fNV(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { (void)i; (void)x; (void)y; (void)z; (void)w; }
static void GLAPIENTRY _noop_VertexAttrib4fvNV(GLuint i, const GLfloat *v) { (void)i; (void)v; }
static void GLAPIENTRY _noop_ArrayElement(GLint i) { (void)i; }

void _mesa_noop_vtxfmt_init(GLvertexformat *vfmt)
{
    if (!vfmt) return;
    memset(vfmt, 0, sizeof(*vfmt));
    vfmt->Begin = _noop_Begin;
    vfmt->End = _noop_End;
    vfmt->Vertex2f = _noop_Vertex2f;
    vfmt->Vertex2fv = _noop_Vertex2fv;
    vfmt->Vertex3f = _noop_Vertex3f;
    vfmt->Vertex3fv = _noop_Vertex3fv;
    vfmt->Vertex4f = _noop_Vertex4f;
    vfmt->Vertex4fv = _noop_Vertex4fv;
    vfmt->Color3f = _noop_Color3f;
    vfmt->Color3fv = _noop_Color3fv;
    vfmt->Color4f = _noop_Color4f;
    vfmt->Color4fv = _noop_Color4fv;
    vfmt->Normal3f = _noop_Normal3f;
    vfmt->Normal3fv = _noop_Normal3fv;
    vfmt->TexCoord2f = _noop_TexCoord2f;
    vfmt->TexCoord2fv = _noop_TexCoord2fv;
    vfmt->TexCoord4f = _noop_TexCoord4f;
    vfmt->TexCoord4fv = _noop_TexCoord4fv;
    vfmt->MultiTexCoord2fARB = _noop_MultiTexCoord2fARB;
    vfmt->EvalCoord1f = _noop_EvalCoord1f;
    vfmt->EvalCoord1fv = _noop_EvalCoord1fv;
    vfmt->EvalCoord2f = _noop_EvalCoord2f;
    vfmt->EvalCoord2fv = _noop_EvalCoord2fv;
    vfmt->EvalPoint1 = _noop_EvalPoint1;
    vfmt->EvalPoint2 = _noop_EvalPoint2;
    vfmt->Materialfv = _noop_Materialfv;
    vfmt->Rectf = _noop_Rectf;
    vfmt->DrawArrays = _noop_DrawArrays;
    vfmt->DrawElements = _noop_DrawElements;
    vfmt->DrawRangeElements = _noop_DrawRangeElements;
    vfmt->VertexAttrib1fNV = _noop_VertexAttrib1fNV;
    vfmt->VertexAttrib1fvNV = _noop_VertexAttrib1fvNV;
    vfmt->VertexAttrib2fNV = _noop_VertexAttrib2fNV;
    vfmt->VertexAttrib2fvNV = _noop_VertexAttrib2fvNV;
    vfmt->VertexAttrib3fNV = _noop_VertexAttrib3fNV;
    vfmt->VertexAttrib3fvNV = _noop_VertexAttrib3fvNV;
    vfmt->VertexAttrib4fNV = _noop_VertexAttrib4fNV;
    vfmt->VertexAttrib4fvNV = _noop_VertexAttrib4fvNV;
    vfmt->ArrayElement = _noop_ArrayElement;
}

//---------------------------------------------------------------------------
// _mesa_alloc_instruction — allocate a display list instruction node
//---------------------------------------------------------------------------

void* _mesa_alloc_instruction(GLcontext *ctx, int opcode, int size)
{
    void *p;
    (void)ctx;
    (void)opcode;
    p = malloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

//---------------------------------------------------------------------------
// mesa_print_display_list — debug stub
//---------------------------------------------------------------------------

void mesa_print_display_list(GLuint list)
{
    gldLogPrintf(GLDLOG_INFO, "mesa_compat: print_display_list(%u) [stub]", list);
}

//---------------------------------------------------------------------------
// _mesa_validate_DrawElements — always returns TRUE (stub)
//---------------------------------------------------------------------------

GLboolean _mesa_validate_DrawElements(GLcontext *ctx, GLenum mode,
                                       GLsizei count, GLenum type,
                                       const GLvoid *indices)
{
    (void)ctx; (void)mode; (void)count; (void)type; (void)indices;
    return GL_TRUE;
}

//---------------------------------------------------------------------------
// _mesa_make_current — stub
//---------------------------------------------------------------------------

void _mesa_make_current(GLcontext *ctx, GLframebuffer *drawBuffer,
                         GLframebuffer *readBuffer)
{
    (void)ctx; (void)drawBuffer; (void)readBuffer;
}

//---------------------------------------------------------------------------
// _mesa_transfer_teximage — stub (varargs, does nothing)
//---------------------------------------------------------------------------

void _mesa_transfer_teximage(GLcontext *ctx, ...)
{
    (void)ctx;
}

//---------------------------------------------------------------------------
// _mesa_choose_tex_format — stub, returns argb8888
//---------------------------------------------------------------------------

const struct gl_texture_format* _mesa_choose_tex_format(GLcontext *ctx,
    GLenum format, GLenum srcFormat, GLenum srcType)
{
    (void)ctx; (void)format; (void)srcFormat; (void)srcType;
    return &_mesa_texformat_argb8888;
}

//---------------------------------------------------------------------------
// _mesa_image_row_stride — compute row stride for pixel packing
//---------------------------------------------------------------------------

GLint _mesa_image_row_stride(const struct gl_pixelstore_attrib *packing,
    GLsizei width, GLenum format, GLenum type)
{
    GLint bytesPerPixel = 4; /* default RGBA */
    GLint stride;
    GLint alignment;

    (void)format; (void)type;

    /* Rough estimate — 4 bytes per pixel for most formats */
    switch (type) {
    case GL_UNSIGNED_BYTE:
        switch (format) {
        case GL_RGB: bytesPerPixel = 3; break;
        case GL_LUMINANCE: case GL_ALPHA: bytesPerPixel = 1; break;
        case GL_LUMINANCE_ALPHA: bytesPerPixel = 2; break;
        default: bytesPerPixel = 4; break;
        }
        break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        bytesPerPixel = 2;
        break;
    default:
        bytesPerPixel = 4;
        break;
    }

    stride = width * bytesPerPixel;
    alignment = packing ? packing->Alignment : 4;
    if (alignment > 1) {
        stride = (stride + alignment - 1) & ~(alignment - 1);
    }
    return stride;
}

//---------------------------------------------------------------------------
// _mesa_image_address — compute address of a pixel in a packed image
//---------------------------------------------------------------------------

GLvoid* _mesa_image_address(const struct gl_pixelstore_attrib *packing,
    const GLvoid *image, GLsizei width, GLsizei height,
    GLenum format, GLenum type, GLint img, GLint row, GLint column)
{
    GLint stride = _mesa_image_row_stride(packing, width, format, type);
    GLint bytesPerPixel = 4;
    const GLubyte *addr;

    (void)height; (void)img;

    switch (type) {
    case GL_UNSIGNED_BYTE:
        switch (format) {
        case GL_RGB: bytesPerPixel = 3; break;
        case GL_LUMINANCE: case GL_ALPHA: bytesPerPixel = 1; break;
        case GL_LUMINANCE_ALPHA: bytesPerPixel = 2; break;
        case GL_COLOR_INDEX: bytesPerPixel = 1; break;
        case GL_BITMAP: bytesPerPixel = 1; break;
        default: bytesPerPixel = 4; break;
        }
        break;
    default:
        bytesPerPixel = 4;
        break;
    }

    addr = (const GLubyte *)image;
    addr += row * stride;

    if (format == GL_BITMAP || type == GL_BITMAP) {
        addr += column / 8;
    } else {
        addr += column * bytesPerPixel;
    }

    return (GLvoid *)addr;
}

//---------------------------------------------------------------------------
// _mesa_unpack_bitmap — unpack a bitmap into a byte array
//---------------------------------------------------------------------------

GLubyte* _mesa_unpack_bitmap(GLsizei width, GLsizei height,
    const GLubyte *bitmap, const struct gl_pixelstore_attrib *packing)
{
    /* Simple implementation: copy the bitmap data as-is.
     * A full implementation would handle row alignment and byte swapping. */
    GLint bytesPerRow = (width + 7) / 8;
    GLint alignment = packing ? packing->Alignment : 4;
    GLint srcStride = (bytesPerRow + alignment - 1) & ~(alignment - 1);
    GLint dstStride = bytesPerRow;
    GLint totalBytes = dstStride * height;
    GLubyte *result;
    GLint i;

    (void)packing;

    if (!bitmap) return NULL;

    result = (GLubyte *)malloc(totalBytes);
    if (!result) return NULL;

    for (i = 0; i < height; i++) {
        memcpy(result + i * dstStride, bitmap + i * srcStride, bytesPerRow);
    }

    return result;
}

//---------------------------------------------------------------------------
// _mesa_problem — log an internal Mesa problem
//---------------------------------------------------------------------------

void _mesa_problem(GLcontext *ctx, const char *msg, ...)
{
    char buf[1024];
    va_list args;

    (void)ctx;

    va_start(args, msg);
    _vsnprintf(buf, sizeof(buf) - 1, msg, args);
    buf[sizeof(buf) - 1] = '\0';
    va_end(args);

    gldLogPrintf(GLDLOG_ERROR, "Mesa problem: %s", buf);
}

//---------------------------------------------------------------------------
// _mesa_native_packing — default pixel store attributes
//---------------------------------------------------------------------------

const struct gl_pixelstore_attrib _mesa_native_packing = {
    4,      /* Alignment */
    0,      /* RowLength */
    0,      /* SkipPixels */
    0,      /* SkipRows */
    0,      /* SwapBytes */
    0       /* LsbFirst */
};

//---------------------------------------------------------------------------
// _mesa_store_teximage3d — stub
//---------------------------------------------------------------------------

void _mesa_store_teximage3d(GLcontext *ctx, GLenum target, GLint level,
    GLint internalFormat, GLint width, GLint height, GLint depth,
    GLint border, GLenum format, GLenum type, const GLvoid *pixels,
    const struct gl_pixelstore_attrib *packing,
    struct gl_texture_object *texObj, struct gl_texture_image *texImage)
{
    (void)ctx; (void)target; (void)level; (void)internalFormat;
    (void)width; (void)height; (void)depth; (void)border;
    (void)format; (void)type; (void)pixels; (void)packing;
    (void)texObj; (void)texImage;
}

//---------------------------------------------------------------------------
// _mesa_store_texsubimage3d — stub
//---------------------------------------------------------------------------

void _mesa_store_texsubimage3d(GLcontext *ctx, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth,
    GLenum format, GLenum type, const GLvoid *pixels,
    const struct gl_pixelstore_attrib *packing,
    struct gl_texture_object *texObj, struct gl_texture_image *texImage)
{
    (void)ctx; (void)target; (void)level;
    (void)xoffset; (void)yoffset; (void)zoffset;
    (void)width; (void)height; (void)depth;
    (void)format; (void)type; (void)pixels; (void)packing;
    (void)texObj; (void)texImage;
}

//---------------------------------------------------------------------------
// _mesa_test_proxy_teximage — always returns TRUE (stub)
//---------------------------------------------------------------------------

GLboolean _mesa_test_proxy_teximage(GLcontext *ctx, GLenum target,
    GLint level, GLint internalFormat, GLenum format, GLenum type,
    GLint width, GLint height, GLint depth, GLint border)
{
    (void)ctx; (void)target; (void)level; (void)internalFormat;
    (void)format; (void)type; (void)width; (void)height;
    (void)depth; (void)border;
    return GL_TRUE;
}

//---------------------------------------------------------------------------
// Math evaluator stubs
//---------------------------------------------------------------------------

void _math_horner_bezier_curve(const GLfloat *cp, GLfloat *out,
                                GLfloat t, GLuint dim, GLint order)
{
    /* Minimal Horner evaluation of a 1D Bezier curve.
     * This is a real implementation needed for evaluators to work. */
    GLuint i, j;
    GLfloat s = 1.0f - t;

    if (order <= 0 || !cp || !out) return;

    /* Copy the last control point as starting value */
    for (j = 0; j < dim; j++)
        out[j] = cp[(order - 1) * dim + j];

    /* Horner's method: iterate from second-to-last down to first */
    for (i = order - 2; (int)i >= 0; i--) {
        for (j = 0; j < dim; j++) {
            out[j] = out[j] * t + cp[i * dim + j] * s;
        }
        /* This is a simplified version; for full correctness we'd need
         * the de Casteljau algorithm, but this is sufficient for the
         * DX9 backend's evaluator support. */
    }

    /* Fallback: just use de Casteljau for correctness */
    /* Actually, let's do a proper de Casteljau for 1D curves */
    {
        GLfloat *tmp = (GLfloat *)malloc(order * dim * sizeof(GLfloat));
        if (!tmp) return;
        memcpy(tmp, cp, order * dim * sizeof(GLfloat));
        for (i = 1; (int)i < order; i++) {
            for (j = 0; (int)j < (order - (int)i); j++) {
                GLuint k;
                for (k = 0; k < dim; k++) {
                    tmp[j * dim + k] = s * tmp[j * dim + k] + t * tmp[(j + 1) * dim + k];
                }
            }
        }
        for (j = 0; j < dim; j++)
            out[j] = tmp[j];
        free(tmp);
    }
}

void _math_horner_bezier_surf(const GLfloat *cp, GLfloat *out,
                               GLfloat u, GLfloat v,
                               GLuint dim, GLint uorder, GLint vorder)
{
    /* Evaluate a 2D Bezier surface patch at (u,v).
     * First evaluate along V for each U control point row,
     * then evaluate the resulting curve along U. */
    GLfloat *tmp;
    GLint i;

    if (uorder <= 0 || vorder <= 0 || !cp || !out) return;

    tmp = (GLfloat *)malloc(uorder * dim * sizeof(GLfloat));
    if (!tmp) return;

    /* For each row of U control points, evaluate the V curve */
    for (i = 0; i < uorder; i++) {
        _math_horner_bezier_curve(cp + i * vorder * dim, tmp + i * dim, v, dim, vorder);
    }

    /* Now evaluate the resulting U curve */
    _math_horner_bezier_curve(tmp, out, u, dim, uorder);

    free(tmp);
}

void _math_de_casteljau_surf(const GLfloat *cp, GLfloat *out,
                              GLfloat *du, GLfloat *dv,
                              GLfloat u, GLfloat v,
                              GLuint dim, GLint uorder, GLint vorder)
{
    /* Evaluate surface and partial derivatives using de Casteljau.
     * For the derivatives, we use finite differences with a small epsilon. */
    GLfloat eps = 1.0e-4f;
    GLfloat out_u1[4], out_u2[4], out_v1[4], out_v2[4];
    GLuint j;

    if (!cp || !out) return;

    /* Evaluate at (u, v) */
    _math_horner_bezier_surf(cp, out, u, v, dim, uorder, vorder);

    if (du) {
        /* Partial derivative with respect to u */
        GLfloat u1 = (u - eps < 0.0f) ? 0.0f : u - eps;
        GLfloat u2 = (u + eps > 1.0f) ? 1.0f : u + eps;
        GLfloat inv = 1.0f / (u2 - u1);
        _math_horner_bezier_surf(cp, out_u1, u1, v, dim, uorder, vorder);
        _math_horner_bezier_surf(cp, out_u2, u2, v, dim, uorder, vorder);
        for (j = 0; j < dim; j++)
            du[j] = (out_u2[j] - out_u1[j]) * inv;
    }

    if (dv) {
        /* Partial derivative with respect to v */
        GLfloat v1 = (v - eps < 0.0f) ? 0.0f : v - eps;
        GLfloat v2 = (v + eps > 1.0f) ? 1.0f : v + eps;
        GLfloat inv = 1.0f / (v2 - v1);
        _math_horner_bezier_surf(cp, out_v1, u, v1, dim, uorder, vorder);
        _math_horner_bezier_surf(cp, out_v2, u, v2, dim, uorder, vorder);
        for (j = 0; j < dim; j++)
            dv[j] = (out_v2[j] - out_v1[j]) * inv;
    }
}

/* _ae_invalidate_state removed — defined in gld_arrayelt.c */

//---------------------------------------------------------------------------
// gldSaveFlushVertices — defined in gld_dlist.c, but declared here
// as a fallback stub in case it's not linked.
//---------------------------------------------------------------------------

/* Note: gldSaveFlushVertices is actually implemented in gld_dlist.c.
 * This weak stub is only here as a safety net. The linker will prefer
 * the real implementation from gld_dlist.c. */
#if 0
void gldSaveFlushVertices(GLcontext *ctx)
{
    (void)ctx;
}
#endif


/* _ae_create_context / _ae_destroy_context removed — defined in gld_arrayelt.c */

//---------------------------------------------------------------------------
// _mesa_alloc_opcode — allocate a display list opcode
//---------------------------------------------------------------------------

static int _mesa_compat_next_opcode = 100; /* start above Mesa's built-in opcodes */

int _mesa_alloc_opcode(GLcontext *ctx, int size,
                        mesa_opcode_execute exec,
                        mesa_opcode_destroy destroy,
                        mesa_opcode_print print)
{
    (void)ctx; (void)size; (void)exec; (void)destroy; (void)print;
    return _mesa_compat_next_opcode++;
}

//---------------------------------------------------------------------------
// _mesa_install_exec_vtxfmt / _mesa_install_save_vtxfmt
//---------------------------------------------------------------------------

void _mesa_install_exec_vtxfmt(GLcontext *ctx, GLvertexformat *vfmt)
{
    if (ctx && vfmt)
        ctx->Exec = vfmt;
}

void _mesa_install_save_vtxfmt(GLcontext *ctx, GLvertexformat *vfmt)
{
    if (ctx && vfmt)
        ctx->Save = vfmt;
}

//---------------------------------------------------------------------------
// _mesa_save_vtxfmt_init — fill save vertex format with no-op stubs
//---------------------------------------------------------------------------

void _mesa_save_vtxfmt_init(GLvertexformat *vfmt)
{
    /* Just use the noop init — the DX9 backend overrides what it needs */
    _mesa_noop_vtxfmt_init(vfmt);
}

//---------------------------------------------------------------------------
// Display list call stubs
//---------------------------------------------------------------------------

void GLAPIENTRY _mesa_CallList(GLuint list)
{
    (void)list;
}

void GLAPIENTRY _mesa_CallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
    (void)n; (void)type; (void)lists;
}

void GLAPIENTRY _mesa_save_CallList(GLuint list)
{
    (void)list;
}

void GLAPIENTRY _mesa_save_CallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
    (void)n; (void)type; (void)lists;
}

void GLAPIENTRY _mesa_save_EvalMesh1(GLenum mode, GLint i1, GLint i2)
{
    (void)mode; (void)i1; (void)i2;
}

void GLAPIENTRY _mesa_save_EvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    (void)mode; (void)i1; (void)i2; (void)j1; (void)j2;
}
