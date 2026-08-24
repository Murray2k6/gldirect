/* Runtime ABI smoke test for an arbitrary GLDirect opengl32.dll build. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLubyte;

#define GL_VERSION 0x1F02u
#define GL_COLOR_BUFFER_BIT 0x00004000u
#define GL_COMPUTE_SHADER 0x91B9u
#define GL_VERTEX_SHADER 0x8B31u
#define GL_GEOMETRY_SHADER 0x8DD9u
#define GL_TESS_CONTROL_SHADER 0x8E88u
#define GL_TESS_EVALUATION_SHADER 0x8E87u
#define GL_FRAGMENT_SHADER 0x8B30u
#define GL_COMPILE_STATUS 0x8B81u
#define GL_LINK_STATUS 0x8B82u
#define GL_SHADER_STORAGE_BUFFER 0x90D2u
#define GL_DYNAMIC_COPY 0x88EAu
#define GL_ALL_BARRIER_BITS 0xFFFFFFFFu
#define GL_TEXTURE_2D 0x0DE1u
#define GL_FRAMEBUFFER 0x8D40u
#define GL_COLOR_ATTACHMENT0 0x8CE0u
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5u
#define GL_TEXTURE_BUFFER 0x8C2Au
#define GL_RGBA 0x1908u
#define GL_RGBA8 0x8058u
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3u
#define GL_R32F 0x822Eu
#define GL_UNSIGNED_BYTE 0x1401u
#define GL_UNSIGNED_SHORT 0x1403u
#define GL_FLOAT 0x1406u
#define GL_WRITE_ONLY 0x88B9u
#define GL_POINTS 0x0000u
#define GL_TRIANGLES 0x0004u
#define GL_PATCHES 0x000Eu
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8Eu
#define GL_INTERLEAVED_ATTRIBS 0x8C8Cu
#define GL_RASTERIZER_DISCARD 0x8C89u
#define GL_COMPILE 0x1300u
#define GL_CURRENT_COLOR 0x0B00u
#define GL_MODELVIEW_MATRIX 0x0BA6u
#define GL_PROJECTION_MATRIX 0x0BA7u
#define GL_MODELVIEW 0x1700u
#define GL_PROJECTION 0x1701u
#define GL_LIGHTING 0x0B50u
#define GL_LIGHT0 0x4000u
#define GL_POSITION 0x1203u
#define GL_DIFFUSE 0x1201u
#define GL_AMBIENT_AND_DIFFUSE 0x1602u
#define GL_AMBIENT 0x1200u
#define GL_SPECULAR 0x1202u
#define GL_FRONT 0x0404u
#define GL_VERTEX_ARRAY 0x8074u
#define GL_TEXTURE_COORD_ARRAY 0x8078u
#define GL_NORMAL_ARRAY 0x8075u
#define GL_TEXTURE_MIN_FILTER 0x2801u
#define GL_TEXTURE_MAG_FILTER 0x2800u
#define GL_LINEAR 0x2601u
#define GL_NO_ERROR 0x0000u
#define GL_INVALID_ENUM 0x0500u
#define GL_INVALID_VALUE 0x0501u
#define GL_INVALID_OPERATION 0x0502u
#define GL_TEXTURE_MIN_LOD 0x813Au
#define GL_TEXTURE_MAX_LOD 0x813Bu
#define GL_BGRA 0x80E1u
#define GL_UNSIGNED_SHORT_5_6_5 0x8363u
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033u
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034u
#define GL_UNSIGNED_INT_8_8_8_8 0x8035u
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367u
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FEu
#define GL_RED_SCALE 0x0D14u
#define GL_VIEWPORT 0x0BA2u
#define GL_SCISSOR_BOX 0x0C10u
#define GL_CURRENT_NORMAL 0x0B02u
#define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06u
#define GL_MAP1_VERTEX_3 0x0A17u
#define GL_COEFF 0x0A00u
#define GL_ORDER 0x0A01u
#define GL_POINT 0x1B00u
#define GL_2D 0x0600u
#define GL_TEXTURE_1D 0x0DE0u

typedef int   (WINAPI *PFN_CHOOSE_PIXEL_FORMAT)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL  (WINAPI *PFN_SET_PIXEL_FORMAT)(HDC, int, const PIXELFORMATDESCRIPTOR *);
typedef HGLRC (WINAPI *PFN_CREATE_CONTEXT)(HDC);
typedef BOOL  (WINAPI *PFN_DELETE_CONTEXT)(HGLRC);
typedef BOOL  (WINAPI *PFN_MAKE_CURRENT)(HDC, HGLRC);
typedef PROC  (WINAPI *PFN_GET_PROC_ADDRESS)(LPCSTR);
typedef BOOL  (WINAPI *PFN_SWAP_BUFFERS)(HDC);
typedef HGLRC (WINAPI *PFN_CREATE_CONTEXT_ATTRIBS)(HDC, HGLRC, const int *);
typedef const GLubyte *(WINAPI *PFN_GET_STRING)(GLenum);
typedef void  (WINAPI *PFN_CLEAR_COLOR)(float, float, float, float);
typedef void  (WINAPI *PFN_CLEAR)(GLbitfield);
typedef unsigned int (WINAPI *PFN_CREATE_SHADER)(GLenum);
typedef void  (WINAPI *PFN_SHADER_SOURCE)(unsigned int, int,
                                          const char *const *, const int *);
typedef void  (WINAPI *PFN_COMPILE_SHADER)(unsigned int);
typedef void  (WINAPI *PFN_GET_SHADER_IV)(unsigned int, GLenum, int *);
typedef void  (WINAPI *PFN_DELETE_SHADER)(unsigned int);
typedef unsigned int (WINAPI *PFN_CREATE_PROGRAM)(void);
typedef void  (WINAPI *PFN_ATTACH_SHADER)(unsigned int, unsigned int);
typedef void  (WINAPI *PFN_LINK_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_GET_PROGRAM_IV)(unsigned int, GLenum, int *);
typedef void  (WINAPI *PFN_GET_PROGRAM_INFO_LOG)(unsigned int, int, int *, char *);
typedef void  (WINAPI *PFN_USE_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_DELETE_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_GEN_BUFFERS)(int, unsigned int *);
typedef void  (WINAPI *PFN_BIND_BUFFER)(GLenum, unsigned int);
typedef void  (WINAPI *PFN_BUFFER_DATA)(GLenum, size_t, const void *, GLenum);
typedef void  (WINAPI *PFN_BIND_BUFFER_BASE)(GLenum, unsigned int, unsigned int);
typedef void  (WINAPI *PFN_GET_BUFFER_SUB_DATA)(GLenum, size_t, size_t, void *);
typedef void  (WINAPI *PFN_DELETE_BUFFERS)(int, const unsigned int *);
typedef void  (WINAPI *PFN_DISPATCH_COMPUTE)(unsigned int, unsigned int, unsigned int);
typedef void  (WINAPI *PFN_MEMORY_BARRIER)(GLbitfield);
typedef void  (WINAPI *PFN_GEN_TEXTURES)(int, unsigned int *);
typedef void  (WINAPI *PFN_BIND_TEXTURE)(GLenum, unsigned int);
typedef void  (WINAPI *PFN_TEX_IMAGE_2D)(GLenum, int, int, int, int, int, GLenum, GLenum, const void *);
typedef void  (WINAPI *PFN_TEX_BUFFER_RANGE)(GLenum, GLenum, unsigned int, size_t, size_t);
typedef void  (WINAPI *PFN_BIND_IMAGE_TEXTURE)(unsigned int, unsigned int, int, unsigned char, int, GLenum, GLenum);
typedef void  (WINAPI *PFN_GET_TEX_IMAGE)(GLenum, int, GLenum, GLenum, void *);
typedef void  (WINAPI *PFN_DELETE_TEXTURES)(int, const unsigned int *);
typedef void  (WINAPI *PFN_GEN_FRAMEBUFFERS)(int, unsigned int *);
typedef void  (WINAPI *PFN_BIND_FRAMEBUFFER)(GLenum, unsigned int);
typedef void  (WINAPI *PFN_FRAMEBUFFER_TEXTURE_2D)(GLenum, GLenum, GLenum,
                                                   unsigned int, int);
typedef GLenum (WINAPI *PFN_CHECK_FRAMEBUFFER_STATUS)(GLenum);
typedef void  (WINAPI *PFN_DELETE_FRAMEBUFFERS)(int, const unsigned int *);
typedef unsigned int (WINAPI *PFN_GEN_LISTS)(int);
typedef void  (WINAPI *PFN_NEW_LIST)(unsigned int, GLenum);
typedef void  (WINAPI *PFN_END_LIST)(void);
typedef void  (WINAPI *PFN_CALL_LIST)(unsigned int);
typedef void  (WINAPI *PFN_DELETE_LISTS)(unsigned int, int);
typedef void  (WINAPI *PFN_COLOR_4F)(float, float, float, float);
typedef void  (WINAPI *PFN_GET_FLOATV)(GLenum, float *);
typedef void  (WINAPI *PFN_DRAW_ARRAYS)(GLenum, int, int);
typedef void  (WINAPI *PFN_VIEWPORT)(int, int, int, int);
typedef void  (WINAPI *PFN_READ_PIXELS)(int, int, int, int, GLenum, GLenum, void *);
typedef GLenum (WINAPI *PFN_GET_ERROR)(void);
typedef void  (WINAPI *PFN_DRAW_ARRAYS_INSTANCED)(GLenum, int, int, int);
typedef void  (WINAPI *PFN_VERTEX_ATTRIB_POINTER)(unsigned int, int, GLenum,
                                                   unsigned char, int,
                                                   const void *);
typedef void  (WINAPI *PFN_VERTEX_ATTRIB_ARRAY)(unsigned int);
typedef void  (WINAPI *PFN_TRANSFORM_FEEDBACK_VARYINGS)(unsigned int, int,
                                                         const char *const *, GLenum);
typedef void  (WINAPI *PFN_BEGIN_TRANSFORM_FEEDBACK)(GLenum);
typedef void  (WINAPI *PFN_END_TRANSFORM_FEEDBACK)(void);
typedef void  (WINAPI *PFN_ENABLE_DISABLE)(GLenum);
typedef const char *(WINAPI *PFN_GET_EXTENSIONS_STRING_ARB)(HDC);
typedef BOOL  (WINAPI *PFN_CHOOSE_PIXEL_FORMAT_ARB)(HDC, const int *,
                                                    const FLOAT *, UINT,
                                                    int *, UINT *);
typedef BOOL  (WINAPI *PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB)(HDC, int, int, UINT,
                                                           const int *, int *);
typedef BOOL  (WINAPI *PFN_SWAP_INTERVAL_EXT)(int);
typedef int   (WINAPI *PFN_GET_SWAP_INTERVAL_EXT)(void);
typedef void  (WINAPI *PFN_LIGHTFV)(GLenum, GLenum, const float *);
typedef void  (WINAPI *PFN_MATERIALFV)(GLenum, GLenum, const float *);
typedef void  (WINAPI *PFN_CLIENT_ARRAY)(GLenum);
typedef void  (WINAPI *PFN_VERTEX_POINTER)(int, GLenum, int, const void *);
typedef void  (WINAPI *PFN_TEXCOORD_POINTER)(int, GLenum, int, const void *);
typedef void  (WINAPI *PFN_NORMAL_POINTER)(GLenum, int, const void *);
typedef int   (WINAPI *PFN_GET_UNIFORM_LOCATION)(unsigned int, const char *);
typedef void  (WINAPI *PFN_UNIFORM_1I)(int, int);
typedef void  (WINAPI *PFN_UNIFORM_1F)(int, float);
typedef void  (WINAPI *PFN_UNIFORM_3F)(int, float, float, float);
typedef void  (WINAPI *PFN_UNIFORM_4F)(int, float, float, float, float);
typedef void  (WINAPI *PFN_UNIFORM_4FV)(int, int, const float *);
typedef void  (WINAPI *PFN_UNIFORM_MATRIX_4FV)(int, int, unsigned char,
                                               const float *);
typedef void  (WINAPI *PFN_DRAW_ELEMENTS_BASE_VERTEX)(GLenum, int, GLenum,
                                                       const void *, int);
typedef void  (WINAPI *PFN_TEX_PARAMETER_I)(GLenum, GLenum, int);
typedef void  (WINAPI *PFN_TEX_PARAMETER_F)(GLenum, GLenum, float);
typedef void  (WINAPI *PFN_GET_TEX_PARAMETER_FV)(GLenum, GLenum, float *);
typedef void  (WINAPI *PFN_GET_DOUBLEV)(GLenum, double *);
typedef void  (WINAPI *PFN_PIXEL_TRANSFER_F)(GLenum, float);
typedef void  (WINAPI *PFN_COLOR_3B)(char, char, char);
typedef void  (WINAPI *PFN_NORMAL_3B)(char, char, char);
typedef void  (WINAPI *PFN_SELECT_BUFFER)(int, unsigned int *);
typedef void  (WINAPI *PFN_FEEDBACK_BUFFER)(int, unsigned int, float *);
typedef void  (WINAPI *PFN_RASTER_POS_2F)(float, float);
typedef void  (WINAPI *PFN_TEX_COORD_4F)(float, float, float, float);
typedef void  (WINAPI *PFN_MAP_1F)(GLenum, float, float, int, int, const float *);
typedef void  (WINAPI *PFN_GET_MAP_DV)(GLenum, GLenum, double *);
typedef void  (WINAPI *PFN_GET_MAP_FV)(GLenum, GLenum, float *);
typedef unsigned char (WINAPI *PFN_IS_TEXTURE_EXT)(unsigned int);
typedef void  (WINAPI *PFN_EVAL_MESH_2)(GLenum, int, int, int, int);
typedef void  (WINAPI *PFN_FOG_F)(GLenum, float);
typedef void  (WINAPI *PFN_MATRIX_MODE)(GLenum);
typedef void  (WINAPI *PFN_LOAD_IDENTITY)(void);
typedef void  (WINAPI *PFN_FRUSTUM)(double, double, double, double, double, double);
typedef void  (WINAPI *PFN_TRANSLATE_F)(float, float, float);

/* WGL_ARB_pixel_format */
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_DRAW_TO_WINDOW_ARB       0x2001
#define WGL_ACCELERATION_ARB         0x2003
#define WGL_SUPPORT_OPENGL_ARB       0x2010
#define WGL_DOUBLE_BUFFER_ARB        0x2011
#define WGL_PIXEL_TYPE_ARB           0x2013
#define WGL_COLOR_BITS_ARB           0x2014
#define WGL_ALPHA_BITS_ARB           0x201B
#define WGL_DEPTH_BITS_ARB           0x2022
#define WGL_STENCIL_BITS_ARB         0x2023
#define WGL_FULL_ACCELERATION_ARB    0x2027
#define WGL_TYPE_RGBA_ARB            0x202B
#define WGL_SAMPLE_BUFFERS_ARB       0x2041
#define WGL_SAMPLES_ARB              0x2042

static LRESULT CALLBACK smokeWindowProc(HWND window, UINT message,
                                         WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(window, message, wParam, lParam);
}

static FARPROC required(HMODULE module, const char *name)
{
    FARPROC proc = GetProcAddress(module, name);
    if (!proc)
        fprintf(stderr, "missing export: %s (error %lu)\n",
                name, (unsigned long)GetLastError());
    return proc;
}

/* TRUE if the wrapper's diag log (CWD, appended across runs) contains the
 * needle.  Used to assert the one-time fault flags fired. */
static int diagLogHas(const char *needle)
{
    FILE *f = fopen("gldirect_diag.log", "r");
    char buf[4096];
    size_t n;
    int found = 0;

    if (!f)
        return 0;
    while ((n = fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
        buf[n] = '\0';
        if (strstr(buf, needle)) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static int textFileHas(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char buf[8192];
    size_t n;
    int found = 0;
    if (!f) return 0;
    while ((n = fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
        buf[n] = '\0';
        if (strstr(buf, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static unsigned long long fileWriteTime(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER ticks;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
        return 0;
    ticks.LowPart = data.ftLastWriteTime.dwLowDateTime;
    ticks.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return ticks.QuadPart;
}

int main(int argc, char **argv)
{
    WNDCLASSEXA wc;
    PIXELFORMATDESCRIPTOR pfd;
    HINSTANCE instance = GetModuleHandleA(NULL);
    HMODULE wrapper;
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC context = NULL;
    ATOM windowClass = 0;
    int format;
    int result = 1;
    PFN_CHOOSE_PIXEL_FORMAT choosePixelFormat;
    PFN_SET_PIXEL_FORMAT setPixelFormat;
    PFN_CREATE_CONTEXT createContext;
    PFN_DELETE_CONTEXT deleteContext;
    PFN_MAKE_CURRENT makeCurrent;
    PFN_GET_PROC_ADDRESS getProcAddress;
    PFN_SWAP_BUFFERS swapBuffers;
    PFN_GET_STRING getString;
    PFN_CLEAR_COLOR clearColor;
    PFN_CLEAR clear;
    PFN_CREATE_CONTEXT_ATTRIBS createContextAttribs;
    const GLubyte *version;
    int remixMode = (argc == 3 && strcmp(argv[2], "--remix") == 0);
    int remixFrameValidated = 0;
    unsigned long long remixLogBefore = 0;
    unsigned long long bridge32LogBefore = 0;
    unsigned long long bridge64LogBefore = 0;

    if (argc != 2 && !remixMode) {
        fprintf(stderr, "usage: wgl_smoke.exe <absolute-opengl32.dll-path> [--remix]\n");
        return 2;
    }

    /* Verbose diagnostics latch on the first log call, which happens during
     * driver init - before the OpenMW section below runs - so it has to be in
     * place before the wrapper loads.  The semantic overlay itself is armed
     * only inside the OpenMW section (GLDIRECT_SEMANTIC_DIAG=1), leaving every
     * earlier section's rendering untouched. */
    _putenv("GLDIRECT_VERBOSE=1");
    if (remixMode)
        _putenv("GLDIRECT_SEMANTIC_DIAG=1");
    if (remixMode)
        _putenv("GLDIRECT_REMIX_FINAL_TEARDOWN=1");
    if (remixMode) {
        remixLogBefore = fileWriteTime("rtx-remix\\logs\\remix-dxvk.log");
        bridge32LogBefore = fileWriteTime("rtx-remix\\logs\\bridge32.log");
        bridge64LogBefore = fileWriteTime("rtx-remix\\logs\\bridge64.log");
    }

    wrapper = LoadLibraryA(argv[1]);
    if (!wrapper) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", (unsigned long)GetLastError());
        return 3;
    }

    choosePixelFormat = (PFN_CHOOSE_PIXEL_FORMAT)required(wrapper, "wglChoosePixelFormat");
    setPixelFormat = (PFN_SET_PIXEL_FORMAT)required(wrapper, "wglSetPixelFormat");
    createContext = (PFN_CREATE_CONTEXT)required(wrapper, "wglCreateContext");
    deleteContext = (PFN_DELETE_CONTEXT)required(wrapper, "wglDeleteContext");
    makeCurrent = (PFN_MAKE_CURRENT)required(wrapper, "wglMakeCurrent");
    getProcAddress = (PFN_GET_PROC_ADDRESS)required(wrapper, "wglGetProcAddress");
    swapBuffers = (PFN_SWAP_BUFFERS)required(wrapper, "wglSwapBuffers");
    getString = (PFN_GET_STRING)required(wrapper, "glGetString");
    clearColor = (PFN_CLEAR_COLOR)required(wrapper, "glClearColor");
    clear = (PFN_CLEAR)required(wrapper, "glClear");
    if (!choosePixelFormat || !setPixelFormat || !createContext ||
        !deleteContext || !makeCurrent || !getProcAddress || !swapBuffers ||
        !getString || !clearColor || !clear)
        goto cleanup;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = smokeWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "GLDirectWglSmoke";
    windowClass = RegisterClassExA(&wc);
    if (!windowClass) {
        fprintf(stderr, "RegisterClassEx failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    window = CreateWindowExA(0, wc.lpszClassName, "GLDirect smoke",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             320, 240, NULL, NULL, instance, NULL);
    if (!window) {
        fprintf(stderr, "CreateWindowEx failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }
    ShowWindow(window, SW_SHOWNA);
    UpdateWindow(window);
    dc = GetDC(window);
    if (!dc)
        goto cleanup;

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    format = choosePixelFormat(dc, &pfd);
    if (format <= 0 || !setPixelFormat(dc, format, &pfd)) {
        fprintf(stderr, "pixel format setup failed: format=%d error=%lu\n",
                format, (unsigned long)GetLastError());
        goto cleanup;
    }

    context = createContext(dc);
    if (!context || !makeCurrent(dc, context)) {
        fprintf(stderr, "context setup failed: error=%lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    createContextAttribs =
        (PFN_CREATE_CONTEXT_ATTRIBS)getProcAddress("wglCreateContextAttribsARB");
    if (createContextAttribs) {
        static const int core46Attributes[] = {
            0x2091, 4,       /* WGL_CONTEXT_MAJOR_VERSION_ARB */
            0x2092, 6,       /* WGL_CONTEXT_MINOR_VERSION_ARB */
            0x9126, 0x0001,  /* WGL_CONTEXT_PROFILE_MASK_ARB / CORE */
            0
        };
        HGLRC coreContext = createContextAttribs(dc, NULL, core46Attributes);
        if (coreContext) {
            if (!makeCurrent(dc, coreContext)) {
                deleteContext(coreContext);
                fprintf(stderr, "making the 4.6 core context current failed\n");
                goto cleanup;
            }
            deleteContext(context);
            context = coreContext;
        }
    }

    /*
     * WGL_ARB_pixel_format. Applications that negotiate a modern context
     * pick their format through this path and treat a missing entry point
     * or an empty result as fatal.
     */
    {
        PFN_GET_EXTENSIONS_STRING_ARB getExtensionsStringARB =
            (PFN_GET_EXTENSIONS_STRING_ARB)getProcAddress("wglGetExtensionsStringARB");
        PFN_CHOOSE_PIXEL_FORMAT_ARB choosePixelFormatARB =
            (PFN_CHOOSE_PIXEL_FORMAT_ARB)getProcAddress("wglChoosePixelFormatARB");
        PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB getPixelFormatAttribivARB =
            (PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB)getProcAddress("wglGetPixelFormatAttribivARB");
        const char *wglExtensions;
        static const int formatAttributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB,  TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
        };
        /* Multisampling is not enumerated; the request must still yield a
         * format instead of an empty result the application treats as
         * fatal. */
        static const int multisampleAttributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB,  TRUE,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB,        4,
            0
        };
        static const int queried[] = {
            WGL_NUMBER_PIXEL_FORMATS_ARB,
            WGL_SUPPORT_OPENGL_ARB,
            WGL_DOUBLE_BUFFER_ARB,
            WGL_PIXEL_TYPE_ARB,
            WGL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB,
            WGL_ALPHA_BITS_ARB,
            WGL_DEPTH_BITS_ARB,
            WGL_STENCIL_BITS_ARB,
            WGL_SAMPLES_ARB
        };
        int arbFormats[8];
        int values[sizeof(queried) / sizeof(queried[0])];
        UINT formatCount = 0;

        if (!getExtensionsStringARB) {
            fprintf(stderr, "wglGetExtensionsStringARB did not resolve\n");
            goto cleanup;
        }
        wglExtensions = getExtensionsStringARB(dc);
        if (!wglExtensions || !strstr(wglExtensions, "WGL_ARB_pixel_format")) {
            fprintf(stderr, "WGL_ARB_pixel_format is not advertised: %s\n",
                    wglExtensions ? wglExtensions : "(null)");
            goto cleanup;
        }
        if (!choosePixelFormatARB || !getPixelFormatAttribivARB) {
            fprintf(stderr, "WGL_ARB_pixel_format entry points did not resolve "
                            "(choose=%p attribiv=%p)\n",
                    (void *)choosePixelFormatARB,
                    (void *)getPixelFormatAttribivARB);
            goto cleanup;
        }

        if (!choosePixelFormatARB(dc, formatAttributes, NULL, 8, arbFormats,
                                  &formatCount) || formatCount == 0) {
            fprintf(stderr, "wglChoosePixelFormatARB returned no format\n");
            goto cleanup;
        }
        if (arbFormats[0] <= 0) {
            fprintf(stderr, "wglChoosePixelFormatARB returned invalid format %d\n",
                    arbFormats[0]);
            goto cleanup;
        }

        if (!getPixelFormatAttribivARB(dc, arbFormats[0], 0,
                                       sizeof(queried) / sizeof(queried[0]),
                                       queried, values)) {
            fprintf(stderr, "wglGetPixelFormatAttribivARB failed for format %d\n",
                    arbFormats[0]);
            goto cleanup;
        }
        if (values[0] < arbFormats[0]) {
            fprintf(stderr, "format %d outside the reported count %d\n",
                    arbFormats[0], values[0]);
            goto cleanup;
        }
        if (values[1] != TRUE || values[2] != TRUE ||
            values[3] != WGL_TYPE_RGBA_ARB ||
            values[4] != WGL_FULL_ACCELERATION_ARB) {
            fprintf(stderr, "chosen format reports the wrong capabilities: "
                            "opengl=%d doublebuffer=%d type=0x%X accel=0x%X\n",
                    values[1], values[2], values[3], values[4]);
            goto cleanup;
        }
        if (values[5] < 32 || values[7] < 24 || values[8] < 8) {
            fprintf(stderr, "chosen format is smaller than requested: "
                            "color=%d depth=%d stencil=%d\n",
                    values[5], values[7], values[8]);
            goto cleanup;
        }

        formatCount = 0;
        if (!choosePixelFormatARB(dc, multisampleAttributes, NULL, 8,
                                  arbFormats, &formatCount) ||
            formatCount == 0 || arbFormats[0] <= 0) {
            fprintf(stderr, "wglChoosePixelFormatARB failed to satisfy a "
                            "multisample request\n");
            goto cleanup;
        }
    }

    /*
     * WGL_EXT_swap_control. id Tech titles drive r_swapInterval through this
     * every time the setting changes.
     */
    {
        PFN_GET_EXTENSIONS_STRING_ARB getExtensionsStringARB =
            (PFN_GET_EXTENSIONS_STRING_ARB)getProcAddress("wglGetExtensionsStringARB");
        PFN_SWAP_INTERVAL_EXT swapIntervalEXT =
            (PFN_SWAP_INTERVAL_EXT)getProcAddress("wglSwapIntervalEXT");
        PFN_GET_SWAP_INTERVAL_EXT getSwapIntervalEXT =
            (PFN_GET_SWAP_INTERVAL_EXT)getProcAddress("wglGetSwapIntervalEXT");
        const char *wglExtensions = getExtensionsStringARB ?
                                    getExtensionsStringARB(dc) : NULL;

        if (!wglExtensions || !strstr(wglExtensions, "WGL_EXT_swap_control")) {
            fprintf(stderr, "WGL_EXT_swap_control is not advertised\n");
            goto cleanup;
        }
        if (!swapIntervalEXT || !getSwapIntervalEXT) {
            fprintf(stderr, "WGL_EXT_swap_control entry points did not resolve\n");
            goto cleanup;
        }
        if (!swapIntervalEXT(1) || getSwapIntervalEXT() != 1) {
            fprintf(stderr, "wglSwapIntervalEXT(1) did not take: reported %d\n",
                    getSwapIntervalEXT());
            goto cleanup;
        }
        if (!swapIntervalEXT(0) || getSwapIntervalEXT() != 0) {
            fprintf(stderr, "wglSwapIntervalEXT(0) did not take: reported %d\n",
                    getSwapIntervalEXT());
            goto cleanup;
        }
        /* Adaptive vsync needs WGL_EXT_swap_control_tear, which is not
         * advertised, so a negative interval must be refused rather than
         * silently accepted. */
        if (swapIntervalEXT(-1)) {
            fprintf(stderr, "wglSwapIntervalEXT(-1) was accepted without "
                            "WGL_EXT_swap_control_tear\n");
            goto cleanup;
        }
    }

    version = getString(GL_VERSION);
    if (!version || !version[0]) {
        fprintf(stderr, "glGetString(GL_VERSION) returned no value\n");
        goto cleanup;
    }
    if (getProcAddress("glCreateShader") == NULL) {
        fprintf(stderr, "glCreateShader did not resolve\n");
        goto cleanup;
    }

    /* RTX Remix is an asynchronous bridge, so synchronous framebuffer
     * readback is not a valid pass/fail oracle.  This mode instead exercises
     * the real x86 client bridge with an FF4-style compatibility shader,
     * publishes a perspective projection plus non-identity view, submits and
     * presents one frame, and verifies that both stages became native DX9. */
    if (remixMode) {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_COLOR_4F color4f = (PFN_COLOR_4F)required(wrapper, "glColor4f");
        PFN_CLIENT_ARRAY enableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glEnableClientState");
        PFN_CLIENT_ARRAY disableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glDisableClientState");
        PFN_VERTEX_POINTER vertexPointer =
            (PFN_VERTEX_POINTER)required(wrapper, "glVertexPointer");
        PFN_TEXCOORD_POINTER texCoordPointer =
            (PFN_TEXCOORD_POINTER)required(wrapper, "glTexCoordPointer");
        PFN_MATRIX_MODE matrixMode = (PFN_MATRIX_MODE)required(wrapper, "glMatrixMode");
        PFN_LOAD_IDENTITY loadIdentity =
            (PFN_LOAD_IDENTITY)required(wrapper, "glLoadIdentity");
        PFN_FRUSTUM frustum = (PFN_FRUSTUM)required(wrapper, "glFrustum");
        PFN_TRANSLATE_F translatef = (PFN_TRANSLATE_F)required(wrapper, "glTranslatef");
        PFN_GET_FLOATV getFloatv = (PFN_GET_FLOATV)required(wrapper, "glGetFloatv");
        static const char *vsSource =
            "varying vec4 v_color; varying vec2 v_texCoord;\n"
            "void main(){ gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;\n"
            "v_color=gl_Color; v_texCoord=vec2(gl_MultiTexCoord0); }\n";
        static const char *fsSource =
            "#define UVCoordScale 1.0\n"
            "varying vec4 v_color; varying vec2 v_texCoord;\n"
            "void main(){ gl_FragColor=vec4(v_texCoord*UVCoordScale,0.25,1.0)*v_color; }\n";
        const float positions[9] = {
            -1.0f, -0.75f, 0.0f,  1.0f, -0.75f, 0.0f,  0.0f, 0.75f, 0.0f
        };
        const float texcoords[6] = { 0, 0, 1, 0, 0.5f, 1 };
        unsigned int vs = 0, fs = 0, program = 0;
        float projectionMatrix[16], modelViewMatrix[16];
        RECT client;
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !viewport || !color4f || !enableClientState ||
            !disableClientState || !vertexPointer || !texCoordPointer ||
            !matrixMode || !loadIdentity || !frustum || !translatef || !getFloatv)
            goto cleanup;

        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "Remix FF4 vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "Remix FF4 fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "Remix FF4 program did not link: %s\n", log);
            goto cleanup;
        }
        if (!diagLogHas("legacy GLSL matrices are native DX9 constants") ||
            !diagLogHas("native DX9 VS/PS pair ready")) {
            fprintf(stderr, "Remix FF4 shader pair did not reach native DX9\n");
            goto cleanup;
        }

        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        matrixMode(GL_PROJECTION); loadIdentity();
        frustum(-1.0, 1.0, -0.75, 0.75, 1.0, 100.0);
        matrixMode(GL_MODELVIEW); loadIdentity(); translatef(0, 0, -4.0f);
        getFloatv(GL_PROJECTION_MATRIX, projectionMatrix);
        getFloatv(GL_MODELVIEW_MATRIX, modelViewMatrix);
        if (projectionMatrix[11] > -0.999f || projectionMatrix[11] < -1.001f ||
            modelViewMatrix[14] > -3.999f || modelViewMatrix[14] < -4.001f) {
            fprintf(stderr, "GL projection/modelview matrix stacks were crossed\n");
            goto cleanup;
        }
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        color4f(1, 1, 1, 1);
        enableClientState(GL_VERTEX_ARRAY); vertexPointer(3, GL_FLOAT, 0, positions);
        enableClientState(GL_TEXTURE_COORD_ARRAY);
        texCoordPointer(2, GL_FLOAT, 0, texcoords);
        useProgram(program); drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        swapBuffers(dc);
        Sleep(1000);
        if (fileWriteTime("rtx-remix\\logs\\remix-dxvk.log") <= remixLogBefore) {
            fprintf(stderr, "RTX Remix server did not write a fresh log\n");
            goto cleanup;
        }
        if (textFileHas("rtx-remix\\logs\\remix-dxvk.log",
                        "CameraManager: rejected an invalid camera") ||
            textFileHas("rtx-remix\\logs\\remix-dxvk.log",
                        "not detecting a valid camera")) {
            fprintf(stderr, "RTX Remix rejected the published GL camera\n");
            goto cleanup;
        }
        if (!textFileHas("rtx-remix\\logs\\remix-dxvk.log",
                         "RenderPass GBuffer Raytrace Mode") &&
            !textFileHas("rtx-remix\\logs\\remix-dxvk.log",
                         "current camera frustum")) {
            fprintf(stderr, "RTX Remix did not confirm a usable camera/render pass\n");
            goto cleanup;
        }
        disableClientState(GL_TEXTURE_COORD_ARRAY);
        disableClientState(GL_VERTEX_ARRAY);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        printf("GL_VERSION=%s\n", (const char *)version);
        remixFrameValidated = 1;
        result = 0;
        goto cleanup;
    }

    /* User-FBO regression: id Tech draws menu layers into texture-backed
     * framebuffers, then samples those textures while compositing the window.
     * The wrapper used to remember only the GL name and leave D3D9 rendering
     * on the backbuffer, producing large magenta blocks and missing menu text.
     * Clear a small off-screen target, leave the pass (which resolves it), and
     * verify the sampling texture received the rendered colour. */
    {
        PFN_GEN_TEXTURES genTextures =
            (PFN_GEN_TEXTURES)getProcAddress("glGenTextures");
        PFN_BIND_TEXTURE bindTexture =
            (PFN_BIND_TEXTURE)getProcAddress("glBindTexture");
        PFN_TEX_IMAGE_2D texImage2D =
            (PFN_TEX_IMAGE_2D)getProcAddress("glTexImage2D");
        PFN_GET_TEX_IMAGE getTexImage =
            (PFN_GET_TEX_IMAGE)getProcAddress("glGetTexImage");
        PFN_DELETE_TEXTURES deleteTextures =
            (PFN_DELETE_TEXTURES)getProcAddress("glDeleteTextures");
        PFN_GEN_FRAMEBUFFERS genFramebuffers =
            (PFN_GEN_FRAMEBUFFERS)getProcAddress("glGenFramebuffers");
        PFN_BIND_FRAMEBUFFER bindFramebuffer =
            (PFN_BIND_FRAMEBUFFER)getProcAddress("glBindFramebuffer");
        PFN_FRAMEBUFFER_TEXTURE_2D framebufferTexture2D =
            (PFN_FRAMEBUFFER_TEXTURE_2D)getProcAddress("glFramebufferTexture2D");
        PFN_CHECK_FRAMEBUFFER_STATUS checkFramebufferStatus =
            (PFN_CHECK_FRAMEBUFFER_STATUS)getProcAddress("glCheckFramebufferStatus");
        PFN_DELETE_FRAMEBUFFERS deleteFramebuffers =
            (PFN_DELETE_FRAMEBUFFERS)getProcAddress("glDeleteFramebuffers");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        unsigned char initial[16 * 16 * 4];
        unsigned char pixels[16 * 16 * 4];
        unsigned int texture = 0, framebuffer = 0;
        RECT client;

        if (!genTextures || !bindTexture || !texImage2D || !getTexImage ||
            !deleteTextures || !genFramebuffers || !bindFramebuffer ||
            !framebufferTexture2D || !checkFramebufferStatus ||
            !deleteFramebuffers || !viewport)
            goto cleanup;
        memset(initial, 0, sizeof(initial));
        memset(pixels, 0, sizeof(pixels));
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, initial);
        genFramebuffers(1, &framebuffer);
        bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture, 0);
        if (checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "texture-backed framebuffer was incomplete\n");
            goto cleanup;
        }
        viewport(0, 0, 16, 16);
        clearColor(0.2f, 0.4f, 0.8f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        bindFramebuffer(GL_FRAMEBUFFER, 0);
        bindTexture(GL_TEXTURE_2D, texture);
        getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (pixels[0] < 45 || pixels[0] > 58 ||
            pixels[1] < 96 || pixels[1] > 109 ||
            pixels[2] < 198 || pixels[2] > 211 || pixels[3] < 250) {
            fprintf(stderr, "FBO resolve readback mismatch: RGBA=(%u,%u,%u,%u)\n",
                    pixels[0], pixels[1], pixels[2], pixels[3]);
            goto cleanup;
        }
        deleteFramebuffers(1, &framebuffer);
        deleteTextures(1, &texture);
        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        printf("TEXTURE_FBO_RESOLVE=PASS\n");
    }

    /* Regression for the Haydee/RTX Remix crash report.  The attached log
     * showed these advertised ARB/EXT entry points resolving to NULL; the game
     * later called one after replacing its bootstrap context and jumped to
     * address zero.  Every name from that log must survive both the initial
     * context and the context-reuse/reset path exercised above. */
    {
        static const char *loggedMissingProcs[] = {
            "glGetAttachedObjectsARB",
            "glGetHandleARB",
            "glGetObjectParameterfvARB",
            "glTextureStorage1DEXT",
            "glTextureStorage2DEXT",
            "glTextureStorage3DEXT",
            "glGetProgramEnvParameterdvARB",
            "glGetProgramEnvParameterfvARB",
            "glGetProgramLocalParameterdvARB",
            "glGetProgramLocalParameterfvARB",
            "glGetProgramStringARB",
            "glGetProgramivARB",
            "glIsProgramARB",
            "glProgramEnvParameter4dvARB",
            "glProgramLocalParameter4dvARB"
        };
        size_t i;

        for (i = 0; i < sizeof(loggedMissingProcs) /
                        sizeof(loggedMissingProcs[0]); ++i) {
            if (getProcAddress(loggedMissingProcs[i]) == NULL) {
                fprintf(stderr, "log regression: %s still resolves to NULL\n",
                        loggedMissingProcs[i]);
                goto cleanup;
            }
        }
    }

    if (getProcAddress("glDefinitelyNotARealEntryPointGLDIRECT") != NULL) {
        fprintf(stderr, "unknown GL entry point incorrectly resolved\n");
        goto cleanup;
    }

    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        static const char *vsSource =
            "#version 460 core\n"
            "void main(){ gl_Position=vec4(0.0,0.0,0.0,1.0); }\n";
        static const char *gsSource =
            "#version 460 core\n"
            "layout(points) in; layout(triangle_strip,max_vertices=3) out;\n"
            "out vec4 stageColor;\n"
            "void main(){ stageColor=vec4(1.0,0.0,0.0,1.0);\n"
            "gl_Position=vec4(-0.75,-0.75,0.0,1.0); EmitVertex();\n"
            "gl_Position=vec4( 0.75,-0.75,0.0,1.0); EmitVertex();\n"
            "gl_Position=vec4( 0.00, 0.75,0.0,1.0); EmitVertex(); EndPrimitive(); }\n";
        static const char *fsSource =
            "#version 460 core\n"
            "in vec4 stageColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=stageColor; }\n";
        const GLenum shaderTypes[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
        const char *sources[3] = { vsSource, gsSource, fsSource };
        unsigned int shaders[3], program;
        unsigned char pixel[4] = { 0, 0, 0, 0 };
        int i, ok, linked;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !viewport || !readPixels || !getError)
            goto cleanup;
        program = createProgram();
        for (i = 0; i < 3; ++i) {
            shaders[i] = createShader(shaderTypes[i]);
            shaderSource(shaders[i], 1, &sources[i], NULL);
            compileShader(shaders[i]);
            ok = 0;
            getShaderiv(shaders[i], GL_COMPILE_STATUS, &ok);
            if (!ok) {
                fprintf(stderr, "programmable stage %d did not compile\n", i);
                goto cleanup;
            }
            attachShader(program, shaders[i]);
        }
        linkProgram(program);
        linked = 0;
        getProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024]; int logLength = 0;
            getProgramInfoLog(program, sizeof(log), &logLength, log);
            fprintf(stderr, "geometry program did not link: %s\n", log);
            goto cleanup;
        }
        /* Deliberately larger than the client area: GL permits this, while
         * D3D9 requires a clipped viewport plus clip-space compensation. */
        viewport(0, 0, 320, 240);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        useProgram(program);
        drawArrays(GL_POINTS, 0, 1);
        useProgram(0);
        if (getError() != 0) {
            fprintf(stderr, "geometry-stage draw reported a GL error\n");
            goto cleanup;
        }
        {
            RECT client;
            GetClientRect(window, &client);
            readPixels(client.right / 2, client.bottom / 2, 1, 1,
                       GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        }
        for (i = 0; i < 3; ++i) deleteShader(shaders[i]);
        deleteProgram(program);
        if (pixel[0] < 200 || pixel[1] > 32 || pixel[2] > 32) {
            RECT client = {0, 0, 0, 0};
            unsigned char *frame = NULL;
            int redCount = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;
            if (GetClientRect(window, &client) && client.right > 0 && client.bottom > 0) {
                int x, y;
                size_t frameBytes = (size_t)client.right * (size_t)client.bottom * 4u;
                frame = (unsigned char *)malloc(frameBytes);
                if (frame) {
                    readPixels(0, 0, client.right, client.bottom,
                               GL_RGBA, GL_UNSIGNED_BYTE, frame);
                    for (y = 0; y < client.bottom; ++y) for (x = 0; x < client.right; ++x) {
                        const unsigned char *p = frame +
                            ((size_t)y * (size_t)client.right + (size_t)x) * 4u;
                        if (p[0] >= 200 && p[1] <= 32 && p[2] <= 32) {
                            if (!redCount) minX = maxX = x, minY = maxY = y;
                            if (x < minX) minX = x; if (x > maxX) maxX = x;
                            if (y < minY) minY = y; if (y > maxY) maxY = y;
                            ++redCount;
                        }
                    }
                    free(frame);
                }
            }
            fprintf(stderr, "geometry-stage D3D9 handoff returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            fprintf(stderr, "red framebuffer pixels=%d bounds=(%d,%d)-(%d,%d) client=%ldx%ld\n",
                    redCount, minX, minY, maxX, maxY,
                    (long)client.right, (long)client.bottom);
            goto cleanup;
        }
    }

    /* Ordinary translated VS/PS path: validates GL clip-depth conversion,
     * half-pixel placement and oversized-viewport compensation in SM3. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VERTEX_ATTRIB_POINTER vertexAttribPointer =
            (PFN_VERTEX_ATTRIB_POINTER)getProcAddress("glVertexAttribPointer");
        PFN_VERTEX_ATTRIB_ARRAY enableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glEnableVertexAttribArray");
        PFN_VERTEX_ATTRIB_ARRAY disableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glDisableVertexAttribArray");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec3 position;\n"
            "void main(){ gl_Position=vec4(position,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=vec4(1.0,1.0,0.0,1.0); }\n";
        const float positions[9] = {
            -0.75f, -0.75f, -0.5f,
             0.75f, -0.75f, -0.5f,
             0.00f,  0.75f, -0.5f
        };
        unsigned int vs, fs, program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !vertexAttribPointer || !enableVertexAttribArray ||
            !disableVertexAttribArray || !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "translated vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "translated fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "translated VS/PS program did not link: %s\n", log);
            goto cleanup;
        }
        viewport(0, 0, 320, 240);
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        vertexAttribPointer(0, 3, GL_FLOAT, 0, 3 * (int)sizeof(float), positions);
        enableVertexAttribArray(0);
        useProgram(program); drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        disableVertexAttribArray(0);
        readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (pixel[0] < 200 || pixel[1] < 200 || pixel[2] > 32) {
            fprintf(stderr, "translated VS/PS viewport path returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }

        /* Readback conversion: every supported packed and wide type must
         * convert the yellow pixel instead of being skipped or memcpy'd raw
         * (raw copies put the components in the wrong bit slots). */
        {
            unsigned short s;
            unsigned int u;
            float f[4];
            readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, &s);
            if (s != 0xFFC0u) { fprintf(stderr, "5_6_5 readback = 0x%04X\n", s); goto cleanup; }
            readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &s);
            if (s != 0xFF0Fu) { fprintf(stderr, "4_4_4_4 readback = 0x%04X\n", s); goto cleanup; }
            readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, &s);
            if (s != 0xFFC1u) { fprintf(stderr, "5_5_5_1 readback = 0x%04X\n", s); goto cleanup; }
            readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, &u);
            if (u != 0xFFFF00FFu) { fprintf(stderr, "8_8_8_8 readback = 0x%08X\n", u); goto cleanup; }
            readPixels(160, 120, 1, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &u);
            if (u != 0xFFFFFF00u) { fprintf(stderr, "8_8_8_8_REV/BGRA readback = 0x%08X\n", u); goto cleanup; }
            readPixels(160, 120, 1, 1, GL_RGBA, GL_FLOAT, f);
            if (f[0] < 0.9f || f[1] < 0.9f || f[2] > 0.05f) {
                fprintf(stderr, "float readback = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
                goto cleanup;
            }
        }
    }

    /* FF4 3D Remake compatibility-profile shaders.  These exact constructs
     * used to link only through the software fallback: legacy built-in vertex
     * inputs/matrices remained undeclared in HLSL and the numeric macro was
     * stripped with its #define line.  A native reflected MVP register proves
     * the vertex stage reached DX9 bytecode rather than merely reporting a
     * successful software link. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        static const char *vsSource =
            "varying vec4 v_color;\n"
            "varying vec2 v_texCoord;\n"
            "void main(){\n"
            " gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;\n"
            " v_color=gl_Color;\n"
            " v_texCoord=vec2(gl_MultiTexCoord0);\n"
            "}\n";
        static const char *fsSource =
            "#define UVCoordScale 1.0\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texCoord;\n"
            "uniform sampler2D tex0; uniform sampler2D tex1; uniform sampler2D tex2;\n"
            "const vec3 offset=vec3(0,-0.501960814,-0.501960814);\n"
            "const vec3 Rcoeff=vec3(1,0.000,1.402);\n"
            "const vec3 Gcoeff=vec3(1,-0.3441,-0.7141);\n"
            "const vec3 Bcoeff=vec3(1,1.772,0.000);\n"
            "void main(){ vec2 tcoord=v_texCoord; vec3 yuv,rgb;\n"
            " yuv.x=texture2D(tex0,tcoord).r; tcoord*=UVCoordScale;\n"
            " yuv.y=texture2D(tex1,tcoord).r; yuv.z=texture2D(tex2,tcoord).r;\n"
            " yuv+=offset; rgb.r=dot(yuv,Rcoeff); rgb.g=dot(yuv,Gcoeff);\n"
            " rgb.b=dot(yuv,Bcoeff); gl_FragColor=vec4(rgb,1.0)*v_color; }\n";
        unsigned int vs, fs, program;
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !deleteProgram)
            goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "FF4 compatibility vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "FF4 compatibility fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "FF4 compatibility program did not link: %s\n", log);
            goto cleanup;
        }
        if (!diagLogHas("legacy GLSL matrices are native DX9 constants")) {
            fprintf(stderr, "FF4 compatibility vertex shader did not reach native DX9\n");
            goto cleanup;
        }
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        fprintf(stderr, "FF4 compatibility shaders reached native DX9\n");
    }

    /* id Tech-style generated GLSL regression.  This combines the constructs
     * that kept Wolfenstein's splash/menu programs off the native D3D9 path:
     * shadow helper overloads, a multisample sampler passed through a helper,
     * bare gl_FragCoord/VPOS use, a packed four-row vertex camera, and the
     * ARB_draw_elements_base_vertex entry point used by the renderer. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_GET_UNIFORM_LOCATION getUniformLocation =
            (PFN_GET_UNIFORM_LOCATION)getProcAddress("glGetUniformLocation");
        PFN_UNIFORM_1I uniform1i = (PFN_UNIFORM_1I)getProcAddress("glUniform1i");
        PFN_UNIFORM_4FV uniform4fv = (PFN_UNIFORM_4FV)getProcAddress("glUniform4fv");
        PFN_DRAW_ELEMENTS_BASE_VERTEX drawElementsBaseVertex =
            (PFN_DRAW_ELEMENTS_BASE_VERTEX)getProcAddress("glDrawElementsBaseVertex");
        PFN_CLIENT_ARRAY enableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glEnableClientState");
        PFN_CLIENT_ARRAY disableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glDisableClientState");
        PFN_VERTEX_POINTER vertexPointer =
            (PFN_VERTEX_POINTER)required(wrapper, "glVertexPointer");
        static const char *vsSource =
            "#version 150\n"
            "uniform vec4 _va_[4];\n"
            "void main(){ vec4 p=gl_Vertex;\n"
            " gl_Position=vec4(dot(p,_va_[0]),dot(p,_va_[1]),"
            "dot(p,_va_[2]),dot(p,_va_[3])); }\n";
        static const char *fsSource =
            "#version 150\n"
            "float shadow2Ddepth(sampler2DShadow image,vec3 tc){"
            " return texture(image,tc); }\n"
            "vec4 fetchMS(sampler2DMS image,ivec2 p,int sampleIndex){"
            " return texelFetch(image,p,sampleIndex); }\n"
            "uniform sampler2DShadow shadowMap; uniform sampler2DMS msImage;\n"
            "out vec4 outColor;\n"
            "void main(){ vec4 frag=gl_FragCoord;"
            " float d=shadow2Ddepth(shadowMap,vec3(0.5,0.5,0.5));"
            " outColor=fetchMS(msImage,ivec2(frag.xy),0)+vec4(d)+frag*0.0; }\n";
        const float cameraRows[16] = {
            1,0,0,0,  0,1,0,0,  0,0,-1.02f,-0.202f,  0,0,-1,0
        };
        const float vertices[9] = {
            -0.5f,-0.5f,-1.0f,  0.5f,-0.5f,-1.0f,  0.0f,0.5f,-1.0f
        };
        const unsigned short indices[3] = { 0, 1, 2 };
        unsigned int vs, fs, program;
        int ok = 0, cameraLoc, shadowLoc, msLoc;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !getUniformLocation || !uniform1i || !uniform4fv ||
            !drawElementsBaseVertex || !enableClientState || !disableClientState ||
            !vertexPointer)
            goto cleanup;

        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "id Tech vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "id Tech fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "id Tech native program did not link: %s\n", log);
            goto cleanup;
        }

        useProgram(program);
        cameraLoc = getUniformLocation(program, "_va_");
        shadowLoc = getUniformLocation(program, "shadowMap");
        msLoc = getUniformLocation(program, "msImage");
        if (cameraLoc < 0 || shadowLoc < 0 || msLoc < 0) {
            fprintf(stderr, "id Tech uniforms did not resolve\n");
            goto cleanup;
        }
        uniform4fv(cameraLoc, 4, cameraRows);
        uniform1i(shadowLoc, 0); uniform1i(msLoc, 1);
        enableClientState(GL_VERTEX_ARRAY);
        vertexPointer(3, GL_FLOAT, 0, vertices);
        drawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices, 0);
        disableClientState(GL_VERTEX_ARRAY);
        useProgram(0);

        if (!diagLogHas("DX9 camera uniform=_va_ published=1")) {
            fprintf(stderr, "id Tech packed camera was not published to DX9\n");
            goto cleanup;
        }
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        fprintf(stderr, "id Tech shader/menu path reached native DX9\n");
    }

    /* Ordinary BGRA data uploaded into DXT5 internal storage must be encoded,
     * not skipped.  id Tech uses this legal path for runtime-generated
     * textures; a constant red block makes the round-trip assertion stable
     * despite block compression. */
    {
        PFN_GEN_TEXTURES genTextures = (PFN_GEN_TEXTURES)required(wrapper, "glGenTextures");
        PFN_BIND_TEXTURE bindTexture = (PFN_BIND_TEXTURE)required(wrapper, "glBindTexture");
        PFN_TEX_IMAGE_2D texImage2D = (PFN_TEX_IMAGE_2D)required(wrapper, "glTexImage2D");
        PFN_GET_TEX_IMAGE getTexImage =
            (PFN_GET_TEX_IMAGE)getProcAddress("glGetTexImage");
        PFN_DELETE_TEXTURES deleteTextures =
            (PFN_DELETE_TEXTURES)required(wrapper, "glDeleteTextures");
        unsigned char bgra[4 * 4 * 4], rgba[4 * 4 * 4];
        unsigned int texture = 0;
        int i;

        if (!genTextures || !bindTexture || !texImage2D || !getTexImage ||
            !deleteTextures)
            goto cleanup;
        for (i = 0; i < 16; ++i) {
            bgra[i * 4 + 0] = 0; bgra[i * 4 + 1] = 0;
            bgra[i * 4 + 2] = 255; bgra[i * 4 + 3] = 255;
        }
        memset(rgba, 0, sizeof(rgba));
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        texImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                   4, 4, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
        getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (rgba[0] < 248 || rgba[1] > 8 || rgba[2] > 16 || rgba[3] < 248) {
            fprintf(stderr, "DXT5 BGRA upload read back %u,%u,%u,%u\n",
                    rgba[0], rgba[1], rgba[2], rgba[3]);
            goto cleanup;
        }
        deleteTextures(1, &texture);
        fprintf(stderr, "DXT5 BGRA runtime upload encoded\n");
    }

    /* Bitwise-operator lowering: the transpiler rewrites &,|,^,<<,>>,~ into
     * float-arithmetic helper functions (SM3 has no integer ops).  This draw
     * must produce byte-exact bitwise results through the real translated
     * pixel-shader path.  uBit = 15450 = 0x3C5A:
     *   r = (uBit & 0xFF)          = 0x5A  =  90
     *   g = ((uBit >> 8) & 0x0F)   = 0x0C  =  12
     *   b = ((uBit ^ 0xF0F0)&0xFF) = 0xAA  = 170
     *   a = ((uBit | 0xFF) & 0xFF) = 0xFF  = 255
     * expected 8_8_8_8 readback (R in MSB) = 0x5A0CAAFF. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VERTEX_ATTRIB_POINTER vertexAttribPointer =
            (PFN_VERTEX_ATTRIB_POINTER)getProcAddress("glVertexAttribPointer");
        PFN_VERTEX_ATTRIB_ARRAY enableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glEnableVertexAttribArray");
        PFN_VERTEX_ATTRIB_ARRAY disableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glDisableVertexAttribArray");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        PFN_GET_UNIFORM_LOCATION getUniformLocation =
            (PFN_GET_UNIFORM_LOCATION)getProcAddress("glGetUniformLocation");
        PFN_UNIFORM_1F uniform1f = (PFN_UNIFORM_1F)getProcAddress("glUniform1f");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec3 position;\n"
            "void main(){ gl_Position=vec4(position,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\n"
            "layout(location=0) out vec4 finalColor;\n"
            "uniform float uBit;\n"
            "void main(){\n"
            "  int x = int(uBit);\n"
            "  int r = x & 255;\n"
            "  int g = (x >> 8) & 15;\n"
            "  int b = (x ^ 61680) & 255;\n"
            "  int a = (x | 255) & 255;\n"
            "  finalColor = vec4(float(r), float(g), float(b), float(a)) / 255.0;\n"
            "}\n";
        const float positions[9] = {
            -0.75f, -0.75f, -0.5f,
             0.75f, -0.75f, -0.5f,
             0.00f,  0.75f, -0.5f
        };
        unsigned int vs, fs, program;
        unsigned int u;
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog ||
            !useProgram || !deleteProgram || !drawArrays || !vertexAttribPointer ||
            !enableVertexAttribArray || !disableVertexAttribArray || !viewport ||
            !readPixels || !getUniformLocation || !uniform1f) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "bitwise vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "bitwise fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "bitwise VS/PS program did not link: %s\n", log);
            goto cleanup;
        }
        viewport(0, 0, 320, 240);
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        vertexAttribPointer(0, 3, GL_FLOAT, 0, 3 * (int)sizeof(float), positions);
        enableVertexAttribArray(0);
        useProgram(program);
        uniform1f(getUniformLocation(program, "uBit"), 15450.0f);
        drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        disableVertexAttribArray(0);
        readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, &u);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (u != 0x5A0CAAFFu) {
            fprintf(stderr, "bitwise readback = 0x%08X (want 0x5A0CAAFF)\n", u);
            goto cleanup;
        }
        fprintf(stderr, "bitwise readback RGBA8 = 0x%08X (r=0x5A g=0x0C b=0xAA a=0xFF)\n", u);
    }

    /* Sampler dimensionality must survive generic texture()/textureLod()
     * lowering; treating every sampler as 2D makes otherwise valid cube and
     * volume shaders fail at D3DCompile time. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec3 position;\n"
            "void main(){gl_Position=vec4(position,1.0);}\n";
        static const char *fsSource =
            "#version 460 core\nuniform samplerCube cubeMap; uniform sampler3D volumeMap;\n"
            "layout(location=0) out vec4 finalColor;\n"
            "void main(){finalColor=textureLod(cubeMap,vec3(1,0,0),0.0)+"
            "texture(volumeMap,vec3(0.5))*0.001;}\n";
        unsigned int vs, fs, program;
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !deleteProgram) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "cube/volume sampler program did not link: %s\n", log);
            goto cleanup;
        }
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
    }

    /* A fragment operation with no Shader Model 3 representation executes in
     * the private GL 4.6 raster path and is copied back to the D3D9 target. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VERTEX_ATTRIB_POINTER vertexAttribPointer =
            (PFN_VERTEX_ATTRIB_POINTER)getProcAddress("glVertexAttribPointer");
        PFN_VERTEX_ATTRIB_ARRAY enableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glEnableVertexAttribArray");
        PFN_VERTEX_ATTRIB_ARRAY disableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glDisableVertexAttribArray");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec2 position;\n"
            "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){uint value=(5u<<1u)^3u;"
            "finalColor=(value==9u)?vec4(0.0,0.0,1.0,1.0):vec4(1.0,0.0,0.0,1.0);}\n";
        const float positions[6] = {
            -0.75f, -0.75f, 0.75f, -0.75f, 0.0f, 0.75f
        };
        unsigned int vs, fs, program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !vertexAttribPointer || !enableVertexAttribArray ||
            !disableVertexAttribArray || !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER);
        shaderSource(vs, 1, &vsSource, NULL); compileShader(vs);
        getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "software-fragment vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER);
        shaderSource(fs, 1, &fsSource, NULL); compileShader(fs);
        getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "software-fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "software-fragment program did not link: %s\n", log);
            goto cleanup;
        }
        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f); clear(GL_COLOR_BUFFER_BIT);
        vertexAttribPointer(0, 2, GL_FLOAT, 0, 2 * (int)sizeof(float), positions);
        enableVertexAttribArray(0);
        useProgram(program); drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        disableVertexAttribArray(0);
        readPixels(client.right / 2, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (pixel[2] < 200 || pixel[0] > 32 || pixel[1] > 32) {
            fprintf(stderr, "software-fragment path returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }
    }

    /* Tessellation control/evaluation execute in the private GL 4.6 stage
     * worker; their generated triangle is then rasterized by D3D9. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\n"
            "const vec2 p[3]=vec2[3](vec2(-0.7,-0.7),vec2(0.7,-0.7),vec2(0.0,0.7));\n"
            "void main(){ gl_Position=vec4(p[gl_VertexID],0.0,1.0); }\n";
        static const char *tcsSource =
            "#version 460 core\nlayout(vertices=3) out;\n"
            "void main(){ gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;"
            "if(gl_InvocationID==0){ gl_TessLevelOuter[0]=1.0; gl_TessLevelOuter[1]=1.0;"
            "gl_TessLevelOuter[2]=1.0; gl_TessLevelInner[0]=1.0; } }\n";
        static const char *tesSource =
            "#version 460 core\nlayout(triangles,equal_spacing,ccw) in;\nout vec4 stageColor;\n"
            "void main(){ gl_Position=gl_TessCoord.x*gl_in[0].gl_Position+"
            "gl_TessCoord.y*gl_in[1].gl_Position+gl_TessCoord.z*gl_in[2].gl_Position;"
            "stageColor=vec4(0.0,1.0,0.0,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nin vec4 stageColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=stageColor; }\n";
        const GLenum types[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER,
                                  GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
        const char *sources[4] = { vsSource, tcsSource, tesSource, fsSource };
        unsigned int shaders[4], program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int i, ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !viewport || !readPixels) goto cleanup;
        program = createProgram();
        for (i = 0; i < 4; ++i) {
            shaders[i] = createShader(types[i]);
            shaderSource(shaders[i], 1, &sources[i], NULL);
            compileShader(shaders[i]);
            getShaderiv(shaders[i], GL_COMPILE_STATUS, &ok);
            if (!ok) { fprintf(stderr, "tessellation stage %d did not compile\n", i); goto cleanup; }
            attachShader(program, shaders[i]);
        }
        linkProgram(program);
        getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "tessellation program did not link: %s\n", log);
            goto cleanup;
        }
        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        useProgram(program);
        drawArrays(GL_PATCHES, 0, 3);
        useProgram(0);
        readPixels(client.right / 2, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        for (i = 0; i < 4; ++i) deleteShader(shaders[i]);
        deleteProgram(program);
        if (pixel[1] < 200 || pixel[0] > 32 || pixel[2] > 32) {
            fprintf(stderr, "tessellation-stage D3D9 handoff returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }
    }

    /* True transform feedback writes the requested post-VS varying bytes into
     * the application's bound buffer while rasterization is discarded. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_TRANSFORM_FEEDBACK_VARYINGS tfVaryings =
            (PFN_TRANSFORM_FEEDBACK_VARYINGS)getProcAddress("glTransformFeedbackVaryings");
        PFN_BEGIN_TRANSFORM_FEEDBACK beginTF =
            (PFN_BEGIN_TRANSFORM_FEEDBACK)getProcAddress("glBeginTransformFeedback");
        PFN_END_TRANSFORM_FEEDBACK endTF =
            (PFN_END_TRANSFORM_FEEDBACK)getProcAddress("glEndTransformFeedback");
        PFN_ENABLE_DISABLE enable = (PFN_ENABLE_DISABLE)required(wrapper, "glEnable");
        PFN_ENABLE_DISABLE disable = (PFN_ENABLE_DISABLE)required(wrapper, "glDisable");
        PFN_GEN_BUFFERS genBuffers = (PFN_GEN_BUFFERS)getProcAddress("glGenBuffers");
        PFN_BIND_BUFFER bindBuffer = (PFN_BIND_BUFFER)getProcAddress("glBindBuffer");
        PFN_BUFFER_DATA bufferData = (PFN_BUFFER_DATA)getProcAddress("glBufferData");
        PFN_BIND_BUFFER_BASE bindBufferBase = (PFN_BIND_BUFFER_BASE)getProcAddress("glBindBufferBase");
        PFN_GET_BUFFER_SUB_DATA getBufferSubData =
            (PFN_GET_BUFFER_SUB_DATA)getProcAddress("glGetBufferSubData");
        PFN_DELETE_BUFFERS deleteBuffers = (PFN_DELETE_BUFFERS)getProcAddress("glDeleteBuffers");
        static const char *vsSource =
            "#version 460 core\nout vec4 tfValue;\n"
            "void main(){ tfValue=vec4(1.0,2.0,3.0,4.0);"
            "gl_Position=vec4(0.0,0.0,0.0,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=vec4(1.0); }\n";
        const char *capture = "tfValue";
        unsigned int vs, fs, program, buffer;
        float values[12];
        int ok = 0, i;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !useProgram || !deleteProgram || !drawArrays ||
            !tfVaryings || !beginTF || !endTF || !enable || !disable ||
            !genBuffers || !bindBuffer || !bufferData || !bindBufferBase ||
            !getBufferSubData || !deleteBuffers) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER);
        shaderSource(vs, 1, &vsSource, NULL); compileShader(vs);
        getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER);
        shaderSource(fs, 1, &fsSource, NULL); compileShader(fs);
        getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        tfVaryings(program, 1, &capture, GL_INTERLEAVED_ATTRIBS);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { fprintf(stderr, "transform-feedback program did not link\n"); goto cleanup; }
        memset(values, 0, sizeof(values));
        genBuffers(1, &buffer);
        bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(values), NULL, GL_DYNAMIC_COPY);
        bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
        useProgram(program); enable(GL_RASTERIZER_DISCARD);
        beginTF(GL_POINTS); drawArrays(GL_POINTS, 0, 3); endTF();
        disable(GL_RASTERIZER_DISCARD); useProgram(0);
        getBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(values), values);
        for (i = 0; i < 3; ++i) {
            if (values[i * 4] != 1.0f || values[i * 4 + 1] != 2.0f ||
                values[i * 4 + 2] != 3.0f || values[i * 4 + 3] != 4.0f) {
                fprintf(stderr, "transform feedback mismatch at vertex %d: %.1f %.1f %.1f %.1f\n",
                        i, values[i * 4], values[i * 4 + 1],
                        values[i * 4 + 2], values[i * 4 + 3]);
                goto cleanup;
            }
        }
        deleteBuffers(1, &buffer); deleteShader(vs); deleteShader(fs); deleteProgram(program);
    }

    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS_INSTANCED drawArraysInstanced =
            (PFN_DRAW_ARRAYS_INSTANCED)getProcAddress("glDrawArraysInstanced");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nout vec4 instanceColor;\n"
            "const vec2 p[3]=vec2[3](vec2(-0.28,-0.45),vec2(0.28,-0.45),vec2(0.0,0.45));\n"
            "void main(){ float x=(gl_InstanceID==0)?-0.48:0.48;"
            "gl_Position=vec4(p[gl_VertexID]+vec2(x,0.0),0.0,1.0);"
            "instanceColor=(gl_InstanceID==0)?vec4(1,0,0,1):vec4(0,0,1,1); }\n";
        static const char *fsSource =
            "#version 460 core\nin vec4 instanceColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=instanceColor; }\n";
        unsigned int vs, fs, program;
        unsigned char left[4] = {0}, right[4] = {0};
        int ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !useProgram || !deleteProgram || !drawArraysInstanced ||
            !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { fprintf(stderr, "instanced GL 4.6 program did not link\n"); goto cleanup; }
        GetClientRect(window, &client); viewport(0, 0, client.right, client.bottom);
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        useProgram(program); drawArraysInstanced(GL_TRIANGLES, 0, 3, 2); useProgram(0);
        readPixels(client.right / 4, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, left);
        readPixels((client.right * 3) / 4, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, right);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (left[0] < 200 || left[2] > 32 || right[2] < 200 || right[0] > 32) {
            fprintf(stderr, "instanced draw mismatch left=(%u,%u,%u) right=(%u,%u,%u)\n",
                    left[0], left[1], left[2], right[0], right[1], right[2]);
            goto cleanup;
        }
    }

    {
        PFN_CREATE_SHADER createShader =
            (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource =
            (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader =
            (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv =
            (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader =
            (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram =
            (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader =
            (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram =
            (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv =
            (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram =
            (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram =
            (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_GEN_BUFFERS genBuffers =
            (PFN_GEN_BUFFERS)getProcAddress("glGenBuffers");
        PFN_BIND_BUFFER bindBuffer =
            (PFN_BIND_BUFFER)getProcAddress("glBindBuffer");
        PFN_BUFFER_DATA bufferData =
            (PFN_BUFFER_DATA)getProcAddress("glBufferData");
        PFN_BIND_BUFFER_BASE bindBufferBase =
            (PFN_BIND_BUFFER_BASE)getProcAddress("glBindBufferBase");
        PFN_GET_BUFFER_SUB_DATA getBufferSubData =
            (PFN_GET_BUFFER_SUB_DATA)getProcAddress("glGetBufferSubData");
        PFN_DELETE_BUFFERS deleteBuffers =
            (PFN_DELETE_BUFFERS)getProcAddress("glDeleteBuffers");
        PFN_DISPATCH_COMPUTE dispatchCompute =
            (PFN_DISPATCH_COMPUTE)getProcAddress("glDispatchCompute");
        PFN_MEMORY_BARRIER memoryBarrier =
            (PFN_MEMORY_BARRIER)getProcAddress("glMemoryBarrier");
        PFN_GEN_TEXTURES genTextures =
            (PFN_GEN_TEXTURES)getProcAddress("glGenTextures");
        PFN_BIND_TEXTURE bindTexture =
            (PFN_BIND_TEXTURE)getProcAddress("glBindTexture");
        PFN_TEX_IMAGE_2D texImage2D =
            (PFN_TEX_IMAGE_2D)getProcAddress("glTexImage2D");
        PFN_TEX_BUFFER_RANGE texBufferRange =
            (PFN_TEX_BUFFER_RANGE)getProcAddress("glTexBufferRange");
        PFN_BIND_IMAGE_TEXTURE bindImageTexture =
            (PFN_BIND_IMAGE_TEXTURE)getProcAddress("glBindImageTexture");
        PFN_GET_TEX_IMAGE getTexImage =
            (PFN_GET_TEX_IMAGE)getProcAddress("glGetTexImage");
        PFN_DELETE_TEXTURES deleteTextures =
            (PFN_DELETE_TEXTURES)getProcAddress("glDeleteTextures");
        static const char *computeSource =
            "#version 460 core\n"
            "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
            "layout(std430, binding=0) buffer Output { uint value; } outputData;\n"
            "layout(binding=0) uniform samplerBuffer inputTexture;\n"
            "layout(rgba8, binding=0) uniform writeonly image2D outputImage;\n"
            "void main() { outputData.value = "
            "(texelFetch(inputTexture, 0).x == 77.0) ? 0x12345678u : 0u; "
            "imageStore(outputImage, ivec2(0), vec4(0.25, 0.5, 0.75, 1.0)); }\n";
        unsigned int shader;
        unsigned int program;
        unsigned int buffer;
        unsigned int texture;
        unsigned int textureBuffer;
        unsigned int inputBuffer;
        float inputValues[5] = { -1.0f, -2.0f, -3.0f, -4.0f, 77.0f };
        unsigned int value = 0;
        unsigned char texel[4] = { 0, 0, 0, 0 };
        int compiled = 0;
        int linked = 0;

        if (!createShader || !shaderSource || !compileShader ||
            !getShaderiv || !deleteShader || !createProgram || !attachShader ||
            !linkProgram || !getProgramiv || !getProgramInfoLog || !useProgram ||
            !deleteProgram || !genBuffers || !bindBuffer || !bufferData ||
            !bindBufferBase || !getBufferSubData || !deleteBuffers ||
            !dispatchCompute || !memoryBarrier || !genTextures || !bindTexture ||
            !texImage2D || !texBufferRange || !bindImageTexture ||
            !getTexImage || !deleteTextures) {
            fprintf(stderr, "compute shader API did not resolve\n");
            goto cleanup;
        }
        shader = createShader(GL_COMPUTE_SHADER);
        shaderSource(shader, 1, &computeSource, NULL);
        compileShader(shader);
        getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            fprintf(stderr, "GLSL 4.60 compute shader did not compile\n");
            deleteShader(shader);
            goto cleanup;
        }
        program = createProgram();
        attachShader(program, shader);
        linkProgram(program);
        getProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024];
            int logLength = 0;
            getProgramInfoLog(program, sizeof(log), &logLength, log);
            fprintf(stderr, "GLSL 4.60 compute program did not link: %s\n", log);
            deleteProgram(program);
            deleteShader(shader);
            goto cleanup;
        }
        genBuffers(1, &buffer);
        bindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        bufferData(GL_SHADER_STORAGE_BUFFER, sizeof(value), &value, GL_DYNAMIC_COPY);
        bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
        genBuffers(1, &inputBuffer);
        bindBuffer(GL_TEXTURE_BUFFER, inputBuffer);
        bufferData(GL_TEXTURE_BUFFER, sizeof(inputValues), inputValues, GL_DYNAMIC_COPY);
        genTextures(1, &textureBuffer);
        bindTexture(GL_TEXTURE_BUFFER, textureBuffer);
        texBufferRange(GL_TEXTURE_BUFFER, GL_R32F, inputBuffer,
                       4 * sizeof(float), sizeof(float));
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, texel);
        bindImageTexture(0, texture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);
        useProgram(program);
        dispatchCompute(1, 1, 1);
        memoryBarrier(GL_ALL_BARRIER_BITS);
        getBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(value), &value);
        getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
        useProgram(0);
        deleteTextures(1, &texture);
        deleteTextures(1, &textureBuffer);
        deleteBuffers(1, &inputBuffer);
        deleteBuffers(1, &buffer);
        deleteProgram(program);
        deleteShader(shader);
        if (value != 0x12345678u) {
            fprintf(stderr, "compute texture-buffer emulation returned 0x%08X instead of 0x12345678\n", value);
            goto cleanup;
        }
        if (texel[0] < 63 || texel[0] > 65 || texel[1] < 127 || texel[1] > 129 ||
            texel[2] < 190 || texel[2] > 192 || texel[3] != 255) {
            fprintf(stderr, "compute image emulation returned RGBA=(%u,%u,%u,%u)\n",
                    texel[0], texel[1], texel[2], texel[3]);
            goto cleanup;
        }
    }

    {
        PFN_GEN_LISTS genLists = (PFN_GEN_LISTS)required(wrapper, "glGenLists");
        PFN_NEW_LIST newList = (PFN_NEW_LIST)required(wrapper, "glNewList");
        PFN_END_LIST endList = (PFN_END_LIST)required(wrapper, "glEndList");
        PFN_CALL_LIST callList = (PFN_CALL_LIST)required(wrapper, "glCallList");
        PFN_DELETE_LISTS deleteLists = (PFN_DELETE_LISTS)required(wrapper, "glDeleteLists");
        PFN_COLOR_4F color4f = (PFN_COLOR_4F)required(wrapper, "glColor4f");
        PFN_GET_FLOATV getFloatv = (PFN_GET_FLOATV)required(wrapper, "glGetFloatv");
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        unsigned int list;
        unsigned int err;
        float before[4], compiled[4], replayed[4];
        if (!genLists || !newList || !endList || !callList || !deleteLists ||
            !color4f || !getFloatv || !getError)
            goto cleanup;
        color4f(0.9f, 0.8f, 0.7f, 0.6f);
        getFloatv(GL_CURRENT_COLOR, before);
        list = genLists(1);
        if (!list) {
            fprintf(stderr, "glGenLists failed\n");
            goto cleanup;
        }
        newList(list, GL_COMPILE);
        color4f(0.1f, 0.2f, 0.3f, 0.4f);
        endList();
        getFloatv(GL_CURRENT_COLOR, compiled);
        callList(list);
        getFloatv(GL_CURRENT_COLOR, replayed);
        deleteLists(list, 1);
        if (memcmp(before, compiled, sizeof(before)) != 0 ||
            replayed[0] != 0.1f || replayed[1] != 0.2f ||
            replayed[2] != 0.3f || replayed[3] != 0.4f) {
            fprintf(stderr, "display-list compile/replay state mismatch: "
                    "before=(%g,%g,%g,%g) compiled=(%g,%g,%g,%g) "
                    "replayed=(%g,%g,%g,%g)\n",
                    before[0], before[1], before[2], before[3],
                    compiled[0], compiled[1], compiled[2], compiled[3],
                    replayed[0], replayed[1], replayed[2], replayed[3]);
            goto cleanup;
        }

        /* Display-list fault paths must report the GL errors the spec names,
         * so a game that checks glGetError can see the fault instead of
         * silently drawing nothing. */
        endList();                              /* no glNewList open */
        err = getError();
        if (err != GL_INVALID_OPERATION) {
            fprintf(stderr, "glEndList without glNewList reported 0x%04X, want GL_INVALID_OPERATION\n", err);
            goto cleanup;
        }
        newList(0, GL_COMPILE);
        err = getError();
        if (err != GL_INVALID_VALUE) {
            fprintf(stderr, "glNewList(0) reported 0x%04X, want GL_INVALID_VALUE\n", err);
            goto cleanup;
        }
        newList(list, GL_COMPILE);
        newList(1, GL_COMPILE);                 /* nested compile */
        err = getError();
        if (err != GL_INVALID_OPERATION) {
            fprintf(stderr, "nested glNewList reported 0x%04X, want GL_INVALID_OPERATION\n", err);
            goto cleanup;
        }
        endList();                              /* finish the outer list */
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "outer glEndList reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }
        callList(0x7FFFFFFFu);                  /* no such list */
        err = getError();
        if (err != GL_INVALID_VALUE) {
            fprintf(stderr, "glCallList(unknown) reported 0x%04X, want GL_INVALID_VALUE\n", err);
            goto cleanup;
        }
        if (genLists(-1) != 0) {
            fprintf(stderr, "glGenLists(-1) returned a list name, want 0\n");
            goto cleanup;
        }
        deleteLists(1, -1);
        err = getError();
        if (err != GL_INVALID_VALUE) {
            fprintf(stderr, "glDeleteLists(range<0) reported 0x%04X, want GL_INVALID_VALUE\n", err);
            goto cleanup;
        }
        if (!diagLogHas("FAULT FLAG [display-list]")) {
            fprintf(stderr, "display-list fault flags missing from gldirect_diag.log\n");
            goto cleanup;
        }
    }

    /* OpenMW-style programmable draws + semantic overlay validation.
     *
     * OpenMW does all lighting in GLSL: per-light uniforms, no glLight* calls,
     * one diffuse sampler.  With GLDIRECT_SEMANTIC_DIAG=1 (and
     * GLDIRECT_VERBOSE=1 so per-draw lines are written) the wrapper's semantic
     * overlay narrates fixed-function state around every draw and logs a hash
     * of the exact submitted geometry.  The checker (check_semantic_overlay.py)
     * asserts:
     *   - the OpenMW-style mat4 camera and viewport are published as D3D9
     *     transform/viewport state ("camera=1" and "viewport=..."),
     *   - uniform-synthesized lights appeared ("synth=1"),
     *   - the mirrored-light path ran ("lights=1") for the glLightfv variant,
     *   - the last two draws (draw 2 and draw 3 of the identical trio below)
     *     produced byte-identical submissions (equal geoHash lines).
     *
     * Draw order is deliberate: draw1/draw2 (synthesized lights, before any
     * glLight call) -> FF variant (mirrored lights) -> draw3 (synthesized
     * again), so the trailing geoHash pair is the identical OpenMW pair. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_GET_UNIFORM_LOCATION getUniformLocation =
            (PFN_GET_UNIFORM_LOCATION)getProcAddress("glGetUniformLocation");
        PFN_UNIFORM_1I uniform1i = (PFN_UNIFORM_1I)getProcAddress("glUniform1i");
        PFN_UNIFORM_3F uniform3f = (PFN_UNIFORM_3F)getProcAddress("glUniform3f");
        PFN_UNIFORM_4F uniform4f = (PFN_UNIFORM_4F)getProcAddress("glUniform4f");
        PFN_UNIFORM_MATRIX_4FV uniformMatrix4fv =
            (PFN_UNIFORM_MATRIX_4FV)getProcAddress("glUniformMatrix4fv");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        PFN_ENABLE_DISABLE enableDisable = (PFN_ENABLE_DISABLE)required(wrapper, "glEnable");
        PFN_ENABLE_DISABLE disable = (PFN_ENABLE_DISABLE)required(wrapper, "glDisable");
        PFN_COLOR_4F color4f = (PFN_COLOR_4F)required(wrapper, "glColor4f");
        PFN_LIGHTFV lightfv = (PFN_LIGHTFV)required(wrapper, "glLightfv");
        PFN_MATERIALFV materialfv = (PFN_MATERIALFV)required(wrapper, "glMaterialfv");
        PFN_CLIENT_ARRAY enableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glEnableClientState");
        PFN_CLIENT_ARRAY disableClientState =
            (PFN_CLIENT_ARRAY)required(wrapper, "glDisableClientState");
        PFN_VERTEX_POINTER vertexPointer = (PFN_VERTEX_POINTER)required(wrapper, "glVertexPointer");
        PFN_TEXCOORD_POINTER texCoordPointer =
            (PFN_TEXCOORD_POINTER)required(wrapper, "glTexCoordPointer");
        PFN_NORMAL_POINTER normalPointer =
            (PFN_NORMAL_POINTER)required(wrapper, "glNormalPointer");
        PFN_GEN_TEXTURES genTextures = (PFN_GEN_TEXTURES)required(wrapper, "glGenTextures");
        PFN_BIND_TEXTURE bindTexture = (PFN_BIND_TEXTURE)required(wrapper, "glBindTexture");
        PFN_TEX_IMAGE_2D texImage2D = (PFN_TEX_IMAGE_2D)required(wrapper, "glTexImage2D");
        PFN_TEX_PARAMETER_I texParameteri =
            (PFN_TEX_PARAMETER_I)required(wrapper, "glTexParameteri");
        PFN_DELETE_TEXTURES deleteTextures =
            (PFN_DELETE_TEXTURES)required(wrapper, "glDeleteTextures");
        static const char *vsSource =
            "#version 330 core\n"
            "uniform mat4 mvp;\n"
            "in vec3 position; in vec2 texCoord;\n"
            "out vec2 vUV;\n"
            "void main(){ vUV=texCoord; gl_Position=mvp*vec4(position,1.0); }\n";
        static const char *fsSource =
            "#version 330 core\n"
            "uniform sampler2D diffuseMap;\n"
            "uniform vec4 sunDiffuseColor; uniform vec4 sunSpecularColor;\n"
            "uniform vec3 sunDirection; uniform vec4 diffuseColor;\n"
            "uniform vec4 ambientColor;\n"
            "in vec2 vUV;\n"
            "layout(location=0) out vec4 finalColor;\n"
            "void main(){\n"
            "  finalColor=texture(diffuseMap,vUV)*diffuseColor"
            "*(sunDiffuseColor+ambientColor);\n"
            "}\n";
        const float positions[18] = {
            -0.75f, -0.75f, -0.5f,  0.75f, -0.75f, -0.5f,
             0.75f,  0.75f, -0.5f, -0.75f, -0.75f, -0.5f,
             0.75f,  0.75f, -0.5f, -0.75f,  0.75f, -0.5f
        };
        const float texcoords[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        const float normals[18] = {
            0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f
        };
        const float identity[16] = {
            1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1
        };
        const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float sunDirection[3] = { 0.0f, 0.0f, 1.0f };
        const float sunPosition[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
        const float gray[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
        const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        const unsigned char texels[16] = {
            255, 255, 255, 255,  255, 255, 255, 255,
            255, 255, 255, 255,  255, 255, 255, 255
        };
        unsigned int vs = 0, fs = 0, program = 0, texture = 0;
        unsigned char pixel[4] = { 0, 0, 0, 0 };
        int ok = 0;

        _putenv("GLDIRECT_SEMANTIC_DIAG=1");
        _putenv("GLDIRECT_VERBOSE=1");

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !getUniformLocation || !uniform1i || !uniform3f || !uniform4f ||
            !uniformMatrix4fv || !drawArrays || !viewport || !readPixels ||
            !getError || !enableDisable || !disable || !lightfv || !materialfv ||
            !enableClientState || !disableClientState || !vertexPointer ||
            !texCoordPointer || !normalPointer || !genTextures || !bindTexture ||
            !texImage2D || !texParameteri || !deleteTextures) {
            fprintf(stderr, "semantic-overlay API did not resolve\n");
            goto cleanup;
        }

        viewport(0, 0, 320, 240);
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, texels);

        vs = createShader(GL_VERTEX_SHADER);
        shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs);
        getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            fprintf(stderr, "OpenMW-style vertex shader did not compile\n");
            goto cleanup;
        }
        fs = createShader(GL_FRAGMENT_SHADER);
        shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs);
        getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            fprintf(stderr, "OpenMW-style fragment shader did not compile\n");
            goto cleanup;
        }
        program = createProgram();
        attachShader(program, vs);
        attachShader(program, fs);
        linkProgram(program);
        getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "OpenMW-style program did not link: %s\n", log);
            goto cleanup;
        }
        /* glUniform* record into the currently bound program, so bind first. */
        useProgram(program);
        uniformMatrix4fv(getUniformLocation(program, "mvp"), 1, 0, identity);
        uniform1i(getUniformLocation(program, "diffuseMap"), 0);
        uniform4f(getUniformLocation(program, "sunDiffuseColor"), 1, 1, 1, 1);
        uniform4f(getUniformLocation(program, "sunSpecularColor"), 1, 1, 1, 1);
        uniform3f(getUniformLocation(program, "sunDirection"),
                  sunDirection[0], sunDirection[1], sunDirection[2]);
        uniform4f(getUniformLocation(program, "diffuseColor"), 1, 1, 1, 1);
        uniform4f(getUniformLocation(program, "ambientColor"), 0.2f, 0.2f, 0.2f, 1);

        enableClientState(GL_VERTEX_ARRAY);
        vertexPointer(3, GL_FLOAT, 0, positions);
        enableClientState(GL_NORMAL_ARRAY);
        normalPointer(GL_FLOAT, 0, normals);
        enableClientState(GL_TEXTURE_COORD_ARRAY);
        texCoordPointer(2, GL_FLOAT, 0, texcoords);

        /* OpenMW turns lighting off in GL and does it in the shader; publish
         * the fixed-function semantic baseline alongside that shader:
         * lighting on, a white diffuse material, current color white. */
        color4f(1, 1, 1, 1);
        materialfv(GL_FRONT, GL_AMBIENT, one);
        materialfv(GL_FRONT, GL_DIFFUSE, one);
        materialfv(GL_FRONT, GL_SPECULAR, black);
        enableDisable(GL_LIGHTING);

        clearColor(0, 0, 0, 1);
        clear(GL_COLOR_BUFFER_BIT);

        /* Draws 1 and 2: identical submissions, synthesized lights. */
        drawArrays(GL_TRIANGLES, 0, 6);
        drawArrays(GL_TRIANGLES, 0, 6);
        readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        if (pixel[0] < 200 || pixel[1] < 200 || pixel[2] < 200) {
            fprintf(stderr, "OpenMW-style draws 1-2 returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }

        /* Fixed-function variant: a real GL light, narrated through the
         * mirrored-light path.  Material gray * lit 0.2 ambient -> ~0.48. */
        useProgram(0);
        enableDisable(GL_LIGHT0);
        lightfv(GL_LIGHT0, GL_POSITION, sunPosition);
        lightfv(GL_LIGHT0, GL_DIFFUSE, white);
        materialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
        drawArrays(GL_TRIANGLES, 0, 6);
        readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        disable(GL_LIGHT0);
        disable(GL_LIGHTING);
        if (pixel[0] < 60 || pixel[0] > 190 ||
            pixel[1] < 60 || pixel[1] > 190 ||
            pixel[2] < 60 || pixel[2] > 190) {
            fprintf(stderr, "fixed-function variant returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }

        /* Draw 3: identical to draws 1/2, so the trailing geoHash pair
         * (draws 2 and 3) is provably byte-identical. */
        useProgram(program);
        drawArrays(GL_TRIANGLES, 0, 6);

        useProgram(0);
        disableClientState(GL_TEXTURE_COORD_ARRAY);
        disableClientState(GL_NORMAL_ARRAY);
        disableClientState(GL_VERTEX_ARRAY);
        deleteTextures(1, &texture);
        deleteProgram(program);
        deleteShader(fs);
        deleteShader(vs);

        if (getError() != 0) {
            fprintf(stderr, "semantic-overlay draws reported a GL error\n");
            goto cleanup;
        }
    }

    /* Legacy GL 1.x fault paths: the stubs must be full implementations, not
     * placeholders.  These exercise the fixes from the 2026-08-16 audit:
     * glGetMapdv real coefficients, glIsTextureEXT, glGetDoublev element
     * counts, float texture parameters, pixel-transfer round trips, signed
     * normalisation clamps and select/feedback validation. */
    {
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        PFN_GEN_TEXTURES genTextures = (PFN_GEN_TEXTURES)required(wrapper, "glGenTextures");
        PFN_BIND_TEXTURE bindTexture = (PFN_BIND_TEXTURE)required(wrapper, "glBindTexture");
        PFN_TEX_PARAMETER_F texParameterf =
            (PFN_TEX_PARAMETER_F)required(wrapper, "glTexParameterf");
        PFN_GET_TEX_PARAMETER_FV getTexParameterfv =
            (PFN_GET_TEX_PARAMETER_FV)required(wrapper, "glGetTexParameterfv");
        PFN_GET_DOUBLEV getDoublev = (PFN_GET_DOUBLEV)required(wrapper, "glGetDoublev");
        PFN_PIXEL_TRANSFER_F pixelTransferf =
            (PFN_PIXEL_TRANSFER_F)required(wrapper, "glPixelTransferf");
        PFN_COLOR_3B color3b = (PFN_COLOR_3B)required(wrapper, "glColor3b");
        PFN_NORMAL_3B normal3b = (PFN_NORMAL_3B)required(wrapper, "glNormal3b");
        PFN_SELECT_BUFFER selectBuffer =
            (PFN_SELECT_BUFFER)required(wrapper, "glSelectBuffer");
        PFN_FEEDBACK_BUFFER feedbackBuffer =
            (PFN_FEEDBACK_BUFFER)required(wrapper, "glFeedbackBuffer");
        PFN_RASTER_POS_2F rasterPos2f =
            (PFN_RASTER_POS_2F)required(wrapper, "glRasterPos2f");
        PFN_TEX_COORD_4F texCoord4f = (PFN_TEX_COORD_4F)required(wrapper, "glTexCoord4f");
        PFN_MAP_1F map1f = (PFN_MAP_1F)required(wrapper, "glMap1f");
        PFN_GET_MAP_DV getMapdv = (PFN_GET_MAP_DV)required(wrapper, "glGetMapdv");
        PFN_GET_MAP_FV getMapfv = (PFN_GET_MAP_FV)required(wrapper, "glGetMapfv");
        PFN_IS_TEXTURE_EXT isTextureEXT =
            (PFN_IS_TEXTURE_EXT)getProcAddress("glIsTextureEXT");
        PFN_EVAL_MESH_2 evalMesh2 = (PFN_EVAL_MESH_2)required(wrapper, "glEvalMesh2");
        unsigned int err, texture;
        unsigned int selectBufferData[16];
        float feedbackBufferData[16];
        double dv[16];
        float fv[16];
        int i;

        if (!getError || !genTextures || !bindTexture || !texParameterf ||
            !getTexParameterfv || !getDoublev || !pixelTransferf || !color3b ||
            !normal3b || !selectBuffer || !feedbackBuffer || !rasterPos2f ||
            !texCoord4f || !map1f || !getMapdv || !getMapfv || !isTextureEXT ||
            !evalMesh2)
            goto cleanup;

        /* Signed normalisation: the most negative value maps to exactly -1.0
         * (spec: max(v/127, -1)), so glColor3b(-128,...) must not produce a
         * value below -1. */
        color3b((char)-128, (char)127, 0);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glColor3b reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }
        normal3b((char)-128, (char)127, 0);
        getMapfv(GL_MAP1_VERTEX_3, GL_COEFF, fv); /* only for the error check */
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glNormal3b reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }

        /* glGetMapdv must return the same coefficients as glGetMapfv, not 0. */
        {
            const float pts[6] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f };
            map1f(GL_MAP1_VERTEX_3, 0.0f, 1.0f, 3, 2, pts);
            err = getError();
            if (err != GL_NO_ERROR) {
                fprintf(stderr, "glMap1f reported 0x%04X, want GL_NO_ERROR\n", err);
                goto cleanup;
            }
            memset(dv, 0x7F, sizeof(dv));
            getMapdv(GL_MAP1_VERTEX_3, GL_COEFF, dv);
            getMapfv(GL_MAP1_VERTEX_3, GL_COEFF, fv);
            for (i = 0; i < 6; i++) {
                if (dv[i] != (double)fv[i]) {
                    fprintf(stderr, "glGetMapdv COEFF[%d]=%g differs from glGetMapfv %g\n",
                            i, dv[i], fv[i]);
                    goto cleanup;
                }
            }
            getMapdv(GL_MAP1_VERTEX_3, GL_ORDER, dv);
            if (dv[0] != 2.0) {
                fprintf(stderr, "glGetMapdv ORDER=%g, want 2\n", dv[0]);
                goto cleanup;
            }
        }

        /* glIsTextureEXT: TRUE for a bound texture, FALSE for a never-used id. */
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glBindTexture reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }
        if (!isTextureEXT(texture)) {
            fprintf(stderr, "glIsTextureEXT(bound texture) returned FALSE\n");
            goto cleanup;
        }
        if (isTextureEXT(texture + 1)) {
            fprintf(stderr, "glIsTextureEXT(unused id) returned TRUE\n");
            goto cleanup;
        }

        /* Float texture parameters must round-trip through glGetTexParameterfv
         * without int truncation. */
        texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 1.5f);
        texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5.25f);
        texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
        fv[0] = 0.0f;
        getTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, fv);
        if (fv[0] != 1.5f) {
            fprintf(stderr, "glGetTexParameterfv(MIN_LOD)=%g, want 1.5\n", fv[0]);
            goto cleanup;
        }
        getTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, fv);
        if (fv[0] != 5.25f) {
            fprintf(stderr, "glGetTexParameterfv(MAX_LOD)=%g, want 5.25\n", fv[0]);
            goto cleanup;
        }
        getTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fv);
        if (fv[0] != 4.0f) {
            fprintf(stderr, "glGetTexParameterfv(MAX_ANISOTROPY_EXT)=%g, want 4\n", fv[0]);
            goto cleanup;
        }

        /* glGetDoublev writes the right element count: 4 for the viewport. */
        for (i = 0; i < 4; i++) dv[i] = -1e30;
        getDoublev(GL_VIEWPORT, dv);
        if (dv[0] == -1e30 || dv[1] == -1e30 || dv[2] == -1e30 || dv[3] == -1e30) {
            fprintf(stderr, "glGetDoublev(GL_VIEWPORT) did not write 4 elements\n");
            goto cleanup;
        }

        /* Pixel-transfer state round-trips in float and double. */
        pixelTransferf(GL_RED_SCALE, 0.5f);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glPixelTransferf(GL_RED_SCALE) reported 0x%04X\n", err);
            goto cleanup;
        }
        getDoublev(GL_RED_SCALE, dv);
        if (dv[0] != 0.5) {
            fprintf(stderr, "glGetDoublev(GL_RED_SCALE)=%g, want 0.5\n", dv[0]);
            goto cleanup;
        }

        /* Raster position latches the active unit's texture coords. */
        texCoord4f(0.25f, 0.5f, 0.75f, 1.0f);
        rasterPos2f(10.0f, 10.0f);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glRasterPos2f reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }

        /* Select/feedback validation: negative size and bad type must error. */
        selectBuffer((int)-1, selectBufferData);
        err = getError();
        if (err != GL_INVALID_VALUE) {
            fprintf(stderr, "glSelectBuffer(-1) reported 0x%04X, want GL_INVALID_VALUE\n", err);
            goto cleanup;
        }
        feedbackBuffer((int)-1, GL_2D, feedbackBufferData);
        err = getError();
        if (err != GL_INVALID_VALUE) {
            fprintf(stderr, "glFeedbackBuffer(-1) reported 0x%04X, want GL_INVALID_VALUE\n", err);
            goto cleanup;
        }
        feedbackBuffer(16, 0x1234u, feedbackBufferData);
        err = getError();
        if (err != GL_INVALID_ENUM) {
            fprintf(stderr, "glFeedbackBuffer(bad type) reported 0x%04X, want GL_INVALID_ENUM\n", err);
            goto cleanup;
        }
        selectBuffer(16, selectBufferData);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glSelectBuffer(valid) reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }
        feedbackBuffer(16, GL_2D, feedbackBufferData);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glFeedbackBuffer(valid) reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }

        /* Eval mesh rejects an invalid mode. */
        evalMesh2(0x1234u, 0, 1, 0, 1);
        err = getError();
        if (err != GL_INVALID_ENUM) {
            fprintf(stderr, "glEvalMesh2(bad mode) reported 0x%04X, want GL_INVALID_ENUM\n", err);
            goto cleanup;
        }
        evalMesh2(GL_POINT, 0, 1, 0, 1);
        err = getError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "glEvalMesh2(GL_POINT) reported 0x%04X, want GL_NO_ERROR\n", err);
            goto cleanup;
        }

        /* glGetString must log through the diag facility (no per-call fopen). */
        if (!diagLogHas("GL: glGetString")) {
            fprintf(stderr, "glGetString diag log line missing from gldirect_diag.log\n");
            goto cleanup;
        }
    }

    /* Fault flagging: wglGetProcAddress must resolve every GL 1.0-4.6 core
     * name this wrapper knows (the classic "GetProcAddress: glGetString
     * (Failed)" crashes happened when core names resolved to NULL and the
     * game called the NULL), must return NULL for a genuinely unknown name,
     * and must flag the unknown name once in the diag log. */
    {
        static const char *coreNames[] = {
            "glBegin",                  /* GL 1.0 */
            "glVertex2f", "glNormal3f", /* GL 1.0 */
            "glDrawRangeElements",      /* GL 1.2 */
            "glMultiTexCoord2fARB",     /* GL 1.3 */
            "glCompressedTexImage2D",   /* GL 1.3 */
            "glFogCoordf",              /* GL 1.4 */
            "glBlendFuncSeparate",      /* GL 1.4 */
            "glGenFramebuffers",        /* GL 3.0 */
            "glBindVertexArray",        /* GL 3.0 */
            "glDrawArraysInstanced",    /* GL 3.1 */
            "glFramebufferTexture",     /* GL 3.2 */
            "glDispatchCompute",        /* GL 4.3 */
            "glBindVertexBuffer",       /* GL 4.4 */
        };
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        PFN_FOG_F fogf = (PFN_FOG_F)required(wrapper, "glFogf");
        size_t i;
        unsigned int err;
        if (!getError || !fogf)
            goto cleanup;
        for (i = 0; i < sizeof(coreNames) / sizeof(coreNames[0]); i++) {
            if (!getProcAddress(coreNames[i])) {
                fprintf(stderr, "wglGetProcAddress(%s) returned NULL for a known core name\n",
                        coreNames[i]);
                goto cleanup;
            }
        }
        if (getProcAddress("glThisNameDoesNotExistXYZ") != NULL) {
            fprintf(stderr, "wglGetProcAddress returned non-NULL for an unknown name\n");
            goto cleanup;
        }
        fogf(0xDEADu, 1.0f);            /* unknown fog pname */
        err = getError();
        if (err != GL_INVALID_ENUM) {
            fprintf(stderr, "glFogf(unknown pname) reported 0x%04X, want GL_INVALID_ENUM\n", err);
            goto cleanup;
        }
        if (!diagLogHas("FAULT FLAG [proc-address] glThisNameDoesNotExistXYZ")) {
            fprintf(stderr, "proc-address fault flag missing from gldirect_diag.log\n");
            goto cleanup;
        }
        if (!diagLogHas("FAULT FLAG [fog]")) {
            fprintf(stderr, "fog fault flag missing from gldirect_diag.log\n");
            goto cleanup;
        }
    }

    clearColor(0.125f, 0.25f, 0.5f, 1.0f);
    clear(GL_COLOR_BUFFER_BIT);
    if (!swapBuffers(dc)) {
        fprintf(stderr, "SwapBuffers failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    printf("GL_VERSION=%s\n", (const char *)version);
    result = 0;

cleanup:
    if (makeCurrent)
        makeCurrent(NULL, NULL);
    if (context && deleteContext)
        deleteContext(context);
    if (dc && window)
        ReleaseDC(window, dc);
    if (window)
        DestroyWindow(window);
    if (windowClass)
        UnregisterClassA("GLDirectWglSmoke", instance);
    FreeLibrary(wrapper);
    if (remixMode && remixFrameValidated) {
        Sleep(200);
        if (!diagLogHas("device released cleanly (0 references remain)") ||
            fileWriteTime("rtx-remix\\logs\\bridge32.log") <= bridge32LogBefore ||
            fileWriteTime("rtx-remix\\logs\\bridge64.log") <= bridge64LogBefore ||
            !textFileHas("rtx-remix\\logs\\bridge32.log",
                         "Shutdown cleanup successful") ||
            !textFileHas("rtx-remix\\logs\\bridge64.log",
                         "Shutdown cleanup successful")) {
            fprintf(stderr, "RTX Remix client/server teardown was not clean\n");
            result = 1;
        } else {
            printf("REMIX_FF4_NATIVE_DX9_CAMERA_VIEWPORT=PASS\n");
            printf("REMIX_CLIENT_SERVER_SHUTDOWN=PASS\n");
        }
    }
    return result;
}
